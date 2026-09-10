#include "category_ledger.h"

#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"

#include <stdlib.h>
#include <string.h>

struct category_ledger
{
    struct name_list *_Nonnull names;
    struct rgb_color *_Nullable colors; /* names と同じ数 */
    bool *_Nullable expanded;           /* names と同じ数 */
    size_t capacity;
};

constexpr uint32_t ledger_version = 1;
/* 走査でだけ見つかったカテゴリに与える色。利用者が右クリックで変えるまでの仮の値（FR-010）。 */
constexpr struct rgb_color default_color = {0x7F, 0x8F, 0xA6};

enum category_ledger_outcome category_ledger_empty(struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable ledger = calloc(1, sizeof *ledger);
    if (ledger == nullptr)
    {
        return CATEGORY_LEDGER_OUT_OF_MEMORY;
    }
    if (name_list_create(&ledger->names) != NAME_LIST_ACCEPTED)
    {
        free(ledger);
        return CATEGORY_LEDGER_OUT_OF_MEMORY;
    }
    *out = ledger;
    return CATEGORY_LEDGER_ACCEPTED;
}

static bool reserve(struct category_ledger *_Nonnull ledger)
{
    size_t count = name_list_count(ledger->names);
    if (count < ledger->capacity)
    {
        return true;
    }
    size_t capacity = ledger->capacity == 0 ? 4 : ledger->capacity * 2;
    struct rgb_color *_Nullable colors = realloc(ledger->colors, capacity * sizeof *colors);
    if (colors == nullptr)
    {
        return false;
    }
    ledger->colors = colors;
    bool *_Nullable expanded = realloc(ledger->expanded, capacity * sizeof *expanded);
    if (expanded == nullptr)
    {
        return false;
    }
    ledger->expanded = expanded;
    ledger->capacity = capacity;
    return true;
}

static enum category_ledger_outcome translate(enum name_list_outcome outcome)
{
    switch (outcome)
    {
    case NAME_LIST_ACCEPTED:
        return CATEGORY_LEDGER_ACCEPTED;
    case NAME_LIST_DUPLICATE:
    case NAME_LIST_INVALID_NAME:
        return CATEGORY_LEDGER_MALFORMED;
    case NAME_LIST_OUT_OF_MEMORY:
        return CATEGORY_LEDGER_OUT_OF_MEMORY;
    }
    return CATEGORY_LEDGER_MALFORMED;
}

/* 名前を足し、色は既定・展開は true で始める。 */
static enum category_ledger_outcome append(struct category_ledger *_Nonnull ledger,
                                           const char *_Nonnull name, size_t length)
{
    if (!reserve(ledger))
    {
        return CATEGORY_LEDGER_OUT_OF_MEMORY;
    }
    enum category_ledger_outcome outcome = translate(name_list_append(ledger->names, name, length));
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    size_t index = name_list_count(ledger->names) - 1;
    ledger->colors[index] = default_color;
    ledger->expanded[index] = true;
    return CATEGORY_LEDGER_ACCEPTED;
}

/* 期待しない字句を結果に写す。記憶不足だけは不正と区別する。 */
static enum category_ledger_outcome unexpected(enum json_token token)
{
    return token == JSON_TOKEN_OUT_OF_MEMORY ? CATEGORY_LEDGER_OUT_OF_MEMORY
                                             : CATEGORY_LEDGER_MALFORMED;
}

static bool expect_key(struct json_reader *_Nonnull reader, const char *_Nonnull key)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY && strcmp(json_reader_text(reader), key) == 0;
}

