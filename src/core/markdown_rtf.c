#include "markdown_rtf.h"

#include "markdown_blocks.h"
#include "note_text.h"
#include "rtf_text.h"
#include "text_buffer.h"

#include <stdlib.h>
#include <string.h>

struct markdown_rtf
{
    struct text_buffer *_Nonnull buffer;
};

/* フォント 0 が本文、1 がコード。色は 1 本文・2 見出し・3 薄字・4 リンク（インラインコードも）・
 * 5 コード文字・6 コード地（rtf_palette の順）。単位は半ポイント。 */
static const char document_header_open[] = "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil ";
static const char document_header_close[] = ";}{\\f1\\fmodern Consolas;}}";
static const char document_body_start[] = "\\f0\\fs22\\cf1";
static const char document_footer[] = "}";
/* 色表の 1 色を \\redN\\greenN\\blueN; で書く。 */
static void append_color(struct text_buffer *_Nonnull out, struct rgb_color color)
{
    static const char *const names[] = {"\\red", "\\green", "\\blue"};
    const unsigned char parts[] = {color.red, color.green, color.blue};
    for (size_t part = 0; part < 3; ++part)
    {
        text_buffer_append_text(out, names[part]);
        char digits[4];
        size_t count = 0;
        unsigned value = parts[part];
        do
        {
            digits[count++] = (char)('0' + value % 10);
            value /= 10;
        } while (value > 0);
        while (count > 0)
        {
            text_buffer_append(out, &digits[--count], 1);
        }
    }
    text_buffer_append_text(out, ";");
}

static void append_header(struct text_buffer *_Nonnull out, struct rtf_palette palette,
                          const char *_Nonnull face)
{
    const struct rgb_color colors[] = {palette.text, palette.heading,   palette.muted,
                                       palette.link, palette.code_text, palette.code_background};
    /* fonttbl の face は実行時に挟む。文言ではないので ui_text には置かない（決定 5）。 */
    text_buffer_append_text(out, document_header_open);
    text_buffer_append_text(out, face);
    text_buffer_append_text(out, document_header_close);
    text_buffer_append_text(out, "{\\colortbl;");
    for (size_t index = 0; index < sizeof colors / sizeof colors[0]; ++index)
    {
        append_color(out, colors[index]);
    }
    text_buffer_append_text(out, "}");
    text_buffer_append_text(out, document_body_start);
}

/* 編集用の本文（#250）。Markdown を解釈せず、UTF-8 の生バイトを構文記号だけ escape して書く。
 * 改行は LF・CRLF・孤立 CR のそれぞれ 1 つを `\par ` 1 つにする（CRLF を 2 つにしない）。
 * 末尾に `\par ` を 1 つ足して末尾の改行を保つ（最後の段落記号は RichEdit が吸収する）。 */
static void render_plain(struct text_buffer *_Nonnull out, const char *_Nonnull text, size_t length)
{
    text_buffer_append_text(out, "\\pard\\sb0\\sa0\\sl240\\slmult1\\cf1 ");
    for (size_t index = 0; index < length; ++index)
    {
        if (text[index] != '\r' && text[index] != '\n')
        {
            rtf_text_append(out, text + index, 1);
            continue;
        }
        if (text[index] == '\r' && index + 1 < length && text[index + 1] == '\n')
        {
            ++index;
        }
        text_buffer_append_text(out, "\\par ");
    }
    text_buffer_append_text(out, "\\par ");
    text_buffer_append_text(out, document_footer);
}

/* 頭まで書いた入れ物を作る。確保に失敗すれば nullptr。 */
static struct markdown_rtf *_Nullable begin(struct rtf_palette palette, const char *_Nonnull face)
{
    struct markdown_rtf *_Nullable rtf = calloc(1, sizeof *rtf);
    if (rtf == nullptr)
    {
        return nullptr;
    }
    if (text_buffer_create(&rtf->buffer) != TEXT_BUFFER_ACCEPTED)
    {
        free(rtf);
        return nullptr;
    }
    append_header(rtf->buffer, palette, face);
    return rtf;
}

/* 書き終えた入れ物を確定して out へ渡す。失敗すれば入れ物を捨て、out は触らない。 */
static enum markdown_rtf_outcome finish(struct markdown_rtf *_Nonnull rtf,
                                        struct markdown_rtf *_Nullable *_Nonnull out)
{
    if (text_buffer_finish(rtf->buffer) != TEXT_BUFFER_ACCEPTED)
    {
        markdown_rtf_destroy(rtf);
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    *out = rtf;
    return MARKDOWN_RTF_CONVERTED;
}

enum markdown_rtf_outcome markdown_rtf_create(const struct note_text *_Nonnull text,
                                              struct rtf_palette palette, const char *_Nonnull face,
                                              struct markdown_rtf *_Nullable *_Nonnull out)
{
    struct markdown_rtf *_Nullable rtf = begin(palette, face);
    if (rtf == nullptr)
    {
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    enum markdown_rtf_outcome rendered =
        markdown_blocks_render(rtf->buffer, note_text_bytes(text), note_text_length(text));
    if (rendered != MARKDOWN_RTF_CONVERTED)
    {
        markdown_rtf_destroy(rtf);
        return rendered;
    }
    text_buffer_append_text(rtf->buffer, document_footer);
    return finish(rtf, out);
}

enum markdown_rtf_outcome markdown_rtf_empty(struct rtf_palette palette, const char *_Nonnull face,
                                             struct markdown_rtf *_Nullable *_Nonnull out)
{
    struct markdown_rtf *_Nullable rtf = begin(palette, face);
    if (rtf == nullptr)
    {
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    text_buffer_append_text(rtf->buffer, document_footer);
    return finish(rtf, out);
}

enum markdown_rtf_outcome markdown_rtf_plain(const struct note_text *_Nonnull text,
                                             struct rtf_palette palette, const char *_Nonnull face,
                                             struct markdown_rtf *_Nullable *_Nonnull out)
{
    struct markdown_rtf *_Nullable rtf = begin(palette, face);
    if (rtf == nullptr)
    {
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    }
    render_plain(rtf->buffer, note_text_bytes(text), note_text_length(text));
    return finish(rtf, out);
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
