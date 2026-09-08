/* ui/win32 の子ウィンドウが親へ伝える通知（Win32 の WM_APP 領域）。値の集合は閉じている。 */
#ifndef NENEFOLIO_FOLIO_MESSAGE_H
#define NENEFOLIO_FOLIO_MESSAGE_H

#include <windows.h>

/* ドロワーで選択中のノートが変わった。親は右ペインを application の表示値で描き直す。 */
constexpr UINT folio_message_selection_changed = WM_APP + 1;

#endif
