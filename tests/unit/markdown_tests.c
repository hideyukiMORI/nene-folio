#include "allocation_probe.h"
#include "folio_language.h"
#include "markdown_autolink.h"
#include "markdown_character_reference.h"
#include "markdown_containers.h"
#include "markdown_inline_tokens.h"
#include "markdown_line.h"
#include "markdown_rtf.h"
#include "markdown_table.h"
#include "note_text.h"
#include "rtf_palette.h"
#include "text_buffer.h"
#include "ui_font.h"
#include "unit_tests.h"

#include <stdint.h>
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
                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 \\bullet\\tab }one\\par\n"
                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 \\bullet\\tab }two\\par\n"
                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 \\bullet\\tab }three\\par\n");
    expect_body("1. first\n12. twelfth",
                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 1.\\tab }first\\par\n"
                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 2.\\tab }twelfth\\par\n");
    expect_body("1.nospace", "\\pard\\sa80\\sl240\\slmult1\\cf1 1.nospace\\par\n");
    expect_body("1.", "\\pard\\li440\\fi-220\\tx440\\sb0\\sa0\\sl240\\slmult1 "
                      "{\\f0\\fs22\\cf1\\b0\\highlight0 1.\\tab }\\par\n");
    expect_body("12x", "\\pard\\sa80\\sl240\\slmult1\\cf1 12x\\par\n");
    expect_body("-dash", "\\pard\\sa80\\sl240\\slmult1\\cf1 -dash\\par\n");
    expect_body("> quoted\n>bare",
                "\\pard\\li480\\sa80\\sl240\\slmult1\\cf3 quoted bare\\cf1\\par\n");
    expect_body("see [docs](https://x.example/) now",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 see {\\cf4\\ul docs\\ul0} now\\par\n");
    expect_body("[no url]", "\\pard\\sa80\\sl240\\slmult1\\cf1 [no url]\\par\n");
    expect_body("[open](", "\\pard\\sa80\\sl240\\slmult1\\cf1 [open](\\par\n");
    expect_body("[a]b", "\\pard\\sa80\\sl240\\slmult1\\cf1 [a]b\\par\n");
    expect_body("para\n- item", "\\pard\\sa80\\sl240\\slmult1\\cf1 para\\par\n"
                                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                                "{\\f0\\fs22\\cf1\\b0\\highlight0 \\bullet\\tab }item\\par\n");
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

static size_t fragment_count(const char *_Nonnull text, const char *_Nonnull fragment)
{
    size_t count = 0;
    const char *cursor = text;
    while ((cursor = strstr(cursor, fragment)) != nullptr)
    {
        ++count;
        cursor += strlen(fragment);
    }
    return count;
}

static void verify_container_paths(void)
{
    expect_fragment("7. parent\n   > quote\n\n   continued\n1. next",
                    "\\li440\\sa40\\sl240\\slmult1\\cf1 continued", "1.\\tab }next");
    expect_fragment("7. parent\n   > quote\n\n   continued\n1. next", "8.\\tab }next", "");
    expect_fragment("7. parent\n   > quote\n   continued\n1. next", "quote continued\\cf1", "");
    expect_fragment("> 7. parent\n>    > quote\n>\n>    continued\n> 2. next", "8.\\tab }next",
                    "2.\\tab }next");
    expect_fragment("7. parent\n   > 2. child\n   >    continued\n\n   parent again\n2. next",
                    "8.\\tab }next", "3.\\tab }next");
    expect_fragment("- parent\n  - child\n    > quote\n\n  parent again\n- next",
                    "\\li440\\sa40\\sl240\\slmult1\\cf1 parent again", "");
    expect_fragment("7. parent\n> outside\n\n1. next", "1.\\tab }next", "8.\\tab }next");
    expect_fragment("> quote\nplain", "quote plain\\cf1", "\\cf1 plain");
    expect_fragment("> quote\n\nplain", "\\cf1 plain", "quote plain");
    expect_fragment("> quote\n# heading", "\\fs44 heading", "quote heading");
    expect_fragment("> quote\n---", "\\sl-20", "\\fs34");
    expect_fragment("> quote\nA | B\n-- | --", "\\trowd", "quote A | B");
    expect_fragment("paragraph\n+\nnext", "paragraph + next", "\\bullet");
    expect_fragment("paragraph\n2. literal", "paragraph 2. literal", "\\tab");
    expect_fragment("7. item\n2.\n3. next", "9.\\tab }next", "3.\\tab }next");
    expect_fragment("-\n  text", "\\li440\\sa40\\sl240\\slmult1\\cf1 text", "- text");
    expect_fragment("-\n\n  text", "\\pard\\sa80\\sl240\\slmult1\\cf1 text", "\\li440\\sa40");
    expect_fragment("7. first\n 1. next", "8.\\tab }next", "1.\\tab }next");
    expect_fragment("7. first\n   1. child\n1. next", "\\li880\\fi-220\\tx880", "1.\\tab }next");
    expect_fragment("7.\n\n1. next", "8.\\tab }next", "1.\\tab }next");
    expect_fragment("7.\n\n   text\n1. next", "1.\\tab }next", "8.\\tab }next");
    expect_fragment("> 7.\n>\n> 1. next", "8.\\tab }next", "1.\\tab }next");
    expect_fragment("> 7.\n\n> 1. next", "1.\\tab }next", "8.\\tab }next");
    expect_same_rendering("7. parent\r   > quote\r\r   continued\r1. next",
                          "7. parent\n   > quote\n\n   continued\n1. next");
}

static void verify_container_markers(void)
{
    expect_fragment("7. > quote\n\n   continued\n1. next", "\\li920\\fi-700\\tx920", "");
    expect_fragment("7. # heading\n1. next", "\\pard\\sb0\\sa80\\b\\cf2\\fs44\\li440", "\\sb200");
    expect_fragment("7. ~~~\n   code\n   ~~~\n1. next", "7.\\tab }\\par\n{\\f1", "1.\\tab }next");
    expect_fragment("7. ~~~\n   ~~~\n1. next", "{\\f1\\fs20\\cf5\\highlight6}", "\\li620");
    expect_fragment("7. >\n1. next", "7.\\tab }\\par\n", "1.\\tab }next");
    expect_fragment("7. - child\n1. next", "7.\\tab }\\par\n\\pard\\li880\\fi-220\\tx880", "");
    expect_fragment("7. ----\n1. next", "7.\\tab }\\par\n{\\pard", "7.----");
    struct markdown_rtf *rtf = convert_syntax("7. > quote\n\n   continued\n1. next");
    require(fragment_count(markdown_rtf_text(rtf), "7.\\tab ") == 1 &&
                fragment_count(markdown_rtf_text(rtf), "8.\\tab ") == 1,
            "each item marker is emitted once across quote and parent continuation");
    markdown_rtf_destroy(rtf);
}

static void verify_container_leaves(void)
{
    expect_fragment("- A | B\n  -- | --\n  x | y\noutside", "\\cf1 outside", "{outside}\\cell");
    expect_fragment("- A | B\n-- | --", "A | B", "\\trowd");
    expect_fragment("> A | B\n> -- | --\nx | y", "\\cf1 x | y", "{x}\\cell");
    expect_fragment("- A | B\n  -- | --\n- next", "\\bullet\\tab }next", "{next}\\cell");
    expect_fragment("7. > ~~~\n   > code\n\n   continued\n1. next", "8.\\tab }next", "");
    expect_fragment("- ~~~\n  > literal\noutside", "> literal\\sa120\\par\n}", "\\cf3");
    expect_fragment("- ~~~\n\tcode\n  ~~~", "  code\\sa120", "\\cf1 code");
    expect_fragment(">\t~~~\n>\t\tcode\n>\t~~~", "\tcode\\sa120", "    code");
    expect_fragment(">    ~~~\n>\t\tcode\n>    ~~~", "   code\\sa120", "\tcode");
    expect_fragment("- ~~~\n  code\n- next", "\\bullet\\tab }next", "code next");
}

static void verify_literal_container_table(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "wide container table source");
    text_buffer_append_text(source, "7. > ");
    for (size_t column = 0; column < 64; ++column)
        text_buffer_append_text(source, "| **head** ");
    text_buffer_append_text(source, "|\n   > ");
    for (size_t column = 0; column < 64; ++column)
        text_buffer_append_text(source, "| -- ");
    text_buffer_append_text(source, "|\n   > ");
    for (size_t column = 0; column < 64; ++column)
        text_buffer_append_text(source, "| cell ");
    text_buffer_append_text(source, "|\n1. next");
    struct markdown_rtf *rtf = convert_syntax(text_buffer_bytes(source));
    const char *body = markdown_rtf_text(rtf);
    require(strstr(body, "7. > | **head**") != nullptr && strstr(body, "   > | --") != nullptr &&
                strstr(body, "   > | cell") != nullptr && fragment_count(body, "**head**") == 64 &&
                strstr(body, "\\trowd") == nullptr,
            "literal table retains every column and original nested prefix");
    require(fragment_count(body, "7.") == 1 && strstr(body, "7.\\tab ") == nullptr &&
                strstr(body, "8.\\tab }next") != nullptr,
            "literal source marker replaces synthesis while the owner sequence continues");
    markdown_rtf_destroy(rtf);
    text_buffer_destroy(source);
}

