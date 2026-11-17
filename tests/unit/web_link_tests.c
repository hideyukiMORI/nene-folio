#include "allocation_probe.h"
#include "appearance_port.h"
#include "folio_state.h"
#include "markdown_bare_link.h"
#include "markdown_references.h"
#include "markdown_rtf.h"
#include "markdown_web_link.h"
#include "note_text.h"
#include "persistence_port.h"
#include "product_link.h"
#include "regex_port.h"
#include "unit_tests.h"
#include "web_link_port.h"
#include "web_uri.h"
#include <stdio.h>
#include <string.h>

static void verify_uri_accepted(void)
{
    const char *const accepted[] = {"http://a",
                                    "HTTPS://example.com/",
                                    "hTtPs://例え.jp/日本😀?a=1&b=2#x",
                                    "http://[::1]:80/a%20b",
                                    "https://example.com/a(b){c}",
                                    "https://example.com/?x=@&y='"};
    /* ADR0100: the scheme is held lower case; every other byte stays as given. */
    const char *const held[] = {"http://a",
                                "https://example.com/",
                                "https://例え.jp/日本😀?a=1&b=2#x",
                                "http://[::1]:80/a%20b",
                                "https://example.com/a(b){c}",
                                "https://example.com/?x=@&y='"};
    const size_t schemes[] = {4, 5, 5, 4, 5, 5};
    for (size_t index = 0; index < sizeof accepted / sizeof accepted[0]; ++index)
    {
        struct web_uri *uri = nullptr;
        require(web_uri_create(accepted[index], strlen(accepted[index]), &uri) == WEB_URI_ACCEPTED,
                "accepted web URI");
        require(same_text(web_uri_bytes(uri), held[index]) &&
                    web_uri_length(uri) == strlen(accepted[index]) &&
                    web_uri_scheme_length(uri) == schemes[index],
                "destination retained with a canonical scheme");
        web_uri_destroy(uri);
    }
}

static void verify_uri_cases(void)
{
    verify_uri_accepted();
    const char *const rejected[] = {"",
                                    "http:",
                                    "http://",
                                    "https:///x",
                                    "https://?x",
                                    "http://#x",
                                    "httpx://a",
                                    "mailto:a@b",
                                    "file:///C:/x",
                                    "//host/path",
                                    "relative",
                                    "\\\\server\\file",
                                    "javascript:alert(1)",
                                    "https://u:p@a/",
                                    "https://a/\n",
                                    "https://a/\r",
                                    "https://a/\t",
                                    "https://a/ ",
                                    "https://a/\\x",
                                    "https://a/\"",
                                    "https://a/<",
                                    "https://a/>",
                                    "https://a/\xC0\xAF",
                                    "https://a/\xED\xA0\x80",
                                    "https://a/\x7F",
                                    "https://a/\xC2\xA0",
                                    "https://a/\xE3\x80\x80"};
    for (size_t index = 0; index < sizeof rejected / sizeof rejected[0]; ++index)
    {
        struct web_uri *uri = nullptr;
        require(web_uri_create(rejected[index], strlen(rejected[index]), &uri) ==
                        WEB_URI_REJECTED &&
                    uri == nullptr,
                "rejected input has no URI");
    }
}

static void verify_uri_boundary(void)
{
    char source[web_uri_byte_limit + 2];
    memset(source, 'a', sizeof source);
    memcpy(source, "https://a/", 10);
    struct web_uri *uri = nullptr;
    require(web_uri_create(source, web_uri_byte_limit, &uri) == WEB_URI_ACCEPTED,
            "exact byte limit accepted");
    source[10] = 'b';
    require(web_uri_bytes(uri)[10] == 'a', "URI owns immutable copy");
    struct web_uri *sentinel = uri;
    require(web_uri_create(source, web_uri_byte_limit + 1, &uri) == WEB_URI_REJECTED &&
                uri == sentinel,
            "over limit preserves output");
    source[11] = '\0';
    require(web_uri_create(source, 12, &uri) == WEB_URI_REJECTED && uri == sentinel,
            "embedded NUL rejected");
    web_uri_destroy(uri);
}

