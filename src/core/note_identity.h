#ifndef NENEFOLIO_NOTE_IDENTITY_H
#define NENEFOLIO_NOTE_IDENTITY_H
#include "note_identity_outcome.h"
struct note_identity;
/* Inputs are validated ledger names, including an already stripped note stem.
 * Copy exactly; never interpret another .md suffix. Failure preserves out. */
[[nodiscard]] enum note_identity_outcome
note_identity_create(const char *_Nonnull category, const char *_Nonnull note,
                     struct note_identity *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_identity_category(
    const struct note_identity *_Nonnull identity);
[[nodiscard]] const char *_Nonnull note_identity_note(
    const struct note_identity *_Nonnull identity);
void note_identity_destroy(struct note_identity *_Nullable identity);
#endif
