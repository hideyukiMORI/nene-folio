#include "markdown_blocks.h"
#include "markdown_code_state.h"
#include "markdown_containers.h"
#include "markdown_inline_tokens.h"
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
    struct markdown_containers *_Nullable containers;
    size_t paragraph_indent;
    size_t heading;
    bool paragraph_quote;
    bool paragraph_item;
    enum markdown_task_kind task;
    char fence;
    size_t fence_length;
    size_t fence_indent;
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
    if (!markdown_line_valid(line))
    {
        parser->outcome = MARKDOWN_RTF_INVALID_INPUT;
        return false;
    }
    if (!reserve_spans(parser))
        return false;
    size_t length = line.length - line.content;
    if (parser->inline_length > SIZE_MAX - length - 1)
        return false;
    if (parser->span_count == 0)
    {
        parser->paragraph_indent = markdown_containers_indent(parser->containers);
        parser->paragraph_quote = markdown_containers_quote_depth(parser->containers) != 0;
        parser->paragraph_item = markdown_containers_pending(parser->containers) != 0;
    }
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

static void marker_label(struct markdown_blocks *_Nonnull parser, enum markdown_task_kind task)
{
    text_buffer_append_text(parser->out, "{\\f0\\fs22\\cf1\\b0\\highlight0 ");
    bool labelled = task_label(parser->out, task);
    if (!labelled && !markdown_containers_marker_ordered(parser->containers))
        text_buffer_append_text(parser->out, "\\bullet\\tab ");
    if (!labelled && markdown_containers_marker_ordered(parser->containers))
    {
        rtf_text_number(parser->out, markdown_containers_marker_number(parser->containers));
        char delimiter = markdown_containers_marker_delimiter(parser->containers);
        rtf_text_append(parser->out, &delimiter, 1);
        text_buffer_append_text(parser->out, "\\tab ");
    }
    text_buffer_append_text(parser->out, "}");
    markdown_containers_consume_marker(parser->containers);
}

static void marker_position(struct markdown_blocks *_Nonnull parser, size_t body)
{
    size_t marker = markdown_containers_marker_indent(parser->containers) - 220;
    text_buffer_append_text(parser->out, "\\fi-");
    rtf_text_number(parser->out, body - marker);
    text_buffer_append_text(parser->out, "\\tx");
    rtf_text_number(parser->out, body);
}

static void marker_paragraph(struct markdown_blocks *_Nonnull parser)
{
    size_t body = markdown_containers_marker_indent(parser->containers);
    text_buffer_append_text(parser->out, "\\pard");
    indent(parser->out, body);
    marker_position(parser, body);
    text_buffer_append_text(parser->out, "\\sb0\\sa0\\sl240\\slmult1 ");
    marker_label(parser, MARKDOWN_TASK_KIND_NONE);
    text_buffer_append_text(parser->out, "\\par\n");
}

static void outer_markers(struct markdown_blocks *_Nonnull parser, size_t remaining)
{
    while (markdown_containers_pending(parser->containers) > remaining)
        marker_paragraph(parser);
}

static void heading_header(struct markdown_blocks *_Nonnull parser)
{
    static const size_t sizes[] = {44, 34, 28, 22, 22, 22};
    text_buffer_append_text(parser->out, "\\pard\\sb");
    rtf_text_number(parser->out, parser->paragraph_item ? 0 : (parser->heading <= 3 ? 200 : 160));
    text_buffer_append_text(parser->out,
                            parser->heading <= 3 ? "\\sa80\\b\\cf2\\fs" : "\\sa60\\b0\\cf2\\fs");
    rtf_text_number(parser->out, sizes[parser->heading - 1]);
    indent(parser->out, parser->paragraph_indent);
    if (markdown_containers_pending(parser->containers) != 0)
    {
        marker_position(parser, parser->paragraph_indent);
        text_buffer_append_text(parser->out, " ");
        marker_label(parser, parser->task);
    }
    else
        text_buffer_append_text(parser->out, " ");
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
    bool marker = markdown_containers_pending(parser->containers) != 0;
    if (marker)
        marker_position(parser, parser->paragraph_indent);
    text_buffer_append_text(parser->out,
                            markdown_containers_in_list(parser->containers) ? "\\sa40" : "\\sa80");
    if (parser->after_rule)
        text_buffer_append_text(parser->out, "\\sb120");
    text_buffer_append_text(parser->out, "\\sl240\\slmult1");
    text_buffer_append_text(parser->out, parser->paragraph_quote ? "\\cf3 " : "\\cf1 ");
    if (marker)
        marker_label(parser, parser->task);
}