static void expect_field(const char *_Nonnull source, const char *_Nonnull expected,
                         const char *_Nonnull absent)
{
    struct note_text *text = nullptr;
    struct markdown_rtf *rtf = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "source ready");
    require(markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT),
                                (struct markdown_rtf_locale){"Arial", FOLIO_LANGUAGE_JA},
                                &rtf) == MARKDOWN_RTF_CONVERTED,
            "field converted");
    if (strstr(markdown_rtf_text(rtf), expected) == nullptr ||
        strstr(markdown_rtf_text(rtf), absent) != nullptr)
        fprintf(stderr, "source=%s\nRTF=%s\n", source, markdown_rtf_text(rtf));
    require(strstr(markdown_rtf_text(rtf), expected) != nullptr &&
                strstr(markdown_rtf_text(rtf), absent) == nullptr,
            "field destination and structure");
    require(same_text(note_text_bytes(text), source), "render never changes Markdown source");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void verify_fields(void)
{
    expect_field("[![a](b[c)](https://example.com/outer)",
                 "HYPERLINK \"HTTPS://example.com/outer\"", "](https");
    expect_field("[![a](b (title]x))](https://example.com/outer)",
                 "HYPERLINK \"HTTPS://example.com/outer\"", "title]x");
    expect_field("[label](https://example.com/a`b) `code`", "HYPERLINK \"HTTPS://example.com/a`b\"",
                 "a`b){");
    expect_field("[label](<https://example.com/a`b>) `code`",
                 "HYPERLINK \"HTTPS://example.com/a`b\"", "<https");
    expect_field("[label](https://example.com/plain \"a`b\") `code`", "{\\f1\\fs20\\cf4 code}",
                 "a`b");
    expect_field("[label](https://example.com/plain (a`b)) `code`",
                 "HYPERLINK \"HTTPS://example.com/plain\"", "a`b");

    expect_field("[label](https://example.com/plain \"caption(\")",
                 "HYPERLINK \"HTTPS://example.com/plain\"", "caption");
    expect_field("[label](https://example.com/plain \"caption)\")",
                 "HYPERLINK \"HTTPS://example.com/plain\"", "caption");
    expect_field("[label](https://example.com/plain 'caption(')",
                 "HYPERLINK \"HTTPS://example.com/plain\"", "caption");
    expect_field("[label](https://example.com/plain 'caption)')",
                 "HYPERLINK \"HTTPS://example.com/plain\"", "caption");
    expect_field("[label](<https://example.com/open(>)", "HYPERLINK \"HTTPS://example.com/open(\"",
                 "<https");
    expect_field("[label](<https://example.com/close)>)",
                 "HYPERLINK \"HTTPS://example.com/close)\"", "<https");

    expect_field("[**visible**](https://example.com/a\\(b\\)?x=1&amp;y=&#50;)",
                 "HYPERLINK \"HTTPS://example.com/a(b)?x=1&y=2\"", "&amp;");
    expect_field("[label](<https://example.com/{x}>)", "HTTPS://example.com/\\{x\\}", "<https");
    expect_field("<https://example.com/?x=&amp;>", "HYPERLINK \"HTTPS://example.com/?x=&amp;\"",
                 "\\b ");
    expect_field("[<https://inner/>](https://outer/)", "HYPERLINK \"HTTPS://outer/\"",
                 "HYPERLINK \"HTTPS://inner/");
    expect_field("[bad](https://example.com/&#10;)", "{\\cf4\\ul bad\\ul0}", "\\field");
    expect_field("[bad](https://example.com/&quot;)", "{\\cf4\\ul bad\\ul0}", "\\field");
    expect_field("[mail](mailto:a@b) [file](file:///C:/x) [relative](a.md)", "\\ul mail",
                 "\\field");
    expect_field("`[code](https://example.com)` ![image](https://example.com)",
                 "https://example.com", "\\field");
    expect_field("| [日😀](https://example.com/日本) | [B](http://b/) |\n| -- | -- |",
                 "HYPERLINK \"HTTPS://example.com/日本\"", "mailto:");
}

/* #850 / ADR0099: the bare link at `start` of `source` as one span, or "" when there is none. */
static void expect_bare(const char *_Nonnull source, size_t start, const char *_Nonnull link)
{
    struct markdown_span span = {.bytes = source, .length = strlen(source), .offset = 0};
    struct markdown_view view = {.spans = &span, .count = 1, .length = span.length};
    size_t length = markdown_bare_link_length(view, start, view.length);
    if (length != strlen(link) || strncmp(source + start, link, length) != 0)
        fprintf(stderr, "bare link in \"%s\" at %zu: %zu bytes, wanted \"%s\"\n", source, start,
                length, link);
    require(length == strlen(link) && strncmp(source + start, link, length) == 0,
            "bare link extent");
}

