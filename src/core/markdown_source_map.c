/* #1008 / ADR0116: the source map. Finishing reads the RTF the traversal wrote and counts, per
 * entry, the control words that leave a paragraph mark in RichEdit's text (checked with a
 * hidden control, D:/NeNeFolio/agents/source-map-20261119/probe-output.txt):
 * \par is one CR; a table row is a start mark (\trowd), one U+0007 per \cell and an end mark
 * (\row). A \par that ends the document is taken into RichEdit's own last mark. */
#include "markdown_source_map.h"
#include "markdown_source_scan.h"
#include <stdlib.h>
#include <string.h>
constexpr size_t first_capacity = 16;

struct markdown_source_map
{
    struct markdown_source_entry *_Nullable entries;
    size_t count;
    size_t capacity;
    size_t total;
};

enum markdown_rtf_outcome
markdown_source_map_create(struct markdown_source_map *_Nullable *_Nonnull out)
{
    struct markdown_source_map *_Nullable map = calloc(1, sizeof *map);
    if (map == nullptr)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    *out = map;
    return MARKDOWN_RTF_CONVERTED;
}

void markdown_source_map_destroy(struct markdown_source_map *_Nullable map)
{
    if (map == nullptr)
        return;
    free(map->entries);
    free(map);
}

bool markdown_source_map_add(struct markdown_source_map *_Nonnull map,
                             struct markdown_source_entry entry)
{
    if (map->count == map->capacity)
    {
        size_t capacity = map->capacity == 0 ? first_capacity : map->capacity * 2;
        struct markdown_source_entry *_Nullable grown =
            realloc(map->entries, capacity * sizeof *grown);
        if (grown == nullptr)
            return false;
        map->entries = grown;
        map->capacity = capacity;
    }
    entry.first = entry.marks = 0;
    map->entries[map->count++] = entry;
    return true;
}

static bool letter(char value)
{
    return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
}

static bool word_is(const char *_Nonnull word, size_t length, const char *_Nonnull name)
{
    return strlen(name) == length && memcmp(word, name, length) == 0;
}

static void count_mark(struct markdown_source_map *_Nonnull map,
                       struct markdown_source_scan *_Nonnull scan, bool par)
{
    if (scan->entry != markdown_source_none)
        ++map->entries[scan->entry].marks;
    else
        ++scan->orphans;
    ++scan->marks;
    scan->last_par = par;
    scan->text_after = false;
}

static bool digit(char value)
{
    return value >= '0' && value <= '9';
}

/* Past a control word's number (with its sign) and the one space that ends it. */
static size_t parameter_end(const struct markdown_source_scan *_Nonnull scan, size_t at)
{
    if (at < scan->length && (scan->rtf[at] == '-' || digit(scan->rtf[at])))
        ++at;
    while (at < scan->length && digit(scan->rtf[at]))
        ++at;
    return at < scan->length && scan->rtf[at] == ' ' ? at + 1 : at;
}

/* What a control word does to the count: \par, \cell, \row and \trowd are marks; \uN is
 * text. */
static void classify(struct markdown_source_map *_Nonnull map,
                     struct markdown_source_scan *_Nonnull scan, const char *_Nonnull word,
                     size_t size)
{
    if (word_is(word, size, "par"))
        count_mark(map, scan, true);
    else if (word_is(word, size, "cell") || word_is(word, size, "row") ||
             word_is(word, size, "trowd"))
        count_mark(map, scan, false);
    else if (word_is(word, size, "u"))
        scan->text_after = true;
}

/* A control word at `at` (just after its backslash). Returns where the RTF goes on. */
static size_t control_word(struct markdown_source_map *_Nonnull map,
                           struct markdown_source_scan *_Nonnull scan, size_t at)
{
    size_t first = at;
    while (at < scan->length && letter(scan->rtf[at]))
        ++at;
    classify(map, scan, scan->rtf + first, at - first);
    return parameter_end(scan, at);
}

/* What follows a backslash at `at`: a control word, or an escaped character (text, except the
 * destination mark `\*`). */
static size_t control(struct markdown_source_map *_Nonnull map,
                      struct markdown_source_scan *_Nonnull scan, size_t at)
{
    if (at + 1 >= scan->length)
        return scan->length;
    char next = scan->rtf[at + 1];
    if (letter(next))
        return control_word(map, scan, at + 1);
    if (next != '*')
        scan->text_after = true;
    return at + (next == '\'' ? 4 : 2);
}

static void enter_entries(const struct markdown_source_map *_Nonnull map,
                          struct markdown_source_scan *_Nonnull scan, size_t at)
{
    while (scan->next < map->count && map->entries[scan->next].rtf <= at)
        scan->entry = scan->next++;
}

void markdown_source_map_finish(struct markdown_source_map *_Nonnull map, const char *_Nonnull rtf,
                                size_t length)
{
    struct markdown_source_scan scan = {
        .rtf = rtf, .length = length, .entry = markdown_source_none};
    size_t at = 0;
    while (at < length)
    {
        enter_entries(map, &scan, at);
        char value = rtf[at];
        if (value == '\\')
        {
            at = control(map, &scan, at);
            continue;
        }
        if (value != '{' && value != '}' && value != '\n' && value != '\r')
            scan.text_after = true;
        ++at;
    }
    size_t first = scan.orphans;
    for (size_t index = 0; index < map->count; ++index)
    {
        map->entries[index].first = first;
        first += map->entries[index].marks;
    }
    map->total = scan.marks - (scan.last_par && !scan.text_after ? 1 : 0);
}

size_t markdown_source_map_total(const struct markdown_source_map *_Nonnull map)
{
    return map->total;
}

size_t markdown_source_map_count(const struct markdown_source_map *_Nonnull map)
{
    return map->count;
}

struct markdown_source_entry markdown_source_map_at(const struct markdown_source_map *_Nonnull map,
                                                    size_t index)
{
    return map->entries[index];
}

/* Entries with no marks (a closing fence) never hold an ordinal; the last entry with marks
 * holds the text after the final mark. */
size_t markdown_source_map_find(const struct markdown_source_map *_Nonnull map, size_t ordinal)
{
    size_t found = markdown_source_none;
    for (size_t index = 0; index < map->count; ++index)
    {
        const struct markdown_source_entry *entry = &map->entries[index];
        if (entry->marks != 0 && entry->first <= ordinal)
            found = index;
    }
    if (found == markdown_source_none)
        return found;
    const struct markdown_source_entry *entry = &map->entries[found];
    return ordinal < entry->first + entry->marks || ordinal == map->total ? found
                                                                          : markdown_source_none;
}
