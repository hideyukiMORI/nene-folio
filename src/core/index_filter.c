#include "index_filter.h"
#include "index_filter_build.h"
#include "index_filter_wrap_outcome.h"
#include <stdlib.h>

struct index_filter
{
    struct index_filter_build *_Nonnull sealed;
    size_t count;
};

/* Only callers that have proved READY enter this ownership commit. */
static enum index_filter_wrap_outcome wrap_sealed(struct index_filter_build *_Nonnull sealed,
                                                  struct index_filter *_Nullable *_Nonnull out)
{
    struct index_filter *_Nullable filter = malloc(sizeof *filter);
    if (filter == nullptr)
        return INDEX_FILTER_WRAP_OUT_OF_MEMORY;
    filter->sealed = sealed;
    filter->count = index_filter_build_count(sealed);
    *out = filter;
    return INDEX_FILTER_WRAP_ACCEPTED;
}

enum index_filter_finish_outcome
index_filter_finish(struct index_filter_build *_Nullable *_Nonnull build,
                    struct index_filter *_Nullable *_Nonnull out)
{
    switch (index_filter_build_readiness(*build))
    {
    case INDEX_FILTER_NOT_READY:
        return INDEX_FILTER_FINISH_NOT_READY;
    case INDEX_FILTER_READY:
        break;
    }
    switch (wrap_sealed(*build, out))
    {
    case INDEX_FILTER_WRAP_OUT_OF_MEMORY:
        return INDEX_FILTER_FINISH_OUT_OF_MEMORY;
    case INDEX_FILTER_WRAP_ACCEPTED:
        *build = nullptr;
        return INDEX_FILTER_FINISH_ACCEPTED;
    }
}

enum index_filter_outcome index_filter_removed(const struct index_filter *_Nonnull filter,
                                               struct note_ref target,
                                               struct index_filter *_Nullable *_Nonnull out)
{
    struct index_filter_build *_Nullable removed = nullptr;
    enum index_filter_outcome built = index_filter_build_removed(filter->sealed, target, &removed);
    if (built != INDEX_FILTER_ACCEPTED)
        return built;
    switch (wrap_sealed(removed, out))
    {
    case INDEX_FILTER_WRAP_OUT_OF_MEMORY:
        index_filter_build_destroy(removed);
        return INDEX_FILTER_OUT_OF_MEMORY;
    case INDEX_FILTER_WRAP_ACCEPTED:
        return INDEX_FILTER_ACCEPTED;
    }
}

static size_t lower_bound(const struct index_filter *_Nonnull filter, size_t category, size_t note)
{
    size_t low = 0;
    size_t high = filter->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        struct note_ref ref = index_filter_build_ref(filter->sealed, middle);
        bool earlier = ref.category < category || (ref.category == category && ref.note < note);
        low = earlier ? middle + 1 : low;
        high = earlier ? high : middle;
    }
    return low;
}

bool index_filter_note(const struct index_filter *_Nonnull filter, struct note_ref ref)
{
    size_t index = lower_bound(filter, ref.category, ref.note);
    if (index == filter->count)
        return false;
    struct note_ref found = index_filter_build_ref(filter->sealed, index);
    return found.category == ref.category && found.note == ref.note;
}

bool index_filter_category(const struct index_filter *_Nonnull filter, size_t category)
{
    size_t index = lower_bound(filter, category, 0);
    return index < filter->count &&
           index_filter_build_ref(filter->sealed, index).category == category;
}

size_t index_filter_count(const struct index_filter *_Nonnull filter)
{
    return filter->count;
}

void index_filter_destroy(struct index_filter *_Nullable filter)
{
    if (filter == nullptr)
        return;
    index_filter_build_destroy(filter->sealed);
    free(filter);
}
