#include "note_tag.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct note_tag
{
    size_t length;
    char bytes[note_tag_max_bytes + 1];
};

static bool blank(char byte)
{
    return byte == ' ' || byte == '\t';
}

static bool forbidden(uint32_t point)
{
    return point < 0x20 || (point >= 0x7f && point <= 0x9f) || point == 0x2028 || point == 0x2029 ||
           point == 0xfeff;
}

static enum note_tag_outcome validate(const char *_Nonnull bytes, size_t length)
{
    if (length == 0)
        return NOTE_TAG_EMPTY;
    if (length > note_tag_max_bytes)
        return NOTE_TAG_TOO_LONG;
    size_t points = 0;
    for (size_t at = 0; at < length;)
    {
        uint32_t point = 0;
        size_t step = utf8_text_decode(bytes + at, length - at, &point);
        if (step == 0 || forbidden(point))
            return NOTE_TAG_INVALID;
        if (++points > note_tag_max_points)
            return NOTE_TAG_TOO_LONG;
        at += step;
    }
    return NOTE_TAG_READY;
}

enum note_tag_outcome note_tag_create(const char *_Nonnull bytes, size_t length,
                                      struct note_tag *_Nullable *_Nonnull out)
{
    while (length > 0 && blank(bytes[0]))
    {
        ++bytes;
        --length;
    }
    while (length > 0 && blank(bytes[length - 1]))
        --length;
    enum note_tag_outcome checked = validate(bytes, length);
    if (checked != NOTE_TAG_READY)
        return checked;
    struct note_tag *_Nullable tag = malloc(sizeof *tag);
    if (tag == nullptr)
        return NOTE_TAG_OUT_OF_MEMORY;
    memcpy(tag->bytes, bytes, length);
    tag->bytes[length] = '\0';
    tag->length = length;
    *out = tag;
    return NOTE_TAG_READY;
}

const char *_Nonnull note_tag_bytes(const struct note_tag *_Nonnull tag)
{
    return tag->bytes;
}

size_t note_tag_length(const struct note_tag *_Nonnull tag)
{
    return tag->length;
}

bool note_tag_equal(const struct note_tag *_Nonnull left, const struct note_tag *_Nonnull right)
{
    return left->length == right->length && memcmp(left->bytes, right->bytes, left->length) == 0;
}

void note_tag_destroy(struct note_tag *_Nullable tag)
{
    free(tag);
}
