#include "folio_state.h"

#include "appearance_port.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "markdown_rtf.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_text.h"
#include "persistence_port.h"
#include "rtf_palette.h"
#include "utf8_text.h"

#include <stdlib.h>

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
    bool selected; /* ノートを選んでいるか */
    size_t selected_category;
    size_t selected_note;
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
    }
    return FOLIO_STATE_DATA_UNREADABLE;
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
    if (state->selected)
    {
        drawer_layout_select(*out, state->selected_category, state->selected_note);
    }
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
        return stored == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_STORE_FAILED;
    }
    category_ledger_destroy(state->categories);
    state->categories = toggled;
    return FOLIO_STATE_READY;
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
        state->selected = true;
        state->selected_category = category;
        state->selected_note = note;
    }
    return outcome;
}

enum folio_state_outcome folio_state_begin_edit(struct folio_state *_Nonnull state)
{
    if (!state->selected)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    state->mode = PANE_MODE_EDIT;
    return FOLIO_STATE_READY;
}

/* 正規化済みの本文を書き戻し、表示値も作り直す。書けなければ何も変えない（ADR 0006 の決定 6）。 */
static enum folio_state_outcome store_edited(struct folio_state *_Nonnull state,
                                             struct note_text *_Nonnull edited)
{
    if (note_text_equals(state->body, edited))
    {
        note_text_destroy(edited);
        return FOLIO_STATE_READY;
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
 * application が UTF-16 を受けるのはこの 1 本だけ（C-014 の例外・ADR 0006 の決定 4）。 */
static enum folio_state_outcome save_note(struct folio_state *_Nonnull state,
                                          const char16_t *_Nonnull units, size_t count)
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
    struct note_text *_Nullable edited = nullptr;
    enum note_text_outcome accepted =
        note_text_from_editor(utf8_text_bytes(narrow), utf8_text_length(narrow),
                              note_text_line_ending(state->body), &edited);
    utf8_text_destroy(narrow);
    if (accepted != NOTE_TEXT_ACCEPTED)
    {
        return accepted == NOTE_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_NOTE_MALFORMED;
    }
    return store_edited(state, edited);
}

enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                const char16_t *_Nonnull units, size_t count)
{
    return save_note(state, units, count);
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
    if (!state->selected)
    {
        return title;
    }
    title.any = true;
    title.ordinal = state->selected_category + 1;
    title.category = category_ledger_name(state->categories, state->selected_category);
    title.note = note_ledger_name(state->notes[state->selected_category], state->selected_note);
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
        return "data/categories.json に書き戻せませんでした。表示は変えていません。";
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
    case FOLIO_STATE_OUT_OF_MEMORY:
        return "記憶域が足りません。";
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
