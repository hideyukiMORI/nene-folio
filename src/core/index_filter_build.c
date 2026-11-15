#include "index_filter_build.h"
#include "ascii_fold.h"
#include "index_filter_rank.h"
#include "note_tag.h"
#include "note_tags.h"
#include "utf8_text.h"
#include <stdint.h>
#include <stdlib.h>

constexpr size_t rank_kinds = 9;
constexpr size_t rank_buckets = rank_kinds * 2;

struct index_filter_build
{
    enum
    {
        BUILD_ENTRY,
        BUILD_TAGS,
        BUILD_NAME_LENGTH,
        BUILD_SINGLE,
        BUILD_TAIL,
        BUILD_SCAN,
        BUILD_SUBSEQUENCE,
        BUILD_BODY,
        BUILD_CATEGORY,
        BUILD_DONE,
        BUILD_PROJECT,
        BUILD_SEALED
    } phase;
    size_t *_Nonnull categories;
    size_t *_Nonnull notes;
    size_t *_Nullable prefix;
    struct index_filter_rank *_Nullable ranks;
    enum index_match_scope scope;
    enum index_match_kind kind;
    bool favorites_only;
    bool favorites_first;
    size_t heads[rank_buckets];
    size_t tails[rank_buckets];
    size_t projected[index_ranked_limit];
    size_t ranked_count;
    size_t project_bucket;
    size_t project_at;
    const char *_Nullable term;
    const struct note_tags *_Nullable tags;
    enum tag_match_mode tag_mode;
    size_t tag_query;
    size_t tag_entry;
    size_t tag_byte;
    size_t term_length;
    size_t steps[256];
    char first;
    char second;
    size_t entries;
    size_t processed;
    size_t count;
    size_t name_length;
    size_t source_length;
    size_t at;
    size_t compared;
    enum
    {
        SOURCE_NAME,
        SOURCE_BODY,
        SOURCE_CATEGORY
    } source;
    bool matched;
};

static void prefix_destroy(size_t *_Nullable prefix)
{
    free(prefix);
}

static void ranks_destroy(struct index_filter_rank *_Nullable ranks)
{
    free(ranks);
}

static void parts_destroy(struct index_filter_build *_Nullable build, size_t *_Nullable categories,
                          size_t *_Nullable notes, size_t *_Nullable prefix)
{
    prefix_destroy(prefix);
    free(categories);
    free(notes);
    free(build);
}

static bool storage_fits(size_t count, size_t prefix_length, enum index_match_scope scope)
{
    return count < SIZE_MAX / sizeof(size_t) && prefix_length <= SIZE_MAX / sizeof(size_t) &&
           (scope == INDEX_MATCH_NAME_BODY || count < SIZE_MAX / sizeof(struct index_filter_rank));
}

static enum index_filter_outcome allocate_build(size_t count, size_t prefix_length,
                                                enum index_match_scope scope,
                                                struct index_filter_build *_Nullable *_Nonnull out)
{
    if (!storage_fits(count, prefix_length, scope))
        return INDEX_FILTER_OUT_OF_MEMORY;
    struct index_filter_build *_Nullable build = calloc(1, sizeof *build);
    if (build == nullptr)
        return INDEX_FILTER_OUT_OF_MEMORY;
    size_t *_Nullable categories = malloc((count + 1) * sizeof *categories);
    size_t *_Nullable notes = malloc((count + 1) * sizeof *notes);
    size_t *_Nullable prefix =
        prefix_length == 0 ? nullptr : malloc(prefix_length * sizeof *prefix);
    struct index_filter_rank *_Nullable ranks =
        scope == INDEX_MATCH_NAMES ? malloc((count + 1) * sizeof *ranks) : nullptr;
    if (categories == nullptr || notes == nullptr || (prefix_length != 0 && prefix == nullptr) ||
        (scope == INDEX_MATCH_NAMES && ranks == nullptr))
    {
        ranks_destroy(ranks);
        parts_destroy(build, categories, notes, prefix);
        return INDEX_FILTER_OUT_OF_MEMORY;
    }
    build->categories = categories;
    build->notes = notes;
    build->prefix = prefix;
    build->ranks = ranks;
    build->scope = scope;
    for (size_t bucket = 0; bucket < rank_buckets; ++bucket)
        build->heads[bucket] = build->tails[bucket] = SIZE_MAX;
    build->entries = count;
    *out = build;
    return INDEX_FILTER_ACCEPTED;
}

