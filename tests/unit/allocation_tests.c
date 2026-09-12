/* 中核のすべての確保を 1 回ずつ失敗させ、OUT_OF_MEMORY を返して片付けることを確かめる。
 * 注入が効くのは eng/coverage.py の測定ビルドだけ（allocation_probe.h）。 */
#include "allocation_probe.h"
#include "appearance_port.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "folio_state.h"
#include "json_reader.h"
#include "json_writer.h"
#include "markdown_rtf.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_text.h"
#include "persistence_port.h"
#include "rtf_palette.h"
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
    struct category_ledger *moved = nullptr;
    completed =
        completed && category_ledger_moved(ledger, 1, 0, &moved) == CATEGORY_LEDGER_ACCEPTED;
    struct rgb_color chosen = {.red = 0x10, .green = 0x20, .blue = 0x30};
    struct category_ledger *recolored = nullptr;
    completed = completed && category_ledger_recolored(ledger, 0, chosen, &recolored) ==
                                 CATEGORY_LEDGER_ACCEPTED;
    category_ledger_destroy(recolored);
    category_ledger_destroy(moved);
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
    struct note_ledger *moved = nullptr;
    completed = completed && note_ledger_moved(ledger, 0, 2, &moved) == NOTE_LEDGER_ACCEPTED;
    note_ledger_destroy(moved);
    note_ledger_destroy(merged);
    name_list_destroy(scanned);
    note_ledger_destroy(ledger);
    return completed;
}

static bool markdown_scenario(void)
{
    const char *source = "# T\n\n**b** `c` [l](u) \xE6\x97\xA5\n- i\n> q\n```\nx\n```\n";
    struct note_text *text = nullptr;
    enum note_text_outcome accepted = note_text_create(source, strlen(source), &text);
    if (accepted == NOTE_TEXT_OUT_OF_MEMORY)
    {
        return false;
    }
    require(accepted == NOTE_TEXT_ACCEPTED, "note under probe");
    struct markdown_rtf *rtf = nullptr;
    enum markdown_rtf_outcome converted =
        markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_DARK), &rtf);
    note_text_destroy(text);
    if (converted == MARKDOWN_RTF_OUT_OF_MEMORY)
    {
        return false;
    }
    require(converted == MARKDOWN_RTF_CONVERTED, "markdown under probe");
    markdown_rtf_destroy(rtf);
    return true;
}

static const struct drawer_metrics probe_metrics = {.top_padding = 1,
                                                    .row_height = 2,
                                                    .category_height = 3,
                                                    .category_gap = 1,
                                                    .category_indent = 3,
                                                    .note_indent = 4};

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
        struct drawer_layout *layout = nullptr;
        completed = drawer_layout_create(categories, per_category, probe_metrics, &layout) ==
                    DRAWER_LAYOUT_CREATED;
        drawer_layout_destroy(layout);
    }
    note_ledger_destroy(notes);
    category_ledger_destroy(categories);
    return completed;
}

/* ノートの居場所を 1 つ作る。 */
static struct note_ref at(size_t category, size_t note)
{
    struct note_ref ref = {.category = category, .note = note};
    return ref;
}

/* 並び替えが作り直す台帳の確保を通す（FR-009）。書き戻しの経路は偽物なので確保しない。 */
static bool reorder_under_probe(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome ordered = folio_state_move_note(state, at(0, 0), at(0, 2));
    require(ordered == FOLIO_STATE_READY || ordered == FOLIO_STATE_OUT_OF_MEMORY,
            "move note under probe");
    if (ordered != FOLIO_STATE_READY)
    {
        return false;
    }
    ordered = folio_state_move_category(state, 0, 1);
    require(ordered == FOLIO_STATE_READY || ordered == FOLIO_STATE_OUT_OF_MEMORY,
            "move category under probe");
    return ordered == FOLIO_STATE_READY;
}

/* 別カテゴリへの移動が作り直す 2 つの台帳（removed / inserted）の確保を通す（ADR 0008）。 */
static bool transfer_under_probe(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome moved = folio_state_move_note(state, at(1, 0), at(0, 1));
    require(moved == FOLIO_STATE_READY || moved == FOLIO_STATE_OUT_OF_MEMORY,
            "transfer note under probe");
    return moved == FOLIO_STATE_READY;
}

/* 変換・畳み込み・書き戻し・表示値の作り直しの確保をすべて通す（FR-006）。 */
static bool edit_under_probe(struct folio_state *_Nonnull state)
{
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "begin edit under probe");
    enum folio_note_change change = FOLIO_NOTE_SAME;
    enum folio_state_outcome compared =
        folio_state_note_changed(state, u"# T\r\nchanged", 12, &change);
    require(compared == FOLIO_STATE_READY || compared == FOLIO_STATE_OUT_OF_MEMORY,
            "compare edit under probe");
    if (compared != FOLIO_STATE_READY)
    {
        return false;
    }
    require(change == FOLIO_NOTE_CHANGED, "changed edit under probe");
    enum folio_state_outcome saved = folio_state_end_edit(state, u"# T\r\nchanged", 12);
    require(saved == FOLIO_STATE_READY || saved == FOLIO_STATE_OUT_OF_MEMORY,
            "end edit under probe");
    return saved == FOLIO_STATE_READY;
}

