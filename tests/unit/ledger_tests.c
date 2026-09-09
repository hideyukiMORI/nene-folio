#include "category_ledger.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_ledger.h"
#include "unit_tests.h"

#include <string.h>

static const char *const category_text =
    "{\"version\": 1, \"categories\": ["
    "{\"name\": \"\xE4\xBB\x95\xE4\xBA\x8B\", \"color\": \"#3D7EFF\", \"expanded\": true},"
    "{\"name\": \"memo\", \"color\": \"#ff8800\", \"expanded\": false}]}";

static struct category_ledger *_Nonnull parse_categories(const char *_Nonnull text)
{
    struct category_ledger *ledger = nullptr;
    require(category_ledger_parse(text, strlen(text), &ledger) == CATEGORY_LEDGER_ACCEPTED, text);
    return ledger;
}

static enum category_ledger_outcome category_outcome(const char *_Nonnull text)
{
    struct category_ledger *ledger = nullptr;
    enum category_ledger_outcome outcome = category_ledger_parse(text, strlen(text), &ledger);
    category_ledger_destroy(ledger);
    return outcome;
}

static struct name_list *_Nonnull names_of(const char *const *_Nonnull items, size_t count)
{
    struct name_list *list = nullptr;
    require(name_list_create(&list) == NAME_LIST_ACCEPTED, "names create");
    for (size_t index = 0; index < count; ++index)
    {
        require(name_list_append(list, items[index], strlen(items[index])) == NAME_LIST_ACCEPTED,
                "names append");
    }
    return list;
}

static void verify_category_parse(void)
{
    struct category_ledger *ledger = parse_categories(category_text);
    require(category_ledger_count(ledger) == 2, "category count");
    require(same_text(category_ledger_name(ledger, 0), "\xE4\xBB\x95\xE4\xBA\x8B"), "name 0");
    require(same_text(category_ledger_name(ledger, 1), "memo"), "name 1");
    require(category_ledger_color(ledger, 0).red == 0x3D &&
                category_ledger_color(ledger, 1).red == 0xFF,
            "colors");
    require(category_ledger_expanded(ledger, 0) && !category_ledger_expanded(ledger, 1),
            "expanded");
    category_ledger_destroy(ledger);
    ledger = parse_categories("{\"version\":1,\"categories\":[]}");
    require(category_ledger_count(ledger) == 0, "empty categories");
    category_ledger_destroy(ledger);
    category_ledger_destroy(nullptr);
}

static void verify_category_rejections(void)
{
    require(category_outcome("{\"version\": 2, \"categories\": []}") ==
                CATEGORY_LEDGER_UNSUPPORTED_VERSION,
            "version 2");
    require(category_outcome("{\"version\": \"1\", \"categories\": []}") ==
                CATEGORY_LEDGER_MALFORMED,
            "version as string");
    require(category_outcome("{\"categories\": [], \"version\": 1}") == CATEGORY_LEDGER_MALFORMED,
            "key order");
    require(category_outcome("[]") == CATEGORY_LEDGER_MALFORMED, "not an object");
    require(category_outcome("{\"version\": 1, \"categories\": {}}") == CATEGORY_LEDGER_MALFORMED,
            "categories not array");
    require(category_outcome("{\"version\": 1, \"categories\": [1]}") == CATEGORY_LEDGER_MALFORMED,
            "entry not object");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"color\": \"#000000\"}]}") ==
                CATEGORY_LEDGER_MALFORMED,
            "entry without name");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": 1}]}") ==
                CATEGORY_LEDGER_MALFORMED,
            "name not string");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": \"a\", \"color\": "
                             "\"red\", \"expanded\": true}]}") == CATEGORY_LEDGER_MALFORMED,
            "bad color");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": \"a\", \"color\": "
                             "\"#000000\", \"expanded\": 1}]}") == CATEGORY_LEDGER_MALFORMED,
            "expanded not bool");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": \"a\", \"color\": "
                             "\"#000000\"}]}") == CATEGORY_LEDGER_MALFORMED,
            "missing expanded");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": \"a\", \"color\": "
                             "\"#000000\", \"expanded\": true, \"extra\": 1}]}") ==
                CATEGORY_LEDGER_MALFORMED,
            "extra key");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": \"a/b\", \"color\": "
                             "\"#000000\", \"expanded\": true}]}") == CATEGORY_LEDGER_MALFORMED,
            "invalid name");
    require(category_outcome("{\"version\": 1, \"categories\": [{\"name\": \"a\", \"color\": "
                             "\"#000000\", \"expanded\": true}, {\"name\": \"a\", \"color\": "
                             "\"#000000\", \"expanded\": true}]}") == CATEGORY_LEDGER_MALFORMED,
            "duplicate name");
    require(category_outcome("{\"version\": 1, \"categories\": [], \"more\": 1}") ==
                CATEGORY_LEDGER_MALFORMED,
            "extra top-level key");
    require(category_outcome("{\"version\": 1, \"categories\": []} x") == CATEGORY_LEDGER_MALFORMED,
            "trailing garbage");
    require(category_outcome("{\"version\": 1, \"categories\": [") == CATEGORY_LEDGER_MALFORMED,
            "truncated");
}

