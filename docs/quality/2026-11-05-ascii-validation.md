# 全文ASCIIの本文検証を短縮する — #616

`note_text`の既存検証入口で、本文全体がNULを含まないASCIIと確認できた場合だけ復号を省く。
非ASCIIまたはNULが一つでもあれば、従来の共通`utf8_text_decode`による検証を先頭から実行する。
採用候補の固定Release比較では、ASCII16MiBの読込入口が中央値21.881850→4.093825ms、
編集入口が43.298025→26.567100msとなった。確保・複製・破棄を含み、編集入口は改行正規化も含む。
検証部分だけの時間や実UIの応答時間ではない。

末尾だけに日本語を置いた16MiBでは、先頭部分の追加走査により読込21.373975→22.739925ms、
編集42.474350→44.475250msと増えた。全入力を速くする変更ではなく、この費用を残す判断である。

## 正典と契約

基点はmain `038b8b711b6b6daadfb927ac58145ef930425454`。#458/#460/#464の過去値は現実測へ流用しない。
`note_text_create` / `note_text_from_editor` → private `validate` → 共通`utf8_text_decode`の入口を維持する。
新API・別decoder・依存・schema・thread・版・品質ゲート設定は加えない。追加waiver none、既存WVR-0001は変更なし。

`src/core/note_text.c`のprivate `plain_ascii`は、残長が`sizeof(uint64_t)`以上のときだけ`memcpy`で読む。
各byteの上位bitとzeroの存在を調べ、短い末尾は範囲内で一つずつ確認する。
全byteが1〜127の場合だけtrueを返すため、制御文字・DELも従来どおり受理する。空入力も受理する。
非整列pointerの整数型への変換、aliasing依存、終端を仮定した読み取り、入力の変更はない。
unsigned減算によるzero検出は存在判定だけに使い、借りによる余分なbitを位置として解釈しない。

NULや非ASCIIを含む場合は旧ループ全体に戻るため、不正UTF-8→NUL→保存上限の優先を維持する。
BOMの一個消費、本文U+FEFFの保持、保存表現込み16MiB、改行正規化、失敗out不変、確保順も変えない。
判断は[ADR0042](../adr/0042-bom-at-file-boundary.md)、検証時点は[ADR0044](../adr/0044-tests-at-completion-and-integration.md)に従う。
同一境界内の実装詳細であり、新しいADRは設けない。

規則: ARC-001/003/007/008/010/012、C-004/005/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。

## 比較と不採用案

全compile/measureの入力snapshot・raw stdout/stderr・終了値・成果物は`tools/record-run.py`で保存した。
原本は`D:/NeNeFolio/agents/616-text-validation/`、作業木/一時出力はD側の同名領域。
`compile-*-commands.json`には実argvがある。各候補のsource/object/exeと失敗原本を別名で保持した。

| 候補 | 内容と判断 |
| --- | --- |
| 1 | 各復号位置で非NUL ASCIIの8byte連続部を進める。ASCIIで短縮したが、日本語対照で悪化。不採用 |
| 2 | 1に非ASCII先頭の早期判定を追加。日本語1MiB読込中央値比1.116、編集1.079。悪化が残り不採用 |
| 3 | UTF-8検証後にNULをまとめて確認。一文字ごとの出力参照を減らしても、日本語の悪化は解消せず不採用。未許可の`memchr`等は追加しない |
| 4 | 先頭ASCII部分を一度だけ進めて旧復号ループへ渡す。固定CPU比較でも日本語編集が約12〜15%増え、不採用 |
| 5 | 全文非NUL ASCIIだけ早期受理し、それ以外は旧検証。対照と末尾非ASCIIの費用を確認して採用 |

`inspect-01`の`llvm-objdump`で、候補2/3には非ASCIIごとのprefix開始分岐が増えることを確認した。
候補4の追加静的読取では、旧復号ループと同じ命令列でも周辺の配置・レジスタに変化があった。
配置だけを性能差の原因と断定しない。補助直接実行のcandidate-04 assemblyもrawを保持する。

最初の測定は各版5sampleをA/B/B/Aとし、block内のcase順も反転した。
旧版の日本語16MiBだけでも約25〜42msに揺れたため、採否には固定CPUの補助計測を追加した。
対象observer threadだけaffinity mask4を指定し、各入力/APIでA/B/B/Aを近接させ、各版10sampleを得た。
1sampleの反復は1KiBが8192回、1MiBが16回、16MiBが2回。QPC経過とthread cycleを保存する。
observer以外のaffinity・優先度・実app・Ledgerのprocessは変更しない。

固定toolchainの正典Release `compile_commands.json`から、note_text/utf8_text/text_bufferの
O2/Ob2/DNDEBUG/MT/C23・警告vectorを取り、repo外の小さなQPC observerへ結んだ。
計時のための製品コード・test hook・新しい製品経路は加えない。
固定CPU observerの導入時は、候補4と旧版の製品objectを再利用し、observerだけ再リンクした。
計測環境はWindows 11 Pro 10.0.26300、Core i9-10850K、20 logical processors。
最初の環境記録で既知のbuild/test processは0、各比較中に親のbuild/hash/archiveを重ねていない。
全OSの無負荷、排他的CPU、一定クロック、cache状態は保証しない。

入力は固定パターンのASCII、日本語と英数字の混在、日本語の9file（各1KiB/1MiB/16MiB）。
完全なUTF-8列を繰り返し、サイズ調整の末尾だけASCIIを置いた。原bytes・SHA256を保存する。
入力読取・hash・warmup・受理bytes照合は計時外。計時中も結果と長さを確認する。

### 採用候補5の実測

`measure-fixed-02`のASCII中央値。単位ms、各版各case10sample。

