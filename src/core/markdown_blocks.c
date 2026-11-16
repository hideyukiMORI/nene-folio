#include "markdown_blocks.h"
#include "code_copy_target.h"
#include "code_syntax.h"
#include "diagram_limits.h"
#include "markdown_code_state.h"
#include "markdown_containers.h"
#include "markdown_inline_tokens.h"
#include "markdown_media.h"
#include "markdown_table.h"
#include "markdown_table_state.h"
#include "markdown_task_kind.h"
#include "note_metadata.h"
#include "note_text.h"
#include "rtf_text.h"
#include "text_buffer.h"
#include "ui_text.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct markdown_blocks
{
    struct text_buffer *_Nonnull out;
    struct markdown_media *_Nonnull media;
    const char *_Nonnull source;
    size_t length;
    size_t next; /* 処理中の行の次の行の開始byte。fenceの先読みが使う。 */
    enum folio_language language;
    enum folio_contrast contrast;
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
    struct code_syntax *_Nullable syntax;
    bool diagram;
    size_t diagram_slot;
    bool copyable; /* このfenceの原文をcode_slotへ集めている（ADR0098）。 */
    bool tabbed;   /* 直前が操作の札のある見出し行。最初のコード行が上の間隔を受け持つ。 */
    size_t code_slot;
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
    text_buffer_append_text(parser->out, "\\sl240\\slmult1");
    text_buffer_append_text(parser->out, parser->paragraph_quote ? "\\cf3 " : "\\cf1 ");
    if (marker)
        marker_label(parser, parser->task);
}

static bool flush(struct markdown_blocks *_Nonnull parser)
{
    if (parser->span_count == 0)
        return true;
    outer_markers(parser, 1);
    paragraph_header(parser);
    struct markdown_view view = {
        .spans = parser->spans, .count = parser->span_count, .length = parser->inline_length};
    parser->outcome =
        markdown_inline_tokens_render((struct markdown_render){parser->out, parser->media}, view);
    if (parser->outcome != MARKDOWN_RTF_CONVERTED)
        return false;
    if (parser->heading != 0)
        text_buffer_append_text(parser->out, "\\b0\\cf1\\fs22");
    else if (parser->paragraph_quote)
        text_buffer_append_text(parser->out, "\\cf1");
    text_buffer_append_text(parser->out, "\\par\n");
    parser->span_count = parser->inline_length = parser->heading = 0;
    parser->task = MARKDOWN_TASK_KIND_NONE;
    return true;
}

