#include "tag_catalog.h"
#include "note_tag.h"
#include "note_tags.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct tag_catalog
{
    struct note_tag *_Nonnull *_Nonnull names;
    size_t *_Nonnull uses;
    size_t count;
    size_t unavailable;
};

static int compare(const struct note_tag *_Nonnull left, const struct note_tag *_Nonnull right)
{
    return strcmp(note_tag_bytes(left), note_tag_bytes(right));
}

static void exchange(const struct note_tag *_Nonnull *_Nonnull names, size_t left, size_t right)
{
    const struct note_tag *_Nonnull held = names[left];
    names[left] = names[right];
    names[right] = held;
}

static void sift(const struct note_tag *_Nonnull *_Nonnull names, size_t count, size_t root)
{
    while (root < count / 2)
    {
        size_t child = root * 2 + 1;
        if (child + 1 < count && compare(names[child], names[child + 1]) < 0)
            child += 1;
        if (compare(names[root], names[child]) >= 0)
            return;
        exchange(names, root, child);
        root = child;
    }
}

static void sort(const struct note_tag *_Nonnull *_Nonnull names, size_t count)
{
    for (size_t root = count / 2; root > 0; --root)
        sift(names, count, root - 1);
    for (size_t end = count; end > 1; --end)
    {
        exchange(names, 0, end - 1);
        sift(names, end - 1, 0);
    }
}

static bool count_names(const struct note_tags *_Nullable const *_Nonnull notes, size_t count,
                        size_t *_Nonnull total, size_t *_Nonnull unavailable)
{
    if (count >= SIZE_MAX / sizeof *notes)
        return false;
    for (size_t index = 0; index < count; ++index)
    {
        const struct note_tags *_Nullable tags = notes[index];
        if (tags == nullptr)
        {
            *unavailable += 1;
            continue;
        }
        size_t added = note_tags_count(tags);
        if (added > SIZE_MAX / sizeof(const struct note_tag *) - 1 - *total)
            return false;
        *total += added;
    }
    return true;
}

static size_t collect(const struct note_tags *_Nullable const *_Nonnull notes, size_t count,
                      const struct note_tag *_Nonnull *_Nonnull names)
{
    size_t at = 0;
    for (size_t index = 0; index < count; ++index)
    {
        const struct note_tags *_Nullable tags = notes[index];
        if (tags == nullptr)
            continue;
        for (size_t item = 0; item < note_tags_count(tags); ++item)
            names[at++] = note_tags_at(tags, item);
    }
    return at;
}

static size_t distinct(const struct note_tag *_Nonnull const *_Nonnull names, size_t count)
{
    size_t result = 0;
    for (size_t index = 0; index < count; ++index)
        result += index == 0 || !note_tag_equal(names[index - 1], names[index]) ? 1 : 0;
    return result;
}

static void parts_destroy(struct tag_catalog *_Nullable catalog,
                          struct note_tag *_Nonnull *_Nullable names, size_t *_Nullable uses,
                          size_t count)
{
    for (size_t index = 0; index < count; ++index)
        note_tag_destroy(names[index]);
    free(names);
    free(uses);
    free(catalog);
}

static bool copy_names(struct tag_catalog *_Nonnull catalog,
                       const struct note_tag *_Nonnull const *_Nonnull names, size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        if (index != 0 && note_tag_equal(names[index - 1], names[index]))
        {
            catalog->uses[catalog->count - 1] += 1;
            continue;
        }
        struct note_tag *_Nullable copied = nullptr;
        if (note_tag_create(note_tag_bytes(names[index]), note_tag_length(names[index]), &copied) !=
            NOTE_TAG_READY)
            return false;
        catalog->names[catalog->count] = copied;
        catalog->uses[catalog->count] = 1;
        catalog->count += 1;
    }
    return true;
}

static enum tag_catalog_outcome freeze(const struct note_tag *_Nonnull const *_Nonnull source,
                                       size_t count, size_t unavailable,
                                       struct tag_catalog *_Nullable *_Nonnull out)
{
    size_t unique = distinct(source, count);
    if (unique >= SIZE_MAX / sizeof(size_t))
        return TAG_CATALOG_OUT_OF_MEMORY;
    struct tag_catalog *_Nullable catalog = calloc(1, sizeof *catalog);
    struct note_tag *_Nonnull *_Nullable names = malloc((unique + 1) * sizeof *names);
    size_t *_Nullable uses = malloc((unique + 1) * sizeof *uses);
    if (catalog == nullptr || names == nullptr || uses == nullptr)
    {
        parts_destroy(catalog, names, uses, 0);
        return TAG_CATALOG_OUT_OF_MEMORY;
    }
    catalog->names = names;
    catalog->uses = uses;
    catalog->unavailable = unavailable;
    if (!copy_names(catalog, source, count))
    {
        parts_destroy(catalog, names, uses, catalog->count);
        return TAG_CATALOG_OUT_OF_MEMORY;
    }
    *out = catalog;
    return TAG_CATALOG_READY;
}

static void scratch_destroy(const struct note_tag *_Nonnull *_Nullable names)
{
    free(names);
}

enum tag_catalog_outcome tag_catalog_create(const struct note_tags *_Nullable const *_Nonnull notes,
                                            size_t count,
                                            struct tag_catalog *_Nullable *_Nonnull out)
{
    size_t total = 0;
    size_t unavailable = 0;
    if (!count_names(notes, count, &total, &unavailable))
        return TAG_CATALOG_OUT_OF_MEMORY;
    const struct note_tag *_Nonnull *_Nullable names = malloc((total + 1) * sizeof *names);
    if (names == nullptr)
        return TAG_CATALOG_OUT_OF_MEMORY;
    size_t filled = collect(notes, count, names);
    sort(names, filled);
    enum tag_catalog_outcome outcome = freeze(names, filled, unavailable, out);
    scratch_destroy(names);
    return outcome;
}

size_t tag_catalog_count(const struct tag_catalog *_Nonnull catalog)
{
    return catalog->count;
}

const struct note_tag *_Nonnull tag_catalog_tag(const struct tag_catalog *_Nonnull catalog,
                                                size_t index)
{
    return catalog->names[index];
}

size_t tag_catalog_usage(const struct tag_catalog *_Nonnull catalog, size_t index)
{
    return catalog->uses[index];
}

size_t tag_catalog_unavailable(const struct tag_catalog *_Nonnull catalog)
{
    return catalog->unavailable;
}

bool tag_catalog_contains(const struct tag_catalog *_Nonnull catalog,
                          const struct note_tag *_Nonnull tag)
{
    size_t low = 0;
    size_t high = catalog->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        int order = compare(catalog->names[middle], tag);
        if (order == 0)
            return true;
        if (order < 0)
            low = middle + 1;
        if (order > 0)
            high = middle;
    }
    return false;
}

void tag_catalog_destroy(struct tag_catalog *_Nullable catalog)
{
    if (catalog == nullptr)
        return;
    parts_destroy(catalog, catalog->names, catalog->uses, catalog->count);
}
