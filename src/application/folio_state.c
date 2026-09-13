#include "folio_state.h"

#include "appearance_port.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "markdown_rtf.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_name.h"
#include "note_text.h"
#include "persistence_port.h"
#include "rtf_palette.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct folio_state
{
    struct persistence_port port;
    enum folio_theme theme;
    struct rtf_palette palette;
    struct category_ledger *_Nullable categories;
    struct note_ledger *_Nonnull *_Nullable notes; /* categories と同じ数・同じ順 */
    size_t notes_count;
    struct markdown_rtf *_Nullable pane; /* 選択中のノートの表示値。無ければ空の文書 */
    struct note_text *_Nullable body;    /* 最後に読んだ本文。何も選んでいなければ空 */
    enum pane_mode mode;
    int scroll; /* ドロワーのスクロール量（要求量・画素・0 以上）。上限は core が決める */
    enum folio_document_kind document;
    size_t selected_category;
    size_t selected_note;
    bool index_pending; /* 初回作成後に同期できなかった台帳。次の保存・変更前に再試行 */
    size_t pending_category;
    bool cursor_any; /* 索引のカーソルがあるか（ADR 0015 の決定 1） */
    enum folio_cursor_kind cursor_kind;
    size_t cursor_category; /* FOLIO_CURSOR_CATEGORY のときのカテゴリ番号 */
};

static enum folio_state_outcome translate(enum persistence_outcome outcome)
{
    switch (outcome)
    {
    case PERSISTENCE_LOADED:
    case PERSISTENCE_ABSENT:
        return FOLIO_STATE_READY;
    case PERSISTENCE_UNREADABLE:
        return FOLIO_STATE_DATA_UNREADABLE;
    case PERSISTENCE_MALFORMED:
        return FOLIO_STATE_LEDGER_MALFORMED;
    case PERSISTENCE_STORED:
        return FOLIO_STATE_READY;
    case PERSISTENCE_UNWRITABLE:
        return FOLIO_STATE_STORE_FAILED;
    case PERSISTENCE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case PERSISTENCE_NAME_TAKEN:
        return FOLIO_STATE_NAME_TAKEN;
    }
    return FOLIO_STATE_DATA_UNREADABLE;
}

