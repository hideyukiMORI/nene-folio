#include "text_buffer.h"

#include <stdlib.h>
#include <string.h>

constexpr size_t buffer_initial = 256;

struct text_buffer
{
    char *_Nonnull bytes; /* 終端付き */
    size_t capacity;
    size_t length;
    bool exhausted; /* 記憶不足が起きた */
};

enum text_buffer_outcome text_buffer_create(struct text_buffer *_Nullable *_Nonnull out)
{
    struct text_buffer *_Nullable buffer = calloc(1, sizeof *buffer);
    if (buffer == nullptr)
    {
        return TEXT_BUFFER_OUT_OF_MEMORY;
    }
    buffer->bytes = malloc(buffer_initial);
    if (buffer->bytes == nullptr)
    {
        free(buffer);
        return TEXT_BUFFER_OUT_OF_MEMORY;
    }
    buffer->bytes[0] = '\0';
    buffer->capacity = buffer_initial;
    *out = buffer;
    return TEXT_BUFFER_ACCEPTED;
}

void text_buffer_append(struct text_buffer *_Nonnull buffer, const char *_Nonnull bytes,
                        size_t count)
{
    if (buffer->exhausted)
    {
        return;
    }
    if (buffer->length + count + 1 > buffer->capacity)
    {
        size_t capacity = buffer->capacity * 2 + count;
        char *_Nullable grown = realloc(buffer->bytes, capacity);
        if (grown == nullptr)
        {
            buffer->exhausted = true;
            return;
        }
        buffer->bytes = grown;
        buffer->capacity = capacity;
    }
    memcpy(buffer->bytes + buffer->length, bytes, count);
    buffer->length += count;
    buffer->bytes[buffer->length] = '\0';
}

void text_buffer_append_text(struct text_buffer *_Nonnull buffer, const char *_Nonnull text)
{
    text_buffer_append(buffer, text, strlen(text));
}

enum text_buffer_outcome text_buffer_finish(const struct text_buffer *_Nonnull buffer)
{
    return buffer->exhausted ? TEXT_BUFFER_OUT_OF_MEMORY : TEXT_BUFFER_ACCEPTED;
}

const char *_Nonnull text_buffer_bytes(const struct text_buffer *_Nonnull buffer)
{
    return buffer->bytes;
}

size_t text_buffer_length(const struct text_buffer *_Nonnull buffer)
{
    return buffer->length;
}

void text_buffer_destroy(struct text_buffer *_Nullable buffer)
{
    if (buffer == nullptr)
    {
        return;
    }
    free(buffer->bytes);
    free(buffer);
}
