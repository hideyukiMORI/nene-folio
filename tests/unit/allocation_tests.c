/* 中核のすべての確保を 1 回ずつ失敗させ、OUT_OF_MEMORY を返して片付けることを確かめる。
 * 注入が効くのは eng/coverage.py の測定ビルドだけ（allocation_probe.h）。 */
#include "allocation_probe.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "folio_state.h"
#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_ledger.h"
#include "persistence_port.h"
#include "unit_tests.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <stdio.h>
#include <string.h>

static const char *const categories_text =
    "{\"version\": 1, \"categories\": ["
    "{\"name\": \"alpha\", \"color\": \"#112233\", \"expanded\": true},"
    "{\"name\": \"beta\", \"color\": \"#445566\", \"expanded\": false}]}";
static const char *const notes_text = "{\"version\": 1, \"notes\": [\"one\", \"two\", \"three\"]}";

/* 各シナリオは、完了したら true、記憶不足を正しく報告して片付けたら false を返す。
 * それ以外の結果は require で止める。 */

static bool text_scenario(void)
{
    struct utf16_text *wide = nullptr;
    enum utf16_text_outcome widened = utf16_text_create("\xE6\x97\xA5\xE6\x9C\xAC", 6, &wide);
    if (widened == UTF16_TEXT_OUT_OF_MEMORY)
    {
        return false;
    }
    require(widened == UTF16_TEXT_CONVERTED, "utf16 under probe");
    struct utf8_text *narrow = nullptr;
    enum utf8_text_outcome narrowed =
        utf8_text_create(utf16_text_units(wide), utf16_text_length(wide), &narrow);
    utf16_text_destroy(wide);
    if (narrowed == UTF8_TEXT_OUT_OF_MEMORY)
    {
        return false;
    }
    require(narrowed == UTF8_TEXT_CONVERTED, "utf8 under probe");
    utf8_text_destroy(narrow);
    return true;
}

static bool reader_scenario(void)
{
    char text[400];
    memset(text, 'x', sizeof text);
    text[0] = '[';
    text[1] = '"';
    text[300] = '"';
    text[301] = ']';
    text[302] = '\0';
    struct json_reader *reader = nullptr;
    if (json_reader_create(text, strlen(text), &reader) == JSON_READER_OUT_OF_MEMORY)
    {
        return false;
    }
    for (;;)
    {
        enum json_token token = json_reader_next(reader);
        if (token == JSON_TOKEN_OUT_OF_MEMORY)
        {
            json_reader_destroy(reader);
            return false;
        }
        require(token != JSON_TOKEN_MALFORMED, "reader under probe");
        if (token == JSON_TOKEN_END)
        {
            json_reader_destroy(reader);
            return true;
        }
    }
}

static bool writer_scenario(void)
{
    struct json_writer *writer = nullptr;
    if (json_writer_create(&writer) == JSON_WRITER_OUT_OF_MEMORY)
    {
        return false;
    }
    json_writer_array_begin(writer);
    for (size_t index = 0; index < 64; ++index)
    {
        json_writer_string(writer, "0123456789");
    }
    json_writer_array_end(writer);
    enum json_writer_outcome outcome = json_writer_finish(writer);
    json_writer_destroy(writer);
    if (outcome == JSON_WRITER_OUT_OF_MEMORY)
    {
        return false;
    }
    require(outcome == JSON_WRITER_ACCEPTED, "writer under probe");
    return true;
}

static bool names_scenario(void)
{
    struct name_list *list = nullptr;
    if (name_list_create(&list) == NAME_LIST_OUT_OF_MEMORY)
    {
        return false;
    }
    for (size_t index = 0; index < 6; ++index)
    {
        char name[2] = {(char)('a' + index), '\0'};
        enum name_list_outcome outcome = name_list_append(list, name, 1);
        if (outcome == NAME_LIST_OUT_OF_MEMORY)
        {
            name_list_destroy(list);
            return false;
        }
        require(outcome == NAME_LIST_ACCEPTED, "names under probe");
    }
    name_list_destroy(list);
    return true;
}

static bool scanned_names(const char *const *_Nonnull items, size_t count,
                          struct name_list *_Nullable *_Nonnull out)
{
    if (name_list_create(out) == NAME_LIST_OUT_OF_MEMORY)
    {
        return false;
    }
    for (size_t index = 0; index < count; ++index)
    {
        if (name_list_append(*out, items[index], strlen(items[index])) == NAME_LIST_OUT_OF_MEMORY)
        {
            name_list_destroy(*out);
            *out = nullptr;
            return false;
        }
    }
    return true;
}

static bool categories_scenario(void)
{
    struct category_ledger *ledger = nullptr;
    enum category_ledger_outcome parsed =
        category_ledger_parse(categories_text, strlen(categories_text), &ledger);
    if (parsed == CATEGORY_LEDGER_OUT_OF_MEMORY)
    {
        return false;
    }
    require(parsed == CATEGORY_LEDGER_ACCEPTED, "categories under probe");
    struct json_writer *writer = nullptr;
    bool completed = json_writer_create(&writer) == JSON_WRITER_ACCEPTED;
    if (completed)
    {
        category_ledger_write(ledger, writer);
        completed = json_writer_finish(writer) == JSON_WRITER_ACCEPTED;
        json_writer_destroy(writer);
    }
    const char *const items[] = {"beta", "gamma", "delta"};
    struct name_list *scanned = nullptr;
    struct category_ledger *merged = nullptr;
    completed = completed && scanned_names(items, 3, &scanned) &&
                category_ledger_reconcile(ledger, scanned, &merged) == CATEGORY_LEDGER_ACCEPTED;
    category_ledger_destroy(merged);
    name_list_destroy(scanned);
    category_ledger_destroy(ledger);
    return completed;
}