static enum folio_state_outcome synchronize_index(struct folio_state *_Nonnull state)
{
    if (!state->index_pending)
    {
        return FOLIO_STATE_READY;
    }
    enum persistence_outcome stored = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, state->pending_category),
        state->notes[state->pending_category]);
    if (stored != PERSISTENCE_STORED)
    {
        return FOLIO_STATE_LEDGER_STALE;
    }
    state->index_pending = false;
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome from_category_ledger(enum category_ledger_outcome outcome)
{
    switch (outcome)
    {
    case CATEGORY_LEDGER_ACCEPTED:
        return FOLIO_STATE_READY;
    case CATEGORY_LEDGER_MALFORMED:
    case CATEGORY_LEDGER_UNSUPPORTED_VERSION:
        return FOLIO_STATE_LEDGER_MALFORMED;
    case CATEGORY_LEDGER_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_LEDGER_MALFORMED;
}

static enum folio_state_outcome from_note_ledger(enum note_ledger_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_LEDGER_ACCEPTED:
        return FOLIO_STATE_READY;
    case NOTE_LEDGER_MALFORMED:
    case NOTE_LEDGER_UNSUPPORTED_VERSION:
        return FOLIO_STATE_LEDGER_MALFORMED;
    case NOTE_LEDGER_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_LEDGER_MALFORMED;
}

/* 台帳の書き戻しの結果を意図の結果に写す。記憶不足だけは書けなかったことと区別する。 */
static enum folio_state_outcome from_store(enum persistence_outcome stored)
{
    return stored == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                               : FOLIO_STATE_STORE_FAILED;
}

/* 台帳（無ければ空）と走査結果（無ければ空）を照合して categories を確定する。 */
static enum folio_state_outcome load_categories(struct folio_state *_Nonnull state,
                                                const struct persistence_port *_Nonnull port)
{
    struct category_ledger *_Nullable stored = nullptr;
    enum persistence_outcome read = port->read_category_ledger(port->adapter, &stored);
    if (read == PERSISTENCE_ABSENT)
    {
        enum folio_state_outcome outcome = from_category_ledger(category_ledger_empty(&stored));
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
    }
    else if (read != PERSISTENCE_LOADED)
    {
        return translate(read);
    }
    struct name_list *_Nullable scanned = nullptr;
    enum persistence_outcome scan = port->scan_categories(port->adapter, &scanned);
    enum folio_state_outcome outcome = translate(scan);
    if (scan == PERSISTENCE_ABSENT && name_list_create(&scanned) != NAME_LIST_ACCEPTED)
    {
        outcome = FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome =
            from_category_ledger(category_ledger_reconcile(stored, scanned, &state->categories));
    }
    name_list_destroy(scanned);
    category_ledger_destroy(stored);
    return outcome;
}

/* 1 カテゴリの index.json（無ければ空）と md の走査結果を照合する。 */
static enum folio_state_outcome load_notes(struct note_ledger *_Nullable *_Nonnull slot,
                                           const struct persistence_port *_Nonnull port,
                                           const char *_Nonnull category)
{
    struct note_ledger *_Nullable stored = nullptr;
    enum persistence_outcome read = port->read_note_ledger(port->adapter, category, &stored);
    if (read == PERSISTENCE_ABSENT)
    {
        enum folio_state_outcome outcome = from_note_ledger(note_ledger_empty(&stored));
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
    }
    else if (read != PERSISTENCE_LOADED)
    {
        return translate(read);
    }
    struct name_list *_Nullable scanned = nullptr;
    enum persistence_outcome scan = port->scan_notes(port->adapter, category, &scanned);
    enum folio_state_outcome outcome = translate(scan);
    if (scan == PERSISTENCE_ABSENT && name_list_create(&scanned) != NAME_LIST_ACCEPTED)
    {
        outcome = FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = from_note_ledger(note_ledger_reconcile(stored, scanned, slot));
    }
    name_list_destroy(scanned);
    note_ledger_destroy(stored);
    return outcome;
}

static enum folio_state_outcome load_all_notes(struct folio_state *_Nonnull state,
                                               const struct persistence_port *_Nonnull port)
{
    size_t count = category_ledger_count(state->categories);
    state->notes = calloc(count + 1, sizeof *state->notes);
    if (state->notes == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < count; ++index)
    {
        struct note_ledger *_Nullable notes = nullptr;
        enum folio_state_outcome outcome =
            load_notes(&notes, port, category_ledger_name(state->categories, index));
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
        state->notes[index] = notes;
        state->notes_count += 1;
    }
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_create(const struct persistence_port *_Nonnull persistence,
                                            const struct appearance_port *_Nonnull appearance,
                                            struct folio_state *_Nullable *_Nonnull out)
{
    struct folio_state *_Nullable state = calloc(1, sizeof *state);
    if (state == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    state->port = *persistence;
    state->theme = appearance->read_theme(appearance->adapter);
    state->palette = rtf_palette_for(state->theme);
    if (markdown_rtf_empty(state->palette, &state->pane) != MARKDOWN_RTF_CONVERTED ||
        note_text_create("", 0, &state->body) != NOTE_TEXT_ACCEPTED)
    {
        folio_state_destroy(state);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome outcome = load_categories(state, persistence);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = load_all_notes(state, persistence);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        folio_state_destroy(state);
        return outcome;
    }
    *out = state;
    return FOLIO_STATE_READY;
}

static struct note_ref located(size_t category, size_t note)
{
    struct note_ref ref = {.category = category, .note = note};
    return ref;
}

static struct drawer_cursor on_note(size_t category, size_t note)
{
    struct drawer_cursor cursor = {.kind = DRAWER_ROW_NOTE, .ref = located(category, note)};
    return cursor;
}

static struct drawer_cursor on_category(size_t category)
{
    struct drawer_cursor cursor = {.kind = DRAWER_ROW_CATEGORY, .ref = located(category, 0)};
    return cursor;
}

/* いまのカーソル。無ければ false。ノートのカーソルは選択そのもの（ADR 0015 の決定 1）。 */
static bool current_cursor(const struct folio_state *_Nonnull state,
                           struct drawer_cursor *_Nonnull out)
{
    if (!state->cursor_any)
    {
        return false;
    }
    switch (state->cursor_kind)
    {
    case FOLIO_CURSOR_NOTE:
        *out = on_note(state->selected_category, state->selected_note);
        return true;
    case FOLIO_CURSOR_CATEGORY:
        *out = on_category(state->cursor_category);
        return true;
    }
    return false;
}

/* カーソルを選択中のノートへ置く（選択が動いたとき）。 */
static void cursor_to_selection(struct folio_state *_Nonnull state)
{
    state->cursor_any = true;
    state->cursor_kind = FOLIO_CURSOR_NOTE;
}

/* カーソルをカテゴリ行へ置く。選択と右ペインは触らない（決定 2 / 3）。 */
static void cursor_to_category(struct folio_state *_Nonnull state, size_t category)
{
    state->cursor_any = true;
    state->cursor_kind = FOLIO_CURSOR_CATEGORY;
    state->cursor_category = category;
}

static enum folio_cursor_kind kind_of(struct drawer_cursor cursor)
{
    switch (cursor.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return FOLIO_CURSOR_CATEGORY;
    case DRAWER_ROW_NOTE:
        return FOLIO_CURSOR_NOTE;
    }
    return FOLIO_CURSOR_NOTE;
}

enum folio_state_outcome folio_state_drawer_layout(const struct folio_state *_Nonnull state,
                                                   struct drawer_metrics metrics,
                                                   struct drawer_layout *_Nullable *_Nonnull out)
{
    const struct note_ledger *_Nonnull const *_Nonnull notes =
        (const struct note_ledger *_Nonnull const *_Nonnull)state->notes;
    if (drawer_layout_create(state->categories, notes, metrics, out) != DRAWER_LAYOUT_CREATED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct note_ref selection = located(state->selected_category, state->selected_note);
    struct drawer_cursor cursor = on_note(0, 0);
    bool any = current_cursor(state, &cursor);
    drawer_layout_mark(*out, state->document == FOLIO_DOCUMENT_NAMED ? &selection : nullptr,
                       any ? &cursor : nullptr);
    drawer_layout_scroll(*out, state->scroll);
    return FOLIO_STATE_READY;
}

/* 0 と上限の間へ丸める（core の drawer_layout_scroll と同じ規則）。 */
static int clamped(int value, int limit)
{
    if (value < 0)
    {
        return 0;
    }
    return value > limit ? limit : value;
}

enum folio_state_outcome folio_state_scroll_drawer(struct folio_state *_Nonnull state,
                                                   struct drawer_metrics metrics, int delta)
{
    struct drawer_layout *_Nullable layout = nullptr;
    enum folio_state_outcome outcome = folio_state_drawer_layout(state, metrics, &layout);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    int limit = drawer_layout_scroll_limit(layout);
    drawer_layout_destroy(layout);
    state->scroll = clamped(clamped(state->scroll, limit) + delta, limit);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_reveal_cursor(struct folio_state *_Nonnull state,
                                                   struct drawer_metrics metrics)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor))
    {
        return FOLIO_STATE_READY;
    }
    struct drawer_layout *_Nullable layout = nullptr;
    enum folio_state_outcome outcome = folio_state_drawer_layout(state, metrics, &layout);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    state->scroll = drawer_layout_reveal(layout, cursor);
    drawer_layout_destroy(layout);
    return FOLIO_STATE_READY;
}

enum folio_theme folio_state_theme(const struct folio_state *_Nonnull state)
{
    return state->theme;
}

size_t folio_state_note_count(const struct folio_state *_Nonnull state)
{
    size_t total = 0;
    for (size_t index = 0; index < state->notes_count; ++index)
    {
        total += note_ledger_count(state->notes[index]);
    }
    return total;
}

enum folio_state_outcome folio_state_toggle_category(struct folio_state *_Nonnull state,
                                                     size_t index)
{
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (index >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    struct category_ledger *_Nullable toggled = nullptr;
    enum folio_state_outcome outcome =
        from_category_ledger(category_ledger_toggled(state->categories, index, &toggled));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored =
        state->port.write_category_ledger(state->port.adapter, toggled);
    if (stored != PERSISTENCE_STORED)
    {
        category_ledger_destroy(toggled);
        return from_store(stored);
    }
    category_ledger_destroy(state->categories);
    state->categories = toggled;
    return FOLIO_STATE_READY;
}

/* 展開したあとのカーソル（ADR 0015 の決定 3）。選択がその中にあればそのノート、
 * 無ければ最初のノート（右ペインが変わる）、ノートが 0 本なら行に留まる。 */
static enum folio_state_outcome cursor_into(struct folio_state *_Nonnull state, size_t index)
{
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == index)
    {
        cursor_to_selection(state);
        return FOLIO_STATE_READY;
    }
    if (note_ledger_count(state->notes[index]) == 0)
    {
        return FOLIO_STATE_READY;
    }
    return folio_state_select_note(state, index, 0);
}

/* 開閉のあとにカーソルを移す。カーソルがそのカテゴリに無ければ動かさない（決定 3）。 */
static enum folio_state_outcome cursor_after_expanded(struct folio_state *_Nonnull state,
                                                      size_t index, bool expanded)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor) || cursor.ref.category != index)
    {
        return FOLIO_STATE_READY;
    }
    if (!expanded)
    {
        cursor_to_category(state, index);
        return FOLIO_STATE_READY;
    }
    return cursor_into(state, index);
}

