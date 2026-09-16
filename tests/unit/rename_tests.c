#include "json_reader.h"
#include "json_writer.h"
#include "note_ledger.h"
#include "note_name.h"
#include "note_rename.h"
#include "rename_journal.h"
#include "unit_tests.h"
#include <string.h>

static const char *_Nonnull const file_id = "0123456789abcdef0123456789abcdef0123456789abcdef";
static const char *_Nonnull const history_id = "fedcba9876543210fedcba9876543210fedcba9876543210";

static struct note_rename *_Nonnull plan(void)
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
    struct note_rename *rename = nullptr;
    require(note_rename_create("カテゴリ", ledger, &target, &rename) == NOTE_RENAME_ACCEPTED,
            "prepared rename");
    note_name_destroy(name);
    note_ledger_destroy(ledger);
    return rename;
}

static void verify_plan(void)
{
    struct note_rename *rename = plan();
    require(same_text(note_rename_category(rename), "カテゴリ") &&
                same_text(note_rename_from(rename), "source") &&
                same_text(note_rename_to(rename), "日本語 名前"),
            "plan owns the names after borrowed input is gone");
    const struct note_ledger *ledger = note_rename_ledger(rename);
    require(note_ledger_count(ledger) == 3 && same_text(note_ledger_name(ledger, 0), "one") &&
                same_text(note_ledger_name(ledger, 1), "日本語 名前") &&
                same_text(note_ledger_name(ledger, 2), "three"),
            "rename preserves the exact index position and other names");
    struct note_ledger *taken = note_rename_take_ledger(rename);
    note_rename_destroy(rename);
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
    struct note_rename *rename = nullptr;
    require(note_rename_create("A", ledger, &target, &rename) == NOTE_RENAME_INVALID &&
                rename == nullptr,
            "another existing note cannot be renamed over");
    target.index = 1;
    require(note_rename_create("A", ledger, &target, &rename) == NOTE_RENAME_INVALID,
            "same-name no-op belongs to the application before preparing an intent");
    target.index = 2;
    require(note_rename_create("A", ledger, &target, &rename) == NOTE_RENAME_INVALID,
            "invalid index cannot create an intent");
    target.index = 0;
    require(note_rename_create("../A", ledger, &target, &rename) == NOTE_RENAME_INVALID,
            "category is validated through the canonical name path");
    note_name_destroy(name);
    note_ledger_destroy(ledger);
}

static void round_trip(const struct note_rename *_Nonnull rename, const char *_Nonnull history)
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
    require(note_rename_equals(rename, rename_journal_rename(journal)) &&
                same_text(rename_journal_file_id(journal), file_id) &&
                same_text(rename_journal_history_id(journal), history),
            "decoded journal owns names, ledger and identities");
    rename_journal_destroy(journal);
}

static void verify_journal(void)
{
    struct note_rename *rename = plan();
    round_trip(rename, "");
    round_trip(rename, history_id);
    struct json_writer *writer = nullptr;
    require(json_writer_create(&writer) == JSON_WRITER_ACCEPTED, "invalid-id writer");
    require(rename_journal_write(rename, "", "", writer) == RENAME_JOURNAL_INVALID &&
                rename_journal_write(rename, "not-an-id", "", writer) == RENAME_JOURNAL_INVALID &&
                rename_journal_write(rename, file_id, "missing", writer) == RENAME_JOURNAL_INVALID,
            "only the optional history identity may be empty");
    json_writer_destroy(writer);
    note_rename_destroy(rename);
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
        struct note_rename *rename = nullptr;
        require(json_reader_create(rejected[index], strlen(rejected[index]), &reader) ==
                    JSON_READER_CREATED,
                "bad-plan reader");
        require(note_rename_read(reader, &rename) == NOTE_RENAME_INVALID && rename == nullptr,
                "bad plan never becomes a typed intent");
        json_reader_destroy(reader);
    }
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
    const char *future = "{\"version\":2}";
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
}
