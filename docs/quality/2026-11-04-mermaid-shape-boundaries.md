# Mermaidノードの閉じ境界 — #543

## 問題と変更

11月4日の品質改善調査で、対応済みの台形を平行四辺形へ同じ文内でつなぐと、
先頭ノードの解析が後続ノードの終端を選び、辺と後続ノードをラベルへ吸収して成功する不具合を見つけた。
修正前の `flowchart TB / A[/label\] --> B[/label/]` は2ノード・1辺の契約試験で失敗した。

`src/core/mermaid_flow.c`の既存ノード解析で、最長の開始記号を優先し、同じ開始記号の候補から
最初の終端を選ぶ。先行する引用外の `]` / `)` / `}` を越えた終端は拒否し、
不完全な開始/終端を別形状や部分図として採用しない。引用内の括弧とescaped quoteは従来どおり保持する。
辺ラベルが使う共通の引用走査、図の配置・描画・worker、原文・schema・版1.2.0・依存は変更しない。
形の契約は[既存対応表](../MARKDOWN.md#mermaidの図)と[ADR0089](../adr/0089-native-lazy-mermaid.md)に従う。
参照した[固定上流grammar](https://raw.githubusercontent.com/mermaid-js/mermaid/1fb7869866dd619168d840291c9c54b4f129758b/packages/mermaid/src/diagrams/flowchart/parser/flow.jison)でも、引用内の文字と構造区切りを分ける。

## 対象検証と全試行

原本は `D:/NeNeFolio/outputs/543-mermaid-shape-end/`。各実行は`tools/record-run.py`で
source/test/config/script/exeの入力snapshot、raw stdout/stderr、子と記録側の終了値を保存した。
構築は固定toolchainの `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug` と
`cmake --build build --target folio_tests --parallel 2`。DebugのASan/UBSan/nullabilityとclang-tidyを維持する。
対象試験は `build/folio_tests.exe --diagram`。

| run | 結果と判断 |
| --- | --- |
| build-red-01 | production未変更の対象build成功、子/記録側0 |
| red-01 | 子/記録側1。台形→平行四辺形で端点/辺が欠落する回帰試験が失敗。記録失敗ではない |
| build-green-01 | 開始記号と最早終端の候補build成功。自己/独立レビューで先行する不適合な閉じ括弧の条件を見つけ、実行検証前に追補 |
| build-green-02 / green-01 | 同種の閉じ括弧境界の修正と対象試験が成功。独立レビューで異種括弧の条件を追補 |
| build-green-03 / green-02 | 最終候補のbuildと対象試験が成功。子/記録側0、recordingErrorsなし、stderrなし |

追加した試験は14形状の同一文内連鎖196通りについて、ノード数・辺数・shape・label・接続先を確認する。
引用内の全種括弧/escaped quote、終端欠落、同種/異種の不適合終端を越した後続ノードの吸収も確認する。
既存のdiagram対象に含まれる方向・layout・図原文収集と拒否境界も同じ実行で成功した。
変更のないnative描画/アプリ操作と無関係なローカル全件回帰は再実行していない。
確保点の追加はなく、確保失敗注入と全体分岐率は最終headの必須CIで確認する。

編集ツールの応答待ちを中断した回ではproduction側だけの適用を実ファイルで確認し、
未適用の試験差分を補った。重複したproduction patchは旧行不一致で拒否された。
この間にbuild/testは開始しておらず、最終の整形・差分確認後にbuild-green-03を新規run-dirで実行した。
初期の存在しない補助ファイルへの探索等は、継続作業のjournalへ記録した。

## レビュー・統合・保証範囲

hideの11月4日の明示許可に基づき、主担当が調査・実装・検証し、`mermaid_parser_review`が
読み取り専用の独立レビューを担当した。二段階の不正終端条件を追補し、07:25時点の最終差分で指摘解消を確認した。
別の`media_lifecycle_review`で見つけた画像cacheの条件と、引用pipe辺ラベルの条件は別焦点で追跡する。
主担当と同じモデル設定を使用し、旧役割分担を継承していない。

自己レビュー: ARC-001/003/004/008/010/012、C-002/005/008/012/014/016、
QLT-008/009/010/012/013、GIT-001〜004。設計境界と正典経路は維持し、ゲート/抑制/APIを変更していない。
追加waiverはnone。既存WVR-0001はWinHTTP callbackに限定して継続する。

最終headの `pwsh -NoProfile -File ./eng/check.ps1` と必須check、main統合、通常exe反映、
data照合、成果物保全/整理は `D:/NeNeFolio/agents/543-mermaid-shape-end/integration-completion.json` で確定する。
対象試験成功だけを統合完了として扱わない。Mermaid全構文・全図の視認性・hideによる実機受理の保証には広げない。
