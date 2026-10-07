#include "category_ledger.h"
#include "category_name.h"
#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_name.h"
#include "rename_journal.h"
#include "rename_plan.h"
#include "unit_tests.h"
#include <string.h>

static const char *_Nonnull const file_id = "0123456789abcdef0123456789abcdef0123456789abcdef";
static const char *_Nonnull const history_id = "fedcba9876543210fedcba9876543210fedcba9876543210";

static struct rename_plan *_Nonnull plan(void)
{
    const char *text = "{\"version\":1,\"notes\":[\"one\",\"source\",\"three\"]}";
    struct note_ledger *ledger = nullptr;
    struct note_name *name = nullptr;
    require(note_ledger_parse(text, strlen(text), &ledger) == NOTE_LEDGER_ACCEPTED,
            "rename ledger");
    require(note_name_create("日本語 名前.md", strlen("日本語 名前.md"), &name) ==
                NOTE_NAME_ACCEPTED,
            "rename target name");
    struct note_rename_target target = {.index = 1, .name = name};
    struct rename_plan *rename = nullptr;
    require(rename_plan_create_note("カテゴリ", ledger, &target, &rename) == RENAME_PLAN_ACCEPTED,
            "prepared rename");
    note_name_destroy(name);
    note_ledger_destroy(ledger);
    return rename;
}

static void verify_plan(void)
{
    struct rename_plan *rename = plan();
    require(same_text(rename_plan_category(rename), "カテゴリ") &&
                same_text(rename_plan_from(rename), "source") &&
                same_text(rename_plan_to(rename), "日本語 名前"),
            "plan owns the names after borrowed input is gone");
    const struct note_ledger *ledger = rename_plan_note_ledger(rename);
    require(note_ledger_count(ledger) == 3 && same_text(note_ledger_name(ledger, 0), "one") &&
                same_text(note_ledger_name(ledger, 1), "日本語 名前") &&
                same_text(note_ledger_name(ledger, 2), "three"),
            "rename preserves the exact index position and other names");
    struct note_ledger *taken = rename_plan_take_note_ledger(rename);
    rename_plan_destroy(rename);
    require(same_text(note_ledger_name(taken, 1), "日本語 名前"),
            "completed ledger ownership transfers");
    note_ledger_destroy(taken);
}

static void verify_refusals(void)
{
    const char *text = "{\"version\":1,\"notes\":[\"one\",\"two\"]}";
    struct note_ledger *ledger = nullptr;
    struct note_name *name = nullptr;
    require(note_ledger_parse(text, strlen(text), &ledger) == NOTE_LEDGER_ACCEPTED,
            "source ledger");
    require(note_name_create("two", 3, &name) == NOTE_NAME_ACCEPTED, "collision name");
    struct note_rename_target target = {.index = 0, .name = name};
    struct rename_plan *rename = nullptr;
    require(rename_plan_create_note("A", ledger, &target, &rename) == RENAME_PLAN_INVALID &&
                rename == nullptr,
            "another existing note cannot be renamed over");
    target.index = 1;
    require(rename_plan_create_note("A", ledger, &target, &rename) == RENAME_PLAN_INVALID,
            "same-name no-op belongs to the application before preparing an intent");
    target.index = 2;
    require(rename_plan_create_note("A", ledger, &target, &rename) == RENAME_PLAN_INVALID,
            "invalid index cannot create an intent");
    target.index = 0;
    require(rename_plan_create_note("../A", ledger, &target, &rename) == RENAME_PLAN_INVALID,
            "category is validated through the canonical name path");
    note_name_destroy(name);
    note_ledger_destroy(ledger);
}

