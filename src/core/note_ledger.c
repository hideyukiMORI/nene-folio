#include "note_ledger.h"

#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"

#include <stdlib.h>
#include <string.h>

struct note_ledger
{
    struct name_list *_Nonnull names;
    bool *_Nonnull favorites;
    size_t capacity;
    size_t *_Nullable priority;
    size_t favorite_count;
};

constexpr uint32_t ledger_version = 2;

static void destroy_construction(struct name_list *_Nullable names, bool *_Nullable favorites)
{
    name_list_destroy(names);
    free(favorites);
}

/* 全必須資源の確保成功後にだけnonnull memberへ束縛する。 */
static enum note_ledger_outcome create(struct note_ledger *_Nullable *_Nonnull out)
{
    struct name_list *_Nullable names = nullptr;
    if (name_list_create(&names) != NAME_LIST_ACCEPTED)
        return NOTE_LEDGER_OUT_OF_MEMORY;
    bool *_Nullable favorites = calloc(4, sizeof *favorites);
    if (favorites == nullptr)
    {
        destroy_construction(names, favorites);
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    struct note_ledger *_Nullable ledger = calloc(1, sizeof *ledger);
    if (ledger == nullptr)
    {
        destroy_construction(names, favorites);
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    ledger->names = names;
    ledger->favorites = favorites;
    ledger->capacity = 4;
    *out = ledger;
    return NOTE_LEDGER_ACCEPTED;
}

/* 名前と星から安定な表示順を一度作り、完成した台帳だけを公開する。 */
static enum note_ledger_outcome complete(struct note_ledger *_Nullable ledger,
                                         enum note_ledger_outcome outcome,
                                         struct note_ledger *_Nullable *_Nonnull out)
{
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        note_ledger_destroy(ledger);
        return outcome;
    }
    size_t count = name_list_count(ledger->names);
    size_t *_Nullable priority = malloc((2 * count + 1) * sizeof *priority);
    if (priority == nullptr)
    {
        note_ledger_destroy(ledger);
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    size_t next = 0;
    for (size_t index = 0; index < count; ++index)
        if (ledger->favorites[index])
            priority[next++] = index;
    ledger->favorite_count = next;
    for (size_t index = 0; index < count; ++index)
        if (!ledger->favorites[index])
            priority[next++] = index;
    for (size_t position = 0; position < count; ++position)
        priority[count + priority[position]] = position;
    ledger->priority = priority;
    *out = ledger;
    return NOTE_LEDGER_ACCEPTED;
}

enum note_ledger_outcome note_ledger_empty(struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable ledger = nullptr;
    enum note_ledger_outcome outcome = create(&ledger);
    return complete(ledger, outcome, out);
}

static enum note_ledger_outcome translate(enum name_list_outcome outcome)
{
    switch (outcome)
    {
    case NAME_LIST_ACCEPTED:
        return NOTE_LEDGER_ACCEPTED;
    case NAME_LIST_DUPLICATE:
    case NAME_LIST_INVALID_NAME:
        return NOTE_LEDGER_MALFORMED;
    case NAME_LIST_OUT_OF_MEMORY:
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    return NOTE_LEDGER_MALFORMED;
}

static bool expect_key(struct json_reader *_Nonnull reader, const char *_Nonnull key)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY &&
           json_reader_text_length(reader) == strlen(key) &&
           strcmp(json_reader_text(reader), key) == 0;
}

/* 期待しない字句を結果に写す。記憶不足だけは不正と区別する。 */
static enum note_ledger_outcome unexpected(enum json_token token)
{
    return token == JSON_TOKEN_OUT_OF_MEMORY ? NOTE_LEDGER_OUT_OF_MEMORY : NOTE_LEDGER_MALFORMED;
}

static enum note_ledger_outcome reserve(struct note_ledger *_Nonnull ledger, size_t count)
{
    if (count <= ledger->capacity)
        return NOTE_LEDGER_ACCEPTED;
    if (count > SIZE_MAX / (4 * sizeof(size_t)) - 1)
        return NOTE_LEDGER_OUT_OF_MEMORY;
    size_t capacity = count * 2;
    bool *_Nullable grown = realloc(ledger->favorites, capacity * sizeof *grown);
    if (grown == nullptr)
        return NOTE_LEDGER_OUT_OF_MEMORY;
    memset(grown + ledger->capacity, 0, (capacity - ledger->capacity) * sizeof *grown);
    ledger->favorites = grown;
    ledger->capacity = capacity;
    return NOTE_LEDGER_ACCEPTED;
}

static enum note_ledger_outcome append(struct note_ledger *_Nonnull ledger, struct note_entry entry)
{
    size_t count = name_list_count(ledger->names);
    enum note_ledger_outcome outcome = reserve(ledger, count + 1);
    if (outcome != NOTE_LEDGER_ACCEPTED)
        return outcome;
    outcome = translate(name_list_append(ledger->names, entry.name, strlen(entry.name)));
    if (outcome == NOTE_LEDGER_ACCEPTED)
        ledger->favorites[count] = entry.favorite;
    return outcome;
}

/* notesとfavoritesの名前集合は、長さを含む同じ境界で検証する。 */
static enum note_ledger_outcome read_names(struct json_reader *_Nonnull reader,
                                           const char *_Nonnull key,
                                           struct name_list *_Nonnull names)
{
    if (!expect_key(reader, key) || json_reader_next(reader) != JSON_TOKEN_ARRAY_BEGIN)
        return NOTE_LEDGER_MALFORMED;
    for (;;)
    {
        enum json_token token = json_reader_next(reader);
        if (token == JSON_TOKEN_ARRAY_END)
            return NOTE_LEDGER_ACCEPTED;
        enum note_ledger_outcome outcome =
            token == JSON_TOKEN_STRING
                ? translate(name_list_append(names, json_reader_text(reader),
                                             json_reader_text_length(reader)))
                : unexpected(token);
        if (outcome != NOTE_LEDGER_ACCEPTED)
            return outcome;
    }
}

static enum note_ledger_outcome parse_names(struct json_reader *_Nonnull reader,
                                            struct note_ledger *_Nonnull ledger)
{
    enum note_ledger_outcome outcome = read_names(reader, "notes", ledger->names);
    return outcome == NOTE_LEDGER_ACCEPTED ? reserve(ledger, name_list_count(ledger->names))
                                           : outcome;
}

static enum note_ledger_outcome apply_favorites(struct note_ledger *_Nonnull ledger,
                                                const struct name_list *_Nonnull favorites)
{
    for (size_t index = 0; index < name_list_count(favorites); ++index)
        if (!name_list_contains(ledger->names, name_list_at(favorites, index)))
            return NOTE_LEDGER_MALFORMED;
    for (size_t index = 0; index < name_list_count(ledger->names); ++index)
        ledger->favorites[index] =
            name_list_contains(favorites, name_list_at(ledger->names, index));
    return NOTE_LEDGER_ACCEPTED;
}

static enum note_ledger_outcome parse_favorites(struct json_reader *_Nonnull reader,
                                                struct note_ledger *_Nonnull ledger)
{
    struct name_list *_Nullable favorites = nullptr;
    if (name_list_create(&favorites) != NAME_LIST_ACCEPTED)
        return NOTE_LEDGER_OUT_OF_MEMORY;
    enum note_ledger_outcome outcome = read_names(reader, "favorites", favorites);
    if (outcome == NOTE_LEDGER_ACCEPTED)
        outcome = apply_favorites(ledger, favorites);
    name_list_destroy(favorites);
    return outcome;
}

static enum note_ledger_outcome parse_document(struct json_reader *_Nonnull reader,
                                               struct note_ledger *_Nonnull ledger)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !expect_key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
    {
        return NOTE_LEDGER_MALFORMED;
    }
    uint32_t version = json_reader_unsigned(reader);
    if (version != 1 && version != ledger_version)
    {
        return NOTE_LEDGER_UNSUPPORTED_VERSION;
    }
    enum note_ledger_outcome outcome = parse_names(reader, ledger);
    if (outcome == NOTE_LEDGER_ACCEPTED && version == ledger_version)
        outcome = parse_favorites(reader, ledger);
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END)
    {
        return NOTE_LEDGER_MALFORMED;
    }
    return NOTE_LEDGER_ACCEPTED;
}