static void verify_category_write(void)
{
    struct category_ledger *ledger = parse_categories(category_text);
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    category_ledger_write(ledger, writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "category write finish");
    const char *expected = "{\n  \"version\": 1,\n  \"categories\": [\n    {\n"
                           "      \"name\": \"\xE4\xBB\x95\xE4\xBA\x8B\",\n"
                           "      \"color\": \"#3D7EFF\",\n      \"expanded\": true\n    },\n"
                           "    {\n      \"name\": \"memo\",\n      \"color\": \"#FF8800\",\n"
                           "      \"expanded\": false\n    }\n  ]\n}";
    require(same_text(json_writer_text(writer), expected), "category text");
    struct category_ledger *again = parse_categories(json_writer_text(writer));
    require(category_ledger_count(again) == 2 && !category_ledger_expanded(again, 1) &&
                category_ledger_color(again, 1).green == 0x88,
            "category round trip");
    category_ledger_destroy(again);
    json_writer_destroy(writer);
    category_ledger_destroy(ledger);
}

static void verify_category_reconcile(void)
{
    struct category_ledger *ledger = parse_categories(category_text);
    const char *const scanned_names[] = {"memo", "new", "\xE4\xBB\x95\xE4\xBA\x8B"};
    struct name_list *scanned = names_of(scanned_names, 3);
    struct category_ledger *merged = nullptr;
    require(category_ledger_reconcile(ledger, scanned, &merged) == CATEGORY_LEDGER_ACCEPTED,
            "reconcile");
    require(category_ledger_count(merged) == 3, "merged count");
    require(same_text(category_ledger_name(merged, 0), "\xE4\xBB\x95\xE4\xBA\x8B") &&
                same_text(category_ledger_name(merged, 1), "memo") &&
                same_text(category_ledger_name(merged, 2), "new"),
            "ledger order then scanned order");
    require(category_ledger_color(merged, 1).red == 0xFF && !category_ledger_expanded(merged, 1),
            "kept settings");
    require(category_ledger_color(merged, 2).red == 0x7F && category_ledger_expanded(merged, 2),
            "default settings");
    category_ledger_destroy(merged);
    name_list_destroy(scanned);
    const char *const only_old[] = {"old"};
    scanned = names_of(only_old, 1);
    require(category_ledger_reconcile(ledger, scanned, &merged) == CATEGORY_LEDGER_ACCEPTED,
            "reconcile drops");
    require(category_ledger_count(merged) == 1 && same_text(category_ledger_name(merged, 0), "old"),
            "absent directories dropped");
    category_ledger_destroy(merged);
    name_list_destroy(scanned);
    category_ledger_destroy(ledger);
}

