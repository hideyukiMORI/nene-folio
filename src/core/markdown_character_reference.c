#include "markdown_character_reference.h"
#include <string.h>

static unsigned digit(char value)
{
    if (value >= '0' && value <= '9')
        return (unsigned)(value - '0');
    if (value >= 'A' && value <= 'F')
        return (unsigned)(value - 'A') + 10;
    if (value >= 'a' && value <= 'f')
        return (unsigned)(value - 'a') + 10;
    return 16;
}

static size_t digits(struct markdown_view view, size_t first, unsigned base,
                     uint32_t *_Nonnull value)
{
    size_t maximum = base == 16 ? 6 : 7;
    size_t count = 0;
    while (count < maximum && first + count < view.length)
    {
        unsigned next = digit(markdown_view_at(view, first + count));
        if (next >= base)
            break;
        *value = *value * base + next;
        ++count;
    }
    return count;
}

static uint32_t scalar(uint32_t value)
{
    if (value == 0 || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF))
        return 0xFFFD;
    return value;
}

static size_t numeric(struct markdown_view view, size_t start, uint32_t *_Nonnull code_point)
{
    size_t first = start + 2;
    char prefix = markdown_view_at(view, first);
    bool hex = prefix == 'x' || prefix == 'X';
    first += hex ? 1 : 0;
    uint32_t value = 0;
    size_t count = digits(view, first, hex ? 16 : 10, &value);
    if (count == 0 || markdown_view_at(view, first + count) != ';')
        return 0;
    *code_point = scalar(value);
    return first + count + 1 - start;
}

static size_t named(struct markdown_view view, size_t start, uint32_t *_Nonnull code_point)
{
    static const struct
    {
        const char *_Nonnull name;
        uint32_t code_point;
    } references[] = {{"amp;", '&'},  {"lt;", '<'},    {"gt;", '>'},
                      {"quot;", '"'}, {"apos;", '\''}, {"nbsp;", 0xA0}};
    for (size_t index = 0; index < sizeof references / sizeof references[0]; ++index)
    {
        size_t length = strlen(references[index].name);
        if (length >= view.length - start)
            continue;
        size_t matched = 0;
        while (matched < length &&
               markdown_view_at(view, start + matched + 1) == references[index].name[matched])
            ++matched;
        if (matched == length)
        {
            *code_point = references[index].code_point;
            return length + 1;
        }
    }
    return 0;
}

size_t markdown_character_reference_read(struct markdown_view view, size_t start,
                                         uint32_t *_Nonnull code_point)
{
    if (start >= view.length || markdown_view_at(view, start) != '&')
        return 0;
    if (view.length - start < 3)
        return 0;
    return markdown_view_at(view, start + 1) == '#' ? numeric(view, start, code_point)
                                                    : named(view, start, code_point);
}
