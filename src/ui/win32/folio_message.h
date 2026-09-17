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

#endif
