#include "allocation_probe.h"
#include "folio_language.h"
#include "markdown_inline_tokens.h"
#include "markdown_line.h"
#include "markdown_rtf.h"
#include "markdown_table.h"
#include "note_text.h"
#include "rtf_palette.h"
#include "text_buffer.h"
#include "ui_font.h"
#include "unit_tests.h"

#include <stdio.h>
#include <string.h>

static const char header[] =
    "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil Noto Sans JP;}{\\f1\\fmodern Consolas;}}"
    "{\\colortbl;\\red36\\green19\\blue24;\\red44\\green0\\blue30;\\red106\\green96\\blue92;"
    "\\red178\\green60\\blue18;\\red237\\green225\\blue232;\\red44\\green0\\blue30;}"
    "\\f0\\fs22\\cf1";

/* markdown を変換し、文書の頭と末尾を除いた本体が expected と同じか。 */
static void expect_body(const char *_Nonnull markdown, const char *_Nonnull expected)
{
    struct note_text *text = nullptr;
    require(note_text_create(markdown, strlen(markdown), &text) == NOTE_TEXT_ACCEPTED, markdown);
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT),
                                ui_font_face(FOLIO_LANGUAGE_JA), &rtf) == MARKDOWN_RTF_CONVERTED,
            "convert");
    const char *whole = markdown_rtf_text(rtf);
    size_t length = markdown_rtf_length(rtf);
    size_t head = strlen(header);
    require(length >= head + 1 && memcmp(whole, header, head) == 0 && whole[length - 1] == '}',
            "document frame");
    char body[2048];
    size_t body_length = length - head - 1;
    require(body_length < sizeof body, "body fits");
    memcpy(body, whole + head, body_length);
    body[body_length] = '\0';
    if (!same_text(body, expected))
    {
        fprintf(stderr, "  markdown: %s\n", markdown);
        require(false, "rtf body");
    }
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void verify_paragraphs(void)
{
    expect_body("one\rtwo", "\\pard\\sa80\\sl240\\slmult1\\cf1 one two\\par\n");
    expect_body("", "");
    expect_body("hello", "\\pard\\sa80\\sl240\\slmult1\\cf1 hello\\par\n");
    expect_body("one\ntwo\n\nthree\n", "\\pard\\sa80\\sl240\\slmult1\\cf1 one "
                                       "two\\par\n\\pard\\sa80\\sl240\\slmult1\\cf1 three\\par\n");
    expect_body("a\r\nb\r\n", "\\pard\\sa80\\sl240\\slmult1\\cf1 a b\\par\n");
    expect_body("  \n\t\n", "");
    expect_body("x \\ { }", "\\pard\\sa80\\sl240\\slmult1\\cf1 x \\\\ \\{ \\}\\par\n");
}

