/* ui/win32 の子ウィンドウが親へ伝える通知（Win32 の WM_APP 領域）。値の集合は閉じている。 */
#ifndef NENEFOLIO_FOLIO_MESSAGE_H
#define NENEFOLIO_FOLIO_MESSAGE_H

#include <windows.h>

/* ドロワーのノート行が押された。主窓が「編集中なら保存 → 選択 → 同じモードで開く」の順を
 * 1 か所で行う（ADR 0013 の決定 4 / 7）。wParam = カテゴリ・lParam = ノート。同期で送り、
 * 結果の enum folio_state_outcome を LRESULT で受ける。 */
constexpr UINT folio_message_select_note = WM_APP + 1;
/* 操作入力から別の区画へ移ったあとに、主窓が実フォーカスを確認して入力面を閉じる。 */
constexpr UINT folio_message_command_focus_lost = WM_APP + 2;
/* 本文がホイールで動いた。EN_VSCROLL が来ない経路なので、番号の帯を主窓が描き直す
 * （ADR 0026 の決定 4）。 */
constexpr UINT folio_message_pane_scrolled = WM_APP + 3;
/* 番号の桁数が変わったので、帯の幅と本文の矩形を配り直す（決定 6）。描画の途中では動かさない。 */
constexpr UINT folio_message_gutter_resized = WM_APP + 4;
/* ドロワーの右クリックのメニューで選ばれた項目（FR-038・ADR 0039 の決定 11）。
 * 主窓が閉じた switch で既存のコマンドの受け口へ渡す。wParam = enum drawer_menu_item・
 * lParam = 送る側のスタックにある const struct note_ref *（カテゴリ行と行の無い所では
 * ノート番号を使わない）。同期で送り、失敗は主窓が出すので戻り値は使わない。 */
constexpr UINT folio_message_drawer_menu = WM_APP + 5;

constexpr UINT folio_message_accessibility_changed = WM_APP + 6;
constexpr UINT folio_message_accessibility_invoke = WM_APP + 7;
/* drawerの描画で索引のkeyboard focusを調べる。主窓がboolを返す。 */
constexpr UINT folio_message_index_focused = WM_APP + 8;
/* drawerのpointer操作が索引へfocusを置く。 */
constexpr UINT folio_message_index_focus = WM_APP + 9;
/* OS外観を安全な時点で読み直す。モーダルも同じ所有者へ要求する（ADR0057）。 */
constexpr UINT folio_message_appearance_changed = WM_APP + 10;
/* drawerの描画でキーボードの印（focus）を描いてよいかを調べる。主窓がboolを返す（ADR0060）。 */
constexpr UINT folio_message_focus_cues = WM_APP + 11;
/* drawerの描画で「すべてのノートを検索」の欄にfocusがあるかを調べる。主窓がboolを返す。 */
constexpr UINT folio_message_filter_focused = WM_APP + 12;

/* lParamは同期呼出し元が所有するmenu_request。 */
constexpr UINT folio_message_open_menu = WM_APP + 13;
/* drawer操作の結果を主窓のreport_outcomeへ渡す。wParam=folio_state_outcome。 */
constexpr UINT folio_message_report_outcome = WM_APP + 14;
/* modal取消の応答: 0=標準OS面、1=受理、2=IME等により拒否。 */
constexpr UINT folio_message_modal_cancel = WM_APP + 15;
constexpr UINT folio_message_modal_reflow = WM_APP + 16;
/* 最外dispatch/native呼出しが戻った地点で通常終了を一度だけ再開する。 */
constexpr UINT folio_message_close_requested = WM_APP + 17;
/* drawer色選択のowner/取消/暗幕を主窓の同期境界へ集約する。 */
constexpr UINT folio_message_choose_color = WM_APP + 18;
/* 本文倍率の適用後、行番号font/幅と配置を更新する。wParamは変更前の先頭論理行。 */
constexpr UINT folio_message_pane_zoomed = WM_APP + 19;
/* 子が覆う点を主窓のresizeへ通せるか同期で問い合わせる。wParamは未使用、lParamはsignedの
 * screen座標。capture中/最大化中/窓縁以外は0、通せるときだけ1。子は寸法を複製しない。 */
constexpr UINT folio_message_resize_through = WM_APP + 20;
#endif
