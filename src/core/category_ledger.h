/* data/categories.json の台帳（FR-007）。カテゴリの表示順・色・展開状態を持つ。
 * 版 1 の形（キーの順序も固定。違えば MALFORMED であって既定値には落ちない・FR-015）:
 *   {"version": 1, "categories": [{"name": "…", "color": "#RRGGBB", "expanded": true}, …]}
 * 名前の規則と重複禁止は name_list に委ねる。 */
#ifndef NENEFOLIO_CATEGORY_LEDGER_H
#define NENEFOLIO_CATEGORY_LEDGER_H

#include "category_ledger_outcome.h"
#include "rgb_color.h"

#include <stddef.h>

struct category_ledger;
struct json_writer;
struct name_list;

[[nodiscard]] enum category_ledger_outcome
category_ledger_empty(struct category_ledger *_Nullable *_Nonnull out);
[[nodiscard]] enum category_ledger_outcome
category_ledger_parse(const char *_Nonnull text, size_t length,
                      struct category_ledger *_Nullable *_Nonnull out);
/* writer へ版 1 の文書を 1 つ書く。完了の判定は json_writer_finish で行う。 */
void category_ledger_write(const struct category_ledger *_Nonnull ledger,
                           struct json_writer *_Nonnull writer);
/* 走査結果と照合した新しい台帳を作る（FR-008 の準用）: ledger の順序で scanned にある名前を残し、
 * scanned にだけある名前を既定の色・展開で末尾に足す。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_reconcile(const struct category_ledger *_Nonnull ledger,
                          const struct name_list *_Nonnull scanned,
                          struct category_ledger *_Nullable *_Nonnull out);
[[nodiscard]] size_t category_ledger_count(const struct category_ledger *_Nonnull ledger);
/* 終端付き。ledger が生きている間だけ有効。index は count 未満であること。 */
[[nodiscard]] const char *_Nonnull category_ledger_name(
    const struct category_ledger *_Nonnull ledger, size_t index);
[[nodiscard]] struct rgb_color category_ledger_color(const struct category_ledger *_Nonnull ledger,
                                                     size_t index);
[[nodiscard]] bool category_ledger_expanded(const struct category_ledger *_Nonnull ledger,
                                            size_t index);
void category_ledger_destroy(struct category_ledger *_Nullable ledger);

#endif
