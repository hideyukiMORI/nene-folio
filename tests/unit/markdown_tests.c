#include "markdown_rtf.h"
#include "note_text.h"
#include "rtf_palette.h"
#include "text_buffer.h"
#include "unit_tests.h"

#include <stdio.h>
#include <string.h>

static const char header[] =
    "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil Yu Gothic UI;}{\\f1\\fmodern Consolas;}}"
    "{\\colortbl;\\red43\\green49\\blue56;\\red16\\green20\\blue24;\\red107\\green114\\blue128;"
    "\\red47\\green90\\blue168;\\red213\\green216\\blue220;\\red27\\green31\\blue36;}"
    "\\f0\\fs22\\cf1";

/* markdown を変換し、文書の頭と末尾を除いた本体が expected と同じか。 */
static void expect_body(const char *_Nonnull markdown, const char *_Nonnull expected)
{
    struct note_text *text = nullptr;
    require(note_text_create(markdown, strlen(markdown), &text) == NOTE_TEXT_ACCEPTED, markdown);
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), &rtf) ==
                MARKDOWN_RTF_CONVERTED,
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
    expect_body("", "");
    expect_body("hello", "\\pard\\sa160\\sl300\\slmult1\\cf1 hello\\par\n");
    expect_body("one\ntwo\n\nthree\n", "\\pard\\sa160\\sl300\\slmult1\\cf1 one "
                                       "two\\par\n\\pard\\sa160\\sl300\\slmult1\\cf1 three\\par\n");
    expect_body("a\r\nb\r\n", "\\pard\\sa160\\sl300\\slmult1\\cf1 a b\\par\n");
    expect_body("  \n\t\n", "");
    expect_body("x \\ { }", "\\pard\\sa160\\sl300\\slmult1\\cf1 x \\\\ \\{ \\}\\par\n");
}

static void verify_unicode(void)
{
    expect_body("\xE6\x97\xA5\xE6\x9C\xAC",
                "\\pard\\sa160\\sl300\\slmult1\\cf1 \\u26085?\\u26412?\\par\n");
    expect_body("\xC3\xA9", "\\pard\\sa160\\sl300\\slmult1\\cf1 \\u233?\\par\n");
    expect_body("\xEF\xBF\xBD", "\\pard\\sa160\\sl300\\slmult1\\cf1 \\u-3?\\par\n");
    expect_body("\xF0\x9F\x98\x80",
                "\\pard\\sa160\\sl300\\slmult1\\cf1 \\u-10179?\\u-8704?\\par\n");
}

static void verify_headings(void)
{
    expect_body("# Title", "\\pard\\sb280\\sa120\\b\\cf2\\fs44 Title\\b0\\cf1\\fs22\\par\n");
    expect_body("## Sub", "\\pard\\sb280\\sa120\\b\\cf2\\fs34 Sub\\b0\\cf1\\fs22\\par\n");
    expect_body("###### Six", "\\pard\\sb280\\sa120\\b\\cf2\\fs22 Six\\b0\\cf1\\fs22\\par\n");
    expect_body("####### Seven", "\\pard\\sa160\\sl300\\slmult1\\cf1 ####### Seven\\par\n");
    expect_body("#NoSpace", "\\pard\\sa160\\sl300\\slmult1\\cf1 #NoSpace\\par\n");
    expect_body("##", "\\pard\\sa160\\sl300\\slmult1\\cf1 ##\\par\n");
    expect_body("# *Em*", "\\pard\\sb280\\sa120\\b\\cf2\\fs44 \\i Em\\i0 \\b0\\cf1\\fs22\\par\n");
}

static void verify_emphasis(void)
{
    expect_body("**bold** text", "\\pard\\sa160\\sl300\\slmult1\\cf1 \\b bold\\b0  text\\par\n");
    expect_body("__bold__", "\\pard\\sa160\\sl300\\slmult1\\cf1 \\b bold\\b0 \\par\n");
    expect_body("*em* and _em_",
                "\\pard\\sa160\\sl300\\slmult1\\cf1 \\i em\\i0  and \\i em\\i0 \\par\n");
    expect_body("snake_case_name", "\\pard\\sa160\\sl300\\slmult1\\cf1 snake_case_name\\par\n");
    expect_body("**unclosed", "\\pard\\sa160\\sl300\\slmult1\\cf1 \\b unclosed\\b0 \\par\n");
    expect_body("*open", "\\pard\\sa160\\sl300\\slmult1\\cf1 \\i open\\i0 \\par\n");
    expect_body("\\*literal\\*", "\\pard\\sa160\\sl300\\slmult1\\cf1 *literal*\\par\n");
    expect_body("trailing\\", "\\pard\\sa160\\sl300\\slmult1\\cf1 trailing\\\\\\par\n");
    expect_body("A_B 1_2 x_9 Z_z", "\\pard\\sa160\\sl300\\slmult1\\cf1 A_B 1_2 x_9 Z_z\\par\n");
    expect_body("\\~ \\@ \\[x] \\{ \\a",
                "\\pard\\sa160\\sl300\\slmult1\\cf1 ~ @ [x] \\{ \\\\a\\par\n");
    expect_body("a\rb", "\\pard\\sa160\\sl300\\slmult1\\cf1 ab\\par\n");
}

