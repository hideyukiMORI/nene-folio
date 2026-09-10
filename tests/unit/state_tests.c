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
    char written_names[64]; /* 最後に書かれたカテゴリ台帳の名前（'/' 区切り） */
    enum persistence_outcome ledger_write_outcome;
    size_t ledger_writes;                  /* write_note_ledger が呼ばれた回数 */
    const char *_Nullable ledger_category; /* 最後に索引を書かれたカテゴリ名 */
    char ledger_order[64];                 /* 最後に書かれた索引の名前（'/' 区切り） */
    enum persistence_outcome move_outcome;
    size_t moves;                    /* move_note が呼ばれた回数 */
    const char *_Nullable move_from; /* 最後の移動元のカテゴリ名 */
    /* 最後に移されたノート名。名前は移動元の台帳が持っており、意図の中で捨てられるので複製する。 */
    char moved_note[64];
    const char *_Nullable move_to; /* 最後の移動先のカテゴリ名 */
    char calls[128];               /* 呼び出しの順（'/' 区切り。move・<カテゴリ>・書いた名前） */
    size_t ledger_fail_at; /* この番号（1 始まり）の索引の書き戻しだけ失敗させる。0 なら使わない */
    const char *_Nullable alt_category; /* この名前のカテゴリだけ別の索引と md を返す */
    const char *_Nonnull alt_notes_text;
    const char *_Nonnull const *_Nullable alt_scanned; /* nullptr で終わる名前の並び */
};

/* 名前を '/' でつないで out（終端付き）へ書き足す。 */
static void append_name(char *_Nonnull out, size_t capacity, size_t *_Nonnull position,
                        const char *_Nonnull name)
{
    size_t length = strlen(name);
    require(*position + length + 2 < capacity, "written names fit the fake");
    if (*position > 0)
    {
        out[*position] = '/';
        *position += 1;
    }
    memcpy(out + *position, name, length);
    *position += length;
    out[*position] = '\0';
}

/* 呼び出しの順を 1 つ記録する（ADR 0008 の決定 3 の順序の検証に使う）。 */
static void record_call(struct persistence_adapter *_Nonnull adapter, const char *_Nonnull name)
{
    size_t position = strlen(adapter->calls);
    append_name(adapter->calls, sizeof adapter->calls, &position, name);
}

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

/* 別の索引を持たせたカテゴリか（別カテゴリへの移動を試すために要る）。 */
static bool is_alt(const struct persistence_adapter *_Nonnull adapter,
                   const char *_Nonnull category)
{
    return adapter->alt_category != nullptr && strcmp(adapter->alt_category, category) == 0;
}