static void verify_unicode(void)
{
    expect_body("\xE6\x97\xA5\xE6\x9C\xAC",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 \xE6\x97\xA5\xE6\x9C\xAC\\par\n");
    expect_body("\xC3\xA9", "\\pard\\sa80\\sl240\\slmult1\\cf1 \xC3\xA9\\par\n");
    expect_body("\xEF\xBF\xBD", "\\pard\\sa80\\sl240\\slmult1\\cf1 \xEF\xBF\xBD\\par\n");
    expect_body("\xF0\x9F\x98\x80", "\\pard\\sa80\\sl240\\slmult1\\cf1 \xF0\x9F\x98\x80\\par\n");
    expect_body("A\xEF\xBF\xBD\vB", "\\pard\\sa80\\sl240\\slmult1\\cf1 A\xEF\xBF\xBD\vB\\par\n");
    expect_body(
        "`{\xEF\xBF\xBD}\\`",
        "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\f1\\fs20\\cf4 \\{\xEF\xBF\xBD\\}\\\\}\\par\n");
}

static void verify_headings(void)
{
    expect_body("# Title", "\\pard\\sb200\\sa80\\b\\cf2\\fs44 Title\\b0\\cf1\\fs22\\par\n");
    expect_body("## Sub", "\\pard\\sb200\\sa80\\b\\cf2\\fs34 Sub\\b0\\cf1\\fs22\\par\n");
    expect_body("### Third", "\\pard\\sb200\\sa80\\b\\cf2\\fs28 Third\\b0\\cf1\\fs22\\par\n");
    expect_body("#### Four", "\\pard\\sb160\\sa60\\b0\\cf2\\fs22 Four\\b0\\cf1\\fs22\\par\n");
    expect_body("##### Five", "\\pard\\sb160\\sa60\\b0\\cf2\\fs22 Five\\b0\\cf1\\fs22\\par\n");
    expect_body("###### Six", "\\pard\\sb160\\sa60\\b0\\cf2\\fs22 Six\\b0\\cf1\\fs22\\par\n");
    expect_body("####### Seven", "\\pard\\sa80\\sl240\\slmult1\\cf1 ####### Seven\\par\n");
    expect_body("#NoSpace", "\\pard\\sa80\\sl240\\slmult1\\cf1 #NoSpace\\par\n");
    expect_body("##", "\\pard\\sa80\\sl240\\slmult1\\cf1 ##\\par\n");
    expect_body("# *Em*", "\\pard\\sb200\\sa80\\b\\cf2\\fs44 {\\i Em}\\b0\\cf1\\fs22\\par\n");
}

static void verify_emphasis(void)
{
    expect_body("**bold** text", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\b bold} text\\par\n");
    expect_body("__bold__", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\b bold}\\par\n");
    expect_body("*em* and _em_", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\i em} and {\\i em}\\par\n");
    expect_body("snake_case_name", "\\pard\\sa80\\sl240\\slmult1\\cf1 snake_case_name\\par\n");
    expect_body("**unclosed", "\\pard\\sa80\\sl240\\slmult1\\cf1 **unclosed\\par\n");
    expect_body("*open", "\\pard\\sa80\\sl240\\slmult1\\cf1 *open\\par\n");
    expect_body("\\*literal\\*", "\\pard\\sa80\\sl240\\slmult1\\cf1 *literal*\\par\n");
    expect_body("trailing\\", "\\pard\\sa80\\sl240\\slmult1\\cf1 trailing\\\\\\par\n");
    expect_body("A_B 1_2 x_9 Z_z", "\\pard\\sa80\\sl240\\slmult1\\cf1 A_B 1_2 x_9 Z_z\\par\n");
    expect_body("\\~ \\@ \\[x] \\{ \\a",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ~ @ [x] \\{ \\\\a\\par\n");
    expect_body("a\rb", "\\pard\\sa80\\sl240\\slmult1\\cf1 a b\\par\n");
}

static void verify_code(void)
{
    expect_body("use `x_y` here", "\\pard\\sa80\\sl240\\slmult1\\cf1 use "
                                  "{\\f1\\fs20\\cf4 x_y} here\\par\n");
    expect_body("`unclosed", "\\pard\\sa80\\sl240\\slmult1\\cf1 `unclosed\\par\n");
    expect_body(
        "```\nint a;\n\n  b\n```\nafter",
        "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb120\\sa0\\sl240\\slmult1 int a;\\par\n"
        "\\pard\\li180\\ri180\\sb0\\sa0\\sl240\\slmult1 \\par\n"
        "\\pard\\li180\\ri180\\sb0\\sa0\\sl240\\slmult1   b\\sa120\\par\n}"
        "\\pard\\sa80\\sl240\\slmult1\\cf1 after\\par\n");
    expect_body("```c\n*raw*\n", "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb120\\sa0\\sl2"
                                 "40\\slmult1 *raw*\\sa120\\par\n}");
    expect_body("text\n```\ncode\n```", "\\pard\\sa80\\sl240\\slmult1\\cf1 text\\par\n"
                                        "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb120\\s"
                                        "a0\\sl240\\slmult1 code\\sa120\\par\n}");
    expect_body("```\n```", "{\\f1\\fs20\\cf5\\highlight6}");
    expect_body("```", "{\\f1\\fs20\\cf5\\highlight6}");
    expect_body("```\n\n```", "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb120\\sa0\\sl240"
                              "\\slmult1 \\sa120\\par\n}");
    expect_body("```\na\n```\n```\nb\n```", "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb12"
                                            "0\\sa0\\sl240\\slmult1 a\\sa120\\par\n}"
                                            "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb12"
                                            "0\\sa0\\sl240\\slmult1 b\\sa120\\par\n}");
}

static void verify_lists_quotes_links(void)
{
    expect_body("- one\n* two\n+ three",
                "\\pard\\li440\\fi-220\\sa40\\sl240\\slmult1\\cf1\\bullet\\tab one\\par\n"
                "\\pard\\li440\\fi-220\\sa40\\sl240\\slmult1\\cf1\\bullet\\tab two\\par\n"
                "\\pard\\li440\\fi-220\\sa40\\sl240\\slmult1\\cf1\\bullet\\tab three\\par\n");
    expect_body("1. first\n12. twelfth",
                "\\pard\\li440\\fi-220\\sa40\\sl240\\slmult1\\cf1 1.\\tab first\\par\n"
                "\\pard\\li440\\fi-220\\sa40\\sl240\\slmult1\\cf1 2.\\tab twelfth\\par\n");
    expect_body("1.nospace", "\\pard\\sa80\\sl240\\slmult1\\cf1 1.nospace\\par\n");
    expect_body("1.", "\\pard\\sa80\\sl240\\slmult1\\cf1 1.\\par\n");
    expect_body("12x", "\\pard\\sa80\\sl240\\slmult1\\cf1 12x\\par\n");
    expect_body("-dash", "\\pard\\sa80\\sl240\\slmult1\\cf1 -dash\\par\n");
    expect_body("> quoted\n>bare",
                "\\pard\\li480\\sa80\\sl240\\slmult1\\cf3 quoted bare\\cf1\\par\n");
    expect_body("see [docs](https://x.example/) now",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 see {\\cf4\\ul docs\\ul0} now\\par\n");
    expect_body("[no url]", "\\pard\\sa80\\sl240\\slmult1\\cf1 [no url]\\par\n");
    expect_body("[open](", "\\pard\\sa80\\sl240\\slmult1\\cf1 [open](\\par\n");
    expect_body("[a]b", "\\pard\\sa80\\sl240\\slmult1\\cf1 [a]b\\par\n");
    expect_body("para\n- item",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 para\\par\n"
                "\\pard\\li440\\fi-220\\sa40\\sl240\\slmult1\\cf1\\bullet\\tab item\\par\n");
}

static void verify_note_text(void)
{
    struct note_text *text = nullptr;
    require(note_text_create("\xEF\xBB\xBFhi", 5, &text) == NOTE_TEXT_ACCEPTED &&
                note_text_length(text) == 2 && same_text(note_text_bytes(text), "hi"),
            "bom stripped");
    note_text_destroy(text);
    require(note_text_create("\xC3", 1, &text) == NOTE_TEXT_INVALID_UTF8, "invalid note");
    require(note_text_create("", 0, &text) == NOTE_TEXT_ACCEPTED && note_text_length(text) == 0,
            "empty note");
    note_text_destroy(text);
    note_text_destroy(nullptr);
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_LIGHT), ui_font_face(FOLIO_LANGUAGE_JA),
                               &rtf) == MARKDOWN_RTF_CONVERTED &&
                markdown_rtf_length(rtf) == strlen(header) + 1 &&
                same_text(markdown_rtf_text(rtf) + strlen(header), "}"),
            "empty document");
    markdown_rtf_destroy(rtf);
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_DARK), ui_font_face(FOLIO_LANGUAGE_JA),
                               &rtf) == MARKDOWN_RTF_CONVERTED &&
                strstr(markdown_rtf_text(rtf), "\\red239\\green228\\blue234;") != nullptr,
            "dark palette in the color table");
    markdown_rtf_destroy(rtf);
    markdown_rtf_destroy(nullptr);
    struct rtf_palette dark = rtf_palette_for(FOLIO_THEME_DARK);
    struct rtf_palette light = rtf_palette_for(FOLIO_THEME_LIGHT);
    require(dark.heading.red == 0xFF && light.heading.red == 0x2C &&
                dark.code_background.red == 0x16 && light.code_background.red == 0x2C,
            "palettes differ by theme");
}

