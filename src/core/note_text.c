#include "note_text.h"

#include "text_buffer.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct note_text
{
    char *_Nonnull bytes;
    size_t length;
};

static bool well_formed(const char *_Nonnull bytes, size_t length)
{
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(bytes + index, length - index, &code_point);
        if (consumed == 0)
        {
            return false;
        }
        index += consumed;
    }
    return true;
}

/* 先頭の BOM は本文ではない（メモ帳が付ける）。書き戻すときも付けない（ADR 0006）。 */
static void drop_bom(const char *_Nonnull *_Nonnull bytes, size_t *_Nonnull length)
{
    if (*length >= 3 && memcmp(*bytes, "\xEF\xBB\xBF", 3) == 0)
    {
        *bytes += 3;
        *length -= 3;
    }
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
    text->bytes = malloc(length + 1);
    if (text->bytes == nullptr)
    {
        free(text);
        return NOTE_TEXT_OUT_OF_MEMORY;
    }
    memcpy(text->bytes, bytes, length);
    text->bytes[length] = '\0';
    text->length = length;
    *out = text;
    return NOTE_TEXT_ACCEPTED;
}

enum note_text_outcome note_text_create(const char *_Nonnull bytes, size_t length,
                                        struct note_text *_Nullable *_Nonnull out)
{
    drop_bom(&bytes, &length);
    if (!well_formed(bytes, length))
    {
        return NOTE_TEXT_INVALID_UTF8;
    }
    return adopt(bytes, length, out);
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

/* CR・CRLF・LF をすべて 1 つの改行と見なし、ending の形で書き直す。 */
static void fold(const char *_Nonnull bytes, size_t length, enum line_ending ending,
                 struct text_buffer *_Nonnull out)
{
    const char *_Nonnull line_break = break_of(ending);
    size_t start = 0;
    size_t index = 0;
    while (index < length)
    {
        if (bytes[index] != '\r' && bytes[index] != '\n')
        {
            index += 1;
            continue;
        }
        text_buffer_append(out, bytes + start, index - start);
        text_buffer_append_text(out, line_break);
        bool pair = bytes[index] == '\r' && index + 1 < length && bytes[index + 1] == '\n';
        index += pair ? 2 : 1;
        start = index;
    }
    text_buffer_append(out, bytes + start, length - start);
}

enum note_text_outcome note_text_from_editor(const char *_Nonnull bytes, size_t length,
                                             enum line_ending ending,
                                             struct note_text *_Nullable *_Nonnull out)
{
    drop_bom(&bytes, &length);
    if (!well_formed(bytes, length))
    {
        return NOTE_TEXT_INVALID_UTF8;
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

const char *_Nonnull note_text_bytes(const struct note_text *_Nonnull text)
{
    return text->bytes;
}

size_t note_text_length(const struct note_text *_Nonnull text)
{
    return text->length;
}

void note_text_destroy(struct note_text *_Nullable text)
{
    if (text == nullptr)
    {
        return;
    }
    free(text->bytes);
    free(text);
}
