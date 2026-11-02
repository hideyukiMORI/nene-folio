#include "markdown_image_copy.h"
#include "note_text.h"
#include "text_buffer.h"
#include <string.h>
static bool encode_byte(unsigned char byte)
{
    if (byte <= 32)
        return true;
    static const char reserved[] = "%#?()[]<>&\"'\\";
    for (size_t index = 0; reserved[index] != '\0'; ++index)
        if (byte == (unsigned char)reserved[index])
            return true;
    return false;
}
static void append_destination(struct text_buffer *_Nonnull buffer,
                               const char *_Nonnull destination)
{
    static const char encoded[] = "0123456789ABCDEF";
    for (size_t index = 0; destination[index] != '\0'; ++index)
    {
        unsigned char byte = (unsigned char)destination[index];
        if (encode_byte(byte))
        {
            char escaped[] = {'%', encoded[byte >> 4], encoded[byte & 15]};
            text_buffer_append(buffer, escaped, sizeof escaped);
        }
        else
            text_buffer_append(buffer, destination + index, 1);
    }
}
static bool valid(const struct note_text *_Nonnull source,
                  const struct image_relocation *_Nonnull relocations, size_t count)
{
    size_t consumed = 0;
    for (size_t index = 0; index < count; ++index)
    {
        struct note_search_span span = relocations[index].source;
        if (span.start < consumed || span.start >= span.end || span.end > note_text_length(source))
            return false;
        consumed = span.end;
    }
    return true;
}
enum note_text_outcome
markdown_image_copy_create(const struct note_text *_Nonnull source,
                           const struct image_relocation *_Nonnull relocations, size_t count,
                           struct note_text *_Nullable *_Nonnull out)
{
    if (!valid(source, relocations, count))
        return NOTE_TEXT_TOO_LARGE;
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return NOTE_TEXT_OUT_OF_MEMORY;
    size_t prefix = note_text_file_length(source) - note_text_length(source);
    text_buffer_append(buffer, note_text_file_bytes(source), prefix);
    const char *bytes = note_text_bytes(source);
    size_t consumed = 0;
    for (size_t index = 0; index < count; ++index)
    {
        struct note_search_span span = relocations[index].source;
        text_buffer_append(buffer, bytes + consumed, span.start - consumed);
        append_destination(buffer, relocations[index].destination);
        consumed = span.end;
    }
    text_buffer_append(buffer, bytes + consumed, note_text_length(source) - consumed);
    enum note_text_outcome result = NOTE_TEXT_OUT_OF_MEMORY;
    if (text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED)
        result = note_text_create(text_buffer_bytes(buffer), text_buffer_length(buffer), out);
    text_buffer_destroy(buffer);
    return result;
}