enum folio_state_outcome folio_state_set_category_expanded(struct folio_state *_Nonnull state,
                                                           size_t index, bool expanded)
{
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (index >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (category_ledger_expanded(state->categories, index) == expanded)
    {
        /* 変わらないなら書く理由が無い（ADR 0013 の決定 5）。カーソルもそのまま。 */
        return FOLIO_STATE_READY;
    }
    enum folio_state_outcome outcome = folio_state_toggle_category(state, index);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    return cursor_after_expanded(state, index, expanded);
}

static bool same_color(struct rgb_color left, struct rgb_color right)
{
    return left.red == right.red && left.green == right.green && left.blue == right.blue;
}

enum folio_state_outcome folio_state_recolor_category(struct folio_state *_Nonnull state,
                                                      size_t index, struct rgb_color color)
{
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (index >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (same_color(category_ledger_color(state->categories, index), color))
    {
        /* 変わらないなら書く理由が無い（ADR 0010 の決定 2）。 */
        return FOLIO_STATE_READY;
    }
    struct category_ledger *_Nullable recolored = nullptr;
    enum folio_state_outcome outcome = from_category_ledger(
        category_ledger_recolored(state->categories, index, color, &recolored));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored =
        state->port.write_category_ledger(state->port.adapter, recolored);
    if (stored != PERSISTENCE_STORED)
    {
        category_ledger_destroy(recolored);
        return from_store(stored);
    }
    category_ledger_destroy(state->categories);
    state->categories = recolored;
    return FOLIO_STATE_READY;
}

/* 並び替えたあとに、元の index 番目が来る位置。 */
static size_t moved_index(size_t from, size_t to, size_t index)
{
    if (index == from)
    {
        return to;
    }
    if (from < to)
    {
        return index > from && index <= to ? index - 1 : index;
    }
    return index >= to && index < from ? index + 1 : index;
}

/* 索引台帳の配列を台帳と同じ順に並べ替える。確保はしない。 */
static void move_notes(struct note_ledger *_Nonnull *_Nonnull items, size_t from, size_t to)
{
    struct note_ledger *_Nonnull moved = items[from];
    while (from < to)
    {
        items[from] = items[from + 1];
        from += 1;
    }
    while (from > to)
    {
        items[from] = items[from - 1];
        from -= 1;
    }
    items[to] = moved;
}

enum folio_state_outcome folio_state_move_category(struct folio_state *_Nonnull state, size_t from,
                                                   size_t to)
{
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    size_t count = category_ledger_count(state->categories);
    if (from >= count || to >= count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (from == to)
    {
        return FOLIO_STATE_READY;
    }
    struct category_ledger *_Nullable moved = nullptr;
    enum folio_state_outcome outcome =
        from_category_ledger(category_ledger_moved(state->categories, from, to, &moved));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored = state->port.write_category_ledger(state->port.adapter, moved);
    if (stored != PERSISTENCE_STORED)
    {
        category_ledger_destroy(moved);
        return from_store(stored);
    }
    category_ledger_destroy(state->categories);
    state->categories = moved;
    move_notes(state->notes, from, to);
    if (state->document != FOLIO_DOCUMENT_NONE)
    {
        state->selected_category = moved_index(from, to, state->selected_category);
    }
    if (state->cursor_any && state->cursor_kind == FOLIO_CURSOR_CATEGORY)
    {
        /* カテゴリ行のカーソルも同じカテゴリを指したまま移る。 */
        state->cursor_category = moved_index(from, to, state->cursor_category);
    }
    return FOLIO_STATE_READY;
}

/* 並び替えた索引台帳を書き戻し、書けたときだけ差し替える。 */
static enum folio_state_outcome store_notes(struct folio_state *_Nonnull state, size_t category,
                                            size_t from, size_t to)
{
    struct note_ledger *_Nullable moved = nullptr;
    enum folio_state_outcome outcome =
        from_note_ledger(note_ledger_moved(state->notes[category], from, to, &moved));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, category), moved);
    if (stored != PERSISTENCE_STORED)
    {
        note_ledger_destroy(moved);
        return from_store(stored);
    }
    note_ledger_destroy(state->notes[category]);
    state->notes[category] = moved;
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == category)
    {
        state->selected_note = moved_index(from, to, state->selected_note);
    }
    return FOLIO_STATE_READY;
}