static bool close_code(struct markdown_blocks *_Nonnull parser)
{
    if (parser->diagram)
    {
        if (markdown_media_end_diagram(parser->media, parser->diagram_slot) != IMAGE_READY)
            parser->outcome = MARKDOWN_RTF_OUT_OF_MEMORY;
        parser->diagram = false;
        parser->code_state = MARKDOWN_CODE_STATE_OUTSIDE;
        return parser->outcome == MARKDOWN_RTF_CONVERTED;
    }
    if (parser->code_state == MARKDOWN_CODE_STATE_PARAGRAPH)
        text_buffer_append_text(parser->out, "\\sa120\\par\n");
    text_buffer_append_text(parser->out, "}");
    parser->code_state = MARKDOWN_CODE_STATE_OUTSIDE;
    code_syntax_destroy(parser->syntax);
    parser->syntax = nullptr;
    if (parser->copyable &&
        markdown_media_end_code(parser->media, parser->code_slot) != IMAGE_READY)
        parser->outcome = MARKDOWN_RTF_OUT_OF_MEMORY;
    parser->copyable = false;
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool fence_closes(const struct markdown_blocks *_Nonnull parser, struct markdown_line line)
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

/* fenceの字下げぶんを除いた、描画するコード行。描画・図の収集・コピー原文の先読みが共有する。 */
static bool code_content(const struct markdown_blocks *_Nonnull parser,
                         struct markdown_line *_Nonnull line)
{
    size_t removed = line->indent < parser->fence_indent ? line->indent : parser->fence_indent;
    return markdown_line_skip(line, removed);
}

/* 開いたfenceが描くコード行を結合した長さ（行間のLFを含み、末尾の改行は含まない）。
 * 終わりの判定は render_next と同じ述語（container の一致と閉じfence）だけを使い、何も書かない。
 * 上限の判定と見出し行の要否を、コード行を書く前に決めるための先読み（ADR0098）。 */
static size_t code_bytes(const struct markdown_blocks *_Nonnull parser)
{
    size_t bytes = 0;
    size_t lines = 0;
    size_t cursor = parser->next;
    while (cursor < parser->length)
    {
        struct markdown_line line = markdown_line_read(parser->source, parser->length, cursor);
        cursor = line.next;
        if (markdown_containers_match(parser->containers, &line) !=
                MARKDOWN_CONTAINER_MATCH_MATCHED ||
            fence_closes(parser, line))
            break;
        if (!code_content(parser, &line))
            return 0; /* 描画がINVALID_INPUTで止まる行。操作は出さない。 */
        bytes += line.padding + (line.length - line.prefix) + (lines++ == 0 ? 0 : 1);
    }
    return bytes;
}

static bool code_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (!code_content(parser, &line))
    {
        parser->outcome = MARKDOWN_RTF_INVALID_INPUT;
        return false;
    }
    if (parser->diagram)
    {
        markdown_media_diagram_line(parser->media, parser->diagram_slot, line);
        return true;
    }
    if (parser->copyable)
        markdown_media_code_line(parser->media, parser->code_slot, line);
    if (parser->code_state == MARKDOWN_CODE_STATE_PARAGRAPH)
        text_buffer_append_text(parser->out, "\\par\n");
    text_buffer_append_text(parser->out, "\\pard\\li");
    rtf_text_number(parser->out, parser->code_indent + 180);
    text_buffer_append_text(parser->out, "\\ri180\\sb");
    text_buffer_append_text(parser->out, parser->tabbed ? "100" : code_spacing(parser->code_state));
    parser->tabbed = false;
    text_buffer_append_text(parser->out, "\\sa0\\sl240\\slmult1 ");
    for (size_t index = 0; index < line.padding; ++index)
        text_buffer_append_text(parser->out, " ");
    if (parser->syntax == nullptr)
        return false;
    code_syntax_append(parser->syntax, line.bytes + line.prefix, line.length - line.prefix);
    parser->code_state = MARKDOWN_CODE_STATE_PARAGRAPH;
    return true;
}

/* 文書の中の札は、言語ラベルと同じ控えめな文字（code_comment・色表の 9）で、地を塗らない。
 * 通常の配色では UI がこの上へボタンを描くので（ADR0098 第3版）、これは下に透けて見えた
 * ときと装飾が出ないときの見え方で、塗った札より目立たず、読めるリンクのままになる。
 * コード面との比は全配色で4.5:1以上。高コントラストは本文の文字色（色表の 1）と下線。
 * 通常の配色では下線の書式を付けない（RichEdit は friendly name のリンクへ自前の下線を
 * 文字色で引くので、表示には下線が残る）。 */
static const char *_Nonnull action_format(enum folio_contrast contrast)
{
    switch (contrast)
    {
    case FOLIO_CONTRAST_STANDARD:
        return "\"}}{\\fldrslt{\\f0\\fs18\\cf9\\ul0 ";
    case FOLIO_CONTRAST_HIGH:
        return "\"}}{\\fldrslt{\\f0\\fs18\\cf1\\ul ";
    }
    return "\"}}{\\fldrslt{\\f0\\fs18\\cf1\\ul ";
}

/* 札の文言の前の、改行しない空白の数。通常の配色では UI が札の上へ絵と文言のボタンを描くので
 * （ADR0098 第3版）、その幅（絵・間・左右の余白で約27pt）を文書の側で確保する。飾らない
 * 高コントラストでは短い札のまま。後ろは 4 と 2。 */
static size_t action_lead(enum folio_contrast contrast)
{
    switch (contrast)
    {
    case FOLIO_CONTRAST_STANDARD:
        return 12;
    case FOLIO_CONTRAST_HIGH:
        return 2;
    }
    return 2;
}

/* コピーの操作（ADR0098 第2版）。宛先は内部の識別子。リンクの区間は札全体で、文言の前後の
 * 改行しない空白も含める（塗りと押せる範囲を同じにし、折返しで割れないようにする）。 */
