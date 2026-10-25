#ifndef NENEFOLIO_MODAL_OWNER_H
#define NENEFOLIO_MODAL_OWNER_H
#include "modal_cancel_outcome.h"
#include "modal_owner_outcome.h"
#include <windows.h>
struct modal_owner;
[[nodiscard]] enum modal_owner_outcome
modal_owner_create(HWND _Nonnull root, struct modal_owner *_Nullable *_Nonnull out);
void modal_owner_destroy(struct modal_owner *_Nullable owner);
/* 製品rootの参照は、proxyや入れ子の実ownerと区別する。 */
[[nodiscard]] HWND _Nullable modal_owner_root(HWND _Nullable window);
[[nodiscard]] HWND _Nonnull modal_owner_current(struct modal_owner *_Nonnull owner);
[[nodiscard]] enum modal_cancel_outcome modal_owner_cancel(struct modal_owner *_Nonnull owner);
void modal_owner_capture_focus(struct modal_owner *_Nonnull owner);
void modal_owner_restore_focus(struct modal_owner *_Nonnull owner);
void modal_owner_arrange(struct modal_owner *_Nonnull owner);
[[nodiscard]] RECT modal_owner_work_area(HWND _Nonnull root);
/* 自前面は自身のreflow、OS面は取消/既存寸法を保った初期anchorへ合流する。 */
void modal_owner_positioned(HWND _Nonnull dialog);
void modal_owner_anchor(HWND _Nonnull root, HWND _Nonnull dialog);
#endif
