#include "markdown_blocks.h"
#include "markdown_code_state.h"
#include "markdown_inline_tokens.h"
#include "markdown_line.h"
#include "markdown_list.h"
#include "markdown_table.h"
#include "markdown_table_state.h"
#include "markdown_task_kind.h"
#include "rtf_text.h"
#include "text_buffer.h"
#include <stdint.h>
#include <stdlib.h>

struct markdown_blocks
{
    struct text_buffer *_Nonnull out;
    const char *_Nonnull source;
    size_t length;
    enum markdown_rtf_outcome outcome;
    struct markdown_span *_Nullable spans;
    size_t span_count;
    size_t span_capacity;
    size_t inline_length;
    struct markdown_list *_Nullable list;
    size_t quote;
    size_t paragraph_indent;
    size_t paragraph_number;
    size_t heading;
    bool paragraph_list;
    bool paragraph_ordered;
    char paragraph_delimiter;
    enum markdown_task_kind task;
    char fence;
    size_t fence_length;
    size_t fence_indent;
    size_t fence_quote;
    size_t fence_list_indent;
    size_t code_indent;
    enum markdown_code_state code_state;
    bool after_rule;
    enum markdown_table_state table_state;
    struct markdown_line delimiter;
};

static bool reserve_spans(struct markdown_blocks *_Nonnull parser)
{
    if (parser->span_count < parser->span_capacity)
        return true;
    size_t capacity = parser->span_capacity == 0 ? 8 : parser->span_capacity * 2;
    if (capacity < parser->span_capacity || capacity > SIZE_MAX / sizeof *parser->spans)
        return false;
    struct markdown_span *grown = realloc(parser->spans, capacity * sizeof *parser->spans);
    if (grown == nullptr)
        return false;
    parser->spans = grown;
    parser->span_capacity = capacity;
    return true;
}

static bool add_span(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (!reserve_spans(parser))
        return false;
    size_t length = line.length - line.content;
    if (parser->inline_length > SIZE_MAX - length - 1)
        return false;
    if (parser->span_count != 0)
        ++parser->inline_length;
    parser->spans[parser->span_count++] = (struct markdown_span){
        .bytes = line.bytes + line.content, .length = length, .offset = parser->inline_length};
    parser->inline_length += length;
    return true;
}

static void indent(struct text_buffer *_Nonnull out, size_t value)
{
    if (value == 0)
        return;
    text_buffer_append_text(out, "\\li");
    rtf_text_number(out, value);
}

static void heading_header(struct markdown_blocks *_Nonnull parser)
{
    static const size_t sizes[] = {44, 34, 28, 22, 22, 22};
    text_buffer_append_text(parser->out, parser->heading <= 3 ? "\\pard\\sb200\\sa80\\b\\cf2\\fs"
                                                              : "\\pard\\sb160\\sa60\\b0\\cf2\\fs");
    rtf_text_number(parser->out, sizes[parser->heading - 1]);
    indent(parser->out, parser->paragraph_indent);
    text_buffer_append_text(parser->out, " ");
}

static bool task_label(struct text_buffer *_Nonnull out, enum markdown_task_kind task)
{
    switch (task)
    {
    case MARKDOWN_TASK_KIND_NONE:
        return false;
    case MARKDOWN_TASK_KIND_OPEN:
        text_buffer_append_text(out, "\\u9744?\\tab ");
        return true;
    case MARKDOWN_TASK_KIND_DONE:
        text_buffer_append_text(out, "\\u9745?\\tab ");
        return true;
    }
    return false;
}

static void list_label(struct markdown_blocks *_Nonnull parser)
{
    if (task_label(parser->out, parser->task))
        return;
    if (!parser->paragraph_ordered)
    {
        text_buffer_append_text(parser->out, "\\bullet\\tab ");
        return;
    }
    text_buffer_append_text(parser->out, " ");
    rtf_text_number(parser->out, parser->paragraph_number);
    rtf_text_append(parser->out, &parser->paragraph_delimiter, 1);
    text_buffer_append_text(parser->out, "\\tab ");
}

static void paragraph_header(struct markdown_blocks *_Nonnull parser)
{
    if (parser->heading != 0)
    {
        heading_header(parser);
        return;
    }
    text_buffer_append_text(parser->out, "\\pard");
    indent(parser->out, parser->paragraph_indent);
    if (parser->paragraph_list)
        text_buffer_append_text(parser->out, "\\fi-220\\sa40");
    else
        text_buffer_append_text(parser->out, "\\sa80");
    if (parser->after_rule)
        text_buffer_append_text(parser->out, "\\sb120");
    text_buffer_append_text(parser->out, "\\sl240\\slmult1");
    text_buffer_append_text(parser->out, parser->quote != 0 ? "\\cf3" : "\\cf1");
    if (parser->paragraph_list)
        list_label(parser);
    else
        text_buffer_append_text(parser->out, " ");
}

