#include "json_writer.h"

#include <stdlib.h>
#include <string.h>

constexpr size_t writer_max_depth = 8;
constexpr size_t buffer_initial = 256;

struct json_writer
{
    char *_Nonnull buffer; /* 終端付き */
    size_t capacity;
    size_t length;
    enum json_writer_outcome failure; /* ACCEPTED なら失敗していない */
    size_t depth;
    bool is_object[writer_max_depth];
    size_t members[writer_max_depth];
    bool key_pending; /* オブジェクトでキーを書き、値を待っている */
    bool root_written;
};

enum json_writer_outcome json_writer_create(struct json_writer *_Nullable *_Nonnull out)
{
    struct json_writer *_Nullable writer = calloc(1, sizeof *writer);
    if (writer == nullptr)
    {
        return JSON_WRITER_OUT_OF_MEMORY;
    }
    writer->buffer = malloc(buffer_initial);
    if (writer->buffer == nullptr)
    {
        free(writer);
        return JSON_WRITER_OUT_OF_MEMORY;
    }
    writer->buffer[0] = '\0';
    writer->capacity = buffer_initial;
    writer->failure = JSON_WRITER_ACCEPTED;
    *out = writer;
    return JSON_WRITER_ACCEPTED;
}

static void append(struct json_writer *_Nonnull writer, const char *_Nonnull bytes, size_t count)
{
    if (writer->failure != JSON_WRITER_ACCEPTED)
    {
        return;
    }
    if (writer->length + count + 1 > writer->capacity)
    {
        size_t capacity = writer->capacity * 2 + count;
        char *_Nullable grown = realloc(writer->buffer, capacity);
        if (grown == nullptr)
        {
            writer->failure = JSON_WRITER_OUT_OF_MEMORY;
            return;
        }
        writer->buffer = grown;
        writer->capacity = capacity;
    }
    memcpy(writer->buffer + writer->length, bytes, count);
    writer->length += count;
    writer->buffer[writer->length] = '\0';
}

static void append_text(struct json_writer *_Nonnull writer, const char *_Nonnull text)
{
    append(writer, text, strlen(text));
}

static void newline_and_indent(struct json_writer *_Nonnull writer)
{
    append(writer, "\n", 1);
    for (size_t level = 0; level < writer->depth; ++level)
    {
        append(writer, "  ", 2);
    }
}

/* 配列の次の要素の前に区切りと字下げを書く。 */
static void separate_array_element(struct json_writer *_Nonnull writer)
{
    size_t *_Nonnull members = &writer->members[writer->depth - 1];
    if (*members > 0)
    {
        append(writer, ",", 1);
    }
    newline_and_indent(writer);
    *members += 1;
}

/* 値を書く前の位置を整える。書けない位置なら MALFORMED を記憶して false。 */
static bool begin_value(struct json_writer *_Nonnull writer)
{
    if (writer->failure != JSON_WRITER_ACCEPTED)
    {
        return false;
    }
    if (writer->depth == 0)
    {
        if (writer->root_written)
        {
            writer->failure = JSON_WRITER_MALFORMED;
            return false;
        }
        writer->root_written = true;
        return true;
    }
    bool in_object = writer->is_object[writer->depth - 1];
    if (in_object != writer->key_pending)
    {
        writer->failure = JSON_WRITER_MALFORMED;
        return false;
    }
    if (!in_object)
    {
        separate_array_element(writer);
    }
    writer->key_pending = false;
    return true;
}

static void begin_container(struct json_writer *_Nonnull writer, bool is_object)
{
    if (!begin_value(writer))
    {
        return;
    }
    if (writer->depth >= writer_max_depth)
    {
        writer->failure = JSON_WRITER_MALFORMED;
        return;
    }
    append(writer, is_object ? "{" : "[", 1);
    writer->is_object[writer->depth] = is_object;
    writer->members[writer->depth] = 0;
    writer->depth += 1;
}

