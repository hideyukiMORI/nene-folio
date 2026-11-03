#ifndef NENEFOLIO_MARKDOWN_MEDIA_H
#define NENEFOLIO_MARKDOWN_MEDIA_H
#include "markdown_images.h"
#include "markdown_line.h"
#include "markdown_media_kind.h"
#include "note_text.h"
struct markdown_media;
constexpr size_t markdown_media_display_limit = 256;
[[nodiscard]] enum image_outcome
markdown_media_create(const char *_Nonnull source, struct markdown_media *_Nullable *_Nonnull out);
/* Display slots and export image references are distinct collections of the same traversal. */
[[nodiscard]] enum image_outcome markdown_media_image(struct markdown_media *_Nonnull media,
                                                      struct markdown_view view,
                                                      struct note_search_span destination,
                                                      size_t *_Nonnull slot);
[[nodiscard]] enum image_outcome markdown_media_begin_diagram(struct markdown_media *_Nonnull media,
                                                              size_t *_Nonnull slot);
void markdown_media_diagram_line(struct markdown_media *_Nonnull media, size_t slot,
                                 struct markdown_line line);
[[nodiscard]] enum image_outcome markdown_media_end_diagram(struct markdown_media *_Nonnull media,
                                                            size_t slot);
[[nodiscard]] size_t markdown_media_count(const struct markdown_media *_Nonnull media);
[[nodiscard]] bool markdown_media_overflow(const struct markdown_media *_Nonnull media);
[[nodiscard]] const struct markdown_images *_Nonnull markdown_media_images(
    const struct markdown_media *_Nonnull media);
[[nodiscard]] enum markdown_media_kind
markdown_media_kind(const struct markdown_media *_Nonnull media, size_t slot);
[[nodiscard]] enum image_outcome markdown_media_status(const struct markdown_media *_Nonnull media,
                                                       size_t slot);
[[nodiscard]] size_t markdown_media_image_index(const struct markdown_media *_Nonnull media,
                                                size_t slot);
[[nodiscard]] const struct note_text *_Nonnull markdown_media_diagram(
    const struct markdown_media *_Nonnull media, size_t slot);
void markdown_media_placeholder(struct text_buffer *_Nonnull output, size_t index);
void markdown_media_destroy(struct markdown_media *_Nullable media);
#endif
