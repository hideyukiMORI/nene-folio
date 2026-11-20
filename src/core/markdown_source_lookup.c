/* #1012 / ADR0116: the caret for 「ここを編集」 and the Markdown for 「Markdownとしてコピー」, from
 * the source map of the reading view. */
#include "markdown_source_lookup.h"
#include "note_text.h"
#include "text_buffer.h"

/* The end of the line that starts at `at`. */
static size_t first_line_end(const char *_Nonnull bytes, size_t at, size_t end)
{
    while (at < end && bytes[at] != '\n' && bytes[at] != '\r')
        ++at;
    return at;
}

/* Past the next unescaped `|` on the line, or the line's end. */
static size_t next_cell(const char *_Nonnull bytes, size_t at, size_t end)
{
    while (at < end && bytes[at] != '|')
        at += bytes[at] == '\\' && at + 1 < end ? 2 : 1;
    return at < end ? at + 1 : end;
}

static size_t skip_spaces(const char *_Nonnull bytes, size_t at, size_t end)
{
    while (at < end && (bytes[at] == ' ' || bytes[at] == '\t'))
        ++at;
    return at;
}

/* How many cells the row's line holds (pipes between them, an outer pipe on either side or not). */
static size_t row_cells(const char *_Nonnull bytes, size_t at, size_t end)
{
    size_t count = 0;
    if (at < end && bytes[at] == '|')
        ++at;
    while (skip_spaces(bytes, at, end) < end)
    {
        ++count;
        at = next_cell(bytes, at, end);
    }
    return count == 0 ? 1 : count;
}

/* In a native table row the marks are [leading] start, one per shown cell, end; the cell under
 * the ordinal is the one whose text follows its mark. Only the header row (its entry also holds
 * the delimiter line) can be led by other paragraphs: a description's spacer, a list item's
 * marker. Its shown cells are its own (the delimiter must match it), so what is left over leads.
 * A body row is padded to the delimiter's columns and is never led, so a short row's padding is
 * not mistaken for a spacer (#1012 review). */
static size_t cell_caret(const char *_Nonnull bytes, struct markdown_source_entry entry,
                         size_t ordinal)
{
    size_t end = first_line_end(bytes, entry.caret, entry.end);
    size_t cells = row_cells(bytes, entry.caret, end);
    bool header = end < entry.end;
    size_t leading = header && entry.marks > cells + 2 ? entry.marks - cells - 2 : 0;
    size_t lead = entry.first + 1 + leading;
    size_t cell = ordinal > lead ? ordinal - lead : 0;
    if (cell >= cells)
        cell = cells - 1;
    size_t at = entry.caret;
    if (at < end && bytes[at] == '|')
        ++at;
    for (size_t index = 0; index < cell; ++index)
        at = next_cell(bytes, at, end);
    return skip_spaces(bytes, at, end);
}

size_t markdown_source_lookup_caret(const struct markdown_source_map *_Nonnull map,
                                    const struct note_text *_Nonnull text, size_t ordinal)
{
    size_t index = markdown_source_map_find(map, ordinal);
    if (index == markdown_source_none)
        return markdown_source_none;
    struct markdown_source_entry entry = markdown_source_map_at(map, index);
    if (entry.end > note_text_length(text))
        return markdown_source_none;
    return entry.kind == MARKDOWN_SOURCE_TABLE_ROW
               ? cell_caret(note_text_bytes(text), entry, ordinal)
               : entry.caret;
}

/* The lines of a whole group (a fence or a table), or of the entry alone outside one. */
static void group_extent(const struct markdown_source_map *_Nonnull map,
                         struct markdown_source_entry entry, size_t *_Nonnull begin,
                         size_t *_Nonnull end)
{
    *begin = entry.begin;
    *end = entry.end;
    for (size_t index = 0; entry.group != 0 && index < markdown_source_map_count(map); ++index)
    {
        struct markdown_source_entry other = markdown_source_map_at(map, index);
        if (other.group != entry.group)
            continue;
        *begin = other.begin < *begin ? other.begin : *begin;
        *end = other.end > *end ? other.end : *end;
    }
}

/* Appends source bytes with every CRLF and lone CR written as LF. */
static void append_lines(struct text_buffer *_Nonnull out, const char *_Nonnull bytes, size_t begin,
                         size_t end)
{
    size_t run = begin;
    for (size_t at = begin; at < end; ++at)
    {
        if (bytes[at] != '\r')
            continue;
        text_buffer_append(out, bytes + run, at - run);
        text_buffer_append_text(out, "\n");
        run = at + 1 < end && bytes[at + 1] == '\n' ? at + 2 : at + 1;
        at = run - 1;
    }
    text_buffer_append(out, bytes + run, end - run);
}

static bool footnote(struct markdown_source_entry entry)
{
    return entry.kind == MARKDOWN_SOURCE_FOOTNOTE || entry.kind == MARKDOWN_SOURCE_SEPARATOR;
}