static bool flush(struct markdown_blocks *_Nonnull parser, bool before_rule)
{
    if (parser->span_count == 0)
        return true;
    outer_markers(parser, 1);
    paragraph_header(parser);
    struct markdown_view view = {
        .spans = parser->spans, .count = parser->span_count, .length = parser->inline_length};
    parser->outcome = markdown_inline_tokens_render(parser->out, view);
    if (parser->outcome != MARKDOWN_RTF_CONVERTED)
        return false;
    if (parser->heading != 0)
        text_buffer_append_text(parser->out, "\\b0\\cf1\\fs22");
    else if (parser->paragraph_quote)
        text_buffer_append_text(parser->out, "\\cf1");
    if (before_rule)
        text_buffer_append_text(parser->out, "\\sa120");
    text_buffer_append_text(parser->out, "\\par\n");
    parser->span_count = parser->inline_length = parser->heading = 0;
    parser->task = MARKDOWN_TASK_KIND_NONE;
    parser->after_rule = false;
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

static bool code_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    size_t removed = line.indent < parser->fence_indent ? line.indent : parser->fence_indent;
    if (!markdown_line_skip(&line, removed))
    {
        parser->outcome = MARKDOWN_RTF_INVALID_INPUT;
        return false;
    }
    if (parser->code_state == MARKDOWN_CODE_STATE_PARAGRAPH)
        text_buffer_append_text(parser->out, "\\par\n");
    text_buffer_append_text(parser->out, "\\pard\\li");
    rtf_text_number(parser->out, parser->code_indent + 180);
    text_buffer_append_text(parser->out, "\\ri180\\sb");
    text_buffer_append_text(parser->out, code_spacing(parser->code_state));
    text_buffer_append_text(parser->out, "\\sa0\\sl240\\slmult1 ");
    for (size_t index = 0; index < line.padding; ++index)
        text_buffer_append_text(parser->out, " ");
    rtf_text_append(parser->out, line.bytes + line.prefix, line.length - line.prefix);
    parser->code_state = MARKDOWN_CODE_STATE_PARAGRAPH;
    return true;
}

static bool start_fence(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                        size_t length)
{
    outer_markers(parser, 0);
    parser->fence = line.bytes[line.content];
    parser->code_state = MARKDOWN_CODE_STATE_EMPTY;
    parser->fence_length = length;
    parser->fence_indent = line.indent;
    parser->code_indent = markdown_containers_indent(parser->containers);
    if (parser->code_indent > SIZE_MAX - 180)
    {
        parser->outcome = MARKDOWN_RTF_INVALID_INPUT;
        return false;
    }
    text_buffer_append_text(parser->out, "{\\f1\\fs20\\cf5\\highlight6");
    return true;
}

static void task_marker(struct markdown_blocks *_Nonnull parser,
                        struct markdown_line *_Nonnull line)
{
    size_t start = line->content;
    if (parser->span_count != 0 || !markdown_containers_task_start(parser->containers) ||
        line->length - start < 4 || line->bytes[start] != '[' || line->bytes[start + 2] != ']' ||
        (line->bytes[start + 3] != ' ' && line->bytes[start + 3] != '\t'))
        return;
    char mark = line->bytes[start + 1];
    if (mark != ' ' && mark != 'x' && mark != 'X')
        return;
    parser->task = mark == ' ' ? MARKDOWN_TASK_KIND_OPEN : MARKDOWN_TASK_KIND_DONE;
    line->content += 4;
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
    return add_span(parser, heading_content(line, level)) && flush(parser, false);
}

static void rule(struct markdown_blocks *_Nonnull parser)
{
    outer_markers(parser, 0);
    text_buffer_append_text(parser->out, "{\\pard\\sb0\\sa0\\sl-20\\slmult0\\fs2\\highlight3");
    indent(parser->out, markdown_containers_indent(parser->containers));
    text_buffer_append_text(parser->out, "\\par}\n");
    parser->after_rule = true;
}

static bool literal_row(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
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
    size_t indent_twips = markdown_containers_indent(parser->containers);
    parser->table_state = markdown_table_native(markdown_table_columns(line), indent_twips)
                              ? MARKDOWN_TABLE_STATE_NATIVE
                              : MARKDOWN_TABLE_STATE_LITERAL;
    if (parser->table_state == MARKDOWN_TABLE_STATE_LITERAL)
    {
        while (markdown_containers_pending(parser->containers) != 0)
            markdown_containers_consume_marker(parser->containers);
        return literal_row(parser, line) && literal_row(parser, delimiter);
    }
    outer_markers(parser, 0);
    struct markdown_table_format format = {.kind = MARKDOWN_ROW_KIND_HEADER,
                                           .indent_twips = indent_twips};
    parser->outcome = markdown_table_row(parser->out, line, delimiter, format);
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool table_body(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (parser->table_state == MARKDOWN_TABLE_STATE_LITERAL)
        return literal_row(parser, line);
    struct markdown_table_format format = {.kind = MARKDOWN_ROW_KIND_BODY,
                                           .indent_twips =
                                               markdown_containers_indent(parser->containers)};
    parser->outcome = markdown_table_row(parser->out, line, parser->delimiter, format);
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
        return start_fence(parser, line, fence);
    }
    task_marker(parser, &line);
    return add_span(parser, line);
}