static void verify_bare_extent(void)
{
    /* GFM 0.29 extended autolink examples and cmark-gfm autolink_delim/check_domain. */
    expect_bare("www.commonmark.org", 0, "www.commonmark.org");
    expect_bare("www.commonmark.org/help", 0, "www.commonmark.org/help");
    expect_bare("www.commonmark.org.", 0, "www.commonmark.org");
    expect_bare("www.commonmark.org/a.b.", 0, "www.commonmark.org/a.b");
    expect_bare("www.google.com/search?q=Markup+(business)", 0,
                "www.google.com/search?q=Markup+(business)");
    expect_bare("www.google.com/search?q=Markup+(business)))", 0,
                "www.google.com/search?q=Markup+(business)");
    expect_bare("www.google.com/search?q=(business))+ok", 0,
                "www.google.com/search?q=(business))+ok");
    expect_bare("www.google.com/search?q=commonmark&hl=en", 0,
                "www.google.com/search?q=commonmark&hl=en");
    expect_bare("www.google.com/search?q=commonmark&hl;", 0, "www.google.com/search?q=commonmark");
    expect_bare("www.commonmark.org/he<lp", 0, "www.commonmark.org/he");
    expect_bare("http://commonmark.org", 0, "http://commonmark.org");
    expect_bare("https://encrypted.google.com/search?q=Markup+(business)", 0,
                "https://encrypted.google.com/search?q=Markup+(business)");
    expect_bare("HTTPS://Example.com/x", 0, "HTTPS://Example.com/x");
    expect_bare("http://localhost:8080/a", 0, "http://localhost:8080/a");
    expect_bare("https://a.b/x;", 0, "https://a.b/x");
    expect_bare("https://a.b/x?!.,:*_~'\"", 0, "https://a.b/x");
    expect_bare("https://a.b/x&amp;", 0, "https://a.b/x");
    expect_bare("https://a.b/x&a1;", 0, "https://a.b/x&a1");
    expect_bare("www.a_b.example.com/x", 0, "www.a_b.example.com/x");
    expect_bare("www.example.a_b.com", 0, "");
    expect_bare("www.example.com.a_b", 0, "");
    expect_bare("http://a_b.com", 0, "");
    expect_bare("www.", 0, "");
    expect_bare("www.x", 0, "www.x");
    expect_bare("WWW.example.com", 0, "");
    expect_bare("https://", 0, "");
    expect_bare("https://.", 0, "");
    expect_bare("ftp://example.com", 0, "");
    expect_bare("mailto:a@example.com", 0, "");
    /* Where a link may start: GFM, and after a non-ASCII character (ADR0099). */
    expect_bare("xhttps://a.b", 1, "");
    expect_bare("1https://a.b", 1, "https://a.b");
    expect_bare("see:https://a.b", 4, "https://a.b");
    expect_bare("(www.a.b)", 1, "www.a.b");
    expect_bare("*www.a.b*", 1, "www.a.b");
    expect_bare("xwww.a.b", 1, "");
    expect_bare("\x1fwww.a.b", 1, "");
    expect_bare("\twww.a.b", 1, "www.a.b");
    expect_bare("参照www.example.com", 6, "www.example.com");
    expect_bare("参照https://example.com", 6, "https://example.com");
    /* It ends at a non-ASCII character (ADR0099): */
    expect_bare("https://example.com/日本", 0, "https://example.com/");
    expect_bare("https://example.comを参照", 0, "https://example.com");
    expect_bare("https://example.com/a。", 0, "https://example.com/a");
    expect_bare("https://example.com/a）", 0, "https://example.com/a");
}

