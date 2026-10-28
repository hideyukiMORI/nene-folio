# Markdown閲覧の孤立CR行境界 — 2026-10-29

Issue: [#402](https://github.com/hideyukiMORI/nene-folio/issues/402)。基点main
`1845f9ffe4862d37516b0e87a8c98af5c5ca4f39`。
規則: ARC-001/003/008/012、C-002/008/012/014/016、QLT-008/009/012/013、GIT-001〜004。
設計は[ADR0070](../adr/0070-markdown-syntax-and-native-tables.md)の既存行境界責務内に収める。
Waivers: none。

## 不具合と変更範囲

`markdown_line_read`はLFだけを行境界とし、行内の孤立CRは共通RTF escapeで捨てられていた。
`one\rtwo`が`onetwo`になり、見出し・fence・表も別行として認識できない。
plain編集・保存時の改行正規化・本文先頭行の取得は既にCRを改行として扱うため、閲覧との不整合だった。

`src/core/markdown_line.c`の既存readerをCRまたはLFまで読む形にし、次位置はCRLFだけ2bytes進める。
LFと孤立CRは1byte、LFCRは2つの境界とする。長さの外にあるLFを取り込まず、末尾CRの後も越境しない。
元sourceを借用する同じblock/inline/table経路を維持し、第2の正規化本文やparser経路は作らない。
保存原文・保存改行方針・plain編集・共通RTF escape・Win32投入処理・公開API・schema・依存・gateは変更しない。

`tests/unit/markdown_tests.c`で、次の利用者契約と境界を既存`--markdown-rtf`へ加えた。

- CRのsoft breakに独立した期待RTFを置き、語間が失われないことを確認する。
- 同じ複合文書のCR版/CRLF版がLF版と完全に同じRTFになることを確認する。
  連続見出し、空行、通常/明示改行、setext、list/継続本文、quote、水平線、閉じた/未閉じfence、表を含む。
- 混在改行、連続CR、LFCR、空のCR/CRLFを確認し、CRLFが二重改行にならないことを確認する。
- 非NUL終端配列の末尾CRとEOF、長さ指定でCRLFの途中に切れた入力を確認する。
- 日本語・emoji・brace・backslash・表内のescapeしたpipeを含め、UTF-8とRTF escapeの保持を確認する。

## 修正前redと初回候補の失敗

QLT-008/012に従い、実不具合を実証する最小回帰として`one\rtwo`の期待を先に追加した。
固定toolchainのDebug buildで`folio_tests.exe --markdown-rtf`を実行しexit 1。
実出力`onetwo`、期待`one two`で失敗した。製品readerはこの時点では基点のままだった。

readerと追加回帰を整えた初回候補はbuild/tidyに成功したが、対象実行はexit 1だった。
新規回帰を通過した後、既存`verify_emphasis`に残っていた`a\rb → ab`の期待で失敗した。
これは今回直す誤動作を固定していた期待であり、Issueの受け入れ条件とplain編集契約に合わせ`a b`へ更新した。
製品コードの追加変更は行わず、変更したunitを再buildして対象実行を再確認した。
初回失敗は`attempt1-final.log`と`attempt1-final-result.json`へ保存し、最終成功と区別する。

## 対象検証と証跡

恒久証跡は`D:/NeNeFolio/agents/402-markdown-line-endings/`。
作業木は`D:/NeNeFolio/worktrees/402-markdown-line-endings`、build一時出力は
`D:/NeNeFolio/outputs/402-markdown-line-endings/build`。
`run-red.ps1`、`run-final.ps1`に実行コマンドを保存した。いずれも`eng/toolchain.ps1`を読み込む。

| 検証 | 実行と結果 |
| --- | --- |
| Debug configure | `cmake -S . -B D:/NeNeFolio/outputs/402-markdown-line-endings/build -G Ninja -DCMAKE_BUILD_TYPE=Debug`、exit 0 |
| 修正前red | 同buildの`folio_tests.exe --markdown-rtf`、追加したCR語間の期待でexit 1 |
| 初回候補 | `cmake --build D:/NeNeFolio/outputs/402-markdown-line-endings/build --target folio_tests`はtidy込みexit 0、対象unitは既存誤期待でexit 1 |
| 最終対象build | 同じbuildコマンド、変更したunitのコンパイルとlink、tidy込みexit 0 |
| 最終Markdown unit | 同buildの`folio_tests.exe --markdown-rtf`、exit 0。DebugのASan/UBSan/nullability、回復なしの正規構成 |
| 対象format | `clang-format --dry-run --Werror src/core/markdown_line.c tests/unit/markdown_tests.c`、exit 0 |
| 対象diff | `git diff --check -- src/core/markdown_line.c tests/unit/markdown_tests.c docs/quality/2026-10-29-markdown-line-endings.md`、exit 0。新規文書の参照はconformanceでも確認 |
| 文書と規約 | `python eng/conformance.py`、exit 0、0 violations。当日日報・引き継ぎ・currentを含む参照整合を確認 |

最終対象unitは既存のplain編集とapplicationの原文Save/SaveAs/EDIT保持も実行する。
確保失敗注入はcanonical buildではinactiveであり、`rejected=0`を注入成功として数えない。
今回のreader変更は確保を増やさないため、注入用buildを追加していない。

`red-inputs.json`と`final-inputs.json`に入力SHA-256、各result JSONにコマンド・終了値・範囲を記録した。
最終製品readerのSHA-256は`58B28B9C84F49B000609E223930127B9D3E7C691C7BEC6AC38D17D5EC2B482A1`、
最終Markdown unitは`AC77F76B66411DF39BE8D23CE20387DF1FC4E5F710CB98B48AFF216C805E1BE2`。
担当交代・commit・push・mergeだけを理由に、この成功対象検証を再実行しない。

## 受理・統合時の確認と限界

対象format/diffは`static-result.json`へ記録した。設計サナが完成差分・独立した期待RTF・境界条件をレビューし、
当日の関連文書を含むconformanceを確認した。未解決の修正指摘はない。
ローカル全件gateは実行せず、最終PR headの必須CI `pwsh -NoProfile -File ./eng/check.ps1`を統合条件とする。
この記録の対象unit成功は、必須CI成功やmain統合の代わりではない。

native処理は変更せず、同じ論理文書のRTF一致で既存投入契約へ戻るため、
[既成功のnative検証](2026-10-26-markdown-syntax.md)を再利用する。GUI・通常data・配布物は操作していない。
list内quoteから親list継続への復帰など既知のcontainer制限は今回の対象外で、[対応範囲](../MARKDOWN.md)を維持する。

main統合・必要反映・恒久成果物の収載後に、親担当が絶対パス・取込状態・未保存/未追跡/無視ファイル・
唯一の成果物・稼働参照・リンクを監査して追加作業木と不要な出力を整理する。branch/commitは保持する。
旧削除拒否済みの作業物は今回の対象外。
