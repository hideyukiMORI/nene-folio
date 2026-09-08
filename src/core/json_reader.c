#include "json_reader.h"

#include "json_reader_phase.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct json_reader
{
    const char *_Nonnull text;
    size_t length;
    size_t position;
    char *_Nonnull scratch; /* 展開済みの文字列。終端付き */
    size_t scratch_capacity;
    size_t scratch_length;
    uint32_t unsigned_value;
    enum json_reader_phase phase;
    enum json_token terminal; /* FAILED のときに返し続ける字句 */
    size_t depth;
    bool is_object[json_reader_max_depth];
    size_t members[json_reader_max_depth];
};

constexpr size_t scratch_initial = 64;
constexpr uint32_t unsigned_max = 0xFFFFFFFFu;

enum json_reader_outcome json_reader_create(const char *_Nonnull text, size_t length,
                                            struct json_reader *_Nullable *_Nonnull out)
{
    struct json_reader *_Nullable reader = calloc(1, sizeof *reader);
    if (reader == nullptr)
    {
        return JSON_READER_OUT_OF_MEMORY;
    }
    reader->scratch = malloc(scratch_initial);
    if (reader->scratch == nullptr)
    {
        free(reader);
        return JSON_READER_OUT_OF_MEMORY;
    }
    reader->scratch[0] = '\0';
    reader->scratch_capacity = scratch_initial;
    reader->text = text;
    reader->length = length;
    reader->phase = JSON_READER_PHASE_VALUE;
    reader->terminal = JSON_TOKEN_MALFORMED;
    *out = reader;
    return JSON_READER_CREATED;
}

static enum json_token fail(struct json_reader *_Nonnull reader, enum json_token terminal)
{
    reader->phase = JSON_READER_PHASE_FAILED;
    reader->terminal = terminal;
    return terminal;
}

static bool scratch_append(struct json_reader *_Nonnull reader, const char *_Nonnull bytes,
                           size_t count)
{
    if (reader->scratch_length + count + 1 > reader->scratch_capacity)
    {
        size_t capacity = reader->scratch_capacity * 2 + count;
        char *_Nullable grown = realloc(reader->scratch, capacity);
        if (grown == nullptr)
        {
            return false;
        }
        reader->scratch = grown;
        reader->scratch_capacity = capacity;
    }
    memcpy(reader->scratch + reader->scratch_length, bytes, count);
    reader->scratch_length += count;
    reader->scratch[reader->scratch_length] = '\0';
    return true;
}

static bool at_end(const struct json_reader *_Nonnull reader)
{
    return reader->position >= reader->length;
}

static char peek(const struct json_reader *_Nonnull reader)
{
    return reader->text[reader->position];
}

static void skip_whitespace(struct json_reader *_Nonnull reader)
{
    while (!at_end(reader))
    {
        char current = peek(reader);
        if (current != ' ' && current != '\t' && current != '\n' && current != '\r')
        {
            return;
        }
        reader->position += 1;
    }
}