static void verify_bare_limits(void)
{
    char source[markdown_bare_link_domain_limit + 32];
    memset(source, 'a', sizeof source);
    memcpy(source, "https://", 8);
    source[8 + markdown_bare_link_domain_limit - 4] = '.';
    source[8 + markdown_bare_link_domain_limit] = '\0';
    expect_bare(source, 0, source);
    source[8 + markdown_bare_link_domain_limit] = 'a';
    source[8 + markdown_bare_link_domain_limit + 1] = '\0';
    expect_bare(source, 0, "");
    struct markdown_span span = {.bytes = "https://a.b/c", .length = 13, .offset = 0};
    struct markdown_view view = {.spans = &span, .count = 1, .length = 13};
    require(markdown_bare_link_length(view, 0, 12) == 12, "a link stops at the plain limit");
    require(markdown_bare_link_length(view, 0, 9) == 9, "the limit may end the domain");
    require(markdown_bare_link_length(view, 0, 8) == 0, "no domain before the limit");
    require(markdown_bare_link_length(view, 0, 14) == 0 &&
                markdown_bare_link_length(view, 13, 13) == 0,
            "out of range limits");
    struct web_uri *uri = nullptr;
    require(markdown_web_link_bare(view, 4, 4, &uri) == WEB_URI_REJECTED &&
                markdown_web_link_bare(view, 0, 14, &uri) == WEB_URI_REJECTED && uri == nullptr,
            "an empty or out of range bare link has no URI");
}

/* A bare link the URI factory refuses (past web_uri_byte_limit) stays plain text. */
static void verify_bare_refused(void)
{
    static char source[web_uri_byte_limit + 16];
    memset(source, 'a', sizeof source - 1);
    memcpy(source, "https://a.b/", 12);
    source[sizeof source - 1] = '\0';
    struct note_text *text = nullptr;
    struct markdown_rtf *rtf = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "long source");
    require(markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT),
                                (struct markdown_rtf_locale){"Arial", FOLIO_LANGUAGE_JA},
                                &rtf) == MARKDOWN_RTF_CONVERTED,
            "long bare link converted");
    require(strstr(markdown_rtf_text(rtf), "\\field") == nullptr &&
                strstr(markdown_rtf_text(rtf), "\\ul ") == nullptr &&
                strstr(markdown_rtf_text(rtf), "https://a.b/aaaa") != nullptr,
            "a refused bare link is plain text");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void expect_text(const char *_Nonnull source)
{
    /* Code blocks carry their own copy action field (#839); no field may open the web. */
    expect_field(source, "", "HYPERLINK \"HTTP");
}

static void verify_bare_fields(void)
{
    expect_field("see https://example.com/ now", "HYPERLINK \"HTTPS://example.com/\"", "<https");
    expect_field("go www.example.com/a.", "HYPERLINK \"HTTP://www.example.com/a\"",
                 "HYPERLINK \"www");
    expect_field("go www.example.com/a.", "{\\cf4\\ul www.example.com/a\\ul0}}}}{\\v  }.",
                 "a.\\ul");
    expect_field("詳細はhttps://example.com/xを参照", "HYPERLINK \"HTTPS://example.com/x\"",
                 "<https");
    expect_field("a https://a.example/x_y_z b", "HYPERLINK \"HTTPS://a.example/x_y_z\"", "\\i ");
    expect_field("(https://a.example/x)", "HYPERLINK \"HTTPS://a.example/x\"", "x)\"");
    expect_field("*https://a.example/x*", "{\\i {\\field", "x*");
    expect_field("**https://a.example/x**.", "HYPERLINK \"HTTPS://a.example/x\"", "x*");
    expect_field("https://a.example/x https://b.example/y", "HYPERLINK \"HTTPS://b.example/y\"",
                 "x https");
    expect_field("1. https://a.example/x\n2. www.b.example", "HYPERLINK \"HTTP://www.b.example\"",
                 "<https");
    expect_field("| https://a.example/x | b |\n| -- | -- |", "HYPERLINK \"HTTPS://a.example/x\"",
                 "x |");
    expect_field("https://a.example/&amp;x", "HYPERLINK \"HTTPS://a.example/&amp;x\"", "\\b ");
    expect_field("https://a.example/x![i](y.png)", "HYPERLINK \"HTTPS://a.example/x\"", "x!");
    expect_field("[https://a.example/x](https://b.example/)", "HYPERLINK \"HTTPS://b.example/\"",
                 "HYPERLINK \"HTTPS://a.example");
    /* Text that is not plain keeps its own meaning (ADR0099). */
    expect_text("`https://a.example/x`");
    expect_text("```\nhttps://a.example/x\n```");
    expect_text("https\\://a.example/x");
    expect_text("xhttps://a.example/x wwwa.b");
    expect_text("name@example.com");
    expect_text("![https://a.example/x](https://b.example/y.png)");
}