static bool flush(struct markdown_blocks *_Nonnull parser, bool before_rule)
{
    if (parser->span_count == 0)
        return true;
    paragraph_header(parser);
    struct markdown_view view = {
        .spans = parser->spans, .count = parser->span_count, .length = parser->inline_length};
    parser->outcome = markdown_inline_tokens_render(parser->out, view);
    if (parser->outcome != MARKDOWN_RTF_CONVERTED)
        return false;
    if (parser->heading != 0)
        text_buffer_append_text(parser->out, "\\b0\\cf1\\fs22");
    else if (parser->quote != 0)
        text_buffer_append_text(parser->out, "\\cf1");
    if (before_rule)
        text_buffer_append_text(parser->out, "\\sa120");
    text_buffer_append_text(parser->out, "\\par\n");
    parser->span_count = parser->inline_length = parser->heading = 0;
    parser->task = MARKDOWN_TASK_KIND_NONE;
    parser->paragraph_list = parser->after_rule = false;
    return true;
}

static void close_code(struct markdown_blocks *_Nonnull parser)
{
    if (parser->code_state == MARKDOWN_CODE_STATE_PARAGRAPH)
        text_buffer_append_text(parser->out, "\\sa120\\par\n");
    text_buffer_append_text(parser->out, "}");
    parser->code_state = MARKDOWN_CODE_STATE_OUTSIDE;
}

static bool fence_closes(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (line.indent > 3 || line.content == line.length || line.bytes[line.content] != parser->fence)
        return false;
    size_t end = line.content;
    while (end < line.length && line.bytes[end] == parser->fence)
        ++end;
    if (end - line.content < parser->fence_length)
        return false;
    while (end < line.length && (line.bytes[end] == ' ' || line.bytes[end] == '\t'))
        ++end;
    return end == line.length;
}

static size_t code_start(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    size_t index = line.prefix;
    size_t columns = 0;
    while (index < line.length && columns < parser->fence_indent)
    {
        char character = line.bytes[index];
        if (character != ' ' && character != '\t')
            break;
        columns += character == '\t' ? 4 - columns % 4 : 1;
        ++index;
    }
    return index;
}

static const char *_Nonnull code_spacing(enum markdown_code_state state)
{
    switch (state)
    {
    case MARKDOWN_CODE_STATE_OUTSIDE:
    case MARKDOWN_CODE_STATE_EMPTY:
        return "120";
    case MARKDOWN_CODE_STATE_PARAGRAPH:
        return "0";
    }
    return "120";
}

static void code_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (parser->code_state == MARKDOWN_CODE_STATE_PARAGRAPH)
        text_buffer_append_text(parser->out, "\\par\n");
    text_buffer_append_text(parser->out, "\\pard\\li");
    rtf_text_number(parser->out, parser->code_indent + 180);
    text_buffer_append_text(parser->out, "\\ri180\\sb");
    text_buffer_append_text(parser->out, code_spacing(parser->code_state));
    text_buffer_append_text(parser->out, "\\sa0\\sl240\\slmult1 ");
    size_t start = code_start(parser, line);
    rtf_text_append(parser->out, line.bytes + start, line.length - start);
    parser->code_state = MARKDOWN_CODE_STATE_PARAGRAPH;
}

static size_t list_depth(struct markdown_blocks *_Nonnull parser)
{
    return parser->list == nullptr ? 0 : markdown_list_depth(parser->list);
}

static void next_item(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (parser->list == nullptr || markdown_list_indent(parser->list) != line.indent)
        return;
    if (markdown_list_same(parser->list, line))
        markdown_list_advance(parser->list);
    else
        markdown_list_pop(&parser->list);
}

static bool list_item(struct markdown_blocks *_Nonnull parser, struct markdown_line *_Nonnull line)
{
    if (!flush(parser, false))
        return false;
    while (parser->list != nullptr && markdown_list_indent(parser->list) > line->indent)
        markdown_list_pop(&parser->list);
    next_item(parser, *line);
    if (parser->list == nullptr || markdown_list_indent(parser->list) < line->indent)
        if (markdown_list_create(parser->list, *line, &parser->list) != MARKDOWN_RTF_CONVERTED)
            return false;
    parser->table_state = MARKDOWN_TABLE_STATE_NONE;
    parser->paragraph_list = true;
    parser->paragraph_ordered = markdown_list_ordered(parser->list);
    parser->paragraph_number = markdown_list_number(parser->list);
    parser->paragraph_delimiter = markdown_list_delimiter(parser->list);
    parser->paragraph_indent = parser->quote * 480 + list_depth(parser) * 440;
    line->content += line->marker;
    line->indent = 0;
    return true;
}

