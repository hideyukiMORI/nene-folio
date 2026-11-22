#include "allocation_probe.h"
#include "image_source.h"
#include "index_filter.h"
#include "index_filter_build.h"
#include "markdown_images.h"
#include "markdown_rtf.h"
#include "note_metadata.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "replace_edit.h"
#include "tag_catalog.h"
#include "unit_tests.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct note_tag *_Nonnull tag(const char *_Nonnull name)
{
    struct note_tag *value = nullptr;
    require(note_tag_create(name, strlen(name), &value) == NOTE_TAG_READY, "tag fixture is valid");
    return value;
}

static struct note_text *_Nonnull text(const char *_Nonnull source)
{
    struct note_text *value = nullptr;
    require(note_text_create(source, strlen(source), &value) == NOTE_TEXT_ACCEPTED,
            "note fixture is valid");
    return value;
}

static struct note_tags *_Nonnull tags(const char *_Nonnull const *_Nonnull names, size_t count)
{
    struct note_tag *owned[note_tags_max_count];
    const struct note_tag *borrowed[note_tags_max_count];
    for (size_t index = 0; index < count; ++index)
    {
        owned[index] = tag(names[index]);
        borrowed[index] = owned[index];
    }
    struct note_tags *value = nullptr;
    require(note_tags_create(borrowed, count, &value) == NOTE_TAGS_READY,
            "tag set fixture is valid");
    for (size_t index = 0; index < count; ++index)
        note_tag_destroy(owned[index]);
    return value;
}

static void names(void)
{
    struct note_tag *value = tag(" \t設計 メモ\t ");
    require(same_text(note_tag_bytes(value), "設計 メモ"),
            "only surrounding ASCII blanks are trimmed");
    static const char *const invalid[] = {"x\tx",         "x\nx",         "\x7f",
                                          "\xc2\x85",     "\xe2\x80\xa8", "\xe2\x80\xa9",
                                          "\xef\xbb\xbf", "\xc0\x80"};
    for (size_t index = 0; index < sizeof invalid / sizeof invalid[0]; ++index)
    {
        struct note_tag *out = value;
        require(note_tag_create(invalid[index], strlen(invalid[index]), &out) == NOTE_TAG_INVALID &&
                    out == value,
                "invalid names preserve an existing output");
    }
    struct note_tag *out = value;
    require(note_tag_create(" \t", 2, &out) == NOTE_TAG_EMPTY && out == value,
            "empty normalized name is refused");
    const char embedded[] = {'a', 0, 'b'};
    require(note_tag_create(embedded, sizeof embedded, &out) == NOTE_TAG_INVALID && out == value,
            "embedded NUL is refused");
    char long_name[257];
    memset(long_name, 'a', sizeof long_name);
    require(note_tag_create(long_name, 64, &out) == NOTE_TAG_READY, "64 scalar name accepted");
    note_tag_destroy(out);
    out = value;
    require(note_tag_create(long_name, 65, &out) == NOTE_TAG_TOO_LONG && out == value,
            "65 scalar name refused");
    require(note_tag_create(long_name, sizeof long_name, &out) == NOTE_TAG_TOO_LONG && out == value,
            "257 byte name refused");
    note_tag_destroy(value);
}

static void sets(void)
{
    const char *const source[] = {"Work", "work", "é", "é", "Work"};
    struct note_tags *original = tags(source, 5);
    require(note_tags_count(original) == 4,
            "case and normalization forms remain distinct; exact duplicate collapses");
    struct note_tag *removed = tag("work");
    struct note_tags *changed = nullptr;
    require(note_tags_with(original, removed, NOTE_TAG_ABSENT, &changed) == NOTE_TAGS_READY &&
                note_tags_count(changed) == 3 && note_tags_count(original) == 4,
            "removing produces a distinct immutable set");
    require(!note_tags_equal(original, changed), "different membership is unequal");
    struct note_tags *restored = nullptr;
    require(note_tags_with(changed, removed, NOTE_TAG_PRESENT, &restored) == NOTE_TAGS_READY &&
                note_tags_equal(original, restored),
            "equality ignores insertion order");
    note_tag_destroy(removed);
    note_tags_destroy(changed);
    note_tags_destroy(restored);
    note_tags_destroy(original);
}

static void read_status(const char *_Nonnull source, enum note_metadata_status expected)
{
    struct note_text *body = text(source);
    const struct note_metadata *metadata = note_text_metadata(body);
    require(note_metadata_status(metadata) == expected, source);
    if (expected != NOTE_METADATA_SUPPORTED)
        require(note_metadata_body(metadata) == 0 &&
                    note_tags_count(note_metadata_tags(metadata)) == 0,
                "unavailable metadata exposes neither hidden body nor partial tags");
    require(same_text(note_text_bytes(body), source), "parsing never rewrites source");
    note_text_destroy(body);
}

