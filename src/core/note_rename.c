#include "note_rename.h"
#include "json_reader.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_name.h"
#include <stdlib.h>
#include <string.h>

struct note_rename
{
    struct name_list *_Nullable category;
    struct name_list *_Nullable names;
    struct note_ledger *_Nullable ledger;
};

static enum note_rename_outcome from_name(enum name_list_outcome outcome)
{
    switch (outcome)
    {
    case NAME_LIST_ACCEPTED:
        return NOTE_RENAME_ACCEPTED;
    case NAME_LIST_DUPLICATE:
    case NAME_LIST_INVALID_NAME:
        return NOTE_RENAME_INVALID;
    case NAME_LIST_OUT_OF_MEMORY:
        return NOTE_RENAME_OUT_OF_MEMORY;
    }
    return NOTE_RENAME_INVALID;
}

static enum note_rename_outcome from_ledger(enum note_ledger_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_LEDGER_ACCEPTED:
        return NOTE_RENAME_ACCEPTED;
    case NOTE_LEDGER_MALFORMED:
    case NOTE_LEDGER_UNSUPPORTED_VERSION:
        return NOTE_RENAME_INVALID;
    case NOTE_LEDGER_OUT_OF_MEMORY:
        return NOTE_RENAME_OUT_OF_MEMORY;
    }
    return NOTE_RENAME_INVALID;
}

static enum note_rename_outcome empty(struct note_rename *_Nullable *_Nonnull out)
{
    struct note_rename *_Nullable rename = calloc(1, sizeof *rename);
    if (rename == nullptr)
    {
        return NOTE_RENAME_OUT_OF_MEMORY;
    }
    if (name_list_create(&rename->category) != NAME_LIST_ACCEPTED ||
        name_list_create(&rename->names) != NAME_LIST_ACCEPTED)
    {
        note_rename_destroy(rename);
        return NOTE_RENAME_OUT_OF_MEMORY;
    }
    *out = rename;
    return NOTE_RENAME_ACCEPTED;
}

static enum note_rename_outcome append_name(struct name_list *_Nonnull list,
                                            const char *_Nonnull text, size_t limit)
{
    size_t length = strlen(text);
    return length > limit ? NOTE_RENAME_INVALID : from_name(name_list_append(list, text, length));
}

enum note_rename_outcome note_rename_create(const char *_Nonnull category,
                                            const struct note_ledger *_Nonnull ledger,
                                            const struct note_rename_target *_Nonnull target,
                                            struct note_rename *_Nullable *_Nonnull out)
{
    if (target->index >= note_ledger_count(ledger))
    {
        return NOTE_RENAME_INVALID;
    }
    struct note_rename *_Nullable rename = nullptr;
    enum note_rename_outcome outcome = empty(&rename);
    if (outcome != NOTE_RENAME_ACCEPTED)
    {
        return outcome;
    }
    outcome = append_name(rename->category, category, name_list_max_length);
    if (outcome == NOTE_RENAME_ACCEPTED)
    {
        outcome = append_name(rename->names, note_ledger_name(ledger, target->index),
                              name_list_max_length - 3);
    }
    if (outcome == NOTE_RENAME_ACCEPTED)
    {
        outcome =
            append_name(rename->names, note_name_stem(target->name), name_list_max_length - 3);
    }
    if (outcome == NOTE_RENAME_ACCEPTED)
    {
        outcome = from_ledger(note_ledger_renamed(ledger, target->index,
                                                  note_name_stem(target->name), &rename->ledger));
    }
    if (outcome != NOTE_RENAME_ACCEPTED)
    {
        note_rename_destroy(rename);
        return outcome;
    }
    *out = rename;
    return NOTE_RENAME_ACCEPTED;
}

static bool key(struct json_reader *_Nonnull reader, const char *_Nonnull expected)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY &&
           json_reader_text_length(reader) == strlen(expected) &&
           strcmp(json_reader_text(reader), expected) == 0;
}

/* readerのOUT_OF_MEMORYはsticky。構文不正へ丸めずに保持する。 */
static enum note_rename_outcome unexpected(struct json_reader *_Nonnull reader)
{
    return json_reader_next(reader) == JSON_TOKEN_OUT_OF_MEMORY ? NOTE_RENAME_OUT_OF_MEMORY
                                                                : NOTE_RENAME_INVALID;
}

static enum note_rename_outcome read_name(struct json_reader *_Nonnull reader,
                                          struct name_list *_Nonnull list,
                                          const char *_Nonnull field, size_t limit)
{
    if (!key(reader, field) || json_reader_next(reader) != JSON_TOKEN_STRING)
    {
        return unexpected(reader);
    }
    size_t length = json_reader_text_length(reader);
    return length > limit ? NOTE_RENAME_INVALID
                          : from_name(name_list_append(list, json_reader_text(reader), length));
}

