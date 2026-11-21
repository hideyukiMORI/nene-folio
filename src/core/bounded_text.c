#include "bounded_text.h"
#include <string.h>

/* The length of out's text, never past the last byte the buffer has. */
static size_t held(const char *_Nonnull out, size_t capacity)
{
    size_t length = 0;
    while (length + 1 < capacity && out[length] != '\0')
        length += 1;
    return length;
}

size_t bounded_text_append(char *_Nonnull out, size_t capacity, const char *_Nonnull text)
{
    size_t at = held(out, capacity);
    size_t room = capacity - 1 - at;
    size_t length = strlen(text);
    if (length > room)
    {
        length = room;
        /* Not in the middle of a character: step back over its continuation bytes. */
        while (length > 0 && ((unsigned char)text[length] & 0xC0U) == 0x80U)
            length -= 1;
    }
    memcpy(out + at, text, length);
    out[at + length] = '\0';
    return at + length;
}

size_t bounded_text_copy(char *_Nonnull out, size_t capacity, const char *_Nonnull text)
{
    out[0] = '\0';
    return bounded_text_append(out, capacity, text);
}
