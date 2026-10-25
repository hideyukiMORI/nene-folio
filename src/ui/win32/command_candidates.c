#include "command_candidates.h"
#include "utf8_text.h"

#include <stdlib.h>

static const struct
{
    enum folio_command_scope scope;
    enum ui_text label;
} groups[] = {
    {FOLIO_COMMAND_SCOPE_NOTE, UI_TEXT_HELP_GROUP_NOTE},
    {FOLIO_COMMAND_SCOPE_CATEGORY, UI_TEXT_HELP_GROUP_CATEGORY},
    {FOLIO_COMMAND_SCOPE_LAYOUT, UI_TEXT_HELP_GROUP_LAYOUT},
    {FOLIO_COMMAND_SCOPE_APP, UI_TEXT_HELP_GROUP_APP},
    {FOLIO_COMMAND_SCOPE_QUIT, UI_TEXT_HELP_GROUP_QUIT},
};

struct command_candidates
{
    bool valid;
    enum folio_language language;
    uint64_t revision;
    size_t count;
    size_t children;
    size_t headings[sizeof groups / sizeof groups[0]];
    struct
    {
        enum folio_command command;
        size_t ordinal;
    } items[];
};

enum candidate_create_outcome
command_candidates_create(struct command_candidates *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct command_candidates *_Nullable candidates = nullptr;
    size_t count = folio_command_count();
    if (count > (SIZE_MAX - sizeof *candidates) / sizeof candidates->items[0])
        return CANDIDATE_OUT_OF_MEMORY;
    candidates = calloc(1, sizeof *candidates + count * sizeof candidates->items[0]);
    if (candidates == nullptr)
        return CANDIDATE_OUT_OF_MEMORY;
    *out = candidates;
    return CANDIDATE_CREATED;
}

void command_candidates_destroy(struct command_candidates *_Nullable candidates)
{
    free(candidates);
}

void command_candidates_invalidate(struct command_candidates *_Nonnull candidates)
{
    candidates->valid = false;
}

bool command_candidates_current(const struct command_candidates *_Nonnull candidates,
                                enum folio_language language, uint64_t revision)
{
    return candidates->valid && revision != UINT64_MAX && candidates->revision == revision &&
           candidates->language == language;
}

static void append_group(struct command_candidates *_Nonnull candidates,
                         const struct utf8_text *_Nonnull query, size_t group)
{
    candidates->headings[group] = SIZE_MAX;
    for (size_t at = 0; at < folio_command_count(); ++at)
    {
        enum folio_command command = folio_command_at(at);
        if (!folio_command_listed(command) || folio_command_scope(command) != groups[group].scope ||
            !folio_command_matches(command, utf8_text_bytes(query), utf8_text_length(query),
                                   candidates->language))
            continue;
        if (candidates->headings[group] == SIZE_MAX)
            candidates->headings[group] = candidates->children++;
        candidates->items[candidates->count].command = command;
        candidates->items[candidates->count].ordinal = candidates->children++;
        candidates->count += 1;
    }
}

void command_candidates_prepare(struct command_candidates *_Nonnull candidates,
                                const struct utf8_text *_Nonnull query,
                                enum folio_language language, uint64_t revision)
{
    candidates->valid = false;
    candidates->count = 0;
    candidates->children = 0;
    candidates->language = language;
    candidates->revision = revision;
    for (size_t group = 0; group < command_candidates_group_count(); ++group)
        append_group(candidates, query, group);
    candidates->valid = true;
}

size_t command_candidates_count(const struct command_candidates *_Nonnull candidates)
{
    return candidates->valid ? candidates->count : 0;
}

size_t command_candidates_child_count(const struct command_candidates *_Nonnull candidates)
{
    return candidates->valid ? candidates->children : 0;
}

bool command_candidates_at(const struct command_candidates *_Nonnull candidates, size_t index,
                           enum folio_command *_Nonnull out)
{
    if (index >= command_candidates_count(candidates))
        return false;
    *out = candidates->items[index].command;
    return true;
}

bool command_candidates_ordinal(const struct command_candidates *_Nonnull candidates,
                                enum command_candidate_kind kind, size_t slot, size_t *_Nonnull out)
{
    if (!candidates->valid)
        return false;
    switch (kind)
    {
    case COMMAND_CANDIDATE_GROUP:
        if (slot >= command_candidates_group_count() || candidates->headings[slot] == SIZE_MAX)
            return false;
        *out = candidates->headings[slot];
        return true;
    case COMMAND_CANDIDATE_COMMAND:
        if (slot >= candidates->count)
            return false;
        *out = candidates->items[slot].ordinal;
        return true;
    }
    return false;
}

bool command_candidates_child(const struct command_candidates *_Nonnull candidates, size_t index,
                              enum command_candidate_kind *_Nonnull kind, size_t *_Nonnull slot)
{
    if (index >= command_candidates_child_count(candidates))
        return false;
    for (size_t group = 0; group < command_candidates_group_count(); ++group)
    {
        if (candidates->headings[group] == index)
        {
            *kind = COMMAND_CANDIDATE_GROUP;
            *slot = group;
            return true;
        }
    }
    size_t low = 0;
    size_t high = candidates->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (candidates->items[middle].ordinal < index)
            low = middle + 1;
        else
            high = middle;
    }
    *kind = COMMAND_CANDIDATE_COMMAND;
    *slot = low;
    return low < candidates->count && candidates->items[low].ordinal == index;
}

size_t command_candidates_group_count(void)
{
    return sizeof groups / sizeof groups[0];
}

size_t command_candidates_group_index(enum folio_command command)
{
    for (size_t group = 0; group < command_candidates_group_count(); ++group)
        if (groups[group].scope == folio_command_scope(command))
            return group;
    return command_candidates_group_count();
}

enum ui_text command_candidates_group_label(size_t group)
{
    return groups[group].label;
}