static void recognition(void)
{
    static const char *const ordinary[] = {"",
                                           "---",
                                           "---\n",
                                           "---\n---\n",
                                           "---\nordinary\n---\n",
                                           "heading\n---\n",
                                           "\n---\ntags: [a]\n---\n",
                                           "---\n- list\n---\n",
                                           "---\n    code: sample\n---\n",
                                           "---\n> quote: sample\n---\n"};
    for (size_t index = 0; index < sizeof ordinary / sizeof ordinary[0]; ++index)
        read_status(ordinary[index], NOTE_METADATA_NONE);
    read_status("---\n\"tags\": [a]\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\n\"tags\":[a]\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\n日本語: value\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\n{tags: [a]}\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\ntags: [a]\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: [a]\ntags: [b]\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntitle: x\ntitle: y\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\nauthor:\n  name: person\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\ntags: x\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\ntags: &x [a]\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\ntags:\n\t- a\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\ntags:\n  - a\n   - b\n---\n", NOTE_METADATA_UNSUPPORTED);
    read_status("---\ntags: [\"a\" junk]\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: [\"a\"]#bad\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: ['unclosed]\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: [\"\\uD800\"]\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: [\"\\U00110000\"]\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: [\"\\q\"]\n---\n", NOTE_METADATA_MALFORMED);
    read_status("---\ntags: [\"\\n\"]\n---\n", NOTE_METADATA_INVALID_TAG);
    read_status("---\ntags: [a, \"\"]\n---\n", NOTE_METADATA_INVALID_TAG);
    read_status("---\ntitle: |\n  ---\n  body\ntags: [a]\n---\nvisible", NOTE_METADATA_SUPPORTED);
    read_status("---\ntitle: >-\n  body\n    more\ntags: []\n---", NOTE_METADATA_SUPPORTED);
    read_status("---\ntitle: |2+\n  body\n #short\ntags: []\n---", NOTE_METADATA_SUPPORTED);
    read_status("---\nsummary: |\n    # scalar text\n  less\ntags: [x]\n---\n",
                NOTE_METADATA_MALFORMED);
    read_status("---\nsummary: |\n    # scalar text\n    same\ntags: [x]\n---\n",
                NOTE_METADATA_SUPPORTED);
    read_status("---\nsummary: |2\n  text\n # outside\n  resumed\n---\n",
                NOTE_METADATA_UNSUPPORTED);
}

/* More shapes of a header, each as YAML reads it: block scalar indicators, comments after a
 * value, tabs as separation, the three line endings, directives and tags, and what may
 * follow a closed value. */
static void metadata_shapes(void)
{
    static const char *const supported[] = {
        "---\ntitle: |2-\n  x\ntags: []\n---",
        "---\ntitle: | # comment\n  x\ntags: []\n---",
        "---\ntags: [a]\t# tab comment\n---",
        "---\ntags:\t[a]\n---",
        "---\ntitle: x #c\ntags: []\n---",
        "---\ntitle: x#c\ntags: []\n---",
        "---\r\ntitle: x\r\ntags: [a]\r\n---\r\nbody",
        "---\rtags: [a]\r---\rbody",
        "---\ntags: [a b]\n---",
        "---\nkey:\n---",
        "---\ntags:\n---",
        "---\ntags: # none\n---",
        "---\ntags:\n  - a # c\n  - b\n---",
        "---\ntags: [a , b]\n---",
        "---\ntags: [ ]\n---",
        "---\nkey : v\n---",
        "---\nurl: http://x\n---",
        "---\ntitle: \"a\" # c\ntags: []\n---",
    };
    static const char *const unsupported[] = {
        "---\n%YAML 1.2\n---", "---\n!tag x\n---",  "---\n&a b\n---",
        "---\n-x: y\n---",     "---\n1key: v\n---", "---\ntags: [[a]]\n---",
    };
    static const char *const malformed[] = {
        "---\ntitle: |22\n  x\n---", "---\ntitle: |-+\n  x\n---", "---\ntitle: |x\n  x\n---",
        "---\ntags: [a]]\n---",      "---\ntags: [a] x\n---",     "---\ntitle: \"a\" b\n---",
    };
    for (size_t index = 0; index < sizeof supported / sizeof supported[0]; ++index)
        read_status(supported[index], NOTE_METADATA_SUPPORTED);
    for (size_t index = 0; index < sizeof unsupported / sizeof unsupported[0]; ++index)
        read_status(unsupported[index], NOTE_METADATA_UNSUPPORTED);
    for (size_t index = 0; index < sizeof malformed / sizeof malformed[0]; ++index)
        read_status(malformed[index], NOTE_METADATA_MALFORMED);
}

