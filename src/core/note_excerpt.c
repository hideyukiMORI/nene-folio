#include "note_excerpt.h"
#include "note_excerpt_span.h"
#include "note_source.h"
#include "note_text.h"
#include <stdlib.h>
#include <string.h>

struct note_excerpt
{
    char *_Nonnull bytes;
    struct note_excerpt_span span;
    struct note_search_span selection;
};

static bool source_boundary(const char *_Nonnull bytes, size_t length, size_t at)
{
    if (at == length)
        return true;
    if (((unsigned char)bytes[at] & 0xc0) == 0x80)
        return false;
    return at == 0 || bytes[at] != '\n' || bytes[at - 1] != '\r';
}

static size_t source_before(const struct note_text *_Nonnull source, size_t line_start,
                            size_t match_start, size_t budget)
{
    size_t start = match_start - line_start > budget ? match_start - budget : line_start;
    while (!source_boundary(note_text_bytes(source), note_text_length(source), start))
        ++start;
    return start;
}

static struct note_excerpt_span measure_span(const struct note_text *_Nonnull source, size_t start,
                                             size_t first_line)
{
    struct note_excerpt_span span = {
        .lines = 1, .extent = NOTE_EXCERPT_FULL, .start = start, .first_line = first_line};
    const char *_Nonnull bytes = note_text_bytes(source);
    size_t length = note_text_length(source);
    while (span.length < length - start)
    {
        size_t next = start + span.length;
        uint32_t point = note_source_next(bytes, length, &next);
        if (point == '\r' && span.lines == note_excerpt_line_limit)
        {
            span.extent = NOTE_EXCERPT_LINE_LIMIT;
            break;
        }
        size_t step = next - start - span.length;
        if (step > note_excerpt_byte_limit - span.length)
        {
            span.extent = NOTE_EXCERPT_BYTE_LIMIT;
            break;
        }
        span.length += step;
        if (point == '\r')
            span.offsets[span.lines++] = span.length;
    }
    size_t end = start + span.length;
    span.trailing_partial = end < length && bytes[end] != '\r' && bytes[end] != '\n';
    return span;
}

static enum note_excerpt_outcome create_span(const struct note_text *_Nonnull source,
                                             const struct note_excerpt_span *_Nonnull span,
                                             struct note_search_span selection,
                                             struct note_excerpt *_Nullable *_Nonnull out)
{
    struct note_excerpt *_Nullable excerpt = malloc(sizeof *excerpt);
    char *_Nullable bytes = malloc(span->length + 1);
    if (excerpt == nullptr || bytes == nullptr)
    {
        free(bytes);
        free(excerpt);
        return NOTE_EXCERPT_OUT_OF_MEMORY;
    }
    memcpy(bytes, note_text_bytes(source) + span->start, span->length);
    bytes[span->length] = '\0';
    excerpt->bytes = bytes;
    excerpt->span = *span;
    excerpt->selection = selection;
    *out = excerpt;
    return NOTE_EXCERPT_READY;
}

enum note_excerpt_outcome note_excerpt_create(const struct note_text *_Nonnull source,
                                              struct note_excerpt *_Nullable *_Nonnull out)
{
    struct note_excerpt_span span = measure_span(source, 0, 1);
    return create_span(source, &span, (struct note_search_span){0}, out);
}

static bool match_boundaries(const struct note_text *_Nonnull source,
                             const struct note_source_match *_Nonnull match)
{
    size_t length = note_text_length(source);
    const char *_Nonnull bytes = note_text_bytes(source);
    if (match->context_start > match->line_start || match->line_start > match->bytes.start ||
        match->bytes.start >= match->bytes.end || match->bytes.end > length ||
        match->bytes.end - match->bytes.start > 1024)
        return false;
    return source_boundary(bytes, length, match->context_start) &&
           source_boundary(bytes, length, match->line_start) &&
           source_boundary(bytes, length, match->bytes.start) &&
           source_boundary(bytes, length, match->bytes.end);
}

static bool single_line_match(const struct note_text *_Nonnull source,
                              const struct note_source_match *_Nonnull match)
{
    if (!match_boundaries(source, match) || match->context_line == 0 ||
        match->context_line > match->line ||
        match->line - match->context_line > note_source_context_lines)
        return false;
    const char *_Nonnull bytes = note_text_bytes(source);
    for (size_t at = match->bytes.start; at < match->bytes.end; ++at)
        if (bytes[at] == '\r' || bytes[at] == '\n')
            return false;
    return true;
}

static struct note_excerpt_span match_span(const struct note_text *_Nonnull source,
                                           const struct note_source_match *_Nonnull match)
{
    size_t start = match->context_start;
    size_t line = match->context_line;
    if (match->bytes.end - start > note_excerpt_byte_limit)
    {
        start = match->line_start;
        line = match->line;
    }
    if (match->bytes.end - start > note_excerpt_byte_limit)
    {
        start = source_before(source, match->line_start, match->bytes.start, 256);
    }
    struct note_excerpt_span span = measure_span(source, start, line);
    span.leading_partial = start > match->line_start;
    return span;
}

enum note_excerpt_outcome note_excerpt_create_match(const struct note_text *_Nonnull source,
                                                    const struct note_source_match *_Nonnull match,
                                                    struct note_excerpt *_Nullable *_Nonnull out)
{
    if (!single_line_match(source, match))
        return NOTE_EXCERPT_INVALID_RANGE;
    struct note_excerpt_span span = match_span(source, match);
    if (match->bytes.end - span.start > span.length)
        return NOTE_EXCERPT_INVALID_RANGE;
    const char *_Nonnull bytes = note_text_bytes(source) + span.start;
    struct note_search_span selection = {
        note_source_units(bytes, match->bytes.start - span.start, NOTE_SOURCE_CRLF),
        note_source_units(bytes, match->bytes.end - span.start, NOTE_SOURCE_CRLF)};
    return create_span(source, &span, selection, out);
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

size_t note_excerpt_first_line(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->span.first_line;
}

bool note_excerpt_leading_partial(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->span.leading_partial;
}

bool note_excerpt_trailing_partial(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->span.trailing_partial;
}

struct note_search_span note_excerpt_selection(const struct note_excerpt *_Nonnull excerpt)
{
    return excerpt->selection;
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

static bool snippet_space(uint32_t point)
{
    return point < 0x20 || point == 0x7f || point == 0x2028 || point == 0x2029;
}

struct note_snippet_clip note_excerpt_snippet(const struct note_text *_Nonnull source,
                                              const struct note_source_match *_Nonnull match,
                                              char *_Nonnull out)
{
    const char *_Nonnull bytes = note_text_bytes(source);
    size_t length = note_text_length(source);
    size_t at = source_before(source, match->line_start, match->bytes.start, 48);
    size_t written = 0;
    struct note_snippet_clip clip = {.leading = at > match->line_start};
    while (at < length && bytes[at] != '\r' && bytes[at] != '\n')
    {
        size_t next = at;
        uint32_t point = note_source_next(bytes, length, &next);
        size_t size = snippet_space(point) ? 1 : next - at;
        if (size > 192 - written)
            break;
        if (snippet_space(point))
            out[written] = ' ';
        else
            memcpy(out + written, bytes + at, size);
        written += size;
        at = next;
    }
    clip.trailing = at < length && bytes[at] != '\r' && bytes[at] != '\n';
    out[written] = '\0';
    return clip;
}