/* The body's span from the first to the last body block in [first, last] (groups whole). */
static bool body_span(const struct markdown_source_map *_Nonnull map,
                      struct markdown_source_selection entries, size_t *_Nonnull begin,
                      size_t *_Nonnull end)
{
    bool found = false;
    for (size_t index = entries.first; index <= entries.last; ++index)
    {
        struct markdown_source_entry entry = markdown_source_map_at(map, index);
        if (footnote(entry))
            continue;
        size_t from = 0;
        size_t to = 0;
        group_extent(map, entry, &from, &to);
        *begin = !found || from < *begin ? from : *begin;
        *end = !found || to > *end ? to : *end;
        found = true;
    }
    return found;
}

/* Each footnote definition in [first, last] once (the separator names the first footnote). A
 * definition written among the body's lines is already in the body's span, so it is not written
 * again (#1012 review). */
static bool append_footnotes(const struct markdown_source_map *_Nonnull map,
                             const struct note_text *_Nonnull text,
                             struct markdown_source_selection entries,
                             struct text_buffer *_Nonnull out)
{
    size_t begin = 0;
    size_t end = 0;
    bool body = body_span(map, entries, &begin, &end);
    bool written = text_buffer_length(out) != 0;
    size_t previous = markdown_source_none;
    for (size_t index = entries.first; index <= entries.last; ++index)
    {
        struct markdown_source_entry entry = markdown_source_map_at(map, index);
        bool covered = body && entry.begin >= begin && entry.end <= end;
        if (!footnote(entry) || entry.begin == previous || covered)
            continue;
        if (entry.end > note_text_length(text))
            return false;
        if (written)
            text_buffer_append_text(out, "\n\n");
        append_lines(out, note_text_bytes(text), entry.begin, entry.end);
        previous = entry.begin;
        written = true;
    }
    return written;
}

bool markdown_source_lookup_copy(const struct markdown_source_map *_Nonnull map,
                                 const struct note_text *_Nonnull text,
                                 struct markdown_source_selection selection,
                                 struct text_buffer *_Nonnull out)
{
    size_t first = markdown_source_map_find(map, selection.first);
    size_t last = markdown_source_map_find(map, selection.last);
    if (first == markdown_source_none || last == markdown_source_none || last < first)
        return false;
    const char *bytes = note_text_bytes(text);
    size_t begin = 0;
    size_t end = 0;
    struct markdown_source_selection entries = {first, last};
    if (body_span(map, entries, &begin, &end))
    {
        if (end > note_text_length(text))
            return false;
        append_lines(out, bytes, begin, end);
    }
    return append_footnotes(map, text, entries, out);
}

/* Whether the block's source lines and the span share a byte; an empty span is a caret, and a
 * block holds it when the caret is on its lines (their ends included). */
static bool overlaps(struct markdown_source_entry entry, struct note_byte_span source)
{
    if (entry.kind == MARKDOWN_SOURCE_SEPARATOR)
        return false;
    if (source.start == source.end)
        return entry.begin <= source.start && source.start <= entry.end;
    return entry.begin < source.end && entry.end > source.start;
}

/* The run of overlapping blocks, from the first in the view's order. A block that writes no
 * paragraph (a closing fence) neither extends nor ends the run. */
static bool overlapping_run(const struct markdown_source_map *_Nonnull map,
                            struct note_byte_span source,
                            struct markdown_source_selection *_Nonnull out)
{
    bool started = false;
    size_t count = markdown_source_map_count(map);
    for (size_t index = 0; index < count; ++index)
    {
        struct markdown_source_entry entry = markdown_source_map_at(map, index);
        if (entry.marks == 0)
            continue;
        if (!overlaps(entry, source))
        {
            if (started)
                break;
            continue;
        }
        if (!started)
            out->first = entry.first;
        out->last = entry.first + entry.marks;
        started = true;
    }
    return started;
}

/* The block whose lines begin soonest at or after the span's start. */
static bool next_block(const struct markdown_source_map *_Nonnull map, struct note_byte_span source,
                       struct markdown_source_selection *_Nonnull out)
{
    bool found = false;
    size_t nearest = 0;
    size_t count = markdown_source_map_count(map);
    for (size_t index = 0; index < count; ++index)
    {
        struct markdown_source_entry entry = markdown_source_map_at(map, index);
        bool candidate = entry.marks != 0 && entry.kind != MARKDOWN_SOURCE_SEPARATOR &&
                         entry.begin >= source.start;
        if (candidate && (!found || entry.begin < nearest))
        {
            nearest = entry.begin;
            out->first = entry.first;
            out->last = entry.first + entry.marks;
            found = true;
        }
    }
    return found;
}

bool markdown_source_lookup_paragraphs(const struct markdown_source_map *_Nonnull map,
                                       struct note_byte_span source,
                                       struct markdown_source_selection *_Nonnull out)
{
    struct markdown_source_selection found = {0, 0};
    if (!overlapping_run(map, source, &found) && !next_block(map, source, &found))
        return false;
    *out = found;
    return true;
}
