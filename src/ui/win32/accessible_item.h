#ifndef NENEFOLIO_ACCESSIBLE_ITEM_H
#define NENEFOLIO_ACCESSIBLE_ITEM_H

#include "accessible_kind.h"
#include "note_ref.h"

#include <stdint.h>

/* 行参照は改訂と対で使い、同じ添字の別ノートへ転用しない（ADR0056）。 */
struct accessible_item
{
    enum accessible_kind kind;
    struct note_ref ref;
    uint64_t revision;
    size_t slot; /* 補助面の表示行/操作。note_refへ異なる意味を詰め込まない。 */
};

[[nodiscard]] bool accessible_item_row(struct accessible_item item);
[[nodiscard]] bool accessible_item_root(struct accessible_item item);
[[nodiscard]] bool accessible_item_surface(struct accessible_item item);
[[nodiscard]] enum accessible_kind accessible_item_host(struct accessible_item item);
[[nodiscard]] bool accessible_item_equal(struct accessible_item left, struct accessible_item right);

#endif