static void round_trip(const struct rename_plan *_Nonnull rename, const char *_Nonnull history)
{
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "journal writer");
    require(rename_journal_write(rename, file_id, history, writer) == RENAME_JOURNAL_ACCEPTED,
            "complete versioned journal");
    struct rename_journal *journal = nullptr;
    require(rename_journal_parse(json_writer_text(writer), json_writer_length(writer), &journal) ==
                RENAME_JOURNAL_ACCEPTED,
            "journal round trip");
    json_writer_destroy(writer);
    require(rename_plan_equals(rename, rename_journal_rename(journal)) &&
                same_text(rename_journal_file_id(journal), file_id) &&
                same_text(rename_journal_history_id(journal), history),
            "decoded journal owns names, ledger and identities");
    rename_journal_destroy(journal);
}

static void verify_journal(void)
{
    struct rename_plan *rename = plan();
    round_trip(rename, "");
    round_trip(rename, history_id);
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "invalid-id writer");
    require(rename_journal_write(rename, "", "", writer) == RENAME_JOURNAL_INVALID &&
                rename_journal_write(rename, "not-an-id", "", writer) == RENAME_JOURNAL_INVALID &&
                rename_journal_write(rename, file_id, "missing", writer) == RENAME_JOURNAL_INVALID,
            "only the optional history identity may be empty");
    json_writer_destroy(writer);
    rename_plan_destroy(rename);
}

static void verify_bad_plans(void)
{
    static const char *const rejected[] = {
        "null",
        "{}",
        "{\"category\":\"../"
        "A\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":1,\"notes\":[\"new\"]}}",
        "{\"category\":\"A\",\"from\":\"old\",\"to\":\"old\",\"ledger\":{\"version\":1,\"notes\":["
        "\"old\"]}}",
        "{\"category\":\"A\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":1,\"notes\":["
        "\"old\",\"new\"]}}",
        "{\"category\":\"A\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":1,\"notes\":["
        "\"other\"]}}",
        "{\"category\":\"A\",\"from\":\"old\",\"to\":\"NUL\",\"ledger\":{\"version\":1,\"notes\":["
        "\"NUL\"]}}",
        "{\"category\":\"A\\u0000bad\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":1,"
        "\"notes\":[\"new\"]}}",
        "{\"category\\u0000bad\":\"A\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":1,"
        "\"notes\":[\"new\"]}}",
        "{\"category\":\"A\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":2,\"notes\":["
        "\"new\"]}}",
        "{\"category\":\"A\",\"from\":\"old\",\"to\":\"new\",\"ledger\":{\"version\":1,\"notes\":["
        "\"new\"]},\"extra\":0}"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        struct json_reader *reader = nullptr;
        struct rename_plan *rename = nullptr;
        require(json_reader_create(rejected[index], strlen(rejected[index]), &reader) ==
                    JSON_READER_CREATED,
                "bad-plan reader");
        require(rename_plan_read(reader, RENAME_KIND_NOTE, &rename) == RENAME_PLAN_INVALID &&
                    rename == nullptr,
                "bad plan never becomes a typed intent");
        json_reader_destroy(reader);
    }
}

static const char *const category_source =
    "{\"version\":1,\"categories\":["
    "{\"name\":\"A\",\"color\":\"#123456\",\"expanded\":true},"
    "{\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},"
    "{\"name\":\"C\",\"color\":\"#998877\",\"expanded\":true}]}";

static struct rename_plan *_Nonnull category_plan(const char *_Nonnull text,
                                                  const char *_Nonnull name)
{
    struct category_ledger *ledger = nullptr;
    struct category_name *target = nullptr;
    struct rename_plan *rename = nullptr;
    require(category_ledger_parse(text, strlen(text), &ledger) == CATEGORY_LEDGER_ACCEPTED &&
                category_name_create(name, strlen(name), &target) == CATEGORY_NAME_ACCEPTED,
            "category plan inputs");
    require(rename_plan_create_category(ledger, 1, target, &rename) == RENAME_PLAN_ACCEPTED,
            "category plan prepared");
    require(same_text(category_ledger_name(ledger, 1), "old"), "source category immutable");
    category_name_destroy(target);
    category_ledger_destroy(ledger);
    return rename;
}

