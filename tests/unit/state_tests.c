#include "appearance_port.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "folio_state.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_text.h"
#include "persistence_port.h"
#include "unit_tests.h"

#include <stdlib.h>
#include <string.h>

/* テスト用のポート実装。application が不完全型として知る persistence_adapter をここで定義する。 */
struct persistence_adapter
{
    enum persistence_outcome categories_outcome;
    const char *_Nonnull categories_text;
    enum persistence_outcome scan_outcome;
    const char *_Nonnull const *_Nullable scanned_categories;
    size_t scanned_category_count;
    enum persistence_outcome notes_outcome;
    const char *_Nonnull notes_text;
    enum persistence_outcome notes_scan_outcome;
    const char *_Nonnull const *_Nullable scanned_notes;
    size_t scanned_note_count;
    size_t note_scans; /* scan_notes が呼ばれた回数 */
    enum persistence_outcome note_outcome;
    const char *_Nonnull note_body;
    const char *_Nullable last_note; /* 最後に read_note で求められたノート名 */
    enum persistence_outcome write_outcome;
    size_t writes;            /* write_category_ledger が呼ばれた回数 */
    size_t written_count;     /* 最後に書かれた台帳のカテゴリ数 */
    bool written_expanded[8]; /* 最後に書かれた台帳の展開状態 */
    enum persistence_outcome note_write_outcome;
    size_t note_writes;                 /* write_note が呼ばれた回数 */
    char written_body[256];             /* 最後に書かれた本文（終端付き） */
    const char *_Nullable written_note; /* 最後に書かれたノート名 */
    const char *_Nullable written_category;
};

static enum persistence_outcome list_names(const char *_Nonnull const *_Nullable items,
                                           size_t count, struct name_list *_Nullable *_Nonnull out)
{
    struct name_list *list = nullptr;
    if (name_list_create(&list) != NAME_LIST_ACCEPTED)
    {
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < count; ++index)
    {
        enum name_list_outcome outcome = name_list_append(list, items[index], strlen(items[index]));
        if (outcome != NAME_LIST_ACCEPTED)
        {
            name_list_destroy(list);
            return outcome == NAME_LIST_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY
                                                      : PERSISTENCE_MALFORMED;
        }
    }
    *out = list;
    return PERSISTENCE_LOADED;
}

static enum persistence_outcome fake_scan_categories(struct persistence_adapter *_Nonnull adapter,
                                                     struct name_list *_Nullable *_Nonnull out)
{
    if (adapter->scan_outcome != PERSISTENCE_LOADED)
    {
        return adapter->scan_outcome;
    }
    return list_names(adapter->scanned_categories, adapter->scanned_category_count, out);
}

static enum persistence_outcome fake_scan_notes(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category,
                                                struct name_list *_Nullable *_Nonnull out)
{
    (void)category;
    adapter->note_scans += 1;
    if (adapter->notes_scan_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_scan_outcome;
    }
    return list_names(adapter->scanned_notes, adapter->scanned_note_count, out);
}