static void scalar_types(void)
{
    static const char *const numeric[] = {"null", "Null", "NULL",   "~",     "true", "False",
                                          "TRUE", "0",    "-12",    "+01",   "0o77", "0xFf",
                                          ".5",   "0.",   "-2E+05", "12e03", ".NaN", "+.INF"};
    for (size_t index = 0; index < sizeof numeric / sizeof numeric[0]; ++index)
    {
        char source[80];
        int length = snprintf(source, sizeof source, "---\ntags: [%s]\n---\n", numeric[index]);
        require(length > 0 && (size_t)length < sizeof source, "numeric fixture fits");
        read_status(source, NOTE_METADATA_INVALID_TAG);
    }
    const char *source = "---\ntags: [\"null\", '42', yes, nUlL, 0xZZ, .infinity, 2026-11-15, "
                         "-name, 'O''Reilly', \"A\\x42\\u65E5\\U0001F600\"] # keep\n---\nbody";
    struct note_text *body = text(source);
    const struct note_metadata *metadata = note_text_metadata(body);
    const struct note_tags *parsed = note_metadata_tags(metadata);
    require(note_metadata_status(metadata) == NOTE_METADATA_SUPPORTED &&
                note_tags_count(parsed) == 10,
            "quoted scalars and YAML core strings share tag identity");
    require(same_text(note_tag_bytes(note_tags_at(parsed, 8)), "O'Reilly") &&
                same_text(note_tag_bytes(note_tags_at(parsed, 9)), "AB日😀"),
            "single quotes and fixed-width Unicode escapes decode once");
    require(same_text(note_text_bytes(body) + note_metadata_body(metadata), "body"),
            "body offset remains in original source");
    note_text_destroy(body);
}

static void metadata_edges(void)
{
    static const char *const malformed[] = {
        "---\ntitle: ok\ntags:[a]\n---", "---\ntags: [a\n---",
        "---\ntags: [\"a\"\n---",        "---\ntags: [a: b]\n---",
        "---\ntags: [a:]\n---",          "---\ntags: [x{y]\n---",
        "---\ntags: [-, x]\n---",        "---\ntags: [\"\\xG0\"]\n---",
        "---\ntags: [\"\\u12\"]\n---",   "---\ntags: [\"end\\\n---",
        "---\ntitle: |++\n  x\n---",     "---\ntitle: |0\n  x\n---",
        "---\ntitle: |2\n x\n---",       "---\ntags: []\n# \x01\n---",
        "---\ntags:\n  - \n---",         "---\ntags:\n  - 'x' junk\n---"};
    for (size_t index = 0; index < sizeof malformed / sizeof malformed[0]; ++index)
        read_status(malformed[index], NOTE_METADATA_MALFORMED);
    static const char *const unsupported[] = {"---\ntitle: &alias value\n---",
                                              "---\ntags: [*alias]\n---",
                                              "---\ntitle: x\n  continuation\n---",
                                              "---\ntags:\n  -x\n---",
                                              "---\ntags:\n  -\n---",
                                              "---\ntitle: x\nnot-a-key\n---",
                                              "---\ntitle: x\nkey? value\n---",
                                              "---\n tags: [a]\n---"};
    for (size_t index = 0; index < sizeof unsupported / sizeof unsupported[0]; ++index)
        read_status(unsupported[index], NOTE_METADATA_UNSUPPORTED);
    read_status("---\n\n# before\n_A-Z9: ['x', 42, true,] # after\nother:\n  - null\n  - 'x'\n"
                "tags: [a:b, url#part, 0o89, 0b11, 1e, 1e+, 1e2x, 'slash\\value']\n---",
                NOTE_METADATA_SUPPORTED);
    read_status("---\n'scalar without colon'\n---", NOTE_METADATA_NONE);
    read_status("---\n* list\n---", NOTE_METADATA_NONE);
    read_status("---\ntags: [\"\\t\", \"\\N\"]\n---", NOTE_METADATA_INVALID_TAG);
    read_status("---\ntags: [\"\\_\", \"\\ \", \"x\"]\n---", NOTE_METADATA_INVALID_TAG);
    read_status("---\ntitle: \"\\0\\a\\b\\t\\n\\v\\f\\r\\e\\ \\\"\\/\\\\\\N\\_\\L\\P\"\n"
                "tags: []\n---",
                NOTE_METADATA_SUPPORTED);
}

