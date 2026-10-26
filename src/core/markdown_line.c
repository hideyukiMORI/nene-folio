#include "markdown_line.h"

static void prefix(struct markdown_line *_Nonnull line);

static bool space(char character)
{
    return character == ' ' || character == '\t';
}

bool markdown_line_valid(struct markdown_line line)
{
    return line.prefix <= line.content && line.content <= line.length &&
           line.marker <= line.length - line.content;
}

struct markdown_line markdown_line_read(const char *_Nonnull source, size_t length, size_t start)
{
    if (start > length)
        start = length;
    size_t end = start;
    while (end < length && source[end] != '\n')
        ++end;
    struct markdown_line line = {
        .bytes = source + start, .length = end - start, .next = end < length ? end + 1 : length};
    if (line.length != 0 && line.bytes[line.length - 1] == '\r')
        --line.length;
    prefix(&line);
    return line;
}

static size_t quote_end(struct markdown_line *_Nonnull line)
{
    size_t index = 0;
    while (index < line->length)
    {
        size_t before = index;
        while (index < line->length && index - before < 3 && line->bytes[index] == ' ')
            ++index;
        if (index == line->length || line->bytes[index] != '>')
            return before;
        ++line->quote;
        ++index;
        if (index < line->length && space(line->bytes[index]))
            ++index;
    }
    return index;
}

static void prefix(struct markdown_line *_Nonnull line)
{
    size_t index = quote_end(line);
    line->prefix = index;
    while (index < line->length && space(line->bytes[index]))
    {
        line->indent += line->bytes[index] == '\t' ? 4 - line->indent % 4 : 1;
        ++index;
    }
    line->content = index;
}

static size_t skip_space(struct markdown_line line, size_t index)
{
    while (index < line.length && space(line.bytes[index]))
        ++index;
    return index;
}

static bool contains_backtick(struct markdown_line line, size_t index)
{
    for (; index < line.length; ++index)
        if (line.bytes[index] == '`')
            return true;
    return false;
}

bool markdown_line_blank(struct markdown_line line)
{
    return !markdown_line_valid(line) || line.content == line.length;
}

size_t markdown_line_setext(struct markdown_line line)
{
    if (!markdown_line_valid(line) || line.indent > 3 || line.content == line.length)
        return 0;
    char marker = line.bytes[line.content];
    if (marker != '=' && marker != '-')
        return 0;
    size_t index = line.content;
    while (index < line.length && line.bytes[index] == marker)
        ++index;
    if (skip_space(line, index) != line.length)
        return 0;
    return marker == '=' ? 1 : 2;
}

bool markdown_line_thematic(struct markdown_line line)
{
    if (!markdown_line_valid(line) || line.indent > 3 || line.content == line.length)
        return false;
    char marker = line.bytes[line.content];
    if (marker != '*' && marker != '-' && marker != '_')
        return false;
    size_t count = 0;
    for (size_t index = line.content; index < line.length; ++index)
    {
        if (line.bytes[index] == marker)
            ++count;
        else if (!space(line.bytes[index]))
            return false;
    }
    return count >= 3;
}

size_t markdown_line_fence(struct markdown_line line)
{
    if (!markdown_line_valid(line) || line.indent > 3 || line.content == line.length)
        return 0;
    char marker = line.bytes[line.content];
    if (marker != '`' && marker != '~')
        return 0;
    size_t end = line.content;
    while (end < line.length && line.bytes[end] == marker)
        ++end;
    if (end - line.content < 3)
        return 0;
    if (marker == '`' && contains_backtick(line, end))
        return 0;
    return end - line.content;
}

size_t markdown_line_heading(struct markdown_line line)
{
    if (!markdown_line_valid(line) || line.indent > 3)
        return 0;
    size_t end = line.content;
    while (end < line.length && line.bytes[end] == '#')
        ++end;
    size_t level = end - line.content;
    if (level == 0 || level > 6 || end == line.length || !space(line.bytes[end]))
        return 0;
    return level;
}

static size_t marker_end(struct markdown_line *_Nonnull line)
{
    size_t index = line->content;
    char character = line->bytes[index];
    if (character == '-' || character == '*' || character == '+')
        return index + 1;
    size_t digits = 0;
    while (index < line->length && line->bytes[index] >= '0' && line->bytes[index] <= '9' &&
           digits < 9)
    {
        line->number = line->number * 10 + (size_t)(line->bytes[index++] - '0');
        ++digits;
    }
    if (digits == 0 || index == line->length ||
        (line->bytes[index] != '.' && line->bytes[index] != ')'))
        return line->content;
    line->ordered = true;
    return index + 1;
}

void markdown_line_list(struct markdown_line *_Nonnull line)
{
    if (!markdown_line_valid(*line) || line->content == line->length)
        return;
    size_t end = marker_end(line);
    if (end == line->content || end == line->length || !space(line->bytes[end]))
        return;
    line->delimiter = line->bytes[end - 1];
    size_t first = end;
    while (end < line->length && space(line->bytes[end]))
        ++end;
    if (end - first > 4)
        end = first + 1;
    line->marker = end - line->content;
}
