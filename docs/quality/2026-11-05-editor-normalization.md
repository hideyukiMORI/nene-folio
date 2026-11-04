# 改行間の本文走査をまとめる — #620

`note_text_from_editor`の保存上限確認と改行正規化で、CR/LFを含まない連続部分をまとめて進める。
固定Releaseの合成16MiB入力では、改行のないASCII本文のLF編集入口が中央値26.95035→14.92035ms、
日本語本文が46.44125→34.84095msとなった。確保・複製・検証・破棄を含む公開入口の時間であり、
正規化だけや実UIの時間ではない。数文字ごとの混在改行では約6%増える費用が残る。

## 正典・契約・変更範囲

基点はmain `2c2f0be304256c2bf788d01ae306423fe2d23eb7`（#616統合後）。#616の旧値を今回の基点値へ流用しない。
`note_text_from_editor` → `validate` → `normalized_within_limit` → `fold` → `adopt`を維持する。
`src/core/note_text.c`のprivate `unbroken_width`で、残長が8byte以上のときだけ`memcpy`して確認する。
CR/LFとのXOR後にzero byteの存在を調べ、含まれるwordと短い末尾は個別に読む。
整数pointerへの変換、alignment/aliasing依存、入力の変更、終端を仮定した読み取りはない。
zero検出の借りによる余分なbitを位置として使わず、実byteを確認して最初の改行で止まる。

呼出し元は既存`break_width`で改行を先に扱う。非改行なら1以上進み、入力幅と出力幅は同じになる。
上限は加算前に残容量と比較する。CRLF→LFの縮小、LF/CR→CRLFの拡張、本文先頭U+FEFFの保護署名を含む
保存表現16MiBを維持する。不正UTF8→NUL→上限→確保の順、失敗out不変、append順と確保順も変えない。
既存decoder、公開API、schema、依存、thread、版、gate、許可集合は変更しない。
同じ境界内の実装詳細であり、新ADRは設けない。[ADR0042](../adr/0042-bom-at-file-boundary.md)に従う。

規則: ARC-001/003/004/007/008/010/012、C-004/005/008/012/014/016、QLT-008/009/010/012/013、GIT-001〜004。
追加waiver none。既存WVR-0001はWinHTTP callbackだけで、今回の変更へ適用しない。

## 計測と不採用案

正典Releaseの`compile_commands.json`からnote_text/utf8_text/text_bufferの実vectorを取得し、
同じO2/Ob2/DNDEBUG/MT/C23・警告設定でrepo外のobserverへ結んだ。製品への計装や新しい公開窓口は加えない。
`compile-*-commands.json`へ実argv/stdout/stderrを保存し、各候補のsource/object/exeは別名で保全した。

| 候補 | 内容・判断 |
| --- | --- |
| 1 | runと改行を別段階で数える。ASCII16MiB LF27.23260→13.08235msだが密集/短い混在行で約5〜10%増。不採用 |
| 2 | 従来の改行分岐を先に保ち、非改行だけまとめる。密集改行はほぼ同等。長い本文の改善と短い混在行の費用を確認して採用 |
| 3 | 先頭最大8byteを逐次確認してからword走査。短い混在行の悪化が約7〜9%へ増え、不採用 |

候補2/3の`llvm-nm`静読では走査helperの独立symbolはなく、単純な関数呼出し費用だけを原因としない。
この補助だけは直接実行し、rawを`candidate-02/03-symbols.txt`へ保存した。
採用時は候補2の記録snapshotからsourceを復元し、整形後の空白除外token一致を確認した。性能は再計測しない。

入力は改行なしASCII/日本語、78文字ASCII行/日本語行、CR/LF密集、数文字ごとの混在改行+BOMの6分布。
各1KiB/1MiBに加え、ASCII長文・日本語長文・ASCII行は16MiBも使い、LF/CRLFの計30caseとした。
固定patternの不足末尾はASCIIで埋める。期待出力はPythonのbytes置換で作り、製品を呼ばない。
fixture全bytes/SHA256/期待出力を保存し、observerのwarmupで全出力bytesを照合する。
計時中は結果型と長さを確認し、作成と破棄を反復する。fixture読取・hash・warmup・JSON出力は計時外。

