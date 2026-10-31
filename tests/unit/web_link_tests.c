#include "allocation_probe.h"
#include "appearance_port.h"
#include "folio_state.h"
#include "markdown_rtf.h"
#include "markdown_web_link.h"
#include "note_text.h"
#include "persistence_port.h"
#include "regex_port.h"
#include "unit_tests.h"
#include "web_link_port.h"
#include "web_uri.h"
#include <stdio.h>
#include <string.h>

static void verify_uri_cases(void)
{
    const char *const accepted[] = {"http://a",
                                    "HTTPS://example.com/",
                                    "hTtPs://例え.jp/日本😀?a=1&b=2#x",
                                    "http://[::1]:80/a%20b",
                                    "https://example.com/a(b){c}",
                                    "https://example.com/?x=@&y='"};
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
    for (size_t index = 0; index < sizeof accepted / sizeof accepted[0]; ++index)
    {
        struct web_uri *uri = nullptr;
        require(web_uri_create(accepted[index], strlen(accepted[index]), &uri) == WEB_URI_ACCEPTED,
                "accepted web URI");
        require(same_text(web_uri_bytes(uri), accepted[index]) &&
                    web_uri_length(uri) == strlen(accepted[index]),
                "exact destination retained");
        web_uri_destroy(uri);
    }
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
    require(markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Arial", &rtf) ==
                MARKDOWN_RTF_CONVERTED,
            "field converted");
    require(strstr(markdown_rtf_text(rtf), expected) != nullptr &&
                strstr(markdown_rtf_text(rtf), absent) == nullptr,
            "field destination and structure");
    require(same_text(note_text_bytes(text), source), "render never changes Markdown source");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void verify_fields(void)
{
    expect_field("[**visible**](https://example.com/a\\(b\\)?x=1&amp;y=&#50;)",
                 "HYPERLINK \"https://example.com/a(b)?x=1&y=2\"", "&amp;");
    expect_field("[label](<https://example.com/{x}>)", "https://example.com/\\{x\\}", "<https");
    expect_field("<https://example.com/?x=&amp;>", "HYPERLINK \"https://example.com/?x=&amp;\"",
                 "\\b ");
    expect_field("[<https://inner/>](https://outer/)", "HYPERLINK \"https://outer/\"",
                 "HYPERLINK \"https://inner/");
    expect_field("[bad](https://example.com/&#10;)", "{\\cf4\\ul bad\\ul0}", "\\field");
    expect_field("[bad](https://example.com/&quot;)", "{\\cf4\\ul bad\\ul0}", "\\field");
    expect_field("[mail](mailto:a@b) [file](file:///C:/x) [relative](a.md)", "\\ul mail",
                 "\\field");
    expect_field("`[code](https://example.com)` ![image](https://example.com)",
                 "https://example.com", "\\field");
    expect_field("| [日😀](https://example.com/日本) | [B](http://b/) |\n| -- | -- |",
                 "HYPERLINK \"https://example.com/日本\"", "mailto:");
}

static size_t opened;
static enum web_link_outcome port_result;
static enum web_link_outcome open_uri(const struct web_uri *_Nonnull uri)
{
    require(same_text(web_uri_bytes(uri), "https://example.com/"),
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
                                .web_link = &links};
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
    const char source[] = "[**label**](https://example.com/?a=1&amp;b=2) <https://example.org/>";
    struct note_text *text = nullptr;
    require(note_text_create(source, sizeof source - 1, &text) == NOTE_TEXT_ACCEPTED, "OOM source");
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        struct markdown_rtf *rtf = nullptr;
        allocation_probe_fail_at(nth);
        enum markdown_rtf_outcome outcome =
            markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Arial", &rtf);
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
    verify_state();
    verify_uri_allocation();
    verify_render_allocation();
    printf("web URI, Markdown field, intent and allocation tests passed\n");
}
