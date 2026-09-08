/* ドロワーの索引の配置（FR-003 / FR-004）。台帳と寸法から行の y 座標を決める純関数で、
 * UI は行を描くだけ（ARC-011 / ADR 0002）。展開していないカテゴリのノートは行にならない。 */
#ifndef NENEFOLIO_DRAWER_LAYOUT_H
#define NENEFOLIO_DRAWER_LAYOUT_H

#include "drawer_layout_outcome.h"
#include "drawer_metrics.h"
#include "drawer_row.h"

#include <stddef.h>

struct category_ledger;
struct drawer_layout;
struct note_ledger;

/* notes はカテゴリと同じ数・同じ順の索引台帳。名前は layout へ複製され、台帳より長く生きてよい。 */
[[nodiscard]] enum drawer_layout_outcome
drawer_layout_create(const struct category_ledger *_Nonnull categories,
                     const struct note_ledger *_Nonnull const *_Nonnull notes,
                     struct drawer_metrics metrics, struct drawer_layout *_Nullable *_Nonnull out);
[[nodiscard]] size_t drawer_layout_row_count(const struct drawer_layout *_Nonnull layout);
/* index は row_count 未満であること。 */
[[nodiscard]] struct drawer_row drawer_layout_row(const struct drawer_layout *_Nonnull layout,
                                                  size_t index);
/* y 座標にある行の番号。行の外なら false で index は触らない（FR-004 のヒットテスト）。 */
[[nodiscard]] bool drawer_layout_hit(const struct drawer_layout *_Nonnull layout, int y,
                                     size_t *_Nonnull index);
void drawer_layout_destroy(struct drawer_layout *_Nullable layout);

#endif