static void task_marker(struct markdown_blocks *_Nonnull parser,
                        struct markdown_line *_Nonnull line)
{
    size_t start = line->content;
    if (!parser->paragraph_list || line->length - start < 4 || line->bytes[start] != '[' ||
        line->bytes[start + 2] != ']' || line->bytes[start + 3] != ' ')
        return;
    char mark = line->bytes[start + 1];
    if (mark != ' ' && mark != 'x' && mark != 'X')
        return;
    parser->task = mark == ' ' ? MARKDOWN_TASK_KIND_OPEN : MARKDOWN_TASK_KIND_DONE;
    line->content += 4;
}

static void continuation_indent(struct markdown_blocks *_Nonnull parser,
                                struct markdown_line *_Nonnull line)
{
    if (parser->list != nullptr)
    {
        size_t base = markdown_list_content_indent(parser->list);
        line->indent = line->indent >= base ? line->indent - base : 0;
    }
    if (parser->span_count == 0)
        parser->paragraph_indent = parser->quote * 480 + list_depth(parser) * 440;
}

static bool list_continuation(struct markdown_blocks *_Nonnull parser,
                              struct markdown_line *_Nonnull line)
{
    while (parser->list != nullptr && line->indent < markdown_list_content_indent(parser->list))
    {
        if (parser->span_count != 0 && line->indent == 0)
            break;
        if (!flush(parser, false))
            return false;
        markdown_list_pop(&parser->list);
    }
    continuation_indent(parser, line);
    return true;
}

static bool item_starts(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    bool interrupts =
        parser->list == nullptr && parser->span_count != 0 && line.ordered && line.number != 1;
    return line.marker != 0 && !markdown_line_thematic(line) && !interrupts;
}

static bool containers(struct markdown_blocks *_Nonnull parser, struct markdown_line *_Nonnull line)
{
    if (line->quote != parser->quote)
    {
        if (!flush(parser, false))
            return false;
        markdown_list_destroy(parser->list);
        parser->list = nullptr;
        parser->table_state = MARKDOWN_TABLE_STATE_NONE;
        parser->quote = line->quote;
    }
    markdown_line_list(line);
    if (item_starts(parser, *line))
        return list_item(parser, line);
    return list_continuation(parser, line);
}

static void start_fence(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                        size_t length)
{
    parser->fence = line.bytes[line.content];
    parser->code_state = MARKDOWN_CODE_STATE_EMPTY;
    parser->fence_length = length;
    parser->fence_indent = line.indent;
    if (parser->list != nullptr)
        parser->fence_indent += markdown_list_content_indent(parser->list);
    parser->fence_quote = line.quote;
    parser->fence_list_indent =
        parser->list == nullptr ? 0 : markdown_list_content_indent(parser->list);
    parser->code_indent = parser->paragraph_indent;
    parser->paragraph_list = false;
    text_buffer_append_text(parser->out, "{\\f1\\fs20\\cf5\\highlight6");
}

static struct markdown_line heading_content(struct markdown_line line, size_t level)
{
    line.content += level + 1;
    while (line.content < line.length &&
           (line.bytes[line.content] == ' ' || line.bytes[line.content] == '\t'))
        ++line.content;
    while (line.length > line.content &&
           (line.bytes[line.length - 1] == ' ' || line.bytes[line.length - 1] == '\t'))
        --line.length;
    size_t hashes = line.length;
    while (hashes > line.content && line.bytes[hashes - 1] == '#')
        --hashes;
    if (hashes < line.length && hashes > line.content && line.bytes[hashes - 1] == ' ')
        line.length = hashes - 1;
    return line;
}

static bool heading_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                         size_t level)
{
    if (!flush(parser, false))
        return false;
    parser->heading = level;
    line = heading_content(line, level);
    return add_span(parser, line) && flush(parser, false);
}

static void rule(struct markdown_blocks *_Nonnull parser)
{
    text_buffer_append_text(parser->out, "{\\pard\\sb0\\sa0\\sl-20\\slmult0\\fs2\\highlight3");
    indent(parser->out, parser->paragraph_indent);
    text_buffer_append_text(parser->out, "\\par}\n");
    parser->after_rule = true;
    parser->paragraph_list = false;
}

static bool literal_row(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (!flush(parser, false))
        return false;
    text_buffer_append_text(parser->out, "\\pard\\sa80\\sl240\\slmult1\\cf1 ");
    rtf_text_append(parser->out, line.bytes, line.length);
    text_buffer_append_text(parser->out, "\\par\n");
    return true;
}