static enum persistence_outcome fake_scan_notes(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category,
                                                struct name_list *_Nullable *_Nonnull out)
{
    adapter->note_scans += 1;
    if (adapter->notes_scan_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_scan_outcome;
    }
    if (!is_alt(adapter, category))
    {
        return list_names(adapter->scanned_notes, adapter->scanned_note_count, out);
    }
    const char *_Nonnull const *_Nullable names = adapter->alt_scanned;
    size_t count = 0;
    while (names != nullptr && names[count] != nullptr)
    {
        count += 1;
    }
    return list_names(names, count, out);
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
    if (adapter->notes_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_outcome;
    }
    const char *_Nonnull text =
        is_alt(adapter, category) ? adapter->alt_notes_text : adapter->notes_text;
    enum note_ledger_outcome parsed = note_ledger_parse(text, strlen(text), out);
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
    size_t position = 0;
    adapter->written_names[0] = '\0';
    for (size_t index = 0; index < adapter->written_count && index < 8; ++index)
    {
        adapter->written_expanded[index] = category_ledger_expanded(ledger, index);
        append_name(adapter->written_names, sizeof adapter->written_names, &position,
                    category_ledger_name(ledger, index));
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

static enum persistence_outcome fake_write_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                       const char *_Nonnull category,
                                                       const struct note_ledger *_Nonnull ledger)
{
    adapter->ledger_writes += 1;
    adapter->ledger_category = category;
    record_call(adapter, category);
    if (adapter->ledger_fail_at == adapter->ledger_writes)
    {
        return PERSISTENCE_UNWRITABLE;
    }
    if (adapter->ledger_write_outcome != PERSISTENCE_STORED)
    {
        return adapter->ledger_write_outcome;
    }
    size_t position = 0;
    adapter->ledger_order[0] = '\0';
    size_t count = note_ledger_count(ledger);
    for (size_t index = 0; index < count; ++index)
    {
        append_name(adapter->ledger_order, sizeof adapter->ledger_order, &position,
                    note_ledger_name(ledger, index));
        record_call(adapter, note_ledger_name(ledger, index));
    }
    return PERSISTENCE_STORED;
}

/* md の移動。指定された結果を返し、呼び出しの順を記録する（rename が先であることの証拠）。 */
static enum persistence_outcome fake_move_note(struct persistence_adapter *_Nonnull adapter,
                                               const char *_Nonnull from_category,
                                               const char *_Nonnull note,
                                               const char *_Nonnull to_category)
{
    adapter->moves += 1;
    adapter->move_from = from_category;
    size_t length = strlen(note);
    require(length + 1 < sizeof adapter->moved_note, "the moved name fits the fake");
    memcpy(adapter->moved_note, note, length + 1);
    adapter->move_to = to_category;
    record_call(adapter, "move");
    return adapter->move_outcome;
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
        .written_names = {'\0'},
        .ledger_write_outcome = PERSISTENCE_STORED,
        .ledger_writes = 0,
        .ledger_category = nullptr,
        .ledger_order = {'\0'},
        .move_outcome = PERSISTENCE_STORED,
        .moves = 0,
        .move_from = nullptr,
        .moved_note = {'\0'},
        .move_to = nullptr,
        .calls = {'\0'},
        .ledger_fail_at = 0,
        .alt_category = nullptr,
        .alt_notes_text = "{\"version\": 1, \"notes\": []}",
        .alt_scanned = nullptr,
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
        .move_note = fake_move_note,
        .read_note_ledger = fake_read_note_ledger,
        .write_note_ledger = fake_write_note_ledger,
    };
    return port;
}

static const struct drawer_metrics metrics = {.top_padding = 0,
                                              .row_height = 10,
                                              .category_height = 10,
                                              .category_gap = 0,
                                              .category_indent = 1,
                                              .note_indent = 2};

/* ノートの居場所を 1 つ作る。 */
static struct note_ref at(size_t category, size_t note)
{
    struct note_ref ref = {.category = category, .note = note};
    return ref;
}

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

/* 50 px の窓で同じ索引を見る。B / A / C が展開なら下端は 90 で上限は 40、
 * B を折り畳めば下端は 60 で上限は 10（ADR 0009 の決定 2）。 */
static const struct drawer_metrics scroll_metrics = {.top_padding = 0,
                                                     .row_height = 10,
                                                     .category_height = 10,
                                                     .category_gap = 0,
                                                     .category_indent = 1,
                                                     .note_indent = 2,
                                                     .viewport_height = 50,
                                                     .bottom_padding = 0};

/* いまの有効なスクロール量を、最初の行の表示座標（0 か負の値）で読む。 */
static int scrolled_top(const struct folio_state *_Nonnull state)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, scroll_metrics, &layout) == FOLIO_STATE_READY,
            "layout for scroll");
    int top = drawer_layout_row(layout, 0).top;
    drawer_layout_destroy(layout);
    return top;
}

/* 丸めと、端での不変。要求量は配置より長く残る（ADR 0009 の「失う・残る」）。 */
static void verify_scroll_rounding(struct folio_state *_Nonnull state)
{
    require(scrolled_top(state) == 0, "a fresh state is at the top");
    require(folio_state_scroll_drawer(state, scroll_metrics, 15) == FOLIO_STATE_READY,
            "scrolling down is accepted");
    require(scrolled_top(state) == -15, "the drawer moved by the delta");
    require(folio_state_scroll_drawer(state, scroll_metrics, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == -15,
            "a zero delta changes nothing");
    require(folio_state_scroll_drawer(state, scroll_metrics, 100) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "past the reach it stops at the reach");
    require(folio_state_scroll_drawer(state, scroll_metrics, 10) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "at the end another step does not move");
    require(folio_state_scroll_drawer(state, scroll_metrics, -1000) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "a large negative delta lands at the top");
}

/* 折り畳みで上限が縮んでも、有効量から数え直すので 1 回で必ず動く（ADR 0009 の決定 3）。 */
static void verify_scroll_recount(struct folio_state *_Nonnull state)
{
    require(folio_state_scroll_drawer(state, scroll_metrics, 40) == FOLIO_STATE_READY,
            "go to the end");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY, "collapse B");
    require(scrolled_top(state) == -10, "a smaller reach rounds the display, not the request");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "expanding again brings the request back");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == -10,
            "collapsed once more");
    require(folio_state_scroll_drawer(state, scroll_metrics, -10) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "one step counts from the effective amount, not the request");
}

