#ifndef NENEFOLIO_IMAGE_EXPORT_H
#define NENEFOLIO_IMAGE_EXPORT_H
#include "archive_note.h"
#include "folio_state_outcome.h"
#include "image_export_policy.h"
#include <stddef.h>
struct image_port;
struct image_export;
struct archive_assets;
[[nodiscard]] enum folio_state_outcome
image_export_create(const struct image_port *_Nonnull port, enum image_export_policy policy,
                    size_t notes, struct image_export *_Nullable *_Nonnull out);
/* The original stays immutable. A changed entry borrows this export's owned copy. */
[[nodiscard]] enum folio_state_outcome image_export_append(struct image_export *_Nonnull export,
                                                           struct archive_note *_Nonnull entry);
[[nodiscard]] bool image_export_external(const struct image_export *_Nonnull export);
[[nodiscard]] const struct archive_assets *_Nonnull image_export_assets(
    const struct image_export *_Nonnull export);
void image_export_destroy(struct image_export *_Nullable export);
#endif
