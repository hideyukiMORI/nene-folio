#include "markdown_references.h"
#include "markdown_destination.h"
#include "markdown_line.h"
#include "markdown_span.h"
#include "note_metadata.h"
#include <stdlib.h>
#include <string.h>

struct markdown_references
{
    struct markdown_reference
    {
        char *_Nonnull label;
        size_t label_length;
        char *_Nonnull destination;
        size_t destination_length;
        size_t offset;
        size_t line_length;
        struct note_search_span target; /* the destination, or a footnote's text (#863, #869) */
        bool footnote;
        size_t number; /* a footnote's number once referred to, or 0 */
    } *_Nullable entries;
    size_t *_Nullable order; /* the numbered footnotes in number order (#869) */
    size_t numbered;
    const char *_Nullable source; /* the note's bytes, borrowed for one render */
    size_t count;
    size_t capacity;
    /* The scan's state between lines: an open fence, and whether a definition may start
     * (it never interrupts a paragraph). */
    size_t fence;
    char marker;
    bool allowed;
};

static bool space(char value)
{
    return value == ' ' || value == '\t' || value == '\n' || value == '\r';
}

static char lower(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value + ('a' - 'A')) : value;
}

/* CommonMark label matching: ASCII case folded, whitespace runs as one space, trimmed. The
 * result is at most the input's length; false for a blank or an over-long label. */
static bool normalize(struct markdown_view view, struct note_search_span range, char *_Nonnull out,
                      size_t *_Nonnull length)
{
    if (range.end < range.start || range.end - range.start > markdown_references_label_limit)
        return false;
    size_t written = 0;
    bool pending = false;
    for (size_t index = range.start; index < range.end; ++index)
    {
        char value = markdown_view_at(view, index);
        if (space(value))
        {
            pending = written != 0;
            continue;
        }
        if (pending)
            out[written++] = ' ';
        pending = false;
        out[written++] = lower(value);
    }
    *length = written;
    return written != 0;
}

/* The `]` that closes a label opened before `start`, or SIZE_MAX (no nested brackets). */
static size_t label_close(struct markdown_view view, size_t start, size_t end)
{
    for (size_t index = start; index < end; ++index)
    {
        char value = markdown_view_at(view, index);
        if (value == '\\')
        {
            ++index;
            continue;
        }
        if (value == '[')
            return SIZE_MAX;
        if (value == ']')
            return index;
    }
    return SIZE_MAX;
}

static char *_Nullable copy_bytes(const char *_Nonnull bytes, size_t length)
{
    char *_Nullable copy = malloc(length + 1);
    if (copy != nullptr)
    {
        memcpy(copy, bytes, length);
        copy[length] = '\0';
    }
    return copy;
}

static bool reserve(struct markdown_references *_Nonnull table)
{
    if (table->count < table->capacity)
        return true;
    size_t capacity = table->capacity == 0 ? 8 : table->capacity * 2;
    capacity = capacity > markdown_references_limit ? markdown_references_limit : capacity;
    struct markdown_reference *_Nullable grown = realloc(table->entries, capacity * sizeof *grown);
    if (grown == nullptr)
        return false;
    table->entries = grown;
    table->capacity = capacity;
    return true;
}