enum note_ledger_outcome note_ledger_read(struct json_reader *_Nonnull reader,
                                          struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable ledger = nullptr;
    enum note_ledger_outcome outcome = create(&ledger);
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    outcome = parse_document(reader, ledger);
    return complete(ledger, outcome, out);
}

enum note_ledger_outcome note_ledger_parse(const char *_Nonnull text, size_t length,
                                           struct note_ledger *_Nullable *_Nonnull out)
{
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
    {
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    struct note_ledger *_Nullable ledger = nullptr;
    enum note_ledger_outcome outcome = note_ledger_read(reader, &ledger);
    if (outcome == NOTE_LEDGER_ACCEPTED && json_reader_next(reader) != JSON_TOKEN_END)
    {
        outcome = NOTE_LEDGER_MALFORMED;
    }
    json_reader_destroy(reader);
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        note_ledger_destroy(ledger);
        return outcome;
    }
    *out = ledger;
    return NOTE_LEDGER_ACCEPTED;
}

void note_ledger_write(const struct note_ledger *_Nonnull ledger,
                       struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, ledger_version);
    json_writer_key(writer, "notes");
    json_writer_array_begin(writer);
    size_t count = name_list_count(ledger->names);
    for (size_t index = 0; index < count; ++index)
    {
        json_writer_string(writer, name_list_at(ledger->names, index));
    }
    json_writer_array_end(writer);
    json_writer_key(writer, "favorites");
    json_writer_array_begin(writer);
    for (size_t index = 0; index < count; ++index)
        if (ledger->favorites[index])
            json_writer_string(writer, name_list_at(ledger->names, index));
    json_writer_array_end(writer);
    json_writer_object_end(writer);
}

