/* 改名意図とOSが取得した識別子の版付き復旧記録。OSは呼ばない（ADR0022）。 */
#ifndef NENEFOLIO_RENAME_JOURNAL_H
#define NENEFOLIO_RENAME_JOURNAL_H
#include "rename_journal_outcome.h"
#include <stddef.h>
struct rename_journal;
struct note_rename;
struct json_writer;
/* volume64（16桁）+file128（32桁）の小文字hex。履歴だけ空を許し、元履歴なしを表す。 */
constexpr size_t rename_journal_id_length = 48;
[[nodiscard]] enum rename_journal_outcome
rename_journal_parse(const char *_Nonnull text, size_t length,
                     struct rename_journal *_Nullable *_Nonnull out);
/* 新しい空のwriterへ完全な版1の文書を出力。planと識別子は呼出し中だけ借りる。 */
[[nodiscard]] enum rename_journal_outcome
rename_journal_write(const struct note_rename *_Nonnull rename, const char *_Nonnull file_id,
                     const char *_Nonnull history_id, struct json_writer *_Nonnull writer);
[[nodiscard]] const struct note_rename *_Nonnull rename_journal_rename(
    const struct rename_journal *_Nonnull journal);
[[nodiscard]] const char *_Nonnull rename_journal_file_id(
    const struct rename_journal *_Nonnull journal);
[[nodiscard]] const char *_Nonnull rename_journal_history_id(
    const struct rename_journal *_Nonnull journal);
void rename_journal_destroy(struct rename_journal *_Nullable journal);
#endif
