#include "note_tags.h"
#include "note_tag.h"
#include <stdlib.h>

struct note_tags
{
    struct note_tag *_Nullable names[note_tags_max_count];
    size_t count;
};

bool note_tags_contains(const struct note_tags *_Nonnull tags, const struct note_tag *_Nonnull tag)
{
    for (size_t index = 0; index < tags->count; ++index)
        if (note_tag_equal(tags->names[index], tag))
            return true;
    return false;
}

enum note_tags_outcome note_tags_create(const struct note_tag *_Nonnull const *_Nonnull names,
                                        size_t count, struct note_tags *_Nullable *_Nonnull out)
{
    if (count > note_tags_max_count)
        return NOTE_TAGS_TOO_MANY;
    struct note_tags *_Nullable tags = nullptr;
    if (note_tags_empty(&tags) != NOTE_TAGS_READY)
        return NOTE_TAGS_OUT_OF_MEMORY;
    for (size_t index = 0; index < count; ++index)
    {
        const struct note_tag *_Nonnull name = names[index];
        if (note_tags_contains(tags, name))
            continue;
        if (note_tag_create(note_tag_bytes(name), note_tag_length(name),
                            &tags->names[tags->count]) != NOTE_TAG_READY)
        {
            note_tags_destroy(tags);
            return NOTE_TAGS_OUT_OF_MEMORY;
        }
        ++tags->count;
    }
    *out = tags;
    return NOTE_TAGS_READY;
}

enum note_tags_outcome note_tags_empty(struct note_tags *_Nullable *_Nonnull out)
{
    struct note_tags *_Nullable tags = calloc(1, sizeof *tags);
    if (tags == nullptr)
        return NOTE_TAGS_OUT_OF_MEMORY;
    *out = tags;
    return NOTE_TAGS_READY;
}

enum note_tags_outcome note_tags_copy(const struct note_tags *_Nonnull tags,
                                      struct note_tags *_Nullable *_Nonnull out)
{
    const struct note_tag *_Nonnull names[note_tags_max_count];
    for (size_t index = 0; index < tags->count; ++index)
        names[index] = tags->names[index];
    return note_tags_create(names, tags->count, out);
}

enum note_tags_outcome note_tags_with(const struct note_tags *_Nonnull tags,
                                      const struct note_tag *_Nonnull tag,
                                      enum note_tag_presence presence,
                                      struct note_tags *_Nullable *_Nonnull out)
{
    bool included = false;
    switch (presence)
    {
    case NOTE_TAG_ABSENT:
        break;
    case NOTE_TAG_PRESENT:
        included = true;
        break;
    }
    const struct note_tag *_Nonnull names[note_tags_max_count + 1];
    size_t count = 0;
    bool present = false;
    for (size_t index = 0; index < tags->count; ++index)
    {
        bool equal = note_tag_equal(tags->names[index], tag);
        present = present || equal;
        if (!equal || included)
            names[count++] = tags->names[index];
    }
    if (included && !present)
        names[count++] = tag;
    return note_tags_create(names, count, out);
}

size_t note_tags_count(const struct note_tags *_Nonnull tags)
{
    return tags->count;
}

const struct note_tag *_Nonnull note_tags_at(const struct note_tags *_Nonnull tags, size_t index)
{
    return tags->names[index];
}

bool note_tags_equal(const struct note_tags *_Nonnull left, const struct note_tags *_Nonnull right)
{
    if (left->count != right->count)
        return false;
    for (size_t index = 0; index < left->count; ++index)
        if (!note_tags_contains(right, left->names[index]))
            return false;
    return true;
}

void note_tags_destroy(struct note_tags *_Nullable tags)
{
    if (tags == nullptr)
        return;
    for (size_t index = 0; index < tags->count; ++index)
        note_tag_destroy(tags->names[index]);
    free(tags);
}
