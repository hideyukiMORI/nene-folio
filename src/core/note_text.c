#include "note_text.h"
#include "persisted_size_limit.h"

#include "text_buffer.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct note_text
{
    char *_Nonnull file_bytes;
    char *_Nonnull bytes;
    size_t length;
    size_t file_length;
};

static enum note_text_outcome validate(const char *_Nonnull bytes, size_t length)
{
    bool contains_nul = false;
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(bytes + index, length - index, &code_point);
        if (consumed == 0)
        {
            return NOTE_TEXT_INVALID_UTF8;
        }
        contains_nul = contains_nul || code_point == 0;
        index += consumed;
    }
    return contains_nul ? NOTE_TEXT_EMBEDDED_NUL : NOTE_TEXT_ACCEPTED;
}

/* 検証済みのバイト列を複製して所有する。 */
static enum note_text_outcome adopt(const char *_Nonnull bytes, size_t length,
                                    struct note_text *_Nullable *_Nonnull out)
{
    struct note_text *_Nullable text = malloc(sizeof *text);
    if (text == nullptr)
    {
        return NOTE_TEXT_OUT_OF_MEMORY;
    }
    size_t prefix = utf8_text_bom_length(bytes, length);
    text->file_length = prefix + length;
    text->file_bytes = malloc(text->file_length + 1);
    if (text->file_bytes == nullptr)
    {
        free(text);
        return NOTE_TEXT_OUT_OF_MEMORY;
    }
    memcpy(text->file_bytes, bytes, prefix);
    text->bytes = text->file_bytes + prefix;
    memcpy(text->bytes, bytes, length);
    text->bytes[length] = '\0';
    text->length = length;
    *out = text;
    return NOTE_TEXT_ACCEPTED;
}

enum note_text_outcome note_text_create(const char *_Nonnull bytes, size_t length,
                                        struct note_text *_Nullable *_Nonnull out)
{
    size_t signature = utf8_text_bom_length(bytes, length);
    bytes += signature;
    length -= signature;
    enum note_text_outcome accepted = validate(bytes, length);
    if (accepted != NOTE_TEXT_ACCEPTED)
    {
        return accepted;
    }
    if (length > persisted_size_limit - utf8_text_bom_length(bytes, length))
    {
        return NOTE_TEXT_TOO_LARGE;
    }
    return adopt(bytes, length, out);
}

enum note_text_outcome note_text_copy(const struct note_text *_Nonnull text,
                                      struct note_text *_Nullable *_Nonnull out)
{
    return adopt(text->bytes, text->length, out);
}

/* 畳んだあとの改行そのもの（終端付き）。 */
static const char *_Nonnull break_of(enum line_ending ending)
{
    switch (ending)
    {
    case LINE_ENDING_LF:
        return "\n";
    case LINE_ENDING_CRLF:
        return "\r\n";
    }
    return "\n";
}

/* 改行tokenの入力byte数。事前計数と書出しが同じCRLF判定を使う。 */
static size_t break_width(const char *_Nonnull bytes, size_t length, size_t index)
{
    if (bytes[index] != '\r' && bytes[index] != '\n')
    {
        return 0;
    }
    bool pair = bytes[index] == '\r' && index + 1 < length && bytes[index + 1] == '\n';
    return pair ? 2 : 1;
}

/* UTF-8検証後に呼ぶ。raw長では断らず、出力byte数を上限内で数える。 */
static bool normalized_within_limit(const char *_Nonnull bytes, size_t length,
                                    enum line_ending ending)
{
    size_t total = utf8_text_bom_length(bytes, length);
    size_t ending_width = strlen(break_of(ending));
    for (size_t index = 0; index < length;)
    {
        size_t input_width = break_width(bytes, length, index);
        size_t output_width = input_width == 0 ? 1 : ending_width;
        if (output_width > persisted_size_limit - total)
        {
            return false;
        }
        total += output_width;
        index += input_width == 0 ? 1 : input_width;
    }
    return true;
}

/* CR・CRLF・LF をすべて 1 つの改行と見なし、ending の形で書き直す。 */
static void fold(const char *_Nonnull bytes, size_t length, enum line_ending ending,
                 struct text_buffer *_Nonnull out)
{
    const char *_Nonnull line_break = break_of(ending);
    size_t start = 0;
    size_t index = 0;
    while (index < length)
    {
        size_t width = break_width(bytes, length, index);
        if (width == 0)
        {
            index += 1;
            continue;
        }
        text_buffer_append(out, bytes + start, index - start);
        text_buffer_append_text(out, line_break);
        index += width;
        start = index;
    }
    text_buffer_append(out, bytes + start, length - start);
}