/* 配置とスクロールの意図が作る配置の確保を通す（ADR 0009 の決定 3）。 */
static bool layout_intent_under_probe(struct folio_state *_Nonnull state)
{
    struct drawer_layout *layout = nullptr;
    bool completed = folio_state_drawer_layout(state, probe_metrics, &layout) == FOLIO_STATE_READY;
    drawer_layout_destroy(layout);
    if (!completed)
    {
        return false;
    }
    enum folio_state_outcome scrolled = folio_state_scroll_drawer(state, probe_metrics, 5);
    require(scrolled == FOLIO_STATE_READY || scrolled == FOLIO_STATE_OUT_OF_MEMORY,
            "scroll under probe");
    return scrolled == FOLIO_STATE_READY;
}

/* トグルと色の変更が複製する台帳の確保を通す（FR-004 / ADR 0010 の決定 1）。 */
static bool ledger_under_probe(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome toggled = folio_state_toggle_category(state, 0);
    require(toggled == FOLIO_STATE_READY || toggled == FOLIO_STATE_OUT_OF_MEMORY,
            "toggle under probe");
    if (toggled != FOLIO_STATE_READY)
    {
        return false;
    }
    struct rgb_color chosen = {.red = 0x10, .green = 0x20, .blue = 0x30};
    enum folio_state_outcome recolored = folio_state_recolor_category(state, 0, chosen);
    require(recolored == FOLIO_STATE_READY || recolored == FOLIO_STATE_OUT_OF_MEMORY,
            "recolor under probe");
    return recolored == FOLIO_STATE_READY;
}

/* 選択・歩み・開閉でのカーソルの移り先・見える位置へ寄せるの確保を通す
 * （FR-005 / FR-018・ADR 0013 の決定 5 / 6・ADR 0015 の決定 2 / 3）。 */
static bool selection_under_probe(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome selected = folio_state_select_note(state, 0, 0);
    require(selected == FOLIO_STATE_READY || selected == FOLIO_STATE_OUT_OF_MEMORY,
            "select under probe");
    if (selected != FOLIO_STATE_READY)
    {
        return false;
    }
    enum folio_state_outcome stepped = folio_state_select_adjacent(state, FOLIO_STEP_LAST);
    require(stepped == FOLIO_STATE_READY || stepped == FOLIO_STATE_OUT_OF_MEMORY,
            "step under probe");
    if (stepped != FOLIO_STATE_READY)
    {
        return false;
    }
    enum folio_state_outcome expanded = folio_state_set_category_expanded(state, 0, true);
    require(expanded == FOLIO_STATE_READY || expanded == FOLIO_STATE_OUT_OF_MEMORY,
            "expand under probe");
    if (expanded != FOLIO_STATE_READY)
    {
        return false;
    }
    enum folio_state_outcome revealed = folio_state_reveal_cursor(state, probe_metrics);
    require(revealed == FOLIO_STATE_READY || revealed == FOLIO_STATE_OUT_OF_MEMORY,
            "reveal under probe");
    return revealed == FOLIO_STATE_READY;
}

/* state を作り、意図を 1 回ずつ通す。adapter は state より長く生きる。 */
static bool state_scenario_with(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = test_adapter_port(adapter);
    struct appearance_port looks = test_appearance_port();
    struct folio_state *state = nullptr;
    enum folio_state_outcome outcome = folio_state_create(&port, &looks, &state);
    if (outcome == FOLIO_STATE_OUT_OF_MEMORY)
    {
        return false;
    }
    require(outcome == FOLIO_STATE_READY, "state under probe");
    bool completed = layout_intent_under_probe(state) && ledger_under_probe(state) &&
                     selection_under_probe(state) && reorder_under_probe(state) &&
                     edit_under_probe(state) && transfer_under_probe(state);
    folio_state_destroy(state);
    return completed;
}

static const char *const second_notes[] = {"four", nullptr};

static bool state_scenario(void)
{
    struct persistence_adapter *adapter = test_adapter_create(categories_text, notes_text);
    /* カテゴリは走査結果（A / B / C）に照合されるので、B だけ別の索引にする。 */
    test_adapter_second_notes(adapter, "B", "{\"version\": 1, \"notes\": [\"four\"]}",
                              second_notes);
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
    exhaust(markdown_scenario, "markdown scenario never completed");
    exhaust(layout_scenario, "layout scenario never completed");
    exhaust(state_scenario, "state scenario never completed");
}
