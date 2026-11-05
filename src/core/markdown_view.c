#include "markdown_view.h"
#include <stdint.h>
bool markdown_view_valid(struct markdown_view view)
{
    size_t offset = 0;
    for (size_t index = 0; index < view.count; ++index)
    {
        const struct markdown_span *span = &view.spans[index];
        if (span->offset != offset || span->length > SIZE_MAX - offset)
            return false;
        offset += span->length;
        if (index + 1 < view.count)
        {
            if (offset == SIZE_MAX)
                return false;
            ++offset;
        }
    }
    return offset == view.length;
}

static bool predicted_span(struct markdown_view view, size_t index, size_t *_Nonnull found)
{
    if (view.count < 32 || view.spans[1].offset == 0 ||
        view.spans[0].length != view.spans[1].length ||
        view.spans[0].length != view.spans[view.count - 2].length)
        return false;
    size_t estimate = index / view.spans[1].offset;
    if (estimate >= view.count)
        return false;
    const struct markdown_span *predicted = &view.spans[estimate];
    if (predicted->offset > index || index - predicted->offset > predicted->length)
        return false;
    *found = estimate;
    return true;
}

static size_t containing_span(struct markdown_view view, size_t index)
{
    if (index <= view.spans[0].length)
        return 0;
    if (view.spans[view.count - 1].offset <= index)
        return view.count - 1;
    size_t low = 0;
    if (predicted_span(view, index, &low))
        return low;
    size_t remaining = view.count;
    while (remaining > 1)
    {
        size_t half = remaining / 2;
        size_t middle = low + half;
        if (view.spans[middle].offset <= index)
            low = middle;
        remaining -= half;
    }
    return low;
}

char markdown_view_at(struct markdown_view view, size_t index)
{
    if (view.count == 0 || index >= view.length)
        return '\0';
    size_t found = 0;
    if (view.count > 1)
        found = containing_span(view, index);
    const struct markdown_span *span = &view.spans[found];
    if (index < span->offset)
        return '\0';
    size_t local = index - span->offset;
    if (local > span->length)
        return '\0';
    return local == span->length ? '\n' : span->bytes[local];
}