/* 同じカテゴリ内の並び替え（ADR 0007 の決定 3）。 */
static enum folio_state_outcome reorder_note(struct folio_state *_Nonnull state, size_t category,
                                             size_t from, size_t to)
{
    size_t count = note_ledger_count(state->notes[category]);
    if (from >= count || to >= count)
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    if (from == to)
    {
        return FOLIO_STATE_READY;
    }
    return store_notes(state, category, from, to);
}

/* 台帳に同じ名前があるか。判定はバイト単位（name_list と同じ・ADR 0008 の文脈）。 */
static bool holds_name(const struct note_ledger *_Nonnull ledger, const char *_Nonnull name)
{
    size_t count = note_ledger_count(ledger);
    for (size_t index = 0; index < count; ++index)
    {
        if (strcmp(note_ledger_name(ledger, index), name) == 0)
        {
            return true;
        }
    }
    return false;
}

/* (a) 範囲と移動先の同名を確かめる。to.note は移動先のノート数と等しければ末尾。 */
static enum folio_state_outcome transfer_allowed(const struct folio_state *_Nonnull state,
                                                 struct note_ref from, struct note_ref to)
{
    if (from.note >= note_ledger_count(state->notes[from.category]) ||
        to.note > note_ledger_count(state->notes[to.category]))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    const char *_Nonnull name = note_ledger_name(state->notes[from.category], from.note);
    return holds_name(state->notes[to.category], name) ? FOLIO_STATE_NAME_TAKEN : FOLIO_STATE_READY;
}

/* (d) 選択中の番号を移動後の索引へ付け替える。別のカテゴリなので 2 つの補正は同時に起きない。 */
static void renumber_selection(struct folio_state *_Nonnull state, struct note_ref from,
                               struct note_ref to)
{
    if (state->document != FOLIO_DOCUMENT_NAMED)
    {
        return;
    }
    if (state->selected_category == from.category && state->selected_note == from.note)
    {
        state->selected_category = to.category;
        state->selected_note = to.note;
        return;
    }
    if (state->selected_category == from.category && state->selected_note > from.note)
    {
        state->selected_note -= 1;
    }
    if (state->selected_category == to.category && state->selected_note >= to.note)
    {
        state->selected_note += 1;
    }
}

/* (e) 移動先 → 移動元 の順に書き戻す。どちらかが書けなければ索引は移動後のまま LEDGER_STALE。 */
static enum folio_state_outcome store_both(struct folio_state *_Nonnull state, size_t to,
                                           size_t from)
{
    enum persistence_outcome grown = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, to), state->notes[to]);
    enum persistence_outcome shrunk = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, from), state->notes[from]);
    if (grown == PERSISTENCE_OUT_OF_MEMORY || shrunk == PERSISTENCE_OUT_OF_MEMORY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (grown != PERSISTENCE_STORED || shrunk != PERSISTENCE_STORED)
    {
        return FOLIO_STATE_LEDGER_STALE;
    }
    return FOLIO_STATE_READY;
}

/* (b) 移動後の 2 つの台帳を作る。どちらかが作れなければ両方捨てる。 */
static enum folio_state_outcome transfer_ledgers(const struct folio_state *_Nonnull state,
                                                 struct note_ref from, struct note_ref to,
                                                 struct note_ledger *_Nullable *_Nonnull pair)
{
    enum folio_state_outcome outcome =
        from_note_ledger(note_ledger_removed(state->notes[from.category], from.note, &pair[0]));
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = from_note_ledger(note_ledger_inserted(
            state->notes[to.category], to.note,
            note_ledger_name(state->notes[from.category], from.note), &pair[1]));
    }
    if (outcome != FOLIO_STATE_READY)
    {
        note_ledger_destroy(pair[0]);
    }
    return outcome;
}

/* 別のカテゴリへ移す（ADR 0008 の決定 3 の (a)〜(e)）。md の rename が先で、台帳が追随する。 */
static enum folio_state_outcome transfer_note(struct folio_state *_Nonnull state,
                                              struct note_ref from, struct note_ref to)
{
    enum folio_state_outcome outcome = transfer_allowed(state, from, to);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    struct note_ledger *_Nullable pair[2] = {nullptr, nullptr};
    outcome = transfer_ledgers(state, from, to, pair);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome moved = state->port.move_note(
        state->port.adapter, category_ledger_name(state->categories, from.category),
        note_ledger_name(state->notes[from.category], from.note),
        category_ledger_name(state->categories, to.category));
    if (moved != PERSISTENCE_STORED)
    {
        note_ledger_destroy(pair[1]);
        note_ledger_destroy(pair[0]);
        return from_store(moved);
    }
    note_ledger_destroy(state->notes[from.category]);
    state->notes[from.category] = pair[0];
    note_ledger_destroy(state->notes[to.category]);
    state->notes[to.category] = pair[1];
    renumber_selection(state, from, to);
    return store_both(state, to.category, from.category);
}