static void end_container(struct json_writer *_Nonnull writer, bool is_object)
{
    if (writer->failure != JSON_WRITER_ACCEPTED)
    {
        return;
    }
    if (writer->depth == 0 || writer->is_object[writer->depth - 1] != is_object ||
        writer->key_pending)
    {
        writer->failure = JSON_WRITER_MALFORMED;
        return;
    }
    size_t members = writer->members[writer->depth - 1];
    writer->depth -= 1;
    if (members > 0)
    {
        newline_and_indent(writer);
    }
    append(writer, is_object ? "}" : "]", 1);
}

void json_writer_object_begin(struct json_writer *_Nonnull writer)
{
    begin_container(writer, true);
}

void json_writer_object_end(struct json_writer *_Nonnull writer)
{
    end_container(writer, true);
}

void json_writer_array_begin(struct json_writer *_Nonnull writer)
{
    begin_container(writer, false);
}

void json_writer_array_end(struct json_writer *_Nonnull writer)
{
    end_container(writer, false);
}

static void append_escaped(struct json_writer *_Nonnull writer, const char *_Nonnull text)
{
    static const char digits[] = "0123456789ABCDEF";
    append(writer, "\"", 1);
    for (const char *_Nonnull cursor = text; *cursor != '\0'; ++cursor)
    {
        unsigned char byte = (unsigned char)*cursor;
        if (byte == '"' || byte == '\\')
        {
            char escaped[2] = {'\\', *cursor};
            append(writer, escaped, 2);
        }
        else if (byte < 0x20)
        {
            char escaped[6] = {'\\', 'u', '0', '0', digits[byte >> 4], digits[byte & 0x0F]};
            append(writer, escaped, 6);
        }
        else
        {
            append(writer, cursor, 1);
        }
    }
    append(writer, "\"", 1);
}

void json_writer_key(struct json_writer *_Nonnull writer, const char *_Nonnull text)
{
    if (writer->failure != JSON_WRITER_ACCEPTED)
    {
        return;
    }
    if (writer->depth == 0 || !writer->is_object[writer->depth - 1] || writer->key_pending)
    {
        writer->failure = JSON_WRITER_MALFORMED;
        return;
    }
    separate_array_element(writer);
    append_escaped(writer, text);
    append(writer, ": ", 2);
    writer->key_pending = true;
}

void json_writer_string(struct json_writer *_Nonnull writer, const char *_Nonnull text)
{
    if (begin_value(writer))
    {
        append_escaped(writer, text);
    }
}

void json_writer_unsigned(struct json_writer *_Nonnull writer, uint32_t value)
{
    if (!begin_value(writer))
    {
        return;
    }
    char digits[10];
    size_t count = 0;
    do
    {
        digits[count] = (char)('0' + value % 10);
        count += 1;
        value /= 10;
    } while (value > 0);
    for (size_t index = 0; index < count; ++index)
    {
        append(writer, &digits[count - 1 - index], 1);
    }
}

void json_writer_boolean(struct json_writer *_Nonnull writer, bool value)
{
    if (begin_value(writer))
    {
        append_text(writer, value ? "true" : "false");
    }
}

enum json_writer_outcome json_writer_finish(const struct json_writer *_Nonnull writer)
{
    if (writer->failure != JSON_WRITER_ACCEPTED)
    {
        return writer->failure;
    }
    if (writer->depth != 0 || !writer->root_written)
    {
        return JSON_WRITER_MALFORMED;
    }
    return JSON_WRITER_ACCEPTED;
}

const char *_Nonnull json_writer_text(const struct json_writer *_Nonnull writer)
{
    return writer->buffer;
}

size_t json_writer_length(const struct json_writer *_Nonnull writer)
{
    return writer->length;
}

void json_writer_destroy(struct json_writer *_Nullable writer)
{
    if (writer == nullptr)
    {
        return;
    }
    free(writer->buffer);
    free(writer);
}