static bool table_candidate(struct markdown_blocks *_Nonnull parser, struct markdown_line line,
                            struct markdown_line next)
{
    if (line.next >= parser->length ||
        markdown_containers_match(parser->containers, &next) != MARKDOWN_CONTAINER_MATCH_MATCHED)
        return false;
    return markdown_table_starts(line, next);
}

static bool table_continues(struct markdown_line line)
{
    markdown_line_list(&line);
    struct markdown_line quote = line;
    return !markdown_line_blank(line) && markdown_line_heading(line) == 0 &&
           markdown_line_fence(line) == 0 && !markdown_line_thematic(line) && line.marker == 0 &&
           !markdown_line_quote(&quote);
}

static bool accept_line(struct markdown_blocks *_Nonnull parser, struct markdown_line raw,
                        struct markdown_line next)
{
    enum markdown_paragraph_state paragraph =
        parser->span_count == 0 ? MARKDOWN_PARAGRAPH_STATE_NONE : MARKDOWN_PARAGRAPH_STATE_OPEN;
    parser->outcome = markdown_containers_accept(parser->containers, raw, next, paragraph);
    if (parser->outcome != MARKDOWN_RTF_CONVERTED)
        return false;
    if (markdown_containers_event(parser->containers) == MARKDOWN_CONTAINER_EVENT_FLUSH_REQUIRED)
    {
        if (!flush(parser, false))
            return false;
        parser->outcome = markdown_containers_accept(parser->containers, raw, next,
                                                     MARKDOWN_PARAGRAPH_STATE_NONE);
    }
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool render_line(struct markdown_blocks *_Nonnull parser, struct markdown_line raw,
                        size_t *_Nonnull cursor)
{
    struct markdown_line next = markdown_line_read(parser->source, parser->length, raw.next);
    if (!accept_line(parser, raw, next))
        return false;
    struct markdown_line line = markdown_containers_line(parser->containers);
    if (markdown_line_blank(line))
    {
        if (!flush(parser, false))
            return false;
        outer_markers(parser, 0);
        return true;
    }
    size_t setext = markdown_line_setext(line);
    if (parser->span_count != 0 && setext != 0)
    {
        parser->heading = setext;
        return flush(parser, false);
    }
    if (table_candidate(parser, line, next))
    {
        if (markdown_containers_match(parser->containers, &next) !=
            MARKDOWN_CONTAINER_MATCH_MATCHED)
            return false;
        *cursor = next.next;
        return start_table(parser, line, next);
    }
    return special_line(parser, line);
}

static bool render_fenced_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (fence_closes(parser, line))
    {
        close_code(parser);
        return true;
    }
    return code_line(parser, line);
}

static bool render_next(struct markdown_blocks *_Nonnull parser, size_t *_Nonnull cursor)
{
    struct markdown_line raw = markdown_line_read(parser->source, parser->length, *cursor);
    struct markdown_line line = raw;
    *cursor = raw.next;
    if (parser->code_state != MARKDOWN_CODE_STATE_OUTSIDE)
    {
        if (markdown_containers_match(parser->containers, &line) ==
            MARKDOWN_CONTAINER_MATCH_MATCHED)
            return render_fenced_line(parser, line);
        close_code(parser);
    }
    line = raw;
    if (parser->table_state != MARKDOWN_TABLE_STATE_NONE)
    {
        if (markdown_containers_match(parser->containers, &line) ==
                MARKDOWN_CONTAINER_MATCH_MATCHED &&
            table_continues(line))
            return table_body(parser, line);
        parser->table_state = MARKDOWN_TABLE_STATE_NONE;
    }
    return render_line(parser, raw, cursor);
}

static void markdown_blocks_destroy(struct markdown_blocks *_Nonnull parser)
{
    free(parser->spans);
    markdown_containers_destroy(parser->containers);
}

enum markdown_rtf_outcome markdown_blocks_render(struct text_buffer *_Nonnull out,
                                                 const char *_Nonnull text, size_t length)
{
    struct markdown_blocks parser = {
        .out = out, .source = text, .length = length, .outcome = MARKDOWN_RTF_CONVERTED};
    parser.outcome = markdown_containers_create(&parser.containers);
    if (parser.outcome != MARKDOWN_RTF_CONVERTED)
        return parser.outcome;
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