static void set_boundaries(void)
{
    struct note_tag *owned[note_tags_max_count + 1];
    const struct note_tag *borrowed[note_tags_max_count + 1];
    for (size_t index = 0; index <= note_tags_max_count; ++index)
    {
        char name[20];
        require(snprintf(name, sizeof name, "tag%zu", index) > 0, "distinct tag fixture");
        owned[index] = tag(name);
        borrowed[index] = owned[index];
    }
    struct note_tags *full = nullptr;
    require(note_tags_create(borrowed, note_tags_max_count, &full) == NOTE_TAGS_READY,
            "32 distinct names are accepted");
    struct note_tags *out = full;
    require(note_tags_create(borrowed, note_tags_max_count + 1, &out) == NOTE_TAGS_TOO_MANY &&
                out == full,
            "33 names preserve output on refusal");
    require(note_tags_with(full, owned[note_tags_max_count], NOTE_TAG_PRESENT, &out) ==
                    NOTE_TAGS_TOO_MANY &&
                out == full,
            "adding a 33rd tag is refused");
    require(note_tags_with(full, owned[0], NOTE_TAG_PRESENT, &out) == NOTE_TAGS_READY &&
                note_tags_equal(full, out),
            "including a present tag is idempotent");
    note_tags_destroy(out);
    require(note_tags_with(full, owned[note_tags_max_count], NOTE_TAG_ABSENT, &out) ==
                    NOTE_TAGS_READY &&
                note_tags_equal(full, out),
            "excluding absent tag is idempotent");
    note_tags_destroy(out);
    note_tags_destroy(full);
    for (size_t index = 0; index <= note_tags_max_count; ++index)
        note_tag_destroy(owned[index]);
    char unicode[260];
    for (size_t index = 0; index < 65; ++index)
        memcpy(unicode + 4 * index, "😀", 4);
    struct note_tag *value = nullptr;
    require(note_tag_create(unicode, 256, &value) == NOTE_TAG_READY,
            "64 supplementary scalars use the full 256-byte allowance");
    struct note_tag *held = value;
    require(note_tag_create(unicode, 260, &value) == NOTE_TAG_TOO_LONG && value == held,
            "65 supplementary scalars exceed both limits without changing output");
    note_tag_destroy(value);
}

static void edit(const char *_Nonnull source, const struct note_tags *_Nonnull desired,
                 struct note_search_span expected, const char16_t *_Nonnull replacement)
{
    struct note_text *body = text(source);
    struct replace_edit *change = nullptr;
    require(note_metadata_edit(note_text_metadata(body), desired, &change) == NOTE_TAG_EDIT_READY,
            "tag change creates one native replacement");
    struct note_search_span actual = replace_edit_span(change);
    require(actual.start == expected.start && actual.end == expected.end,
            "native UTF-16 span matches fixed expected offsets");
    size_t count = 0;
    while (replacement[count] != u'\0')
        ++count;
    require(replace_edit_length(change) == count && memcmp(replace_edit_units(change), replacement,
                                                           (count + 1) * sizeof *replacement) == 0,
            "only requested tags change; kept raw spelling/comment/newline is exact");
    require(same_text(note_text_bytes(body), source), "edit generation does not mutate source");
    replace_edit_destroy(change);
    note_text_destroy(body);
}

static void edits(void)
{
    const char *const chosen[] = {"keep", "new tag"};
    struct note_tags *desired = tags(chosen, 2);
    edit("# Body", desired, (struct note_search_span){0, 0},
         u"---\rtags:\r  - \"keep\"\r  - \"new tag\"\r---\r");
    edit("---\ntitle: 'old' # same\n---\n# Body", desired, (struct note_search_span){24, 24},
         u"tags:\r  - \"keep\"\r  - \"new tag\"\r");
    edit("---\ntags: ['keep', drop] # outside\n---\nBody", desired,
         (struct note_search_span){10, 24}, u"['keep', \"new tag\"]");
    edit("---\ntags: # header\n  - 'keep' # exact\n  # independent\n  - drop # remove\nother: "
         "42\n---\nBody",
         desired, (struct note_search_span){4, 72},
         u"tags: # header\r  - 'keep' # exact\r  # independent\r  - \"new tag\"\r");
    struct note_tags *empty = nullptr;
    require(note_tags_empty(&empty) == NOTE_TAGS_READY, "empty desired set");
    edit("---\ntags: [a, a] # keep\n---\nBody", empty, (struct note_search_span){10, 16}, u"[]");
    edit("---\ntags:\n  - a # remove\n# keep\n---\nBody", empty, (struct note_search_span){4, 25},
         u"tags:\r");
    struct note_text *same = text("---\ntags: [\"keep\", 'new tag', keep]\n---\nBody");
    struct replace_edit *unchanged = nullptr;
    require(note_metadata_edit(note_text_metadata(same), desired, &unchanged) ==
                    NOTE_TAG_EDIT_UNCHANGED &&
                unchanged == nullptr,
            "same membership does not normalize duplicates or create Undo");
    note_text_destroy(same);
    same = text("---\n\"tags\": [keep]\n---\nBody");
    require(note_metadata_edit(note_text_metadata(same), desired, &unchanged) ==
                    NOTE_TAG_EDIT_UNAVAILABLE &&
                unchanged == nullptr,
            "unsupported header is never replaced by a new header");
    note_text_destroy(same);
    note_tags_destroy(empty);
    note_tags_destroy(desired);
}

