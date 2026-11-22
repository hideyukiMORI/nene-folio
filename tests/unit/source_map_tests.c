/* #1008 / ADR0116: the reading view's source map. Counting marks on hand-written RTF, then the
 * entries the real traversal records for each kind of block. The native check that RichEdit
 * keeps exactly `total` marks is in the win32 tests. */
#include "allocation_probe.h"
#include "folio_theme.h"
#include "markdown_rtf.h"
#include "markdown_source_lookup.h"
#include "markdown_source_map.h"
#include "note_text.h"
#include "text_buffer.h"
#include "unit_tests.h"
#include <stdio.h>
#include <string.h>

static struct markdown_source_map *_Nonnull counted(const char *_Nonnull rtf,
                                                    const size_t *_Nonnull at, size_t count)
{
    struct markdown_source_map *map = nullptr;
    require(markdown_source_map_create(&map) == MARKDOWN_RTF_CONVERTED, "source map");
    for (size_t index = 0; index < count; ++index)
        require(markdown_source_map_add(map, (struct markdown_source_entry){.rtf = at[index]}),
                "source map entry");
    markdown_source_map_finish(map, rtf, strlen(rtf));
    return map;
}

static void expect_marks(const struct markdown_source_map *_Nonnull map,
                         const size_t *_Nonnull marks, size_t count, size_t total)
{
    require(markdown_source_map_count(map) == count, "entry count");
    for (size_t index = 0; index < count; ++index)
    {
        struct markdown_source_entry entry = markdown_source_map_at(map, index);
        if (entry.marks != marks[index])
            fprintf(stderr, "entry %zu: %zu marks, expected %zu\n", index, entry.marks,
                    marks[index]);
        require(entry.marks == marks[index], "an entry's marks");
    }
    if (markdown_source_map_total(map) != total)
        fprintf(stderr, "total %zu, expected %zu\n", markdown_source_map_total(map), total);
    require(markdown_source_map_total(map) == total, "the view's total");
}

/* What counts as a mark, and the final \par that RichEdit takes into its own. */
static void counting(void)
{
    const char two[] = "{\\rtf1 a\\par b\\par}";
    const size_t two_at[] = {7, 13};
    struct markdown_source_map *map = counted(two, two_at, 2);
    expect_marks(map, (const size_t[]){1, 1}, 2, 1);
    require(markdown_source_map_find(map, 0) == 0 && markdown_source_map_find(map, 1) == 1 &&
                markdown_source_map_find(map, 2) == markdown_source_none,
            "ordinals find their entries");
    markdown_source_map_destroy(map);
    const char open[] = "{\\rtf1 a\\par b}";
    map = counted(open, (const size_t[]){7}, 1);
    expect_marks(map, (const size_t[]){1}, 1, 1);
    markdown_source_map_destroy(map);
    const char words[] = "{\\rtf1\\pard x\\\\par \\cellx100 \\u12354? y\\line z\\sa0\\par}";
    map = counted(words, (const size_t[]){0}, 1);
    expect_marks(map, (const size_t[]){1}, 1, 0);
    markdown_source_map_destroy(map);
    const char row[] = "{\\rtf1 p\\par\\trowd\\cellx1\\cellx2 a\\cell b\\cell\\row q}";
    map = counted(row, (const size_t[]){0, 12}, 2);
    expect_marks(map, (const size_t[]){1, 4}, 2, 5);
    markdown_source_map_destroy(map);
    const char tail[] = "{\\rtf1 p\\par\\trowd\\cellx1 a\\cell\\row\n}";
    map = counted(tail, (const size_t[]){0, 12}, 2);
    expect_marks(map, (const size_t[]){1, 3}, 2, 4);
    markdown_source_map_destroy(map);
    const char before[] = "{\\rtf1 lead\\par x\\par}";
    map = counted(before, (const size_t[]){16}, 1);
    expect_marks(map, (const size_t[]){1}, 1, 1);
    require(markdown_source_map_find(map, 0) == markdown_source_none &&
                markdown_source_map_find(map, 1) == 0 && markdown_source_map_at(map, 0).first == 1,
            "a mark before the first entry belongs to none and shifts the ordinals");
    markdown_source_map_destroy(map);
}