static void code_copy_action(struct markdown_blocks *_Nonnull parser)
{
    const char *label = ui_text_line(UI_TEXT_CODE_COPY, parser->language);
    size_t lead = action_lead(parser->contrast);
    text_buffer_append_text(parser->out, "{\\field{\\*\\fldinst{HYPERLINK \"");
    code_copy_target_append(parser->out, parser->code_slot);
    text_buffer_append_text(parser->out, action_format(parser->contrast));
    for (size_t index = 0; index < lead; ++index)
        text_buffer_append_text(parser->out, "\\~");
    rtf_text_append(parser->out, label, strlen(label));
    text_buffer_append_text(parser->out, lead == 2 ? "\\~\\~}}}" : "\\~\\~\\~\\~}}}");
}

/* 見出し行: 言語ラベル（あれば）とコピーの操作（原文を集めるブロックだけ）。どちらも無ければ
 * 何も書かない（未指定の空fenceは従来どおり）。操作のある行は段落を右寄せにし、右の余白を
 * 0、前の間隔を2pt、後を3pt、行送りを固定の13ptにして、札をブロックの右上の角へ小さく付ける。
 * RichEdit は文字の地を段落の前後の間隔まで塗るので、札の高さは 2+13+3 の18ptで、長い
 * ラベルが折り返しても行ごとの高さが変わらない。札の下からコードまでの5ptは最初のコード行が
 * 持つ。幅や倍率は追わない（ADR0098 第2版）。 */
static void code_header(struct markdown_blocks *_Nonnull parser, struct markdown_span word)
{
    if (word.length == 0 && !parser->copyable)
        return;
    text_buffer_append_text(parser->out, parser->copyable ? "\\pard\\qr\\li" : "\\pard\\li");
    rtf_text_number(parser->out, parser->code_indent + 180);
    text_buffer_append_text(parser->out, parser->copyable
                                             ? "\\ri0\\sb40\\sa60\\sl-260\\slmult0 "
                                             : "\\ri180\\sb120\\sa80\\sl240\\slmult1 ");
    parser->tabbed = parser->copyable;
    if (word.length != 0)
    {
        text_buffer_append_text(parser->out, "{\\f0\\fs18\\cf9 ");
        rtf_text_append(parser->out, word.bytes, word.length);
        text_buffer_append_text(parser->out, "}");
    }
    /* ラベルと操作の間は全角幅の空白1つ（約9pt）で離す。塗らないよう、リンクの外に置く。 */
    if (word.length != 0 && parser->copyable)
        text_buffer_append_text(parser->out, "{\\f0\\fs18\\cf9 \\u8195?}");
    if (parser->copyable)
        code_copy_action(parser);
    parser->code_state = MARKDOWN_CODE_STATE_PARAGRAPH;
}

/* 集める原文が空でなく上限内のブロックだけ、コピー用のslotを取る。 */
static bool begin_copy(struct markdown_blocks *_Nonnull parser)
{
    size_t bytes = code_bytes(parser);
    if (bytes == 0)
        return true;
    enum image_outcome begun = markdown_media_begin_code(parser->media, bytes, &parser->code_slot);
    parser->copyable = begun == IMAGE_READY;
    if (begun == IMAGE_OUT_OF_MEMORY)
        parser->outcome = MARKDOWN_RTF_OUT_OF_MEMORY;
    return begun != IMAGE_OUT_OF_MEMORY;
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
    size_t info = line.content + length;
    struct markdown_span word = code_language_word(line.bytes + info, line.length - info);
    if (word.length == 7 && memcmp(word.bytes, "mermaid", 7) == 0)
    {
        parser->diagram = true;
        parser->diagram_slot = diagram_none;
        enum image_outcome result =
            markdown_media_begin_diagram(parser->media, &parser->diagram_slot);
        if (result == IMAGE_OUT_OF_MEMORY || result == IMAGE_FAILED)
            return false;
        text_buffer_append_text(parser->out, "{\\pard\\sb120\\sa120\\sl240\\slmult1\\highlight0");
        indent(parser->out, parser->code_indent);
        text_buffer_append_text(parser->out, " ");
        if (result == IMAGE_READY)
            markdown_media_placeholder(parser->out, parser->diagram_slot);
        text_buffer_append_text(parser->out, "\\par}\n");
        return true;
    }
    enum code_language language = code_language_parse(line.bytes + info, line.length - info);
    parser->outcome = code_syntax_create(language, parser->out, &parser->syntax);
    if (parser->outcome != MARKDOWN_RTF_CONVERTED)
        return false;
    if (!begin_copy(parser))
        return false;
    text_buffer_append_text(parser->out, "{\\f1\\fs20\\cf5\\highlight6");
    code_header(parser, word);
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
    if (!flush(parser))
        return false;
    parser->heading = level;
    return add_span(parser, heading_content(line, level)) && flush(parser);
}

