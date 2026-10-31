#include "markdown_autolink.h"

static bool letter(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static bool alphanumeric(char value)
{
    return letter(value) || (value >= '0' && value <= '9');
}

static bool uri(struct markdown_view view, size_t start, size_t end)
{
    if (!letter(markdown_view_at(view, start)))
        return false;
    size_t index = start + 1;
    while (index < end && index - start <= 32)
    {
        char value = markdown_view_at(view, index);
        if (value == ':')
            return index - start >= 2;
        if (!alphanumeric(value) && value != '+' && value != '.' && value != '-')
            return false;
        ++index;
    }
    return false;
}

static bool local_character(char value)
{
    if (alphanumeric(value))
        return true;
    static const char punctuation[] = ".!#$%&'*+/=?^_`{|}~-";
    for (size_t index = 0; index < sizeof punctuation - 1; ++index)
        if (punctuation[index] == value)
            return true;
    return false;
}

static bool label_ends(struct markdown_view view, size_t start, size_t end)
{
    return end > start && alphanumeric(markdown_view_at(view, end - 1));
}

static bool label_character(char value, size_t offset)
{
    return offset < 63 && (alphanumeric(value) || (value == '-' && offset != 0));
}

static bool domain(struct markdown_view view, size_t start, size_t end)
{
    size_t label = start;
    for (size_t index = start; index < end; ++index)
    {
        char value = markdown_view_at(view, index);
        if (value == '.')
        {
            if (!label_ends(view, label, index))
                return false;
            label = index + 1;
            continue;
        }
        if (!label_character(value, index - label))
            return false;
    }
    return label_ends(view, label, end);
}

static bool email(struct markdown_view view, size_t start, size_t end)
{
    size_t index = start;
    while (index < end && local_character(markdown_view_at(view, index)))
        ++index;
    return index > start && markdown_view_at(view, index) == '@' && domain(view, index + 1, end);
}

static size_t closing_bracket(struct markdown_view view, size_t start)
{
    size_t end = start + 1;
    while (end < view.length && markdown_view_at(view, end) != '>')
    {
        unsigned char value = (unsigned char)markdown_view_at(view, end);
        if (value <= ' ' || value == 0x7F || value == '<')
            return 0;
        ++end;
    }
    return end == view.length ? 0 : end;
}

size_t markdown_autolink_length(struct markdown_view view, size_t start)
{
    if (start >= view.length || markdown_view_at(view, start) != '<')
        return 0;
    size_t end = closing_bracket(view, start);
    if (end == 0 || end == start + 1)
        return 0;
    return uri(view, start + 1, end) || email(view, start + 1, end) ? end - start + 1 : 0;
}
