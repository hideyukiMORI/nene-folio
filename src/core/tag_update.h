#ifndef NENEFOLIO_TAG_UPDATE_H
#define NENEFOLIO_TAG_UPDATE_H
#include "tag_update_kind.h"
#include "tag_update_outcome.h"
struct tag_update;
struct tag_update_request;
struct note_text;
/* Immutable per-note preview, owning identity and, only for CHANGE, both body snapshots. */
[[nodiscard]] enum tag_update_outcome
tag_update_create(const struct tag_update_request *_Nonnull request,
                  struct tag_update *_Nullable *_Nonnull out);
[[nodiscard]] enum tag_update_kind tag_update_kind(const struct tag_update *_Nonnull update);
[[nodiscard]] const char *_Nonnull tag_update_category(const struct tag_update *_Nonnull update);
[[nodiscard]] const char *_Nonnull tag_update_note(const struct tag_update *_Nonnull update);
/* Optional snapshots exist exactly for CHANGE. Null is absence, never an allocation failure. */
[[nodiscard]] const struct note_text *_Nullable tag_update_source(
    const struct tag_update *_Nonnull update);
[[nodiscard]] const struct note_text *_Nullable tag_update_body(
    const struct tag_update *_Nonnull update);
void tag_update_destroy(struct tag_update *_Nullable update);
#endif