static void verify_category_plan(void)
{
    struct rename_plan *rename = category_plan(category_source, "日本語.md");
    require(rename_plan_kind(rename) == RENAME_KIND_CATEGORY &&
                same_text(rename_plan_from(rename), "old") &&
                same_text(rename_plan_to(rename), "日本語.md"),
            "category has no note extension normalization");
    const struct category_ledger *ledger = rename_plan_category_ledger(rename);
    const char *const names[] = {"A", "日本語.md", "C"};
    const struct rgb_color colors[] = {{0x12, 0x34, 0x56}, {0xAB, 0xCD, 0xEF}, {0x99, 0x88, 0x77}};
    require(category_ledger_count(ledger) == 3, "category row count preserved");
    for (size_t index = 0; index < 3; ++index)
    {
        struct rgb_color color = category_ledger_color(ledger, index);
        require(same_text(category_ledger_name(ledger, index), names[index]) &&
                    color.red == colors[index].red && color.green == colors[index].green &&
                    color.blue == colors[index].blue &&
                    category_ledger_expanded(ledger, index) == (index != 1),
                "every row retains its position and metadata");
    }
    round_trip(rename, "");
    round_trip(rename, file_id);
    struct category_ledger *taken = rename_plan_take_category_ledger(rename);
    rename_plan_destroy(rename);
    require(same_text(category_ledger_name(taken, 1), "日本語.md"),
            "typed category transfer survives owner");
    category_ledger_destroy(taken);
}

static void verify_category_refusals(void)
{
    struct category_ledger *ledger = nullptr;
    struct category_name *name = nullptr;
    require(category_ledger_parse(category_source, strlen(category_source), &ledger) ==
                    CATEGORY_LEDGER_ACCEPTED &&
                category_name_create("A", 1, &name) == CATEGORY_NAME_ACCEPTED,
            "category refusal inputs");
    struct category_ledger *out = ledger;
    require(category_ledger_renamed(ledger, 3, name, &out) == CATEGORY_LEDGER_MALFORMED &&
                out == ledger,
            "out of range leaves nonnull ledger output unchanged");
    require(category_ledger_renamed(ledger, 1, name, &out) == CATEGORY_LEDGER_MALFORMED &&
                out == ledger,
            "duplicate rename leaves nonnull ledger output unchanged");
    struct rename_plan *sentinel = plan();
    struct rename_plan *rename = sentinel;
    require(rename_plan_create_category(ledger, 3, name, &rename) == RENAME_PLAN_INVALID &&
                rename == sentinel,
            "plan range failure preserves nonnull output");
    require(rename_plan_create_category(ledger, 1, name, &rename) == RENAME_PLAN_INVALID &&
                rename == sentinel,
            "plan duplicate failure preserves nonnull output");
    category_name_destroy(name);
    require(category_name_create("old", 3, &name) == CATEGORY_NAME_ACCEPTED, "same-name input");
    require(category_ledger_renamed(ledger, 1, name, &out) == CATEGORY_LEDGER_ACCEPTED &&
                out != ledger,
            "ledger same name owns a separate copy");
    category_ledger_destroy(out);
    require(rename_plan_create_category(ledger, 1, name, &rename) == RENAME_PLAN_INVALID &&
                rename == sentinel,
            "same-name plan has no ambiguous old/new names");
    rename_plan_destroy(sentinel);
    category_name_destroy(name);
    category_ledger_destroy(ledger);
    char long_name[name_list_max_length + 1];
    memset(long_name, 'x', sizeof long_name - 1);
    long_name[sizeof long_name - 1] = '\0';
    rename = category_plan(category_source, long_name);
    round_trip(rename, "");
    rename_plan_destroy(rename);
}