/* Copies the borrowed label and the destination's bytes into a new entry of the table. */
static enum markdown_rtf_outcome record(struct markdown_references *_Nonnull table,
                                        struct markdown_reference entry,
                                        const struct markdown_destination *_Nonnull destination)
{
    entry.destination_length = markdown_destination_length(destination);
    char *_Nullable label = copy_bytes(entry.label, entry.label_length);
    char *_Nullable bytes =
        copy_bytes(markdown_destination_bytes(destination), entry.destination_length);
    if (label == nullptr || bytes == nullptr || !reserve(table))
    {
        free(label);
        free(bytes);
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    entry.label = label;
    entry.destination = bytes;
    table->entries[table->count++] = entry;
    return MARKDOWN_RTF_CONVERTED;
}

/* #869: a footnote keeps its text's place in the line; it has no destination. */
static enum markdown_rtf_outcome footnote(struct markdown_references *_Nonnull table,
                                          struct markdown_reference entry,
                                          struct markdown_line line, bool *_Nonnull defined)
{
    size_t start = entry.target.start;
    size_t end = line.length;
    while (start < end && space(line.bytes[start]))
        ++start;
    while (end > start && space(line.bytes[end - 1]))
        --end;
    char *_Nullable label = copy_bytes(entry.label, entry.label_length);
    char *_Nullable empty = copy_bytes("", 0);
    if (label == nullptr || empty == nullptr || !reserve(table))
    {
        free(label);
        free(empty);
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    entry.label = label;
    entry.destination = empty;
    entry.target = (struct note_search_span){start, end};
    entry.footnote = true;
    table->entries[table->count++] = entry;
    *defined = true;
    return MARKDOWN_RTF_CONVERTED;
}

/* A one-line definition `[label]: destination "title"`, indented by at most three columns.
 * MARKDOWN_RTF_CONVERTED with *defined when the line was recorded. */
static enum markdown_rtf_outcome definition(struct markdown_references *_Nonnull table,
                                            struct markdown_line line, size_t offset,
                                            bool *_Nonnull defined)
{
    *defined = false;
    if (line.indent > 3 || line.content >= line.length || line.bytes[line.content] != '[' ||
        table->count == markdown_references_limit)
        return MARKDOWN_RTF_CONVERTED;
    struct markdown_span span = {.bytes = line.bytes, .length = line.length, .offset = 0};
    struct markdown_view view = {.spans = &span, .count = 1, .length = line.length};
    size_t close = label_close(view, line.content + 1, line.length);
    char label[markdown_references_label_limit];
    struct markdown_reference entry = {.label = label,
                                       .destination = label,
                                       .offset = offset,
                                       .line_length = line.length,
                                       .target = {close + 2, line.length}};
    if (close == SIZE_MAX || close + 1 >= line.length || line.bytes[close + 1] != ':' ||
        !normalize(view, (struct note_search_span){line.content + 1, close}, label,
                   &entry.label_length))
        return MARKDOWN_RTF_CONVERTED;
    if (label[0] == '^')
        return footnote(table, entry, line, defined);
    struct markdown_destination *_Nullable destination = nullptr;
    enum image_outcome read = markdown_destination_create(
        view, (struct note_search_span){close + 2, line.length}, &destination);
    if (read == IMAGE_OUT_OF_MEMORY)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    if (read != IMAGE_READY)
        return MARKDOWN_RTF_CONVERTED;
    enum markdown_rtf_outcome kept = record(table, entry, destination);
    markdown_destination_destroy(destination);
    *defined = kept == MARKDOWN_RTF_CONVERTED;
    return kept;
}

static void after_line(struct markdown_references *_Nonnull table, struct markdown_line line,
                       bool defined)
{
    size_t run = markdown_line_fence(line);
    if (table->fence != 0)
    {
        if (run >= table->fence && line.bytes[line.content] == table->marker)
            table->fence = 0;
        table->allowed = table->fence == 0;
        return;
    }
    if (run != 0)
    {
        table->fence = run;
        table->marker = line.bytes[line.content];
        table->allowed = false;
        return;
    }
    table->allowed = defined || markdown_line_blank(line) || markdown_line_heading(line) != 0 ||
                     markdown_line_thematic(line);
}

static enum markdown_rtf_outcome scan(struct markdown_references *_Nonnull table,
                                      const char *_Nonnull source, size_t length, size_t cursor)
{
    table->allowed = true;
    while (cursor < length)
    {
        struct markdown_line line = markdown_line_read(source, length, cursor);
        bool defined = false;
        if (table->fence == 0 && table->allowed)
        {
            enum markdown_rtf_outcome read = definition(table, line, cursor, &defined);
            if (read != MARKDOWN_RTF_CONVERTED)
                return read;
        }
        after_line(table, line, defined);
        cursor = line.next;
    }
    return MARKDOWN_RTF_CONVERTED;
}

enum markdown_rtf_outcome
markdown_references_create(const struct note_text *_Nonnull text,
                           struct markdown_references *_Nullable *_Nonnull out)
{
    struct markdown_references *_Nullable table = calloc(1, sizeof *table);
    if (table == nullptr)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    table->source = note_text_bytes(text);
    enum markdown_rtf_outcome scanned = scan(table, note_text_bytes(text), note_text_length(text),
                                             note_metadata_body(note_text_metadata(text)));
    if (scanned == MARKDOWN_RTF_CONVERTED && table->count != 0)
    {
        table->order = calloc(table->count, sizeof *table->order);
        scanned = table->order == nullptr ? MARKDOWN_RTF_OUT_OF_MEMORY : scanned;
    }
    if (scanned != MARKDOWN_RTF_CONVERTED)
    {
        markdown_references_destroy(table);
        return scanned;
    }
    *out = table;
    return MARKDOWN_RTF_CONVERTED;
}

bool markdown_references_defines(const struct markdown_references *_Nullable table, size_t offset)
{
    if (table == nullptr)
        return false;
    /* Definitions are recorded in source order, so the offsets are increasing. */
    size_t low = 0;
    size_t high = table->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (table->entries[middle].offset < offset)
            low = middle + 1;
        else
            high = middle;
    }
    return low < table->count && table->entries[low].offset == offset;
}

static size_t find_kind(const struct markdown_references *_Nullable table,
                        struct markdown_view view, struct note_search_span range, bool footnotes)
{
    char label[markdown_references_label_limit];
    size_t length = 0;
    if (table == nullptr || !normalize(view, range, label, &length))
        return SIZE_MAX;
    for (size_t index = 0; index < table->count; ++index)
    {
        const struct markdown_reference *entry = &table->entries[index];
        if (entry->footnote == footnotes && entry->label_length == length &&
            memcmp(entry->label, label, length) == 0)
            return index;
    }
    return SIZE_MAX;
}

size_t markdown_references_find(const struct markdown_references *_Nullable table,
                                struct markdown_view view, size_t start, size_t end)
{
    return find_kind(table, view, (struct note_search_span){start, end}, false);
}

size_t markdown_references_footnote(const struct markdown_references *_Nullable table,
                                    struct markdown_view view, size_t start, size_t end)
{
    return find_kind(table, view, (struct note_search_span){start, end}, true);
}

size_t markdown_references_number(struct markdown_references *_Nonnull table, size_t index)
{
    struct markdown_reference *entry = &table->entries[index];
    if (entry->number == 0 && table->order != nullptr)
    {
        table->order[table->numbered++] = index;
        entry->number = table->numbered;
    }
    return entry->number;
}

size_t markdown_references_numbered(const struct markdown_references *_Nullable table)
{
    return table == nullptr ? 0 : table->numbered;
}

size_t markdown_references_numbered_at(const struct markdown_references *_Nonnull table,
                                       size_t rank)
{
    return table->order != nullptr ? table->order[rank] : 0;
}

const char *_Nonnull markdown_references_destination(
    const struct markdown_references *_Nonnull table, size_t index)
{
    return table->entries[index].destination;
}

size_t markdown_references_destination_length(const struct markdown_references *_Nonnull table,
                                              size_t index)
{
    return table->entries[index].destination_length;
}

struct markdown_span markdown_references_line(const struct markdown_references *_Nonnull table,
                                              size_t index)
{
    const struct markdown_reference *entry = &table->entries[index];
    /* The table is only made from a note's text, so the source is set whenever it has entries. */
    const char *_Nonnull source = table->source != nullptr ? table->source : "";
    return (struct markdown_span){
        .bytes = source + entry->offset, .length = entry->line_length, .offset = 0};
}

struct note_search_span
markdown_references_destination_span(const struct markdown_references *_Nonnull table, size_t index)
{
    return table->entries[index].target;
}

void markdown_references_destroy(struct markdown_references *_Nullable table)
{
    if (table == nullptr)
        return;
    for (size_t index = 0; index < table->count; ++index)
    {
        free(table->entries[index].label);
        free(table->entries[index].destination);
    }
    free(table->entries);
    free(table->order);
    free(table);
}
