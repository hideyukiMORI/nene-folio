/* 文書の一覧とカテゴリ設定の唯一の所有者（ARC-004）。起動時にポートから台帳と走査結果を受け、
 * core の照合で 1 つの索引に固定する。外から見える状態は不変で、UI は派生値を受け取るだけ。 */
#ifndef NENEFOLIO_FOLIO_STATE_H
#define NENEFOLIO_FOLIO_STATE_H

#include "drawer_metrics.h"
#include "folio_state_outcome.h"

struct drawer_layout;
struct folio_state;
struct persistence_port;

/* port はこの呼び出しの間だけ使う。 */
[[nodiscard]] enum folio_state_outcome
folio_state_create(const struct persistence_port *_Nonnull port,
                   struct folio_state *_Nullable *_Nonnull out);
/* いまの索引と寸法からドロワーの配置を作る。呼び出し側が drawer_layout_destroy する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_drawer_layout(const struct folio_state *_Nonnull state, struct drawer_metrics metrics,
                          struct drawer_layout *_Nullable *_Nonnull out);
void folio_state_destroy(struct folio_state *_Nullable state);

#endif