static struct markdown_rtf *_Nonnull rendered(const char *_Nonnull source)
{
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "map source");
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT),
                                (struct markdown_rtf_locale){"Noto Sans JP", FOLIO_LANGUAGE_JA},
                                &rtf) == MARKDOWN_RTF_CONVERTED,
            "map rendering");
    note_text_destroy(text);
    return rtf;
}

struct expected_block
{
    enum markdown_source_kind kind;
    const char *_Nonnull lines;
    const char *_Nonnull caret;
    size_t marks;
};

static void expect_block(const char *_Nonnull source, struct markdown_source_entry entry,
                         struct expected_block expected, size_t index)
{
    size_t length = strlen(expected.lines);
    bool lines = entry.end - entry.begin == length &&
                 memcmp(source + entry.begin, expected.lines, length) == 0;
    bool caret = strncmp(source + entry.caret, expected.caret, strlen(expected.caret)) == 0;
    if (entry.kind != expected.kind || !lines || !caret || entry.marks != expected.marks)
        fprintf(stderr, "block %zu: kind %u lines [%.*s] caret [%.12s] marks %zu\n", index,
                (unsigned)entry.kind, (int)(entry.end - entry.begin), source + entry.begin,
                source + entry.caret, entry.marks);
    require(entry.kind == expected.kind && lines && caret && entry.marks == expected.marks,
            "a block's kind, lines, caret and marks");
}

static const char every_block[] =
    "---\ntags: [map]\n---\n"
    "# 見出し\n\n段落の1行目\n2行目\n\n- 項目\n\n> 引用\n\n```c\nint a;\nint b;\n```\n\n"
    "```mermaid\ngraph TB\nA --> B\n```\n\n---\n\n| a | b |\n| - | - |\n| 1 | 2 |\n\n"
    "用語\n: 説明\n\n本文[^n]\n\n[r]: https://example.com\n\n[^n]: 脚注\n";

static void every_kind(void)
{
    struct markdown_rtf *rtf = rendered(every_block);
    const struct markdown_source_map *map = markdown_rtf_source_map(rtf);
    require(map != nullptr, "the reading view has a source map");
    const struct expected_block blocks[] = {
        {MARKDOWN_SOURCE_HEADING, "# 見出し", "見出し", 1},
        {MARKDOWN_SOURCE_PARAGRAPH, "段落の1行目\n2行目", "段落", 1},
        {MARKDOWN_SOURCE_PARAGRAPH, "- 項目", "項目", 1},
        {MARKDOWN_SOURCE_PARAGRAPH, "> 引用", "引用", 1},
        {MARKDOWN_SOURCE_CODE_HEADER, "```c", "```c", 1},
        {MARKDOWN_SOURCE_CODE_LINE, "int a;", "int a;", 1},
        {MARKDOWN_SOURCE_CODE_LINE, "int b;", "int b;", 1},
        {MARKDOWN_SOURCE_CODE_END, "```", "```", 0},
        {MARKDOWN_SOURCE_DIAGRAM, "```mermaid", "```mermaid", 1},
        {MARKDOWN_SOURCE_CODE_END, "```", "```", 0},
        {MARKDOWN_SOURCE_RULE, "---", "---", 3},
        {MARKDOWN_SOURCE_TABLE_ROW, "| a | b |\n| - | - |", "| a", 4},
        {MARKDOWN_SOURCE_TABLE_ROW, "| 1 | 2 |", "| 1", 4},
        {MARKDOWN_SOURCE_DEFINITION, "用語", "用語", 1},
        {MARKDOWN_SOURCE_DEFINITION, ": 説明", "説明", 3},
        {MARKDOWN_SOURCE_PARAGRAPH, "本文[^n]", "本文", 1},
        {MARKDOWN_SOURCE_SEPARATOR, "[^n]: 脚注", "脚注", 3},
        {MARKDOWN_SOURCE_FOOTNOTE, "[^n]: 脚注", "脚注", 1},
    };
    size_t count = markdown_source_map_count(map);
    if (count != sizeof blocks / sizeof blocks[0])
    {
        fprintf(stderr, "source map: %zu entries\n", count);
        for (size_t index = 0; index < count; ++index)
        {
            struct markdown_source_entry entry = markdown_source_map_at(map, index);
            fprintf(stderr, "  %zu: kind %u [%.*s] marks %zu\n", index, (unsigned)entry.kind,
                    (int)(entry.end - entry.begin), every_block + entry.begin, entry.marks);
        }
    }
    require(count == sizeof blocks / sizeof blocks[0], "one entry per block");
    size_t sum = 0;
    for (size_t index = 0; index < count; ++index)
    {
        struct markdown_source_entry entry = markdown_source_map_at(map, index);
        expect_block(every_block, entry, blocks[index], index);
        require(entry.first == sum, "ordinals follow each other");
        sum += entry.marks;
    }
    require(markdown_source_map_total(map) == sum - 1, "the final \\par is RichEdit's own");
    struct markdown_source_entry fence = markdown_source_map_at(map, 4);
    struct markdown_source_entry closing = markdown_source_map_at(map, 7);
    struct markdown_source_entry row = markdown_source_map_at(map, 12);
    require(fence.group != 0 && fence.group == closing.group &&
                row.group == markdown_source_map_at(map, 11).group && row.group != fence.group &&
                markdown_source_map_at(map, 1).group == 0 &&
                markdown_source_map_at(map, 8).group != 0 &&
                markdown_source_map_at(map, 8).group == markdown_source_map_at(map, 9).group &&
                markdown_source_map_at(map, 8).group != fence.group,
            "a fence, a diagram and a table are groups; a paragraph is in none");
    require(markdown_source_map_find(map, markdown_source_map_at(map, 5).first) == 5 &&
                markdown_source_map_find(map, markdown_source_map_at(map, 11).first + 3) == 11,
            "a render's ordinals find their blocks");
    markdown_rtf_destroy(rtf);
}

