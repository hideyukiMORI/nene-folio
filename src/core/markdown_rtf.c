#include "markdown_rtf.h"

#include "markdown_line_kind.h"
#include "markdown_style.h"
#include "note_text.h"
#include "text_buffer.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct markdown_rtf
{
    struct text_buffer *_Nonnull buffer;
};

/* フォント 0 が本文、1 がコード。単位は半ポイント。 */
static const char document_header[] =
    "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil Yu Gothic UI;}{\\f1\\fmodern Consolas;}}\\f0\\fs20";
static const char document_footer[] = "}";
static const int heading_sizes[] = {36, 30, 26, 22, 20, 20};
constexpr size_t heading_max = 6;

static bool is_digit(char character)
{
    return character >= '0' && character <= '9';
}

static size_t leading(const char *_Nonnull line, size_t length, char character)
{
    size_t count = 0;
    while (count < length && line[count] == character)
    {
        count += 1;
    }
    return count;
}

/* 数字の並びの後に ". " があれば、その全体の長さを返す。無ければ 0。 */
static size_t ordered_marker(const char *_Nonnull line, size_t length)
{
    size_t digits = 0;
    while (digits < length && is_digit(line[digits]))
    {
        digits += 1;
    }
    if (digits == 0 || digits + 2 > length || line[digits] != '.' || line[digits + 1] != ' ')
    {
        return 0;
    }
    return digits + 2;
}

static bool bullet_marker(const char *_Nonnull line, size_t length)
{
    if (length < 2 || line[1] != ' ')
    {
        return false;
    }
    return line[0] == '-' || line[0] == '*' || line[0] == '+';
}

static bool blank(const char *_Nonnull line, size_t length)
{
    for (size_t index = 0; index < length; ++index)
    {
        if (line[index] != ' ' && line[index] != '\t' && line[index] != '\r')
        {
            return false;
        }
    }
    return true;
}

static enum markdown_line_kind classify(const char *_Nonnull line, size_t length)
{
    if (blank(line, length))
    {
        return MARKDOWN_LINE_BLANK;
    }
    size_t hashes = leading(line, length, '#');
    if (hashes > 0 && hashes <= heading_max && hashes < length && line[hashes] == ' ')
    {
        return MARKDOWN_LINE_HEADING;
    }
    if (leading(line, length, '`') >= 3)
    {
        return MARKDOWN_LINE_FENCE;
    }
    if (line[0] == '>')
    {
        return MARKDOWN_LINE_QUOTE;
    }
    if (bullet_marker(line, length))
    {
        return MARKDOWN_LINE_BULLET;
    }
    return ordered_marker(line, length) > 0 ? MARKDOWN_LINE_ORDERED : MARKDOWN_LINE_TEXT;
}

/* UTF-16 の 1 単位を \uN? で書く。\u は 16 bit 符号付きなので 0x8000 以上は負数にする。 */
static void append_unit(struct text_buffer *_Nonnull out, uint32_t unit)
{
    long value = unit >= 0x8000 ? (long)unit - 0x10000 : (long)unit;
    char text[16];
    size_t written = 0;
    text[written++] = '\\';
    text[written++] = 'u';
    if (value < 0)
    {
        text[written++] = '-';
        value = -value;
    }
    char digits[8];
    size_t digit_count = 0;
    do
    {
        digits[digit_count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value > 0);
    while (digit_count > 0)
    {
        text[written++] = digits[--digit_count];
    }
    text[written++] = '?';
    text_buffer_append(out, text, written);
}

/* 1 文字を RTF として書く。制御文字は \ { } を守り、非 ASCII は \uN? にする。 */
static void append_code_point(struct text_buffer *_Nonnull out, uint32_t code_point)
{
    if (code_point == '\\' || code_point == '{' || code_point == '}')
    {
        char escaped[2] = {'\\', (char)code_point};
        text_buffer_append(out, escaped, 2);
        return;
    }
    if (code_point < 0x80)
    {
        char raw = (char)code_point;
        text_buffer_append(out, &raw, 1);
        return;
    }
    if (code_point < 0x10000)
    {
        append_unit(out, code_point);
        return;
    }
    append_unit(out, 0xD800 + ((code_point - 0x10000) >> 10));
    append_unit(out, 0xDC00 + ((code_point - 0x10000) & 0x3FF));
}

/* text のバイト列を UTF-8 として読みながら書く。装飾は解釈しない。 */
static void append_plain(struct text_buffer *_Nonnull out, const char *_Nonnull text, size_t length)
{
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(text + index, length - index, &code_point);
        if (consumed == 0)
        {
            index += 1; /* note_text が検証済みなので来ない。来ても止まらない */
            continue;
        }
        if (code_point != '\r')
        {
            append_code_point(out, code_point);
        }
        index += consumed;
    }
}

