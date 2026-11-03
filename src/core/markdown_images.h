#ifndef NENEFOLIO_MARKDOWN_IMAGES_H
#define NENEFOLIO_MARKDOWN_IMAGES_H
#include "image_source.h"
#include "markdown_view.h"
struct markdown_images;
struct text_buffer;
constexpr size_t markdown_images_reference_limit = 4096;
[[nodiscard]] enum image_outcome
markdown_images_create(const char *_Nonnull source,
                       struct markdown_images *_Nullable *_Nonnull out);
/* Internal render collector. The source and spans refer to the same Markdown allocation. */
[[nodiscard]] enum image_outcome markdown_images_append(struct markdown_images *_Nonnull images,
                                                        struct markdown_view view,
                                                        struct note_search_span destination);
[[nodiscard]] size_t markdown_images_count(const struct markdown_images *_Nonnull images);
[[nodiscard]] bool markdown_images_overflow(const struct markdown_images *_Nonnull images);
[[nodiscard]] bool markdown_images_advisory(const struct markdown_images *_Nonnull images);
[[nodiscard]] const struct image_source *_Nonnull markdown_images_at(
    const struct markdown_images *_Nonnull images, size_t index);
void markdown_images_destroy(struct markdown_images *_Nullable images);
#endif