static void unicode_positions(void)
{
    const char *const names[] = {"new"};
    struct note_tags *desired = tags(names, 1);
    edit("---\r\ntitle: \"😀\"\r\ntags: [old]\r\n---\r\nbody", desired,
         (struct note_search_span){22, 27}, u"[\"new\"]");
    edit("---\rtags: [old]\r---\rbody", desired, (struct note_search_span){10, 15}, u"[\"new\"]");
    struct note_text *body = text("\xef\xbb\xbf---\ntags: [a]\n---\nbody");
    require(note_metadata_status(note_text_metadata(body)) == NOTE_METADATA_SUPPORTED,
            "file signature is handled once by note_text");
    note_text_destroy(body);
    body = text("\xef\xbb\xbf\xef\xbb\xbf---\ntags: [a]\n---\nbody");
    require(note_metadata_status(note_text_metadata(body)) == NOTE_METADATA_NONE,
            "remaining U+FEFF is content, not a second signature");
    note_text_destroy(body);
    note_tags_destroy(desired);
}

static void quoted_edits(void)
{
    const char *const names[] = {"a", "new"};
    struct note_tags *desired = tags(names, 2);
    edit("---\ntags: [a, a, b]\n---\n", desired, (struct note_search_span){10, 19},
         u"[a, a, \"new\"]");
    edit("line\r\nbody", desired, (struct note_search_span){0, 0},
         u"---\rtags:\r  - \"a\"\r  - \"new\"\r---\r");
    edit("line\rbody", desired, (struct note_search_span){0, 0},
         u"---\rtags:\r  - \"a\"\r  - \"new\"\r---\r");
    note_tags_destroy(desired);
    const char *const escaped[] = {"\"\\😀"};
    desired = tags(escaped, 1);
    edit("---\ntags: [old]\n---\n", desired, (struct note_search_span){10, 15},
         u"[\"\\\"\\\\😀\"]");
    note_tags_destroy(desired);
}

static void header_byte_boundary(void)
{
    char *source = malloc(note_metadata_max_bytes + 1);
    require(source != nullptr, "exact byte boundary fixture allocation");
    const char *prefix = "---\ntitle: x\n#";
    size_t start = strlen(prefix);
    memcpy(source, prefix, start);
    memset(source + start, 'x', note_metadata_max_bytes - start - 5);
    memcpy(source + note_metadata_max_bytes - 5, "\n---\n", 6);
    struct note_text *body = text(source);
    require(note_metadata_status(note_text_metadata(body)) == NOTE_METADATA_SUPPORTED,
            "exactly 64 KiB header is supported");
    const char *const names[] = {"new"};
    struct note_tags *desired = tags(names, 1);
    struct replace_edit *change = nullptr;
    require(note_metadata_edit(note_text_metadata(body), desired, &change) == NOTE_TAG_EDIT_LIMIT &&
                change == nullptr,
            "expanding a full header is refused before native replacement");
    note_tags_destroy(desired);
    note_text_destroy(body);
    free(source);
}

static void rendering(void)
{
    const char *source =
        "---\ntitle: hidden-title\ntags: [hidden-tag]\n---\n# Visible\n![image](images/a.png)\n";
    struct note_text *body = text(source);
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(body, folio_theme_palette(FOLIO_THEME_LIGHT),
                                (struct markdown_rtf_locale){"Consolas", FOLIO_LANGUAGE_JA},
                                &rtf) == MARKDOWN_RTF_CONVERTED,
            "supported metadata renders through the canonical Markdown path");
    require(strstr(markdown_rtf_text(rtf), "hidden-title") == nullptr &&
                strstr(markdown_rtf_text(rtf), "hidden-tag") == nullptr &&
                strstr(markdown_rtf_text(rtf), "Visible") != nullptr,
            "viewer hides supported metadata only");
    const struct markdown_images *images = markdown_rtf_images(rtf);
    require(images != nullptr && markdown_images_count(images) == 1, "body media still collected");
    require(same_text(image_source_target(markdown_images_at(images, 0)), "images/a.png"),
            "image keeps source destination");
    struct note_search_span destination = image_source_span(markdown_images_at(images, 0));
    size_t start = (size_t)(strstr(source, "images/a.png") - source);
    require(destination.start == start && destination.end == start + 12,
            "metadata hiding does not shift the original image source range");
    markdown_rtf_destroy(rtf);
    require(markdown_rtf_plain(body, folio_theme_palette(FOLIO_THEME_LIGHT), "Consolas", &rtf) ==
                    MARKDOWN_RTF_CONVERTED &&
                strstr(markdown_rtf_text(rtf), "hidden-tag") != nullptr,
            "editing retains all source metadata");
    markdown_rtf_destroy(rtf);
    note_text_destroy(body);
}