static bool well_formed(const char *_Nonnull bytes, size_t length)
{
    size_t index = 0;
    while (index < length)
    {
        uint32_t point = 0;
        size_t step = utf8_text_decode(bytes + index, length - index, &point);
        if (step == 0)
            return false;
        index += step;
    }
    return true;
}

static void prepare(struct index_filter_build *_Nonnull build, const char *_Nonnull term,
                    size_t length)
{
    for (size_t byte = 0; byte < 256; ++byte)
        build->steps[byte] = length;
    for (size_t index = 0; index < length - 1; ++index)
        build->steps[(unsigned char)ascii_fold_byte(term[index])] = length - 1 - index;
    build->steps[(unsigned char)ascii_fold_byte(term[length - 1])] = 0;
    for (size_t byte = 0; byte < 256; ++byte)
        build->steps[byte] = build->steps[(unsigned char)ascii_fold_byte((char)byte)];
    build->first = build->second = term[0];
    if (length == 1)
        for (size_t byte = 0; byte < 256; ++byte)
            if (build->steps[byte] == 0 && (char)byte != build->first)
                build->second = (char)byte;
    build->term_length = length;
    build->term = term;
}

static void seal(struct index_filter_build *_Nonnull build)
{
    prefix_destroy(build->prefix);
    build->prefix = nullptr;
    build->term = nullptr;
    build->tags = nullptr;
    build->phase = BUILD_SEALED;
}

static void prepare_prefix(struct index_filter_build *_Nonnull build, const char *_Nonnull term)
{
    size_t *_Nonnull prefix = build->prefix;
    prefix[0] = 0;
    for (size_t index = 1; index < build->term_length; ++index)
    {
        size_t matched = prefix[index - 1];
        while (matched > 0 && ascii_fold_byte(term[index]) != ascii_fold_byte(term[matched]))
            matched = prefix[matched - 1];
        if (ascii_fold_byte(term[index]) == ascii_fold_byte(term[matched]))
            matched += 1;
        prefix[index] = matched;
    }
}

static bool tag_conditions(const struct index_filter_query *_Nonnull query)
{
    return query->tags != nullptr && note_tags_count(query->tags) != 0;
}

static void prepare_query(struct index_filter_build *_Nonnull build,
                          const struct index_filter_query *_Nonnull query, size_t prefix_length)
{
    if (query->length != 0)
        prepare(build, query->term, query->length);
    build->tags = tag_conditions(query) ? query->tags : nullptr;
    build->tag_mode = query->mode;
    build->favorites_only = query->scope == INDEX_MATCH_NAMES && query->favorites_only;
    build->favorites_first = query->scope == INDEX_MATCH_NAMES && query->favorites_first;
    if (prefix_length != 0)
        prepare_prefix(build, query->term);
}

enum index_filter_outcome
index_filter_build_create(const struct index_filter_query *_Nonnull query, size_t count,
                          struct index_filter_build *_Nullable *_Nonnull out)
{
    size_t length = query->length;
    if (length == 0 && !tag_conditions(query) && query->scope == INDEX_MATCH_NAME_BODY)
        return INDEX_FILTER_NO_TERM;
    if (!well_formed(query->term, length))
        return INDEX_FILTER_MALFORMED;
    struct index_filter_build *_Nullable build = nullptr;
    size_t prefix_length = count > 0 && length >= 2 ? length : 0;
    enum index_filter_outcome allocated =
        allocate_build(count, prefix_length, query->scope, &build);
    if (allocated != INDEX_FILTER_ACCEPTED)
        return allocated;
    prepare_query(build, query, prefix_length);
    if (count == 0)
        seal(build);
    *out = build;
    return INDEX_FILTER_ACCEPTED;
}