static void verify_container_columns(void)
{
    struct markdown_line line = markdown_line_read("\tcode", 5, 0);
    require(markdown_line_skip(&line, 2) && line.prefix == 1 && line.padding == 2 &&
                line.column == 2 && line.indent == 2,
            "list continuation consumes only two columns of a tab");
    require(markdown_line_skip(&line, 1) && line.padding == 1 && line.column == 3,
            "fence deindent consumes virtual padding before source bytes");
    line = markdown_line_read(">\t\tcode", 7, 0);
    require(markdown_line_quote(&line) && line.padding == 2 && line.indent == 6,
            "optional quote spacing consumes one column of a tab");
    require(markdown_line_skip(&line, 3) && line.prefix == 3 && line.padding == 3,
            "the next partial tab retains its remaining columns");
    struct markdown_line column = {.bytes = "x", .length = 1, .column = SIZE_MAX};
    require(!markdown_line_valid(column),
            "a visual column cannot exceed its consumed prefix bytes");
    struct markdown_line invalid = {.bytes = " ", .length = 1, .indent = 8};
    require(!markdown_line_skip(&invalid, 8) && invalid.prefix == 0 && invalid.column == 0,
            "an inconsistent indent cannot make column consumption exceed borrowed bytes");
    invalid = (struct markdown_line){.bytes = "x", .length = 1, .padding = 1};
    require(!markdown_line_valid(invalid), "virtual tab padding requires its source tab");
}

static void verify_container_queries(void)
{
    struct markdown_containers *owner = nullptr;
    require(markdown_containers_create(&owner) == MARKDOWN_RTF_CONVERTED, "container owner");
    require(markdown_line_valid(markdown_containers_line(owner)) &&
                markdown_containers_line(owner).bytes[0] == '\0',
            "a newly created owner exposes a valid empty borrowed line");
    struct markdown_line parent = markdown_line_read("7. parent", 9, 0);
    struct markdown_line next = markdown_line_read("2. next", 7, 0);
    require(markdown_containers_accept(owner, parent, next, MARKDOWN_PARAGRAPH_STATE_NONE) ==
                MARKDOWN_RTF_CONVERTED,
            "initial parent item");
    struct markdown_line matching = markdown_line_read("   payload", 10, 0);
    size_t calls = allocation_probe_calls();
    require(markdown_containers_match(owner, &matching) == MARKDOWN_CONTAINER_MATCH_MATCHED &&
                matching.content == 3 && allocation_probe_calls() == calls &&
                markdown_containers_marker_number(owner) == 7,
            "strict lookahead allocates nothing and does not advance the item");
    struct markdown_line outside = markdown_line_read("outside", 7, 0);
    require(markdown_containers_match(owner, &outside) == MARKDOWN_CONTAINER_MATCH_LEFT &&
                outside.prefix == 0 && markdown_containers_marker_number(owner) == 7,
            "strict scope mismatch leaves query and owner unchanged");
    require(markdown_containers_accept(owner, next, outside, MARKDOWN_PARAGRAPH_STATE_OPEN) ==
                    MARKDOWN_RTF_CONVERTED &&
                markdown_containers_event(owner) == MARKDOWN_CONTAINER_EVENT_FLUSH_REQUIRED &&
                markdown_containers_line(owner).bytes == parent.bytes &&
                markdown_containers_marker_number(owner) == 7,
            "a sibling requests flush before replacing the old item or accepted line");
    markdown_containers_consume_marker(owner);
    require(markdown_containers_accept(owner, next, outside, MARKDOWN_PARAGRAPH_STATE_NONE) ==
                    MARKDOWN_RTF_CONVERTED &&
                markdown_containers_marker_number(owner) == 8,
            "an existing sibling advances even when its source number is two");
    struct markdown_line invalid = {.bytes = "x", .length = 1, .content = 2};
    require(markdown_containers_match(owner, &invalid) == MARKDOWN_CONTAINER_MATCH_INVALID_INPUT,
            "strict mismatch and malformed borrowed query are different outcomes");
    require(markdown_containers_accept(owner, invalid, outside, MARKDOWN_PARAGRAPH_STATE_NONE) ==
                MARKDOWN_RTF_INVALID_INPUT,
            "normal acceptance validates borrowed bounds");
    markdown_containers_destroy(owner);
}

static void verify_container_style_boundaries(void)
{
    expect_fragment("7. ordered\n- bullet\n1. ordered again", "1.\\tab }ordered again", "8.\\tab ");
    expect_fragment("7. first\n3) changed\n1) next", "4)\\tab }next", "8)\\tab ");
    expect_fragment("7. parent\n   > 3. inside\n   > - bullet\n\n   resumed\n1. next",
                    "8.\\tab }next", "4.\\tab }next");
    expect_fragment("paragraph\n1.\nnext", "paragraph 1. next", "\\tab");
    expect_fragment("123456789. max\n1. next", "123456790.\\tab }next", "1.\\tab }next");
    expect_fragment("1234567890. literal", "1234567890. literal", "\\tab");
    expect_fragment("7. # title ###  \n1. next", "}title\\b0", "title ###");
    expect_fragment("# title###", "title###", "title\\b0");
}

static void verify_empty_owner_queries(void)
{
    struct markdown_containers *owner = nullptr;
    require(markdown_containers_create(&owner) == MARKDOWN_RTF_CONVERTED, "empty query owner");
    require(markdown_containers_indent(owner) == 0 && markdown_containers_quote_depth(owner) == 0 &&
                !markdown_containers_in_list(owner) && !markdown_containers_task_start(owner) &&
                markdown_containers_pending(owner) == 0,
            "empty owner has root scope and no task or pending marker");
    require(markdown_containers_marker_indent(owner) == 0 &&
                markdown_containers_marker_number(owner) == 0 &&
                markdown_containers_marker_delimiter(owner) == '.' &&
                !markdown_containers_marker_ordered(owner),
            "optional marker queries are safe before the first accepted item");
    markdown_containers_consume_marker(owner);
    struct markdown_line root = markdown_line_read("root", 4, 0);
    require(markdown_containers_match(owner, &root) == MARKDOWN_CONTAINER_MATCH_MATCHED &&
                root.prefix == 0,
            "empty scope strictly matches a root line without altering its prefix");
    require(markdown_containers_accept(owner, root, root, (enum markdown_paragraph_state)99) ==
                    MARKDOWN_RTF_INVALID_INPUT &&
                markdown_containers_line(owner).length == 0,
            "invalid paragraph enum is rejected before the accepted line changes");
    struct markdown_line invalid = {.bytes = "x", .length = 1, .content = 2};
    require(markdown_containers_accept(owner, root, invalid, MARKDOWN_PARAGRAPH_STATE_NONE) ==
                    MARKDOWN_RTF_INVALID_INPUT &&
                markdown_containers_line(owner).length == 0,
            "malformed lookahead is rejected before root scope is committed");
    markdown_containers_destroy(owner);
    markdown_containers_destroy(nullptr);
}