enum note_ledger_outcome note_ledger_reconcile(const struct note_ledger *_Nonnull ledger,
                                               const struct name_list *_Nonnull scanned,
                                               struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable target = nullptr;
    enum note_ledger_outcome outcome = create(&target);
    for (size_t index = 0; outcome == NOTE_LEDGER_ACCEPTED && index < note_ledger_count(ledger);
         ++index)
    {
        struct note_entry entry = note_ledger_entry(ledger, index);
        if (name_list_contains(scanned, entry.name))
            outcome = append(target, entry);
    }
    for (size_t index = 0; outcome == NOTE_LEDGER_ACCEPTED && index < name_list_count(scanned);
         ++index)
    {
        struct note_entry entry = {.name = name_list_at(scanned, index), .favorite = false};
        if (!name_list_contains(ledger->names, entry.name))
            outcome = append(target, entry);
    }
    return complete(target, outcome, out);
}

/* from を to へ移したあと、index 番目に来るのは元の何番目か。 */
static size_t source_index(size_t from, size_t to, size_t index)
{
    if (index == to)
    {
        return from;
    }
    if (from < to)
    {
        return index >= from && index < to ? index + 1 : index;
    }
    return index > to && index <= from ? index - 1 : index;
}

enum note_ledger_outcome note_ledger_moved(const struct note_ledger *_Nonnull ledger, size_t from,
                                           size_t to, struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable target = nullptr;
    enum note_ledger_outcome outcome = create(&target);
    size_t count = name_list_count(ledger->names);
    for (size_t index = 0; outcome == NOTE_LEDGER_ACCEPTED && index < count; ++index)
    {
        outcome = append(target, note_ledger_entry(ledger, source_index(from, to, index)));
    }
    return complete(target, outcome, out);
}