/* 他の意図はスクロール量を触らない（見える位置へ寄せない）。 */
static void verify_scroll(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state for scroll");
    verify_scroll_rounding(state);
    verify_scroll_recount(state);
    require(folio_state_scroll_drawer(state, scroll_metrics, 5) == FOLIO_STATE_READY,
            "scroll a little");
    require(folio_state_select_note(state, 1, 0) == FOLIO_STATE_READY && scrolled_top(state) == -5,
            "selecting a note leaves the amount alone");
    require(folio_state_move_note(state, at(1, 0), at(1, 2)) == FOLIO_STATE_READY &&
                scrolled_top(state) == -5,
            "reordering leaves the amount alone");
    folio_state_destroy(state);
}

/* 配置の行の名前を '/' でつないだ 1 行にする。折り畳んだカテゴリのノートは出ない。 */
static void row_order(const struct folio_state *_Nonnull state, char *_Nonnull out, size_t capacity)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    size_t position = 0;
    size_t count = drawer_layout_row_count(layout);
    out[0] = '\0';
    for (size_t index = 0; index < count; ++index)
    {
        append_name(out, capacity, &position, drawer_layout_row(layout, index).text);
    }
    drawer_layout_destroy(layout);
}

static struct folio_state *_Nonnull ready_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = port_for(adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(&port, &looks, &state) == FOLIO_STATE_READY, "state for move");
    return state;
}

/* 索引はカテゴリと一緒に動く。B の索引を先に並べ替えてから B を末尾へ運ぶ。 */
static void verify_move_category(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"), "the order before the move");
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY,
            "reorder B's notes");
    require(folio_state_move_category(state, 0, 2) == FOLIO_STATE_READY, "B moves to the end");
    row_order(state, order, sizeof order);
    require(same_text(order, "A/C/one/two/three/B/three/one/two"),
            "the category and its own index travel together");
    require(adapter.writes == 1 && same_text(adapter.written_names, "A/C/B"),
            "the moved ledger was written once");
    require(!adapter.written_expanded[0] && adapter.written_expanded[1] &&
                adapter.written_expanded[2],
            "the expansion travels with the category");
    require(folio_state_move_category(state, 2, 0) == FOLIO_STATE_READY, "B moves back");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"), "moving up restores the order");
    require(adapter.writes == 2 && same_text(adapter.written_names, "B/A/C"), "the second write");
    require(folio_state_move_category(state, 1, 1) == FOLIO_STATE_READY && adapter.writes == 2,
            "the same position writes nothing");
    require(folio_state_move_category(state, 3, 0) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                folio_state_move_category(state, 0, 3) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 2,
            "out of range is refused without writing");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_STORE_FAILED, "store failed");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"),
            "the failed write leaves the order alone");
    adapter.write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_OUT_OF_MEMORY, "move oom");
    folio_state_destroy(state);
}

