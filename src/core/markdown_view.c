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

char markdown_view_at(struct markdown_view view, size_t index)
{
    if (view.count == 0 || index >= view.length)
        return '\0';
    size_t low = 0;
    size_t high = view.count;
    while (low + 1 < high)
    {
        size_t middle = low + (high - low) / 2;
        if (view.spans[middle].offset <= index)
            low = middle;
        else
            high = middle;
    }
    const struct markdown_span *span = &view.spans[low];
    if (index < span->offset)
        return '\0';
    size_t local = index - span->offset;
    if (local > span->length)
        return '\0';
    return local == span->length ? '\n' : span->bytes[local];
}
