# コードブロックの言語指定と色分け — #524

## 変更

hideの依頼により、#522の画像修正に続いてfenceの言語指定を実装した。
`code_language`が先頭info語・aliases・基本字句規則を所有し、`code_syntax`がブロック内だけの状態で
keyword/string/comment/numberを着色する。既存`markdown_blocks`→`markdown_rtf`→RichEditへ合流し、
文字escape・container/fence判定を複製しない。C/C++、JS/TS、Python、JSON、Shell、PowerShell、SQLに対応する。
未指定/未知語は単色、編集は原文のまま。対応名と制限は[Markdownの書き方](../MARKDOWN.md)に記した。

`rtf_palette`の4役割を既存色表7〜10へ追加し、127配色の固定値を`folio_theme`だけに収載した。
508組のコード背景との比は最小4.5072496。単体検証も各配色の4役割に4.5:1以上を要求する。
高コントラストではOSのwindowTextへ揃え、palette比較にも新役割を含める。

規則: ARC-001/003/004/005/008/009/010/012、C-002/003/005/008/011/012/013/016、QLT-008/009/012/013、GIT-001〜004。
設計を先に[ADR0086](../adr/0086-fenced-code-syntax.md)へ記し、仕様・用語・配置・利用案内を更新した。
保存schema・版・依存・Store提出物は変更しない。追加waiverはnone。
既存WVR-0001はWinHTTP callback ABIだけに適用し、今回の変更には使わない。

## 対象検証

Windowsの隔離desktopと実RichEditを使用。正規Debug targetのclang-tidy・ASan/UBSan/nullabilityを有効にした。
原本は`D:/NeNeFolio/outputs/524-code-syntax/`、恒久記録は`D:/NeNeFolio/agents/code-syntax-20261103/`。
`tools/record-run.py`で実行前入力と実行後成果物のsnapshot/hash、raw出力を保持する。

| 実行コマンド・記録 | 結果 |
| --- | --- |
| `cmake --build build --target folio_tests minimap_structure_tests nenefolio`（最終`syntax-08`） | 成功。最終製品入力のコンパイル/静的解析 |
| `build/folio_tests.exe --markdown-rtf`（最終`syntax-08`） | 成功。各言語/aliases/未知語、文字列内のコメント記号、複数行引用/コメント、SQL入れ子、PowerShell here-string、未閉じ/escape、日本語、64KiB長文、fence/container/EOF、原文/plainを確認 |
| `build/folio_tests.exe --themes` / `--system-contrast`（`target-05`） | 成功。127配色、比較、設定経由のOS色尊重を確認 |
| `ctest --test-dir build -R '^minimap_structure$' --output-on-failure --no-tests=error`（最終`syntax-08`） | 168checks成功。実際の文字・色、明暗/合成HC色、原文編集への復帰、選択/dirty/Undo、minimapを確認 |

`target-01`は実行スクリプトのtarget名大小の誤り、`target-02`はテスト入口の61行、`target-03`は
長文fixtureの確保後null分岐、`target-04`はWin32定数のheader不足を拒否した。検査を弱めず呼出し/fixtureを修正した。
`target-05`はunit3件が成功したがnativeの期待がCR/末尾段落を誤っていたためrun全体は失敗。
`native-06`でUTF-16全値を観測し、GetWindowTextのCRLFとRichEditが末尾段落を吸収する既存契約に照合した。
その期待を修正した`native-07`が成功した。文字欠落を期待値変更で隠したものではない。
続く`policy-01`で未許可の外部symbol `strchr`を検出し、終端までの局所走査へ揃えた。
許可表は広げず、影響するMarkdown unitとnativeを`syntax-08`で再実行して成功した。
配色/HC関係入力は不変のため`target-05`のtheme/contrast成功を再利用する。

新しいownerと色付き出力の確保失敗は既存測定ビルドの全確保失敗注入へ追加した。
通常Debugのunitは注入inactiveを明示し、注入の成功証明にはしない。
確保失敗・分岐coverageを含む最終headの必須`eng/check.ps1` CI、main統合、通常exe反映、data保持、
追加作業物の保全整理は同agentsの`integration-completion.json`で確定する。CI成功前に完了扱いにしない。

## 限界

基本字句の色分けであり、意味解析やコード実行ではない。補間式の内側、JS正規表現、C++ raw literal、
Shell here-document、SQL方言固有の引用は厳密な色分け対象外。全ての原文は保持する。
編集画面のリアルタイム着色・言語選択GUI・言語見出しは追加していない。
native確認は観測Windows/RichEditと合成入力の範囲で、全OS/DPI・物理IME・全言語方言の保証ではない。
