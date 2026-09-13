#include "rename_journal.h"
#include "json_reader.h"
#include "json_writer.h"
#include "note_rename.h"
#include <stdlib.h>
#include <string.h>

struct rename_journal
{
    struct note_rename *_Nullable rename;
    char file_id[rename_journal_id_length + 1];
    char history_id[rename_journal_id_length + 1];
};

constexpr uint32_t journal_version = 1;

static bool valid_id(const char *_Nonnull text, size_t length)
{
    if (length != rename_journal_id_length)
    {
        return false;
    }
    for (size_t index = 0; index < length; ++index)
    {
        char digit = text[index];
        if (!((digit >= '0' && digit <= '9') || (digit >= 'a' && digit <= 'f')))
        {
            return false;
        }
    }
    return true;
}

static enum rename_journal_outcome from_writer(enum json_writer_outcome outcome)
{
    switch (outcome)
    {
    case JSON_WRITER_ACCEPTED:
        return RENAME_JOURNAL_ACCEPTED;
    case JSON_WRITER_MALFORMED:
        return RENAME_JOURNAL_INVALID;
    case JSON_WRITER_OUT_OF_MEMORY:
        return RENAME_JOURNAL_OUT_OF_MEMORY;
    }
    return RENAME_JOURNAL_INVALID;
}

enum rename_journal_outcome rename_journal_write(const struct note_rename *_Nonnull rename,
                                                 const char *_Nonnull file_id,
                                                 const char *_Nonnull history_id,
                                                 struct json_writer *_Nonnull writer)
{
    if (!valid_id(file_id, strlen(file_id)) ||
        (history_id[0] != '\0' && !valid_id(history_id, strlen(history_id))))
    {
        return RENAME_JOURNAL_INVALID;
    }
    json_writer_object_begin(writer);
    json_writer_key(writer, "version");
    json_writer_unsigned(writer, journal_version);
    json_writer_key(writer, "rename");
    note_rename_write(rename, writer);
    json_writer_key(writer, "fileId");
    json_writer_string(writer, file_id);
    json_writer_key(writer, "historyId");
    json_writer_string(writer, history_id);
    json_writer_object_end(writer);
    return from_writer(json_writer_finish(writer));
}

static bool key(struct json_reader *_Nonnull reader, const char *_Nonnull expected)
{
    return json_reader_next(reader) == JSON_TOKEN_KEY &&
           json_reader_text_length(reader) == strlen(expected) &&
           strcmp(json_reader_text(reader), expected) == 0;
}

static enum rename_journal_outcome unexpected(struct json_reader *_Nonnull reader)
{
    return json_reader_next(reader) == JSON_TOKEN_OUT_OF_MEMORY ? RENAME_JOURNAL_OUT_OF_MEMORY
                                                                : RENAME_JOURNAL_INVALID;
}

static enum rename_journal_outcome read_id(struct json_reader *_Nonnull reader,
                                           const char *_Nonnull field, bool optional,
                                           char *_Nonnull out)
{
    if (!key(reader, field) || json_reader_next(reader) != JSON_TOKEN_STRING)
    {
        return unexpected(reader);
    }
    const char *_Nonnull text = json_reader_text(reader);
    size_t length = json_reader_text_length(reader);
    if (!(optional && length == 0) && !valid_id(text, length))
    {
        return RENAME_JOURNAL_INVALID;
    }
    memcpy(out, text, length + 1);
    return RENAME_JOURNAL_ACCEPTED;
}

static enum rename_journal_outcome read_header(struct json_reader *_Nonnull reader)
{
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_BEGIN || !key(reader, "version") ||
        json_reader_next(reader) != JSON_TOKEN_UNSIGNED)
    {
        return unexpected(reader);
    }
    if (json_reader_unsigned(reader) != journal_version)
    {
        return RENAME_JOURNAL_UNSUPPORTED_VERSION;
    }
    if (!key(reader, "rename"))
    {
        return unexpected(reader);
    }
    return RENAME_JOURNAL_ACCEPTED;
}

static enum rename_journal_outcome read_document(struct json_reader *_Nonnull reader,
                                                 struct rename_journal *_Nonnull journal)
{
    enum rename_journal_outcome header = read_header(reader);
    if (header != RENAME_JOURNAL_ACCEPTED)
    {
        return header;
    }
    enum note_rename_outcome parsed = note_rename_read(reader, &journal->rename);
    if (parsed != NOTE_RENAME_ACCEPTED)
    {
        return parsed == NOTE_RENAME_OUT_OF_MEMORY ? RENAME_JOURNAL_OUT_OF_MEMORY
                                                   : RENAME_JOURNAL_INVALID;
    }
    enum rename_journal_outcome outcome = read_id(reader, "fileId", false, journal->file_id);
    if (outcome == RENAME_JOURNAL_ACCEPTED)
    {
        outcome = read_id(reader, "historyId", true, journal->history_id);
    }
    if (outcome != RENAME_JOURNAL_ACCEPTED)
    {
        return outcome;
    }
    if (json_reader_next(reader) != JSON_TOKEN_OBJECT_END ||
        json_reader_next(reader) != JSON_TOKEN_END)
    {
        return unexpected(reader);
    }
    return RENAME_JOURNAL_ACCEPTED;
}

enum rename_journal_outcome rename_journal_parse(const char *_Nonnull text, size_t length,
                                                 struct rename_journal *_Nullable *_Nonnull out)
{
    struct rename_journal *_Nullable journal = calloc(1, sizeof *journal);
    if (journal == nullptr)
    {
        return RENAME_JOURNAL_OUT_OF_MEMORY;
    }
    struct json_reader *_Nullable reader = nullptr;
    if (json_reader_create(text, length, &reader) != JSON_READER_CREATED)
    {
        rename_journal_destroy(journal);
        return RENAME_JOURNAL_OUT_OF_MEMORY;
    }
    enum rename_journal_outcome outcome = read_document(reader, journal);
    json_reader_destroy(reader);
    if (outcome != RENAME_JOURNAL_ACCEPTED)
    {
        rename_journal_destroy(journal);
        return outcome;
    }
    *out = journal;
    return RENAME_JOURNAL_ACCEPTED;
}

const struct note_rename *_Nonnull rename_journal_rename(
    const struct rename_journal *_Nonnull journal)
{
    return journal->rename;
}

const char *_Nonnull rename_journal_file_id(const struct rename_journal *_Nonnull journal)
{
    return journal->file_id;
}

const char *_Nonnull rename_journal_history_id(const struct rename_journal *_Nonnull journal)
{
    return journal->history_id;
}

void rename_journal_destroy(struct rename_journal *_Nullable journal)
{
    if (journal == nullptr)
    {
        return;
    }
    note_rename_destroy(journal->rename);
    free(journal);
}
