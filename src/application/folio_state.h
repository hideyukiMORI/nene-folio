/* 文書の一覧とカテゴリ設定の唯一の所有者（ARC-004）。起動時にポートから台帳と走査結果を受け、
 * core の照合で 1 つの索引に固定する。外から見える状態は不変で、UI は派生値を受け取り、
 * 操作は意図（folio_state_toggle_category）として渡すだけ（ARC-011）。 */
#ifndef NENEFOLIO_FOLIO_STATE_H
#define NENEFOLIO_FOLIO_STATE_H

#include "drawer_metrics.h"
#include "folio_state_outcome.h"

#include <stddef.h>

struct drawer_layout;
struct folio_state;
struct persistence_port;

/* port は複製して持つ。port の adapter は state より長く生きていなければならない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_create(const struct persistence_port *_Nonnull port,
                   struct folio_state *_Nullable *_Nonnull out);
/* カテゴリの展開状態を反転し、台帳を書き戻す（FR-004 / FR-007）。書き戻せなければ状態は変えない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_category(struct folio_state *_Nonnull state, size_t index);
/* READY 以外の結果を利用者に見せる 1 行（UTF-8・終端付き・静的）。READY は空文字列。 */
[[nodiscard]] const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome);
/* いまの索引と寸法からドロワーの配置を作る。呼び出し側が drawer_layout_destroy する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_drawer_layout(const struct folio_state *_Nonnull state, struct drawer_metrics metrics,
                          struct drawer_layout *_Nullable *_Nonnull out);
void folio_state_destroy(struct folio_state *_Nullable state);

#endif