static void verify_move_note(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY,
            "three moves to the front");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"), "only B's notes moved");
    require(adapter.ledger_writes == 1 && same_text(adapter.ledger_category, "B") &&
                same_text(adapter.ledger_order, "three/one/two"),
            "the index of the right category was written once");
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_READY, "and back");
    require(adapter.ledger_writes == 2 && same_text(adapter.ledger_order, "one/two/three"),
            "moving down restores the order");
    require(folio_state_move_note(state, at(0, 1), at(0, 1)) == FOLIO_STATE_READY &&
                adapter.ledger_writes == 2,
            "the same position writes nothing");
    require(folio_state_move_note(state, at(3, 0), at(3, 1)) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "no such category");
    require(folio_state_move_note(state, at(0, 3), at(0, 0)) == FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_move_note(state, at(0, 0), at(0, 3)) == FOLIO_STATE_NO_SUCH_NOTE &&
                adapter.ledger_writes == 2,
            "out of range is refused without writing");
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_STORE_FAILED,
            "index not written");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"),
            "the failed write leaves the notes alone");
    adapter.ledger_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_OUT_OF_MEMORY,
            "index oom");
    folio_state_destroy(state);
}

/* 選択と編集中の本文は、番号が付け替わっても同じノートを指し続ける（ADR 0007 の決定 3 / 7）。 */
static void verify_move_selection(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY,
            "three moves up");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 1 && same_text(title.category, "B") && same_text(title.note, "two"),
            "the selection follows the note when its neighbour moves");
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_READY,
            "three moves back");
    require(same_text(folio_state_pane_title(state).note, "two"), "and back again");
    require(folio_state_move_note(state, at(1, 0), at(1, 2)) == FOLIO_STATE_READY,
            "another category moves");
    require(same_text(adapter.ledger_category, "A") &&
                same_text(folio_state_pane_title(state).note, "two"),
            "a move in another category leaves the selection alone");
    require(folio_state_move_category(state, 0, 2) == FOLIO_STATE_READY, "B moves to the end");
    title = folio_state_pane_title(state);
    require(title.ordinal == 3 && same_text(title.category, "B") && same_text(title.note, "two"),
            "the selected note keeps its category through the move");
    folio_state_destroy(state);
}

/* A だけ別の索引（four / five）にして、別カテゴリへの移動を試せる状態を作る。
 * カテゴリは B（展開・one/two/three）・A（折り畳み・four/five）・C（展開・one/two/three）。 */
static struct folio_state *_Nonnull transfer_state(struct persistence_adapter *_Nonnull adapter)
{
    static const char *const alt_scanned[] = {"four", "five", nullptr};
    test_adapter_second_notes(adapter, "A", "{\"version\": 1, \"notes\": [\"four\", \"five\"]}",
                              alt_scanned);
    return ready_state(adapter);
}

/* B の two を A の 1 番目へ移す（ADR 0008 の決定 3 の (a)〜(e)）。 */
static void verify_transfer_note(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_READY,
            "two moves into A");
    require(adapter.moves == 1 && same_text(adapter.move_from, "B") &&
                same_text(adapter.moved_note, "two") && same_text(adapter.move_to, "A"),
            "the md was renamed once, from B to A");
    require(same_text(adapter.calls, "move/A/four/two/five/B/one/three"),
            "the rename comes first, then the destination ledger, then the source");
    require(adapter.ledger_writes == 2 && same_text(adapter.ledger_order, "one/three"),
            "the source ledger is the last write");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/four/two/five/C/one/two/three"),
            "both indexes follow the file");
    folio_state_destroy(state);
}

/* 折り畳んだカテゴリの末尾（ノート数と同じ番号）へ落とす。 */
static void verify_transfer_to_end(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 0), at(1, 2)) == FOLIO_STATE_READY,
            "one moves to the end of A");
    require(same_text(adapter.calls, "move/A/four/five/one/B/two/three"),
            "the destination ledger gained the note at its end");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two/three/A/four/five/one/C/one/two/three"), "the new order");
    folio_state_destroy(state);
}

/* 選択中で編集中のノートを移しても、本文とモードはそのままで note_ref だけ付け替わる。 */
static void verify_transfer_selection(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "begin edit");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY,
            "move the selected note");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "A") && same_text(title.note, "two"),
            "the title follows the note into its new category");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody") &&
                adapter.note_writes == 0,
            "the body and the mode are untouched and nothing was saved");
    folio_state_destroy(state);
}

