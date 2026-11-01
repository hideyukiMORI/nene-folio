#include "rtf_text.h"
#include "text_buffer.h"
void rtf_text_append(struct text_buffer *_Nonnull out, const char *_Nonnull text, size_t length)
{
    size_t start = 0;
    for (size_t index = 0; index < length; ++index)
    {
        char character = text[index];
        if (character != '\r' && character != '\\' && character != '{' && character != '}')
            continue;
        if (index > start)
            text_buffer_append(out, text + start, index - start);
        if (character != '\r')
        {
            text_buffer_append_text(out, "\\");
            text_buffer_append(out, text + index, 1);
        }
        start = index + 1;
    }
    if (start < length)
        text_buffer_append(out, text + start, length - start);
}
void rtf_text_number(struct text_buffer *_Nonnull out, size_t value)
{
    char digits[3 * sizeof value];
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count != 0)
        text_buffer_append(out, &digits[--count], 1);
}