enum folio_state_outcome folio_state_move_note(struct folio_state *_Nonnull state,
                                               struct note_ref from, struct note_ref to)
{
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    size_t count = category_ledger_count(state->categories);
    if (from.category >= count || to.category >= count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (from.category == to.category)
    {
        return reorder_note(state, from.category, from.note, to.note);
    }
    return transfer_note(state, from, to);
}

/* 本文を読んで RTF にする。読めない理由は 1 つに畳む（無い・読めない・UTF-8 でない）。 */
static enum folio_state_outcome render_note(struct folio_state *_Nonnull state,
                                            const char *_Nonnull category,
                                            const char *_Nonnull note)
{
    struct note_text *_Nullable body = nullptr;
    enum persistence_outcome read =
        state->port.read_note(state->port.adapter, category, note, &body);
    if (read == PERSISTENCE_OUT_OF_MEMORY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (read != PERSISTENCE_LOADED)
    {
        return FOLIO_STATE_NOTE_UNREADABLE;
    }
    struct markdown_rtf *_Nullable rendered = nullptr;
    enum markdown_rtf_outcome converted = markdown_rtf_create(body, state->palette, &rendered);
    if (converted != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(body);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    markdown_rtf_destroy(state->pane);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = body;
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                 size_t category, size_t note)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }

    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (category >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (note >= note_ledger_count(state->notes[category]))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    enum folio_state_outcome outcome =
        render_note(state, category_ledger_name(state->categories, category),
                    note_ledger_name(state->notes[category], note));
    if (outcome == FOLIO_STATE_READY)
    {
        state->document = FOLIO_DOCUMENT_NAMED;
        state->selected_category = category;
        state->selected_note = note;
        cursor_to_selection(state);
    }
    return outcome;
}

/* 行になるノートの数。折り畳んだカテゴリは 0（drawer_layout と同じ規則）。 */
static size_t visible_count(const struct folio_state *_Nonnull state, size_t category)
{
    return category_ledger_expanded(state->categories, category)
               ? note_ledger_count(state->notes[category])
               : 0;
}

/* カテゴリの最初の止まる行。見えるノートがあればその 1 本目、無ければカテゴリ行（決定 2）。 */
static struct drawer_cursor first_stop_in(const struct folio_state *_Nonnull state, size_t category)
{
    return visible_count(state, category) > 0 ? on_note(category, 0) : on_category(category);
}

/* カテゴリの最後の止まる行。 */
static struct drawer_cursor last_stop_in(const struct folio_state *_Nonnull state, size_t category)
{
    size_t notes = visible_count(state, category);
    return notes > 0 ? on_note(category, notes - 1) : on_category(category);
}

/* 台帳の順で最初の止まる行。カテゴリが 1 つも無ければ false。 */
static bool first_stop(const struct folio_state *_Nonnull state, struct drawer_cursor *_Nonnull out)
{
    if (category_ledger_count(state->categories) == 0)
    {
        return false;
    }
    *out = first_stop_in(state, 0);
    return true;
}

/* 台帳の順で最後の止まる行。カテゴリが 1 つも無ければ false。 */
static bool last_stop(const struct folio_state *_Nonnull state, struct drawer_cursor *_Nonnull out)
{
    size_t count = category_ledger_count(state->categories);
    if (count == 0)
    {
        return false;
    }
    *out = last_stop_in(state, count - 1);
    return true;
}

/* from と同じカテゴリで、from の次に来るノート行の番号（見えるノート数以上なら次は無い）。
 * カテゴリ行のカーソルの次は、そのカテゴリの中の 1 本目になる。 */
static size_t after_in(struct drawer_cursor from)
{
    switch (from.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return 0;
    case DRAWER_ROW_NOTE:
        return from.ref.note + 1;
    }
    return 0;
}

/* from と同じカテゴリで、from より前にあるノート行の数（0 なら前に無い）。 */
static size_t before_in(const struct folio_state *_Nonnull state, struct drawer_cursor from)
{
    size_t notes = visible_count(state, from.ref.category);
    switch (from.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return 0;
    case DRAWER_ROW_NOTE:
        return from.ref.note < notes ? from.ref.note : notes;
    }
    return 0;
}

/* from より後ろにある最初の止まる行。端なら false。from が折り畳んだカテゴリの中のノートでも、
 * その位置から数え直す（自分のカテゴリ行へは戻らない）。 */
static bool next_stop(const struct folio_state *_Nonnull state, struct drawer_cursor from,
                      struct drawer_cursor *_Nonnull out)
{
    size_t count = category_ledger_count(state->categories);
    size_t start = after_in(from);
    for (size_t category = from.ref.category; category < count; ++category)
    {
        if (category != from.ref.category)
        {
            *out = first_stop_in(state, category);
            return true;
        }
        if (start < visible_count(state, category))
        {
            *out = on_note(category, start);
            return true;
        }
    }
    return false;
}

/* from より前にある最後の止まる行。端なら false。 */
static bool previous_stop(const struct folio_state *_Nonnull state, struct drawer_cursor from,
                          struct drawer_cursor *_Nonnull out)
{
    size_t reach = before_in(state, from);
    size_t category = from.ref.category + 1;
    while (category > 0)
    {
        category -= 1;
        if (category != from.ref.category)
        {
            *out = last_stop_in(state, category);
            return true;
        }
        if (reach > 0)
        {
            *out = on_note(category, reach - 1);
            return true;
        }
    }
    return false;
}

/* 歩みの行き先。端なら false（ADR 0015 の決定 2）。 */
static bool stepped_to(const struct folio_state *_Nonnull state, enum folio_step step,
                       struct drawer_cursor *_Nonnull out)
{
    struct drawer_cursor from = on_note(0, 0);
    bool any = current_cursor(state, &from);
    switch (step)
    {
    case FOLIO_STEP_NEXT:
        return any ? next_stop(state, from, out) : first_stop(state, out);
    case FOLIO_STEP_PREVIOUS:
        return any ? previous_stop(state, from, out) : last_stop(state, out);
    case FOLIO_STEP_FIRST:
        return first_stop(state, out);
    case FOLIO_STEP_LAST:
        return last_stop(state, out);
    }
    return false;
}

/* 行き先のノートへ移る。同じノートなら読み直さず、カーソルだけをそのノートへ戻す。 */
static enum folio_state_outcome stepped_to_note(struct folio_state *_Nonnull state,
                                                struct note_ref target)
{
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == target.category &&
        state->selected_note == target.note)
    {
        cursor_to_selection(state);
        return FOLIO_STATE_READY;
    }
    return folio_state_select_note(state, target.category, target.note);
}

enum folio_state_outcome folio_state_select_adjacent(struct folio_state *_Nonnull state,
                                                     enum folio_step step)
{
    struct drawer_cursor target = on_note(0, 0);
    if (!first_stop(state, &target))
    {
        /* 止まる行が 1 つも無い（カテゴリが無い）。 */
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    if (!stepped_to(state, step, &target))
    {
        /* 端では動かない（ADR 0015 の決定 2）。 */
        return FOLIO_STATE_READY;
    }
    switch (target.kind)
    {
    case DRAWER_ROW_CATEGORY:
        /* カテゴリ行ではカーソルだけが動く。選択も右ペインも変えない（決定 2）。 */
        cursor_to_category(state, target.ref.category);
        return FOLIO_STATE_READY;
    case DRAWER_ROW_NOTE:
        return stepped_to_note(state, target.ref);
    }
    return FOLIO_STATE_READY;
}

bool folio_state_step_kind(const struct folio_state *_Nonnull state, enum folio_step step,
                           enum folio_cursor_kind *_Nonnull kind, struct note_ref *_Nonnull out)
{
    struct drawer_cursor target = on_note(0, 0);
    if (!stepped_to(state, step, &target))
    {
        return false;
    }
    *kind = kind_of(target);
    *out = target.ref;
    return true;
}

bool folio_state_cursor(const struct folio_state *_Nonnull state,
                        enum folio_cursor_kind *_Nonnull kind, struct note_ref *_Nonnull out)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor))
    {
        return false;
    }
    *kind = kind_of(cursor);
    *out = cursor.ref;
    return true;
}

