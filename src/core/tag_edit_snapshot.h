/* Immutable source and destination for one explicitly applied tag edit (ADR0092). */
#ifndef NENEFOLIO_TAG_EDIT_SNAPSHOT_H
#define NENEFOLIO_TAG_EDIT_SNAPSHOT_H
#include "tag_edit_identity.h"
#include "tag_edit_snapshot_outcome.h"
struct note_text;
struct tag_edit_snapshot;
/* Take body only on READY; on failure caller still owns body and out is unchanged.
 * Body is the canonical editor-normalized source. Identity strings are copied.
 * Application discards the session when the document is replaced, including a new untitled. */
[[nodiscard]] enum tag_edit_snapshot_outcome
tag_edit_snapshot_create(struct note_text *_Nonnull body,
                         const struct tag_edit_identity *_Nonnull identity,
                         struct tag_edit_snapshot *_Nullable *_Nonnull out);
[[nodiscard]] bool tag_edit_snapshot_holds(const struct tag_edit_snapshot *_Nonnull snapshot,
                                           const struct note_text *_Nonnull current,
                                           const struct tag_edit_identity *_Nonnull identity);
[[nodiscard]] const struct note_text *_Nonnull tag_edit_snapshot_text(
    const struct tag_edit_snapshot *_Nonnull snapshot);
void tag_edit_snapshot_destroy(struct tag_edit_snapshot *_Nullable snapshot);
#endif
