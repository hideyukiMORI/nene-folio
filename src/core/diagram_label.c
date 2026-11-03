#include "diagram_label.h"
#include "markdown_character_reference.h"
#include "text_buffer.h"
#include "utf8_text.h"
#include <string.h>
static size_t break_length(struct diagram_text text)
{
    static const char *const breaks[] = {"<br/>", "<br />", "<br>", "\\n"};
    for (size_t index = 0; index < sizeof breaks / sizeof breaks[0]; ++index)
        if (diagram_text_starts(text, breaks[index]))
            return strlen(breaks[index]);
    return 0;
}
static size_t reference(struct diagram_text text, uint32_t *_Nonnull point)
{
    struct markdown_span span = {.bytes = text.bytes, .length = text.length};
    struct markdown_view view = {.spans = &span, .count = 1, .length = text.length};
    if (text.bytes[0] == '&')
        return markdown_character_reference_read(view, 0, point);
    if (text.bytes[0] != '#')
        return 0;
    char entity[32] = "&";
    size_t source = 1;
    size_t length = 1;
    if (source < text.length && text.bytes[source] >= '0' && text.bytes[source] <= '9')
        entity[length++] = '#';
    while (source < text.length && length < sizeof entity - 1)
    {
        char value = text.bytes[source++];
        entity[length++] = value;
        if (value == ';')
            break;
    }
    span.bytes = entity;
    span.length = view.length = length;
    return markdown_character_reference_read(view, 0, point) == length ? source : 0;
}
static bool html_tag(struct diagram_text text)
{
    if (text.length < 2 || text.bytes[0] != '<')
        return false;
    char next = text.bytes[1];
    return (next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '/' ||
           next == '!';
}
static size_t append_escape(struct text_buffer *_Nonnull buffer, struct diagram_text text)
{
    size_t line = break_length(text);
    if (line != 0)
    {
        text_buffer_append_text(buffer, "\n");
        return line;
    }
    uint32_t point = 0;
    size_t entity = reference(text, &point);
    if (entity != 0)
    {
        char encoded[utf8_text_max_encoded];
        size_t count = utf8_text_encode(point, encoded);
        text_buffer_append(buffer, encoded, count);
        return entity;
    }
    if (text.length >= 2 && text.bytes[0] == '\\' &&
        (text.bytes[1] == '"' || text.bytes[1] == '\\'))
    {
        text_buffer_append(buffer, text.bytes + 1, 1);
        return 2;
    }
    return 0;
}
static enum diagram_outcome append_label(struct text_buffer *_Nonnull buffer,
                                         struct diagram_text text)
{
    size_t cursor = 0;
    while (cursor < text.length)
    {
        struct diagram_text rest = diagram_text_skip(text, cursor);
        size_t escaped = append_escape(buffer, rest);
        if (escaped != 0)
        {
            cursor += escaped;
            continue;
        }
        if (html_tag(rest) || rest.bytes[0] == '`')
            return DIAGRAM_UNSUPPORTED;
        text_buffer_append(buffer, rest.bytes, 1);
        ++cursor;
    }
    return DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_label_store(struct diagram_model *_Nonnull model,
                                         struct diagram_text text, size_t *_Nonnull index)
{
    text = diagram_text_trim(text);
    if (text.length >= 2 && text.bytes[0] == '"' && text.bytes[text.length - 1] == '"')
    {
        ++text.bytes;
        text.length -= 2;
    }
    if (text.length > diagram_label_limit)
        return DIAGRAM_TOO_LARGE;
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return DIAGRAM_OUT_OF_MEMORY;
    enum diagram_outcome outcome = append_label(buffer, text);
    if (outcome == DIAGRAM_ACCEPTED && text_buffer_finish(buffer) != TEXT_BUFFER_ACCEPTED)
        outcome = DIAGRAM_OUT_OF_MEMORY;
    if (outcome == DIAGRAM_ACCEPTED)
        outcome =
            diagram_model_text(model, text_buffer_bytes(buffer), text_buffer_length(buffer), index);
    text_buffer_destroy(buffer);
    return outcome;
}