static void rule(struct markdown_blocks *_Nonnull parser)
{
    outer_markers(parser, 0);
    /* RichEdit extends the highlight into paragraph spacing. Keep the gaps uncolored. */
    static const char gap[] = "{\\pard\\sb0\\sa0\\sl-240\\slmult0\\fs2\\highlight0\\par}\n";
    text_buffer_append_text(parser->out, gap);
    text_buffer_append_text(parser->out, "{\\pard\\sb0\\sa0\\sl-20\\slmult0\\fs2\\highlight3");
    indent(parser->out, markdown_containers_indent(parser->containers));
    text_buffer_append_text(parser->out, "\\par}\n");
    text_buffer_append_text(parser->out, gap);
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
    if (!flush(parser))
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
    parser->outcome = markdown_table_row((struct markdown_render){parser->out, parser->media}, line,
                                         delimiter, format);
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool table_body(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    if (parser->table_state == MARKDOWN_TABLE_STATE_LITERAL)
        return literal_row(parser, line);
    struct markdown_table_format format = {.kind = MARKDOWN_ROW_KIND_BODY,
                                           .indent_twips =
                                               markdown_containers_indent(parser->containers)};
    parser->outcome = markdown_table_row((struct markdown_render){parser->out, parser->media}, line,
                                         parser->delimiter, format);
    return parser->outcome == MARKDOWN_RTF_CONVERTED;
}

static bool special_line(struct markdown_blocks *_Nonnull parser, struct markdown_line line)
{
    size_t level = markdown_line_heading(line);
    if (level != 0)
        return heading_line(parser, line, level);
    if (markdown_line_thematic(line))
    {
        if (!flush(parser))
            return false;
        rule(parser);
        return true;
    }
    size_t fence = markdown_line_fence(line);
    if (fence != 0)
    {
        if (!flush(parser))
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
        if (!flush(parser))
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
        if (!flush(parser))
            return false;
        outer_markers(parser, 0);
        return true;
    }
    size_t setext = markdown_line_setext(line);
    if (parser->span_count != 0 && setext != 0)
    {
        parser->heading = setext;
        return flush(parser);
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
        return close_code(parser);
    return code_line(parser, line);
}

static bool render_next(struct markdown_blocks *_Nonnull parser, size_t *_Nonnull cursor)
{
    struct markdown_line raw = markdown_line_read(parser->source, parser->length, *cursor);
    struct markdown_line line = raw;
    *cursor = raw.next;
    parser->next = raw.next;
    if (parser->code_state != MARKDOWN_CODE_STATE_OUTSIDE)
    {
        if (markdown_containers_match(parser->containers, &line) ==
            MARKDOWN_CONTAINER_MATCH_MATCHED)
            return render_fenced_line(parser, line);
        if (!close_code(parser))
            return false;
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
    code_syntax_destroy(parser->syntax);
    free(parser->spans);
    markdown_containers_destroy(parser->containers);
}

enum markdown_rtf_outcome markdown_blocks_render(struct markdown_render render,
                                                 const struct note_text *_Nonnull text,
                                                 enum folio_language language,
                                                 enum folio_contrast contrast)
{
    size_t length = note_text_length(text);
    struct markdown_blocks parser = {.out = render.text,
                                     .media = render.media,
                                     .source = note_text_bytes(text),
                                     .length = length,
                                     .language = language,
                                     .contrast = contrast,
                                     .outcome = MARKDOWN_RTF_CONVERTED};
    parser.outcome = markdown_containers_create(&parser.containers);
    if (parser.outcome != MARKDOWN_RTF_CONVERTED)
        return parser.outcome;
    size_t cursor = note_metadata_body(note_text_metadata(text));
    bool accepted = true;
    while (cursor < length && accepted)
        accepted = render_next(&parser, &cursor);
    if (parser.code_state != MARKDOWN_CODE_STATE_OUTSIDE)
        accepted = close_code(&parser) && accepted;
    if (accepted)
        accepted = flush(&parser);
    markdown_blocks_destroy(&parser);
    if (!accepted && parser.outcome == MARKDOWN_RTF_CONVERTED)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    return parser.outcome;
}