/* One document per review finding (#1008): each block's kind, lines, caret and marks. */
static void expect_blocks(const char *_Nonnull source, const struct expected_block *_Nonnull blocks,
                          size_t count)
{
    struct markdown_rtf *rtf = rendered(source);
    const struct markdown_source_map *map = markdown_rtf_source_map(rtf);
    if (markdown_source_map_count(map) != count)
        fprintf(stderr, "%zu entries for: %s\n", markdown_source_map_count(map), source);
    require(markdown_source_map_count(map) == count, "one entry per block");
    for (size_t index = 0; index < count; ++index)
        expect_block(source, markdown_source_map_at(map, index), blocks[index], index);
    markdown_rtf_destroy(rtf);
}

static void review_cases(void)
{
    const struct expected_block setext[] = {{MARKDOWN_SOURCE_HEADING, "Title\n=====", "Title", 1},
                                            {MARKDOWN_SOURCE_PARAGRAPH, "x", "x", 1}};
    expect_blocks("Title\n=====\n\nx\n", setext, 2);
    const struct expected_block empty_item[] = {{MARKDOWN_SOURCE_PARAGRAPH, "a", "a", 1},
                                                {MARKDOWN_SOURCE_PARAGRAPH, "-", "", 1},
                                                {MARKDOWN_SOURCE_PARAGRAPH, "b", "b", 1}};
    expect_blocks("a\n\n-\n\nb\n", empty_item, 3);
    const struct expected_block table_after[] = {
        {MARKDOWN_SOURCE_DEFINITION, "用語", "用語", 1},
        {MARKDOWN_SOURCE_DEFINITION, ": 説明", "説明", 3},
        {MARKDOWN_SOURCE_TABLE_ROW, "| x |\n| - |", "| x", 4}};
    expect_blocks("用語\n: 説明\n| x |\n| - |\n", table_after, 3);
    const struct expected_block lone_cr[] = {{MARKDOWN_SOURCE_PARAGRAPH, "a\rb", "a", 1},
                                             {MARKDOWN_SOURCE_PARAGRAPH, "1. c", "c", 1}};
    expect_blocks("a\rb\r\r1. c\r", lone_cr, 2);
}

/* CRLF lines end before their CR; a paragraph inside a list keeps the marker's line. */
static void line_endings(void)
{
    static const char source[] = "a\r\nb\r\n\r\n1. c\r\n";
    struct markdown_rtf *rtf = rendered(source);
    const struct markdown_source_map *map = markdown_rtf_source_map(rtf);
    require(markdown_source_map_count(map) == 2, "two blocks");
    struct markdown_source_entry first = markdown_source_map_at(map, 0);
    struct markdown_source_entry item = markdown_source_map_at(map, 1);
    require(first.begin == 0 && first.end == 4 && item.begin == 8 && item.end == 12 &&
                source[item.caret] == 'c',
            "lines and carets with CRLF");
    markdown_rtf_destroy(rtf);
}

