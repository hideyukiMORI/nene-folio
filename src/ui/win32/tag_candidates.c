#include "tag_candidates.h"
#include "ascii_fold.h"
#include "note_tag.h"
#include "note_tags.h"
#include "tag_candidates_source.h"
#include "tag_catalog.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct tag_candidates
{
    bool valid;
    uint64_t revision;
    size_t count;
    struct note_tag *_Nonnull *_Nullable names;
    size_t *_Nullable uses;
};

enum candidate_create_outcome tag_candidates_create(struct tag_candidates *_Nullable *_Nonnull out)
{
    struct tag_candidates *_Nullable made = calloc(1, sizeof *made);
    if (made == nullptr)
        return CANDIDATE_OUT_OF_MEMORY;
    *out = made;
    return CANDIDATE_CREATED;
}

void tag_candidates_invalidate(struct tag_candidates *_Nonnull candidates)
{
    for (size_t at = 0; at < candidates->count; ++at)
        note_tag_destroy(candidates->names[at]);
    free(candidates->names);
    free(candidates->uses);
    candidates->names = nullptr;
    candidates->uses = nullptr;
    candidates->count = 0;
    candidates->valid = false;
}

void tag_candidates_destroy(struct tag_candidates *_Nullable candidates)
{
    if (candidates == nullptr)
        return;
    tag_candidates_invalidate(candidates);
    free(candidates);
}

bool tag_candidates_current(const struct tag_candidates *_Nonnull candidates, uint64_t revision)
{
    return candidates->valid && revision != UINT64_MAX && candidates->revision == revision;
}

static bool matches_at(const char *_Nonnull name, const char *_Nonnull query, size_t count)
{
    for (size_t at = 0; at < count; ++at)
        if (ascii_fold_byte(name[at]) != ascii_fold_byte(query[at]))
            return false;
    return true;
}

static bool matches(const struct note_tag *_Nonnull tag, const struct utf8_text *_Nonnull query)
{
    size_t length = note_tag_length(tag);
    size_t wanted = utf8_text_length(query);
    if (wanted > length)
        return false;
    for (size_t at = 0; at <= length - wanted; ++at)
        if (matches_at(note_tag_bytes(tag) + at, utf8_text_bytes(query), wanted))
            return true;
    return false;
}

static bool append(struct tag_candidates *_Nonnull candidates, const struct note_tag *_Nonnull tag,
                   size_t usage, const struct utf8_text *_Nonnull query)
{
    if (!matches(tag, query))
        return true;
    struct note_tag *_Nullable copied = nullptr;
    if (note_tag_create(note_tag_bytes(tag), note_tag_length(tag), &copied) != NOTE_TAG_READY)
        return false;
    candidates->names[candidates->count] = copied;
    candidates->uses[candidates->count++] = usage;
    return true;
}

static bool copy_catalog(struct tag_candidates *_Nonnull candidates,
                         const struct tag_candidates_source *_Nonnull source)
{
    if (source->catalog == nullptr)
        return true;
    for (size_t at = 0; at < tag_catalog_count(source->catalog); ++at)
        if (!append(candidates, tag_catalog_tag(source->catalog, at),
                    tag_catalog_usage(source->catalog, at), source->query))
            return false;
    return true;
}

static bool copy_selected(struct tag_candidates *_Nonnull candidates,
                          const struct tag_candidates_source *_Nonnull source)
{
    if (source->selected == nullptr)
        return true;
    for (size_t at = 0; at < note_tags_count(source->selected); ++at)
    {
        const struct note_tag *_Nonnull tag = note_tags_at(source->selected, at);
        if (source->catalog != nullptr && tag_catalog_contains(source->catalog, tag))
            continue;
        if (!append(candidates, tag, 0, source->query))
            return false;
    }
    return true;
}

static bool reserve(struct tag_candidates *_Nonnull candidates,
                    const struct tag_candidates_source *_Nonnull source)
{
    size_t count = source->catalog == nullptr ? 0 : tag_catalog_count(source->catalog);
    size_t extra = source->selected == nullptr ? 0 : note_tags_count(source->selected);
    if (count >= SIZE_MAX / sizeof *candidates->names - extra)
        return false;
    candidates->names = malloc((count + extra + 1) * sizeof *candidates->names);
    candidates->uses = malloc((count + extra + 1) * sizeof *candidates->uses);
    return candidates->names != nullptr && candidates->uses != nullptr;
}

enum candidate_create_outcome
tag_candidates_prepare(struct tag_candidates *_Nonnull candidates,
                       const struct tag_candidates_source *_Nonnull source)
{
    tag_candidates_invalidate(candidates);
    if (!reserve(candidates, source) || !copy_catalog(candidates, source) ||
        !copy_selected(candidates, source))
    {
        tag_candidates_invalidate(candidates);
        return CANDIDATE_OUT_OF_MEMORY;
    }
    candidates->revision = source->revision;
    candidates->valid = true;
    return CANDIDATE_CREATED;
}

size_t tag_candidates_count(const struct tag_candidates *_Nonnull candidates)
{
    return candidates->valid ? candidates->count : 0;
}

const struct note_tag *_Nonnull tag_candidates_tag(const struct tag_candidates *_Nonnull candidates,
                                                   size_t index)
{
    return candidates->names[index];
}

size_t tag_candidates_usage(const struct tag_candidates *_Nonnull candidates, size_t index)
{
    return candidates->uses[index];
}
