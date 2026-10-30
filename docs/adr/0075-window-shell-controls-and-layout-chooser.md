# ADR 0075 — 窓操作を常設し、狭幅の表示選択を同じMENUへまとめる

- 状態: 受理（製品編集前に設計と旧placement観測を確定。完成候補の検証・統合は品質記録で別途確認する）
- 日付: 2026-10-30
- Issue: #452
- 規則: ARC-001/004/011/012、C-002/008/012/017/018、QLT-008/012/013
- 補正対象: ADR0047/0048/0060/0064。関連: ADR0056/0059。保存・本文・既存layout意味・modal ownerは維持する。

## 根拠

hideは枠なし主窓の最小化・最大化・最大化解除の入口を求めた。既存OS system commandとheader空白の操作はあるが、専用buttonが無い。
閉じるに最小化と最大化/復元を並べると最小320幅の表示3択と移動領域が共存しないため、現在の表示選択だけを狭幅でまとめる。
デザインリナの批評と独立sourceレビューを照合し、狭幅で最小化/最大化を隠す案、native popupの別経路、独自restore状態を採らない。

## 決定

### 1. 一つのheader geometry

高さ44、一覧最小320×360、両方/ノート最小560×360、右端余白16、C様式28×28/間8を維持する。
右側はヘルプ、設定、最小化、最大化または復元、閉じる。どの対応幅でも3つのshell入口を残す。
新規は既存の実font測定による字付き/絵のみ縮退を保ち、表示入口との間に移動24以上を残す。

表示3択の既存段階56/16（560以上）、44/16、44/8、40/8は、DPI済み残量で順に選ぶ。
最後も移動24を保てない場合だけ、同じ表示slotを幅52のA様式chooserへする。現在layoutのicon16、間4、chevron16、左右8。
320幅では新規16–44、chooser72–124、help132–160、settings168–196、min204–232、max240–268、close276–304となり、移動28を残す。
384幅では40×3の表示群68–188、560幅では56×3の表示群188–356。これらは説明用logical座標で、独立breakpointを実装へ複写しない。
同じgeometryを描画、hit、new-note残量、tooltip、keyboard可視判定、UIA bounds、MENU anchorが使う。縮退flagを別stateに保存しない。

shellの絵は既存icon_paintのcode-native図形、既存palette/state/focusを使う。別button描画系、bitmap、theme tokenは加えない。

### 2. OS状態とshell操作

最小化/最大化/復元は共通accessible actionからWM_SYSCOMMANDへ送り、既存shell_system_action/DefWindowProcへ合流する。
IsZoomed/IsIconic/WINDOWPLACEMENTがOS状態の正本。擬似最大化、第二のnormal rect、保存settingsを加えない。
pointerは既存press/release/cancel/captureを使い、captureを解放してからsystem commandへ進む。header空白/ダブルクリックと既存×の意味を保持する。

閉じる/最小化/最大化・復元の集合判定を共有し、描画/pointer/UIAの可否をshell_readyへ揃える。
native/child modal中もこの集合だけを有効にする。UIA hostの再入例外も有効なshell Invokeだけへ揃え、refresh中の拒否と外側executing値の復元を保つ。
新規/表示chooser/3択など内容操作を有効化せず、modal Tab順へheaderを混ぜない。既存modal_ownerのfocus捕捉/復帰と終了寿命を維持する。

最大化/復元のicon、tooltip、UIA Nameは同じ現在IsZoomedから選ぶ。名前はui_textの三言語（最小化/Minimize/最小化、
最大化/Maximize/最大化、元に戻す/Restore/还原）。WM_SIZE後の再描画/tooltip更新/UIA Name通知へ接続する。
新しいOS shortcut literalを追加せず、記述していないacceleratorを表示しない。

### 3. 表示chooserとMENU

意味/実行は既存folio_layout_modeとfolio_command。新しいfolio_menu_contextを既存CommandLayer MENUへ追加し、3行のcaption/shortcut/access keyは既存command metadataから導く。
chooserは現在modeを含む短い三言語Name、Button/Invoke/ExpandCollapse。tooltip/HelpTextも同じmetadataから作り、特定modeのshortcutをchooser自身のAcceleratorKeyへ偽って写さない。
pointerのopen、keyboard open、Space、Esc、capture取消は既存MENU入口の契約を使う。mouse-openの未選択、keyboard-openの先頭有効候補を、現在modeへ勝手に読み替えない。

menuのhover/keyboard候補は従来command_selection、現在modeの印はfolio_state_layout_modeから都度導く別の意味。
ui_partの共通menu描画へ現在印と列幅を渡し、layoutの3行だけ同じ列幅を予約する。他contextは列幅0で既存字頭を保つ。
menu幅測定/切詰め/ink.labelが同じ列幅を使う。新しい選択stateは保存しない。
UIA Nameは既存command caption、現在行のItemStatusは既存UI_TEXT_ACCESSIBLE_CURRENT、他行は空。SelectionItemは従来候補のまま。
実readerがItemStatusを必ず発話するとは保証しない。実行は既存の面を閉じてからexecute_commandへ合流する。
Invoke/Expand/Collapseとpointerのopen/dismissは既存MENU openerの同じ経路へ追加する。
accessibility_hostが実行中snapshotのために見るMENU状態も、既存操作MENUの入口だけに限定せず実surfaceのMENU状態から導く。

### 4. 可視・focus・寿命

Tabは新規→表示の一slot→ヘルプ→設定→最小化→最大化/復元→閉じる。3択は群に一回、chooserは一要素。
縮退でkeyboard_targetが隠れるときは、3択→chooser、chooser→現在modeの3択へ写す。rootが実focusを持つ場合だけfocus通知を出し、他HWND/nativeから奪わない。