static void verify_borrowed_column_rejections(void)
{
    struct markdown_line line = {.bytes = "x", .length = 1, .prefix = 1};
    require(!markdown_line_valid(line), "a consumed prefix cannot be beyond the content start");
    line = (struct markdown_line){.bytes = "x", .length = 1, .marker = 2};
    require(!markdown_line_valid(line), "marker bytes must fit the remaining borrowed content");
    line = (struct markdown_line){.bytes = "x", .length = 1, .padding = 4};
    require(!markdown_line_valid(line), "one partial tab cannot retain four virtual columns");
    line = (struct markdown_line){.bytes = "x", .length = 1, .column = SIZE_MAX, .indent = 1};
    require(!markdown_line_valid(line), "visual column plus remaining indent cannot overflow");
    line = (struct markdown_line){.bytes = "x",
                                  .length = 1,
                                  .prefix = 1,
                                  .content = 1,
                                  .column = 3,
                                  .padding = 1,
                                  .indent = 1};
    require(!markdown_line_valid(line), "virtual padding requires a preceding source tab");
    line = (struct markdown_line){.bytes = "\t",
                                  .length = 1,
                                  .prefix = 1,
                                  .content = 1,
                                  .column = 2,
                                  .padding = 2,
                                  .indent = 1};
    require(!markdown_line_valid(line), "virtual padding cannot exceed the remaining indent");
    line.column = 1;
    line.padding = 1;
    require(!markdown_line_valid(line), "partial tab suffix must reach the physical tab stop");
    line = markdown_line_read("  x", 3, 0);
    require(!markdown_line_skip(&line, 3) && line.prefix == 0,
            "column removal cannot exceed known whitespace");
    line = (struct markdown_line){.bytes = "x", .length = 1, .indent = 1};
    require(!markdown_line_skip(&line, 1) && line.prefix == 0,
            "inconsistent indent metadata cannot consume a nonspace byte");
    line = markdown_line_read("    > literal", 13, 0);
    require(!markdown_line_quote(&line) && line.prefix == 0,
            "four residual columns do not open a quote");
}

static void verify_borrowed_marker_rejections(void)
{
    struct markdown_line line = {.bytes = "x", .length = 1, .content = 2};
    require(!markdown_line_quote(&line) && !markdown_line_enter_list(&line),
            "quote and item consumers reject inconsistent content bounds");
    markdown_line_list(&line);
    require(line.marker == 0, "list classification checks bounds before looking for a marker");
    line = markdown_line_read("plain", 5, 0);
    require(!markdown_line_enter_list(&line), "ordinary text cannot be entered as a list item");
    line = (struct markdown_line){.bytes = "- x", .length = 3, .marker = 1, .marker_columns = 1};
    require(!markdown_line_enter_list(&line) && line.prefix == 0,
            "marker padding must contain at least one column");
    line = (struct markdown_line){.bytes = "-x", .length = 2, .marker = 1, .marker_columns = 3};
    require(!markdown_line_enter_list(&line) && line.prefix == 0,
            "padding metadata cannot strip the nonspace body after a marker");
    line = (struct markdown_line){
        .bytes = " - x", .length = 4, .content = 1, .indent = 2, .marker = 1, .marker_columns = 2};
    require(!markdown_line_enter_list(&line) && line.prefix == 0,
            "inconsistent marker indentation is rejected without changing the query");
}

static void verify_table_query_rejections(void)
{
    struct text_buffer *out = nullptr;
    require(text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED, "table rejection output");
    struct markdown_line header_line = markdown_line_read("A | B", 5, 0);
    struct markdown_line delimiter = markdown_line_read("-- | --", 7, 0);
    struct markdown_line invalid = {.bytes = "x", .length = 1, .content = 2};
    require(markdown_table_columns(invalid) == 0 && !markdown_table_delimiter(invalid) &&
                !markdown_table_starts(invalid, delimiter) &&
                !markdown_table_starts(header_line, invalid),
            "borrowed table classifiers reject malformed header and delimiter queries");
    require(
        markdown_table_row(out, invalid, delimiter,
                           (struct markdown_table_format){.kind = MARKDOWN_ROW_KIND_HEADER}) ==
                MARKDOWN_RTF_INVALID_INPUT &&
            markdown_table_row(out, header_line, invalid,
                               (struct markdown_table_format){.kind = MARKDOWN_ROW_KIND_BODY}) ==
                MARKDOWN_RTF_INVALID_INPUT &&
            text_buffer_length(out) == 0,
        "native row rejects malformed queries before appending partial RTF");
    struct text_buffer *wide = nullptr;
    require(text_buffer_create(&wide) == TEXT_BUFFER_ACCEPTED, "native ceiling delimiter");
    for (size_t column = 0; column < 64; ++column)
        text_buffer_append_text(wide, "| -- ");
    text_buffer_append_text(wide, "|");
    delimiter = markdown_line_read(text_buffer_bytes(wide), text_buffer_length(wide), 0);
    require(markdown_table_row(out, header_line, delimiter,
                               (struct markdown_table_format){.kind = MARKDOWN_ROW_KIND_HEADER}) ==
                    MARKDOWN_RTF_INVALID_INPUT &&
                text_buffer_length(out) == 0,
            "native row API rejects more than sixty-three columns rather than truncating");
    text_buffer_destroy(wide);
    text_buffer_destroy(out);
}

static void verify_nested_leaf_boundaries(void)
{
    expect_fragment("~~~\ncode\n~~~ \t\noutside", "\\cf1 outside", "~~~");
    expect_fragment("~~~\ncode\n    ~~~\n~~~\noutside", "    ~~~\\sa120", "code outside");
    expect_fragment("   ~~~\ncode\n~~~", " code\\sa120", "   code");
    expect_fragment("> A | B\n> -- | --\n> > nested", "\\li960\\sa80", "{> nested}\\cell");
    expect_fragment("7. parent\n   > A | B\n   > -- | --\n\n   resumed\n1. next", "8.\\tab }next",
                    "1.\\tab }next");
    expect_fragment("> A | B\n-- | --", "A | B", "\\trowd");
    expect_fragment("A | B\n--x | --", "--x | --", "\\trowd");
    struct markdown_rtf *rtf = convert_syntax("| \tA\t | B\t |\n| -- | -- |\n| \tx\t | y\t |");
    const char *body = markdown_rtf_text(rtf);
    require(strchr(body, '\t') == nullptr && strstr(body, "{\\b A}\\cell") != nullptr &&
                strstr(body, "{x}\\cell") != nullptr,
            "table cell leading and trailing tabs are trimmed as syntactic whitespace");
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
    expect_fragment("1) one\n9) two", "2)\\tab }two", "9)");
    expect_fragment("0. zero\n4. next", "1.\\tab }next", "4.");
    expect_fragment("paragraph\n2. literal", "paragraph 2. literal", "\\tab");
    expect_fragment("paragraph\n1. item", "1.\\tab }item", "paragraph 1.");
    expect_fragment("- parent\n  - child\n- next", "\\li880\\fi-220", "child next");
    expect_fragment("1. one\n\n   continued\n\n2. two", "2.\\tab }two", "1.\\tab }two");
    expect_fragment("> - item\n>   continued", "\\li920\\fi-220", "\\cf3 continued");
    expect_fragment("> > ## nested", "\\fs34\\li960 nested", "> ##");
}

static void verify_image_literal(void)
{
    expect_body("![**alt**](**destination**)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ![**alt**](**destination**)\\par\n");
    expect_body("![**alt**](destination)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ![**alt**](destination)\\par\n");
    expect_body("![alt](**destination**)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ![alt](**destination**)\\par\n");
    expect_body("![**alt** and ~~strike~~ and `code` and [link](url)](destination)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ![**alt** and ~~strike~~ and `code` and "
                "[link](url)](destination)\\par\n");
    expect_body("**before   ![**inside**](url) after**",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\b before   ![**inside**](url) after}\\par\n");
    expect_body("**outside ![inside**](url) tail**",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\b outside ![inside**](url) tail}\\par\n");
    expect_body("[before   ![**inside**](url) after](outer)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul before   ![**inside**](url) "
                "after\\ul0}\\par\n");
    expect_body(
        "![escaped \\*mark* and 日本 {x}](url)",
        "\\pard\\sa80\\sl240\\slmult1\\cf1 ![escaped \\\\*mark* and 日本 \\{x\\}](url)\\par\n");
    expect_body("![line\nbreak](url)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ![line break](url)\\par\n");
    expect_body("![**a](u)**", "\\pard\\sa80\\sl240\\slmult1\\cf1 ![**a](u)**\\par\n");
    expect_fragment("![**open**](url", "{\\b open}", "![**open**](url)");
    expect_fragment("| ![a\\|b](url) |\n|---|\n| c |", "![a|b](url)", "\\\\|");
}

static void verify_image_boundaries(void)
{
    expect_body("[\\![**a**](u)](v)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul ![{\\b a}](u)\\ul0}\\par\n");
    expect_body("[\\\\![**a**](u)](v)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul \\\\![**a**](u)\\ul0}\\par\n");
    expect_body("[![a](b](c))",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul ![a](b\\ul0})\\par\n");
    expect_body("[label]( ![hidden](url) )",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul label\\ul0}\\par\n");
    expect_body("![`[code](url)`](destination)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ![`[code](url)`](destination)\\par\n");
    expect_body("[label `![x](url)` after](outer)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul label "
                "{\\f1\\fs20\\cf4 ![x](url)} after\\ul0}\\par\n");
}

