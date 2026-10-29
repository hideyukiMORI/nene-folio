#include "index_filter_build.h"
#include "ascii_fold.h"
#include "utf8_text.h"
#include <stdint.h>
#include <stdlib.h>

struct index_filter_build
{
    enum
    {
        BUILD_ENTRY,
        BUILD_NAME_LENGTH,
        BUILD_SINGLE,
        BUILD_TAIL,
        BUILD_HEAD,
        BUILD_FULL,
        BUILD_BODY,
        BUILD_DONE,
        BUILD_SEALED
    } phase;
    size_t *_Nonnull categories;
    size_t *_Nonnull notes;
    const char *_Nullable term;
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
        SOURCE_BODY
    } source;
    bool matched;
};

static enum index_filter_outcome allocate_build(size_t count,
                                                struct index_filter_build *_Nullable *_Nonnull out)
{
    if (count >= SIZE_MAX / sizeof(size_t))
        return INDEX_FILTER_OUT_OF_MEMORY;
    struct index_filter_build *_Nullable build = calloc(1, sizeof *build);
    if (build == nullptr)
        return INDEX_FILTER_OUT_OF_MEMORY;
    size_t *_Nullable categories = malloc((count + 1) * sizeof *categories);
    size_t *_Nullable notes = malloc((count + 1) * sizeof *notes);
    if (categories == nullptr || notes == nullptr)
    {
        free(categories);
        free(notes);
        free(build);
        return INDEX_FILTER_OUT_OF_MEMORY;
    }
    build->categories = categories;
    build->notes = notes;
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
    build->term = nullptr;
    build->phase = BUILD_SEALED;
}

enum index_filter_outcome
index_filter_build_create(const char *_Nonnull term, size_t length, size_t count,
                          struct index_filter_build *_Nullable *_Nonnull out)
{
    if (length == 0)
        return INDEX_FILTER_NO_TERM;
    if (!well_formed(term, length))
        return INDEX_FILTER_MALFORMED;
    struct index_filter_build *_Nullable build = nullptr;
    enum index_filter_outcome allocated = allocate_build(count, &build);
    if (allocated != INDEX_FILTER_ACCEPTED)
        return allocated;
    prepare(build, term, length);
    if (count == 0)
        seal(build);
    *out = build;
    return INDEX_FILTER_ACCEPTED;
}

static void admit(struct index_filter_build *_Nonnull build, struct note_ref ref)
{
    build->categories[build->count] = ref.category;
    build->notes[build->count] = ref.note;
    build->count += 1;
}

static void finish_entry(struct index_filter_build *_Nonnull build, struct note_ref ref)
{
    if (build->matched)
        admit(build, ref);
    build->processed += 1;
    build->phase = BUILD_ENTRY;
    if (build->processed == build->entries)
        seal(build);
}

static void end_source(struct index_filter_build *_Nonnull build)
{
    switch (build->source)
    {
    case SOURCE_NAME:
        build->phase = BUILD_BODY;
        break;
    case SOURCE_BODY:
        build->phase = BUILD_DONE;
        break;
    }
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

static void single_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    char byte = bytes[build->at];
    if (byte == build->first || byte == build->second)
    {
        build->matched = true;
        build->phase = BUILD_DONE;
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
        build->phase = BUILD_HEAD;
        return;
    }
    move_candidate(build, amount);
}

static void head_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    if (ascii_fold_byte(bytes[build->at]) != ascii_fold_byte(build->first))
    {
        move_candidate(build, 1);
        return;
    }
    build->compared = 0;
    build->phase = BUILD_FULL;
}

static void full_byte(struct index_filter_build *_Nonnull build, const char *_Nonnull bytes)
{
    /* BUILD_FULL requires the live term borrow; sanitizers enforce this binding. */
    const char *_Nonnull term = build->term;
    if (ascii_fold_byte(bytes[build->at + build->compared]) !=
        ascii_fold_byte(term[build->compared]))
    {
        move_candidate(build, 1);
        return;
    }
    build->compared += 1;
    if (build->compared == build->term_length)
    {
        build->matched = true;
        build->phase = BUILD_DONE;
    }
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
    case BUILD_HEAD:
        head_byte(build, bytes);
        break;
    case BUILD_FULL:
        full_byte(build, bytes);
        break;
    case BUILD_ENTRY:
    case BUILD_NAME_LENGTH:
    case BUILD_BODY:
    case BUILD_DONE:
    case BUILD_SEALED:
        break;
    }
}

static void tick(struct index_filter_build *_Nonnull build,
                 const struct index_filter_entry *_Nonnull entry)
{
    if (build->phase == BUILD_ENTRY)
    {
        build->matched = false;
        build->source = SOURCE_NAME;
        build->name_length = 0;
        build->phase = entry->body == nullptr ? BUILD_DONE : BUILD_NAME_LENGTH;
        return;
    }
    if (build->phase == BUILD_NAME_LENGTH)
    {
        if (entry->name[build->name_length] == '\0')
            begin_source(build, build->name_length);
        else
            build->name_length += 1;
        return;
    }
    const char *_Nonnull bytes = entry->name;
    switch (build->source)
    {
    case SOURCE_NAME:
        bytes = entry->name;
        break;
    case SOURCE_BODY:
        bytes = entry->body;
        break;
    }
    compare_byte(build, bytes);
}

static bool settle(struct index_filter_build *_Nonnull build,
                   const struct index_filter_entry *_Nonnull entry)
{
    if (build->phase == BUILD_BODY)
    {
        build->source = SOURCE_BODY;
        begin_source(build, entry->length);
    }
    if (build->phase != BUILD_DONE)
        return false;
    finish_entry(build, entry->ref);
    return true;
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

enum index_filter_outcome
index_filter_build_removed(const struct index_filter_build *_Nonnull sealed, struct note_ref target,
                           struct index_filter_build *_Nullable *_Nonnull out)
{
    struct index_filter_build *_Nullable removed = nullptr;
    enum index_filter_outcome allocated = allocate_build(sealed->count, &removed);
    if (allocated != INDEX_FILTER_ACCEPTED)
        return allocated;
    for (size_t index = 0; index < sealed->count; ++index)
    {
        struct note_ref ref = index_filter_build_ref(sealed, index);
        if (ref.category == target.category && ref.note == target.note)
            continue;
        if (ref.category == target.category && ref.note > target.note)
            ref.note -= 1;
        admit(removed, ref);
    }
    seal(removed);
    *out = removed;
    return INDEX_FILTER_ACCEPTED;
}

void index_filter_build_destroy(struct index_filter_build *_Nullable build)
{
    if (build == nullptr)
        return;
    free(build->categories);
    free(build->notes);
    free(build);
}