static void bounded_headers(void)
{
    char *source = malloc(note_metadata_max_bytes + 40);
    require(source != nullptr, "bounded header fixture allocation");
    memcpy(source, "---\n#", 5);
    memset(source + 5, 'x', note_metadata_max_bytes - 5);
    memcpy(source + note_metadata_max_bytes, "\ntags: [x]\n---\n", 16);
    read_status(source, NOTE_METADATA_LIMIT);
    size_t at = 4;
    for (size_t index = 0; index < note_metadata_max_keys; ++index)
    {
        int written = snprintf(source + at, note_metadata_max_bytes - at, "k%zu: value\n", index);
        require(written > 0, "root key fixture fits");
        at += (size_t)written;
    }
    memcpy(source + at, "---\nbody", 9);
    struct note_text *body = text(source);
    require(note_metadata_status(note_text_metadata(body)) == NOTE_METADATA_SUPPORTED,
            "128 existing keys are readable");
    const char *const names[] = {"new"};
    struct note_tags *desired = tags(names, 1);
    struct replace_edit *change = nullptr;
    require(note_metadata_edit(note_text_metadata(body), desired, &change) == NOTE_TAG_EDIT_LIMIT &&
                change == nullptr,
            "adding a 129th key is refused before native editing");
    note_text_destroy(body);
    memcpy(source, "---\ntags:\n", 10);
    at = 10;
    for (size_t index = 0; index < note_tags_max_count; ++index)
    {
        memcpy(source + at, "  - keep\n", 9);
        at += 9;
    }
    memcpy(source + at, "---\nbody", 9);
    body = text(source);
    require(note_metadata_status(note_text_metadata(body)) == NOTE_METADATA_SUPPORTED,
            "32 duplicate raw entries are readable");
    struct note_tag *keep = tag("keep");
    struct note_tags *both = nullptr;
    require(note_tags_with(desired, keep, NOTE_TAG_PRESENT, &both) == NOTE_TAGS_READY,
            "desired keeps duplicates and adds a tag");
    require(note_metadata_edit(note_text_metadata(body), both, &change) == NOTE_TAG_EDIT_LIMIT &&
                change == nullptr,
            "adding a 33rd raw entry is refused even when membership has only two names");
    note_tags_destroy(both);
    note_tag_destroy(keep);
    note_tags_destroy(desired);
    note_text_destroy(body);
    free(source);
}

static bool allocation_enabled(void)
{
    allocation_probe_fail_at(1);
    struct note_tag *value = nullptr;
    enum note_tag_outcome outcome = note_tag_create("x", 1, &value);
    allocation_probe_fail_at(0);
    note_tag_destroy(value);
    return outcome == NOTE_TAG_OUT_OF_MEMORY;
}

static void metadata_failures(const char *_Nonnull source)
{
    struct note_text *held = text("original");
    allocation_probe_fail_at(0);
    struct note_text *created = text(source);
    size_t count = allocation_probe_calls();
    note_text_destroy(created);
    for (size_t nth = 1; nth <= count; ++nth)
    {
        struct note_text *out = held;
        allocation_probe_fail_at(nth);
        enum note_text_outcome outcome = note_text_create(source, strlen(source), &out);
        allocation_probe_fail_at(0);
        require(outcome == NOTE_TEXT_OUT_OF_MEMORY && out == held &&
                    same_text(note_text_bytes(held), "original"),
                "every metadata allocation failure preserves the old note owner");
    }
    note_text_destroy(held);
}

static void edit_failures(const char *_Nonnull source)
{
    struct note_text *body = text(source);
    const char *const names[] = {"keep", "new"};
    struct note_tags *desired = tags(names, 2);
    struct replace_edit *held = nullptr;
    require(replace_edit_create((struct note_search_span){1, 2}, 0, &held) == REPLACE_EDIT_READY,
            "held edit fixture");
    struct replace_edit *created = nullptr;
    allocation_probe_fail_at(0);
    require(note_metadata_edit(note_text_metadata(body), desired, &created) == NOTE_TAG_EDIT_READY,
            "baseline tag edit");
    size_t count = allocation_probe_calls();
    replace_edit_destroy(created);
    for (size_t nth = 1; nth <= count; ++nth)
    {
        struct replace_edit *out = held;
        allocation_probe_fail_at(nth);
        enum note_tag_edit_outcome outcome =
            note_metadata_edit(note_text_metadata(body), desired, &out);
        allocation_probe_fail_at(0);
        require(outcome == NOTE_TAG_EDIT_OUT_OF_MEMORY && out == held &&
                    same_text(note_text_bytes(body), source),
                "every edit allocation failure preserves source and existing output");
    }
    replace_edit_destroy(held);
    note_tags_destroy(desired);
    note_text_destroy(body);
}

