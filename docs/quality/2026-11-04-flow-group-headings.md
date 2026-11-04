# 接続辺から保護するsubgraph見出し — #571

flowのsubgraph見出しをgroup背景と同時に描いていたため、後続の辺が文字を上書きしていた。
4入力×4方向×明暗の48見出し中14件・計2,186pxで再現した。最終修正後は全48見出しと16辺ラベルの文字矩形が単独参照と一致した。

## 正典経路と変更

製品編集前に[ADR0089](../adr/0089-native-lazy-mermaid.md)へ方針を追記した。
既存`diagram_flow_layout.c`のgroup背景と見出しのemitを分け、group背景→全path→辺label→group見出し→nodeの順に出す。
見出しへ既存の`DIAGRAM_INK_GROUP`を指定し、文字primitiveの幅をgroupの残り幅から既存metricsの実文字幅へ絞った。
nativeの`draw_text`は従来から実測幅でDrawTextしているため、glyphの位置・折返し・幅・高さは変わらない。
塗りの対象を実文字矩形だけに限定し、広い空白帯を塗って線を消さない。

最初に全path後・辺label前へ出した時点では、12件の線による差が消えたが、入れ子LRの2件・46pxが残った。
辺labelの背景余白が見出しを覆っていたため、見出しを辺labelの後へ移した。辺labelの実文字も同時に保護されることを追加確認した。
この追加確認はpaddingの背景色の一致ではなく、実測文字矩形の一致である。

`construct`へpassを足した最初のbuildは認知的複雑度11/上限10で停止した。配置計算からsceneへの出力段階を`emit_scene`へ分け、
描画順を一か所へ集めた。閾値・除外・抑制を変えない。group/node/path/辺labelの配置、接続点、矢頭、primitive数、確保回数は維持する。
owner・公開API・renderer/router・schema・版・依存・ゲートは増やさない。

## 画像による回帰

既存native試験の`--group-headings`を追加し、引数なしの必須CIにも結線した。
実sceneのgroup背景をすべて描き、その見出しだけを同じcanvas/font/絶対座標/全体寸法で描く参照を作る。
実測文字幅に限った見出し内部のBGRAを比較し、境界1logical pxを除く領域が一致することを要求する。
座標/経路の期待式や出力順のassertは追加しない。各全図・参照を保存してから最後に集約assertする。

| 添字 | 入力 | 見出し数（各方向/theme） |
| --- | --- | ---: |
| 0 | group外の始点から内部の終点へ接続 | 1 |
| 1 | 内から外への太線、破線の戻り、多行見出し | 1 |
| 2 | 入れ子group、多行見出し、接続/戻りの辺label | 2 |
| 3 | 辺なし・入れ子・多行の対照 | 2 |

方向0/1/2/3がTB/BT/LR/RL、theme0/1が明/暗。全32図・48見出し、全図と参照の計64bitmapである。
`group-heading-<入力>-<方向>-<theme>.bmp`と`-isolated.bmp`を各runに保持した。
入力2の辺labelについては、実sceneのcaptionを実測文字寸法へ狭めた独立sceneを同じcanvasで描き、
全16文字矩形を比較する。文字中心・内容・fontは実scene/metricsから取得し、文字のない入力もそのまま通る。
この内部参照の中間bitmapは別ファイルへ出さず、数・比較結果をrawに残す。表示確認用の全図は保存する。

## 試行と結果

作業先は`D:/NeNeFolio/worktrees/571-flow-group-headings`、出力は同名`outputs/`、恒久原本は同名`agents/`。
main`e564d0b`へ文書だけの#569を同期した後、固定Debug/Ninja・clang-tidy・ASan/UBSan/nullabilityで対象buildした。
`tools/record-run.py`で入力snapshot・argv・raw・終了値・exe/bitmapを保存した。

| run | 操作 | 結果 |
| --- | --- | --- |
| build-red-01 | 製品未変更、`cmake --build build --target diagram_render_tests folio_tests --parallel 2` | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --group-headings <red-bitmaps>` | 14/48見出し・2186px差、子/記録側1 |
| build-green-01 | 見出しを全path後へ移動 | constructの複雑度11/上限10、子/記録側1 |
| build-green-02 | 配置とscene emit段階を分離 | 子/記録側0 |
| native-green-01 | `<green-bitmaps>`へ初回修正の画像保存 | 2/48見出し・46px差、子/記録側1 |
| build-green-03 | 見出しを辺label後へ移動、辺の文字比較を追加 | 子/記録側0 |
| native-green-02 | `<green2-bitmaps>`へ最終画像保存 | 48見出し・16辺文字すべて差0、1208checks、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 既存の図対象成功、子/記録側0 |
| verify-01 | 整形、conformance、symbols core/application、`git diff --check` | 整形成功・違反0・2libraries違反0・diff成功、子/記録側0 |

全runのrecordingErrorsは空。意図したredと初回修正のpixel assert失敗以外のstderrは空。
複雑度診断はbuildのstdoutに保持した。初回green画像を上書きせず、最終画像は新しいdirectoryへ保存した。
`run-summary.json`へ全終了値/記録エラー/件数を収載。成功したcode/test入力はpush/統合で再実行しない。

`compare-bitmaps.py`で修正前と最終画像を比較し、全32図の文字矩形外pixelは不変だった。
見出し参照32枚と辺なし対照8全図の全SHA256が一致。全図は12枚だけ見出し矩形内で変化し、他20枚は不変。
原本は`bitmap-comparison.json`。赤3枚・初回修正の残差を含む画像・最終6全図を視認した。
見出しを移動/再計測して差を消したものではない。

## 自己レビュー・限界・完了条件

単体で調査・実装・自己レビュー・検証を行い、追加の独立レビューは使わない。
規則: ARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
追加waiverはnone。既存WVR-0001はWinHTTP callbackだけで継続する。
今回の4入力・4方向・明暗と境界1logical pxを除く内部の確認で、任意図の見出しと辺labelの配置衝突全般、全DPI/書体/実機受理は保証しない。
最終画像では辺文字も保たれたが、後描きの優先順位だけで任意の文字同士の衝突を解決したとは扱わない。
実app/実ノートの試験操作、Store再提出/公開は行わない。
最終head必須CI・正典統合・通常反映/data照合・保全整理の確定値は
`D:/NeNeFolio/agents/571-flow-group-headings/integration-completion.json`へ収載する。