bool folio_state_selection(const struct folio_state *_Nonnull state, struct note_ref *_Nonnull out)
{
    if (state->document != FOLIO_DOCUMENT_NAMED)
    {
        return false;
    }
    *out = located(state->selected_category, state->selected_note);
    return true;
}

enum folio_state_outcome folio_state_begin_edit(struct folio_state *_Nonnull state)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    state->mode = PANE_MODE_EDIT;
    return FOLIO_STATE_READY;
}

enum folio_document_kind folio_state_document_kind(const struct folio_state *_Nonnull state)
{
    return state->document;
}

size_t folio_state_category_count(const struct folio_state *_Nonnull state)
{
    return category_ledger_count(state->categories);
}

size_t folio_state_document_category(const struct folio_state *_Nonnull state)
{
    return state->selected_category;
}

const char *_Nonnull folio_state_category_name(const struct folio_state *_Nonnull state,
                                               size_t category)
{
    return category_ledger_name(state->categories, category);
}

size_t folio_state_current_category(const struct folio_state *_Nonnull state)
{
    struct drawer_cursor cursor = {0};
    if (current_cursor(state, &cursor))
    {
        return cursor.ref.category;
    }
    return state->document == FOLIO_DOCUMENT_NONE ? 0 : state->selected_category;
}

enum folio_state_outcome folio_state_new_note(struct folio_state *_Nonnull state, size_t category)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    if (category >= folio_state_category_count(state))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    struct note_text *_Nullable body = nullptr;
    struct markdown_rtf *_Nullable pane = nullptr;
    if (note_text_create("", 0, &body) != NOTE_TEXT_ACCEPTED ||
        markdown_rtf_empty(state->palette, &pane) != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(body);
        markdown_rtf_destroy(pane);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    note_text_destroy(state->body);
    markdown_rtf_destroy(state->pane);
    state->body = body;
    state->pane = pane;
    state->document = FOLIO_DOCUMENT_UNTITLED;
    state->selected_category = category;
    state->mode = PANE_MODE_EDIT;
    state->cursor_any = false;
    return FOLIO_STATE_READY;
}

/* 書き戻す直前に、いまファイルにある本文を履歴へ写させる（ADR 0012 の決定 2）。
 * 元の md が無ければ写すものが無いので、そのまま書き戻しへ進む。 */
