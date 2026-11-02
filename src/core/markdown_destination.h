#ifndef NENEFOLIO_MARKDOWN_DESTINATION_H
#define NENEFOLIO_MARKDOWN_DESTINATION_H
#include "image_outcome.h"
#include "markdown_view.h"
#include "note_search_span.h"
struct markdown_destination;
[[nodiscard]] struct note_search_span
markdown_destination_span(const struct markdown_destination *_Nonnull value);
[[nodiscard]] enum image_outcome
markdown_destination_create(struct markdown_view view, struct note_search_span span,
                            struct markdown_destination *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull markdown_destination_bytes(
    const struct markdown_destination *_Nonnull value);
[[nodiscard]] size_t markdown_destination_length(const struct markdown_destination *_Nonnull value);
void markdown_destination_destroy(struct markdown_destination *_Nullable value);
#endif