static bool notes_scenario(void)
{
    struct note_ledger *ledger = nullptr;
    enum note_ledger_outcome parsed = note_ledger_parse(notes_text, strlen(notes_text), &ledger);
    if (parsed == NOTE_LEDGER_OUT_OF_MEMORY)
    {
        return false;
    }
    require(parsed == NOTE_LEDGER_ACCEPTED, "notes under probe");
    struct json_writer *writer = nullptr;
    bool completed = json_writer_create(&writer) == JSON_WRITER_ACCEPTED;
    if (completed)
    {
        note_ledger_write(ledger, writer);
        completed = json_writer_finish(writer) == JSON_WRITER_ACCEPTED;
        json_writer_destroy(writer);
    }
    const char *const items[] = {"three", "four", "one"};
    struct name_list *scanned = nullptr;
    struct note_ledger *merged = nullptr;
    completed = completed && scanned_names(items, 3, &scanned) &&
                note_ledger_reconcile(ledger, scanned, &merged) == NOTE_LEDGER_ACCEPTED;
    note_ledger_destroy(merged);
    name_list_destroy(scanned);
    note_ledger_destroy(ledger);
    return completed;
}

static bool layout_scenario(void)
{
    struct category_ledger *categories = nullptr;
    struct note_ledger *notes = nullptr;
    bool completed =
        category_ledger_parse(categories_text, strlen(categories_text), &categories) ==
            CATEGORY_LEDGER_ACCEPTED &&
        note_ledger_parse(notes_text, strlen(notes_text), &notes) == NOTE_LEDGER_ACCEPTED;
    if (completed)
    {
        const struct note_ledger *const per_category[] = {notes, notes};
        struct drawer_metrics metrics = {
            .top_padding = 1, .row_height = 2, .category_indent = 3, .note_indent = 4};
        struct drawer_layout *layout = nullptr;
        completed = drawer_layout_create(categories, per_category, metrics, &layout) ==
                    DRAWER_LAYOUT_CREATED;
        drawer_layout_destroy(layout);
    }
    note_ledger_destroy(notes);
    category_ledger_destroy(categories);
    return completed;
}

/* state を作り、配置とトグルを 1 回ずつ通す。adapter は state より長く生きる。 */
static bool state_scenario_with(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = test_adapter_port(adapter);
    struct folio_state *state = nullptr;
    enum folio_state_outcome outcome = folio_state_create(&port, &state);
    if (outcome == FOLIO_STATE_OUT_OF_MEMORY)
    {
        return false;
    }
    require(outcome == FOLIO_STATE_READY, "state under probe");
    struct drawer_metrics metrics = {
        .top_padding = 1, .row_height = 2, .category_indent = 3, .note_indent = 4};
    struct drawer_layout *layout = nullptr;
    bool completed = folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY;
    drawer_layout_destroy(layout);
    if (completed)
    {
        enum folio_state_outcome toggled = folio_state_toggle_category(state, 0);
        completed = toggled == FOLIO_STATE_READY;
        require(completed || toggled == FOLIO_STATE_OUT_OF_MEMORY, "toggle under probe");
    }
    folio_state_destroy(state);
    return completed;
}

static bool state_scenario(void)
{
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    bool completed = state_scenario_with(adapter);
    test_adapter_destroy(adapter);
    return completed;
}

/* 1 回目・2 回目・… の確保を順に失敗させ、シナリオが完了するまで続ける。 */
static void exhaust(bool (*_Nonnull scenario)(void), const char *_Nonnull description)
{
    for (size_t nth = 1; nth < 4096; ++nth)
    {
        allocation_probe_fail_at(nth);
        bool completed = scenario();
        allocation_probe_fail_at(0);
        if (completed)
        {
            return;
        }
    }
    require(false, description);
}

/* 注入が効いているか。正典のビルドでは中核が本物の CRT を呼ぶので効かない。 */
static bool probe_active(void)
{
    allocation_probe_fail_at(1);
    struct name_list *list = nullptr;
    enum name_list_outcome outcome = name_list_create(&list);
    allocation_probe_fail_at(0);
    name_list_destroy(list);
    return outcome == NAME_LIST_OUT_OF_MEMORY;
}

void run_allocation_tests(void)
{
    if (!probe_active())
    {
        printf("allocation probe inactive: canonical build calls the real CRT\n");
        return;
    }
    exhaust(text_scenario, "text scenario never completed");
    exhaust(reader_scenario, "reader scenario never completed");
    exhaust(writer_scenario, "writer scenario never completed");
    exhaust(names_scenario, "names scenario never completed");
    exhaust(categories_scenario, "categories scenario never completed");
    exhaust(notes_scenario, "notes scenario never completed");
    exhaust(layout_scenario, "layout scenario never completed");
    exhaust(state_scenario, "state scenario never completed");
}
