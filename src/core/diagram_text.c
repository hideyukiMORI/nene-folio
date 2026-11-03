#include "diagram_text.h"
#include <string.h>
static bool whitespace(char value)
{
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}
struct diagram_text diagram_text_trim(struct diagram_text text)
{
    while (text.length != 0 && whitespace(text.bytes[0]))
    {
        ++text.bytes;
        --text.length;
    }
    while (text.length != 0 && whitespace(text.bytes[text.length - 1]))
        --text.length;
    return text;
}
struct diagram_text diagram_text_skip(struct diagram_text text, size_t count)
{
    if (count > text.length)
        count = text.length;
    return (struct diagram_text){text.bytes + count, text.length - count};
}
bool diagram_text_equal(struct diagram_text text, const char *_Nonnull word)
{
    size_t length = strlen(word);
    return text.length == length && memcmp(text.bytes, word, length) == 0;
}
bool diagram_text_starts(struct diagram_text text, const char *_Nonnull word)
{
    size_t length = strlen(word);
    return text.length >= length && memcmp(text.bytes, word, length) == 0;
}
bool diagram_text_character_in(char value, const char *_Nonnull characters)
{
    for (size_t index = 0; characters[index] != '\0'; ++index)
        if (characters[index] == value)
            return true;
    return false;
}
struct diagram_text diagram_text_word(struct diagram_text *_Nonnull text)
{
    *text = diagram_text_trim(*text);
    size_t count = 0;
    while (count < text->length && !whitespace(text->bytes[count]))
        ++count;
    struct diagram_text word = {text->bytes, count};
    *text = diagram_text_trim(diagram_text_skip(*text, count));
    return word;
}
static bool identifier_end(struct diagram_text text, size_t at)
{
    char value = text.bytes[at];
    if (whitespace(value) || diagram_text_character_in(value, "()[]{}<>|&:;,=~@\""))
        return true;
    if (value == '-' && at + 1 < text.length)
        return diagram_text_character_in(text.bytes[at + 1], "->.x)");
    if (value == '.' && at + 1 < text.length)
        return text.bytes[at + 1] == '-';
    return false;
}
struct diagram_text diagram_text_identifier(struct diagram_text *_Nonnull text)
{
    *text = diagram_text_trim(*text);
    size_t count = 0;
    while (count < text->length && !identifier_end(*text, count))
        ++count;
    struct diagram_text word = {text->bytes, count};
    *text = diagram_text_trim(diagram_text_skip(*text, count));
    return word;
}