static bool expect_version(struct json_reader *_Nonnull reader, uint32_t *_Nonnull version)
{
    if (!expect_key(reader, "version") || json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
    {
        return false;
    }
    *version = json_reader_unsigned(reader);
    return true;
}

/* {"name": …, "color": …, "expanded": …} を 1 つ読んで足す。OBJECT_BEGIN は読まれた後。 */
static enum category_ledger_outcome parse_entry(struct json_reader *_Nonnull reader,
                                                struct category_ledger *_Nonnull ledger)
{
    if (!expect_key(reader, "name") || json_reader_next(reader) != JSON_TOKEN_STRING)
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    enum category_ledger_outcome outcome =
        append(ledger, json_reader_text(reader), json_reader_text_length(reader));
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    size_t index = name_list_count(ledger->names) - 1;
    if (!expect_key(reader, "color") || json_reader_next(reader) != JSON_TOKEN_STRING ||
        rgb_color_parse_hex(json_reader_text(reader), json_reader_text_length(reader),
                            &ledger->colors[index]) != RGB_COLOR_PARSED)
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    if (!expect_key(reader, "expanded"))
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    enum json_token flag = json_reader_next(reader);
    if (flag != JSON_TOKEN_TRUE && flag != JSON_TOKEN_FALSE)
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    ledger->expanded[index] = flag == JSON_TOKEN_TRUE;
    return json_reader_next(reader) == JSON_TOKEN_OBJECT_END ? CATEGORY_LEDGER_ACCEPTED
                                                             : CATEGORY_LEDGER_MALFORMED;
}

static enum category_ledger_outcome parse_entries(struct json_reader *_Nonnull reader,
                                                  struct category_ledger *_Nonnull ledger)
{
    if (!expect_key(reader, "categories") || json_reader_next(reader) != JSON_TOKEN_ARRAY_BEGIN)
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    for (;;)
    {
        enum json_token token = json_reader_next(reader);
        if (token == JSON_TOKEN_ARRAY_END)
        {
            return CATEGORY_LEDGER_ACCEPTED;
        }
        enum category_ledger_outcome outcome =
            token == JSON_TOKEN_OBJECT_BEGIN ? parse_entry(reader, ledger) : unexpected(token);
        if (outcome != CATEGORY_LEDGER_ACCEPTED)
        {
            return outcome;
        }
    }
}

static enum category_ledger_outcome parse_document(struct json_reader *_Nonnull reader,
                                                   struct category_ledger *_Nonnull ledger)
{
    uint32_t version = 0;
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !expect_version(reader, &version))
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    if (version != ledger_version)
    {
        return CATEGORY_LEDGER_UNSUPPORTED_VERSION;
    }
    enum category_ledger_outcome outcome = parse_entries(reader, ledger);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END ||
        json_reader_next(reader) != JSON_TOKEN_END)
    {
        return CATEGORY_LEDGER_MALFORMED;
    }
    return CATEGORY_LEDGER_ACCEPTED;
}

enum category_ledger_outcome category_ledger_parse(const char *_Nonnull text, size_t length,
                                                   struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable ledger = nullptr;
    enum category_ledger_outcome outcome = category_ledger_empty(&ledger);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
    {
        category_ledger_destroy(ledger);
        return CATEGORY_LEDGER_OUT_OF_MEMORY;
    }
    outcome = parse_document(reader, ledger);
    json_reader_destroy(reader);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        category_ledger_destroy(ledger);
        return outcome;
    }
    *out = ledger;
    return CATEGORY_LEDGER_ACCEPTED;
}

void category_ledger_write(const struct category_ledger *_Nonnull ledger,
                           struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, ledger_version);
    json_writer_key(writer, "categories");
    json_writer_array_begin(writer);
    size_t count = name_list_count(ledger->names);
    for (size_t index = 0; index < count; ++index)
    {
        char hex[rgb_color_hex_length + 1] = {0};
        rgb_color_write_hex(ledger->colors[index], hex);
        json_writer_object_begin(writer);
        json_writer_key(writer, "name");
        json_writer_string(writer, name_list_at(ledger->names, index));
        json_writer_key(writer, "color");
        json_writer_string(writer, hex);
        json_writer_key(writer, "expanded");
        json_writer_boolean(writer, ledger->expanded[index]);
        json_writer_object_end(writer);
    }
    json_writer_array_end(writer);
    json_writer_object_end(writer);
}

/* source の index 番目を色と展開ごと target の末尾へ写す。 */
static enum category_ledger_outcome copy_entry(struct category_ledger *_Nonnull target,
                                               const struct category_ledger *_Nonnull source,
                                               size_t index)
{
    const char *_Nonnull name = name_list_at(source->names, index);
    enum category_ledger_outcome outcome = append(target, name, strlen(name));
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    size_t last = name_list_count(target->names) - 1;
    target->colors[last] = source->colors[index];
    target->expanded[last] = source->expanded[index];
    return CATEGORY_LEDGER_ACCEPTED;
}