static size_t rank_of(enum index_match_kind kind)
{
    switch (kind)
    {
    case INDEX_MATCH_ALL:
        return 0;
    case INDEX_MATCH_NAME_EXACT:
        return 1;
    case INDEX_MATCH_NAME_PREFIX:
        return 2;
    case INDEX_MATCH_NAME_CONTAINS:
        return 3;
    case INDEX_MATCH_NAME_SUBSEQUENCE:
        return 4;
    case INDEX_MATCH_CATEGORY_EXACT:
        return 5;
    case INDEX_MATCH_CATEGORY_PREFIX:
        return 6;
    case INDEX_MATCH_CATEGORY_CONTAINS:
        return 7;
    case INDEX_MATCH_CATEGORY_SUBSEQUENCE:
        return 8;
    }
}

static void append_rank(struct index_filter_build *_Nonnull build, enum index_match_kind kind,
                        bool favorite)
{
    struct index_filter_rank *_Nonnull ranks = build->ranks;
    size_t bucket = rank_of(kind);
    if (build->favorites_first && !favorite)
        bucket += rank_kinds;
    ranks[build->count] =
        (struct index_filter_rank){.kind = kind, .favorite = favorite, .next = SIZE_MAX};
    if (build->heads[bucket] == SIZE_MAX)
        build->heads[bucket] = build->count;
    else
        ranks[build->tails[bucket]].next = build->count;
    build->tails[bucket] = build->count;
}

static void admit(struct index_filter_build *_Nonnull build, struct note_ref ref,
                  enum index_match_kind kind, bool favorite)
{
    build->categories[build->count] = ref.category;
    build->notes[build->count] = ref.note;
    if (build->scope == INDEX_MATCH_NAMES)
        append_rank(build, kind, favorite);
    build->count += 1;
}

static void begin_projection(struct index_filter_build *_Nonnull build)
{
    if (build->scope == INDEX_MATCH_NAME_BODY || build->count == 0)
    {
        seal(build);
        return;
    }
    build->phase = BUILD_PROJECT;
    build->project_bucket = 0;
    build->project_at = build->heads[0];
}

static void project_one(struct index_filter_build *_Nonnull build)
{
    if (build->project_at == SIZE_MAX)
    {
        build->project_bucket += 1;
        if (build->project_bucket == rank_buckets)
            seal(build);
        else
            build->project_at = build->heads[build->project_bucket];
        return;
    }
    const struct index_filter_rank *_Nonnull ranks = build->ranks;
    build->projected[build->ranked_count] = build->project_at;
    build->ranked_count += 1;
    build->project_at = ranks[build->project_at].next;
    if (build->ranked_count == index_ranked_limit)
        seal(build);
}

static void finish_entry(struct index_filter_build *_Nonnull build,
                         const struct index_filter_entry *_Nonnull entry)
{
    if (build->matched)
        admit(build, entry->ref, build->kind, entry->favorite);
    build->processed += 1;
    build->phase = BUILD_ENTRY;
    if (build->processed == build->entries)
        begin_projection(build);
}

static void next_source(struct index_filter_build *_Nonnull build)
{
    switch (build->source)
    {
    case SOURCE_NAME:
        build->phase = build->scope == INDEX_MATCH_NAMES ? BUILD_CATEGORY : BUILD_BODY;
        break;
    case SOURCE_BODY:
    case SOURCE_CATEGORY:
        build->phase = BUILD_DONE;
        break;
    }
}

static void end_source(struct index_filter_build *_Nonnull build)
{
    if (build->scope == INDEX_MATCH_NAMES && build->source_length >= build->term_length)
    {
        build->at = build->compared = 0;
        build->phase = BUILD_SUBSEQUENCE;
        return;
    }
    next_source(build);
}

static void begin_source(struct index_filter_build *_Nonnull build, size_t length)
{
    build->source_length = length;
    build->at = build->compared = 0;
    build->phase = build->term_length == 1 ? BUILD_SINGLE : BUILD_TAIL;
    if (build->term_length > length)
        end_source(build);
}

