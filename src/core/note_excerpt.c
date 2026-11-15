#include "note_excerpt.h"
#include "note_excerpt_span.h"
#include "note_text.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct note_excerpt
{
    char *_Nonnull bytes;
    struct note_excerpt_span span;
};

static size_t separator_size(const char *_Nonnull bytes, size_t length, size_t at)
{
    if (bytes[at] == '\n')
        return 1;
    if (bytes[at] == '\r')
        return at + 1 < length && bytes[at + 1] == '\n' ? 2 : 1;
    return 0;
}

static struct note_excerpt_span prefix_span(const struct note_text *_Nonnull source)
{
    struct note_excerpt_span span = {.lines = 1, .extent = NOTE_EXCERPT_FULL};
    const char *_Nonnull bytes = note_text_bytes(source);
    size_t length = note_text_length(source);
    while (span.length < length)
    {
        size_t separator = separator_size(bytes, length, span.length);
        if (separator != 0 && span.lines == note_excerpt_line_limit)
        {
            span.extent = NOTE_EXCERPT_LINE_LIMIT;
            break;
        }
        uint32_t point = 0;
        size_t step = separator != 0
                          ? separator
                          : utf8_text_decode(bytes + span.length, length - span.length, &point);
        if (step > note_excerpt_byte_limit - span.length)
        {
            span.extent = NOTE_EXCERPT_BYTE_LIMIT;
            break;
        }
        span.length += step;
        if (separator != 0)
            span.offsets[span.lines++] = span.length;
    }
    return span;
}

enum note_excerpt_outcome note_excerpt_create(const struct note_text *_Nonnull source,
                                              struct note_excerpt *_Nullable *_Nonnull out)
{
    struct note_excerpt_span span = prefix_span(source);
    struct note_excerpt *_Nullable excerpt = malloc(sizeof *excerpt);
    char *_Nullable bytes = malloc(span.length + 1);
    if (excerpt == nullptr || bytes == nullptr)
    {
        free(bytes);
        free(excerpt);
        return NOTE_EXCERPT_OUT_OF_MEMORY;
    }
    memcpy(bytes, note_text_bytes(source), span.length);
    bytes[span.length] = '\0';
    excerpt->bytes = bytes;
    excerpt->span = span;
    *out = excerpt;
    return NOTE_EXCERPT_READY;
}

const char *_Nonnull note_excerpt_bytes(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->bytes;
}

size_t note_excerpt_length(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->span.length;
}

size_t note_excerpt_line_count(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->span.lines;
}

struct note_excerpt_line note_excerpt_line(const struct note_excerpt *_Nonnull excerpt,
                                           size_t index)
{
    size_t start = excerpt->span.offsets[index];
    size_t end =
        index + 1 < excerpt->span.lines ? excerpt->span.offsets[index + 1] : excerpt->span.length;
    while (end > start && (excerpt->bytes[end - 1] == '\r' || excerpt->bytes[end - 1] == '\n'))
        end -= 1;
    return (struct note_excerpt_line){.offset = start, .length = end - start};
}

enum note_excerpt_extent note_excerpt_extent(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->span.extent;
}

void note_excerpt_destroy(struct note_excerpt *_Nullable excerpt)
{
    if (excerpt == nullptr)
        return;
    free(excerpt->bytes);
    free(excerpt);
}