static enum category_ledger_outcome merge(struct category_ledger *_Nonnull target,
                                          const struct category_ledger *_Nonnull ledger,
                                          const struct name_list *_Nonnull scanned)
{
    size_t kept = name_list_count(ledger->names);
    for (size_t index = 0; index < kept; ++index)
    {
        if (!name_list_contains(scanned, name_list_at(ledger->names, index)))
        {
            continue;
        }
        enum category_ledger_outcome outcome = copy_entry(target, ledger, index);
        if (outcome != CATEGORY_LEDGER_ACCEPTED)
        {
            return outcome;
        }
    }
    size_t found = name_list_count(scanned);
    for (size_t index = 0; index < found; ++index)
    {
        const char *_Nonnull name = name_list_at(scanned, index);
        if (name_list_contains(ledger->names, name))
        {
            continue;
        }
        enum category_ledger_outcome outcome = append(target, name, strlen(name));
        if (outcome != CATEGORY_LEDGER_ACCEPTED)
        {
            return outcome;
        }
    }
    return CATEGORY_LEDGER_ACCEPTED;
}

enum category_ledger_outcome
category_ledger_reconcile(const struct category_ledger *_Nonnull ledger,
                          const struct name_list *_Nonnull scanned,
                          struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable target = nullptr;
    enum category_ledger_outcome outcome = category_ledger_empty(&target);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    outcome = merge(target, ledger, scanned);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        category_ledger_destroy(target);
        return outcome;
    }
    *out = target;
    return CATEGORY_LEDGER_ACCEPTED;
}

/* 同じ順序・同じ値の新しい台帳を作る。1 か所だけ変える操作の土台（ARC-005）。 */
static enum category_ledger_outcome duplicate(const struct category_ledger *_Nonnull ledger,
                                              struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable target = nullptr;
    enum category_ledger_outcome outcome = category_ledger_empty(&target);
    size_t count = name_list_count(ledger->names);
    for (size_t entry = 0; outcome == CATEGORY_LEDGER_ACCEPTED && entry < count; ++entry)
    {
        outcome = copy_entry(target, ledger, entry);
    }
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        category_ledger_destroy(target);
        return outcome;
    }
    *out = target;
    return CATEGORY_LEDGER_ACCEPTED;
}

enum category_ledger_outcome
category_ledger_toggled(const struct category_ledger *_Nonnull ledger, size_t index,
                        struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable target = nullptr;
    enum category_ledger_outcome outcome = duplicate(ledger, &target);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    target->expanded[index] = !target->expanded[index];
    *out = target;
    return CATEGORY_LEDGER_ACCEPTED;
}

enum category_ledger_outcome
category_ledger_recolored(const struct category_ledger *_Nonnull ledger, size_t index,
                          struct rgb_color color, struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable target = nullptr;
    enum category_ledger_outcome outcome = duplicate(ledger, &target);
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    target->colors[index] = color;
    *out = target;
    return CATEGORY_LEDGER_ACCEPTED;
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

enum category_ledger_outcome category_ledger_moved(const struct category_ledger *_Nonnull ledger,
                                                   size_t from, size_t to,
                                                   struct category_ledger *_Nullable *_Nonnull out)
{
    struct category_ledger *_Nullable target = nullptr;
    enum category_ledger_outcome outcome = category_ledger_empty(&target);
    size_t count = name_list_count(ledger->names);
    for (size_t index = 0; outcome == CATEGORY_LEDGER_ACCEPTED && index < count; ++index)
    {
        outcome = copy_entry(target, ledger, source_index(from, to, index));
    }
    if (outcome != CATEGORY_LEDGER_ACCEPTED)
    {
        category_ledger_destroy(target);
        return outcome;
    }
    *out = target;
    return CATEGORY_LEDGER_ACCEPTED;
}

size_t category_ledger_count(const struct category_ledger *_Nonnull ledger)
{
    return name_list_count(ledger->names);
}

const char *_Nonnull category_ledger_name(const struct category_ledger *_Nonnull ledger,
                                          size_t index)
{
    return name_list_at(ledger->names, index);
}

struct rgb_color category_ledger_color(const struct category_ledger *_Nonnull ledger, size_t index)
{
    return ledger->colors[index];
}

bool category_ledger_expanded(const struct category_ledger *_Nonnull ledger, size_t index)
{
    return ledger->expanded[index];
}

void category_ledger_destroy(struct category_ledger *_Nullable ledger)
{
    if (ledger == nullptr)
    {
        return;
    }
    name_list_destroy(ledger->names);
    free(ledger->colors);
    free(ledger->expanded);
    free(ledger);
}