static void verify_text_buffer(void)
{
    struct text_buffer *buffer = nullptr;
    require(text_buffer_create(&buffer) == TEXT_BUFFER_ACCEPTED, "buffer");
    for (size_t index = 0; index < 100; ++index)
    {
        text_buffer_append_text(buffer, "0123456789");
    }
    text_buffer_append(buffer, "ab", 1);
    require(text_buffer_finish(buffer) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_length(buffer) == 1001 && text_buffer_bytes(buffer)[1000] == 'a' &&
                text_buffer_bytes(buffer)[1001] == '\0',
            "buffer grows");
    text_buffer_destroy(buffer);
    text_buffer_destroy(nullptr);
}

/* fonttbl の \\f0 は引数で受けた face で、コードの \\f1 は Consolas のまま（ADR 0032 の決定 5）。
 */
static void expect_face(enum folio_language language)
{
    const char *face = ui_font_face(language);
    char wanted[64];
    int written = snprintf(wanted, sizeof wanted, "{\\f0\\fnil %s;}", face);
    require(written > 0 && (size_t)written < sizeof wanted, "the fonttbl entry fits");
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_LIGHT), face, &rtf) ==
                MARKDOWN_RTF_CONVERTED,
            "the empty document is built with the face");
    require(strstr(markdown_rtf_text(rtf), wanted) != nullptr, "the face is in the fonttbl");
    require(strstr(markdown_rtf_text(rtf), "{\\f1\\fmodern Consolas;}") != nullptr,
            "the code font stays Consolas");
    markdown_rtf_destroy(rtf);
}

