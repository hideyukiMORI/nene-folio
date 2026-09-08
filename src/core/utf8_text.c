#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct utf8_text
{
    char *_Nonnull bytes;
    size_t length;
};

constexpr uint32_t surrogate_low = 0xD800;
constexpr uint32_t surrogate_high_end = 0xDC00;
constexpr uint32_t surrogate_end = 0xE000;
constexpr uint32_t code_point_max = 0x10FFFF;
constexpr uint32_t supplementary_base = 0x10000;

static size_t sequence_length(unsigned char lead)
{
    if (lead < 0x80)
    {
        return 1;
    }
    if (lead >= 0xC2 && lead < 0xE0)
    {
        return 2;
    }
    if (lead >= 0xE0 && lead < 0xF0)
    {
        return 3;
    }
    if (lead >= 0xF0 && lead < 0xF5)
    {
        return 4;
    }
    return 0;
}

static bool continuation(unsigned char byte)
{
    return (byte & 0xC0) == 0x80;
}

static uint32_t assemble(const unsigned char *_Nonnull bytes, size_t count)
{
    static const unsigned char lead_masks[] = {0, 0x7F, 0x1F, 0x0F, 0x07};
    uint32_t value = bytes[0] & lead_masks[count];
    for (size_t index = 1; index < count; ++index)
    {
        value = (value << 6) | (bytes[index] & 0x3Fu);
    }
    return value;
}

static bool well_formed(uint32_t value, size_t count)
{
    static const uint32_t minimums[] = {0, 0, 0x80, 0x800, 0x10000};
    if (value < minimums[count] || value > code_point_max)
    {
        return false;
    }
    return value < surrogate_low || value >= surrogate_end;
}

size_t utf8_text_decode(const char *_Nonnull bytes, size_t length, uint32_t *_Nonnull code_point)
{
    const unsigned char *_Nonnull raw = (const unsigned char *)bytes;
    size_t count = length == 0 ? 0 : sequence_length(raw[0]);
    if (count == 0 || count > length)
    {
        return 0;
    }
    for (size_t index = 1; index < count; ++index)
    {
        if (!continuation(raw[index]))
        {
            return 0;
        }
    }
    uint32_t value = assemble(raw, count);
    if (!well_formed(value, count))
    {
        return 0;
    }
    *code_point = value;
    return count;
}

size_t utf8_text_encode(uint32_t code_point, char *_Nonnull out)
{
    unsigned char *_Nonnull raw = (unsigned char *)out;
    if (code_point < 0x80)
    {
        raw[0] = (unsigned char)code_point;
        return 1;
    }
    if (code_point < 0x800)
    {
        raw[0] = (unsigned char)(0xC0 | (code_point >> 6));
        raw[1] = (unsigned char)(0x80 | (code_point & 0x3F));
        return 2;
    }
    if (code_point < supplementary_base)
    {
        if (code_point >= surrogate_low && code_point < surrogate_end)
        {
            return 0;
        }
        raw[0] = (unsigned char)(0xE0 | (code_point >> 12));
        raw[1] = (unsigned char)(0x80 | ((code_point >> 6) & 0x3F));
        raw[2] = (unsigned char)(0x80 | (code_point & 0x3F));
        return 3;
    }
    if (code_point > code_point_max)
    {
        return 0;
    }
    raw[0] = (unsigned char)(0xF0 | (code_point >> 18));
    raw[1] = (unsigned char)(0x80 | ((code_point >> 12) & 0x3F));
    raw[2] = (unsigned char)(0x80 | ((code_point >> 6) & 0x3F));
    raw[3] = (unsigned char)(0x80 | (code_point & 0x3F));
    return 4;
}

/* 1 単位または 2 単位（サロゲート対）を読み、消費した単位数を返す。不正なら 0。 */
static size_t decode_utf16(const char16_t *_Nonnull units, size_t length,
                           uint32_t *_Nonnull code_point)
{
    uint32_t first = units[0];
    if (first < surrogate_low || first >= surrogate_end)
    {
        *code_point = first;
        return 1;
    }
    if (first >= surrogate_high_end || length < 2)
    {
        return 0;
    }
    uint32_t second = units[1];
    if (second < surrogate_high_end || second >= surrogate_end)
    {
        return 0;
    }
    *code_point =
        supplementary_base + ((first - surrogate_low) << 10) + (second - surrogate_high_end);
    return 2;
}

enum utf8_text_outcome utf8_text_create(const char16_t *_Nonnull units, size_t length,
                                        struct utf8_text *_Nullable *_Nonnull out)
{
    /* 1 単位は最大 3 バイト、サロゲート対（2 単位）は 4 バイトなので 3 倍で足りる。 */
    char *_Nullable bytes = malloc(length * 3 + 1);
    if (bytes == nullptr)
    {
        return UTF8_TEXT_OUT_OF_MEMORY;
    }
    size_t written = 0;
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = decode_utf16(units + index, length - index, &code_point);
        if (consumed == 0)
        {
            free(bytes);
            return UTF8_TEXT_INVALID_UTF16;
        }
        index += consumed;
        written += utf8_text_encode(code_point, bytes + written);
    }
    bytes[written] = '\0';
    struct utf8_text *_Nullable text = malloc(sizeof *text);
    if (text == nullptr)
    {
        free(bytes);
        return UTF8_TEXT_OUT_OF_MEMORY;
    }
    text->bytes = bytes;
    text->length = written;
    *out = text;
    return UTF8_TEXT_CONVERTED;
}

const char *_Nonnull utf8_text_bytes(const struct utf8_text *_Nonnull text)
{
    return text->bytes;
}

size_t utf8_text_length(const struct utf8_text *_Nonnull text)
{
    return text->length;
}

void utf8_text_destroy(struct utf8_text *_Nullable text)
{
    if (text == nullptr)
    {
        return;
    }
    free(text->bytes);
    free(text);
}
