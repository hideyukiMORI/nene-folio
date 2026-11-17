#include "markdown_image_copy.h"
#include "note_text.h"
#include "text_buffer.h"
#include <stdlib.h>
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

static bool before(struct note_search_span left, struct note_search_span right)
{
    return left.start < right.start || (left.start == right.start && left.end < right.end);
}

/* #863: reference images share their definition's span and are collected after inline images
 * that follow them in the source. Order the relocations by span (insertion: the collector's
 * order is nearly sorted) and keep one per span; the same span sent to two places is refused. */
static void sort_by_span(struct image_relocation *_Nonnull relocations, size_t count)
{
    for (size_t index = 1; index < count; ++index)
    {
        struct image_relocation moving = relocations[index];
        size_t at = index;
        for (; at != 0 && before(moving.source, relocations[at - 1].source); --at)
            relocations[at] = relocations[at - 1];
        relocations[at] = moving;
    }
}

static bool ordered(struct image_relocation *_Nonnull relocations, size_t *_Nonnull count)
{
    sort_by_span(relocations, *count);
    size_t kept = 0;
    for (size_t index = 0; index < *count; ++index)
    {
        struct image_relocation *last = kept == 0 ? nullptr : &relocations[kept - 1];
        bool repeated = last != nullptr && last->source.start == relocations[index].source.start &&
                        last->source.end == relocations[index].source.end;
        if (repeated && strcmp(last->destination, relocations[index].destination) != 0)
            return false;
        if (!repeated)
            relocations[kept++] = relocations[index];
    }
    *count = kept;
    return true;
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

static enum note_text_outcome copy_ordered(const struct note_text *_Nonnull source,
                                           const struct image_relocation *_Nonnull relocations,
                                           size_t count, struct note_text *_Nullable *_Nonnull out)
{
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

enum note_text_outcome
markdown_image_copy_create(const struct note_text *_Nonnull source,
                           const struct image_relocation *_Nonnull relocations, size_t count,
                           struct note_text *_Nullable *_Nonnull out)
{
    struct image_relocation *_Nullable sorted = malloc((count == 0 ? 1 : count) * sizeof *sorted);
    if (sorted == nullptr)
        return NOTE_TEXT_OUT_OF_MEMORY;
    if (count != 0)
        memcpy(sorted, relocations, count * sizeof *sorted);
    enum note_text_outcome result = NOTE_TEXT_TOO_LARGE;
    if (ordered(sorted, &count) && valid(source, sorted, count))
        result = copy_ordered(source, sorted, count, out);
    free(sorted);
    return result;
}