static void verify_fonttbl(void)
{
    for (size_t index = 0; index < folio_language_count; ++index)
    {
        expect_face((enum folio_language)index);
    }
    struct note_text *text = nullptr;
    require(note_text_create("abc", 3, &text) == NOTE_TEXT_ACCEPTED, "note");
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_DARK),
                                ui_font_face(FOLIO_LANGUAGE_ZH_HANS),
                                &rtf) == MARKDOWN_RTF_CONVERTED,
            "a document with a body is built with the face too");
    note_text_destroy(text);
    require(strstr(markdown_rtf_text(rtf), "Noto Sans SC;") != nullptr,
            "the simplified Chinese face reaches the fonttbl");
    markdown_rtf_destroy(rtf);
}

static const char plain_start[] = "\\pard\\sb0\\sa0\\sl240\\slmult1\\cf1 ";

/* 編集用の RTF（#250）が頭 + 段落の書式 + expected と完全に一致するか。 */
static void expect_plain(const char *_Nonnull input, size_t input_length,
                         const char *_Nonnull expected)
{
    struct note_text *text = nullptr;
    require(note_text_create(input, input_length, &text) == NOTE_TEXT_ACCEPTED, "plain note");
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_plain(text, rtf_palette_for(FOLIO_THEME_LIGHT),
                               ui_font_face(FOLIO_LANGUAGE_JA), &rtf) == MARKDOWN_RTF_CONVERTED,
            "plain convert");
    const char *whole = markdown_rtf_text(rtf);
    size_t head = strlen(header);
    size_t start = strlen(plain_start);
    require(markdown_rtf_length(rtf) == head + start + strlen(expected) &&
                memcmp(whole, header, head) == 0 && memcmp(whole + head, plain_start, start) == 0,
            "plain frame");
    if (!same_text(whole + head + start, expected))
    {
        fprintf(stderr, "  plain: %s\n  got: %s\n", input, whole + head + start);
        require(false, "plain body");
    }
    bool escaped = strchr(input, '\\') != nullptr; /* `\\b` は escape した字で、書式ではない */
    require((escaped || strstr(whole + head, "\\b") == nullptr) &&
                strstr(whole + head, "\\f1") == nullptr &&
                strstr(whole + head, "\\fs") == nullptr && strstr(whole + head, "\\ul") == nullptr,
            "the plain body has no markdown formatting");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void verify_plain(void)
{
    /* 改行の 3 形はどれも \par 1 つ。CRLF を 2 つにしない。 */
    expect_plain("A\nB", 3, "A\\par B\\par }");
    expect_plain("A\r\nB", 4, "A\\par B\\par }");
    expect_plain("A\rB", 3, "A\\par B\\par }");
    expect_plain("A\r\n\r\nB", 6, "A\\par \\par B\\par }");
    expect_plain("A\n\rB", 4, "A\\par \\par B\\par }");
    /* 構文記号だけ escape し、TAB・VT・制御文字・UTF-8 は生のまま。 */
    expect_plain("a\\b{c}d", 7, "a\\\\b\\{c\\}d\\par }");
    expect_plain("A\tB\vC\x01"
                 "D",
                 7,
                 "A\tB\vC\x01"
                 "D\\par }");
    expect_plain("\xE6\x97\xA5\xE6\x9C\xAC", 6, "\xE6\x97\xA5\xE6\x9C\xAC\\par }");
    /* 末尾の改行を保つ。 */
    expect_plain("A\n", 2, "A\\par \\par }");
    expect_plain("A\r", 2, "A\\par \\par }");
    expect_plain("A\r\n", 3, "A\\par \\par }");
    /* Markdown の記法を解釈しない。 */
    expect_plain("# H\n- x\n`c`\n**b** [l](u)", 24,
                 "# H\\par - x\\par `c`\\par **b** [l](u)\\par }");
    expect_plain("```\ncode\n```", 12, "```\\par code\\par ```\\par }");
    /* 空の本文は頭 + \par + }。頭は閲覧の空の文書と同じバイト列で始まる。 */
    expect_plain("", 0, "\\par }");
    struct markdown_rtf *view = nullptr;
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_LIGHT), ui_font_face(FOLIO_LANGUAGE_JA),
                               &view) == MARKDOWN_RTF_CONVERTED,
            "empty view");
    struct note_text *text = nullptr;
    require(note_text_create("", 0, &text) == NOTE_TEXT_ACCEPTED, "empty plain note");
    struct markdown_rtf *plain = nullptr;
    require(markdown_rtf_plain(text, rtf_palette_for(FOLIO_THEME_LIGHT),
                               ui_font_face(FOLIO_LANGUAGE_JA), &plain) == MARKDOWN_RTF_CONVERTED,
            "empty plain");
    size_t shared = markdown_rtf_length(view) - 1;
    require(memcmp(markdown_rtf_text(view), markdown_rtf_text(plain), shared) == 0,
            "the plain header is the view header");
    markdown_rtf_destroy(plain);
    markdown_rtf_destroy(view);
    note_text_destroy(text);
}