void run_markdown_link_tests(void)
{
    verify_image_literal();
    verify_image_boundaries();
    expect_body("[a]([b) [x](v)](u)", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul a\\ul0} "
                                      "{\\cf4\\ul x\\ul0}](u)\\par\n");
    expect_body("[a]([b](u)) [x](v)", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul a\\ul0} "
                                      "{\\cf4\\ul x\\ul0}\\par\n");
    expect_body("[a](u)[x](v)", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul a\\ul0}"
                                "{\\cf4\\ul x\\ul0}\\par\n");
    expect_body("[**a**](u) plain",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul {\\b a}\\ul0} plain\\par\n");
    expect_body("**[a](u)** [x](v)", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\b {\\cf4\\ul a\\ul0}} "
                                     "{\\cf4\\ul x\\ul0}\\par\n");
    expect_body("`[a](u)` [x](v)", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\f1\\fs20\\cf4 [a](u)} "
                                   "{\\cf4\\ul x\\ul0}\\par\n");
    expect_body("[a](`u) tail`) [x](v)", "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\cf4\\ul a\\ul0} "
                                         "{\\cf4\\ul x\\ul0}\\par\n");
    expect_fragment("*before [x](foo*) after", "*before {\\cf4\\ul x\\ul0} after", "\\i ");
    expect_fragment("*before [x](foo) after*", "{\\i before {\\cf4\\ul x\\ul0} after}", "foo");
    expect_fragment("![alt](url)", "![alt](url)", "\\ul");
    expect_body("\\![label](url)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 !{\\cf4\\ul label\\ul0}\\par\n");
    expect_body("\\\\![alt](url)", "\\pard\\sa80\\sl240\\slmult1\\cf1 \\\\![alt](url)\\par\n");
    expect_body("\\\\\\![label](url)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 \\\\!{\\cf4\\ul label\\ul0}\\par\n");
    expect_body("! [label](url)",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 ! {\\cf4\\ul label\\ul0}\\par\n");
    expect_body("`\\![label](url)`",
                "\\pard\\sa80\\sl240\\slmult1\\cf1 {\\f1\\fs20\\cf4 \\\\![label](url)}\\par\n");
    expect_body("\\![label](url", "\\pard\\sa80\\sl240\\slmult1\\cf1 ![label](url\\par\n");
    expect_body("\\![label(url)", "\\pard\\sa80\\sl240\\slmult1\\cf1 ![label(url)\\par\n");
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
    run_markdown_link_tests();
    expect_fragment("\\~~literal\\~~", "~~literal~~", "\\strike");
    expect_fragment("~~~ lang\nraw\n```\n~~~~", "raw\\par\n", "lang");
    expect_fragment("````\na\n```\nb\n````", "```\\par\n", "\\cf1 b");
    expect_fragment("- item\n  ```\n  code\noutside", "\\cf1 outside", "code outside");
    expect_fragment("```\na\n``` trailing\nb\n```", "``` trailing\\par\n", "\\cf1 b");
}

void run_markdown_table_setext_tests(void)
{
    expect_body("a\\|b\n---", "\\pard\\sb200\\sa80\\b\\cf2\\fs34 a|b\\b0\\cf1\\fs22\\par\n");
}

void run_markdown_table_bare_tests(void)
{
    expect_fragment("head\n|---|", "\\trowd", "\\cf1 head |---|");
}