static void verify_category_toggle(void)
{
    struct category_ledger *ledger = parse_categories(category_text);
    struct category_ledger *toggled = nullptr;
    require(category_ledger_toggled(ledger, 1, &toggled) == CATEGORY_LEDGER_ACCEPTED, "toggle");
    require(category_ledger_count(toggled) == 2 && category_ledger_expanded(toggled, 0) &&
                category_ledger_expanded(toggled, 1),
            "only the chosen entry flips");
    require(same_text(category_ledger_name(toggled, 1), "memo") &&
                category_ledger_color(toggled, 1).red == 0xFF,
            "names and colors are kept");
    require(!category_ledger_expanded(ledger, 1), "source is untouched");
    struct category_ledger *again = nullptr;
    require(category_ledger_toggled(toggled, 1, &again) == CATEGORY_LEDGER_ACCEPTED &&
                !category_ledger_expanded(again, 1),
            "toggle back");
    category_ledger_destroy(again);
    category_ledger_destroy(toggled);
    category_ledger_destroy(ledger);
}

/* 3 つのカテゴリを from から to へ動かし、名前を並べた 3 文字を返す。 */
static void moved_categories(size_t from, size_t to, char *_Nonnull out)
{
    struct category_ledger *ledger =
        parse_categories("{\"version\": 1, \"categories\": ["
                         "{\"name\": \"a\", \"color\": \"#010101\", \"expanded\": true},"
                         "{\"name\": \"b\", \"color\": \"#020202\", \"expanded\": false},"
                         "{\"name\": \"c\", \"color\": \"#030303\", \"expanded\": true}]}");
    struct category_ledger *moved = nullptr;
    require(category_ledger_moved(ledger, from, to, &moved) == CATEGORY_LEDGER_ACCEPTED, "moved");
    require(category_ledger_count(moved) == 3, "moved keeps the count");
    for (size_t index = 0; index < 3; ++index)
    {
        out[index] = category_ledger_name(moved, index)[0];
        /* 色と展開は名前と一緒に移る。 */
        require(category_ledger_color(moved, index).red == (unsigned char)(out[index] - 'a' + 1) &&
                    category_ledger_expanded(moved, index) == (out[index] != 'b'),
                "colors and expansion travel with the name");
    }
    out[3] = '\0';
    require(same_text(category_ledger_name(ledger, 0), "a"), "the source ledger is untouched");
    category_ledger_destroy(moved);
    category_ledger_destroy(ledger);
}

static void verify_category_moved(void)
{
    char order[4] = {0};
    moved_categories(0, 2, order);
    require(same_text(order, "bca"), "the first entry moves to the end");
    moved_categories(2, 0, order);
    require(same_text(order, "cab"), "the last entry moves to the front");
    moved_categories(0, 1, order);
    require(same_text(order, "bac"), "one step down");
    moved_categories(2, 1, order);
    require(same_text(order, "acb"), "one step up");
    moved_categories(1, 1, order);
    require(same_text(order, "abc"), "the same position is a copy");
}

static struct note_ledger *_Nonnull parse_notes(const char *_Nonnull text)
{
    struct note_ledger *ledger = nullptr;
    require(note_ledger_parse(text, strlen(text), &ledger) == NOTE_LEDGER_ACCEPTED, text);
    return ledger;
}

static enum note_ledger_outcome note_outcome(const char *_Nonnull text)
{
    struct note_ledger *ledger = nullptr;
    enum note_ledger_outcome outcome = note_ledger_parse(text, strlen(text), &ledger);
    note_ledger_destroy(ledger);
    return outcome;
}

