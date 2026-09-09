/* ui/win32 の子ウィンドウが親へ伝える通知（Win32 の WM_APP 領域）。値の集合は閉じている。 */
#ifndef NENEFOLIO_FOLIO_MESSAGE_H
#define NENEFOLIO_FOLIO_MESSAGE_H

#include <windows.h>

/* ドロワーで選択中のノートが変わった。親は右ペインを application の表示値で描き直す。 */
constexpr UINT folio_message_selection_changed = WM_APP + 1;
/* 編集中なら先に保存して閲覧へ戻してほしい（ADR 0006 の決定 5）。同期で送り、
 * 結果の enum folio_state_outcome を LRESULT で受ける。閲覧中なら READY。 */
constexpr UINT folio_message_edit_flush = WM_APP + 2;

#endif