/* #861 / ADR0101: reference links resolve through the note's definitions. */
static void verify_reference_links(void)
{
    expect_field("[text][ref]\n\n[ref]: https://example.com/r",
                 "HYPERLINK \"HTTPS://example.com/r\"", "[ref]");
    expect_field("[ref][]\n\n[ref]: https://example.com/r", "{\\cf4\\ul ref\\ul0}", "[]");
    expect_field("see [ref].\n\n[ref]: https://example.com/r", "{\\cf4\\ul ref\\ul0}", "[ref]");
    expect_field("[ref]: https://example.com/r\n\n[text][ref]",
                 "HYPERLINK \"HTTPS://example.com/r\"", "]: https");
    expect_field("[Foo  Bar][]\n\n[foo bar]: https://example.com/case",
                 "HYPERLINK \"HTTPS://example.com/case\"", "[Foo");
    expect_field("[a][x]\n\n[x]: <https://example.com/a%20b> \"title\"",
                 "HYPERLINK \"HTTPS://example.com/a%20b\"", "title");
    expect_field("[a][x]\n\n[x]: https://example.com/first\n[x]: https://example.com/second",
                 "HYPERLINK \"HTTPS://example.com/first\"", "second");
    expect_field("[a][x]\n\n  [x]: https://example.com/indented",
                 "HYPERLINK \"HTTPS://example.com/indented\"", "]: https");
    expect_field("[a][x] [b][y]\n\n[x]: https://example.com/x\n[y]: https://example.com/y",
                 "HYPERLINK \"HTTPS://example.com/y\"", "]: https");
    expect_field("| [a][x] |\n| -- |\n\n[x]: https://example.com/cell",
                 "HYPERLINK \"HTTPS://example.com/cell\"", "]: https");
    expect_field("# [title][x]\n\n[x]: https://example.com/h",
                 "HYPERLINK \"HTTPS://example.com/h\"", "]: https");
    /* Not definitions, or not resolved: the text stays as written. */
    expect_field("[a][missing]\n\n[x]: https://example.com/x", "[a][missing]", "HYPERLINK");
    /* (The URL in a line that is not a definition is still a bare link, #850.) */
    expect_field("para\n[x]: https://example.com/x\n\n[a][x]", "para [x]: ", "\\ul a\\ul0");
    expect_field("```\n[x]: https://example.com/x\n```\n\n[a][x]", "]: https://example.com/x",
                 "\\ul a\\ul0");
    expect_field("    [x]: https://example.com/x\n\n[a][x]", "[a][x]", "\\ul a\\ul0");
    expect_field("[x]: https://example.com/x trailing\n\n[a][x]", "[a][x]", "\\ul a\\ul0");
    /* #863: an image reference is an image, never a link. */
    expect_field("![alt][x]\n\n[x]: https://example.com/i.png", "<nenefolio:image:0>", "HYPERLINK");
    expect_field("`[a][x]`\n\n[x]: https://example.com/x", "[a][x]", "HYPERLINK");
    expect_field("[a](https://example.com/inline)[x]\n\n[x]: https://example.com/x",
                 "HYPERLINK \"HTTPS://example.com/inline\"", "]: https");
    /* A definition is a link reference even when its destination is not a web URI. */
    expect_field("[a][x]\n\n[x]: mailto:a@b", "{\\cf4\\ul a\\ul0}", "HYPERLINK");
    expect_field("[a][*x*]\n\n[*x*]: https://example.com/star",
                 "HYPERLINK \"HTTPS://example.com/star\"", "\\i ");
}

