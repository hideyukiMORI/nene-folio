#include "markdown_destination.h"
#include "markdown_character_reference.h"
#include "text_buffer.h"
#include "utf8_text.h"
#include <stdlib.h>

struct markdown_destination
{
    struct text_buffer *_Nonnull buffer;
    struct note_search_span span;
};
static bool space(char character)
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r';
}
static size_t destination_end(struct markdown_view view, size_t start, size_t end)
{
    bool angle = markdown_view_at(view, start) == '<';
    size_t cursor = start + (angle ? 1 : 0);
    for (; cursor < end; ++cursor)
    {
        char character = markdown_view_at(view, cursor);
        if (character == '\\' && cursor + 1 < end)
        {
            ++cursor;
            continue;
        }
        if (angle ? character == '>' : space(character))
            return cursor;
    }
    return angle ? SIZE_MAX : end;
}
static bool valid_title(struct markdown_view view, size_t start, size_t end)
{
    if (start == end)
        return true;
    if (!space(markdown_view_at(view, start)))
        return false;
    while (start < end && space(markdown_view_at(view, start)))
        ++start;
    while (end > start && space(markdown_view_at(view, end - 1)))
        --end;
    if (start == end)
        return true;
    char opening = markdown_view_at(view, start);
    char closing = opening == '(' ? ')' : opening;
    return (opening == '"' || opening == '\'' || opening == '(') && end - start >= 2 &&
           markdown_view_at(view, end - 1) == closing;
}
static bool destination_span(struct markdown_view view, struct note_search_span *_Nonnull span)
{
    while (span->start < span->end && space(markdown_view_at(view, span->start)))
        ++span->start;
    if (span->start == span->end)
        return false;
    size_t end = destination_end(view, span->start, span->end);
    if (end == SIZE_MAX)
        return false;
    bool angle = markdown_view_at(view, span->start) == '<';
    if (!valid_title(view, end + (angle ? 1 : 0), span->end))
        return false;
    span->start += angle ? 1 : 0;
    span->end = end;
    return span->start < span->end;
}

static bool punctuation(char character)
{
    return (character > ' ' && character < '0') || (character > '9' && character < 'A') ||
           (character > 'Z' && character < 'a') || (character > 'z' && character < 0x7F);
}

static size_t append_decoded(struct text_buffer *_Nonnull buffer, struct markdown_view view,
                             size_t start, size_t end)
{
    char character = markdown_view_at(view, start);
    if (character == '\\' && start + 1 < end && punctuation(markdown_view_at(view, start + 1)))
    {
        character = markdown_view_at(view, start + 1);
        text_buffer_append(buffer, &character, 1);
        return 2;
    }
    uint32_t point = 0;
    size_t length = character == '&' ? markdown_character_reference_read(view, start, &point) : 0;
    if (length != 0 && length <= end - start)
    {
        char encoded[utf8_text_max_encoded];
        size_t count = utf8_text_encode(point, encoded);
        text_buffer_append(buffer, encoded, count);
        return length;
    }
    text_buffer_append(buffer, &character, 1);
    return 1;
}

enum image_outcome markdown_destination_create(struct markdown_view view,
                                               struct note_search_span span,
                                               struct markdown_destination *_Nullable *_Nonnull out)
{
    if (span.start >= span.end || span.end > view.length)
        return IMAGE_UNSUPPORTED;
    if (!destination_span(view, &span))
        return IMAGE_UNSUPPORTED;
    struct markdown_destination *_Nullable value = malloc(sizeof *value);
    if (value == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    if (text_buffer_create(&value->buffer) != TEXT_BUFFER_ACCEPTED)
    {
        free(value);
        return IMAGE_OUT_OF_MEMORY;
    }
    value->span = span;
    while (span.start < span.end)
        span.start += append_decoded(value->buffer, view, span.start, span.end);
    if (text_buffer_finish(value->buffer) != TEXT_BUFFER_ACCEPTED)
    {
        markdown_destination_destroy(value);
        return IMAGE_OUT_OF_MEMORY;
    }
    *out = value;
    return IMAGE_READY;
}

const char *_Nonnull markdown_destination_bytes(const struct markdown_destination *_Nonnull value)
{
    return text_buffer_bytes(value->buffer);
}
struct note_search_span markdown_destination_span(const struct markdown_destination *_Nonnull value)
{
    return value->span;
}
size_t markdown_destination_length(const struct markdown_destination *_Nonnull value)
{
    return text_buffer_length(value->buffer);
}
void markdown_destination_destroy(struct markdown_destination *_Nullable value)
{
    if (value != nullptr)
        text_buffer_destroy(value->buffer);
    free(value);
}