static void verify_category_equality(void)
{
    static const char *const unequal[] = {
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#123457\",\"expanded\":true},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},{\"name\":\"C\",\"color\":\"#"
        "998877\",\"expanded\":true}]}",
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#123556\",\"expanded\":true},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},{\"name\":\"C\",\"color\":\"#"
        "998877\",\"expanded\":true}]}",
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#133456\",\"expanded\":true},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},{\"name\":\"C\",\"color\":\"#"
        "998877\",\"expanded\":true}]}",
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#123456\",\"expanded\":false},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},{\"name\":\"C\",\"color\":\"#"
        "998877\",\"expanded\":true}]}",
        "{\"version\":1,\"categories\":[{\"name\":\"C\",\"color\":\"#998877\",\"expanded\":true},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},{\"name\":\"A\",\"color\":\"#"
        "123456\",\"expanded\":true}]}",
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#123456\",\"expanded\":true},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false}]}",
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#123456\",\"expanded\":true},{"
        "\"name\":\"old\",\"color\":\"#ABCDEF\",\"expanded\":false},{\"name\":\"different\","
        "\"color\":\"#998877\",\"expanded\":true}]}"};
    struct rename_plan *left = category_plan(category_source, "new");
    struct rename_plan *right = category_plan(category_source, "new");
    require(rename_plan_equals(left, right), "separately owned identical category plans agree");
    rename_plan_destroy(right);
    for (size_t index = 0; index < sizeof unequal / sizeof unequal[0]; ++index)
    {
        right = category_plan(unequal[index], "new");
        require(!rename_plan_equals(left, right),
                "same from/to never hides metadata, order or rows");
        rename_plan_destroy(right);
    }
    right = category_plan(category_source, "different");
    require(!rename_plan_equals(left, right), "different new category disagrees");
    rename_plan_destroy(right);
    right = plan();
    require(rename_plan_kind(right) == RENAME_KIND_NOTE && !rename_plan_equals(left, right),
            "different kinds never compare typed ledgers");
    rename_plan_destroy(right);
    rename_plan_destroy(left);
}

static void verify_composed_category_read(void)
{
    const char *text = "{\"ledger\":{\"version\":1,\"categories\":[]},\"after\":7}";
    struct json_reader *reader = nullptr;
    struct category_ledger *ledger = nullptr;
    require(json_reader_create(text, strlen(text), &reader) == JSON_READER_CREATED &&
                json_reader_next(reader) == JSON_TOKEN_OBJECT_BEGIN &&
                json_reader_next(reader) == JSON_TOKEN_KEY,
            "composed category reader");
    require(category_ledger_read(reader, &ledger) == CATEGORY_LEDGER_ACCEPTED &&
                category_ledger_count(ledger) == 0 && json_reader_next(reader) == JSON_TOKEN_KEY &&
                same_text(json_reader_text(reader), "after") &&
                json_reader_next(reader) == JSON_TOKEN_UNSIGNED &&
                json_reader_unsigned(reader) == 7,
            "composed read stops at its own object boundary");
    json_reader_destroy(reader);
    struct category_ledger *out = ledger;
    const char *extra = "{\"version\":1,\"categories\":[]}{}";
    require(category_ledger_parse(extra, strlen(extra), &out) == CATEGORY_LEDGER_MALFORMED &&
                out == ledger,
            "outer parse alone rejects trailing data and preserves output");
    const char *bad_key = "{\"version\\u0000bad\":1,\"categories\":[]}";
    require(category_ledger_parse(bad_key, strlen(bad_key), &out) == CATEGORY_LEDGER_MALFORMED &&
                out == ledger,
            "embedded NUL key cannot masquerade as version");
    category_ledger_destroy(ledger);
}