static bool agrees_with_ledger(const struct note_rename *_Nonnull rename)
{
    bool found = false;
    size_t count = note_ledger_count(rename->ledger);
    for (size_t index = 0; index < count; ++index)
    {
        const char *_Nonnull name = note_ledger_name(rename->ledger, index);
        if (strcmp(name, note_rename_from(rename)) == 0)
        {
            return false;
        }
        found = found || strcmp(name, note_rename_to(rename)) == 0;
    }
    return found;
}

static enum note_rename_outcome read_document(struct json_reader *_Nonnull reader,
                                              struct note_rename *_Nonnull rename)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN)
    {
        return unexpected(reader);
    }
    enum note_rename_outcome outcome =
        read_name(reader, rename->category, "category", name_list_max_length);
    if (outcome == NOTE_RENAME_ACCEPTED)
    {
        outcome = read_name(reader, rename->names, "from", name_list_max_length - 3);
    }
    if (outcome == NOTE_RENAME_ACCEPTED)
    {
        outcome = read_name(reader, rename->names, "to", name_list_max_length - 3);
    }
    if (outcome != NOTE_RENAME_ACCEPTED)
    {
        return outcome;
    }
    if (!key(reader, "ledger"))
    {
        return unexpected(reader);
    }
    outcome = from_ledger(note_ledger_read(reader, &rename->ledger));
    if (outcome != NOTE_RENAME_ACCEPTED)
    {
        return outcome;
    }
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END)
    {
        return unexpected(reader);
    }
    return agrees_with_ledger(rename) ? NOTE_RENAME_ACCEPTED : NOTE_RENAME_INVALID;
}

enum note_rename_outcome note_rename_read(struct json_reader *_Nonnull reader,
                                          struct note_rename *_Nullable *_Nonnull out)
{
    struct note_rename *_Nullable rename = nullptr;
    enum note_rename_outcome outcome = empty(&rename);
    if (outcome != NOTE_RENAME_ACCEPTED)
    {
        return outcome;
    }
    outcome = read_document(reader, rename);
    if (outcome != NOTE_RENAME_ACCEPTED)
    {
        note_rename_destroy(rename);
        return outcome;
    }
    *out = rename;
    return NOTE_RENAME_ACCEPTED;
}

void note_rename_write(const struct note_rename *_Nonnull rename,
                       struct json_writer *_Nonnull writer)
{
    json_writer_object_begin(writer);
    json_writer_key(writer, "category");
    json_writer_string(writer, note_rename_category(rename));
    json_writer_key(writer, "from");
    json_writer_string(writer, note_rename_from(rename));
    json_writer_key(writer, "to");
    json_writer_string(writer, note_rename_to(rename));
    json_writer_key(writer, "ledger");
    note_ledger_write(rename->ledger, writer);
    json_writer_object_end(writer);
}

const char *_Nonnull note_rename_category(const struct note_rename *_Nonnull rename)
{
    return name_list_at(rename->category, 0);
}

const char *_Nonnull note_rename_from(const struct note_rename *_Nonnull rename)
{
    return name_list_at(rename->names, 0);
}

const char *_Nonnull note_rename_to(const struct note_rename *_Nonnull rename)
{
    return name_list_at(rename->names, 1);
}

const struct note_ledger *_Nonnull note_rename_ledger(const struct note_rename *_Nonnull rename)
{
    return rename->ledger;
}

bool note_rename_equals(const struct note_rename *_Nonnull left,
                        const struct note_rename *_Nonnull right)
{
    if (strcmp(note_rename_category(left), note_rename_category(right)) != 0 ||
        strcmp(note_rename_from(left), note_rename_from(right)) != 0 ||
        strcmp(note_rename_to(left), note_rename_to(right)) != 0)
    {
        return false;
    }
    size_t count = note_ledger_count(left->ledger);
    if (count != note_ledger_count(right->ledger))
    {
        return false;
    }
    for (size_t index = 0; index < count; ++index)
    {
        if (strcmp(note_ledger_name(left->ledger, index), note_ledger_name(right->ledger, index)) !=
            0)
        {
            return false;
        }
    }
    return true;
}

struct note_ledger *_Nonnull note_rename_take_ledger(struct note_rename *_Nonnull rename)
{
    struct note_ledger *_Nonnull ledger = rename->ledger;
    rename->ledger = nullptr;
    return ledger;
}

void note_rename_destroy(struct note_rename *_Nullable rename)
{
    if (rename == nullptr)
    {
        return;
    }
    name_list_destroy(rename->category);
    name_list_destroy(rename->names);
    note_ledger_destroy(rename->ledger);
    free(rename);
}