static enum persistence_outcome
fake_read_category_ledger(struct persistence_adapter *_Nonnull adapter,
                          struct category_ledger *_Nullable *_Nonnull out)
{
    if (adapter->categories_outcome != PERSISTENCE_LOADED)
    {
        return adapter->categories_outcome;
    }
    enum category_ledger_outcome parsed =
        category_ledger_parse(adapter->categories_text, strlen(adapter->categories_text), out);
    if (parsed == CATEGORY_LEDGER_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return parsed == CATEGORY_LEDGER_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY
                                                   : PERSISTENCE_MALFORMED;
}

static enum persistence_outcome fake_read_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                      const char *_Nonnull category,
                                                      struct note_ledger *_Nullable *_Nonnull out)
{
    (void)category;
    if (adapter->notes_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_outcome;
    }
    enum note_ledger_outcome parsed =
        note_ledger_parse(adapter->notes_text, strlen(adapter->notes_text), out);
    if (parsed == NOTE_LEDGER_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return parsed == NOTE_LEDGER_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY : PERSISTENCE_MALFORMED;
}

static enum persistence_outcome
fake_write_category_ledger(struct persistence_adapter *_Nonnull adapter,
                           const struct category_ledger *_Nonnull ledger)
{
    adapter->writes += 1;
    if (adapter->write_outcome != PERSISTENCE_STORED)
    {
        return adapter->write_outcome;
    }
    adapter->written_count = category_ledger_count(ledger);
    for (size_t index = 0; index < adapter->written_count && index < 8; ++index)
    {
        adapter->written_expanded[index] = category_ledger_expanded(ledger, index);
    }
    return PERSISTENCE_STORED;
}

static enum persistence_outcome fake_read_note(struct persistence_adapter *_Nonnull adapter,
                                               const char *_Nonnull category,
                                               const char *_Nonnull note,
                                               struct note_text *_Nullable *_Nonnull out)
{
    (void)category;
    adapter->last_note = note;
    if (adapter->note_outcome != PERSISTENCE_LOADED)
    {
        return adapter->note_outcome;
    }
    enum note_text_outcome accepted =
        note_text_create(adapter->note_body, strlen(adapter->note_body), out);
    if (accepted == NOTE_TEXT_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return accepted == NOTE_TEXT_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY : PERSISTENCE_MALFORMED;
}

static enum persistence_outcome fake_write_note(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category,
                                                const char *_Nonnull note,
                                                const struct note_text *_Nonnull body)
{
    adapter->note_writes += 1;
    adapter->written_category = category;
    adapter->written_note = note;
    if (adapter->note_write_outcome != PERSISTENCE_STORED)
    {
        return adapter->note_write_outcome;
    }
    size_t length = note_text_length(body);
    require(length + 1 < sizeof adapter->written_body, "written body fits the fake");
    memcpy(adapter->written_body, note_text_bytes(body), length + 1);
    return PERSISTENCE_STORED;
}

/* テスト用の外観ポート。application が不完全型として知る appearance_adapter をここで定義する。 */
struct appearance_adapter
{
    enum folio_theme theme;
};

static enum folio_theme fake_read_theme(struct appearance_adapter *_Nonnull adapter)
{
    return adapter->theme;
}

static struct appearance_adapter dark_adapter = {.theme = FOLIO_THEME_DARK};

static struct appearance_port looks_for(struct appearance_adapter *_Nonnull adapter)
{
    struct appearance_port port = {.adapter = adapter, .read_theme = fake_read_theme};
    return port;
}

static const char *const scanned_categories[] = {"A", "B", "C"};
static const char *const scanned_notes[] = {"two", "one", "three"};

static struct persistence_adapter healthy_adapter(void)
{
    struct persistence_adapter adapter = {
        .categories_outcome = PERSISTENCE_LOADED,
        .categories_text = "{\"version\": 1, \"categories\": ["
                           "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": true},"
                           "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": false}]}",
        .scan_outcome = PERSISTENCE_LOADED,
        .scanned_categories = scanned_categories,
        .scanned_category_count = 3,
        .notes_outcome = PERSISTENCE_LOADED,
        .notes_text = "{\"version\": 1, \"notes\": [\"one\", \"two\"]}",
        .notes_scan_outcome = PERSISTENCE_LOADED,
        .scanned_notes = scanned_notes,
        .scanned_note_count = 3,
        .note_scans = 0,
        .note_outcome = PERSISTENCE_LOADED,
        .note_body = "# Hello\n\nbody",
        .last_note = nullptr,
        .write_outcome = PERSISTENCE_STORED,
        .writes = 0,
        .written_count = 0,
        .written_expanded = {false},
        .note_write_outcome = PERSISTENCE_STORED,
        .note_writes = 0,
        .written_body = {'\0'},
        .written_note = nullptr,
        .written_category = nullptr,
    };
    return adapter;
}