static void verify_table_start_queries(void)
{
    struct markdown_line delimiter = markdown_line_read("|---|", 5, 0);
    static const char *_Nonnull const rejected[] = {"", "  ", "# title|", "***", "~~~lang|"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
        require(!markdown_table_starts(
                    markdown_line_read(rejected[index], strlen(rejected[index]), 0), delimiter),
                "blank, heading, thematic and fence cannot start a table");
    struct markdown_line header_line = markdown_line_read("head", 4, 0);
    require(markdown_table_starts(header_line, delimiter), "one-column header needs no pipe");
    delimiter = markdown_line_read(":---:", 5, 0);
    require(markdown_table_starts(header_line, delimiter), "colon delimiter needs no pipe");
    delimiter = markdown_line_read("---", 3, 0);
    require(!markdown_table_starts(markdown_line_read("a\\|b", 4, 0), delimiter) &&
                !markdown_table_starts(markdown_line_read("|head|", 6, 0), delimiter),
            "setext delimiter wins independently of header pipe spelling");
}

static void verify_table_start_headings(void)
{
    run_markdown_table_setext_tests();
    expect_body("|head|\n---", "\\pard\\sb200\\sa80\\b\\cf2\\fs34 |head|\\b0\\cf1\\fs22\\par\n");
    expect_body("head\n--", "\\pard\\sb200\\sa80\\b\\cf2\\fs34 head\\b0\\cf1\\fs22\\par\n");
    expect_body("head\n-", "\\pard\\sb200\\sa80\\b\\cf2\\fs34 head\\b0\\cf1\\fs22\\par\n");
    expect_body("head\n===", "\\pard\\sb200\\sa80\\b\\cf2\\fs44 head\\b0\\cf1\\fs22\\par\n");
    expect_body("before\n|head|\n---",
                "\\pard\\sb200\\sa80\\b\\cf2\\fs34 before |head|\\b0\\cf1\\fs22\\par\n");
    expect_body("before\n---\n|---|",
                "\\pard\\sb200\\sa80\\b\\cf2\\fs34 before\\b0\\cf1\\fs22\\par\n"
                "\\pard\\sa80\\sl240\\slmult1\\cf1 |---|\\par\n");
    expect_body("before\n===\n|---|",
                "\\pard\\sb200\\sa80\\b\\cf2\\fs44 before\\b0\\cf1\\fs22\\par\n"
                "\\pard\\sa80\\sl240\\slmult1\\cf1 |---|\\par\n");
    expect_fragment("===\n|---|", "{\\b ===}\\cell", "\\fs44");
}

static void verify_table_start_blocks(void)
{
    expect_body("# title|\n|---|", "\\pard\\sb200\\sa80\\b\\cf2\\fs44 title|\\b0\\cf1\\fs22\\par\n"
                                   "\\pard\\sa80\\sl240\\slmult1\\cf1 |---|\\par\n");
    expect_body("~~~lang|\n|---|\n~~~\ntail",
                "{\\f1\\fs20\\cf5\\highlight6\\pard\\li180\\ri180\\sb120\\sa0\\sl240\\slmult1 "
                "|---|\\sa120\\par\n}\\pard\\sa80\\sl240\\slmult1\\cf1 tail\\par\n");
    expect_body("***\n|---|", "{\\pard\\sb0\\sa0\\sl-240\\slmult0\\fs2\\highlight0\\par}\n{"
                              "\\pard\\sb0\\sa0\\sl-20\\slmult0\\fs2\\highlight3\\par}\n{"
                              "\\pard\\sb0\\sa0\\sl-240\\slmult0\\fs2\\highlight0\\par}\n"
                              "\\pard\\sa80\\sl240\\slmult1\\cf1 |---|\\par\n");
    expect_body("\n|---|", "\\pard\\sa80\\sl240\\slmult1\\cf1 |---|\\par\n");
}

static void verify_table_start_containers(void)
{
    expect_body("> a\\|b\n> ---",
                "\\pard\\sb200\\sa80\\b\\cf2\\fs34\\li480 a|b\\b0\\cf1\\fs22\\par\n");
    expect_body("- a\\|b\n  ---",
                "\\pard\\sb0\\sa80\\b\\cf2\\fs34\\li440\\fi-220\\tx440 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 \\bullet\\tab }a|b\\b0\\cf1\\fs22\\par\n");
    expect_body("> quote\nlazy\\|text\n---",
                "\\pard\\li480\\sa80\\sl240\\slmult1\\cf3 quote lazy|text\\cf1\\par\n"
                "{\\pard\\sb0\\sa0\\sl-240\\slmult0\\fs2\\highlight0\\par}\n{\\pard\\sb0\\sa0\\sl-"
                "20\\slmult0\\fs2\\highlight3\\par}\n{\\pard\\sb0\\sa0\\sl-"
                "240\\slmult0\\fs2\\highlight0\\par}\n");
    expect_body("> a\n:---:", "\\pard\\li480\\sa80\\sl240\\slmult1\\cf3 a :---:\\cf1\\par\n");
    expect_fragment("> head\n> |---|\n> cell", "\\trleft480", "\\cf3 head");
    expect_fragment("- head\n  |---|\n  cell", "\\trleft440", "{\\b - head}");
    expect_fragment("> quote\nhead\n|---|", "\\trleft0", "\\trleft480");
}

void run_markdown_table_start_tests(void)
{
    verify_table_start_queries();
    verify_table_start_headings();
    verify_table_start_blocks();
    verify_table_start_containers();
    run_markdown_table_bare_tests();
    expect_fragment("head\n:---:", "\\qc {\\b head}\\cell", "\\cf1 head :---:");
    expect_fragment("a\\|b\n|---|", "{\\b a|b}\\cell", "\\\\|");
    expect_fragment("a"
                    "\\\\"
                    "\\|"
                    "b\n|---|",
                    "{\\b a\\\\|b}\\cell", "\\cellx4500");
    expect_fragment("a\\\\|b\n--- | ---", "{\\b a\\\\}\\cell", "a\\\\|b");
    expect_fragment("A | B\n--- | ---", "{\\b B}\\cell", "A | B");
    expect_body("A | B\n:---:", "\\pard\\sa80\\sl240\\slmult1\\cf1 A | B :---:\\par\n");
    expect_body("head\n|::|", "\\pard\\sa80\\sl240\\slmult1\\cf1 head |::|\\par\n");
}

static void verify_tables(void)
{
    run_markdown_table_start_tests();
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

static void verify_deep_mixed_containers(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "mixed depth source");
    for (size_t depth = 0; depth < 256; ++depth)
        text_buffer_append_text(source, "- > ");
    text_buffer_append_text(source, "deep\n\nroot");
    struct markdown_rtf *rtf = convert_syntax(text_buffer_bytes(source));
    require(strstr(markdown_rtf_text(rtf), "\\li235520") != nullptr &&
                strstr(markdown_rtf_text(rtf), "\\cf1 root\\par") != nullptr &&
                fragment_count(markdown_rtf_text(rtf), "\\bullet\\tab ") == 256,
            "512 ordered container frames terminate, retain every item, and return to root");
    markdown_rtf_destroy(rtf);
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

static size_t verify_render_allocation(const char *_Nonnull source)
{
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
    return rejected;
}

static void verify_syntax_allocation(void)
{
    (void)verify_render_allocation(
        "paragraph\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10"
        "\n\n- parent\n  - child\n    > > > > > > > > > quote\n\nA | B\n-- | "
        "--\n`x` | ~~y~~\n\ntail");
    (void)verify_render_allocation("<https://example.com/a_b> **&amp;&#x1F600;**\n\n---\n\n"
                                   "| <mail@example.org> | &#123; |\n| -- | -- |\n"
                                   "| `&amp;` | ![&amp;](url) |");
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
    require(
        markdown_table_row(out, header_line, delimiter,
                           (struct markdown_table_format){.kind = (enum markdown_row_kind)99}) ==
                MARKDOWN_RTF_INVALID_INPUT &&
            text_buffer_length(out) == 0,
        "invalid row enum is rejected before any RTF is appended");
    text_buffer_destroy(out);
}

static void expect_table_indent(const char *_Nonnull source, size_t indent_twips)
{
    struct markdown_rtf *rtf = convert_syntax(source);
    const char *body = markdown_rtf_text(rtf);
    char row[64];
    char first[64];
    char second[64];
    snprintf(row, sizeof row, "\\trowd\\trgaph80\\trleft%zu\\", indent_twips);
    snprintf(first, sizeof first, "\\cellx%zu\\", indent_twips + 4500);
    snprintf(second, sizeof second, "\\cellx%zu\\", indent_twips + 9000);
    require(fragment_count(body, row) == 2 && fragment_count(body, first) == 2 &&
                fragment_count(body, second) == 2,
            "header and body share canonical container indent and absolute cell edges");
    require(strstr(body, "\\pard\\sa80\\sl240\\slmult1\\cf1 Tail\\par") != nullptr,
            "following root paragraph restores ordinary indentation and cell style");
    markdown_rtf_destroy(rtf);
}

static void verify_table_indentation(void)
{
    expect_table_indent("H_A | H_B\n-- | --\nx | y\n\nTail", 0);
    expect_table_indent("> H_A | H_B\n> -- | --\n> x | y\n\nTail", 480);
    expect_table_indent("7. H_A | H_B\n   -- | --\n   x | y\n\nTail", 440);
    expect_table_indent("- > H_A | H_B\n  > -- | --\n  > x | y\n\nTail", 920);
    expect_table_indent("> - H_A | H_B\n>   -- | --\n>   x | y\n\nTail", 920);
    struct markdown_rtf *rtf =
        convert_syntax("7. H_A | H_B\n   -- | --\n   x | y\n1. next\n\nTail");
    const char *body = markdown_rtf_text(rtf);
    require(fragment_count(body, "7.\\tab ") == 1 && fragment_count(body, "8.\\tab }next") == 1 &&
                fragment_count(body, "\\trowd") == 2,
            "native table marker occurs once and following sibling retains numbering");
    markdown_rtf_destroy(rtf);
}

static void verify_table_format_limits(void)
{
    require(markdown_table_native(1, 0) && markdown_table_native(63, 32767) &&
                !markdown_table_native(0, 0) && !markdown_table_native(64, 0) &&
                !markdown_table_native(2, 32768) && !markdown_table_native(2, SIZE_MAX),
            "one product range predicate includes both boundaries and rejects excess");
    struct text_buffer *out = nullptr;
    require(text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED, "table format boundary output");
    struct markdown_line line = markdown_line_read("A | B", 5, 0);
    struct markdown_line delimiter = markdown_line_read("-- | --", 7, 0);
    struct markdown_table_format format = {.kind = MARKDOWN_ROW_KIND_HEADER, .indent_twips = 32768};
    require(markdown_table_row(out, line, delimiter, format) == MARKDOWN_RTF_INVALID_INPUT &&
                text_buffer_length(out) == 0,
            "above-policy indent is rejected before output or overflow");
    format.indent_twips = SIZE_MAX;
    require(markdown_table_row(out, line, delimiter, format) == MARKDOWN_RTF_INVALID_INPUT &&
                text_buffer_length(out) == 0,
            "an unrepresentable direct indent cannot wrap during cell edge addition");
    format.indent_twips = 32767;
    struct markdown_line malformed = markdown_line_read("bad | --", 8, 0);
    require(markdown_table_row(out, line, malformed, format) == MARKDOWN_RTF_INVALID_INPUT &&
                text_buffer_length(out) == 0,
            "direct row input requires actual delimiter syntax");
    require(markdown_table_row(out, line, delimiter, format) == MARKDOWN_RTF_CONVERTED &&
                strstr(text_buffer_bytes(out), "\\trleft32767\\") != nullptr &&
                strstr(text_buffer_bytes(out), "\\cellx37267\\") != nullptr &&
                strstr(text_buffer_bytes(out), "\\cellx41767\\") != nullptr,
            "inclusive policy boundary safely emits indent plus initial edges");
    text_buffer_destroy(out);
}

static void quoted_table_line(struct text_buffer *_Nonnull source, const char *_Nonnull lead,
                              size_t depth, const char *_Nonnull content)
{
    text_buffer_append_text(source, lead);
    for (size_t index = 0; index < depth; ++index)
        text_buffer_append_text(source, "> ");
    text_buffer_append_text(source, content);
    text_buffer_append_text(source, "\n");
}

static struct text_buffer *_Nonnull deep_table_source(size_t depth)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "deep table source");
    quoted_table_line(source, "7. ", depth, "**H_A** | [H_B](u)");
    quoted_table_line(source, "   ", depth, "-- | --");
    quoted_table_line(source, "   ", depth, "`x` | ~~y~~");
    text_buffer_append_text(source, "1. next\n\nTail");
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED, "deep table source complete");
    return source;
}

static void verify_literal_lines(const char *_Nonnull rtf,
                                 const struct text_buffer *_Nonnull source)
{
    size_t start = 0;
    for (size_t row = 0; row < 3; ++row)
    {
        struct markdown_line line =
            markdown_line_read(text_buffer_bytes(source), text_buffer_length(source), start);
        char expected[512];
        require(line.length < sizeof expected, "literal source line fits");
        memcpy(expected, line.bytes, line.length);
        expected[line.length] = '\0';
        require(strstr(rtf, expected) != nullptr,
                "above-policy literal preserves the complete original line and every marker");
        start = line.next;
    }
}

static void verify_deep_table_policy(void)
{
    struct text_buffer *native = deep_table_source(67);
    expect_table_indent(text_buffer_bytes(native), 32600);
    text_buffer_destroy(native);
    struct text_buffer *literal = deep_table_source(68);
    struct markdown_rtf *rtf = convert_syntax(text_buffer_bytes(literal));
    const char *body = markdown_rtf_text(rtf);
    verify_literal_lines(body, literal);
    require(strstr(body, "\\trowd") == nullptr && fragment_count(body, "7.") == 1 &&
                strstr(body, "7.\\tab ") == nullptr && strstr(body, "8.\\tab }next") != nullptr,
            "literal fallback consumes the original item marker once and keeps following sequence");
    markdown_rtf_destroy(rtf);
    text_buffer_destroy(literal);
}