static struct markdown_rtf *_Nonnull convert_syntax(const char *_Nonnull source)
{
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "syntax source");
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Noto Sans JP", &rtf) ==
                MARKDOWN_RTF_CONVERTED,
            "syntax conversion");
    note_text_destroy(text);
    return rtf;
}

static void expect_same_rendering(const char *_Nonnull source, const char *_Nonnull canonical)
{
    struct markdown_rtf *actual = convert_syntax(source);
    struct markdown_rtf *expected = convert_syntax(canonical);
    if (markdown_rtf_length(actual) != markdown_rtf_length(expected) ||
        memcmp(markdown_rtf_text(actual), markdown_rtf_text(expected),
               markdown_rtf_length(expected)) != 0)
    {
        fprintf(stderr, "source=%s\nRTF=%s\n", source, markdown_rtf_text(actual));
        require(false, "logical line endings produce identical RTF");
    }
    markdown_rtf_destroy(actual);
    markdown_rtf_destroy(expected);
}

static void expect_breaks(const char *_Nonnull canonical, const char *_Nonnull ending)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "line-ending source");
    for (size_t index = 0; canonical[index] != '\0'; ++index)
    {
        if (canonical[index] == '\n')
            text_buffer_append_text(source, ending);
        else
            text_buffer_append(source, canonical + index, 1);
    }
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED, "line-ending source complete");
    expect_same_rendering(text_buffer_bytes(source), canonical);
    text_buffer_destroy(source);
}

static void verify_line_endings(void)
{
    const char *canonical =
        "# first\n# second\n\nsoft\nwrap  \nhard\\\nbreak\n\n"
        "setext\n===\n\n1. item\n   continued\n2. next\n\n> quote\n> continued\n\n"
        "----\n\n~~~c\n\xE6\x97\xA5\xE6\x9C\xAC \xF0\x9F\x98\x80 {\\}\n\ncode\n~~~\n\n"
        "A | B\n--- | ---\n\xE6\x97\xA5\xE6\x9C\xAC | `a\\|b`\n\n"
        "\xF0\x9F\x98\x80 \\{brace\\} \\\\ tail\n\n~~~\nunclosed\n";
    expect_breaks(canonical, "\r");
    expect_breaks(canonical, "\r\n");
    expect_same_rendering("# first\r\n# second\r\n\rone\ntwo  \rthree\\\r\nfour\r",
                          "# first\n# second\n\none\ntwo  \nthree\\\nfour\n");
    expect_same_rendering("one\n\rtwo", "one\n\ntwo");
    expect_same_rendering("one\r\rtwo", "one\n\ntwo");
    expect_same_rendering("\r", "\n");
    expect_same_rendering("\r\n\r\n", "\n\n");
}