static void move_candidate(struct index_filter_build *_Nonnull build, size_t amount)
{
    size_t last = build->source_length - build->term_length;
    if (amount > last - build->at)
    {
        end_source(build);
        return;
    }
    build->at += amount;
    build->phase = BUILD_TAIL;
}

static enum index_match_kind source_kind(const struct index_filter_build *_Nonnull build,
                                         size_t start)
{
    if (build->source == SOURCE_CATEGORY)
    {
        if (build->source_length == build->term_length)
            return INDEX_MATCH_CATEGORY_EXACT;
        return start == 0 ? INDEX_MATCH_CATEGORY_PREFIX : INDEX_MATCH_CATEGORY_CONTAINS;
    }
    if (build->source_length == build->term_length)
        return INDEX_MATCH_NAME_EXACT;
    return start == 0 ? INDEX_MATCH_NAME_PREFIX : INDEX_MATCH_NAME_CONTAINS;
}

static void matched_source(struct index_filter_build *_Nonnull build, size_t start)
{
    build->matched = true;
    build->kind = source_kind(build, start);
    build->phase = BUILD_DONE;
}

static void single_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    char byte = bytes[build->at];
    if (byte == build->first || byte == build->second)
    {
        matched_source(build, build->at);
        return;
    }
    build->at += 1;
    if (build->at == build->source_length)
        end_source(build);
}

static void tail_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    size_t amount = build->steps[(unsigned char)bytes[build->at + build->term_length - 1]];
    if (amount == 0)
    {
        build->phase = BUILD_SCAN;
        return;
    }
    move_candidate(build, amount);
}

static void resume_tail(struct index_filter_build *_Nonnull build)
{
    build->phase = BUILD_TAIL;
    if (build->at > build->source_length - build->term_length)
        end_source(build);
}

static void scan_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    /* BUILD_SCAN keeps r=at, q=compared and requires both live borrowed/owned inputs. */
    const char *_Nonnull term = build->term;
    const size_t *_Nonnull prefix = build->prefix;
    if (ascii_fold_byte(bytes[build->at]) != ascii_fold_byte(term[build->compared]))
    {
        if (build->compared > 0)
            build->compared = prefix[build->compared - 1];
        else
            build->at += 1;
        if (build->compared == 0)
            resume_tail(build);
        return;
    }
    build->at += 1;
    build->compared += 1;
    if (build->compared == build->term_length)
    {
        matched_source(build, build->at - build->term_length);
    }
    else if (build->at == build->source_length)
        end_source(build);
}

static uint32_t folded_point(uint32_t point)
{
    return point <= 0x7f ? (uint32_t)(unsigned char)ascii_fold_byte((char)point) : point;
}

static void subsequence_point(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    const char *_Nonnull term = build->term;
    uint32_t candidate = 0;
    uint32_t wanted = 0;
    size_t source_step =
        utf8_text_decode(bytes + build->at, build->source_length - build->at, &candidate);
    size_t query_step =
        utf8_text_decode(term + build->compared, build->term_length - build->compared, &wanted);
    build->at += source_step;
    if (folded_point(candidate) == folded_point(wanted))
        build->compared += query_step;
    if (build->compared == build->term_length)
    {
        build->matched = true;
        build->kind = build->source == SOURCE_CATEGORY ? INDEX_MATCH_CATEGORY_SUBSEQUENCE
                                                       : INDEX_MATCH_NAME_SUBSEQUENCE;
        build->phase = BUILD_DONE;
    }
    else if (build->at == build->source_length)
        next_source(build);
}

static void compare_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    switch (build->phase)
    {
    case BUILD_SINGLE:
        single_byte(build, bytes);
        break;
    case BUILD_TAIL:
        tail_byte(build, bytes);
        break;
    case BUILD_SCAN:
        scan_byte(build, bytes);
        break;
    case BUILD_SUBSEQUENCE:
        subsequence_point(build, bytes);
        break;
    case BUILD_ENTRY:
    case BUILD_TAGS:
    case BUILD_NAME_LENGTH:
    case BUILD_BODY:
    case BUILD_CATEGORY:
    case BUILD_DONE:
    case BUILD_PROJECT:
    case BUILD_SEALED:
        break;
    }
}