static enum folio_state_outcome archive_before_store(struct folio_state *_Nonnull state)
{
    enum persistence_outcome archived = state->port.archive_note(
        state->port.adapter, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note));
    if (archived == PERSISTENCE_STORED || archived == PERSISTENCE_ABSENT)
    {
        return FOLIO_STATE_READY;
    }
    return archived == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                 : FOLIO_STATE_HISTORY_FAILED;
}

/* 正規化済みの本文を書き戻し、表示値も作り直す。書けなければ何も変えない（ADR 0006 の決定 6）。
 * 履歴を残せなければ書き戻しにも進まない（ADR 0012 の決定 2）。 */
static enum folio_state_outcome store_edited(struct folio_state *_Nonnull state,
                                             struct note_text *_Nonnull edited)
{
    if (note_text_equals(state->body, edited))
    {
        note_text_destroy(edited);
        return FOLIO_STATE_READY;
    }
    enum folio_state_outcome archived = archive_before_store(state);
    if (archived != FOLIO_STATE_READY)
    {
        note_text_destroy(edited);
        return archived;
    }
    struct markdown_rtf *_Nullable rendered = nullptr;
    if (markdown_rtf_create(edited, state->palette, &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(edited);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum persistence_outcome stored = state->port.write_note(
        state->port.adapter, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note), edited);
    if (stored != PERSISTENCE_STORED)
    {
        markdown_rtf_destroy(rendered);
        note_text_destroy(edited);
        return stored == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_NOTE_STORE_FAILED;
    }
    markdown_rtf_destroy(state->pane);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = edited;
    return FOLIO_STATE_READY;
}

/* UI が持つ編集中の本文（UTF-16）を core で検証・変換し、読んだ本文の改行の形へ揃える。
 * 保存と読み取り専用の変更問い合わせがこの 1 本を共有する（C-014・ADR 0016 の決定 10）。 */
static enum folio_state_outcome edited_text(const struct folio_state *_Nonnull state,
                                            const char16_t *_Nonnull units, size_t count,
                                            struct note_text *_Nullable *_Nonnull out)
{
    if (state->mode != PANE_MODE_EDIT)
    {
        return FOLIO_STATE_NOT_EDITING;
    }
    struct utf8_text *_Nullable narrow = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(units, count, &narrow);
    if (converted != UTF8_TEXT_CONVERTED)
    {
        return converted == UTF8_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                    : FOLIO_STATE_NOTE_MALFORMED;
    }
    enum note_text_outcome accepted = note_text_from_editor(
        utf8_text_bytes(narrow), utf8_text_length(narrow), note_text_line_ending(state->body), out);
    utf8_text_destroy(narrow);
    if (accepted != NOTE_TEXT_ACCEPTED)
    {
        return accepted == NOTE_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_NOTE_MALFORMED;
    }
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome save_note(struct folio_state *_Nonnull state,
                                          const char16_t *_Nonnull units, size_t count)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }
    enum folio_state_outcome synced = synchronize_index(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    struct note_text *_Nullable edited = nullptr;
    enum folio_state_outcome outcome = edited_text(state, units, count, &edited);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    return store_edited(state, edited);
}

/* 副作用より先にすべてを確保。mdが公開された後は確保せず、表示をファイルへ揃える。 */
static enum folio_state_outcome create_edited(struct folio_state *_Nonnull state,
                                              const struct note_destination *_Nonnull destination,
                                              struct note_text *_Nonnull edited)
{
    size_t category = destination->category;
    const char *_Nonnull name = note_name_stem(destination->name);
    struct note_ledger *_Nullable ledger = nullptr;
    struct markdown_rtf *_Nullable rendered = nullptr;
    size_t index = note_ledger_count(state->notes[category]);
    enum folio_state_outcome prepared =
        from_note_ledger(note_ledger_inserted(state->notes[category], index, name, &ledger));
    if (prepared == FOLIO_STATE_READY &&
        markdown_rtf_create(edited, state->palette, &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        prepared = FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum persistence_outcome stored = PERSISTENCE_UNWRITABLE;
    if (prepared == FOLIO_STATE_READY)
    {
        stored = state->port.create_note(
            state->port.adapter, category_ledger_name(state->categories, category), name, edited);
    }
    if (stored != PERSISTENCE_STORED)
    {
        note_ledger_destroy(ledger);
        markdown_rtf_destroy(rendered);
        note_text_destroy(edited);
        return prepared != FOLIO_STATE_READY
                   ? prepared
                   : (stored == PERSISTENCE_UNWRITABLE ? FOLIO_STATE_NOTE_STORE_FAILED
                                                       : translate(stored));
    }
    note_ledger_destroy(state->notes[category]);
    state->notes[category] = ledger;
    markdown_rtf_destroy(state->pane);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = edited;
    state->document = FOLIO_DOCUMENT_NAMED;
    state->selected_category = category;
    state->selected_note = index;
    if (!state->cursor_any)
    {
        cursor_to_selection(state);
    }
    state->index_pending = true;
    state->pending_category = category;
    return synchronize_index(state);
}

enum folio_state_outcome folio_state_store_new(struct folio_state *_Nonnull state,
                                               const struct note_destination *_Nonnull destination,
                                               const char16_t *_Nonnull units, size_t count)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    if (state->document == FOLIO_DOCUMENT_NAMED)
    {
        return FOLIO_STATE_ALREADY_NAMED;
    }
    if (destination->category >= folio_state_category_count(state))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (holds_name(state->notes[destination->category], note_name_stem(destination->name)))
    {
        return FOLIO_STATE_NAME_TAKEN;
    }
    struct note_text *_Nullable edited = nullptr;
    enum folio_state_outcome converted = edited_text(state, units, count, &edited);
    if (converted != FOLIO_STATE_READY)
    {
        return converted;
    }
    return create_edited(state, destination, edited);
}

enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                const char16_t *_Nonnull units, size_t count)
{
    return save_note(state, units, count);
}

enum folio_state_outcome folio_state_note_changed(const struct folio_state *_Nonnull state,
                                                  const char16_t *_Nonnull units, size_t count,
                                                  enum folio_note_change *_Nonnull out)
{
    if (state->index_pending)
    {
        return FOLIO_STATE_LEDGER_STALE;
    }
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        *out = FOLIO_NOTE_CHANGED;
        return FOLIO_STATE_READY;
    }
    struct note_text *_Nullable edited = nullptr;
    enum folio_state_outcome outcome = edited_text(state, units, count, &edited);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    *out = note_text_equals(state->body, edited) ? FOLIO_NOTE_SAME : FOLIO_NOTE_CHANGED;
    note_text_destroy(edited);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_end_edit(struct folio_state *_Nonnull state,
                                              const char16_t *_Nonnull units, size_t count)
{
    enum folio_state_outcome outcome = save_note(state, units, count);
    if (outcome == FOLIO_STATE_READY)
    {
        state->mode = PANE_MODE_VIEW;
    }
    return outcome;
}

enum pane_mode folio_state_pane_mode(const struct folio_state *_Nonnull state)
{
    return state->mode;
}

const char *_Nonnull folio_state_pane_text(const struct folio_state *_Nonnull state)
{
    return note_text_bytes(state->body);
}

size_t folio_state_pane_text_length(const struct folio_state *_Nonnull state)
{
    return note_text_length(state->body);
}

struct pane_title_view folio_state_pane_title(const struct folio_state *_Nonnull state)
{
    struct pane_title_view title = {
        .any = false, .ordinal = 0, .category = "", .note = "", .color = {0, 0, 0}};
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return title;
    }
    title.any = true;
    title.ordinal = state->selected_category + 1;
    title.category = category_ledger_name(state->categories, state->selected_category);
    title.note =
        state->document == FOLIO_DOCUMENT_UNTITLED
            ? "無題（未保存）"
            : note_ledger_name(state->notes[state->selected_category], state->selected_note);
    title.color = category_ledger_color(state->categories, state->selected_category);
    return title;
}