static void verify_note_parse(void)
{
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\"]}");
    require(note_ledger_count(ledger) == 2 && same_text(note_ledger_name(ledger, 1), "b"),
            "notes parsed");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "writer");
    note_ledger_write(ledger, writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "note write finish");
    require(same_text(json_writer_text(writer),
                      "{\n  \"version\": 1,\n  \"notes\": [\n    \"a\",\n    \"b\"\n  ]\n}"),
            "note text");
    json_writer_destroy(writer);
    note_ledger_destroy(ledger);
    ledger = parse_notes("{\"version\":1,\"notes\":[]}");
    require(note_ledger_count(ledger) == 0, "empty notes");
    note_ledger_destroy(ledger);
    note_ledger_destroy(nullptr);
    require(note_outcome("{\"version\": 3, \"notes\": []}") == NOTE_LEDGER_UNSUPPORTED_VERSION,
            "note version");
    require(note_outcome("{\"version\": 1, \"notes\": [1]}") == NOTE_LEDGER_MALFORMED,
            "note not string");
    require(note_outcome("{\"version\": 1, \"notes\": [\"a\", \"a\"]}") == NOTE_LEDGER_MALFORMED,
            "note duplicate");
    require(note_outcome("{\"version\": 1, \"notes\": [\"\"]}") == NOTE_LEDGER_MALFORMED,
            "note invalid name");
    require(note_outcome("{\"version\": 1, \"items\": []}") == NOTE_LEDGER_MALFORMED, "note key");
    require(note_outcome("{\"version\": 1, \"notes\": []} }") == NOTE_LEDGER_MALFORMED,
            "note trailing");
    require(note_outcome("{\"version\": 1, \"notes\": [], \"x\": 0}") == NOTE_LEDGER_MALFORMED,
            "note extra key");
    require(note_outcome("{\"version\": 1}") == NOTE_LEDGER_MALFORMED, "note missing notes");
    require(note_outcome("42") == NOTE_LEDGER_MALFORMED, "note not object");
    require(note_outcome("{\"version\": \"1\", \"notes\": []}") == NOTE_LEDGER_MALFORMED,
            "note version as string");
    require(note_outcome("{\"x\": 1, \"notes\": []}") == NOTE_LEDGER_MALFORMED, "note first key");
    require(note_outcome("{\"version\": 1, \"notes\": {}}") == NOTE_LEDGER_MALFORMED,
            "notes not array");
}

static void verify_note_reconcile(void)
{
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\", \"c\"]}");
    const char *const scanned_names[] = {"c", "a", "d"};
    struct name_list *scanned = names_of(scanned_names, 3);
    struct note_ledger *merged = nullptr;
    require(note_ledger_reconcile(ledger, scanned, &merged) == NOTE_LEDGER_ACCEPTED, "reconcile");
    require(note_ledger_count(merged) == 3 && same_text(note_ledger_name(merged, 0), "a") &&
                same_text(note_ledger_name(merged, 1), "c") &&
                same_text(note_ledger_name(merged, 2), "d"),
            "ledger order, missing dropped, new appended");
    note_ledger_destroy(merged);
    name_list_destroy(scanned);
    note_ledger_destroy(ledger);
}

/* 3 つのノートを from から to へ動かし、名前を並べた 3 文字を返す。 */
static void moved_notes(size_t from, size_t to, char *_Nonnull out)
{
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\", \"c\"]}");
    struct note_ledger *moved = nullptr;
    require(note_ledger_moved(ledger, from, to, &moved) == NOTE_LEDGER_ACCEPTED, "note moved");
    require(note_ledger_count(moved) == 3, "note moved keeps the count");
    for (size_t index = 0; index < 3; ++index)
    {
        out[index] = note_ledger_name(moved, index)[0];
    }
    out[3] = '\0';
    require(same_text(note_ledger_name(ledger, 0), "a"), "the source note ledger is untouched");
    note_ledger_destroy(moved);
    note_ledger_destroy(ledger);
}

/* 並び替えた台帳を書いて読み直すと、同じ順序に戻る（FR-008 の往復）。 */
static void verify_note_round_trip(void)
{
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\", \"c\"]}");
    struct note_ledger *moved = nullptr;
    require(note_ledger_moved(ledger, 2, 0, &moved) == NOTE_LEDGER_ACCEPTED, "round trip moved");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "round trip writer");
    note_ledger_write(moved, writer);
    require(json_writer_finish(writer) == JSON_WRITER_ACCEPTED, "round trip finish");
    require(same_text(json_writer_text(writer),
                      "{\n  \"version\": 1,\n  \"notes\": [\n    \"c\",\n    \"a\",\n    \"b\"\n  "
                      "]\n}"),
            "the written document holds the new order");
    struct note_ledger *again = nullptr;
    require(note_ledger_parse(json_writer_text(writer), json_writer_length(writer), &again) ==
                NOTE_LEDGER_ACCEPTED,
            "round trip parse");
    require(same_text(note_ledger_name(again, 0), "c") &&
                same_text(note_ledger_name(again, 1), "a") &&
                same_text(note_ledger_name(again, 2), "b"),
            "parsing the written document gives the same order");
    note_ledger_destroy(again);
    json_writer_destroy(writer);
    note_ledger_destroy(moved);
    note_ledger_destroy(ledger);
}