static void catalog_values(void)
{
    const char *const left[] = {"red", "blue", "red", "日"};
    const char *const right[] = {"blue", "Red"};
    struct note_tags *first = tags(left, 4);
    struct note_tags *second = tags(right, 2);
    struct note_tags *empty = tags(left, 0);
    const struct note_tags *notes[] = {first, second, nullptr, empty};
    struct tag_catalog *catalog = nullptr;
    require(tag_catalog_create(notes, 4, &catalog) == TAG_CATALOG_READY,
            "saved sets create an immutable catalog");
    struct tag_catalog *held = catalog;
    require(tag_catalog_create(notes, SIZE_MAX, &catalog) == TAG_CATALOG_OUT_OF_MEMORY &&
                catalog == held,
            "unrepresentable source count is refused before array access");
    note_tags_destroy(first);
    note_tags_destroy(second);
    note_tags_destroy(empty);
    require(tag_catalog_count(catalog) == 4 && tag_catalog_unavailable(catalog) == 1,
            "unavailable note is distinct from known untagged; raw duplicate counts once");
    const char *const expected[] = {"Red", "blue", "red", "日"};
    const size_t uses[] = {1, 2, 1, 1};
    for (size_t index = 0; index < 4; ++index)
        require(same_text(note_tag_bytes(tag_catalog_tag(catalog, index)), expected[index]) &&
                    tag_catalog_usage(catalog, index) == uses[index],
                "catalog owns exact names in byte order and independent expected note counts");
    const char *const queries[] = {"Red", "blue", "red", "日", "RED", "a", "z", "日本"};
    for (size_t index = 0; index < sizeof queries / sizeof queries[0]; ++index)
    {
        struct note_tag *query = tag(queries[index]);
        require(tag_catalog_contains(catalog, query) == (index < 4),
                "sorted membership covers both ends, interior gaps and exact case");
        note_tag_destroy(query);
    }
    tag_catalog_destroy(catalog);
    require(tag_catalog_create(notes, 0, &catalog) == TAG_CATALOG_READY &&
                tag_catalog_count(catalog) == 0 && tag_catalog_unavailable(catalog) == 0,
            "empty completed catalog has no unavailable notes");
    tag_catalog_destroy(catalog);
}

static void catalog_failures(void)
{
    const char *const names[] = {"red", "blue", "日"};
    struct note_tags *selected = tags(names, 3);
    const struct note_tags *notes[] = {selected, nullptr, selected};
    struct tag_catalog *held = nullptr;
    require(tag_catalog_create(notes, 0, &held) == TAG_CATALOG_READY, "held catalog fixture");
    struct tag_catalog *created = nullptr;
    allocation_probe_fail_at(0);
    require(tag_catalog_create(notes, 3, &created) == TAG_CATALOG_READY,
            "catalog allocation baseline");
    size_t count = allocation_probe_calls();
    tag_catalog_destroy(created);
    for (size_t nth = 1; nth <= count; ++nth)
    {
        struct tag_catalog *out = held;
        allocation_probe_fail_at(nth);
        enum tag_catalog_outcome outcome = tag_catalog_create(notes, 3, &out);
        allocation_probe_fail_at(0);
        require(outcome == TAG_CATALOG_OUT_OF_MEMORY && out == held &&
                    tag_catalog_count(held) == 0 && note_tags_count(selected) == 3,
                "every catalog allocation failure preserves old output and borrowed names");
    }
    tag_catalog_destroy(held);
    note_tags_destroy(selected);
}

static void allocation_failures(void)
{
    if (!allocation_enabled())
        return;
    metadata_failures("plain body");
    metadata_failures(
        "---\ntitle: \"escapes\\n\\u65E5\"\ntags: [a, 'b''c', \"d\\x20e\", a]\n---\nbody");
    metadata_failures("---\ntags:\n  - keep\n  - drop\nother: |\n  bytes\n---\nbody");
    edit_failures("plain body");
    edit_failures("---\ntags: [keep, drop]\n---\nbody");
    edit_failures("---\ntags:\n  - keep\n  # preserved\n  - drop\n---\nbody");
    edit_failures("---\ntitle: unchanged\n---\nbody");
    catalog_failures();
}