static void begin_text(struct index_filter_build *_Nonnull build)
{
    build->matched = build->term_length == 0;
    build->phase = build->matched ? BUILD_DONE : BUILD_NAME_LENGTH;
}

static void tag_decided(struct index_filter_build *_Nonnull build, bool matched)
{
    switch (build->tag_mode)
    {
    case TAG_MATCH_ALL:
        if (!matched)
        {
            build->phase = BUILD_DONE;
            return;
        }
        break;
    case TAG_MATCH_ANY:
        if (matched)
        {
            begin_text(build);
            return;
        }
        break;
    }
    build->tag_query += 1;
    build->tag_entry = build->tag_byte = 0;
    const struct note_tags *_Nonnull selected = build->tags;
    if (build->tag_query != note_tags_count(selected))
        return;
    switch (build->tag_mode)
    {
    case TAG_MATCH_ALL:
        begin_text(build);
        break;
    case TAG_MATCH_ANY:
        build->phase = BUILD_DONE;
        break;
    }
}

static void tag_byte(struct index_filter_build *_Nonnull build,
                     const struct note_tags *_Nonnull available)
{
    if (build->tag_entry == note_tags_count(available))
    {
        tag_decided(build, false);
        return;
    }
    const struct note_tags *_Nonnull selected = build->tags;
    const char *_Nonnull wanted = note_tag_bytes(note_tags_at(selected, build->tag_query));
    const char *_Nonnull candidate = note_tag_bytes(note_tags_at(available, build->tag_entry));
    char left = wanted[build->tag_byte];
    char right = candidate[build->tag_byte];
    if (left != right)
    {
        build->tag_entry += 1;
        build->tag_byte = 0;
        return;
    }
    if (left == '\0')
        tag_decided(build, true);
    else
        build->tag_byte += 1;
}

static void begin_entry(struct index_filter_build *_Nonnull build,
                        const struct index_filter_entry *_Nonnull entry)
{
    build->matched = false;
    build->kind = INDEX_MATCH_ALL;
    build->source = SOURCE_NAME;
    build->name_length = 0;
    if ((build->scope == INDEX_MATCH_NAME_BODY && entry->body == nullptr) ||
        (build->tags != nullptr && entry->tags == nullptr) ||
        (build->favorites_only && !entry->favorite))
    {
        build->phase = BUILD_DONE;
        return;
    }
    if (build->tags == nullptr)
    {
        begin_text(build);
        return;
    }
    build->tag_query = build->tag_entry = build->tag_byte = 0;
    build->phase = BUILD_TAGS;
}

static const char *_Nonnull source_bytes(const struct index_filter_build *_Nonnull build,
                                         const struct index_filter_entry *_Nonnull entry)
{
    switch (build->source)
    {
    case SOURCE_NAME:
        return entry->name;
    case SOURCE_BODY:
        return entry->body;
    case SOURCE_CATEGORY:
        return entry->category_name;
    }
}

static void tick(struct index_filter_build *_Nonnull build,
                 const struct index_filter_entry *_Nonnull entry)
{
    if (build->phase == BUILD_PROJECT)
    {
        project_one(build);
        return;
    }
    if (build->phase == BUILD_ENTRY)
    {
        begin_entry(build, entry);
        return;
    }
    if (build->phase == BUILD_TAGS)
    {
        tag_byte(build, entry->tags);
        return;
    }
    if (build->phase == BUILD_NAME_LENGTH)
    {
        const char *_Nonnull name = source_bytes(build, entry);
        if (name[build->name_length] == '\0')
            begin_source(build, build->name_length);
        else
            build->name_length += 1;
        return;
    }
    compare_byte(build, source_bytes(build, entry));
}

static bool settle(struct index_filter_build *_Nonnull build,
                   const struct index_filter_entry *_Nonnull entry)
{
    if (build->phase == BUILD_BODY)
    {
        build->source = SOURCE_BODY;
        begin_source(build, entry->length);
    }
    if (build->phase == BUILD_CATEGORY)
    {
        build->source = SOURCE_CATEGORY;
        build->name_length = 0;
        build->phase = entry->category_name == nullptr ? BUILD_DONE : BUILD_NAME_LENGTH;
    }
    if (build->phase == BUILD_SEALED)
        return true;
    if (build->phase != BUILD_DONE)
        return false;
    finish_entry(build, entry);
    return build->phase != BUILD_PROJECT;
}