static void verify_note_moved(void)
{
    char order[4] = {0};
    moved_notes(0, 2, order);
    require(same_text(order, "bca"), "the first note moves to the end");
    moved_notes(2, 0, order);
    require(same_text(order, "cab"), "the last note moves to the front");
    moved_notes(1, 0, order);
    require(same_text(order, "bac"), "one step up");
    moved_notes(1, 2, order);
    require(same_text(order, "acb"), "one step down");
    moved_notes(0, 0, order);
    require(same_text(order, "abc"), "the same position is a copy");
    verify_note_round_trip();
}

/* "a"/"b"/"c" の index に name を挿し、名前を並べた文字列を返す。 */
static void inserted_notes(size_t index, const char *_Nonnull name, char *_Nonnull out)
{
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\", \"c\"]}");
    struct note_ledger *grown = nullptr;
    require(note_ledger_inserted(ledger, index, name, &grown) == NOTE_LEDGER_ACCEPTED,
            "note inserted");
    require(note_ledger_count(grown) == 4, "inserted grows by one");
    for (size_t position = 0; position < 4; ++position)
    {
        out[position] = note_ledger_name(grown, position)[0];
    }
    out[4] = '\0';
    require(note_ledger_count(ledger) == 3, "the source ledger is untouched");
    note_ledger_destroy(grown);
    note_ledger_destroy(ledger);
}

/* "a"/"b"/"c" から index を除き、名前を並べた文字列を返す。 */
static void removed_notes(size_t index, char *_Nonnull out)
{
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\", \"c\"]}");
    struct note_ledger *shrunk = nullptr;
    require(note_ledger_removed(ledger, index, &shrunk) == NOTE_LEDGER_ACCEPTED, "note removed");
    require(note_ledger_count(shrunk) == 2, "removed shrinks by one");
    for (size_t position = 0; position < 2; ++position)
    {
        out[position] = note_ledger_name(shrunk, position)[0];
    }
    out[2] = '\0';
    require(note_ledger_count(ledger) == 3, "the source ledger is untouched");
    note_ledger_destroy(shrunk);
    note_ledger_destroy(ledger);
}

static void verify_note_transfer(void)
{
    char order[8] = {0};
    inserted_notes(0, "d", order);
    require(same_text(order, "dabc"), "inserted at the front");
    inserted_notes(1, "d", order);
    require(same_text(order, "adbc"), "inserted in the middle");
    inserted_notes(3, "d", order);
    require(same_text(order, "abcd"), "the count inserts at the end");
    removed_notes(0, order);
    require(same_text(order, "bc"), "removed the first");
    removed_notes(1, order);
    require(same_text(order, "ac"), "removed the middle");
    removed_notes(2, order);
    require(same_text(order, "ab"), "removed the last");
    struct note_ledger *ledger = parse_notes("{\"version\": 1, \"notes\": [\"a\", \"b\", \"c\"]}");
    struct note_ledger *grown = nullptr;
    require(note_ledger_inserted(ledger, 0, "b", &grown) == NOTE_LEDGER_MALFORMED,
            "a duplicate name is refused");
    require(note_ledger_inserted(ledger, 0, "a/b", &grown) == NOTE_LEDGER_MALFORMED,
            "an invalid name is refused");
    note_ledger_destroy(ledger);
}

void run_ledger_tests(void)
{
    verify_category_parse();
    verify_category_rejections();
    verify_category_write();
    verify_category_reconcile();
    verify_category_toggle();
    verify_category_moved();
    verify_note_parse();
    verify_note_reconcile();
    verify_note_moved();
    verify_note_transfer();
}