/* The definition reader's edges: escapes, nesting, fences, empty and long labels, the limit. */
static void verify_reference_edges(void)
{
    expect_field("[a][x\\]y]\n\n[x\\]y]: https://example.com/esc",
                 "HYPERLINK \"HTTPS://example.com/esc\"", "]: https");
    expect_field("[a][x]\n\n[x[y]: https://example.com/nested\n", "[a][x]", "\\ul a\\ul0");
    expect_field("[a][x]\n\n[]: https://example.com/empty", "[a][x]", "\\ul a\\ul0");
    expect_field("[a][x]\n\n[x] https://example.com/nocolon", "[a][x]", "\\ul a\\ul0");
    expect_field("[a][x]\n\n[x]:", "[a][x]", "\\ul a\\ul0");
    expect_field("~~~\n[x]: https://example.com/a\n~~~\n[x]: https://example.com/b\n\n[a][x]",
                 "HYPERLINK \"HTTPS://example.com/b\"", "HYPERLINK \"HTTPS://example.com/a\"");
    expect_field("```\n[x]: https://example.com/a\n~~~\n[a][x]", "[a][x]", "\\ul a\\ul0");
    expect_field("# t\n[x]: https://example.com/after-heading\n\n[a][x]",
                 "HYPERLINK \"HTTPS://example.com/after-heading\"", "]: https");
    static char long_label[markdown_references_label_limit + 64];
    char *cursor = long_label;
    cursor += sprintf(cursor, "[a][");
    memset(cursor, 'l', markdown_references_label_limit + 1);
    cursor += markdown_references_label_limit + 1;
    sprintf(cursor, "]");
    expect_field(long_label, "[a][lll", "\\ul a\\ul0");
    /* Definitions beyond the limit are not kept. */
    static char many[(markdown_references_limit + 2) * 32];
    size_t used = (size_t)sprintf(many, "[a][z]\n\n");
    for (size_t index = 0; index <= markdown_references_limit; ++index)
        used += (size_t)sprintf(many + used, "[d%zu]: https://e.example/\n", index);
    used += (size_t)sprintf(many + used, "[z]: https://example.com/over\n");
    expect_field(many, "[a][z]", "\\ul a\\ul0");
}

/* #869 / ADR0103: footnote references are raised numbers in first-reference order; the
 * referred footnotes follow the note, a size smaller; definitions are not shown in place. */
static void verify_footnotes(void)
{
    expect_field("a[^n] b[^m] c[^n]\n\n[^m]: second\n[^n]: first",
                 "a{\\super\\cf4 1} b{\\super\\cf4 2} c{\\super\\cf4 1}", "[^n]");
    expect_field("a[^n] b[^m]\n\n[^m]: second\n[^n]: first", "fs18 1. first\\fs22", "]: first");
    expect_field("a[^n] b[^m]\n\n[^m]: second\n[^n]: first", "fs18 2. second\\fs22", "]: second");
    expect_field("a[^x]\n\n[^x]: has **bold** and [l](https://example.com/f)", "{\\b bold}",
                 "HYPERLINK \"HTTPS://example.com/f\"\"");
    expect_field("a[^x]\n\n[^x]: has **bold** and [l](https://example.com/f)",
                 "HYPERLINK \"HTTPS://example.com/f\"", "]: has");
    expect_field("a[^missing]\n\n[^x]: never", "a[^missing]", "never");
    expect_field("no reference\n\n[^x]: unused", "no reference", "unused");
    expect_field("a[^X]\n\n[^x]: case", "{\\super\\cf4 1}", "[^X]");
    /* A footnote label is never a link reference; the reference after `[a]` stands alone. */
    expect_field("[a][^x]\n\n[^x]: note", "[a]{\\super\\cf4 1}", "HYPERLINK");
    expect_field("![a][^x]\n\n[^x]: note", "[^x]", "\\super");
    expect_field("`[^x]`\n\n[^x]: note", "[^x]", "\\super");
    expect_field("# t[^x]\n\n| c[^y] |\n| -- |\n\n[^x]: one\n[^y]: two", "fs18 2. two", "]: two");
    expect_field("a[^x]\n\n[^x]: see[^y]\n[^y]: nested", "fs18 2. nested", "]: nested");
    expect_field("a[^x]\n\n[^x]:", "fs18 1. \\fs22", "[^x]");
}

static size_t opened;
static enum web_link_outcome port_result;
static const char *_Nonnull expected_uri = "https://example.com/";
static enum web_link_outcome open_uri(const struct web_uri *_Nonnull uri)
{
    require(same_text(web_uri_bytes(uri), expected_uri) &&
                web_uri_length(uri) == strlen(expected_uri),
            "only validated exact URI at port");
    opened += 1;
    return port_result;
}

static struct folio_state *_Nonnull create_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port persistence = test_adapter_port(adapter);
    struct appearance_port appearance = test_appearance_port();
    struct regex_port regex = test_regex_port();
    struct web_link_port links = {.open = open_uri};
    struct folio_ports ports = {.persistence = &persistence,
                                .appearance = &appearance,
                                .regex = &regex,
                                .web_link = &links,
                                .clipboard = test_clipboard_port(),
                                .transfer = test_transfer_port(),
                                .images = test_image_port()};
    struct folio_state *state = nullptr;
    require(folio_state_create(&ports, &state) == FOLIO_STATE_READY, "state for link intents");
    return state;
}

