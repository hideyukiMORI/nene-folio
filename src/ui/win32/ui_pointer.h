/* OS標準pointerの投影。対象のhit/実行可否は各HWND所有者から受ける（ADR0083）。 */
#ifndef NENEFOLIO_UI_POINTER_H
#define NENEFOLIO_UI_POINTER_H

#include "ui_pointer_kind.h"
#include <windows.h>

void ui_pointer_show(enum ui_pointer_kind kind);
[[nodiscard]] bool ui_pointer_client(HWND _Nonnull window, WPARAM target, LPARAM hit);
/* 最後に取得したmessageの位置。共有pointerを読み直さない。 */
[[nodiscard]] POINT ui_pointer_message_point(HWND _Nonnull window);
/* 表示/入力状態が変わる通知の後、同じrootに属する実hit HWNDだけを再評価する。 */
void ui_pointer_after(HWND _Nonnull window, UINT message);

#endif