observerのthreadだけCPU affinity mask4、優先度は不変。各caseでA/B/B/A、各版10sample。
1sampleの反復は1KiBが2048回、1MiBが8回、16MiBが1回。QPC経過とQueryThreadCycleTimeを取得した。
Windows 11 Pro 10.0.26300、Core i9-10850K、20 logical processors。
計測前の既知build/test processは0、比較中に親のbuild/hash/archiveを重ねない。
OS全体のidle、排他的CPU、一定クロックやcache状態は保証しない。cycleを一定周波数の時間へ換算しない。

### 採用候補2の結果

`measure-02`の中央値ms。各版各case10sample、全標本/range/cycleは`paired-02.json`とsummaryにある。

| 入力 | LF: 旧→候補 | CRLF: 旧→候補 |
| --- | ---: | ---: |
| ASCII長文1MiB | 1.61600→0.85974 | 1.64029→0.85612 |
| ASCII長文16MiB | 26.95035→14.92035 | 26.63155→14.15595 |
| 日本語長文1MiB | 2.95896→2.12633 | 2.85614→2.08251 |
| 日本語長文16MiB | 46.44125→34.84095 | 46.39665→35.07045 |
| ASCII行1MiB | 2.10567→1.22016 | 1.99101→1.12602 |
| ASCII行16MiB | 35.60975→20.77695 | 36.21580→21.35465 |
| 日本語行1MiB | 3.29501→2.46020 | 3.32757→2.42059 |
| 密集改行1MiB | 7.21839→7.22336 | 7.98019→7.82145 |
| 短い混在行1MiB | 4.53204→4.79468 | 4.76536→5.07038 |

短い混在行の比はLF1.058/CRLF1.064。この不利な分布を含め、全入力が速くなるとは主張しない。
1KiB、候補1/3の全値も原本に残す。実UI・I/O・RTF・検索全体の速度や時間上限へ一般化しない。

## 対象検証

`tests/unit/text_tests.c`へ、明示したraw/LF/CRLF期待値8patternを追加し、既存`run_text_tests`へ結んだ。
prefix0〜24、suffix0/9、開始offset0〜7、両改行形式の6,400入力で出力bytes・終端・入力不変を確認する。
正確な確保長で終端なしの入力を渡し、CR/LF/CRLFが8byte境界を跨ぐ場合、UTF8/BOM、近い制御byteを含む。
既存のUTF8/NUL拒否順、正規化後サイズ、BOM込み上限、確保前拒否のテストも同じ`--text-validation`で使う。
新確保や別selectorは追加せず、既存の確保失敗検証は最終CIで維持する。

親が実装/計測/検証を実行し、`text_validation_review`は親と同じ既定モデルで候補2/3の独立静読を担当した。
範囲/zero判定/進捗/上限/append・確保順に修正必須指摘0。既存editor成功testの出力比較不足という指摘を上の追加で補った。
子による編集/build/test/追加copyはない。詳細は同taskの`independent-review.md`。

## 原本・統合・限界

全compile/measure/configure/build/testの入力snapshot・成果物・raw・終了値は`tools/record-run.py`で保存する。
恒久原本は`D:/NeNeFolio/agents/620-editor-normalization/`。全体経過は`D:/NeNeFolio/agents/quality-20261105/journal.md`。
対象build/testと文書検査の実結果、最終head必須CI、正典merge、通常反映、data不変、全保全/整理の確定は
`D:/NeNeFolio/agents/620-editor-normalization/integration-completion.json`を参照する。
対象検証だけを完了とは扱わず、最終PR headで必須`pwsh -NoProfile -File ./eng/check.ps1`を通す。
実app/実ノート/Storeは試験に使わない。D側専用worktree/outputは取込/未保存/ignored/リンク/稼働参照の監査後に整理し、branch/commitを保持する。

## 対象検証の確定

configure-debug-01成功。build-debug-01は新testのnesting level4をC-012上限3で拒否し、未生成exeの記録error1を保持した。
既存validation_bytesへ確保を寄せて関数の配置を移し、build-debug-02で残り27actionsを完了した。
format/tidy/警告error・ASan/UBSan/nullability・recover禁止を維持し、製品sourceは採用計測から変えていない。
text-validation-01はfolio_tests.exe --text-validationで子/記録0、stdout成功、stderr空。
成功runは記録error0、全入力は終了時も不変。既存の確保順を保つため、重複した独自OOM sweepは追加しない。
文書/規約検査と最終CIは実runと完了原本へ記録する。
