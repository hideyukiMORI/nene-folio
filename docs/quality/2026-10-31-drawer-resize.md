# 一覧の残像を直し、両表示の境界幅を調整する — #475

- Issue: [#475](https://github.com/hideyukiMORI/nene-folio/issues/475)、base main `8bde75c`。
- 規則: FR-013/039、ARC-001/002/004/005/007/008/011/012、C-002/008/012/013/016/017/018、QLT-008/012/013、GIT-001〜004。
- 方針: [ADR0076](../adr/0076-adjustable-drawer-width.md)、ADR0047/0056/0060の補正。
- 原本: `D:/NeNeFolio/agents/drawer-resize-20261031/`、作業木/出力はD側の`475-drawer-resize`。

## 根拠と変更

hideは通常版の窓resizeを実機確認し、一覧だけの連続resizeで検索枠と＋/−の旧描画が残ることを報告した。
幅変更を保って修復し、同じ配置経路で両表示の境界dragを追加する方針を承認した。
`drawer_window`のWM_SIZEで全clientを無効化し、既存のDIB→WM_PAINTへ戻す。

選択した一覧幅は`folio_window`が実行中だけlogical pxで持つ。初期240・一覧最小200・本文領域最小320。
窓縮小では実幅だけを制限し、窓を広げると選択幅へ戻す。一覧/ノート単独表示でも選択幅を保持する。
本文側の既存余白8pxを境界の当たり領域とし、描画・pointer・tooltip・UIAは同じ`regions_for`の矩形を使う。
ドラッグ・キー・RangeValueは同じ幅適用と`rearrange_keeping_line`へ渡す。保存や本文の再投入はしない。
capture中のEscape/喪失/別操作/非active化/親resize/DPI変更は開始幅へ戻し、窓外releaseは範囲内の幅を確定する。

F6/Tabで境界へ届き、左右4・PageUp/PageDown32・Home/Endのキー操作を提供する。
UIAは既存のhost/node寿命にSlider/RangeValueを追加し、値・範囲・可否を同じ表示値から公開する。
仮想要素の兄弟順は上部ヘッダー→境界→本文操作帯とする。文言/Helpは既存の3言語`ui_text`へ収載した。
schema・版・依存・gateの変更なし。Waivers none。

## 対象検証

`tools/record-run.py`で入力snapshot、実argv、raw出力、成果物、子とwrapperの終了値を記録した。
正典Debugのcompile_commands/Ninja/File API、tool/runtime hashと環境を照合し、fixtureを派生した。
ASan/UBSan/nullability、recover禁止、警告集合と4runtimeを保持し、entryのsubsystemだけconsoleへ変える。
保存済み`folio_window.c`全bytesの後ろへfixtureを追加し、元window archive memberが選ばれないことをlink mapで確認した。
他の製品実装は保存した正典libraryを使う。専用の非入力desktopと合成ノートだけを用いた。

| 実行（driverは原本directory） | 結果 |
| --- | --- |
| `record-debug-v1.ps1 -Stage Configure -Attempt configure-debug-01` | 正典Debug configure成功 |
| 同driver `-Stage Build -Attempt build-baseline-01` / `build-candidate-04` | 修正前/最終候補のDebug・tidy成功 |
| `product-driver.py prepare repaint-baseline-v1 --build build-baseline-01` / `repaint-candidate-v1 --build build-candidate-02` | 同じfixtureのcompile/link成功 |
| `product-run.py repaint-baseline-v1` / `repaint-candidate-v1` | 10checks/5fail → 10checks/0fail |
| `product-driver.py prepare width-final-v7 --build build-candidate-04 --fixture width-fixture.c` | 最終候補のcompile/link成功 |
| `product-run.py width-final-v7 seed-width-v1` | 86checks/0fail |
| 同driver `table-final-v7 seed-width-v1 --compiled width-final-v7 table` | 21checks/0fail |
| 同driver `edges-final-v7 seed-width-v1 --compiled width-final-v7 edges` | 23checks/0fail |
| 同driver `latency-final-v7 seed-width-v1 --compiled width-final-v7 latency` | 8checks/0fail、連続40回の幅変更を記録 |
| 同driver `cursors-final-v7 seed-width-v1 --compiled width-final-v7 cursors` | 8checks/0fail、進入/退出/capture/非表示、子窓とnonclientの委譲 |

成功stageはchild/wrapperとも0・記録error0。修正前の残像試験はnative/child/wrapper1・記録error0。
5通りの幅/高さで、自然更新と強制全面更新の差は修正前838/552/361/361/474画素、修正後はすべて0。
修正前後のrepaint fixture hashは同一。最終変更はUIAの兄弟順とcursor表示なので、一覧WM_SIZE/描画入力不変の画像比較を再利用した。
画像も目視し、旧検索枠と＋/−の残像消失、本文と境界の位置を確認した。

幅操作は16万文字の未保存本文で確認した。本文bytes、選択、dirty、実Undo→Redo、focus、論理行300、
索引scroll/cursor/選択を保持する。窓外release、両端、Escape、capture喪失、無効化、mode切替、
親resize、最大化/復元、非active化、DPI通知、本文wheelへの取消を確認した。
キーボードの移動幅・端・Enter/Space非実行、UIAの実COM RangeValue、丸め・非有限/範囲外の拒否、
settings/IME/非表示の抑止と、窓破棄後の保持参照の失効も確認した。
閲覧表ではnative3列の幅が本文幅へ追従して元へ戻り、選択と論理行8を保持した。

連続40回の幅変更は、SendMessageと通常message処理/paintを含め平均13.028ms・最大17.593msの一回観測。
性能の一般保証や物理drag/DWM呈示の測定ではない。実環境はWindows `10.0.26300.0`、
Msftedit.dll `10.0.26100.9949`、120dpi（125%）。異なるDPI間の物理移動・実IME・実readerは未確認。
通常data、共有cursor、実機の未保存本文は操作していない。

失敗試行も保存する。candidate build01は4関数の複雑度を検出し、責務を整理してbuild02で成功した。
width compile-v1はSDKの`small` macroとfloat昇格、v2はSDK定数の二重定義。
observe-v3はfixtureがRangeValueをIUnknown型の関数ポインタで呼びUBSan停止し、正しいinterface型へ直した。
v4は合成IME開始が選択77文字を置換するnative動作を幅変更の保持と混同したため、IME可否確認を独立させた。
edge-v1はsigned16座標を超える合成pointer値と、別HWNDのdrawer fragment rootからの兄弟移動という誤った前提を修正した。
final-check01はARC-007がUIからの共有pointer座標取得を拒否した。ゲートを変えず、既存のmouse通知/押下/hoverからcursorを投影する形へ直した。
build04とfinal-v7ではその修正を含めて146checksを確認し、非変更の一覧描画比較は再利用した。
各失敗を製品成功とは扱わず、新規run-dirで修正・確認した。

## 自己レビューと統合

ARC-001/004/005/008/011: 一覧幅の唯一のowner、既存RichEdit/索引状態、共通の配置/入力/UIAの経路を維持する。
ARC-002/007、C-008/013: 製品のtest窓口・adapter依存・時計・thread・入れ子pumpを追加しない。
C-002/012/016/017/018: 新しい閉集合を列挙し、範囲/取消/COM寿命を確認し、表示文言は正典表へ置いた。
QLT-008/012/013: 同一fixtureの負側根拠、対象境界、合成入力の限界を区別した。全回帰のローカル再実行は行わない。

最終format/diff/conformanceと証跡hash監査を記録し、最終headの必須CI成功後に統合する。
main・通常exeの版/hash/反映/起動・恒久保全と整理の確定値は原本の`integration-completion.json`に残す。
通常版が起動中なら保存/終了を待ち、強制終了しない。12:26時点では通常版は停止済み。
停止済み継続goal、#466、公開beta、旧削除拒否55箇所は対象外。
追加worktreeと出力は取込・未保存/未追跡/ignored・唯一の成果物・link・稼働参照を監査して整理し、branch/commitは保持する。
