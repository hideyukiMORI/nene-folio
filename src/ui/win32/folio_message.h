/* ui/win32 の子ウィンドウが親へ伝える通知（Win32 の WM_APP 領域）。値の集合は閉じている。 */
#ifndef NENEFOLIO_FOLIO_MESSAGE_H
#define NENEFOLIO_FOLIO_MESSAGE_H

#include <windows.h>

/* ドロワーのノート行が押された。主窓が「編集中なら保存 → 選択 → 同じモードで開く」の順を
 * 1 か所で行う（ADR 0013 の決定 4 / 7）。wParam = カテゴリ・lParam = ノート。同期で送り、
 * 結果の enum folio_state_outcome を LRESULT で受ける。 */
constexpr UINT folio_message_select_note = WM_APP + 1;

#endif