static void verify_line_bounds(void)
{
    const char terminal[] = {'a', '\r'};
    struct markdown_line line = markdown_line_read(terminal, sizeof terminal, 0);
    require(line.length == 1 && line.next == sizeof terminal && line.bytes[0] == 'a',
            "terminal CR ends a bounded source without reading beyond it");
    line = markdown_line_read(terminal, sizeof terminal, line.next);
    require(line.length == 0 && line.next == sizeof terminal, "EOF after terminal CR is empty");
    const char sliced[] = {'a', '\r', '\n', 'b'};
    line = markdown_line_read(sliced, 2, 0);
    require(line.length == 1 && line.next == 2, "CRLF is not consumed beyond the declared length");
    const char crlf[] = {'\r', '\n'};
    line = markdown_line_read(crlf, sizeof crlf, 0);
    require(line.length == 0 && line.next == sizeof crlf, "CRLF is one empty logical line");
    const char lfcr[] = {'\n', '\r'};
    line = markdown_line_read(lfcr, sizeof lfcr, 0);
    require(line.length == 0 && line.next == 1, "LFCR starts with a separate LF boundary");
    line = markdown_line_read(lfcr, sizeof lfcr, line.next);
    require(line.length == 0 && line.next == sizeof lfcr, "LFCR has a second CR boundary");
}

static void expect_fragment(const char *_Nonnull source, const char *_Nonnull included,
                            const char *_Nonnull excluded)
{
    struct markdown_rtf *rtf = convert_syntax(source);
    const char *body = markdown_rtf_text(rtf) + strlen(header);
    if (strstr(body, included) == nullptr ||
        (excluded[0] != '\0' && strstr(body, excluded) != nullptr))
    {
        fprintf(stderr, "source=%s\nRTF=%s\n", source, body);
        require(false, "syntax fragment and forbidden interpretation");
    }
    markdown_rtf_destroy(rtf);
}

static void verify_new_blocks(void)
{
    expect_fragment("----", "\\sl-20\\slmult0\\fs2\\highlight3\\par}", "----");
    expect_fragment("- - -", "\\sl-20", "\\bullet");
    expect_fragment("* * *", "\\sl-20", "\\bullet");
    expect_fragment("_ _ _", "\\sl-20", "\\i ");
    expect_fragment("title\n----", "\\fs34 title", "\\sl-20");
    expect_fragment("title\n===", "\\fs44 title", "===");
    expect_fragment("a  \nb\\\nc\nd", "a\\line b\\line c d", "  ");
    expect_fragment("1) one\n9) two", "2)\\tab two", "9)");
    expect_fragment("0. zero\n4. next", "1.\\tab next", "4.");
    expect_fragment("paragraph\n2. literal", "paragraph 2. literal", "\\tab");
    expect_fragment("paragraph\n1. item", "1.\\tab item", "paragraph 1.");
    expect_fragment("- parent\n  - child\n- next", "\\li880\\fi-220", "child next");
    expect_fragment("1. one\n\n   continued\n\n2. two", "2.\\tab two", "1.\\tab two");
    expect_fragment("> - item\n>   continued", "\\li920\\fi-220", "\\cf3 continued");
    expect_fragment("> > ## nested", "\\fs34\\li960 nested", "> ##");
    expect_fragment("- [ ] open\n- [x] done\n- [X] done", "\\u9744?\\tab open", "[x]");
    expect_fragment("- [x] done", "\\u9745?\\tab done", "[x]");
}