static struct index_filter_entry filter_entry(size_t index, const struct note_text *_Nullable body)
{
    const struct note_tags *available = nullptr;
    if (body != nullptr && note_metadata_available(note_text_metadata(body)))
        available = note_metadata_tags(note_text_metadata(body));
    return (struct index_filter_entry){.ref = {0, index},
                                       .name = "name",
                                       .body = body == nullptr ? nullptr : note_text_bytes(body),
                                       .length = body == nullptr ? 0 : note_text_length(body),
                                       .tags = available};
}

static struct index_filter *_Nonnull matching(const struct index_filter_query *_Nonnull query,
                                              const struct index_filter_entry *_Nonnull entries,
                                              size_t count, size_t budget)
{
    struct index_filter_build *build = nullptr;
    require(index_filter_build_create(query, count, &build) == INDEX_FILTER_ACCEPTED,
            "tag query is accepted including a tag-only request");
    for (size_t index = 0; index < count; ++index)
    {
        enum index_filter_step step = INDEX_FILTER_STEP_MORE;
        while (step == INDEX_FILTER_STEP_MORE)
        {
            size_t work = budget;
            step = index_filter_build_step(build, &entries[index], &work);
            require(work < budget, "every unfinished tag step consumes bounded work");
        }
    }
    struct index_filter *result = nullptr;
    require(index_filter_finish(&build, &result) == INDEX_FILTER_FINISH_ACCEPTED &&
                build == nullptr,
            "only a sealed result transfers ownership");
    return result;
}

static void match_bits(const struct index_filter *_Nonnull filter, unsigned mask, size_t count)
{
    size_t matched = 0;
    for (size_t index = 0; index < count; ++index)
    {
        bool expected = (mask & (1u << index)) != 0;
        require(index_filter_note(filter, (struct note_ref){0, index}) == expected,
                "membership matches an independently fixed bit set");
        matched += expected ? 1 : 0;
    }
    require(index_filter_count(filter) == matched, "result has no extra references");
}

static void match_queries(const struct index_filter_entry *_Nonnull entries, size_t count,
                          const struct note_tags *_Nonnull selected)
{
    const struct index_filter_query queries[] = {
        {.term = "", .length = 0, .tags = selected, .mode = TAG_MATCH_ALL},
        {.term = "", .length = 0, .tags = selected, .mode = TAG_MATCH_ANY},
        {.term = "NEEDLE", .length = 6, .tags = selected, .mode = TAG_MATCH_ALL},
        {.term = "needle", .length = 6, .tags = selected, .mode = TAG_MATCH_ANY},
        {.term = "needle", .length = 6, .tags = nullptr, .mode = TAG_MATCH_ALL}};
    const unsigned expected[] = {0x41, 0x43, 0x01, 0x03, 0x1f};
    const size_t budgets[] = {1, 3, 65536};
    for (size_t query = 0; query < sizeof queries / sizeof queries[0]; ++query)
        for (size_t budget = 0; budget < sizeof budgets / sizeof budgets[0]; ++budget)
        {
            struct index_filter *result =
                matching(&queries[query], entries, count, budgets[budget]);
            match_bits(result, expected[query], count);
            index_filter_destroy(result);
        }
}

static void tag_matching(void)
{
    const char *const sources[] = {"---\ntags: [red, blue]\n---\nneedle",
                                   "---\ntags: [blue]\n---\nneedle",
                                   "---\ntags: [Red]\n---\nneedle",
                                   "---\ntags: *alias\n---\nneedle",
                                   "plain needle",
                                   "ignored",
                                   "---\ntags: [red, blue]\n---\nother"};
    struct note_text *bodies[7];
    struct index_filter_entry entries[7];
    for (size_t index = 0; index < 7; ++index)
    {
        bodies[index] = text(sources[index]);
        entries[index] = filter_entry(index, bodies[index]);
    }
    entries[5] = filter_entry(5, nullptr);
    const char *const names[] = {"red", "blue"};
    struct note_tags *selected = tags(names, 2);
    match_queries(entries, 7, selected);
    struct index_filter_query query = {
        .term = "", .length = 0, .tags = selected, .mode = TAG_MATCH_ALL};
    struct index_filter *empty = matching(&query, entries, 0, 1);
    require(index_filter_count(empty) == 0, "zero notes seal tag-only query immediately");
    index_filter_destroy(empty);
    note_tags_destroy(selected);
    for (size_t index = 0; index < 7; ++index)
        note_text_destroy(bodies[index]);
}

void run_tag_allocation_tests(void)
{
    require(allocation_enabled(), "tag allocation selector requires actual failure injection");
    allocation_failures();
}

void run_tag_tests(void)
{
    names();
    sets();
    recognition();
    metadata_shapes();
    scalar_types();
    metadata_edges();
    set_boundaries();
    edits();
    unicode_positions();
    quoted_edits();
    header_byte_boundary();
    rendering();
    bounded_headers();
    tag_matching();
    catalog_values();
    allocation_failures();
}