static void verify_state(void)
{
    struct persistence_adapter *adapter = test_adapter_create(
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#112233\",\"expanded\":true}]}",
        "{\"version\":1,\"notes\":[\"one\",\"two\",\"three\"]}");
    struct folio_state *state = create_state(adapter);
    opened = 0;
    const char url[] = "https://example.com/";
    require(folio_state_open_web_link(state, url, sizeof url - 1) == FOLIO_STATE_WEB_LINK_REJECTED,
            "no document cannot open");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "viewing a note");
    require(folio_state_open_web_link(state, "file:///C:/x", 12) == FOLIO_STATE_WEB_LINK_REJECTED &&
                opened == 0,
            "unsupported target never reaches port");
    port_result = WEB_LINK_OPENED;
    require(folio_state_open_web_link(state, url, sizeof url - 1) == FOLIO_STATE_READY &&
                opened == 1,
            "explicit view intent reaches port once");
    /* ADR0100: the pane reads the instruction's upper-case scheme; the port gets lower case. */
    require(folio_state_open_web_link(state, "HTTPS://example.com/", 20) == FOLIO_STATE_READY &&
                opened == 2,
            "an upper-case instruction scheme reaches the port in canonical lower case");
    opened = 1;
    port_result = WEB_LINK_UNAVAILABLE;
    require(folio_state_open_web_link(state, url, sizeof url - 1) ==
                FOLIO_STATE_WEB_LINK_UNAVAILABLE,
            "OS failure preserved");
    port_result = WEB_LINK_OUT_OF_MEMORY;
    require(folio_state_open_web_link(state, url, sizeof url - 1) == FOLIO_STATE_OUT_OF_MEMORY,
            "port OOM preserved");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing begins");
    require(folio_state_open_web_link(state, url, sizeof url - 1) ==
                    FOLIO_STATE_WEB_LINK_REJECTED &&
                opened == 3,
            "editing never launches");
    require(test_adapter_body_publications(adapter) == 0, "link intent never saves a note");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

/* ADR0097: 閉じた集合の全値が、検証を通る絶対httpsの固定URLを一つずつ持つ。 */
static const char *_Nonnull const product_uris[] = {
    [PRODUCT_LINK_REPOSITORY] = "https://github.com/hideyukiMORI/nene-folio",
    [PRODUCT_LINK_RELEASES] = "https://github.com/hideyukiMORI/nene-folio/releases",
    [PRODUCT_LINK_STORE] = "https://apps.microsoft.com/detail/9PCHS225SWBX",
    [PRODUCT_LINK_ISSUES] = "https://github.com/hideyukiMORI/nene-folio/issues"};

static void verify_product_table(void)
{
    require(sizeof product_uris / sizeof product_uris[0] == product_link_count,
            "expected table covers every product link");
    for (size_t link = 0; link < product_link_count; ++link)
    {
        const char *bytes = product_link_uri((enum product_link)link);
        size_t length = product_link_uri_length((enum product_link)link);
        require(same_text(bytes, product_uris[link]) && length == strlen(product_uris[link]),
                "product link has its exact official destination");
        require(strncmp(bytes, "https://", 8) == 0, "product link is absolute https");
        struct web_uri *uri = nullptr;
        require(web_uri_create(bytes, length, &uri) == WEB_URI_ACCEPTED,
                "product link passes the shared URI validation");
        web_uri_destroy(uri);
    }
    require(product_link_uri((enum product_link)product_link_count)[0] == '\0' &&
                product_link_uri_length((enum product_link)product_link_count) == 0,
            "a value outside the set has no destination");
}

static void expect_product_opened(struct folio_state *_Nonnull state, const char *_Nonnull message)
{
    for (size_t link = 0; link < product_link_count; ++link)
    {
        expected_uri = product_uris[link];
        opened = 0;
        require(folio_state_open_product_link(state, (enum product_link)link) ==
                        FOLIO_STATE_READY &&
                    opened == 1,
                message);
    }
}