| 入力 | 読込: 旧→候補 | 編集: 旧→候補 |
| --- | ---: | ---: |
| 1KiB | 0.001258→0.000143 | 0.002653→0.001447 |
| 1MiB | 1.678581→0.274650 | 3.253478→2.412275 |
| 16MiB | 21.881850→4.093825 | 43.298025→26.567100 |

同じ回の1MiB対照は、混在の読込1.566747→1.598322ms・編集2.859756→2.853541ms、
日本語の読込1.531219→1.490772ms・編集2.841844→2.860019ms。
16MiB日本語は最初の読込比1.099・編集比0.911と大きく振れたため、その記録を残したまま限定再確認した。

`measure-late-01`は、ASCII末尾3byteだけ日本語とする不利な分布3fileを追加し、既存の日本語16MiBも再利用した。
末尾日本語の中央値比は1KiB読込1.049/編集1.041、1MiB読込1.043/編集1.011、16MiB読込1.064/編集1.047。
日本語16MiBの再確認は読込24.167550→24.264300ms、編集45.408200→45.941325msだった。
cycle比も原本に保持するが、cycle数を一定周波数のCPU時間へ換算しない。

全標本とrangeは`paired-01`〜`paired-04`、`paired-fixed-01/02`、`paired-late-01`のJSONとsummaryにある。
不利な分布や不採用候補を最終値から隠さず、一般倍率、全UTF-8分布、実UIの時間上限は主張しない。

## 対象テストと静的確認

`tests/unit/text_tests.c`へ、独立期待値による次の境界を加え、既存`run_text_tests`へ結ぶ。
対象selectorは既存`--text-validation`を再利用し、新入口は増やさない。

- 長さ0〜40、開始offset0〜7、終端なし・正確な確保長で受理bytesと入力不変を確認する。
- 25位置×全256byte×開始offset0〜7で、ASCII1〜127の受理、NUL、孤立/不正UTF-8と失敗out不変を確認する。
- 2/3/4byteの上下限・BOMの本文中保持・過長・surrogate・上限超・切断・不正continuationを、prefix1〜17とsuffix0/9で確認する。
- 0x01/0x7Fの反復、NUL位置15と後方32の不正UTF-8により、zero検出と拒否優先を確認する。

既存のBOM、正規化、NUL、16MiB境界も同selectorで再利用する。新確保はなく、別のOOM sweepは加えない。
親が実装/計測/対象検証を直接実施し、`text_validation_review`は同じ既定モデルで独立静読だけを担当した。
候補1/4/5に正当性の必須指摘0。NUL位置を15へ動かす提案を採用。編集・build・test・追加copyの委任はない。

Debugのformat/tidy/ASan/UBSan/nullability付き対象buildと単体結果は、下の完了追記で確定する。
整形後の製品sourceは採用計時sourceと空白除外tokenが一致し、性能計測を反復しない。
最終PR headの必須`pwsh -NoProfile -File ./eng/check.ps1`を統合条件とし、対象結果だけで統合完了とは扱わない。

## 準備・検証時の停止も保持

- `compile-baseline-01`は補助の引用符処理で版macroを壊し、compilerが拒否した。子1/記録125、exe未生成の記録error1。
  Windowsの`CommandLineToArgvW`で実argvを復元する補助へ直し、新規baseline-02へ成功出力を作った。製品sourceは変えない。
- compile03未完了時の補助result読取と、未生成input-listの測定準備が停止した。測定子/run-dirは未作成。
  compile完了を確認してから開始したmeasure-03とは分けて記録する。
- 整形再利用の補助readはencoding省略で停止し、UTF-8指定へ直して同じ成功結果を照合した。
- `build-debug-01`は新testのnesting level4をC-012上限3で拒否し、exe未生成。
  バイト置換の確認とalignment用fixtureの配置を分けて直し、上限・除外・抑制を変えずbuild-debug-02へ進んだ。
- 推測した文書path等の読取失敗、補助診断の全体経過は`D:/NeNeFolio/agents/quality-20261105/journal.md`に残す。

## 証跡・統合・限界

通常app・実ノート・Storeは試験に使わない。今回の公開入口計測はファイルI/O、Markdown/RTF生成、検索全体を含まない。
確定head/CI/main、通常版反映、data不変、全保全と安全整理の原本は
`D:/NeNeFolio/agents/616-text-validation/integration-completion.json`へ記録する。
追加作業木と不要出力は取込・未保存/ignored・唯一の成果物・稼働参照・リンクを確認後に整理し、branch/commitを保持する。

## 対象検証の確定

`configure-debug-01`は正典Debug設定で成功。`build-debug-01`の新test nesting違反を修正し、
`build-debug-02`で残り27actionsを完了した。format・警告error・tidy・ASan/UBSan/nullabilityとrecover禁止は維持する。
`text-validation-01`は`folio_tests.exe --text-validation`を実行し、子/記録0、stdoutは`text validation tests passed`、stderr空。
両成功runの入力は終了時も不変、記録error0。新規確保や公開selectorは追加していない。
文書/規約検査と最終CIの確定結果は同taskの実run・完了原本を参照する。

## 08:12 — #616の統合・反映・整理を完了

PR619最終head`a7295603`の必須CI41930597783 full-gate/checkが成功し、main`2c2f0be`へ正典統合した。
通常版buildとdata32fileのbytes/hash/作成更新日時不変を確認。通常app起動0。
1,378fileをZIPへ全hash照合し、取込tree/dirty/ignored/リンク/稼働参照の監査後に専用worktree/outputを削除した。
branch/commit保持。Issueはmerge後30秒でもOPENだったが、closingIssuesReferences/merge済みを照合して完了として閉じた。
確定原本はD:/NeNeFolio/agents/616-text-validation/integration-completion.json。