static struct persistence_port port_for(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = {
        .adapter = adapter,
        .scan_categories = fake_scan_categories,
        .scan_notes = fake_scan_notes,
        .read_category_ledger = fake_read_category_ledger,
        .write_category_ledger = fake_write_category_ledger,
        .read_note = fake_read_note,
        .write_note = fake_write_note,
        .read_note_ledger = fake_read_note_ledger,
    };
    return port;
}

static const struct drawer_metrics metrics = {.top_padding = 0,
                                              .row_height = 10,
                                              .category_height = 10,
                                              .category_gap = 0,
                                              .category_indent = 1,
                                              .note_indent = 2};

static void verify_ready_state(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state ready");
    require(adapter.note_scans == 3, "one note scan per category");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "theme from the port");
    require(folio_state_note_count(state) == 9, "nine notes in three categories");
    require(!folio_state_pane_title(state).any, "no title before selection");
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    /* B（展開: one, two, three）・A（畳んだまま）・C（既定で展開: one, two, three） */
    require(drawer_layout_row_count(layout) == 9, "row count");
    require(same_text(drawer_layout_row(layout, 0).text, "B") &&
                same_text(drawer_layout_row(layout, 1).text, "one") &&
                same_text(drawer_layout_row(layout, 2).text, "two") &&
                same_text(drawer_layout_row(layout, 3).text, "three") &&
                same_text(drawer_layout_row(layout, 4).text, "A") &&
                same_text(drawer_layout_row(layout, 5).text, "C") &&
                same_text(drawer_layout_row(layout, 8).text, "three"),
            "row order follows ledgers then scan");
    require(drawer_layout_row(layout, 0).color.red == 0x11 &&
                drawer_layout_row(layout, 5).color.red == 0x7F,
            "row colors");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
    folio_state_destroy(nullptr);
}

static void verify_absent_data(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_ABSENT;
    adapter.scan_outcome = PERSISTENCE_ABSENT;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "absent data is empty");
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY &&
                drawer_layout_row_count(layout) == 0,
            "no rows without data");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.notes_outcome = PERSISTENCE_ABSENT;
    adapter.notes_scan_outcome = PERSISTENCE_ABSENT;
    port = port_for(&adapter);
    looks = looks_for(&dark_adapter);
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "absent notes");
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY &&
                drawer_layout_row_count(layout) == 3,
            "only category rows");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
}

static size_t row_count(const struct folio_state *_Nonnull state)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    size_t count = drawer_layout_row_count(layout);
    drawer_layout_destroy(layout);
    return count;
}

static void verify_toggle(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state for toggle");
    require(row_count(state) == 9, "rows before toggle");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY, "collapse B");
    require(row_count(state) == 6, "B's notes are hidden");
    require(adapter.writes == 1 && adapter.written_count == 3 && !adapter.written_expanded[0] &&
                !adapter.written_expanded[1] && adapter.written_expanded[2],
            "toggled ledger was written");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    require(row_count(state) == 9 && adapter.writes == 2 && adapter.written_expanded[1],
            "A's notes appear and the write follows");
    require(folio_state_toggle_category(state, 3) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 2,
            "out of range is refused without writing");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_STORE_FAILED, "store failed");
    require(row_count(state) == 9, "state is unchanged when the write fails");
    adapter.write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_OUT_OF_MEMORY,
            "write out of memory");
    folio_state_destroy(state);
}