/* 閉じるバッククォートまでをコード扱いで書く。閉じが無ければ 0 を返して呼び出し側に任せる。 */
static size_t render_code_span(struct text_buffer *_Nonnull out, const char *_Nonnull text,
                               size_t length)
{
    for (size_t end = 1; end < length; ++end)
    {
        if (text[end] == '`')
        {
            text_buffer_append_text(out, "{\\f1 ");
            append_plain(out, text + 1, end - 1);
            text_buffer_append_text(out, "}");
            return end + 1;
        }
    }
    return 0;
}

/* [文字](url) を下線の文字として書く。形が違えば 0。 */
static size_t render_link(struct text_buffer *_Nonnull out, const char *_Nonnull text,
                          size_t length)
{
    size_t close = 1;
    while (close < length && text[close] != ']')
    {
        close += 1;
    }
    if (close + 1 >= length || text[close + 1] != '(')
    {
        return 0;
    }
    size_t end = close + 2;
    while (end < length && text[end] != ')')
    {
        end += 1;
    }
    if (end >= length)
    {
        return 0;
    }
    text_buffer_append_text(out, "{\\ul ");
    append_plain(out, text + 1, close - 1);
    text_buffer_append_text(out, "}");
    return end + 1;
}

static bool word_character(char character)
{
    return is_digit(character) || (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') || ((unsigned char)character >= 0x80);
}

/* 強調の記号（** __ * _）を切り替える。記号なら消費した長さ、違えば 0。
 * 語の中の _（snake_case）は強調ではなく文字として扱う。 */
static size_t render_emphasis(struct text_buffer *_Nonnull out, const char *_Nonnull text,
                              size_t length, struct markdown_style *_Nonnull style)
{
    if (text[0] != '*' && text[0] != '_')
    {
        return 0;
    }
    if (text[0] == '_' && word_character(style->previous) && length >= 2 && word_character(text[1]))
    {
        return 0;
    }
    if (length >= 2 && text[1] == text[0])
    {
        style->bold = !style->bold;
        text_buffer_append_text(out, style->bold ? "\\b " : "\\b0 ");
        return 2;
    }
    style->italic = !style->italic;
    text_buffer_append_text(out, style->italic ? "\\i " : "\\i0 ");
    return 1;
}

/* バックスラッシュで打ち消せるのは ASCII の記号だけ（それ以外の \ は文字）。 */
static bool punctuation(char character)
{
    return (character > ' ' && character < '0') || (character > '9' && character < 'A') ||
           (character > 'Z' && character < 'a') || (character > 'z' && character < 0x7F);
}

/* 行内の 1 要素を書き、消費した長さを返す。 */
static size_t render_inline_at(struct text_buffer *_Nonnull out, const char *_Nonnull text,
                               size_t length, struct markdown_style *_Nonnull style)
{
    size_t consumed = 0;
    if (text[0] == '`')
    {
        consumed = render_code_span(out, text, length);
    }
    else if (text[0] == '[')
    {
        consumed = render_link(out, text, length);
    }
    else if (text[0] == '\\' && length >= 2 && punctuation(text[1]))
    {
        append_plain(out, text + 1, 1);
        consumed = 2;
    }
    else
    {
        consumed = render_emphasis(out, text, length, style);
    }
    if (consumed > 0)
    {
        return consumed;
    }
    uint32_t code_point = 0;
    size_t width = utf8_text_decode(text, length, &code_point);
    append_plain(out, text, width == 0 ? 1 : width);
    return width == 0 ? 1 : width;
}

static void render_inline(struct text_buffer *_Nonnull out, const char *_Nonnull text,
                          size_t length)
{
    struct markdown_style style = {.bold = false, .italic = false, .previous = ' '};
    for (size_t index = 0; index < length;)
    {
        index += render_inline_at(out, text + index, length - index, &style);
        style.previous = text[index - 1];
    }
    if (style.bold)
    {
        text_buffer_append_text(out, "\\b0 ");
    }
    if (style.italic)
    {
        text_buffer_append_text(out, "\\i0 ");
    }
}

static void render_heading(struct text_buffer *_Nonnull out, const char *_Nonnull line,
                           size_t length)
{
    size_t level = leading(line, length, '#');
    char size[8];
    int value = heading_sizes[level - 1];
    size[0] = (char)('0' + value / 10);
    size[1] = (char)('0' + value % 10);
    size[2] = '\0';
    text_buffer_append_text(out, "\\pard\\sb240\\sa120\\b\\fs");
    text_buffer_append_text(out, size);
    text_buffer_append_text(out, " ");
    render_inline(out, line + level + 1, length - level - 1);
    text_buffer_append_text(out, "\\b0\\fs20\\par\n");
}

static void render_block(struct text_buffer *_Nonnull out, enum markdown_line_kind kind,
                         const char *_Nonnull line, size_t length)
{
    switch (kind)
    {
    case MARKDOWN_LINE_HEADING:
        render_heading(out, line, length);
        return;
    case MARKDOWN_LINE_QUOTE:
    {
        size_t skip = length >= 2 && line[1] == ' ' ? 2 : 1;
        text_buffer_append_text(out, "\\pard\\li600\\sa60\\i ");
        render_inline(out, line + skip, length - skip);
        text_buffer_append_text(out, "\\i0\\par\n");
        return;
    }
    case MARKDOWN_LINE_BULLET:
        text_buffer_append_text(out, "\\pard\\li500\\fi-250\\sa60\\bullet\\tab ");
        render_inline(out, line + 2, length - 2);
        text_buffer_append_text(out, "\\par\n");
        return;
    case MARKDOWN_LINE_ORDERED:
    {
        size_t marker = ordered_marker(line, length);
        text_buffer_append_text(out, "\\pard\\li500\\fi-250\\sa60 ");
        append_plain(out, line, marker - 1);
        text_buffer_append_text(out, "\\tab ");
        render_inline(out, line + marker, length - marker);
        text_buffer_append_text(out, "\\par\n");
        return;
    }
    case MARKDOWN_LINE_BLANK:
    case MARKDOWN_LINE_FENCE:
    case MARKDOWN_LINE_TEXT:
        return;
    }
}

/* 段落を閉じる。開いていなければ何もしない。 */
static void close_paragraph(struct text_buffer *_Nonnull out, bool *_Nonnull open)
{
    if (*open)
    {
        text_buffer_append_text(out, "\\par\n");
        *open = false;
    }
}

/* 1 行を文書の状態に応じて書く。in_code と paragraph_open は行をまたぐ状態。 */
static void render_line(struct text_buffer *_Nonnull out, const char *_Nonnull line, size_t length,
                        bool *_Nonnull state)
{
    bool *_Nonnull in_code = &state[0];
    bool *_Nonnull paragraph_open = &state[1];
    enum markdown_line_kind kind = classify(line, length);
    if (*in_code)
    {
        if (kind == MARKDOWN_LINE_FENCE)
        {
            *in_code = false;
            return;
        }
        text_buffer_append_text(out, "\\pard\\li300\\f1\\fs18 ");
        append_plain(out, line, length);
        text_buffer_append_text(out, "\\f0\\fs20\\par\n");
        return;
    }
    if (kind == MARKDOWN_LINE_TEXT)
    {
        text_buffer_append_text(out, *paragraph_open ? " " : "\\pard\\sa120 ");
        render_inline(out, line, length);
        *paragraph_open = true;
        return;
    }
    close_paragraph(out, paragraph_open);
    if (kind == MARKDOWN_LINE_FENCE)
    {
        *in_code = true;
        return;
    }
    render_block(out, kind, line, length);
}

static void render_document(struct text_buffer *_Nonnull out, const char *_Nonnull text,
                            size_t length)
{
    text_buffer_append_text(out, document_header);
    bool state[2] = {false, false};
    size_t start = 0;
    for (size_t index = 0; index <= length; ++index)
    {
        if (index < length && text[index] != '\n')
        {
            continue;
        }
        if (index == length && start == length)
        {
            break; /* 末尾の改行の後に行は無い */
        }
        size_t line_length = index - start;
        if (line_length > 0 && text[start + line_length - 1] == '\r')
        {
            line_length -= 1;
        }
        render_line(out, text + start, line_length, state);
        start = index + 1;
    }
    close_paragraph(out, &state[1]);
    text_buffer_append_text(out, document_footer);
}

static enum markdown_rtf_outcome build(struct markdown_rtf *_Nullable *_Nonnull out,
                                       const char *_Nonnull text, size_t length)
{
    struct markdown_rtf *_Nullable rtf = calloc(1, sizeof *rtf);
    if (rtf == nullptr)
    {
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    if (text_buffer_create(&rtf->buffer) != TEXT_BUFFER_ACCEPTED)
    {
        free(rtf);
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    render_document(rtf->buffer, text, length);
    if (text_buffer_finish(rtf->buffer) != TEXT_BUFFER_ACCEPTED)
    {
        markdown_rtf_destroy(rtf);
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    *out = rtf;
    return MARKDOWN_RTF_CONVERTED;
}

enum markdown_rtf_outcome markdown_rtf_create(const struct note_text *_Nonnull text,
                                              struct markdown_rtf *_Nullable *_Nonnull out)
{
    return build(out, note_text_bytes(text), note_text_length(text));
}

enum markdown_rtf_outcome markdown_rtf_empty(struct markdown_rtf *_Nullable *_Nonnull out)
{
    return build(out, "", 0);
}

const char *_Nonnull markdown_rtf_text(const struct markdown_rtf *_Nonnull rtf)
{
    return text_buffer_bytes(rtf->buffer);
}

size_t markdown_rtf_length(const struct markdown_rtf *_Nonnull rtf)
{
    return text_buffer_length(rtf->buffer);
}

void markdown_rtf_destroy(struct markdown_rtf *_Nullable rtf)
{
    if (rtf == nullptr)
    {
        return;
    }
    text_buffer_destroy(rtf->buffer);
    free(rtf);
}