static void verify_new_inline(void)
{
    expect_fragment("*a **b* c**", "*a {\\b b* c}", "\\i ");
    expect_fragment("~~gone~~ tail", "{\\strike gone} tail", "~~");
    expect_fragment("~~unclosed", "~~unclosed", "\\strike");
    expect_fragment("``a`b``", "{\\f1\\fs20\\cf4 a`b}", "``");
    expect_fragment("` a `", "\\cf4 a}", " a ");
    expect_fragment("`  `", "\\cf4   }", "``");
    expect_fragment("`a\\`b`", "\\cf4 a\\\\}b`", "\\cf4 a`b");
    expect_fragment("\\\\`code`", "\\\\{\\f1\\fs20\\cf4 code}", "`code`");
    expect_fragment("\\`literal", "`literal", "\\f1");
    expect_fragment("``a\nb``", "\\cf4 a b}", "\\line");
    expect_fragment("# **bold** tail", "{\\b bold} tail", "\\b0  tail");
    expect_fragment("*before [x](foo*) after", "*before {\\cf4\\ul x\\ul0} after", "\\i ");
    expect_fragment("*before [x](foo) after*", "{\\i before {\\cf4\\ul x\\ul0} after}", "foo");
    expect_fragment("![alt](url)", "![alt](url)", "\\ul");
    expect_fragment("\\~~literal\\~~", "~~literal~~", "\\strike");
    expect_fragment("~~~ lang\nraw\n```\n~~~~", "raw\\par\n", "lang");
    expect_fragment("````\na\n```\nb\n````", "```\\par\n", "\\cf1 b");
    expect_fragment("- item\n  ```\n  code\noutside", "\\cf1 outside", "code outside");
    expect_fragment("```\na\n``` trailing\nb\n```", "``` trailing\\par\n", "\\cf1 b");
}

static void verify_tables(void)
{
    expect_fragment("A | B\n:--- | ---:\nx | y", "\\trowd", ":---");
    expect_fragment("| A | B |\n| :--: | -- |\n| x | y |", "\\qc {\\b A}", "| A");
    expect_fragment("A | B\n--- | ---\nx", "{x}\\cell ", "\\cf1 x");
    expect_fragment("A | B\n--- | ---\nx | y | ignored", "{y}\\cell ", "ignored");
    expect_fragment("A | B\n--- | ---\n`a\\|b` | ~~x~~", "\\cf4 a|b}", "a\\\\|b");
    expect_fragment("A | B\n--- | ---\n# heading | x", "\\fs44 heading | x", "{# heading");
    expect_fragment("A | B\n--- | ---\n~~~ lang | x\ncode\n~~~", "\\highlight6", "{~~~ lang");
    expect_fragment("A | B\n---", "A | B", "\\trowd");
}

static void verify_large_syntax(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "large source buffer");
    for (size_t index = 0; index < 20000; ++index)
        text_buffer_append_text(source, "[");
    text_buffer_append_text(source, "\ntext");
    expect_fragment(text_buffer_bytes(source), "text", "\\ul");
    text_buffer_destroy(source);
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "space source buffer");
    text_buffer_append_text(source, "text");
    for (size_t index = 0; index < 20000; ++index)
        text_buffer_append_text(source, " ");
    text_buffer_append_text(source, "\nnext");
    expect_fragment(text_buffer_bytes(source), "text\\line next", "  ");
    text_buffer_destroy(source);
}

static void verify_deep_and_wide(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "nested source");
    for (size_t depth = 0; depth < 512; ++depth)
    {
        for (size_t column = 0; column < depth; ++column)
            text_buffer_append_text(source, "  ");
        text_buffer_append_text(source, "- nested\n");
    }
    text_buffer_append_text(source, "\nend");
    expect_fragment(text_buffer_bytes(source), "end", "- nested");
    text_buffer_destroy(source);
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "wide source");
    for (size_t row = 0; row < 3; ++row)
    {
        for (size_t column = 0; column < 64; ++column)
            text_buffer_append_text(source, row == 1 ? "| --- " : "| **cell** ");
        text_buffer_append_text(source, "|\n");
    }
    expect_fragment(text_buffer_bytes(source), "**cell**", "\\trowd");
    expect_fragment(text_buffer_bytes(source), "|\\par\n", "\\b ");
    text_buffer_destroy(source);
}

