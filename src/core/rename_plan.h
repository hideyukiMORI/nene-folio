/* 種別・改名する名前・完成後の台帳を一つの不透明な意図として所有する（ADR0041）。 */
#ifndef NENEFOLIO_RENAME_PLAN_H
#define NENEFOLIO_RENAME_PLAN_H
#include "note_rename_target.h"
#include "rename_kind.h"
#include "rename_plan_outcome.h"
struct rename_plan;
struct note_ledger;
struct category_ledger;
struct category_name;
struct json_reader;
struct json_writer;
/* 作成・読取の失敗では out と入力の所有物を変更しない。 */
[[nodiscard]] enum rename_plan_outcome
rename_plan_create_note(const char *_Nonnull category, const struct note_ledger *_Nonnull ledger,
                        const struct note_rename_target *_Nonnull target,
                        struct rename_plan *_Nullable *_Nonnull out);
[[nodiscard]] enum rename_plan_outcome
rename_plan_create_category(const struct category_ledger *_Nonnull ledger, size_t index,
                            const struct category_name *_Nonnull name,
                            struct rename_plan *_Nullable *_Nonnull out);
/* journal が版と種別を検証し、内側の object を同じ codec へ渡す。 */
[[nodiscard]] enum rename_plan_outcome
rename_plan_read(struct json_reader *_Nonnull reader, enum rename_kind kind,
                 struct rename_plan *_Nullable *_Nonnull out);
void rename_plan_write(const struct rename_plan *_Nonnull plan,
                       struct json_writer *_Nonnull writer);
[[nodiscard]] enum rename_kind rename_plan_kind(const struct rename_plan *_Nonnull plan);
[[nodiscard]] const char *_Nonnull rename_plan_from(const struct rename_plan *_Nonnull plan);
[[nodiscard]] const char *_Nonnull rename_plan_to(const struct rename_plan *_Nonnull plan);
/* 種別を網羅してから呼ぶ。category/note_ledger は NOTE、category_ledger は CATEGORY 限定。
 * 借用は plan が生きていて take を呼ぶ前だけ有効。異種の getter/take を呼んではならない。 */
[[nodiscard]] const char *_Nonnull rename_plan_category(const struct rename_plan *_Nonnull plan);
[[nodiscard]] const struct note_ledger *_Nonnull rename_plan_note_ledger(
    const struct rename_plan *_Nonnull plan);
[[nodiscard]] const struct category_ledger *_Nonnull rename_plan_category_ledger(
    const struct rename_plan *_Nonnull plan);
[[nodiscard]] bool rename_plan_equals(const struct rename_plan *_Nonnull left,
                                      const struct rename_plan *_Nonnull right);
/* 完了後、種類の合う take を一度だけ呼んで台帳の所有を移す。以後は destroy だけを呼ぶ。 */
[[nodiscard]] struct note_ledger *_Nonnull rename_plan_take_note_ledger(
    struct rename_plan *_Nonnull plan);
[[nodiscard]] struct category_ledger *_Nonnull rename_plan_take_category_ledger(
    struct rename_plan *_Nonnull plan);
void rename_plan_destroy(struct rename_plan *_Nullable plan);
#endif
