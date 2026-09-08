#include "note_ledger.h"

#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"

#include <stdlib.h>
#include <string.h>

struct note_ledger
{
    struct name_list *_Nonnull names;
};

constexpr uint32_t ledger_version = 1;

enum note_ledger_outcome note_ledger_empty(struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable ledger = calloc(1, sizeof *ledger);
    if (ledger == nullptr)
    {
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    if (name_list_create(&ledger->names) != NAME_LIST_ACCEPTED)
    {
        free(ledger);
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    *out = ledger;
    return NOTE_LEDGER_ACCEPTED;
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
    return json_reader_next(reader) == JSON_TOKEN_KEY && strcmp(json_reader_text(reader), key) == 0;
}

/* 期待しない字句を結果に写す。記憶不足だけは不正と区別する。 */
static enum note_ledger_outcome unexpected(enum json_token token)
{
    return token == JSON_TOKEN_OUT_OF_MEMORY ? NOTE_LEDGER_OUT_OF_MEMORY : NOTE_LEDGER_MALFORMED;
}

/* 直前に読んだ文字列をノート名として足す。 */
static enum note_ledger_outcome append_current(const struct json_reader *_Nonnull reader,
                                               struct note_ledger *_Nonnull ledger)
{
    return translate(
        name_list_append(ledger->names, json_reader_text(reader), json_reader_text_length(reader)));
}

static enum note_ledger_outcome parse_names(struct json_reader *_Nonnull reader,
                                            struct note_ledger *_Nonnull ledger)
{
    if (!expect_key(reader, "notes") || json_reader_next(reader) != JSON_TOKEN_ARRAY_BEGIN)
    {
        return NOTE_LEDGER_MALFORMED;
    }
    for (;;)
    {
        enum json_token token = json_reader_next(reader);
        if (token == JSON_TOKEN_ARRAY_END)
        {
            return NOTE_LEDGER_ACCEPTED;
        }
        enum note_ledger_outcome outcome =
            token == JSON_TOKEN_STRING ? append_current(reader, ledger) : unexpected(token);
        if (outcome != NOTE_LEDGER_ACCEPTED)
        {
            return outcome;
        }
    }
}

static enum note_ledger_outcome parse_document(struct json_reader *_Nonnull reader,
                                               struct note_ledger *_Nonnull ledger)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !expect_key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
    {
        return NOTE_LEDGER_MALFORMED;
    }
    if (json_reader_unsigned(reader) != ledger_version)
    {
        return NOTE_LEDGER_UNSUPPORTED_VERSION;
    }
    enum note_ledger_outcome outcome = parse_names(reader, ledger);
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END ||
        json_reader_next(reader) != JSON_TOKEN_END)
    {
        return NOTE_LEDGER_MALFORMED;
    }
    return NOTE_LEDGER_ACCEPTED;
}

enum note_ledger_outcome note_ledger_parse(const char *_Nonnull text, size_t length,
                                           struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable ledger = nullptr;
    enum note_ledger_outcome outcome = note_ledger_empty(&ledger);
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
    {
        note_ledger_destroy(ledger);
        return NOTE_LEDGER_OUT_OF_MEMORY;
    }
    outcome = parse_document(reader, ledger);
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
    json_writer_object_end(writer);
}

/* names のうち、keep にあり skip に無いものを順に target へ足す。 */
static enum note_ledger_outcome append_filtered(struct note_ledger *_Nonnull target,
                                                const struct name_list *_Nonnull names,
                                                const struct name_list *_Nonnull keep,
                                                const struct name_list *_Nullable skip)
{
    size_t count = name_list_count(names);
    for (size_t index = 0; index < count; ++index)
    {
        const char *_Nonnull name = name_list_at(names, index);
        if (!name_list_contains(keep, name) || (skip != nullptr && name_list_contains(skip, name)))
        {
            continue;
        }
        enum note_ledger_outcome outcome =
            translate(name_list_append(target->names, name, strlen(name)));
        if (outcome != NOTE_LEDGER_ACCEPTED)
        {
            return outcome;
        }
    }
    return NOTE_LEDGER_ACCEPTED;
}

enum note_ledger_outcome note_ledger_reconcile(const struct note_ledger *_Nonnull ledger,
                                               const struct name_list *_Nonnull scanned,
                                               struct note_ledger *_Nullable *_Nonnull out)
{
    struct note_ledger *_Nullable target = nullptr;
    enum note_ledger_outcome outcome = note_ledger_empty(&target);
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        return outcome;
    }
    outcome = append_filtered(target, ledger->names, scanned, nullptr);
    if (outcome == NOTE_LEDGER_ACCEPTED)
    {
        outcome = append_filtered(target, scanned, scanned, ledger->names);
    }
    if (outcome != NOTE_LEDGER_ACCEPTED)
    {
        note_ledger_destroy(target);
        return outcome;
    }
    *out = target;
    return NOTE_LEDGER_ACCEPTED;
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
    free(ledger);
}