enum index_filter_step index_filter_build_step(struct index_filter_build *_Nonnull build,
                                               const struct index_filter_entry *_Nonnull entry,
                                               size_t *_Nonnull work)
{
    if (build->phase == BUILD_SEALED)
        return INDEX_FILTER_STEP_ALL_DONE;
    if (*work == 0)
        return INDEX_FILTER_STEP_MORE;
    for (;;)
    {
        *work -= 1;
        tick(build, entry);
        if (settle(build, entry))
            return build->phase == BUILD_SEALED ? INDEX_FILTER_STEP_ALL_DONE
                                                : INDEX_FILTER_STEP_ENTRY_DONE;
        if (*work == 0)
            return INDEX_FILTER_STEP_MORE;
    }
}

enum index_filter_readiness
index_filter_build_readiness(const struct index_filter_build *_Nonnull build)
{
    return build->phase == BUILD_SEALED ? INDEX_FILTER_READY : INDEX_FILTER_NOT_READY;
}

size_t index_filter_build_count(const struct index_filter_build *_Nonnull sealed)
{
    return sealed->count;
}

struct note_ref index_filter_build_ref(const struct index_filter_build *_Nonnull sealed,
                                       size_t index)
{
    return (struct note_ref){.category = sealed->categories[index], .note = sealed->notes[index]};
}

size_t index_filter_build_ranked_count(const struct index_filter_build *_Nonnull sealed)
{
    return sealed->ranked_count;
}

struct index_ranked_note index_filter_build_ranked(const struct index_filter_build *_Nonnull sealed,
                                                   size_t index)
{
    size_t at = sealed->projected[index];
    const struct index_filter_rank *_Nonnull ranks = sealed->ranks;
    return (struct index_ranked_note){.ref = index_filter_build_ref(sealed, at),
                                      .kind = ranks[at].kind,
                                      .favorite = ranks[at].favorite};
}

static void copy_match(struct index_filter_build *_Nonnull removed,
                       const struct index_filter_build *_Nonnull sealed, size_t index,
                       struct note_ref ref)
{
    enum index_match_kind kind = INDEX_MATCH_ALL;
    bool favorite = false;
    if (sealed->scope == INDEX_MATCH_NAMES)
    {
        const struct index_filter_rank *_Nonnull ranks = sealed->ranks;
        kind = ranks[index].kind;
        favorite = ranks[index].favorite;
    }
    admit(removed, ref, kind, favorite);
}

enum index_filter_outcome
index_filter_build_removed(const struct index_filter_build *_Nonnull sealed, struct note_ref target,
                           struct index_filter_build *_Nullable *_Nonnull out)
{
    struct index_filter_build *_Nullable removed = nullptr;
    enum index_filter_outcome allocated = allocate_build(sealed->count, 0, sealed->scope, &removed);
    if (allocated != INDEX_FILTER_ACCEPTED)
        return allocated;
    removed->favorites_first = sealed->favorites_first;
    removed->favorites_only = sealed->favorites_only;
    for (size_t index = 0; index < sealed->count; ++index)
    {
        struct note_ref ref = index_filter_build_ref(sealed, index);
        if (ref.category == target.category && ref.note == target.note)
            continue;
        if (ref.category == target.category && ref.note > target.note)
            ref.note -= 1;
        copy_match(removed, sealed, index, ref);
    }
    begin_projection(removed);
    while (removed->phase == BUILD_PROJECT)
        project_one(removed);
    *out = removed;
    return INDEX_FILTER_ACCEPTED;
}

void index_filter_build_destroy(struct index_filter_build *_Nullable build)
{
    if (build == nullptr)
        return;
    ranks_destroy(build->ranks);
    parts_destroy(build, build->categories, build->notes, build->prefix);
}