static header nodeは同じkindで存続する。hiddenならoffscreen/disabled・空boundsとし、Invoke/Select/SetFocusを拒否する。
可視子集合は同じgeometryから導き、header表現が変わった時だけHEADER/必要なLAYOUT_GROUPの構造変更を通知する。
通知用の前snapshotは許すが、layoutの第二状態を持たない。既存surface_revisionをheaderへ流用せず、開閉したMENUの行だけを失効させる。

開いたMENUのanchorを古いgeometryのまま使わない。rootの実resize/DPI通知入口でMENUだけを既存hide経路へ閉じてから配置する。
arrangeへ無条件closeを入れず、初回openを閉じない。pressed/capture/旧子providerを同じ閉鎖で片付け、古いUPを実行しない。
単なるZ-order変化/hoverで閉じず、settings/themes等も閉じない。最小化や他窓/nativeへ無条件SetFocusをしない。

child面の復元観測で、OSがrootへfocusを戻した後の遅延focus_lostが設定面を閉じることを確認した。
同じWM_SETFOCUSの既存normalize_root_focus入口で、settings_surfaceかつnative深さ0・非最小化・
実focusがroot・root有効可視の場合だけ、既存command_focus_targetの有効可視な入力へfocus_command_inputで戻す。
新しい保存focusや強制activationは加えず、通常NOTE・他HWND・最小化中・native中は既存経路を維持する。
focus_lost通知を一律無視しない。設定とテーマの入力先は既存判定を使い、未確定テーマを承認/取消しない。

### 5. 一覧入出のplacement

ADR0047の保存wide_placement/restore_maximizedと、現在のOS IsZoomedを区別する。
normalから一覧へ入り、その一覧をOS最大化した後の退出は、入る前のnormal状態へ戻し、一覧のnormal位置/高さと保存wide幅を保持する。
保存trueの出口は元の最大化とnormal位置を戻す既存wide_placement経路を保持する。
旧観測は4ケース完備・technicalFailures0・契約不一致6件を確認した。normal保存側の2ケースでは退出後もIsZoomed1/showCmd3が残り、
幅だけが戻って一覧のnormal位置/高さを失った。max保存側は最大化と元のnormal位置を保持していた。
初回の同process desktop解放失敗はexit3で保持し、同じ4ケースとoracleを固定したcontroller/worker方式で旧観測を完備した。

修復は、保存restore_maximized=falseでDRAWERから退出し、現在IsZoomedの場合に限定する。
SW_SHOWNOACTIVATEでOSを通常状態へ戻し、後続のIsZoomed/IsIconicでnormal化を確認してから、既存set_layout_widthへ保存wide_widthを渡す。
通常状態にならなければfalseを返し、既存transactionでmodeとbefore placementへ戻す。保存wide_placement全体へ古いnormal位置を巻き戻さない。
保存trueの出口と、現在すでにnormalの出口は既存の正典経路を維持する。新しいrestore rectや第二のOS状態を保存しない。
ShowWindowの戻り値を成否に使わず、workspace位置をSetWindowPosへ直渡ししない。失敗時のmode/placement rollbackと同期WM_SIZEのtransactionを維持する。

## 完成候補の検証

対象は新menu context/text metadata、headerの最小幅と縮退境界、pointerのrelease/cancel、keyboard一slot巡回、UIAの可視集合/静的参照/構造通知/Name/操作拒否、
MENUの現在印と候補、open→resize/DPI→close/古いUP拒否、OSのnormal→max→restoreとminimize→normal/max復帰、child/native面中のshell操作に限定する。
変更境界の専用合成data/非input desktopで実OS契約を確認し、画像は代表的な3言語・最小幅と通常幅・実DPI条件を確認する。
bitmapが不要なcode-native iconなので画像生成は行わない。実mouse/IME/reader/monitor跨ぎは実施したものだけ報告する。
本文・保存・検索の無関係な全試験を追加せず、成功入力不変の結果は工程を跨いで再利用する。最終head必須CIが統合条件。
Snap hover/Win+Zの完全対応、保存schema、Store/installer、永続window配置は本件へ含めない。HTCLIENTだけから全Snap非対応とも断定しない。
Waivers none。

## 却下した選択肢

- 狭幅で最小化/最大化を隠す: 対応する最小窓で常設の窓操作が失われる。
- 新しいnative popupを追加する: 同じ表示commandに第二のメニュー描画・入力・UIA経路が生まれる。
- 全幅で表示chooserだけにする: 3択を置ける幅でも直接選択の入口を減らす。必要な幅だけ縮退する。
- 擬似最大化や独自restore rectを保存する: OSと既存一覧用placementの意味が重複する。
- normal保存の退出でwide_placement全体を戻す: 一覧中に動かした位置/高さを古い値へ巻き戻す。
- static headerへsurface_revisionを流用する: 補助面の行の寿命と、同じheader要素の可視変化が混ざる。
- 共通arrangeのたびにMENUを閉じる: 初回openや内容配置まで閉じてしまう。実resize/DPI入口で扱う。

## 根拠・一次資料

旧sourceはmain`3ef671f`、旧観測は4ケース・24snapshot段階・6契約不一致・技術不備0。worker/controller/記録wrapperは全て実exit2。
試験用desktop解放成功と原本44点の親照合を含む。旧動作の負側根拠であり、製品候補の成功ではない。
詳細・失敗attempt・条件・未確認事項は[品質記録](../quality/2026-10-30-window-controls.md)へ残す。

- [Microsoft ShowWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-showwindow)
- [Microsoft GetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowplacement)
- [Microsoft SetWindowPlacement](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowplacement)
- [Microsoft CloseDesktop](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-closedesktop)
- [Microsoft UI Automation threading](https://learn.microsoft.com/en-us/windows/win32/winauto/uiauto-threading)