static void verify_v1_read(void)
{
    const char *text = "{\"version\":1,\"rename\":{\"category\":\"A\",\"from\":\"old\",\"to\":"
                       "\"new\",\"ledger\":{\"version\":1,\"notes\":[\"new\"]}},\"fileId\":"
                       "\"0123456789abcdef0123456789abcdef0123456789abcdef\",\"historyId\":\"\"}";
    struct rename_journal *journal = nullptr;
    require(rename_journal_parse(text, strlen(text), &journal) == RENAME_JOURNAL_ACCEPTED &&
                rename_plan_kind(rename_journal_rename(journal)) == RENAME_KIND_NOTE &&
                same_text(rename_plan_category(rename_journal_rename(journal)), "A") &&
                same_text(rename_plan_from(rename_journal_rename(journal)), "old") &&
                same_text(rename_plan_to(rename_journal_rename(journal)), "new"),
            "v1 normalizes to NOTE");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "v1 to v2 writer");
    require(rename_journal_write(rename_journal_rename(journal), file_id, "", writer) ==
                    RENAME_JOURNAL_ACCEPTED &&
                strstr(json_writer_text(writer), "\"version\": 2") != nullptr &&
                strstr(json_writer_text(writer), "\"kind\": \"note\"") != nullptr,
            "new writes use v2 even for a normalized NOTE");
    json_writer_destroy(writer);
    rename_journal_destroy(journal);
}

static void verify_v2_refusals(void)
{
    static const char *const rejected[] = {
        "{\"version\":2}",
        "{\"version\":2,\"kind\":\"unknown\"}",
        "{\"version\":2,\"kind\":\"note\\u0000bad\"}",
        "{\"version\":2,\"kind\":true}",
        "{\"version\":2,\"kind\":\"category\",\"rename\":{\"category\":\"A\",\"from\":\"old\","
        "\"to\":\"new\",\"ledger\":{\"version\":1,\"notes\":[\"new\"]}}}",
        "{\"version\":2,\"kind\":\"note\",\"rename\":{\"from\":\"old\",\"to\":\"new\",\"ledger\":{"
        "\"version\":1,\"categories\":[]}}}",
        "{\"version\":2,\"kind\":\"category\",\"rename\":{\"from\":\"old\",\"to\":\"new\","
        "\"ledger\":{\"version\":1,\"notes\":[\"new\"]}}}",
        "{\"version\":2,\"kind\":\"category\",\"rename\":{\"from\":\"old\",\"to\":\"new\","
        "\"ledger\":{\"version\":1,\"categories\":[]}}}",
        "{\"version\":2,\"kind\":\"category\",\"rename\":{\"from\":\"old\",\"to\":\"new\","
        "\"ledger\":{\"version\":1,\"categories\":[{\"name\":\"old\",\"color\":\"#123456\","
        "\"expanded\":true}]}}}",
        "{\"version\":2,\"kind\":\"category\",\"rename\":{\"from\":\"old\",\"to\":\".history\","
        "\"ledger\":{\"version\":1,\"categories\":[]}}}",
        "{\"version\":2,\"kind\":\"category\",\"rename\":{\"from\":\"old\",\"to\":\"categories."
        "json\",\"ledger\":{\"version\":1,\"categories\":[]}}}",
        "{\"version\":1,\"kind\":\"note\",\"rename\":{}}",
        "{\"version\":2,\"kind\":\"note\",\"kind\":\"note\"}",
        "{\"version\":2,\"kind\":\"note\",\"extra\":0,\"rename\":{}}"};
    struct rename_journal *sentinel = nullptr;
    const char *valid = "{\"version\":1,\"rename\":{\"category\":\"A\",\"from\":\"old\",\"to\":"
                        "\"new\",\"ledger\":{\"version\":1,\"notes\":[\"new\"]}},\"fileId\":"
                        "\"0123456789abcdef0123456789abcdef0123456789abcdef\",\"historyId\":\"\"}";
    require(rename_journal_parse(valid, strlen(valid), &sentinel) == RENAME_JOURNAL_ACCEPTED,
            "nonnull sentinel journal");
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        struct rename_journal *out = sentinel;
        require(rename_journal_parse(rejected[index], strlen(rejected[index]), &out) ==
                        RENAME_JOURNAL_INVALID &&
                    out == sentinel,
                "kind/payload/extra keys reject with nonnull output unchanged");
    }
    rename_journal_destroy(sentinel);
}