size_t run_markdown_table_allocation_tests(void)
{
    struct text_buffer *native = deep_table_source(67);
    struct text_buffer *literal = deep_table_source(68);
    size_t rejected = verify_render_allocation(text_buffer_bytes(native));
    rejected += verify_render_allocation(text_buffer_bytes(literal));
    text_buffer_destroy(native);
    text_buffer_destroy(literal);
    return rejected;
}

void run_markdown_table_indent_tests(void)
{
    verify_tables();
    verify_literal_container_table();
    verify_table_query_rejections();
    verify_borrowed_input();
    verify_table_indentation();
    verify_table_format_limits();
    verify_deep_table_policy();
}

void run_markdown_task_tests(void)
{
    expect_body("- [x]\ttext", "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                               "{\\f0\\fs22\\cf1\\b0\\highlight0 \\u9745?\\tab }text\\par\n");
    expect_fragment("- [ ] open\n- [x] done\n- [X] done", "\\u9744?\\tab }open", "[x]");
    expect_fragment("- [x] done", "\\u9745?\\tab }done", "[x]");
    expect_fragment("- [X] done", "\\u9745?\\tab }done", "[X]");
    expect_same_rendering("- [ ]\topen", "- [ ] open");
    expect_same_rendering("- [x]\tdone", "- [x] done");
    expect_same_rendering("- [X]\tdone", "- [X] done");
    expect_body("- [ ]\t", "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                           "{\\f0\\fs22\\cf1\\b0\\highlight0 \\u9744?\\tab }\\par\n");
    expect_body("- [x]\t \ttext  ",
                "\\pard\\li440\\fi-220\\tx440\\sa40\\sl240\\slmult1\\cf1 "
                "{\\f0\\fs22\\cf1\\b0\\highlight0 \\u9745?\\tab } \ttext  \\par\n");
    expect_fragment("7. [x]\tdone\n1. next", "\\u9745?\\tab }done", "[x]");
    expect_fragment("7. [x]\tdone\n1. next", "8.\\tab }next", "1.\\tab }next");
    expect_fragment("> - [x] item", "\\u9745?\\tab }item", "[x]");
    expect_same_rendering("> - [x]\titem", "> - [x] item");
    expect_fragment("- > - [x] item", "\\u9745?\\tab }item", "[x]");
    expect_same_rendering("- > - [x]\titem", "- > - [x] item");

    expect_fragment("- > [x] quote", "[x] quote", "\\u9745?");
    expect_fragment("- > [x]\tquote", "[x]\tquote", "\\u9745?");
    expect_fragment("- item\n  [x] literal", "item [x] literal", "\\u9745?");
    expect_fragment("- item\n  [x]\tliteral", "item [x]\tliteral", "\\u9745?");
    expect_fragment("- [z] literal", "[z] literal", "\\u9745?");
    expect_fragment("- [z]\tliteral", "[z]\tliteral", "\\u9745?");
    expect_fragment("- [x) literal", "[x) literal", "\\u9745?");
    expect_fragment("- [x)\tliteral", "[x)\tliteral", "\\u9745?");
    expect_fragment("- [x]nospace", "[x]nospace", "\\u9745?");
    expect_fragment("- [x]", "[x]", "\\u9745?");
    expect_fragment("- [] literal", "[] literal", "\\u9744?");
    expect_fragment("- [\t]\ttext", "[\t]\ttext", "\\u9744?");
    expect_fragment("- [x]\xC2\xA0text", "[x]\xC2\xA0text", "\\u9745?");
    expect_fragment("- [\xC2\xA0]\ttext", "[\xC2\xA0]\ttext", "\\u9744?");
    expect_fragment("[x]\ttext", "[x]\ttext", "\\u9745?");
    expect_fragment("> [x]\ttext", "[x]\ttext", "\\u9745?");
    expect_fragment("- \\[x]\ttext", "[x]\ttext", "\\u9745?");
    expect_fragment("- `[x]\ttext`", "\\cf4 [x]\ttext}", "\\u9745?");
}

static void expect_inline_view(struct markdown_view view, const char *_Nonnull expected,
                               size_t length)
{
    struct text_buffer *out = nullptr;
    require(text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED, "direct inline output");
    require(markdown_inline_tokens_render(out, view) == MARKDOWN_RTF_CONVERTED &&
                text_buffer_finish(out) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_length(out) == length &&
                memcmp(text_buffer_bytes(out), expected, length) == 0,
            "direct inline bytes match expected RTF");
    text_buffer_destroy(out);
}

static void expect_inline_text(const char *_Nonnull source, const char *_Nonnull expected)
{
    struct markdown_span span = {.bytes = source, .length = strlen(source)};
    struct markdown_view view = {.spans = &span, .count = 1, .length = span.length};
    expect_inline_view(view, expected, strlen(expected));
}

static void append_inline_run(struct text_buffer *_Nonnull buffer, char character, size_t length)
{
    for (size_t index = 0; index < length; ++index)
        text_buffer_append(buffer, &character, 1);
}

static void verify_inline_backtick_storage(void)
{
    struct text_buffer *source = nullptr;
    struct text_buffer *expected = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_create(&expected) == TEXT_BUFFER_ACCEPTED,
            "long backtick buffers");
    append_inline_run(source, '`', 513);
    text_buffer_append_text(source, "x");
    append_inline_run(source, '`', 513);
    text_buffer_append_text(source, "a");
    append_inline_run(source, '`', 514);
    text_buffer_append_text(source, "b");
    text_buffer_append_text(expected, "{\\f1\\fs20\\cf4 x}a");
    append_inline_run(expected, '`', 514);
    text_buffer_append_text(expected, "b");
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_finish(expected) == TEXT_BUFFER_ACCEPTED,
            "long backtick bytes complete");
    struct markdown_span span = {.bytes = text_buffer_bytes(source),
                                 .length = text_buffer_length(source)};
    struct markdown_view view = {.spans = &span, .count = 1, .length = span.length};
    expect_inline_view(view, text_buffer_bytes(expected), text_buffer_length(expected));
    text_buffer_destroy(expected);
    text_buffer_destroy(source);
}

static void verify_inline_delimiter_storage(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "long delimiter source");
    text_buffer_append_text(source, "a");
    append_inline_run(source, '*', 1025);
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED, "long delimiter bytes complete");
    struct markdown_span span = {.bytes = text_buffer_bytes(source),
                                 .length = text_buffer_length(source)};
    struct markdown_view view = {.spans = &span, .count = 1, .length = span.length};
    expect_inline_view(view, span.bytes, span.length);
    text_buffer_destroy(source);
}

static void verify_inline_coherent_storage(void)
{
    struct text_buffer *source = nullptr;
    struct text_buffer *expected = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_create(&expected) == TEXT_BUFFER_ACCEPTED,
            "nested inline buffers");
    for (size_t index = 0; index < 32; ++index)
    {
        text_buffer_append_text(source, "*a ");
        text_buffer_append_text(expected, "{\\i a ");
    }
    text_buffer_append_text(source, "z");
    text_buffer_append_text(expected, "z");
    for (size_t index = 0; index < 32; ++index)
    {
        text_buffer_append_text(source, " a*");
        text_buffer_append_text(expected, " a}");
    }
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_finish(expected) == TEXT_BUFFER_ACCEPTED,
            "nested inline bytes complete");
    expect_inline_text(text_buffer_bytes(source), text_buffer_bytes(expected));
    text_buffer_destroy(expected);
    text_buffer_destroy(source);
}

static void verify_inline_literal_boundaries(void)
{
    expect_inline_text("ab![cd](u)ef", "ab![cd](u)ef");
    expect_inline_text("ab!/[cd](u)ef", "ab!/{\\cf4\\ul cd\\ul0}ef");
    expect_inline_text("日本!/{x}\\a", "日本!/\\{x\\}\\\\a");
    expect_inline_text("a\tb\rc", "a\tbc");
    expect_inline_text("a \nb", "a b");
    expect_inline_text("a  \nb", "a\\line b");
    expect_inline_text("a\\\nb", "a\\line b");
    expect_inline_text("a\\\\\nb", "a\\\\ b");
    expect_inline_text("\\`literal", "`literal");
}

