/* 改名する名前と完成後の台帳を一つの不透明な意図として所有する（ADR0022）。 */
#ifndef NENEFOLIO_NOTE_RENAME_H
#define NENEFOLIO_NOTE_RENAME_H
#include "note_rename_outcome.h"
#include "note_rename_target.h"
struct note_rename;
struct note_ledger;
struct json_reader;
struct json_writer;
[[nodiscard]] enum note_rename_outcome
note_rename_create(const char *_Nonnull category, const struct note_ledger *_Nonnull ledger,
                   const struct note_rename_target *_Nonnull target,
                   struct note_rename *_Nullable *_Nonnull out);
/* journalの内側のobjectを同じcodecで読み書きする。外側が版を持つ。 */
[[nodiscard]] enum note_rename_outcome
note_rename_read(struct json_reader *_Nonnull reader, struct note_rename *_Nullable *_Nonnull out);
void note_rename_write(const struct note_rename *_Nonnull rename,
                       struct json_writer *_Nonnull writer);
[[nodiscard]] const char *_Nonnull note_rename_category(const struct note_rename *_Nonnull rename);
[[nodiscard]] const char *_Nonnull note_rename_from(const struct note_rename *_Nonnull rename);
[[nodiscard]] const char *_Nonnull note_rename_to(const struct note_rename *_Nonnull rename);
[[nodiscard]] const struct note_ledger *_Nonnull note_rename_ledger(
    const struct note_rename *_Nonnull rename);
[[nodiscard]] bool note_rename_equals(const struct note_rename *_Nonnull left,
                                      const struct note_rename *_Nonnull right);
/* 完了後だけ一度呼び、台帳の所有を移す。以後はdestroy以外を呼ばない。 */
[[nodiscard]] struct note_ledger *_Nonnull note_rename_take_ledger(
    struct note_rename *_Nonnull rename);
void note_rename_destroy(struct note_rename *_Nullable rename);
#endif