static void verify_select(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state for select");
    require(strstr(folio_state_pane_rtf(state), "\\colortbl;") != nullptr &&
                strstr(folio_state_pane_rtf(state), "\\par") == nullptr &&
                folio_state_pane_rtf_length(state) == strlen(folio_state_pane_rtf(state)),
            "empty pane before selection");
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
    require(same_text(adapter.last_note, "two"), "asked the port for the note");
    const char *rtf = folio_state_pane_rtf(state);
    require(strstr(rtf, "\\b\\cf2\\fs44 Hello") != nullptr && strstr(rtf, "body\\par") != nullptr &&
                folio_state_pane_rtf_length(state) == strlen(rtf),
            "pane shows the rendered note");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.any && title.ordinal == 1 && same_text(title.category, "B") &&
                same_text(title.note, "two") && title.color.red == 0x11,
            "title follows the selection");
    struct drawer_layout *marked = nullptr;
    require(folio_state_drawer_layout(state, metrics, &marked) == FOLIO_STATE_READY &&
                drawer_layout_row(marked, 2).selected && !drawer_layout_row(marked, 1).selected,
            "layout marks the selected note");
    drawer_layout_destroy(marked);
    require(folio_state_select_note(state, 5, 0) == FOLIO_STATE_NO_SUCH_CATEGORY, "bad category");
    require(folio_state_select_note(state, 0, 3) == FOLIO_STATE_NO_SUCH_NOTE, "bad note");
    adapter.note_outcome = PERSISTENCE_ABSENT;
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_NOTE_UNREADABLE &&
                folio_state_pane_rtf(state) == rtf && folio_state_pane_title(state).note[0] == 't',
            "absent note keeps the pane and the selection");
    adapter.note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_NOTE_UNREADABLE, "unreadable note");
    adapter.note_outcome = PERSISTENCE_LOADED;
    adapter.note_body = "\xC3";
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_NOTE_UNREADABLE, "invalid utf8");
    adapter.note_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_OUT_OF_MEMORY, "note oom");
    folio_state_destroy(state);
}

/* CRLF のノートを 1 つ選び、編集モードに入った状態を作る。 */
static struct folio_state *_Nonnull edited_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = port_for(adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state for edit");
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"),
            "pane text is the body");
    require(folio_state_pane_text_length(state) == 15, "pane text length");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "begin edit");
    return state;
}

static void verify_edit_guards(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state for guards");
    require(folio_state_pane_mode(state) == PANE_MODE_VIEW, "view before any intent");
    require(same_text(folio_state_pane_text(state), "") && folio_state_pane_text_length(state) == 0,
            "no body before selection");
    require(folio_state_begin_edit(state) == FOLIO_STATE_NOTHING_SELECTED, "nothing selected");
    require(folio_state_pane_mode(state) == PANE_MODE_VIEW, "the refused intent keeps view");
    require(folio_state_store_note(state, u"x", 1) == FOLIO_STATE_NOT_EDITING, "store while view");
    require(folio_state_end_edit(state, u"x", 1) == FOLIO_STATE_NOT_EDITING, "end while view");
    require(adapter.note_writes == 0, "refused intents never write");
    folio_state_destroy(state);
}

static void verify_edit_unchanged(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_store_note(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY &&
                adapter.note_writes == 0 && folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "the same body is not written and the mode stays");
    require(folio_state_end_edit(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY &&
                adapter.note_writes == 0 && folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "leaving without a change writes nothing");
    folio_state_destroy(state);
}

static void verify_edit_saves(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    /* RichEdit は CRLF を返すが、LF が混ざっても元の形（CRLF）へ畳む。 */
    require(folio_state_store_note(state, u"# Hello\r\n\r\nbody\nmore", 20) == FOLIO_STATE_READY,
            "store a changed body");
    require(adapter.note_writes == 1 &&
                same_text(adapter.written_body, "# Hello\r\n\r\nbody\r\nmore"),
            "the written bytes keep the original line ending");
    require(same_text(adapter.written_category, "B") && same_text(adapter.written_note, "two"),
            "written to the selected note");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody\r\nmore") &&
                strstr(folio_state_pane_rtf(state), "more") != nullptr,
            "the read body and the pane follow the save");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "Ctrl+S stays in edit");
    require(folio_state_end_edit(state, u"# Hello\r\n\r\nbody\r\nmore\r\nlast", 27) ==
                FOLIO_STATE_READY,
            "end edit saves");
    require(adapter.note_writes == 2 && folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "leaving with a change writes once and returns to view");
    folio_state_destroy(state);
}