static void allocation_of(const char *_Nonnull source)
{
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED,
            "allocation source");
    size_t calls = 0;
    for (size_t nth = 0; nth <= calls; ++nth)
    {
        allocation_probe_fail_at(nth);
        struct markdown_rtf *rtf = nullptr;
        enum markdown_rtf_outcome outcome = markdown_rtf_create(
            text, folio_theme_palette(FOLIO_THEME_LIGHT),
            (struct markdown_rtf_locale){"Noto Sans JP", FOLIO_LANGUAGE_JA}, &rtf);
        if (nth == 0)
            calls = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? outcome == MARKDOWN_RTF_CONVERTED
                         : outcome == MARKDOWN_RTF_OUT_OF_MEMORY,
                "every allocation failure while mapping is typed");
        markdown_rtf_destroy(rtf);
    }
    note_text_destroy(text);
}

/* Every block's kind, then an indented code block (#1192), which no fence opens. */
static void allocation(void)
{
    allocation_of(every_block);
    allocation_of("text\n\n    a\n\n    b\n");
}

/* #1012: the edit caret for a paragraph ordinal, and the Markdown a selection copies. */
struct lookup_case
{
    struct markdown_rtf *_Nonnull rtf;
    struct note_text *_Nonnull text;
    const struct markdown_source_map *_Nonnull map;
};

static struct lookup_case lookup_for(const char *_Nonnull source)
{
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "lookup source");
    struct markdown_rtf *rtf = rendered(source);
    return (struct lookup_case){rtf, text, markdown_rtf_source_map(rtf)};
}

static void lookup_done(struct lookup_case lookup)
{
    markdown_rtf_destroy(lookup.rtf);
    note_text_destroy(lookup.text);
}

static void expect_caret(struct lookup_case lookup, size_t ordinal, const char *_Nonnull starts)
{
    size_t caret = markdown_source_lookup_caret(lookup.map, lookup.text, ordinal);
    const char *bytes = note_text_bytes(lookup.text);
    bool same =
        caret != markdown_source_none && strncmp(bytes + caret, starts, strlen(starts)) == 0;
    if (!same)
        fprintf(stderr, "caret for ordinal %zu: [%.10s], expected [%s]\n", ordinal,
                caret == markdown_source_none ? "(none)" : bytes + caret, starts);
    require(same, "the edit caret is where the paragraph's text starts");
}

static void expect_copy(struct lookup_case lookup, size_t first, size_t last,
                        const char *_Nonnull expected)
{
    struct text_buffer *buffer = nullptr;
    require(text_buffer_create(&buffer) == TEXT_BUFFER_ACCEPTED, "copy buffer");
    bool copied = markdown_source_lookup_copy(
        lookup.map, lookup.text, (struct markdown_source_selection){first, last}, buffer);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED, "copy finished");
    bool same = copied && strcmp(text_buffer_bytes(buffer), expected) == 0;
    if (!same)
        fprintf(stderr, "copy %zu..%zu: [%s], expected [%s]\n", first, last,
                text_buffer_bytes(buffer), expected);
    require(same, "a selection copies its blocks' Markdown");
    text_buffer_destroy(buffer);
}

static size_t first_of(struct lookup_case lookup, size_t index)
{
    return markdown_source_map_at(lookup.map, index).first;
}