static void verify_code(void)
{
    expect_body("use `x_y` here", "\\pard\\sa160\\sl300\\slmult1\\cf1 use "
                                  "{\\f1\\fs20\\cf4 x_y} here\\par\n");
    expect_body("`unclosed", "\\pard\\sa160\\sl300\\slmult1\\cf1 `unclosed\\par\n");
    expect_body(
        "```\nint a;\n\n  b\n```\nafter",
        "\\pard\\li240\\sa40\\f1\\fs20\\cf5\\highlight6 int a;\\highlight0\\f0\\fs22\\cf1\\par\n"
        "\\pard\\li240\\sa40\\f1\\fs20\\cf5\\highlight6 \\highlight0\\f0\\fs22\\cf1\\par\n"
        "\\pard\\li240\\sa40\\f1\\fs20\\cf5\\highlight6   b\\highlight0\\f0\\fs22\\cf1\\par\n"
        "\\pard\\sa160\\sl300\\slmult1\\cf1 after\\par\n");
    expect_body(
        "```c\n*raw*\n",
        "\\pard\\li240\\sa40\\f1\\fs20\\cf5\\highlight6 *raw*\\highlight0\\f0\\fs22\\cf1\\par\n");
    expect_body(
        "text\n```\ncode\n```",
        "\\pard\\sa160\\sl300\\slmult1\\cf1 text\\par\n"
        "\\pard\\li240\\sa40\\f1\\fs20\\cf5\\highlight6 code\\highlight0\\f0\\fs22\\cf1\\par\n");
}

static void verify_lists_quotes_links(void)
{
    expect_body("- one\n* two\n+ three",
                "\\pard\\li440\\fi-220\\sa80\\sl300\\slmult1\\cf1\\bullet\\tab one\\par\n"
                "\\pard\\li440\\fi-220\\sa80\\sl300\\slmult1\\cf1\\bullet\\tab two\\par\n"
                "\\pard\\li440\\fi-220\\sa80\\sl300\\slmult1\\cf1\\bullet\\tab three\\par\n");
    expect_body("1. first\n12. twelfth",
                "\\pard\\li440\\fi-220\\sa80\\sl300\\slmult1\\cf1 1.\\tab first\\par\n"
                "\\pard\\li440\\fi-220\\sa80\\sl300\\slmult1\\cf1 12.\\tab twelfth\\par\n");
    expect_body("1.nospace", "\\pard\\sa160\\sl300\\slmult1\\cf1 1.nospace\\par\n");
    expect_body("1.", "\\pard\\sa160\\sl300\\slmult1\\cf1 1.\\par\n");
    expect_body("12x", "\\pard\\sa160\\sl300\\slmult1\\cf1 12x\\par\n");
    expect_body("-dash", "\\pard\\sa160\\sl300\\slmult1\\cf1 -dash\\par\n");
    expect_body("> quoted\n>bare", "\\pard\\li480\\sa100\\sl300\\slmult1\\cf3 quoted\\cf1\\par\n"
                                   "\\pard\\li480\\sa100\\sl300\\slmult1\\cf3 bare\\cf1\\par\n");
    expect_body("see [docs](https://x.example/) now",
                "\\pard\\sa160\\sl300\\slmult1\\cf1 see {\\cf4\\ul docs\\ul0} now\\par\n");
    expect_body("[no url]", "\\pard\\sa160\\sl300\\slmult1\\cf1 [no url]\\par\n");
    expect_body("[open](", "\\pard\\sa160\\sl300\\slmult1\\cf1 [open](\\par\n");
    expect_body("[a]b", "\\pard\\sa160\\sl300\\slmult1\\cf1 [a]b\\par\n");
    expect_body("para\n- item",
                "\\pard\\sa160\\sl300\\slmult1\\cf1 para\\par\n"
                "\\pard\\li440\\fi-220\\sa80\\sl300\\slmult1\\cf1\\bullet\\tab item\\par\n");
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
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_LIGHT), &rtf) ==
                    MARKDOWN_RTF_CONVERTED &&
                markdown_rtf_length(rtf) == strlen(header) + 1 &&
                same_text(markdown_rtf_text(rtf) + strlen(header), "}"),
            "empty document");
    markdown_rtf_destroy(rtf);
    require(markdown_rtf_empty(rtf_palette_for(FOLIO_THEME_DARK), &rtf) == MARKDOWN_RTF_CONVERTED &&
                strstr(markdown_rtf_text(rtf), "\\red195\\green200\\blue207;") != nullptr,
            "dark palette in the color table");
    markdown_rtf_destroy(rtf);
    markdown_rtf_destroy(nullptr);
    struct rtf_palette dark = rtf_palette_for(FOLIO_THEME_DARK);
    struct rtf_palette light = rtf_palette_for(FOLIO_THEME_LIGHT);
    require(dark.heading.red == 0xFF && light.heading.red == 0x10 &&
                dark.code_background.red == 0x06 && light.code_background.red == 0x1B,
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

void run_markdown_tests(void)
{
    verify_paragraphs();
    verify_unicode();
    verify_headings();
    verify_emphasis();
    verify_code();
    verify_lists_quotes_links();
    verify_note_text();
    verify_text_buffer();
}