static void verify_inline_borrowed_storage(void)
{
    const struct markdown_span spans[] = {{.bytes = "a  ", .length = 3},
                                          {.bytes = "", .length = 0, .offset = 4},
                                          {.bytes = "b", .length = 1, .offset = 5}};
    struct markdown_view view = {.spans = spans, .count = 3, .length = 6};
    expect_inline_view(view, "a\\line  b", strlen("a\\line  b"));
    struct markdown_span cell = {.bytes = "![a\\|b](u)", .length = strlen("![a\\|b](u)")};
    view = (struct markdown_view){
        .spans = &cell, .count = 1, .length = cell.length, .table_cell = true};
    expect_inline_view(view, "![a|b](u)", strlen("![a|b](u)"));
    cell = (struct markdown_span){.bytes = "`a\\|b`", .length = strlen("`a\\|b`")};
    view.length = cell.length;
    expect_inline_view(view, "{\\f1\\fs20\\cf4 a|b}", strlen("{\\f1\\fs20\\cf4 a|b}"));
}

static void verify_inline_space_boundaries(void)
{
    expect_inline_text(" abc def  ", " abc def  ");
    expect_inline_text(" abc def\nq", " abc def q");
    expect_inline_text("a b \nc d  \ne f   \ng", "a b c d\\line e f\\line g");
    expect_inline_text("a b \\\nc", "a b \\line c");
    expect_inline_text("*a b* [c d](u) [e] (v)", "{\\i a b} {\\cf4\\ul c d\\ul0} [e] (v)");
    expect_inline_text("` a b ` / `  `", "{\\f1\\fs20\\cf4 a b} / {\\f1\\fs20\\cf4   }");
    expect_inline_text("x ![a b](u v) y", "x ![a b](u v) y");
}

static void verify_inline_space_borrowed(void)
{
    const struct markdown_span spans[] = {{.bytes = " a b  ", .length = 6},
                                          {.bytes = "", .length = 0, .offset = 7},
                                          {.bytes = "c d", .length = 3, .offset = 8}};
    struct markdown_view view = {.spans = spans, .count = 3, .length = 11};
    expect_inline_view(view, " a b\\line  c d", strlen(" a b\\line  c d"));
    struct markdown_span cell = {.bytes = "![a b\\|c](u)", .length = strlen("![a b\\|c](u)")};
    view = (struct markdown_view){
        .spans = &cell, .count = 1, .length = cell.length, .table_cell = true};
    expect_inline_view(view, "![a b|c](u)", strlen("![a b|c](u)"));
    cell = (struct markdown_span){.bytes = "`a b\\|c`", .length = strlen("`a b\\|c`")};
    view.length = cell.length;
    expect_inline_view(view, "{\\f1\\fs20\\cf4 a b|c}", strlen("{\\f1\\fs20\\cf4 a b|c}"));
}

static void verify_inline_storage_guards(void)
{
    struct text_buffer *out = nullptr;
    require(text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED, "inline guard output");
    text_buffer_append_text(out, "kept");
    struct markdown_span span = {.bytes = "", .length = 0};
    struct markdown_view view = {.spans = &span, .count = 0, .length = 0};
    allocation_probe_fail_at(0);
    require(markdown_inline_tokens_render(out, view) == MARKDOWN_RTF_CONVERTED,
            "empty view does not require token storage");
    view.count = 1;
    span.length = view.length = SIZE_MAX;
    require(markdown_inline_tokens_render(out, view) == MARKDOWN_RTF_OUT_OF_MEMORY,
            "SIZE_MAX guard precedes borrowed source scanning");
    span.length = view.length = SIZE_MAX - 1;
    require(markdown_inline_tokens_render(out, view) == MARKDOWN_RTF_OUT_OF_MEMORY,
            "existing allocation length ceiling precedes source scanning");
    span.length = 0;
    span.offset = 1;
    view.length = 0;
    require(markdown_inline_tokens_render(out, view) == MARKDOWN_RTF_INVALID_INPUT,
            "invalid offsets remain invalid input before planning");
    require(allocation_probe_calls() == 0 && text_buffer_length(out) == 4 &&
                same_text(text_buffer_bytes(out), "kept"),
            "empty and rejected views allocate nothing or change existing output");
    allocation_probe_fail_at(0);
    text_buffer_destroy(out);
}

static bool verify_inline_literal_storage(size_t length, const char *_Nonnull label,
                                          const char *_Nonnull pattern)
{
    size_t pattern_length = strlen(pattern);
    require(pattern_length != 0, "inline storage pattern is nonempty");
    struct text_buffer *source = nullptr;
    struct text_buffer *out = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED,
            "inline literal storage buffers");
    for (size_t index = 0; index < length; ++index)
        text_buffer_append(source, pattern + index % pattern_length, 1);
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED, "inline literal source complete");
    struct markdown_span span = {.bytes = text_buffer_bytes(source), .length = length};
    struct markdown_view view = {.spans = &span, .count = 1, .length = length};
    allocation_probe_fail_at(0);
    enum markdown_rtf_outcome rendered = markdown_inline_tokens_render(out, view);
    size_t calls = allocation_probe_calls();
    size_t maximum = allocation_probe_calloc_max_bytes();
    allocation_probe_fail_at(0);
    constexpr size_t limit = 4096;
    bool observed = calls != 0;
    printf(
        "INLINE_STORAGE,%s,source_bytes=%zu,calloc_max_bytes=%zu,limit=%zu,calls=%zu,active=%d\n",
        label, length, maximum, limit, calls, (int)observed);
    require(rendered == MARKDOWN_RTF_CONVERTED && text_buffer_finish(out) == TEXT_BUFFER_ACCEPTED &&
                text_buffer_length(out) == length &&
                memcmp(text_buffer_bytes(out), text_buffer_bytes(source), length) == 0,
            "direct literal RTF is exactly the unchanged source bytes");
    text_buffer_destroy(out);
    text_buffer_destroy(source);
    if (observed)
        require(maximum != 0 && maximum <= limit,
                "literal inline metadata calloc request is observed and stays within 4096 bytes");
    return observed;
}

bool run_markdown_inline_phrase_tests(void)
{
    bool short_observed = verify_inline_literal_storage(7, "phrase-short", "abc def ");
    bool long_observed = verify_inline_literal_storage(64 * 1024, "phrase-long", "abc def ");
    if (!short_observed || !long_observed)
        printf("Inline phrase allocation observation inactive in canonical build\n");
    return short_observed && long_observed;
}

static void verify_calloc_observation_overflow(void)
{
    allocation_probe_fail_at(1);
    char *rejected = folio_probe_calloc(SIZE_MAX, 2);
    size_t maximum = allocation_probe_calloc_max_bytes();
    allocation_probe_fail_at(0);
    require(rejected == nullptr && maximum == SIZE_MAX,
            "calloc observation saturates without sending an overflowing request to the CRT");
    require(allocation_probe_calloc_max_bytes() == 0, "calloc maximum resets with fail_at");
}

bool run_markdown_inline_storage_tests(void)
{
    bool phrase_observed = run_markdown_inline_phrase_tests();
    bool short_observed = verify_inline_literal_storage(7, "short", "a");
    bool long_observed = verify_inline_literal_storage(64 * 1024, "long", "a");
    verify_calloc_observation_overflow();
    verify_inline_backtick_storage();
    verify_inline_delimiter_storage();
    verify_inline_coherent_storage();
    verify_inline_literal_boundaries();
    verify_inline_borrowed_storage();
    verify_inline_space_boundaries();
    verify_inline_space_borrowed();
    verify_inline_storage_guards();
    if (!short_observed || !long_observed)
        printf("Inline storage allocation observation inactive in canonical build\n");
    return phrase_observed && short_observed && long_observed;
}

static size_t verify_inline_metadata_allocation(void)
{
    const char *source = "literal **bold** `code` [label](url)";
    struct markdown_span span = {.bytes = source, .length = strlen(source)};
    struct markdown_view view = {.spans = &span, .count = 1, .length = span.length};
    size_t rejected = 0;
    for (size_t fail = 1; fail <= 2; ++fail)
    {
        struct text_buffer *out = nullptr;
        require(text_buffer_create(&out) == TEXT_BUFFER_ACCEPTED, "metadata OOM output");
        text_buffer_append_text(out, "kept");
        allocation_probe_fail_at(fail);
        enum markdown_rtf_outcome result = markdown_inline_tokens_render(out, view);
        size_t calls = allocation_probe_calls();
        allocation_probe_fail_at(0);
        if (calls == 0)
        {
            require(result == MARKDOWN_RTF_CONVERTED &&
                        text_buffer_finish(out) == TEXT_BUFFER_ACCEPTED,
                    "unhooked direct render completes");
            text_buffer_destroy(out);
            return 0;
        }
        require(result == MARKDOWN_RTF_OUT_OF_MEMORY && text_buffer_length(out) == 4 &&
                    same_text(text_buffer_bytes(out), "kept"),
                "each inline metadata allocation failure keeps existing buffer bytes");
        text_buffer_destroy(out);
        ++rejected;
    }
    return rejected;
}