static void lookup_caret(void)
{
    struct lookup_case lookup = lookup_for(every_block);
    expect_caret(lookup, first_of(lookup, 1), "段落");
    expect_caret(lookup, first_of(lookup, 5), "int a;");
    expect_caret(lookup, first_of(lookup, 11) + 1, "a |");
    expect_caret(lookup, first_of(lookup, 11) + 2, "b |");
    expect_caret(lookup, first_of(lookup, 12) + 1, "1 |");
    expect_caret(lookup, first_of(lookup, 12) + 2, "2 |");
    expect_caret(lookup, first_of(lookup, 12) + 3, "2 |");
    expect_caret(lookup, first_of(lookup, 17), "脚注");
    lookup_done(lookup);
    lookup = lookup_for("用語\n: 説明\n| x | y |\n| - | - |\n");
    expect_caret(lookup, first_of(lookup, 2) + 2, "x |");
    expect_caret(lookup, first_of(lookup, 2) + 3, "y |");
    lookup_done(lookup);
    lookup = lookup_for("x | y\n- | -\n1 | 2\n");
    expect_caret(lookup, first_of(lookup, 1) + 2, "2");
    lookup_done(lookup);
    /* #1012 review: a body row shorter than the delimiter is padded, not led by a spacer. */
    lookup = lookup_for("| a | b | c |\n|-|-|-|\n| x | y |\n");
    expect_caret(lookup, first_of(lookup, 0) + 3, "c |");
    expect_caret(lookup, first_of(lookup, 1) + 1, "x |");
    expect_caret(lookup, first_of(lookup, 1) + 2, "y |");
    expect_caret(lookup, first_of(lookup, 1) + 3, "y |");
    lookup_done(lookup);
    /* A table in a list item, right after a description: both lead the header row. */
    lookup = lookup_for("用語\n: 説明\n\n- | p | q |\n  | - | - |\n");
    size_t rows = markdown_source_map_count(lookup.map);
    struct markdown_source_entry row = markdown_source_map_at(lookup.map, rows - 1);
    require(row.kind == MARKDOWN_SOURCE_TABLE_ROW, "the list item's table is native");
    expect_caret(lookup, row.first + row.marks - 2, "q |");
    expect_caret(lookup, row.first + row.marks - 3, "p |");
    lookup_done(lookup);
}

/* A selection that copies nothing: the answer is false, whatever the buffer holds. */
static void expect_no_copy(struct lookup_case lookup, const struct note_text *_Nonnull text,
                           size_t first, size_t last)
{
    struct text_buffer *buffer = nullptr;
    require(text_buffer_create(&buffer) == TEXT_BUFFER_ACCEPTED, "copy buffer");
    require(!markdown_source_lookup_copy(lookup.map, text,
                                         (struct markdown_source_selection){first, last}, buffer),
            "a selection no block holds copies nothing");
    text_buffer_destroy(buffer);
}

/* The edges of the lookups: escaped pipes, old Mac line ends, selections that hold nothing, and a
 * map asked about a text shorter than the one it was made from. */
static void lookup_edges(void)
{
    struct lookup_case lookup = lookup_for("| a\\|b | c |\n|-|-|\n| 1\\|2 | 3 |\n");
    expect_caret(lookup, first_of(lookup, 0) + 1, "a\\|b |");
    expect_caret(lookup, first_of(lookup, 0) + 2, "c |"); /* the escaped pipe ends no cell */
    expect_caret(lookup, first_of(lookup, 1) + 2, "3 |");
    size_t past = first_of(lookup, 1) + 100;
    require(markdown_source_lookup_caret(lookup.map, lookup.text, past) == markdown_source_none,
            "an ordinal past the last paragraph has no caret");
    expect_no_copy(lookup, lookup.text, past, past);
    expect_no_copy(lookup, lookup.text, first_of(lookup, 1), first_of(lookup, 0));
    /* The note was cut short after the map was made: nothing is read past its end. */
    struct note_text *cut = nullptr;
    require(note_text_create("| a", 3, &cut) == NOTE_TEXT_ACCEPTED, "shorter text");
    require(markdown_source_lookup_caret(lookup.map, cut, first_of(lookup, 0) + 1) ==
                markdown_source_none,
            "a block that ends past the text has no caret");
    expect_no_copy(lookup, cut, first_of(lookup, 0), first_of(lookup, 1));
    note_text_destroy(cut);
    lookup_done(lookup);
    lookup = lookup_for("a\rb\r\rc\r");
    expect_copy(lookup, 0, 1, "a\nb\n\nc"); /* lone CR line ends are written as LF too */
    lookup_done(lookup);
    lookup = lookup_for("x[^a] y[^b]\n\n[^a]: one\n[^b]: two\n");
    size_t count = markdown_source_map_count(lookup.map);
    size_t last = markdown_source_map_at(lookup.map, count - 1).first;
    expect_copy(lookup, 0, last, "x[^a] y[^b]\n\n[^a]: one\n\n[^b]: two");
    cut = nullptr;
    require(note_text_create("x[^a] y[^b]\n", 12, &cut) == NOTE_TEXT_ACCEPTED, "body only");
    expect_no_copy(lookup, cut, 0, last);    /* the footnotes' lines are past the shorter text */
    expect_no_copy(lookup, cut, last, last); /* ...also when a footnote alone is selected */
    note_text_destroy(cut);
    lookup_done(lookup);
}

