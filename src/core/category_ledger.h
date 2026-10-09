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
struct category_name;
struct json_reader;
struct json_writer;
struct name_list;

[[nodiscard]] enum category_ledger_outcome
category_ledger_empty(struct category_ledger *_Nullable *_Nonnull out);
[[nodiscard]] enum category_ledger_outcome
category_ledger_parse(const char *_Nonnull text, size_t length,
                      struct category_ledger *_Nullable *_Nonnull out);
/* reader の次の object だけを読む。外側の終端は呼出し側が検査する。失敗では out 不変。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_read(struct json_reader *_Nonnull reader,
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
/* index の展開状態だけを反転した新しい台帳を作る（FR-004）。index は count 未満であること。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_toggled(const struct category_ledger *_Nonnull ledger, size_t index,
                        struct category_ledger *_Nullable *_Nonnull out);
/* index の色だけを変えた新しい台帳を作る（FR-010）。index は count 未満であること。
 * 元の色と同じ色でも複製を返す。「変える意味があるか」は application が決める（ARC-011）。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_recolored(const struct category_ledger *_Nonnull ledger, size_t index,
                          struct rgb_color color, struct category_ledger *_Nullable *_Nonnull out);
/* index の位置に name のカテゴリを既定の色・展開で挿した新しい台帳を作る（ADR 0039 決定 10）。
 * index は count 以下で、count なら末尾に付く。name が既にあれば MALFORMED。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_inserted(const struct category_ledger *_Nonnull ledger, size_t index,
                         const struct category_name *_Nonnull name,
                         struct category_ledger *_Nullable *_Nonnull out);
/* index のカテゴリだけを除く。index は count 未満。残る色・展開・順序と元台帳を保持する。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_removed(const struct category_ledger *_Nonnull ledger, size_t index,
                        struct category_ledger *_Nullable *_Nonnull out);
/* index の名前だけを変える。範囲外/重複は MALFORMED、失敗時 out と元台帳は不変。
 * 同じ名前でも独立の写しを返す。no-op/大小文字の衝突は application が判断する。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_renamed(const struct category_ledger *_Nonnull ledger, size_t index,
                        const struct category_name *_Nonnull name,
                        struct category_ledger *_Nullable *_Nonnull out);
/* from 番目を to 番目へ移した、順序だけが違う新しい台帳を作る（FR-009）。
 * from と to は count 未満であること。from == to でも複製を返す。 */
[[nodiscard]] enum category_ledger_outcome
category_ledger_moved(const struct category_ledger *_Nonnull ledger, size_t from, size_t to,
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