static void reject_changed_journal(const char *_Nonnull text, const char *_Nonnull old,
                                   const char *_Nonnull replacement)
{
    const char *at = strstr(text, old);
    require(at != nullptr, "mutation locates a real serialized field");
    size_t prefix = (size_t)(at - text);
    size_t inserted = strlen(replacement);
    const char *tail = at + strlen(old);
    char changed[4096];
    require(prefix + inserted + strlen(tail) < sizeof changed, "bounded journal mutation");
    memcpy(changed, text, prefix);
    memcpy(changed + prefix, replacement, inserted);
    memcpy(changed + prefix + inserted, tail, strlen(tail) + 1);
    struct rename_journal *journal = nullptr;
    require(rename_journal_parse(changed, strlen(changed), &journal) == RENAME_JOURNAL_INVALID &&
                journal == nullptr,
            "complete mutated record is strictly refused");
}

static void verify_complete_v2_rejections(void)
{
    struct rename_plan *rename = category_plan(category_source, "new");
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED &&
                rename_journal_write(rename, file_id, "", writer) == RENAME_JOURNAL_ACCEPTED,
            "complete record for strict rejection tests");
    const char *text = json_writer_text(writer);
    reject_changed_journal(text, "\"kind\": \"category\"", "\"kind\": \"note\"");
    reject_changed_journal(text, "\"name\": \"new\"", "\"name\": \"old\"");
    reject_changed_journal(text, "\"name\": \"new\"", "\"name\": \"absent\"");
    reject_changed_journal(text, "\"name\": \"A\"", "\"name\": \"new\"");
    reject_changed_journal(text, "\"categories\":", "\"categories\\u0000bad\":");
    reject_changed_journal(text, "\"color\":", "\"color\\u0000bad\":");
    reject_changed_journal(text, "\"fileId\": \"0123456789abcdef0123456789abcdef0123456789abcdef\"",
                           "\"fileId\": \"0123456789ABCDEF0123456789abcdef0123456789abcdef\"");
    reject_changed_journal(text, "\"fileId\": \"0123456789abcdef0123456789abcdef0123456789abcdef\"",
                           "\"fileId\": \"\"");
    reject_changed_journal(text, "\"historyId\": \"\"", "\"historyId\": \"bad\"");
    reject_changed_journal(text, "\"historyId\": \"\"", "\"extra\": 0");
    reject_changed_journal(text, "\"historyId\": \"\"", "\"historyId\": \"\", \"extra\": 0");
    reject_changed_journal(text, "\"ledger\":", "\"extra\": 0, \"ledger\":");
    reject_changed_journal(text, "\"expanded\": false", "\"expanded\": null");
    json_writer_destroy(writer);
    rename_plan_destroy(rename);
}

static void verify_bad_journals(void)
{
    static const char *const rejected[] = {"",
                                           "null",
                                           "{}",
                                           "{\"version\":1}",
                                           "{\"version\":1,\"rename\":null}",
                                           "{\"version\\u0000bad\":1}"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        struct rename_journal *journal = nullptr;
        require(rename_journal_parse(rejected[index], strlen(rejected[index]), &journal) ==
                        RENAME_JOURNAL_INVALID &&
                    journal == nullptr,
                "bad journal is refused");
    }
    const char *future = "{\"version\":3}";
    struct rename_journal *journal = nullptr;
    require(rename_journal_parse(future, strlen(future), &journal) ==
                RENAME_JOURNAL_UNSUPPORTED_VERSION,
            "unknown version is not silently recovered");
    const char *bad_key = "{\"version\":1,\"notes\\u0000bad\":[\"a\"]}";
    struct note_ledger *ledger = nullptr;
    require(note_ledger_parse(bad_key, strlen(bad_key), &ledger) == NOTE_LEDGER_MALFORMED,
            "nested ledger codec rejects a key with an embedded NUL suffix");
}

void run_rename_tests(void)
{
    verify_plan();
    verify_refusals();
    verify_journal();
    verify_bad_plans();
    verify_bad_journals();
    verify_category_plan();
    verify_category_refusals();
    verify_category_equality();
    verify_composed_category_read();
    verify_v1_read();
    verify_v2_refusals();
    verify_complete_v2_rejections();
}
