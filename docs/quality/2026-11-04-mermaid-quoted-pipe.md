# 引用された辺ラベルのpipe — #546

2026-11-04、継続的な品質改善の独立R/Oレビューから、`A -->|"a|b"| B`が不正扱いになる条件を調べた。
前段`diagram_statement`は外側pipeと内側二重引用符を同じ`quote`へ入れ、内側pipeで引用終了と誤判定した。
後段`mermaid_flow`の`closing_at`は引用内pipeを保持しており、同じ入力に対する走査条件が一致していなかった。

## 正典経路と修正

既存の`diagram_statement_scan`へpipeの内外だけを追加し、同じ走査で二重引用符と独立して追跡する。
引用内では既存のescaped quote/backslash処理を使い、pipeを変更しない。pipeラベル内の括弧や
セミコロン・改行・コメント印は文の境界にせず、quoteまたはpipeが未閉じならモデルを公開しない。
sequenceの分割、ラベル保存、描画経路、公開API、保存schema、版、依存は変更しない。

上流固定commitの[string lexer](https://raw.githubusercontent.com/mermaid-js/mermaid/1fb7869866dd619168d840291c9c54b4f129758b/packages/mermaid/src/diagrams/flowchart/parser/flow.jison)
でも、二重引用符内のpipeは文字として扱う。nativeで既に扱うescaped quoteの契約はそのまま確認する。
上流grammar全体との完全互換を主張するものではない。

## 試行記録

作業木は`D:/NeNeFolio/worktrees/546-mermaid-quoted-pipe`、出力は同名の`outputs/`。
`tools/record-run.py`で入力snapshot・子コマンド・生成exe・raw stdout/stderr・終了値を保存した。
固定toolchainのDebug/Ninja、clang-tidyとASan/UBSan/nullabilityで`folio_tests`だけをbuildし、
実行対象は`folio_tests.exe --diagram`へ限定する。

- `build-red-01`: production未変更のmain`932bc18`へ対象試験を追加し、子/記録側0。
- `red-01`: 最初の引用pipeで`DIAGRAM_INVALID`（outcome 1）、子/記録側1、recordingErrorsなし。
  原文は`flowchart TB / A -->|"a|b"| B -->|plain| C; / C --> D["tail|x"]`。
- 修正前再現後、#544統合のmain`786d6d5`へffした。cache/CMake/文書だけでparser入力は不変。
- `build-green-01`: 修正したscannerを固定Debug/tidyでbuildし、子/記録側0。
- `green-01`: `folio_tests.exe --diagram`の解析・配置・遅延media対象が成功、子/記録側0。
  build/実行ともrecordingErrorsは空、stderrも空。下記境界と既存関連試験を確認した。

追加正例は引用内pipeの0/1/2本、escaped quote/backslash、改行/セミコロン/括弧/コメント印、
文字参照と既存の改行escape、非引用の通常pipeラベル。各例で4ノード・3辺とラベル全文を照合し、
後続の辺と引用されたノードラベルを消費しないことを確認する。未閉じquote/pipeの4例は部分モデルを拒否する。
実ノート・稼働app・実通信は使わない。

08:04:46の`mermaid_parser_review`による独立R/Oレビューは追加指摘なし。親と同じモデル設定で
静的読取だけを担当し、編集/build/実行/アプリ操作は主担当が行う。読み取りコマンドは全て終了し、追加成果物なし。
整形・conformance・symbols・diff確認は`verify-01`へ記録し、最終headの必須CIを統合条件にする。
read-onlyコマンドとbuild後の記録に一時的な待ちが出たが、最終resultまで確認して成功を判定した。

## 規則と限界

ARC-001/003/004/008/010/012、C-002/005/008/011/012/014/016、QLT-008/009/010/012、GIT-001〜004、ADR0089。
構文の正典経路と所有者を維持し、抑制・閾値・除外を追加しない。追加waiverはnone。
既存WVR-0001はWinHTTP callbackだけへ継続する。

最終head/CI/main/通常反映/data照合/保全整理は
`D:/NeNeFolio/agents/546-mermaid-quoted-pipe/integration-completion.json`へ収載する。
