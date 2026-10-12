# 編集本文の有限境界と BOM の多重除去（#270 / #271）

Issue: [#270](https://github.com/hideyukiMORI/nene-folio/issues/270)（調査・記録）、
[#271](https://github.com/hideyukiMORI/nene-folio/issues/271)（修正）。FR-005/006/007/008/033、ADR0006/0042。
規則: ARC-001/004/007/008/009/010、C-003/005/007/008/014/016、QLT-008/009/013。Waivers: none。

## 修正前の有限境界

main `2f4f334`（製品ソースは `130bfda` と同一）を、利用者の data から独立した実 Win32 probe で測った。
Windows `10.0.26300`、msftedit の数値版 `10.0.26100.9549`。非表示の RichEdit と正典の
`note_text → markdown_rtf_plain → note_pane_edit → note_pane_text → note_text_from_editor` を使用した。

75文字 × 18文脈 ＋ 22追加 = **1,372ケース**。C0/C1、DEL、FFFD、FEFF、ゼロ幅空白、NBSP、結合文字、
日本語、絵文字、RTF escape、既拒否のLS/PSについて、単独・反復・先頭/末尾・LF/CR/CRLF隣接を測定した。
最初の改行がCRならCRLF、LFならLFへ揃える既存の正規化と、文字の欠落を区別した。

| 対象 | 結果 |
| --- | --- |
| 受理本文のRichEdit内部UTF-16 | 1,318/1,318一致。特殊文字そのものを保持 |
| coreの再保存本文 | 1,314/1,318一致。差4件は先頭の連続U+FEFF |
| U+0007 / U+2028 / U+2029の編集拒否 | core 54/54、実application代表3/3で拒否 |
| 実保存代表 | matrixの受理34ケースと、別fixtureの再現10ケース。全1,372件の実保存ではない |
| RichEditの変更印 / readonly | 全受理ケースで0/0 |

実保存34件のraw差9件は、孤立CRや混在改行の意図した正規化だった。
再現10件のraw差4件は、孤立CRの正規化2件と先頭FEFFの欠落2件。両群には重複条件があり、44独立条件とは数えない。
大きなfixtureの同期が遅かったため、完了済み1,297行を再利用し、残り75行の往復を追測した。
全結果のid・件数・重複なしを集計スクリプトで確認した。

## #271 の再現と原因

ファイルraw `EF BB BF EF BB BF EF BB BF 41`（FEFF×3+A）は、次の順で文字が失われた。

1. `file_bytes`と`note_text_create`がそれぞれ先頭を1個除き、application本文はFEFF+A。
2. RichEditはFEFF+Aを保持し、打鍵なし・変更印0。
3. `note_text_from_editor`がさらに先頭を1個除くため、applicationはCHANGEDと判断する。
4. 未編集保存がREADYとなり、rawと再読込本文はAだけになる。履歴は1件増える。
5. 履歴rawも元rawからFEFF1個が減り、履歴読取でさらに2個減ってAだけになる。

通常のBOM1個+Aは本文A、SAMEとなり、元rawと履歴0件を保持した。
FEFF×4+Aも未編集保存で減り、改行後のFEFFは保持された。RichEditの受理文字集合の問題ではない。
本文コピーの`note_corpus_put`と閲覧時別名保存にも、ファイル入力用の生成関数を再使用する同じ原因があった。

ADR0042で、rawファイルの取得と形式の解釈を分け、BOMをファイル入力時に一度だけ消費すると決定した。
本文のコピーと編集入力はFEFFを保持する。本文先頭がFEFFの保存時だけ署名を1個付け、再読込で本文が減ることを防ぐ。
16 MiBの上限にはその署名も含める。通常の出力、履歴配置・保持数、JSON schemaの版は変えない。

## 修正前のコマンドと証跡

保存先: `D:/NeNeFolio/agents/2026-10-12-text-boundary/`。

| コマンド | 結果 |
| --- | --- |
| `pwsh -NoProfile -File D:/NeNeFolio/agents/2026-10-12-text-boundary/run.ps1` | compile exit0。最後の75件はexit1（FEFFの差4件）。先行1,297行と結合 |
| `pwsh -NoProfile -File D:/NeNeFolio/agents/2026-10-12-text-boundary/run-repro.ps1` | compile/run exit0。10件の観測用で、差が無いことを示すexit0ではない |
| `python D:/NeNeFolio/agents/2026-10-12-text-boundary/analyze.py` | exit0、全1,372件の集計一致 |

`combined-results.tsv`、`cases.json`、`summary.json`、`metadata.json`と、`application-repro/results.tsv`・
`history-files.json`にraw/本文/内部単位列と履歴を保持する。再生成用のC/Python/PowerShellも保持する。
setup不一致、途中停止、fixtureのJSON順序修正などの試行はattemptログに分離した。
ASan起動時の既知の`interception_win: unhandled instruction`警告はstderrへ保存。追加のsanitizer診断はない。

有限matrixは全Unicode・大量反復・長文・別OSを網羅しない。字形・WM_PAINT・IME・物理打鍵・別DPIと#243の性能は未測定。
過去に欠落した文字の自動復旧はできず、修正後の履歴raw保持で過去の履歴を修復したことにはならない。

## #271 修正後の対象検証

`file_bytes`は全raw bytesを公開し、`note_text_create`と`json_reader_create`が共通の署名判定で先頭1個を消費する。
`note_text_copy`は検証済み本文を複製し、`note_corpus_put`と閲覧時別名保存がこの経路を使う。
本文と署名付き保存表現は同じ所有バッファから借り、adapterのwrite/createが保存表現を使う。
最終ソースと統合・exeの値は[当日日報](../reports/2026-10-12.md)の再開後の節を参照する。

旧実装に先頭U+FEFFを含む編集本文の期待値を加え、build exit0 / run exit1（`FAIL: folded body`）を確認した。
これは新APIが未実装でコンパイルできないという失敗ではなく、既存の本文欠落を捕捉した回帰である。
修正後の`folio_tests.exe --bom-boundary`はexit0。
署名0〜4個、コピー・保存往復、署名込みサイズ境界・改行の収縮/膨張、不正UTF-8/NUL優先、
JSON共通読取と台帳2種・設定・改名記録、no-write、実編集、VIEW/EDIT別名保存、検索写し、履歴を確認した。
ログ: `D:/NeNeFolio/agents/271-bom-impl/`の`red.log`・`green.log`と各build log。

独立した実OS probeは同じWindows/msfteditで、候補srcとASan付きの4 librariesを固定して実行した。
測定の開始・終了でsrc全件とlibのSHA256不変を確認した（`final-proof.json`の`source_stable`・`libraries_stable`）。

| 実測 | 結果 |
| --- | --- |
| BOM0〜4個の読込→RTF→RichEdit取得→未編集保存 | 本文は署名1個だけを除いた値、SAME。元raw/更新時刻不変・履歴なし |
| 先頭FEFFの実入力、追記保存、別名保存 | 本文FEFFを全て保持。VIEWのコピーとEDITの入力、検索写しも一致 |
| 履歴raw・読取・復元保存 | 元rawをBOM込みで保持し、履歴本文も復元後本文も期待と一致 |
| JSONファイルのBOM | note ledger/settingsの実adapterが1個を受理し2個をMALFORMEDで拒否。文字列中FEFF保持 |
| 物理サイズ | 本文16,777,213 bytes＋署名3 = 16,777,216 bytesをcreate/write/read可能。+1はTOO_LARGE/UNWRITABLEで拒否、元ファイル不変 |
| 別processでの再読込 | 保存した2ノートの本文一致 |

`pwsh -NoProfile -File D:/NeNeFolio/agents/271-bom-probe/run.ps1`の本体は**175 assertion、exit0**。
最初のrestartだけは、JSON2個拒否のfixtureを残したためstate生成に失敗（exit2）。
fixtureを正当な空台帳と設定へ戻し、`probe.exe --reload`だけ追測して**8 assertion、exit0**を確認した。
本体175項目を繰り返していない。`finalize.ps1`では別途rawの16進列/SHA256/サイズ13件一致とprobe process 0を記録した。
製品の修正は追加していない。既知のASan起動時警告はstderrに保存し、追加の診断はなかった。

証跡: `D:/NeNeFolio/agents/271-bom-probe/`の`report.md`・`results.tsv`・`restart-followup.tsv`・
`metadata.json`・`final-proof.json`・C/PowerShell sourceとログ。実行した有限範囲と未測定の範囲を分け、
修正前の1,372ケース全てを再度実保存したとは主張しない。

## 最終ゲートと受理

製品source `3b1747e`で、`pwsh -NoProfile -File ./eng/check.ps1`はexit0。
本文型・rawファイル読取・共通JSON readerの変更が全consumerへ届くため、その具体的な影響を正典ゲートで確認した。
conformance/symbols違反0、CTest 2/2、分岐3272/3486 = 93.861%、negative 7.86%の閾値未達拒否、
実ツールproof 19/19。測定ビルドでは、先頭FEFFを含むcreate/copy/from_editorの各確保1〜8回目を失敗させ、
out不変・元本文保持・所有元の解放を確認した。ログは`D:/NeNeFolio/agents/271-bom-impl/gate.log`。

実OSに渡した`production-candidate.diff`と最終`production-final.diff`は、ともにSHA256
`BC057001789B17D254F8920A9C62E3119A6EAB98EB6DFC6269CDFA7A0AAB09EE`。
設計席が12 productionファイルの差分、raw読取の全consumer、コピー2か所、所有バッファと上限判定を確認し技術受理した。
成功済みの対象検証は、製品source不変のpush/Ready/mergeで再利用する。ゲートの抑制・除外・閾値・依存変更は無い。

16 MiBの実測はpersistence port/file_bytesの保存と再読込までで、巨大RichEdit表示は未測定。
JSON実adapterは2代表で、改名記録の実adapterは今回未測定（全codecは単体で確認）。
目視・物理IME・別DPI/別OSを確認済みにはしない。Waivers: none。
