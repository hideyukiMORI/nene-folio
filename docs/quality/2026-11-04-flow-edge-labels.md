# 後続の辺に上書きされないflowラベル — #567

flowの辺をpath→labelの組ごとに描いていたため、後続のpathが先行labelの塗り/文字を上書きしていた。
4方向・明暗24図の48ラベル中16件で計4,950pxの差を再現した。全pathの後へlabelを出す修正後は全48件が単独描画と一致した。

## 正典経路と変更

製品編集前に[ADR0089](../adr/0089-native-lazy-mermaid.md)へ判断を追記した。
既存`diagram_flow_layout.c`の一時workspaceへ、上限1024辺の`diagram_point`配列を追加する。
`emit_link`で従来と同じpath・接続点を作り、そこで計算済みの物理label座標を一度だけ保存する。
全pathの後に`emit_link_label`で従来と同じ座標/metrics/文字/塗り/paddingを出し、その後にnodeを描く。
非表示辺と幅0のlabelは従来どおり出さない。path・矢頭・線種・node/group・文字の寸法/位置とprimitive数は維持する。
workspaceは従来のfactory内だけにあり、完成時に破棄する。確保回数・公開API・owner・renderer/routerを追加しない。
schema・版・依存・上限・ゲートの変更はない。

## 回帰試験と画像の基準

既存native試験へ`--flow-labels`を追加し、引数なしの必須CIへ結線した。
実sceneのcaption/background文字primitiveだけを、同じfont/metrics・絶対座標・全体寸法で単独描画する。
全図と参照のBGRAを比較し、境界1logical pxを除くlabel内部が一致することを要求する。
座標や経路の期待式を試験へ複写しない。全48画像を保存してから最後に集約assertするため、修正前の失敗でも全条件が残る。

| 添字 | 入力 | ラベル数（各方向/theme） |
| --- | --- | ---: |
| 0 | 同じ始点から2本の分岐、先行labelは2行 | 2 |
| 1 | 2行label、戻り破線、同じ行先への太線、非表示辺 | 3 |
| 2 | 単一辺の対照 | 1 |

方向添字0/1/2/3がTB/BT/LR/RL、theme0/1が明/暗。
`flow-label-<入力>-<方向>-<theme>.bmp`が全図、`-isolated.bmp`が参照である。
修正前は入力0/1の先頭labelだけ、全16件で上書きされた。入力0はTB/BT各79px・LR/RL各410px、
入力1はTB/BT各336px・LR410px・RL415px（各theme）。他の32ラベルは差0だった。

## 試行と結果

作業先は`D:/NeNeFolio/worktrees/567-flow-edge-labels`、出力は同名`outputs/`、恒久原本は同名`agents/`。
固定Debug/Ninja、clang-tidy、ASan/UBSan/nullabilityを使い、`tools/record-run.py`で入力snapshot・argv・raw・終了値・exe/bitmapを保存した。

| run | コマンド・条件 | 結果 |
| --- | --- | --- |
| build-red-01 | mainc063a60・製品未変更、`cmake --build build --target diagram_render_tests folio_tests --parallel 2` | 子/記録側0 |
| red-01 | `diagram_render_tests.exe --flow-labels <red-bitmaps>` | 16/48ラベル・計4,950px差、子/記録側1 |
| build-green-01 | main7ea0e1c同期後・flowの出力順修正、同じtarget | 子/記録側0 |
| native-green-01 | `diagram_render_tests.exe --flow-labels <green-bitmaps>` | 全48ラベル差0・776checks、子/記録側0 |
| core-green-01 | `folio_tests.exe --diagram` | 既存の図対象成功、子/記録側0 |
| verify-01 | 整形、conformance、symbols core/application、`git diff --check` | 整形成功・違反0・2libraries違反0・diff成功、子/記録側0 |

全runのrecordingErrorsは空。redの意図したassert失敗以外のstderrは空。実装をなぞる出力順assertは追加せず、
native画像と既存coreの図・非表示辺・上限・確保失敗等の契約で確認した。成功後のcode/test入力はpush/統合で再実行しない（QLT-008/012）。

redの終了後に#565統合mainを取り込んだ。未commit変更はnativeだけで、全差分・元ファイル・追加関数群とstash SHAを保存した。
cleanな作業木をffし、統合mainのmessage試験を保持したまま保存済みflow関数群を挿入した。
共通比較helperの名称だけ`heading_pixels`→`text_pixels`へ合わせ、比較式・flow fixtureは変えていない。
#565の製品変更はsequenceだけで、flow/canvas/metrics/font入力は不変。退避stashも保持している。

`compare-bitmaps.py`で全24図の寸法とlabel矩形を照合し、文字矩形外の全pixelは不変だった。
参照24枚と単一辺の対照8全図は修正前後の全SHA256一致。16全図だけlabel矩形内が変わった。
原本は`bitmap-comparison.json`。参照自体の不変により、文字や配置を変えて差を消していないことも確認した。
修正前3全図と修正後6全図を視認し、明暗・4方向・戻り/並行・非表示・単一辺の表示を確認した。

## 自己レビュー・限界・完了条件

単体で実装・自己レビュー・対象実行を行った。既存flow owner内の座標保持と出力順変更で、追加の独立レビューは使わない。
規則: ARC-001/003/004/008/012、C-002/008/011/012/014/016、QLT-008/009/010/012/013、GIT-001〜004、ADR0089。
追加waiverはnone。既存WVR-0001はWinHTTP callbackだけで継続する。
今回の3入力・4方向・明暗と、境界1logical pxを除く内部の保証であり、任意図/全DPI/書体/hideの実機受理へ拡張しない。
実app/実ノートの試験操作、Store再提出/公開は行っていない。
最終head必須CI・正典統合・通常反映/data照合・保全整理の確定値は
`D:/NeNeFolio/agents/567-flow-edge-labels/integration-completion.json`へ収載する。