enum note_text_outcome note_text_from_editor(const char *_Nonnull bytes, size_t length,
                                             enum line_ending ending,
                                             struct note_text *_Nullable *_Nonnull out)
{
    enum note_text_outcome accepted = validate(bytes, length);
    if (accepted != NOTE_TEXT_ACCEPTED)
    {
        return accepted;
    }
    if (!normalized_within_limit(bytes, length, ending))
    {
        return NOTE_TEXT_TOO_LARGE;
    }
    struct text_buffer *_Nullable folded = nullptr;
    if (text_buffer_create(&folded) != TEXT_BUFFER_ACCEPTED)
    {
        return NOTE_TEXT_OUT_OF_MEMORY;
    }
    fold(bytes, length, ending, folded);
    enum note_text_outcome outcome = NOTE_TEXT_OUT_OF_MEMORY;
    if (text_buffer_finish(folded) == TEXT_BUFFER_ACCEPTED)
    {
        outcome = adopt(text_buffer_bytes(folded), text_buffer_length(folded), out);
    }
    text_buffer_destroy(folded);
    return outcome;
}

enum line_ending note_text_line_ending(const struct note_text *_Nonnull text)
{
    for (size_t index = 0; index < text->length; ++index)
    {
        if (text->bytes[index] == '\r')
        {
            return LINE_ENDING_CRLF;
        }
        if (text->bytes[index] == '\n')
        {
            return LINE_ENDING_LF;
        }
    }
    return LINE_ENDING_LF;
}

bool note_text_equals(const struct note_text *_Nonnull text, const struct note_text *_Nonnull other)
{
    return text->length == other->length && memcmp(text->bytes, other->bytes, text->length) == 0;
}

/* RichEdit が流し込みで別の文字へ変える文字（2026-10-12 実測・Issue #242）。集合はここ 1 か所。 */
static constexpr uint32_t editor_altered[] = {0x0007, 0x2028, 0x2029};

static bool editor_alters(uint32_t code_point)
{
    for (size_t index = 0; index < sizeof editor_altered / sizeof editor_altered[0]; ++index)
    {
        if (editor_altered[index] == code_point)
        {
            return true;
        }
    }
    return false;
}

enum note_text_editability note_text_editability(const struct note_text *_Nonnull text)
{
    /* 本文は生成時に検証済みなので、decode が 0 を返すことは無い。 */
    for (size_t index = 0; index < text->length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(text->bytes + index, text->length - index, &code_point);
        if (editor_alters(code_point))
        {
            return NOTE_TEXT_EDITOR_ALTERS;
        }
        index += consumed;
    }
    return NOTE_TEXT_EDITABLE;
}

const char *_Nonnull note_text_bytes(const struct note_text *_Nonnull text)
{
    return text->bytes;
}

size_t note_text_length(const struct note_text *_Nonnull text)
{
    return text->length;
}

const char *_Nonnull note_text_file_bytes(const struct note_text *_Nonnull text)
{
    return text->file_bytes;
}

size_t note_text_file_length(const struct note_text *_Nonnull text)
{
    return text->file_length;
}

static bool blank(char value)
{
    return value == ' ' || value == '\t';
}

/* at から改行（CR / LF）の手前まで進んだ位置。 */
static size_t line_end(const struct note_text *_Nonnull text, size_t at)
{
    while (at < text->length && text->bytes[at] != '\r' && text->bytes[at] != '\n')
    {
        at += 1;
    }
    return at;
}

void note_text_first_line(const struct note_text *_Nonnull text, size_t *_Nonnull start,
                          size_t *_Nonnull length)
{
    /* CRLF は CR と LF の間に空の行があるものとして読む。空の行は飛ばすので結果は変わらない。 */
    for (size_t at = 0; at < text->length;)
    {
        size_t end = line_end(text, at);
        size_t first = at;
        while (first < end && blank(text->bytes[first]))
        {
            first += 1;
        }
        size_t last = end;
        while (last > first && blank(text->bytes[last - 1]))
        {
            last -= 1;
        }
        if (last > first)
        {
            *start = first;
            *length = last - first;
            return;
        }
        at = end + 1;
    }
    *start = text->length;
    *length = 0;
}

void note_text_destroy(struct note_text *_Nullable text)
{
    if (text == nullptr)
    {
        return;
    }
    free(text->file_bytes);
    free(text);
}
