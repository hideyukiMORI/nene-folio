#include "folio_state.h"

#include "category_ledger.h"
#include "drawer_layout.h"
#include "name_list.h"
#include "note_ledger.h"
#include "persistence_port.h"

#include <stdlib.h>

struct folio_state
{
    struct persistence_port port;
    struct category_ledger *_Nullable categories;
    struct note_ledger *_Nonnull *_Nullable notes; /* categories と同じ数・同じ順 */
    size_t notes_count;
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

enum folio_state_outcome folio_state_create(const struct persistence_port *_Nonnull port,
                                            struct folio_state *_Nullable *_Nonnull out)
{
    struct folio_state *_Nullable state = calloc(1, sizeof *state);
    if (state == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    state->port = *port;
    enum folio_state_outcome outcome = load_categories(state, port);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = load_all_notes(state, port);
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
    return FOLIO_STATE_READY;
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
    free(state);
}
