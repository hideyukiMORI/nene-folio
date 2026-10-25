#include "theme_candidates.h"
#include "utf8_text.h"

#include <stdlib.h>

struct theme_candidates
{
    bool valid;
    enum folio_language language;
    uint64_t revision;
    size_t count;
    enum folio_theme_choice items[];
};

enum candidate_create_outcome
theme_candidates_create(struct theme_candidates *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct theme_candidates *_Nullable candidates = nullptr;
    size_t count = folio_theme_choice_count();
    if (count > (SIZE_MAX - sizeof *candidates) / sizeof candidates->items[0])
        return CANDIDATE_OUT_OF_MEMORY;
    candidates = calloc(1, sizeof *candidates + count * sizeof candidates->items[0]);
    if (candidates == nullptr)
        return CANDIDATE_OUT_OF_MEMORY;
    *out = candidates;
    return CANDIDATE_CREATED;
}

void theme_candidates_destroy(struct theme_candidates *_Nullable candidates)
{
    free(candidates);
}

void theme_candidates_invalidate(struct theme_candidates *_Nonnull candidates)
{
    candidates->valid = false;
}

bool theme_candidates_current(const struct theme_candidates *_Nonnull candidates,
                              enum folio_language language, uint64_t revision)
{
    return candidates->valid && revision != UINT64_MAX && candidates->revision == revision &&
           candidates->language == language;
}

void theme_candidates_prepare(struct theme_candidates *_Nonnull candidates,
                              const struct utf8_text *_Nonnull query, enum folio_language language,
                              uint64_t revision)
{
    candidates->valid = false;
    candidates->count = 0;
    for (size_t at = 0; at < folio_theme_choice_count(); ++at)
    {
        enum folio_theme_choice choice = folio_theme_choice_at(at);
        if (folio_theme_choice_matches(choice, utf8_text_bytes(query), utf8_text_length(query),
                                       language))
            candidates->items[candidates->count++] = choice;
    }
    candidates->language = language;
    candidates->revision = revision;
    candidates->valid = true;
}

size_t theme_candidates_count(const struct theme_candidates *_Nonnull candidates)
{
    return candidates->valid ? candidates->count : 0;
}

bool theme_candidates_at(const struct theme_candidates *_Nonnull candidates, size_t index,
                         enum folio_theme_choice *_Nonnull out)
{
    if (index >= theme_candidates_count(candidates))
        return false;
    *out = candidates->items[index];
    return true;
}
