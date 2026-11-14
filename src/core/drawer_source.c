#include "drawer_source.h"
#include "category_ledger.h"
#include "index_filter.h"
#include "note_ledger.h"

bool drawer_source_filtered(const struct drawer_source *_Nonnull source)
{
    return source->filter != nullptr || source->scope == NOTE_SCOPE_FAVORITES;
}

bool drawer_source_shown_note(const struct drawer_source *_Nonnull source, struct note_ref ref)
{
    if (source->filter != nullptr && !index_filter_note(source->filter, ref))
        return false;
    switch (source->scope)
    {
    case NOTE_SCOPE_ALL:
        return true;
    case NOTE_SCOPE_FAVORITES:
        return note_ledger_favorite(source->notes[ref.category], ref.note);
    }
    return false;
}

static bool starred_category(const struct drawer_source *_Nonnull source, size_t category)
{
    const struct note_ledger *ledger = source->notes[category];
    size_t count = note_ledger_favorite_count(ledger);
    if (source->filter == nullptr)
        return count != 0;
    for (size_t position = 0; position < count; ++position)
    {
        size_t note = note_ledger_ordered(ledger, NOTE_ORDER_FAVORITES_FIRST, position);
        struct note_ref ref = {.category = category, .note = note};
        if (drawer_source_shown_note(source, ref))
            return true;
    }
    return false;
}

bool drawer_source_shown_category(const struct drawer_source *_Nonnull source, size_t category)
{
    switch (source->scope)
    {
    case NOTE_SCOPE_ALL:
        return source->filter == nullptr || index_filter_category(source->filter, category);
    case NOTE_SCOPE_FAVORITES:
        return starred_category(source, category);
    }
    return false;
}

bool drawer_source_expanded(const struct drawer_source *_Nonnull source, size_t category)
{
    return drawer_source_filtered(source) || category_ledger_expanded(source->categories, category);
}
