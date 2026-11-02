#ifndef NENEFOLIO_MARKDOWN_IMAGE_COPY_H
#define NENEFOLIO_MARKDOWN_IMAGE_COPY_H
#include "image_relocation.h"
#include "note_text_outcome.h"
struct note_text;
/* Source byte spans from the canonical render collector; only exported copies are changed. */
[[nodiscard]] enum note_text_outcome
markdown_image_copy_create(const struct note_text *_Nonnull source,
                           const struct image_relocation *_Nonnull relocations, size_t count,
                           struct note_text *_Nullable *_Nonnull out);
#endif