const char *_Nonnull folio_state_pane_rtf(const struct folio_state *_Nonnull state)
{
    return markdown_rtf_text(state->pane);
}

size_t folio_state_pane_rtf_length(const struct folio_state *_Nonnull state)
{
    return markdown_rtf_length(state->pane);
}

const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome)
{
    switch (outcome)
    {
    case FOLIO_STATE_READY:
        return "";
    case FOLIO_STATE_DATA_UNREADABLE:
        return "data/ を読めませんでした。";
    case FOLIO_STATE_LEDGER_MALFORMED:
        return "data/ の台帳（categories.json / index.json）が版 1 の形ではありません。";
    case FOLIO_STATE_STORE_FAILED:
        return "data/ の台帳（categories.json / index.json）に書き戻せませんでした。表示は変えて"
               "いません。";
    case FOLIO_STATE_NO_SUCH_CATEGORY:
        return "索引に無いカテゴリが操作されました。";
    case FOLIO_STATE_NO_SUCH_NOTE:
        return "索引に無いノートが操作されました。";
    case FOLIO_STATE_NOTE_UNREADABLE:
        return "ノートを読めませんでした。表示は変えていません。";
    case FOLIO_STATE_NOTHING_SELECTED:
        return "ノートを選んでから編集してください。";
    case FOLIO_STATE_NOT_EDITING:
        return "編集モードではありません。";
    case FOLIO_STATE_NOTE_MALFORMED:
        return "編集中の本文に壊れた文字があります。保存していません。";
    case FOLIO_STATE_NOTE_STORE_FAILED:
        return "ノートを書き戻せませんでした。編集中の本文はそのままです。";
    case FOLIO_STATE_HISTORY_FAILED:
        return "履歴を書けなかったので保存していません。編集中の本文は残っています。";
    case FOLIO_STATE_UNSAVED_CHANGES:
        return "未保存の変更があります。保存するか、未保存変更を破棄して終了してください。";
    case FOLIO_STATE_NAME_TAKEN:
        return "同じ名前のノートがあります。別の名前を指定してください。既存ファイルは変更していま"
               "せん。";
    case FOLIO_STATE_LEDGER_STALE:
        return "mdは反映しましたが、台帳（index."
               "json）を書き戻せませんでした。保存を再試行するか、次回の起動で揃います。";
    case FOLIO_STATE_OUT_OF_MEMORY:
        return "記憶域が足りません。";
    case FOLIO_STATE_NAME_REQUIRED:
        return "無題のノートに名前をつけて保存してください。本文は残っています。";
    case FOLIO_STATE_INVALID_NAME:
        return "使えない名前です。予約名・末尾の空白やピリオド・区切りを避け、."
               "mdを含め255バイト以内で指定してください。";
    case FOLIO_STATE_ALREADY_NAMED:
        return "このノートには名前があります。別名保存（:saveas）または名前変更（:"
               "rename）を使ってください。";
    case FOLIO_STATE_CANCELLED:
        return "";
    }
    return "data/ を読めませんでした。";
}

void folio_state_destroy(struct folio_state *_Nullable state)
{
    if (state == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < state->notes_count; ++index)
    {
        note_ledger_destroy(state->notes[index]);
    }
    free(state->notes);
    category_ledger_destroy(state->categories);
    markdown_rtf_destroy(state->pane);
    note_text_destroy(state->body);
    free(state);
}