static void verify_edit_failures(void)
{
    static const char16_t lone_surrogate[] = {u'a', 0xD83D, u'b'};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_store_note(state, lone_surrogate, 3) == FOLIO_STATE_NOTE_MALFORMED &&
                adapter.note_writes == 0,
            "a lone surrogate is refused without writing");
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_NOTE_STORE_FAILED,
            "the store failure is reported");
    require(adapter.note_writes == 1 && folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"),
            "the failed store changes neither the mode nor the read body");
    require(folio_state_end_edit(state, u"changed", 7) == FOLIO_STATE_NOTE_STORE_FAILED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "leaving is refused while the store fails");
    adapter.note_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_OUT_OF_MEMORY,
            "the write reports out of memory");
    folio_state_destroy(state);
}

static void verify_failure_lines(void)
{
    require(same_text(folio_state_failure_line(FOLIO_STATE_READY), ""), "ready has no line");
    require(strlen(folio_state_failure_line(FOLIO_STATE_DATA_UNREADABLE)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_LEDGER_MALFORMED)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_STORE_FAILED)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NO_SUCH_CATEGORY)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NO_SUCH_NOTE)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NOTE_UNREADABLE)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NOTHING_SELECTED)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NOT_EDITING)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NOTE_MALFORMED)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_NOTE_STORE_FAILED)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_OUT_OF_MEMORY)) > 0,
            "every failure has a line");
}

static void expect_failure(struct persistence_adapter adapter, enum folio_state_outcome expected,
                           const char *_Nonnull description)
{
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == expected, description);
}

static void verify_failures(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable categories");
    adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_MALFORMED;
    expect_failure(adapter, FOLIO_STATE_LEDGER_MALFORMED, "malformed categories");
    adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_OUT_OF_MEMORY;
    expect_failure(adapter, FOLIO_STATE_OUT_OF_MEMORY, "out of memory categories");
    adapter = healthy_adapter();
    adapter.scan_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable scan");
    adapter = healthy_adapter();
    adapter.notes_outcome = PERSISTENCE_MALFORMED;
    expect_failure(adapter, FOLIO_STATE_LEDGER_MALFORMED, "malformed notes");
    adapter = healthy_adapter();
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable notes");
    adapter = healthy_adapter();
    adapter.notes_scan_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable note scan");
    adapter = healthy_adapter();
    adapter.notes_scan_outcome = PERSISTENCE_OUT_OF_MEMORY;
    expect_failure(adapter, FOLIO_STATE_OUT_OF_MEMORY, "out of memory note scan");
}

struct persistence_adapter *_Nonnull test_adapter_create(const char *_Nonnull categories_text,
                                                         const char *_Nonnull notes_text)
{
    struct persistence_adapter *adapter = malloc(sizeof *adapter);
    if (adapter == nullptr)
    {
        exit(1);
    }
    *adapter = healthy_adapter();
    adapter->categories_text = categories_text;
    adapter->notes_text = notes_text;
    return adapter;
}

struct persistence_port test_adapter_port(struct persistence_adapter *_Nonnull adapter)
{
    return port_for(adapter);
}

void test_adapter_destroy(struct persistence_adapter *_Nullable adapter)
{
    free(adapter);
}

struct appearance_port test_appearance_port(void)
{
    return looks_for(&dark_adapter);
}

void run_state_tests(void)
{
    verify_ready_state();
    verify_absent_data();
    verify_failures();
    verify_toggle();
    verify_select();
    verify_edit_guards();
    verify_edit_unchanged();
    verify_edit_saves();
    verify_edit_failures();
    verify_failure_lines();
}
