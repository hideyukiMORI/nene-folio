#include "utf16_text.h"

#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct utf16_text
{
    char16_t *_Nonnull units;
    size_t length;
};

constexpr uint32_t supplementary_base = 0x10000;

static size_t encode_utf16(uint32_t code_point, char16_t *_Nonnull out)
{
    if (code_point < supplementary_base)
    {
        out[0] = (char16_t)code_point;
        return 1;
    }
    uint32_t offset = code_point - supplementary_base;
    out[0] = (char16_t)(0xD800 + (offset >> 10));
    out[1] = (char16_t)(0xDC00 + (offset & 0x3FF));
    return 2;
}

enum utf16_text_outcome utf16_text_create(const char *_Nonnull bytes, size_t length,
                                          struct utf16_text *_Nullable *_Nonnull out)
{
    /* 1 バイトは最大 1 単位、4 バイト列だけ 2 単位なので、バイト数ぶんの単位で足りる。 */
    char16_t *_Nullable units = malloc((length + 1) * sizeof *units);
    if (units == nullptr)
    {
        return UTF16_TEXT_OUT_OF_MEMORY;
    }
    size_t written = 0;
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(bytes + index, length - index, &code_point);
        if (consumed == 0)
        {
            free(units);
            return UTF16_TEXT_INVALID_UTF8;
        }
        index += consumed;
        written += encode_utf16(code_point, units + written);
    }
    units[written] = u'\0';
    struct utf16_text *_Nullable text = malloc(sizeof *text);
    if (text == nullptr)
    {
        free(units);
        return UTF16_TEXT_OUT_OF_MEMORY;
    }
    text->units = units;
    text->length = written;
    *out = text;
    return UTF16_TEXT_CONVERTED;
}

const char16_t *_Nonnull utf16_text_units(const struct utf16_text *_Nonnull text)
{
    return text->units;
}

size_t utf16_text_length(const struct utf16_text *_Nonnull text)
{
    return text->length;
}

void utf16_text_destroy(struct utf16_text *_Nullable text)
{
    if (text == nullptr)
    {
        return;
    }
    free(text->units);
    free(text);
}

/* 確保しない写し（決定 5）。まず長さを数え、収まると分かってから書く。
 * 表の文言は単体が ui_text_unit_limit に収まることを固定するので、TOO_LONG はノート名
 * のような外から来る文字列でしか起きない。 */
enum utf16_text_fill_outcome utf16_text_fill(const char *_Nonnull utf8, char16_t *_Nonnull out,
                                             size_t capacity, size_t *_Nonnull written)
{
    size_t length = strlen(utf8);
    size_t units = 0;
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(utf8 + index, length - index, &code_point);
        if (consumed == 0)
        {
            return UTF16_TEXT_FILL_MALFORMED;
        }
        index += consumed;
        units += code_point < supplementary_base ? 1 : 2;
    }
    if (units + 1 > capacity)
    {
        return UTF16_TEXT_FILL_TOO_LONG;
    }
    size_t filled = 0;
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        index += utf8_text_decode(utf8 + index, length - index, &code_point);
        filled += encode_utf16(code_point, out + filled);
    }
    out[filled] = u'\0';
    *written = filled;
    return UTF16_TEXT_FILL_READY;
}