enum note_ledger_outcome note_ledger_inserted(const struct note_ledger *_Nonnull ledger,
                                              size_t index, struct note_entry entry,
                                              struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable target = nullptr;
    enum note_ledger_outcome outcome = create(&target);
    size_t count = name_list_count(ledger->names) + 1;
    for (size_t position = 0; outcome == NOTE_LEDGER_ACCEPTED && position < count; ++position)
    {
        struct note_entry current =
            position == index
                ? entry
                : note_ledger_entry(ledger, position < index ? position : position - 1);
        outcome = append(target, current);
    }
    return complete(target, outcome, out);
}

enum note_ledger_outcome note_ledger_removed(const struct note_ledger *_Nonnull ledger,
                                             size_t index,
                                             struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable target = nullptr;
    enum note_ledger_outcome outcome = create(&target);
    size_t count = name_list_count(ledger->names);
    for (size_t position = 0; outcome == NOTE_LEDGER_ACCEPTED && position < count; ++position)
    {
        if (position == index)
        {
            continue;
        }
        outcome = append(target, note_ledger_entry(ledger, position));
    }
    return complete(target, outcome, out);
}

enum note_ledger_outcome note_ledger_renamed(const struct note_ledger *_Nonnull ledger,
                                             size_t index, const char *_Nonnull name,
                                             struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable removed = nullptr;
    enum note_ledger_outcome outcome = note_ledger_removed(ledger, index, &removed);
    if (outcome == NOTE_LEDGER_ACCEPTED)
    {
        struct note_entry entry = {.name = name, .favorite = ledger->favorites[index]};
        outcome = note_ledger_inserted(removed, index, entry, out);
    }
    note_ledger_destroy(removed);
    return outcome;
}

enum note_ledger_outcome note_ledger_toggled(const struct note_ledger *_Nonnull ledger,
                                             size_t index,
                                             struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable target = nullptr;
    enum note_ledger_outcome outcome = create(&target);
    for (size_t position = 0;
         outcome == NOTE_LEDGER_ACCEPTED && position < note_ledger_count(ledger); ++position)
    {
        struct note_entry entry = note_ledger_entry(ledger, position);
        if (position == index)
            entry.favorite = !entry.favorite;
        outcome = append(target, entry);
    }
    return complete(target, outcome, out);
}

struct note_entry note_ledger_entry(const struct note_ledger *_Nonnull ledger, size_t index)
{
    struct note_entry entry = {.name = name_list_at(ledger->names, index),
                               .favorite = ledger->favorites[index]};
    return entry;
}

bool note_ledger_favorite(const struct note_ledger *_Nonnull ledger, size_t index)
{
    return ledger->favorites[index];
}

size_t note_ledger_favorite_count(const struct note_ledger *_Nonnull ledger)
{
    return ledger->favorite_count;
}

size_t note_ledger_ordered(const struct note_ledger *_Nonnull ledger, enum note_order order,
                           size_t position)
{
    switch (order)
    {
    case NOTE_ORDER_MANUAL:
        return position;
    case NOTE_ORDER_FAVORITES_FIRST:
        return ledger->priority[position];
    }
    return position;
}

size_t note_ledger_position(const struct note_ledger *_Nonnull ledger, enum note_order order,
                            size_t note)
{
    switch (order)
    {
    case NOTE_ORDER_MANUAL:
        return note;
    case NOTE_ORDER_FAVORITES_FIRST:
        return ledger->priority[note_ledger_count(ledger) + note];
    }
    return note;
}

size_t note_ledger_count(const struct note_ledger *_Nonnull ledger)
{
    return name_list_count(ledger->names);
}

const char *_Nonnull note_ledger_name(const struct note_ledger *_Nonnull ledger, size_t index)
{
    return name_list_at(ledger->names, index);
}

void note_ledger_destroy(struct note_ledger *_Nullable ledger)
{
    if (ledger == nullptr)
    {
        return;
    }
    name_list_destroy(ledger->names);
    free(ledger->favorites);
    free(ledger->priority);
    free(ledger);
}
