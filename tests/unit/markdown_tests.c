#include "markdown_rtf.h"
#include "note_text.h"
#include "text_buffer.h"
#include "unit_tests.h"

#include <stdio.h>
#include <string.h>

static const char header[] =
    "{\\rtf1\\ansi\\deff0{\\fonttbl{\\f0\\fnil Yu Gothic UI;}{\\f1\\fmodern Consolas;}}\\f0\\fs20";

/* markdown を変換し、文書の頭と末尾を除いた本体が expected と同じか。 */
static void expect_body(const char *_Nonnull markdown, const char *_Nonnull expected)
{
    struct note_text *text = nullptr;
    require(note_text_create(markdown, strlen(markdown), &text) == NOTE_TEXT_ACCEPTED, markdown);
    struct markdown_rtf *rtf = nullptr;
    require(markdown_rtf_create(text, &rtf) == MARKDOWN_RTF_CONVERTED, "convert");
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
    expect_body("hello", "\\pard\\sa120 hello\\par\n");
    expect_body("one\ntwo\n\nthree\n", "\\pard\\sa120 one two\\par\n\\pard\\sa120 three\\par\n");
    expect_body("a\r\nb\r\n", "\\pard\\sa120 a b\\par\n");
    expect_body("  \n\t\n", "");
    expect_body("x \\ { }", "\\pard\\sa120 x \\\\ \\{ \\}\\par\n");
}

static void verify_unicode(void)
{
    expect_body("\xE6\x97\xA5\xE6\x9C\xAC", "\\pard\\sa120 \\u26085?\\u26412?\\par\n");
    expect_body("\xC3\xA9", "\\pard\\sa120 \\u233?\\par\n");
    expect_body("\xEF\xBF\xBD", "\\pard\\sa120 \\u-3?\\par\n");
    expect_body("\xF0\x9F\x98\x80", "\\pard\\sa120 \\u-10179?\\u-8704?\\par\n");
}

static void verify_headings(void)
{
    expect_body("# Title", "\\pard\\sb240\\sa120\\b\\fs36 Title\\b0\\fs20\\par\n");
    expect_body("## Sub", "\\pard\\sb240\\sa120\\b\\fs30 Sub\\b0\\fs20\\par\n");
    expect_body("###### Six", "\\pard\\sb240\\sa120\\b\\fs20 Six\\b0\\fs20\\par\n");
    expect_body("####### Seven", "\\pard\\sa120 ####### Seven\\par\n");
    expect_body("#NoSpace", "\\pard\\sa120 #NoSpace\\par\n");
    expect_body("##", "\\pard\\sa120 ##\\par\n");
    expect_body("# *Em*", "\\pard\\sb240\\sa120\\b\\fs36 \\i Em\\i0 \\b0\\fs20\\par\n");
}

static void verify_emphasis(void)
{
    expect_body("**bold** text", "\\pard\\sa120 \\b bold\\b0  text\\par\n");
    expect_body("__bold__", "\\pard\\sa120 \\b bold\\b0 \\par\n");
    expect_body("*em* and _em_", "\\pard\\sa120 \\i em\\i0  and \\i em\\i0 \\par\n");
    expect_body("snake_case_name", "\\pard\\sa120 snake_case_name\\par\n");
    expect_body("**unclosed", "\\pard\\sa120 \\b unclosed\\b0 \\par\n");
    expect_body("*open", "\\pard\\sa120 \\i open\\i0 \\par\n");
    expect_body("\\*literal\\*", "\\pard\\sa120 *literal*\\par\n");
    expect_body("trailing\\", "\\pard\\sa120 trailing\\\\\\par\n");
    expect_body("A_B 1_2 x_9 Z_z", "\\pard\\sa120 A_B 1_2 x_9 Z_z\\par\n");
    expect_body("\\~ \\@ \\[x] \\{ \\a", "\\pard\\sa120 ~ @ [x] \\{ \\\\a\\par\n");
    expect_body("a\rb", "\\pard\\sa120 ab\\par\n");
}

static void verify_code(void)
{
    expect_body("use `x_y` here", "\\pard\\sa120 use {\\f1 x_y} here\\par\n");
    expect_body("`unclosed", "\\pard\\sa120 `unclosed\\par\n");
    expect_body("```\nint a;\n\n  b\n```\nafter", "\\pard\\li300\\f1\\fs18 int a;\\f0\\fs20\\par\n"
                                                  "\\pard\\li300\\f1\\fs18 \\f0\\fs20\\par\n"
                                                  "\\pard\\li300\\f1\\fs18   b\\f0\\fs20\\par\n"
                                                  "\\pard\\sa120 after\\par\n");
    expect_body("```c\n*raw*\n", "\\pard\\li300\\f1\\fs18 *raw*\\f0\\fs20\\par\n");
    expect_body("text\n```\ncode\n```", "\\pard\\sa120 text\\par\n"
                                        "\\pard\\li300\\f1\\fs18 code\\f0\\fs20\\par\n");
}

static void verify_lists_quotes_links(void)
{
    expect_body("- one\n* two\n+ three", "\\pard\\li500\\fi-250\\sa60\\bullet\\tab one\\par\n"
                                         "\\pard\\li500\\fi-250\\sa60\\bullet\\tab two\\par\n"
                                         "\\pard\\li500\\fi-250\\sa60\\bullet\\tab three\\par\n");
    expect_body("1. first\n12. twelfth", "\\pard\\li500\\fi-250\\sa60 1.\\tab first\\par\n"
                                         "\\pard\\li500\\fi-250\\sa60 12.\\tab twelfth\\par\n");
    expect_body("1.nospace", "\\pard\\sa120 1.nospace\\par\n");
    expect_body("1.", "\\pard\\sa120 1.\\par\n");
    expect_body("12x", "\\pard\\sa120 12x\\par\n");
    expect_body("-dash", "\\pard\\sa120 -dash\\par\n");
    expect_body("> quoted\n>bare", "\\pard\\li600\\sa60\\i quoted\\i0\\par\n"
                                   "\\pard\\li600\\sa60\\i bare\\i0\\par\n");
    expect_body("see [docs](https://x.example/) now", "\\pard\\sa120 see {\\ul docs} now\\par\n");
    expect_body("[no url]", "\\pard\\sa120 [no url]\\par\n");
    expect_body("[open](", "\\pard\\sa120 [open](\\par\n");
    expect_body("[a]b", "\\pard\\sa120 [a]b\\par\n");
    expect_body("para\n- item", "\\pard\\sa120 para\\par\n"
                                "\\pard\\li500\\fi-250\\sa60\\bullet\\tab item\\par\n");
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
    require(markdown_rtf_empty(&rtf) == MARKDOWN_RTF_CONVERTED &&
                markdown_rtf_length(rtf) == strlen(header) + 1,
            "empty document");
    markdown_rtf_destroy(rtf);
    markdown_rtf_destroy(nullptr);
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