static void lookup_copy(void)
{
    struct lookup_case lookup = lookup_for("a[^n]\n\n[^n]: x\n\nb\n");
    size_t count = markdown_source_map_count(lookup.map);
    expect_copy(lookup, 0, markdown_source_map_at(lookup.map, count - 1).first,
                "a[^n]\n\n[^n]: x\n\nb"); /* #1012 review: a definition among the body once */
    lookup_done(lookup);
    lookup = lookup_for(every_block);
    expect_copy(lookup, first_of(lookup, 6), first_of(lookup, 6), "```c\nint a;\nint b;\n```");
    expect_copy(lookup, first_of(lookup, 1), first_of(lookup, 2), "段落の1行目\n2行目\n\n- 項目");
    expect_copy(lookup, first_of(lookup, 12) + 1, first_of(lookup, 12) + 1,
                "| a | b |\n| - | - |\n| 1 | 2 |");
    expect_copy(lookup, first_of(lookup, 15), first_of(lookup, 17), "本文[^n]\n\n[^n]: 脚注");
    expect_copy(lookup, first_of(lookup, 17), first_of(lookup, 17), "[^n]: 脚注");
    lookup_done(lookup);
    lookup = lookup_for("a\r\nb\r\n\r\nc\r\n");
    expect_copy(lookup, 0, 1, "a\nb\n\nc");
    lookup_done(lookup);
}

/* #1071: from source bytes to the view's paragraphs. */
static void expect_paragraphs(struct lookup_case lookup, const char *_Nonnull from,
                              const char *_Nonnull to, struct markdown_source_selection entries)
{
    const char *bytes = note_text_bytes(lookup.text);
    const char *start = strstr(bytes, from);
    const char *end = strstr(bytes, to);
    require(start != nullptr && end != nullptr, "the test's source holds both ends");
    struct note_byte_span span = {(size_t)(start - bytes), (size_t)(end - bytes) + strlen(to)};
    struct markdown_source_selection found = {99, 99};
    bool mapped = markdown_source_lookup_paragraphs(lookup.map, span, &found);
    struct markdown_source_entry first = markdown_source_map_at(lookup.map, entries.first);
    struct markdown_source_entry last = markdown_source_map_at(lookup.map, entries.last);
    bool same = mapped && found.first == first.first && found.last == last.first + last.marks;
    if (!same)
        fprintf(stderr, "paragraphs for [%s..%s]: %zu..%zu, expected %zu..%zu\n", from, to,
                found.first, found.last, first.first, last.first + last.marks);
    require(same, "source bytes find the view's paragraphs");
}

static void lookup_paragraphs(void)
{
    struct lookup_case lookup =
        lookup_for("# 題\n\n一つ目の段落\n\n二つ目の段落\n続きの行\n\n- 項目A\n- 項目B\n");
    expect_paragraphs(lookup, "一つ目", "一つ目", (struct markdown_source_selection){1, 1});
    expect_paragraphs(lookup, "続き", "続き", (struct markdown_source_selection){2, 2});
    expect_paragraphs(lookup, "一つ目", "続きの行", (struct markdown_source_selection){1, 2});
    expect_paragraphs(lookup, "# 題", "項目B", (struct markdown_source_selection){0, 4});
    const char *bytes = note_text_bytes(lookup.text);
    struct markdown_source_selection found = {99, 99};
    struct note_byte_span blank = {(size_t)(strstr(bytes, "\n\n一つ目") - bytes) + 1,
                                   (size_t)(strstr(bytes, "\n\n一つ目") - bytes) + 1};
    require(markdown_source_lookup_paragraphs(lookup.map, blank, &found) &&
                found.first == markdown_source_map_at(lookup.map, 1).first,
            "a caret on a blank line takes the next block");
    struct note_byte_span caret = {(size_t)(strstr(bytes, "項目B") - bytes) + 3,
                                   (size_t)(strstr(bytes, "項目B") - bytes) + 3};
    require(markdown_source_lookup_paragraphs(lookup.map, caret, &found) &&
                found.first == markdown_source_map_at(lookup.map, 4).first &&
                found.last == found.first + markdown_source_map_at(lookup.map, 4).marks,
            "a caret inside a block takes that block");
    struct note_byte_span past = {note_text_length(lookup.text) + 5,
                                  note_text_length(lookup.text) + 9};
    found = (struct markdown_source_selection){7, 7};
    require(!markdown_source_lookup_paragraphs(lookup.map, past, &found) && found.first == 7,
            "bytes past every block find nothing and leave out alone");
    lookup_done(lookup);
    lookup = lookup_for("前\n\n```c\nint a;\nint b;\n```\n\n後[^1]\n\n[^1]: 脚注の文\n");
    expect_paragraphs(lookup, "int b;", "int b;", (struct markdown_source_selection){3, 3});
    expect_paragraphs(lookup, "```c", "int a;", (struct markdown_source_selection){1, 2});
    expect_paragraphs(
        lookup, "脚注の文", "脚注の文",
        (struct markdown_source_selection){markdown_source_map_count(lookup.map) - 1,
                                           markdown_source_map_count(lookup.map) - 1});
    lookup_done(lookup);
}