static int hex_value(char digit)
{
    if (digit >= '0' && digit <= '9')
    {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f')
    {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F')
    {
        return digit - 'A' + 10;
    }
    return -1;
}

/* \u の後の 4 桁を読む。不正なら false。 */
static bool read_hex4(struct json_reader *_Nonnull reader, uint32_t *_Nonnull value)
{
    if (reader->position + 4 > reader->length)
    {
        return false;
    }
    uint32_t result = 0;
    for (size_t index = 0; index < 4; ++index)
    {
        int digit = hex_value(reader->text[reader->position + index]);
        if (digit < 0)
        {
            return false;
        }
        result = (result << 4) | (uint32_t)digit;
    }
    reader->position += 4;
    *value = result;
    return true;
}

/* \uXXXX（必要なら続く \uYYYY のサロゲート対）を 1 コードポイントに読む。 */
static bool read_unicode_escape(struct json_reader *_Nonnull reader, uint32_t *_Nonnull code_point)
{
    uint32_t first = 0;
    if (!read_hex4(reader, &first))
    {
        return false;
    }
    if (first < 0xD800 || first >= 0xE000)
    {
        *code_point = first;
        return true;
    }
    if (first >= 0xDC00 || reader->position + 2 > reader->length ||
        reader->text[reader->position] != '\\' || reader->text[reader->position + 1] != 'u')
    {
        return false;
    }
    reader->position += 2;
    uint32_t second = 0;
    if (!read_hex4(reader, &second) || second < 0xDC00 || second >= 0xE000)
    {
        return false;
    }
    *code_point = 0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
    return true;
}

/* 1 文字エスケープの展開先。対応しないものは '\0'。 */
static char simple_escape(char escape)
{
    static const char sources[] = "\"\\/bfnrt";
    static const char targets[] = "\"\\/\b\f\n\r\t";
    for (size_t index = 0; sources[index] != '\0'; ++index)
    {
        if (sources[index] == escape)
        {
            return targets[index];
        }
    }
    return '\0';
}

/* バックスラッシュの直後から 1 つのエスケープを展開する。 */
static enum json_token read_escape(struct json_reader *_Nonnull reader)
{
    if (at_end(reader))
    {
        return JSON_TOKEN_MALFORMED;
    }
    char escape = peek(reader);
    reader->position += 1;
    char encoded[utf8_text_max_encoded];
    size_t count = 0;
    if (escape == 'u')
    {
        uint32_t code_point = 0;
        if (!read_unicode_escape(reader, &code_point))
        {
            return JSON_TOKEN_MALFORMED;
        }
        count = utf8_text_encode(code_point, encoded);
    }
    else
    {
        encoded[0] = simple_escape(escape);
        count = encoded[0] == '\0' ? 0 : 1;
    }
    if (count == 0)
    {
        return JSON_TOKEN_MALFORMED;
    }
    return scratch_append(reader, encoded, count) ? JSON_TOKEN_STRING : JSON_TOKEN_OUT_OF_MEMORY;
}

/* 生の UTF-8 の 1 文字を検証して写す。 */
static enum json_token read_raw_character(struct json_reader *_Nonnull reader)
{
    uint32_t code_point = 0;
    size_t consumed = utf8_text_decode(reader->text + reader->position,
                                       reader->length - reader->position, &code_point);
    if (consumed == 0 || code_point < 0x20)
    {
        return JSON_TOKEN_MALFORMED;
    }
    if (!scratch_append(reader, reader->text + reader->position, consumed))
    {
        return JSON_TOKEN_OUT_OF_MEMORY;
    }
    reader->position += consumed;
    return JSON_TOKEN_STRING;
}

/* 開き引用符の直後から閉じ引用符までを scratch へ展開する。 */
static enum json_token read_string(struct json_reader *_Nonnull reader)
{
    reader->scratch_length = 0;
    reader->scratch[0] = '\0';
    while (!at_end(reader))
    {
        char current = peek(reader);
        if (current == '"')
        {
            reader->position += 1;
            return JSON_TOKEN_STRING;
        }
        enum json_token step = JSON_TOKEN_STRING;
        if (current == '\\')
        {
            reader->position += 1;
            step = read_escape(reader);
        }
        else
        {
            step = read_raw_character(reader);
        }
        if (step != JSON_TOKEN_STRING)
        {
            return step;
        }
    }
    return JSON_TOKEN_MALFORMED;
}

static bool is_digit(char character)
{
    return character >= '0' && character <= '9';
}

static size_t skip_digits(struct json_reader *_Nonnull reader)
{
    size_t count = 0;
    while (!at_end(reader) && is_digit(peek(reader)))
    {
        reader->position += 1;
        count += 1;
    }
    return count;
}

/* 次の文字が first か second なら 1 つ進めて true。 */
static bool accept_either(struct json_reader *_Nonnull reader, char first, char second)
{
    if (at_end(reader) || (peek(reader) != first && peek(reader) != second))
    {
        return false;
    }
    reader->position += 1;
    return true;
}

/* 小数部があれば読み飛ばす。あるのに数字が続かなければ false。 */
static bool skip_fraction(struct json_reader *_Nonnull reader)
{
    if (!accept_either(reader, '.', '.'))
    {
        return true;
    }
    return skip_digits(reader) > 0;
}

/* 指数部があれば読み飛ばす。あるのに数字が続かなければ false。 */
static bool skip_exponent(struct json_reader *_Nonnull reader)
{
    if (!accept_either(reader, 'e', 'E'))
    {
        return true;
    }
    accept_either(reader, '+', '-');
    return skip_digits(reader) > 0;
}

/* 整数部を読み、32 bit に収まる非負整数ならその値を返す（収まらなければ overflow）。 */
static bool read_integer_part(struct json_reader *_Nonnull reader, uint32_t *_Nonnull value,
                              bool *_Nonnull overflow)
{
    size_t start = reader->position;
    if (!at_end(reader) && peek(reader) == '0')
    {
        reader->position += 1;
        *value = 0;
        return true;
    }
    if (skip_digits(reader) == 0)
    {
        return false;
    }
    uint64_t accumulated = 0;
    for (size_t index = start; index < reader->position; ++index)
    {
        accumulated = accumulated * 10 + (uint64_t)(reader->text[index] - '0');
        if (accumulated > unsigned_max)
        {
            *overflow = true;
            return true;
        }
    }
    *value = (uint32_t)accumulated;
    return true;
}

static enum json_token read_number(struct json_reader *_Nonnull reader)
{
    bool negative = peek(reader) == '-';
    if (negative)
    {
        reader->position += 1;
    }
    uint32_t value = 0;
    bool overflow = false;
    if (!read_integer_part(reader, &value, &overflow))
    {
        return JSON_TOKEN_MALFORMED;
    }
    size_t integer_end = reader->position;
    if (!skip_fraction(reader) || !skip_exponent(reader))
    {
        return JSON_TOKEN_MALFORMED;
    }
    if (negative || overflow || reader->position != integer_end)
    {
        return JSON_TOKEN_NUMBER;
    }
    reader->unsigned_value = value;
    return JSON_TOKEN_UNSIGNED;
}

static enum json_token read_literal(struct json_reader *_Nonnull reader)
{
    static const char *const words[] = {"true", "false", "null"};
    static const enum json_token tokens[] = {JSON_TOKEN_TRUE, JSON_TOKEN_FALSE, JSON_TOKEN_NULL};
    for (size_t index = 0; index < 3; ++index)
    {
        size_t word_length = strlen(words[index]);
        if (reader->position + word_length <= reader->length &&
            memcmp(reader->text + reader->position, words[index], word_length) == 0)
        {
            reader->position += word_length;
            return tokens[index];
        }
    }
    return JSON_TOKEN_MALFORMED;
}

static bool push(struct json_reader *_Nonnull reader, bool is_object)
{
    if (reader->depth >= json_reader_max_depth)
    {
        return false;
    }
    reader->is_object[reader->depth] = is_object;
    reader->members[reader->depth] = 0;
    reader->depth += 1;
    reader->phase = is_object ? JSON_READER_PHASE_KEY : JSON_READER_PHASE_VALUE;
    return true;
}

/* 値を 1 つ読み終えた後の位置へ進む。 */
static void after_value(struct json_reader *_Nonnull reader)
{
    if (reader->depth == 0)
    {
        reader->phase = JSON_READER_PHASE_DONE;
        return;
    }
    reader->members[reader->depth - 1] += 1;
    reader->phase = JSON_READER_PHASE_AFTER_VALUE;
}

static enum json_token pop(struct json_reader *_Nonnull reader, bool is_object)
{
    if (reader->depth == 0 || reader->is_object[reader->depth - 1] != is_object)
    {
        return JSON_TOKEN_MALFORMED;
    }
    reader->depth -= 1;
    after_value(reader);
    return is_object ? JSON_TOKEN_OBJECT_END : JSON_TOKEN_ARRAY_END;
}

static bool container_empty(const struct json_reader *_Nonnull reader)
{
    return reader->depth > 0 && reader->members[reader->depth - 1] == 0;
}

static enum json_token read_scalar(struct json_reader *_Nonnull reader, char current)
{
    if (current == '"')
    {
        reader->position += 1;
        return read_string(reader);
    }
    if (current == '-' || is_digit(current))
    {
        return read_number(reader);
    }
    return read_literal(reader);
}

static enum json_token read_value(struct json_reader *_Nonnull reader)
{
    char current = peek(reader);
    if (current == '{' || current == '[')
    {
        reader->position += 1;
        if (!push(reader, current == '{'))
        {
            return JSON_TOKEN_MALFORMED;
        }
        return current == '{' ? JSON_TOKEN_OBJECT_BEGIN : JSON_TOKEN_ARRAY_BEGIN;
    }
    if (current == ']' && container_empty(reader))
    {
        reader->position += 1;
        return pop(reader, false);
    }
    enum json_token token = read_scalar(reader, current);
    if (token != JSON_TOKEN_MALFORMED && token != JSON_TOKEN_OUT_OF_MEMORY)
    {
        after_value(reader);
    }
    return token;
}

/* キーを読み、続く : まで消費する。 */
static enum json_token read_key(struct json_reader *_Nonnull reader)
{
    char current = peek(reader);
    if (current == '}' && container_empty(reader))
    {
        reader->position += 1;
        return pop(reader, true);
    }
    if (current != '"')
    {
        return JSON_TOKEN_MALFORMED;
    }
    reader->position += 1;
    enum json_token token = read_string(reader);
    if (token != JSON_TOKEN_STRING)
    {
        return token;
    }
    skip_whitespace(reader);
    if (at_end(reader) || peek(reader) != ':')
    {
        return JSON_TOKEN_MALFORMED;
    }
    reader->position += 1;
    reader->phase = JSON_READER_PHASE_VALUE;
    return JSON_TOKEN_KEY;
}

/* , なら次の要素へ進み（字句は出さない）、閉じ括弧なら容器を閉じる。 */
static enum json_token read_separator(struct json_reader *_Nonnull reader)
{
    char current = peek(reader);
    reader->position += 1;
    if (current == ',')
    {
        reader->phase =
            reader->is_object[reader->depth - 1] ? JSON_READER_PHASE_KEY : JSON_READER_PHASE_VALUE;
        return JSON_TOKEN_KEY; /* 継続を示す内部値。呼び出し側でループする */
    }
    if (current == '}' || current == ']')
    {
        return pop(reader, current == '}');
    }
    return JSON_TOKEN_MALFORMED;
}

static enum json_token step(struct json_reader *_Nonnull reader)
{
    skip_whitespace(reader);
    if (at_end(reader))
    {
        if (reader->phase != JSON_READER_PHASE_DONE)
        {
            return JSON_TOKEN_MALFORMED;
        }
        reader->phase = JSON_READER_PHASE_ENDED;
        return JSON_TOKEN_END;
    }
    switch (reader->phase)
    {
    case JSON_READER_PHASE_VALUE:
        return read_value(reader);
    case JSON_READER_PHASE_KEY:
        return read_key(reader);
    case JSON_READER_PHASE_AFTER_VALUE:
        return read_separator(reader);
    case JSON_READER_PHASE_DONE:
    case JSON_READER_PHASE_ENDED:
    case JSON_READER_PHASE_FAILED:
        return JSON_TOKEN_MALFORMED;
    }
    return JSON_TOKEN_MALFORMED;
}

enum json_token json_reader_next(struct json_reader *_Nonnull reader)
{
    if (reader->phase == JSON_READER_PHASE_FAILED)
    {
        return reader->terminal;
    }
    if (reader->phase == JSON_READER_PHASE_ENDED)
    {
        return JSON_TOKEN_END;
    }
    for (;;)
    {
        bool separator = reader->phase == JSON_READER_PHASE_AFTER_VALUE;
        enum json_token token = step(reader);
        if (token == JSON_TOKEN_MALFORMED || token == JSON_TOKEN_OUT_OF_MEMORY)
        {
            return fail(reader, token);
        }
        if (!(separator && token == JSON_TOKEN_KEY))
        {
            return token;
        }
    }
}

const char *_Nonnull json_reader_text(const struct json_reader *_Nonnull reader)
{
    return reader->scratch;
}

size_t json_reader_text_length(const struct json_reader *_Nonnull reader)
{
    return reader->scratch_length;
}

uint32_t json_reader_unsigned(const struct json_reader *_Nonnull reader)
{
    return reader->unsigned_value;
}

void json_reader_destroy(struct json_reader *_Nullable reader)
{
    if (reader == nullptr)
    {
        return;
    }
    free(reader->scratch);
    free(reader);
}