size_t run_markdown_inline_storage_allocation_tests(void)
{
    size_t metadata = verify_inline_metadata_allocation();
    size_t factory =
        verify_render_allocation("literal **bold** `code` [label](url) ![**alt**](url)\nnext");
    printf("INLINE_STORAGE_OOM,metadata_rejected=%zu,factory_rejected=%zu\n", metadata, factory);
    if (metadata != 0 || factory != 0)
        require(metadata == 2 && factory != 0,
                "inline metadata and public factory OOM are both injected");
    return metadata + factory;
}

static void verify_autolink_rendering(void)
{
    expect_same_rendering("<https://example.com/a_b?q=2>", "[https://example.com/a_b?q=2](url)");
    expect_same_rendering("<name+tag@example-domain.org>", "[name+tag@example-domain.org](url)");
    expect_same_rendering("<AB+1.-:>", "[AB+1.-:](url)");
    expect_fragment("<https://example.com/**x**[a](b)>",
                    "{\\cf4\\ul https://example.com/**x**[a](b)\\ul0}", "{\\b x}");
    expect_fragment("<https://example.com/\\[\\>", "https://example.com/\\\\[\\\\\\ul0}", "{\\f1");
    expect_fragment("<https://example.com/&amp;>", "example.com/&amp;\\ul0}",
                    "example.com/&\\ul0}");
    expect_fragment("`<https://example.com/>`", "{\\f1\\fs20\\cf4 <https://example.com/>", "\\ul ");
    expect_fragment("\\<https://example.com/>", "<https://example.com/>", "\\ul ");
    expect_fragment("![<https://example.com/>](image)", "![<https://example.com/>](image)",
                    "\\ul ");
    expect_fragment("<https://x/`raw>`after`", "{\\f1\\fs20\\cf4 after}", "{\\f1\\fs20\\cf4 raw");
    expect_fragment("`before <https://x/`>", "{\\f1\\fs20\\cf4 before <https://x/}>", "\\ul ");
    expect_fragment("**<https://x/>**", "{\\b {\\cf4\\ul https://x/\\ul0}}", "<https:");
    expect_fragment("| <https://x/a\\|b> | &amp; |\n| -- | -- |", "https://x/a|b\\ul0}", "a\\\\|b");
    const char *const rejected[] = {"<>",
                                    "<h:x>",
                                    "<1x:y>",
                                    "<ab!c:d>",
                                    "<ab cd:e>",
                                    "<ab:c\t>",
                                    "<ab:c\177>",
                                    "<ab:c",
                                    "<@example.org>",
                                    "<x@>",
                                    "<x@a..b>",
                                    "<x@-host>",
                                    "<x@host->",
                                    "<x@host.>",
                                    "<x@y_>",
                                    "https://example.com"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
        expect_fragment(rejected[index], "\\cf1 ", "\\ul ");
}

static void verify_character_references(void)
{
    expect_same_rendering("&amp; &lt; &gt; &quot; &apos; &nbsp;", "& < > \" ' \xC2\xA0");
    expect_same_rendering("&#65; &#x41; &#X41; &#x65E5; &#128512;",
                          "A A A \xE6\x97\xA5 \xF0\x9F\x98\x80");
    expect_same_rendering("&#0; &#55296; &#xDFFF; &#1114112; &#9999999;",
                          "\xEF\xBF\xBD \xEF\xBF\xBD \xEF\xBF\xBD \xEF\xBF\xBD \xEF\xBF\xBD");
    expect_same_rendering("&#x10FFFF;", "\xF4\x8F\xBF\xBF");
    expect_same_rendering("&#123;&#92;&#125;", "{\\\\}");
    expect_same_rendering("&#42;plain&#42;", "\\*plain\\*");
    expect_same_rendering("A&#10;&#13;B", "A  B");
    expect_fragment("**&amp;** [&#65;](url)", "{\\b &} {\\cf4\\ul A\\ul0}", "&#65;");
    expect_fragment("`&amp;&#65;`", "{\\f1\\fs20\\cf4 &amp;&#65;}", "{\\f1\\fs20\\cf4 &A}");
    expect_fragment("```\n&amp;&#65;\n```", "&amp;&#65;", "&A");
    expect_fragment("![&amp;&#65;](image)", "![&amp;&#65;](image)", "![&A]");
    expect_fragment("\\&amp; &amp;amp;", "&amp; &amp;", "& &");
    const char *const rejected[] = {"&",     "&#",     "&#;",         "&#x;",        "&#X;",
                                    "&#xG;", "&#-1;",  "&#12345678;", "&#x1234567;", "&#65",
                                    "&AMP;", "&copy;", "&am;",        "&amp"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
        expect_fragment(rejected[index], rejected[index], "{\\b ");
}

static void verify_light_inline_bounds(void)
{
    const char *source = "<https://example.com>";
    struct markdown_span span = {.bytes = source, .length = strlen(source)};
    struct markdown_view view = {.spans = &span, .count = 1, .length = span.length};
    uint32_t value = 123;
    require(markdown_autolink_length(view, view.length) == 0 &&
                markdown_character_reference_read(view, view.length, &value) == 0 && value == 123,
            "inline matching at end leaves output unchanged");
    require(markdown_character_reference_read(view, 0, &value) == 0 && value == 123,
            "non-reference leaves output unchanged");
    char buffer[100];
    memset(buffer, 'a', sizeof buffer);
    buffer[0] = '<';
    buffer[33] = ':';
    buffer[34] = '>';
    buffer[35] = '\0';
    expect_fragment(buffer, "{\\cf4\\ul ", "<aaa");
    buffer[33] = 'a';
    buffer[34] = ':';
    buffer[35] = '>';
    buffer[36] = '\0';
    expect_fragment(buffer, "<aaa", "\\ul ");
    memset(buffer, 'a', sizeof buffer);
    buffer[0] = '<';
    buffer[1] = 'x';
    buffer[2] = '@';
    buffer[66] = '>';
    buffer[67] = '\0';
    expect_fragment(buffer, "{\\cf4\\ul x@", "<x@");
    buffer[66] = 'a';
    buffer[67] = '>';
    buffer[68] = '\0';
    expect_fragment(buffer, "<x@", "\\ul ");
    struct text_buffer *large = nullptr;
    require(text_buffer_create(&large) == TEXT_BUFFER_ACCEPTED, "long inline source");
    for (size_t index = 0; index < 4096; ++index)
        text_buffer_append_text(large, "<invalid-&unknown; ");
    text_buffer_append_text(large, "<https://valid/>");
    expect_fragment(text_buffer_bytes(large), "{\\cf4\\ul https://valid/\\ul0}", "{\\b ");
    text_buffer_destroy(large);
}

static void verify_rule_spacing(void)
{
    const char *const sources[] = {"***", "before\n\n***\n\nafter", "# Heading\n\n***\n\n## Next",
                                   "- item\n\n  ***\n\n  next", "***\n***"};
    for (size_t index = 0; index < sizeof sources / sizeof sources[0]; ++index)
        expect_fragment(sources[index], "{\\pard\\sb0\\sa0\\sl-240\\slmult0\\fs2\\highlight0\\par}",
                        "\\sa80\\sb120");
    expect_same_rendering("before\n***\nafter", "before\n\n***\n\nafter");
    expect_fragment("## Heading\n\nordinary", "\\sb200\\sa80", "\\sb240");
}

void run_markdown_tests(void)
{
    verify_autolink_rendering();
    verify_character_references();
    verify_light_inline_bounds();
    verify_rule_spacing();
    (void)run_markdown_inline_storage_tests();
    (void)run_markdown_inline_storage_allocation_tests();
    verify_line_endings();
    verify_line_bounds();
    run_markdown_source_state_tests();
    run_markdown_table_indent_tests();
    verify_container_paths();
    verify_container_markers();
    verify_container_leaves();
    verify_container_columns();
    verify_container_queries();
    verify_container_style_boundaries();
    run_markdown_task_tests();
    verify_empty_owner_queries();
    verify_borrowed_column_rejections();
    verify_borrowed_marker_rejections();
    verify_nested_leaf_boundaries();
    verify_new_blocks();
    verify_new_inline();
    verify_large_syntax();
    verify_deep_and_wide();
    verify_deep_mixed_containers();
    verify_wide_delimiter_cells();
    verify_syntax_allocation();
    (void)run_markdown_table_allocation_tests();
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