static void verify_wide_delimiter_cells(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "wide invalid delimiter source");
    for (size_t index = 0; index < 64; ++index)
        text_buffer_append_text(source, index == 63 ? "| :: " : "| --- ");
    text_buffer_append_text(source, "|");
    struct markdown_line line =
        markdown_line_read(text_buffer_bytes(source), text_buffer_length(source), 0);
    require(!markdown_table_delimiter(line), "invalid sixty-fourth delimiter cell is not a table");
    text_buffer_destroy(source);
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "wide empty delimiter source");
    for (size_t index = 0; index < 64; ++index)
        text_buffer_append_text(source, index == 63 ? "|  " : "| --- ");
    text_buffer_append_text(source, "|");
    line = markdown_line_read(text_buffer_bytes(source), text_buffer_length(source), 0);
    require(!markdown_table_delimiter(line), "empty sixty-fourth delimiter cell is not a table");
    text_buffer_destroy(source);
}

static void verify_syntax_allocation(void)
{
    const char *source = "paragraph\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10"
                         "\n\n- parent\n  - child\n\nA | B\n-- | --\n`x` | ~~y~~\n\ntail";
    struct note_text *text = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "OOM source");
    struct markdown_rtf *sentinel = nullptr;
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_LIGHT), "Noto Sans JP", &sentinel) ==
                MARKDOWN_RTF_CONVERTED,
            "OOM sentinel");
    bool finished = false;
    size_t rejected = 0;
    for (size_t fail = 1; fail < 200 && !finished; ++fail)
    {
        allocation_probe_fail_at(fail);
        struct markdown_rtf *out = sentinel;
        enum markdown_rtf_outcome result =
            markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Noto Sans JP", &out);
        size_t calls = allocation_probe_calls();
        allocation_probe_fail_at(0);
        if (calls == 0)
            printf("Markdown allocation injection inactive in canonical build\n");
        if (result == MARKDOWN_RTF_OUT_OF_MEMORY)
        {
            require(out == sentinel, "OOM keeps the output unchanged");
            ++rejected;
        }
        if (result == MARKDOWN_RTF_CONVERTED)
        {
            markdown_rtf_destroy(out);
            finished = true;
        }
    }
    require(finished, "syntax allocation exhaustion reaches success");
    printf("Markdown syntax allocation exhaustion completed: rejected=%zu\n", rejected);
    markdown_rtf_destroy(sentinel);
    note_text_destroy(text);
}

static void verify_borrowed_input(void)
{
    struct markdown_span span = {.bytes = "x", .length = 1, .offset = 2};
    struct markdown_view view = {.spans = &span, .count = 1, .length = 1};
    struct text_buffer *out = nullptr;
    require(text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED, "invalid input output");
    require(markdown_inline_tokens_render(out, view) == MARKDOWN_RTF_INVALID_INPUT &&
                text_buffer_length(out) == 0,
            "inconsistent borrowed offsets are typed invalid input");
    view.count = 0;
    require(!markdown_view_valid(view) && markdown_view_at(view, 0) == '\0',
            "empty borrowed spans never access memory for a positive length");
    struct markdown_line line = {.bytes = "x", .length = 1, .content = 2};
    require(!markdown_line_valid(line) && markdown_line_heading(line) == 0,
            "line source bounds are checked before classification");
    struct markdown_line header_line = markdown_line_read("A | B", 5, 0);
    struct markdown_line delimiter = markdown_line_read("-- | --", 7, 0);
    require(markdown_table_row(out, header_line, delimiter, (enum markdown_row_kind)99) ==
                    MARKDOWN_RTF_INVALID_INPUT &&
                text_buffer_length(out) == 0,
            "invalid row enum is rejected before any RTF is appended");
    text_buffer_destroy(out);
}

void run_markdown_tests(void)
{
    verify_line_endings();
    verify_line_bounds();
    run_markdown_source_state_tests();
    verify_borrowed_input();
    verify_new_blocks();
    verify_new_inline();
    verify_tables();
    verify_large_syntax();
    verify_deep_and_wide();
    verify_wide_delimiter_cells();
    verify_syntax_allocation();
    verify_plain();
    verify_paragraphs();
    verify_fonttbl();
    verify_unicode();
    verify_headings();
    verify_emphasis();
    verify_code();
    verify_lists_quotes_links();
    verify_note_text();
    verify_text_buffer();
}
