#ifndef NENEFOLIO_IMAGE_SOURCE_H
#define NENEFOLIO_IMAGE_SOURCE_H
#include "image_outcome.h"
#include "image_source_kind.h"
#include "note_search_span.h"
#include <stddef.h>
struct image_source;
[[nodiscard]] enum image_outcome image_source_copy(const struct image_source *_Nonnull source,
                                                   struct image_source *_Nullable *_Nonnull out);
constexpr size_t image_source_byte_limit = 8192;
[[nodiscard]] enum image_outcome image_source_create(const char *_Nonnull destination,
                                                     struct note_search_span source,
                                                     struct image_source *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull image_source_target(const struct image_source *_Nonnull source);
[[nodiscard]] enum image_source_kind image_source_kind(const struct image_source *_Nonnull source);
[[nodiscard]] bool image_source_recommended(const struct image_source *_Nonnull source);
[[nodiscard]] struct note_search_span image_source_span(const struct image_source *_Nonnull source);
void image_source_destroy(struct image_source *_Nullable source);
#endif
