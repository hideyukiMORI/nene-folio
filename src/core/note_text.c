#include "note_text.h"

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

enum note_text_outcome note_text_create(const char *_Nonnull bytes, size_t length,
                                        struct note_text *_Nullable *_Nonnull out)
{
    if (length >= 3 && memcmp(bytes, "\xEF\xBB\xBF", 3) == 0)
    {
        bytes += 3;
        length -= 3;
    }
    if (!well_formed(bytes, length))
    {
        return NOTE_TEXT_INVALID_UTF8;
    }
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
