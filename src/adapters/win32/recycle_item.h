/* 検査済みの通常 DOS 絶対パスを専用 RecycleItem で送る、adapter 内部の OS 境界。 */
#ifndef NENEFOLIO_RECYCLE_ITEM_H
#define NENEFOLIO_RECYCLE_ITEM_H

#include "trash_outcome.h"
#include <stddef.h>

[[nodiscard]] enum trash_outcome recycle_item_send(const wchar_t *_Nonnull path);

#endif
