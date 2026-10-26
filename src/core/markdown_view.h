#ifndef NENEFOLIO_MARKDOWN_VIEW_H
#define NENEFOLIO_MARKDOWN_VIEW_H
#include "markdown_span.h"
#include <stdbool.h>
struct markdown_view
{
    const struct markdown_span *_Nonnull spans;
    size_t count;
    size_t length;
    bool table_cell;
};
/* Checked borrowed parse input; no ownership or persistent state. */
[[nodiscard]] bool markdown_view_valid(struct markdown_view view);
[[nodiscard]] char markdown_view_at(struct markdown_view view, size_t index);
#endif