/* 移動元で後ろにあったノートは 1 つ詰み、移動先で挿入位置以降のノートは 1 つ進む。 */
static void verify_transfer_renumbering(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_select_note(state, 0, 2) == FOLIO_STATE_READY, "select B / three");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY, "two leaves B");
    require(same_text(folio_state_pane_title(state).note, "three"),
            "the note behind the moved one closes up");
    require(folio_state_select_note(state, 1, 2) == FOLIO_STATE_READY, "select A / five");
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_READY, "one joins A");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "A") && same_text(title.note, "five"),
            "the notes at and after the insertion move down");
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / three again");
    require(folio_state_move_note(state, at(0, 0), at(1, 3)) == FOLIO_STATE_READY,
            "the selected note itself moves to the end of A");
    require(same_text(folio_state_pane_title(state).category, "A") &&
                same_text(folio_state_pane_title(state).note, "three"),
            "the selection travels with it");
    folio_state_destroy(state);
}

/* 同名・範囲外・ファイルを移せないときは何も変えない（ADR 0008 の決定 3 の (a) / (c)）。 */
static void verify_transfer_refusals(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 0), at(2, 0)) == FOLIO_STATE_NAME_TAKEN,
            "C already holds a note called one");
    require(adapter.moves == 0 && adapter.ledger_writes == 0, "a refused move never touches data/");
    require(folio_state_move_note(state, at(0, 0), at(3, 0)) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "no such destination category");
    require(folio_state_move_note(state, at(0, 3), at(1, 0)) == FOLIO_STATE_NO_SUCH_NOTE,
            "no such note");
    require(folio_state_move_note(state, at(0, 0), at(1, 3)) == FOLIO_STATE_NO_SUCH_NOTE,
            "the destination position is past the end");
    adapter.move_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_STORE_FAILED,
            "the file could not be moved");
    require(adapter.moves == 1 && adapter.ledger_writes == 0 && same_text(adapter.calls, "move"),
            "no ledger is written when the rename fails");
    adapter.move_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_OUT_OF_MEMORY,
            "the rename reports out of memory");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"),
            "the refused moves changed nothing");
    folio_state_destroy(state);
}

/* 台帳だけ書けないときは、索引はファイルに追随したまま LEDGER_STALE（決定 3 の (e)）。 */
static void verify_transfer_stale_ledger(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.ledger_fail_at = 1;
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_LEDGER_STALE,
            "the destination ledger could not be written");
    require(adapter.moves == 1 && adapter.ledger_writes == 2 &&
                same_text(adapter.calls, "move/A/B/one/three"),
            "both ledgers were attempted, the destination first");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/four/two/five/C/one/two/three"),
            "the index follows the file even when a ledger is stale");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.ledger_fail_at = 2;
    state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_LEDGER_STALE,
            "the source ledger could not be written either");
    require(same_text(adapter.calls, "move/A/four/two/five/B"),
            "the destination was still written");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.ledger_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_OUT_OF_MEMORY,
            "out of memory is not folded into stale");
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

/* 編集中でも並び替えられる。本文は UI が持つので保存を挟まない（ADR 0007 の決定 7）。 */
static void verify_move_while_editing(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_READY, "move while editing");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "the mode is kept");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"), "the body is kept");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "B") && same_text(title.note, "two"),
            "the selection is renumbered");
    require(adapter.note_writes == 0, "the reorder never saves the note");
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
                strlen(folio_state_failure_line(FOLIO_STATE_NAME_TAKEN)) > 0 &&
                strlen(folio_state_failure_line(FOLIO_STATE_LEDGER_STALE)) > 0 &&
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

void test_adapter_second_notes(struct persistence_adapter *_Nonnull adapter,
                               const char *_Nonnull category, const char *_Nonnull notes_text,
                               const char *_Nonnull const *_Nonnull scanned)
{
    adapter->alt_category = category;
    adapter->alt_notes_text = notes_text;
    adapter->alt_scanned = scanned;
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
    verify_scroll();
    verify_move_category();
    verify_move_note();
    verify_move_selection();
    verify_transfer_note();
    verify_transfer_to_end();
    verify_transfer_selection();
    verify_transfer_renumbering();
    verify_transfer_refusals();
    verify_transfer_stale_ledger();
    verify_select();
    verify_edit_guards();
    verify_edit_unchanged();
    verify_edit_saves();
    verify_edit_failures();
    verify_move_while_editing();
    verify_failure_lines();
}
