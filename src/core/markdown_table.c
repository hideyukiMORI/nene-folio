#include "markdown_table.h"
#include "markdown_inline_tokens.h"
#include "rtf_text.h"
#include "text_buffer.h"

struct markdown_table
{
    struct markdown_span spans[63];
    size_t count;
};

static bool escaped(const char *_Nonnull text, size_t index)
{
    size_t count = 0;
    while (index != 0 && text[--index] == '\\')
        ++count;
    return count % 2 != 0;
}

static void add_cell(struct markdown_table *_Nonnull cells, const char *_Nonnull text,
                     size_t length)
{
    while (length != 0 && (*text == ' ' || *text == '\t'))
    {
        ++text;
        --length;
    }
    while (length != 0 && (text[length - 1] == ' ' || text[length - 1] == '\t'))
        --length;
    if (cells->count < 63)
        cells->spans[cells->count] = (struct markdown_span){.bytes = text, .length = length};
    ++cells->count;
}

static size_t row_end(struct markdown_line line)
{
    size_t end = line.length;
    while (end > line.content && (line.bytes[end - 1] == ' ' || line.bytes[end - 1] == '\t'))
        --end;
    if (end > line.content && line.bytes[end - 1] == '|' && !escaped(line.bytes, end - 1))
        --end;
    return end;
}

static struct markdown_table split(struct markdown_line line)
{
    struct markdown_table cells = {0};
    size_t start = line.content;
    size_t end = row_end(line);
    if (start < end && line.bytes[start] == '|')
        ++start;
    for (size_t index = start; index < end; ++index)
    {
        if (line.bytes[index] != '|' || escaped(line.bytes, index))
            continue;
        add_cell(&cells, line.bytes + start, index - start);
        start = index + 1;
    }
    add_cell(&cells, line.bytes + start, end - start);
    return cells;
}

size_t markdown_table_columns(struct markdown_line line)
{
    return markdown_line_valid(line) ? split(line).count : 0;
}

static bool delimiter_cell(struct markdown_span span)
{
    size_t start = 0;
    size_t end = span.length;
    if (end != 0 && span.bytes[0] == ':')
        ++start;
    if (end > start && span.bytes[end - 1] == ':')
        --end;
    if (start == end)
        return false;
    for (size_t index = start; index < end; ++index)
        if (span.bytes[index] != '-')
            return false;
    return true;
}

static bool wide_delimiter(struct markdown_line line)
{
    size_t start = line.content;
    size_t end = row_end(line);
    if (start < end && line.bytes[start] == '|')
        ++start;
    for (size_t index = start; index <= end; ++index)
    {
        if (index != end && (line.bytes[index] != '|' || escaped(line.bytes, index)))
            continue;
        struct markdown_table cell = {0};
        add_cell(&cell, line.bytes + start, index - start);
        if (!delimiter_cell(cell.spans[0]))
            return false;
        start = index + 1;
    }
    return true;
}

bool markdown_table_delimiter(struct markdown_line line)
{
    if (!markdown_line_valid(line))
        return false;
    struct markdown_table cells = split(line);
    if (cells.count > 63)
        return wide_delimiter(line);
    for (size_t index = 0; index < cells.count; ++index)
        if (!delimiter_cell(cells.spans[index]))
            return false;
    return true;
}

bool markdown_table_starts(struct markdown_line line, struct markdown_line delimiter)
{
    if (!markdown_line_valid(line) || !markdown_line_valid(delimiter))
        return false;
    for (size_t index = line.content; index < line.length; ++index)
        if (line.bytes[index] == '|')
            return markdown_table_columns(line) == markdown_table_columns(delimiter) &&
                   markdown_table_delimiter(delimiter);
    return false;
}

static void row_header(struct text_buffer *_Nonnull out, size_t columns)
{
    text_buffer_append_text(out, "\\trowd\\trgaph80\\trleft0");
    for (size_t index = 0; index < columns; ++index)
    {
        text_buffer_append_text(
            out, "\\clbrdrt\\brdrs\\brdrw10\\brdrcf3\\clbrdrb\\brdrs\\brdrw10\\brdrcf3"
                 "\\clbrdrl\\brdrs\\brdrw10\\brdrcf3\\clbrdrr\\brdrs\\brdrw10\\brdrcf3\\cellx");
        rtf_text_number(out, (index + 1) * (9000 / columns));
    }
}

static void cell_header(struct text_buffer *_Nonnull out, struct markdown_span delimiter,
                        enum markdown_row_kind kind)
{
    text_buffer_append_text(out, "\\pard\\intbl\\sa80\\sl240\\slmult1\\cf1\\fs22");
    bool left = delimiter.length != 0 && delimiter.bytes[0] == ':';
    bool right = delimiter.length != 0 && delimiter.bytes[delimiter.length - 1] == ':';
    text_buffer_append_text(out, right ? (left ? "\\qc " : "\\qr ") : "\\ql ");
    switch (kind)
    {
    case MARKDOWN_ROW_KIND_HEADER:
        text_buffer_append_text(out, "{\\b ");
        return;
    case MARKDOWN_ROW_KIND_BODY:
        text_buffer_append_text(out, "{");
        return;
    }
}

static bool row_kind_valid(enum markdown_row_kind kind)
{
    switch (kind)
    {
    case MARKDOWN_ROW_KIND_HEADER:
    case MARKDOWN_ROW_KIND_BODY:
        return true;
    }
    return false;
}

enum markdown_rtf_outcome markdown_table_row(struct text_buffer *_Nonnull out,
                                             struct markdown_line line,
                                             struct markdown_line delimiter,
                                             enum markdown_row_kind kind)
{
    if (!row_kind_valid(kind) || !markdown_line_valid(line) || !markdown_line_valid(delimiter))
        return MARKDOWN_RTF_INVALID_INPUT;
    struct markdown_table cells = split(line);
    struct markdown_table delimiters = split(delimiter);
    if (delimiters.count == 0 || delimiters.count > 63)
        return MARKDOWN_RTF_INVALID_INPUT;
    row_header(out, delimiters.count);
    for (size_t index = 0; index < delimiters.count; ++index)
    {
        struct markdown_span cell = {.bytes = "", .length = 0};
        if (index < cells.count)
            cell = cells.spans[index];
        cell_header(out, delimiters.spans[index], kind);
        struct markdown_view view = {
            .spans = &cell, .count = 1, .length = cell.length, .table_cell = true};
        enum markdown_rtf_outcome rendered = markdown_inline_tokens_render(out, view);
        if (rendered != MARKDOWN_RTF_CONVERTED)
            return rendered;
        text_buffer_append_text(out, "}\\cell ");
    }
    text_buffer_append_text(out, "\\row\n");
    return MARKDOWN_RTF_CONVERTED;
}
