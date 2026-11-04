# 横向きの辺と斜辺付きノードの接続 — #554

2026-11-04の継続品質改善。矩形中央を使うflow辺の端点が、parallelogram/trapezoidの斜辺や
asymmetricの左切込みから離れていた。製品未変更の実font/GDIで再現し、同じlayoutの端点を補正した。

## 正典経路と変更

製品編集前に[ADR0089](../adr/0089-native-lazy-mermaid.md)へ判断を追記した。
既存`diagram_geometry.c/.h`に`diagram_geometry_attachment`を追加し、正のlayout矩形と
物理的な辺中央を受け取り、形状の接続点を返す。flow layoutは方向変換後に同じpathの先頭/末尾だけを
このqueryへ渡す。中間点・辺label・矢頭/線種・非表示辺の扱いと、nativeの点列/比率は維持する。

斜辺は既存styleの除数7を使い、端点の実際のy位置から線形補間する。奇数高の半pixelのずれを
無視した一律の幅/14・幅/7加算にはしない。積を64bitで計算し、最後に最も近いlogical pxへ丸める。
上下方向と別形状の既存位置は保つ。新しいowner、router、描画後の補線、保存schema、版、依存、上限の変更はない。

## 入力・試行・結果

main`735575f`をcleanな作業木へ取り込んでから、最初にnative試験だけを追加した。
作業先は`D:/NeNeFolio/worktrees/554-flow-attachment`、出力は同名`outputs/`、恒久先は同名`agents/`。
固定Debug/Ninja、clang-tidy、ASan/UBSan/nullabilityを使用し、`tools/record-run.py`へargv、入力snapshot、
実行exe、raw出力、子/記録側終了値を保存する。bitmapとrun一式は終了時に全hash照合して保全する。

| 試行 | コマンド・内容 | 結果 |
| --- | --- | --- |
| build-red-01 | `cmake --build build --target diagram_render_tests folio_tests --parallel 2`、製品未変更 | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --attachments <red-bitmaps>` | 48枚を保存、288端点中108不接触、子/記録側1 |
| build-green-01 | 製品の端点query、core境界、nativeの丸め境界を追加 | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --attachments <green-bitmaps>` | 3045checks、288端点と追加420端点の全接触、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 関連の解析・配置・遅延media対象成功、子/記録側0 |
| verify-01 | `clang-format --dry-run --Werror`、`eng/conformance.py --build-dir build`、`eng/symbols.py --build-dir build --require core application`、`git diff --check` | 整形成功、conformance 0違反、symbols 2libraries/0違反、diff成功、子/記録側0 |

全runのrecordingErrorsは空。red-01のstderrは意図した不接触assert、それ以外の対象build/試験/verifyのstderrは空。
推測した`tests/native`の読取は失敗し、`rg --files tests`で実在する`tests/win32`へ訂正した。
成功した不変入力は再実行せず、最終headの必須CIへ進める。

nativeの48条件は6形状×LR/RL/TB/BT×明暗。2つの日本語14字ラベルを実Noto Sans JPで計測する。
順方向の4点path、戻り/自己辺の6点pathを明示的に検査し、circle marker、太線、辺labelを含む。
末尾の非表示辺を含む4リンク中、見える3リンクだけがsceneへ出ることを確認する。
bitmap名は`attachment-<shape>-<direction>-<theme>.bmp`、shapeはhexagon、parallelogram、reverse
parallelogram、trapezoid、reverse trapezoid、asymmetricの順、directionはLR/RL/TB/BT、themeは明/暗。

実`diagram_native_shape_draw`をBeginPath/EndPath内で呼び、PathToRegionで実際の輪郭を取得する。
各端点のx/yそれぞれ±1logical px（2device px）内に領域の内外両方があることを確認する。
これは丸め/線幅を許容する**2D近傍の接触**であり、同じy上の厳密な交点や全DPIを保証する判定ではない。
図形点列を試験へ複写せず、実bitmapも確認する。修正前の2枚と修正後の横向き5形状×2方向の10枚を視認した。

横向きの4つの四辺形とasymmetricで隙間が解消した。上下方向の24枚とhexagon横向き4枚は、
修正前後で全SHA256が一致。変更は残りの20枚だけで、照合原本は`bitmap-comparison.json`。
追加の420端点は6形状×7幅（64/65/319/320/321/5779/5780）×5高（40/41/45/188/4097）×左右を検査する。
coreでは全15shapeの奇数高41・幅700の左右期待位置と、上下の不変位置を確認した。

## 独立レビュー・限界

主担当が単体で編集/実行し、`mermaid_parser_review`が読み取り専用で独立レビューした。
初めに「奇数高のhexagonは同じy上では輪郭が内側へ寄るため追加matrixが落ちる」と指摘された。
実行では全条件成功しており、端点から頂点までは0.5pxで2D近傍に含まれることを再確認した。
レビュー側も失敗予測を訂正して指摘を撤回、09:42:03に追加指摘なし。hexagonを余分に変更しなかった。

規則: ARC-001/003/004/008/010/012、C-002/005/008/011/012/014/016、
QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
追加waiverはnone。既存WVR-0001はWinHTTP callback限定で継続する。
実app/実ノートの操作、Store再提出/公開は行わない。hideによる通常版の実機受理は未確認。
任意の斜めrouter、全DPIでの完全な交点一致、新しい図種は保証範囲へ広げない。
最終CI/通常反映/data照合/保全整理の確定原本は`D:/NeNeFolio/agents/554-flow-attachment/integration-completion.json`へ。
