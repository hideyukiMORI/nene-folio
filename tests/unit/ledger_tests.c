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
    require(category_ledger_color(merged, 2).red == 0x8A && category_ledger_expanded(merged, 2),
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

void run_ledger_tests(void)
{
    verify_category_parse();
    verify_category_rejections();
    verify_category_write();
    verify_category_reconcile();
    verify_note_parse();
    verify_note_reconcile();
}