static bool start_table(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                        struct markdown_line delimiter)
{
    if (!flush(parser, false))
        return false;
    parser->delimiter = delimiter;
    parser->table_state = markdown_table_columns(line) > 63 ? MARKDOWN_TABLE_STATE_LITERAL
                                                            : MARKDOWN_TABLE_STATE_NATIVE;
    parser->paragraph_list = false;
    if (parser->table_state == MARKDOWN_TABLE_STATE_LITERAL)
        return literal_row(parser, line) && literal_row(parser, delimiter);
    parser->outcome = markdown_table_row(parser->out, line, delimiter, MARKDOWN_ROW_KIND_HEADER);
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool table_body(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (parser->table_state == MARKDOWN_TABLE_STATE_LITERAL)
        return literal_row(parser, line);
    parser->outcome =
        markdown_table_row(parser->out, line, parser->delimiter, MARKDOWN_ROW_KIND_BODY);
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool special_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    size_t level = markdown_line_heading(line);
    if (level != 0)
        return heading_line(parser, line, level);
    if (markdown_line_thematic(line))
    {
        if (!flush(parser, true))
            return false;
        rule(parser);
        return true;
    }
    size_t fence = markdown_line_fence(line);
    if (fence != 0)
    {
        if (!flush(parser, false))
            return false;
        start_fence(parser, line, fence);
        return true;
    }
    task_marker(parser, &line);
    return add_span(parser, line);
}

static bool has_pipe(struct markdown_line line)
{
    for (size_t index = line.content; index < line.length; ++index)
        if (line.bytes[index] == '|')
            return true;
    return false;
}

static bool table_candidate(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                            struct markdown_line next)
{
    return line.next < parser->length && has_pipe(line) && next.quote == line.quote &&
           markdown_table_columns(line) == markdown_table_columns(next) &&
           markdown_table_delimiter(next);
}

static bool table_continues(struct markdown_line line)
{
    return markdown_line_heading(line) == 0 && markdown_line_fence(line) == 0 &&
           !markdown_line_thematic(line) && line.marker == 0;
}

static bool render_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                        size_t *_Nonnull cursor)
{
    if (markdown_line_blank(line))
    {
        parser->table_state = MARKDOWN_TABLE_STATE_NONE;
        return flush(parser, false);
    }
    if (!containers(parser, &line))
        return false;
    size_t setext = markdown_line_setext(line);
    if (parser->span_count != 0 && setext != 0)
    {
        parser->heading = setext;
        return flush(parser, false);
    }
    if (parser->table_state != MARKDOWN_TABLE_STATE_NONE && table_continues(line))
        return table_body(parser, line);
    parser->table_state = MARKDOWN_TABLE_STATE_NONE;
    struct markdown_line next = markdown_line_read(parser->source, parser->length, line.next);
    if (table_candidate(parser, line, next))
    {
        *cursor = next.next;
        return start_table(parser, line, next);
    }
    return special_line(parser, line);
}

static void render_fenced_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    struct markdown_line adjusted = line;
    size_t base = parser->fence_indent;
    adjusted.indent = line.indent >= base ? line.indent - base : 0;
    if (fence_closes(parser, adjusted))
        close_code(parser);
    else
        code_line(parser, line);
}

static bool fence_container(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    return line.quote == parser->fence_quote &&
           (parser->fence_list_indent == 0 || markdown_line_blank(line) ||
            line.indent >= parser->fence_list_indent);
}

static bool render_next(struct markdown_blocks *_Nonnull parser, size_t *_Nonnull cursor)
{
    struct markdown_line line = markdown_line_read(parser->source, parser->length, *cursor);
    *cursor = line.next;
    if (parser->code_state != MARKDOWN_CODE_STATE_OUTSIDE && fence_container(parser, line))
    {
        render_fenced_line(parser, line);
        return true;
    }
    if (parser->code_state != MARKDOWN_CODE_STATE_OUTSIDE)
        close_code(parser);
    return render_line(parser, line, cursor);
}

static void markdown_blocks_destroy(struct markdown_blocks *_Nonnull parser)
{
    free(parser->spans);
    markdown_list_destroy(parser->list);
}

enum markdown_rtf_outcome markdown_blocks_render(struct text_buffer *_Nonnull out,
                                                 const char *_Nonnull text, size_t length)
{
    struct markdown_blocks parser = {
        .out = out, .source = text, .length = length, .outcome = MARKDOWN_RTF_CONVERTED};
    size_t cursor = 0;
    bool accepted = true;
    while (cursor < length && accepted)
        accepted = render_next(&parser, &cursor);
    if (parser.code_state != MARKDOWN_CODE_STATE_OUTSIDE)
        close_code(&parser);
    if (accepted)
        accepted = flush(&parser, false);
    markdown_blocks_destroy(&parser);
    if (!accepted && parser.outcome == MARKDOWN_RTF_CONVERTED)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    return parser.outcome;
}
