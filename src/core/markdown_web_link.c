#include "markdown_web_link.h"
#include "markdown_character_reference.h"
#include "text_buffer.h"
#include "utf8_text.h"

static bool punctuation(char character)
{
    return (character > ' ' && character < '0') || (character > '9' && character < 'A') ||
           (character > 'Z' && character < 'a') || (character > 'z' && character < 0x7F);
}

static size_t append_decoded(struct text_buffer *_Nonnull buffer, struct markdown_view view,
                             size_t start, size_t end)
{
    char character = markdown_view_at(view, start);
    if (character == '\\' && start + 1 < end && punctuation(markdown_view_at(view, start + 1)))
    {
        character = markdown_view_at(view, start + 1);
        text_buffer_append(buffer, &character, 1);
        return 2;
    }
    uint32_t point = 0;
    size_t length = character == '&' ? markdown_character_reference_read(view, start, &point) : 0;
    if (length != 0 && length <= end - start)
    {
        char encoded[utf8_text_max_encoded];
        size_t count = utf8_text_encode(point, encoded);
        text_buffer_append(buffer, encoded, count);
        return length;
    }
    text_buffer_append(buffer, &character, 1);
    return 1;
}

static enum web_uri_outcome finish(struct text_buffer *_Nonnull buffer,
                                   struct web_uri *_Nullable *_Nonnull out)
{
    enum web_uri_outcome outcome = WEB_URI_OUT_OF_MEMORY;
    if (text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED)
        outcome = web_uri_create(text_buffer_bytes(buffer), text_buffer_length(buffer), out);
    text_buffer_destroy(buffer);
    return outcome;
}

enum web_uri_outcome markdown_web_link_destination(struct markdown_view view, size_t start,
                                                   size_t end,
                                                   struct web_uri *_Nullable *_Nonnull out)
{
    if (start >= end || end > view.length)
        return WEB_URI_REJECTED;
    if (markdown_view_at(view, start) == '<' && markdown_view_at(view, end - 1) == '>')
    {
        start += 1;
        end -= 1;
    }
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return WEB_URI_OUT_OF_MEMORY;
    while (start < end)
        start += append_decoded(buffer, view, start, end);
    return finish(buffer, out);
}

enum web_uri_outcome markdown_web_link_autolink(struct markdown_view view, size_t start, size_t end,
                                                struct web_uri *_Nullable *_Nonnull out)
{
    if (start >= end || end > view.length)
        return WEB_URI_REJECTED;
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return WEB_URI_OUT_OF_MEMORY;
    for (size_t index = start; index < end; ++index)
    {
        char character = markdown_view_at(view, index);
        text_buffer_append(buffer, &character, 1);
    }
    return finish(buffer, out);
}
