#include "markdown_web_link.h"
#include "markdown_destination.h"
#include "text_buffer.h"
#include "utf8_text.h"

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
    struct markdown_destination *_Nullable value = nullptr;
    enum image_outcome read =
        markdown_destination_create(view, (struct note_search_span){start, end}, &value);
    if (read == IMAGE_OUT_OF_MEMORY)
        return WEB_URI_OUT_OF_MEMORY;
    if (read != IMAGE_READY)
        return WEB_URI_REJECTED;
    enum web_uri_outcome result =
        web_uri_create(markdown_destination_bytes(value), markdown_destination_length(value), out);
    markdown_destination_destroy(value);
    return result;
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