/* #1192: the lines of an indented code block are entries of one group, like a fence's lines
 * without its opening and closing lines. The edit caret is where a line's code starts (after
 * the four columns), and a copy takes the block's lines whole. */
static void indented_code(void)
{
    static const char source[] = "text\n\n    a\n\n      b\nafter\n";
    const struct expected_block blocks[] = {{MARKDOWN_SOURCE_PARAGRAPH, "text", "text", 1},
                                            {MARKDOWN_SOURCE_CODE_LINE, "    a", "a\n", 1},
                                            {MARKDOWN_SOURCE_CODE_LINE, "", "\n      b", 1},
                                            {MARKDOWN_SOURCE_CODE_LINE, "      b", "  b\n", 1},
                                            {MARKDOWN_SOURCE_PARAGRAPH, "after", "after", 1}};
    expect_blocks(source, blocks, 5);
    struct lookup_case lookup = lookup_for(source);
    size_t group = markdown_source_map_at(lookup.map, 1).group;
    require(group != 0 && markdown_source_map_at(lookup.map, 2).group == group &&
                markdown_source_map_at(lookup.map, 3).group == group &&
                markdown_source_map_at(lookup.map, 0).group == 0 &&
                markdown_source_map_at(lookup.map, 4).group == 0,
            "the indented code's lines are one group");
    expect_caret(lookup, first_of(lookup, 1), "a\n");
    expect_caret(lookup, first_of(lookup, 3), "  b\n");
    expect_copy(lookup, first_of(lookup, 2), first_of(lookup, 2), "    a\n\n      b");
    expect_copy(lookup, first_of(lookup, 0), first_of(lookup, 1), "text\n\n    a\n\n      b");
    expect_paragraphs(lookup, "    a", "a", (struct markdown_source_selection){1, 1});
    expect_paragraphs(lookup, "      b", "b", (struct markdown_source_selection){3, 3});
    lookup_done(lookup);
    /* In a list item whose marker waits: the marker's own line and the code line both come
     * from the item's line. Two blocks of indented code are two groups. */
    static const char item[] = "-     code\n\n  text\n\n      more\n";
    const struct expected_block item_blocks[] = {
        {MARKDOWN_SOURCE_CODE_LINE, "-     code", "code", 1},
        {MARKDOWN_SOURCE_CODE_LINE, "-     code", "code", 1},
        {MARKDOWN_SOURCE_PARAGRAPH, "  text", "text", 1},
        {MARKDOWN_SOURCE_CODE_LINE, "      more", "more", 1}};
    expect_blocks(item, item_blocks, 4);
    lookup = lookup_for(item);
    require(markdown_source_map_at(lookup.map, 0).group ==
                    markdown_source_map_at(lookup.map, 1).group &&
                markdown_source_map_at(lookup.map, 3).group !=
                    markdown_source_map_at(lookup.map, 1).group &&
                markdown_source_map_at(lookup.map, 3).group != 0,
            "each indented code block is a group of its own");
    expect_caret(lookup, first_of(lookup, 0), "code");
    expect_copy(lookup, first_of(lookup, 3), first_of(lookup, 3), "      more");
    lookup_done(lookup);
}

void run_source_map_tests(void)
{
    indented_code();
    counting();
    every_kind();
    line_endings();
    review_cases();
    lookup_caret();
    lookup_copy();
    lookup_edges();
    lookup_paragraphs();
    allocation();
    printf("source map tests passed\n");
}
