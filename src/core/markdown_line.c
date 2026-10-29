#include "markdown_line.h"
#include <stdint.h>

static bool space(char character)
{
    return character == ' ' || character == '\t';
}

static size_t space_width(char character, size_t column)
{
    return character == '\t' ? 4 - column % 4 : 1;
}

bool markdown_line_valid(struct markdown_line line)
{
    if (line.prefix > line.content || line.content > line.length ||
        line.marker > line.length - line.content || line.padding > 3 ||
        line.column > SIZE_MAX - line.indent)
        return false;
    if (line.prefix <= SIZE_MAX / 4 && line.column > line.prefix * 4)
        return false;
    if (line.padding == 0)
        return true;
    return line.prefix != 0 && line.bytes[line.prefix - 1] == '\t' && line.indent >= line.padding &&
           (line.column + line.padding) % 4 == 0;
}

static bool prefix(struct markdown_line *_Nonnull line)
{
    size_t index = line->prefix;
    size_t column = line->column;
    if (column > SIZE_MAX - line->padding)
        return false;
    column += line->padding;
    while (index < line->length && space(line->bytes[index]))
    {
        size_t width = space_width(line->bytes[index], column);
        if (column > SIZE_MAX - width)
            return false;
        column += width;
        ++index;
    }
    line->content = index;
    line->indent = column - line->column;
    return true;
}

struct markdown_line markdown_line_read(const char *_Nonnull source, size_t length, size_t start)
{
    if (start > length)
        start = length;
    size_t end = start;
    while (end < length && source[end] != '\r' && source[end] != '\n')
        ++end;
    size_t next = end;
    if (next < length)
    {
        ++next;
        if (source[end] == '\r' && next < length && source[next] == '\n')
            ++next;
    }
    struct markdown_line line = {.bytes = source + start, .length = end - start, .next = next};
    if (!prefix(&line))
        line.content = line.length;
    return line;
}

static bool consume_space(struct markdown_line *_Nonnull line, size_t columns)
{
    size_t pending = columns < line->padding ? columns : line->padding;
    line->padding -= pending;
    line->column += pending;
    columns -= pending;
    while (columns != 0)
    {
        if (line->prefix == line->length || !space(line->bytes[line->prefix]))
            return false;
        size_t width = space_width(line->bytes[line->prefix], line->column);
        size_t taken = columns < width ? columns : width;
        if (line->column > SIZE_MAX - taken)
            return false;
        ++line->prefix;
        line->padding = width - taken;
        line->column += taken;
        columns -= taken;
    }
    return true;
}

bool markdown_line_skip(struct markdown_line *_Nonnull line, size_t columns)
{
    if (!markdown_line_valid(*line) || columns > line->indent)
        return false;
    struct markdown_line skipped = *line;
    if (!consume_space(&skipped, columns) || !prefix(&skipped))
        return false;
    *line = skipped;
    return true;
}

static bool advance(struct markdown_line *_Nonnull line, size_t bytes)
{
    if (line->padding != 0 || bytes > line->length - line->prefix ||
        line->column > SIZE_MAX - bytes)
        return false;
    line->prefix += bytes;
    line->column += bytes;
    return prefix(line);
}

bool markdown_line_quote(struct markdown_line *_Nonnull line)
{
    if (!markdown_line_valid(*line) || line->indent > 3 || line->content == line->length ||
        line->bytes[line->content] != '>')
        return false;
    struct markdown_line entered = *line;
    if (!markdown_line_skip(&entered, entered.indent) || !advance(&entered, 1))
        return false;
    if (entered.indent != 0 && !markdown_line_skip(&entered, 1))
        return false;
    *line = entered;
    return true;
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
    line->marker = line->marker_columns = line->number = 0;
    line->ordered = false;
    if (!markdown_line_valid(*line) || line->content == line->length || line->indent > 3)
        return;
    size_t end = marker_end(line);
    if (end == line->content || (end < line->length && !space(line->bytes[end])))
        return;
    size_t token = end - line->content;
    struct markdown_line after = *line;
    if (!markdown_line_skip(&after, after.indent) || !advance(&after, token))
        return;
    size_t padding = after.indent;
    if (padding == 0 || padding > 4 || markdown_line_blank(after))
        padding = 1;
    line->delimiter = line->bytes[end - 1];
    line->marker = token;
    line->marker_columns = token + padding;
}

bool markdown_line_enter_list(struct markdown_line *_Nonnull line)
{
    if (!markdown_line_valid(*line) || line->marker == 0 || line->marker_columns <= line->marker)
        return false;
    struct markdown_line entered = *line;
    size_t padding = line->marker_columns - line->marker;
    if (!markdown_line_skip(&entered, entered.indent) || !advance(&entered, entered.marker))
        return false;
    entered.marker = entered.marker_columns = 0;
    if (!markdown_line_blank(entered) && !markdown_line_skip(&entered, padding))
        return false;
    *line = entered;
    return true;
}