static void verify_product_failures(struct folio_state *_Nonnull state)
{
    expected_uri = product_uris[PRODUCT_LINK_ISSUES];
    opened = 0;
    port_result = WEB_LINK_UNAVAILABLE;
    require(folio_state_open_product_link(state, PRODUCT_LINK_ISSUES) ==
                    FOLIO_STATE_WEB_LINK_UNAVAILABLE &&
                opened == 1,
            "product link preserves the OS failure");
    port_result = WEB_LINK_OUT_OF_MEMORY;
    require(folio_state_open_product_link(state, PRODUCT_LINK_ISSUES) ==
                    FOLIO_STATE_OUT_OF_MEMORY &&
                opened == 2,
            "product link preserves port OOM");
    require(folio_state_open_product_link(state, (enum product_link)product_link_count) ==
                    FOLIO_STATE_WEB_LINK_REJECTED &&
                opened == 2,
            "a value outside the set never reaches the port");
    port_result = WEB_LINK_OPENED;
    for (size_t nth = 1; nth <= 2; ++nth)
    {
        opened = 0;
        allocation_probe_fail_at(nth);
        enum folio_state_outcome outcome =
            folio_state_open_product_link(state, PRODUCT_LINK_ISSUES);
        size_t calls = allocation_probe_calls();
        allocation_probe_fail_at(0);
        if (calls != 0)
            require(outcome == FOLIO_STATE_OUT_OF_MEMORY && opened == 0,
                    "URI allocation failure never reaches the port");
    }
}

static void verify_product_state(void)
{
    struct persistence_adapter *adapter = test_adapter_create(
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#112233\",\"expanded\":true}]}",
        "{\"version\":1,\"notes\":[\"one\",\"two\",\"three\"]}");
    struct folio_state *state = create_state(adapter);
    port_result = WEB_LINK_OPENED;
    expect_product_opened(state, "product link opens once without a document");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "viewing a note for product links");
    expect_product_opened(state, "product link opens once while viewing");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing for product links");
    expect_product_opened(state, "product link opens once while editing");
    verify_product_failures(state);
    expected_uri = "https://example.com/";
    opened = 0;
    require(folio_state_open_web_link(state, expected_uri, strlen(expected_uri)) ==
                    FOLIO_STATE_WEB_LINK_REJECTED &&
                opened == 0,
            "product links leave the note-link guard unchanged");
    require(test_adapter_body_publications(adapter) == 0, "product link never saves a note");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

static void verify_uri_allocation(void)
{
    for (size_t nth = 1; nth <= 2; ++nth)
    {
        struct web_uri *uri = nullptr;
        allocation_probe_fail_at(nth);
        enum web_uri_outcome outcome = web_uri_create("https://example.com/", 20, &uri);
        size_t calls = allocation_probe_calls();
        allocation_probe_fail_at(0);
        if (calls != 0)
            require(outcome == WEB_URI_OUT_OF_MEMORY && uri == nullptr,
                    "URI allocation fails atomically");
        web_uri_destroy(uri);
    }
}

static void verify_render_allocation(void)
{
    const char source[] =
        "[**label**](https://example.com/?a=1&amp;b=2) <https://example.org/> www.example.net/x "
        "[r][d] note[^f]\n\n[d]: https://example.com/d\n[e]: https://example.com/e\n"
        "[^f]: a **footnote** with [a link](https://example.com/f)";
    struct note_text *text = nullptr;
    require(note_text_create(source, sizeof source - 1, &text) == NOTE_TEXT_ACCEPTED, "OOM source");
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        struct markdown_rtf *rtf = nullptr;
        allocation_probe_fail_at(nth);
        enum markdown_rtf_outcome outcome =
            markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT),
                                (struct markdown_rtf_locale){"Arial", FOLIO_LANGUAGE_JA}, &rtf);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? outcome == MARKDOWN_RTF_CONVERTED
                         : outcome == MARKDOWN_RTF_OUT_OF_MEMORY && rtf == nullptr,
                "field generation allocation failure is never partial success");
        markdown_rtf_destroy(rtf);
    }
    note_text_destroy(text);
}

void run_web_link_tests(void)
{
    verify_uri_cases();
    verify_uri_boundary();
    verify_fields();
    verify_bare_extent();
    verify_bare_limits();
    verify_bare_fields();
    verify_bare_refused();
    verify_reference_links();
    verify_reference_edges();
    verify_footnotes();
    verify_state();
    verify_product_table();
    verify_product_state();
    verify_uri_allocation();
    verify_render_allocation();
    printf("web URI, Markdown field, intent, product link and allocation tests passed\n");
}
