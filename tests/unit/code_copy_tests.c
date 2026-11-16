/* 閲覧のコードブロックのコピー（#839 / ADR0098）: 見出し行のRTF、集めた原文、上限、
 * 内部宛先の偽造不可、application の意図と偽のクリップボード。 */
#include "allocation_probe.h"
#include "appearance_port.h"
#include "appearance_snapshot.h"

#include "clipboard_port.h"
#include "code_copy_target.h"
#include "folio_state.h"
#include "markdown_media.h"
#include "markdown_rtf.h"
#include "note_text.h"
#include "persistence_port.h"
#include "regex_port.h"
#include "rtf_palette.h"
#include "text_buffer.h"
#include "theme_palette.h"

#include "ui_font.h"
#include "unit_tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct clipboard_adapter
{
    size_t writes;
    enum clipboard_outcome result;
    size_t length;
    char text[512];
};

static struct clipboard_adapter clipboard = {.result = CLIPBOARD_WRITTEN};

static enum clipboard_outcome record_write(struct clipboard_adapter *_Nonnull adapter,
                                           const char *_Nonnull bytes, size_t length)
{
    require(length < sizeof adapter->text, "the fake clipboard holds the text");
    adapter->writes += 1;
    adapter->length = length;
    memcpy(adapter->text, bytes, length);
    adapter->text[length] = '\0';
    return adapter->result;
}

const struct clipboard_port *_Nonnull test_clipboard_port(void)
{
    static const struct clipboard_port port = {.adapter = &clipboard, .write = record_write};
    return &port;
}

#define OPEN "{\\f1\\fs20\\cf5\\highlight6"
#define HEADER "\\ri180\\sb120\\sa80\\sl240\\slmult1 "
#define HEADER_TAB "\\ri0\\sb40\\sa40\\sl240\\slmult1 "
#define FIRST "\\ri180\\sb80\\sa0\\sl240\\slmult1 "
#define ACTION_OPEN "{\\field{\\*\\fldinst{HYPERLINK \"nenefolio-code:"
#define ACTION_LABEL "\"}}{\\fldrslt{\\f0\\fs18\\cf6\\highlight9\\ul0 \\~\\~"
#define ACTION_CLOSE "\\~\\~}}}"
#define COPY_JA "\xE3\x82\xB3\xE3\x83\x94\xE3\x83\xBC"
#define COPY_ZH "\xE5\xA4\x8D\xE5\x88\xB6"

static struct markdown_rtf *_Nonnull render(const char *_Nonnull markdown,
                                            enum folio_language language)
{
    struct note_text *text = nullptr;
    require(note_text_create(markdown, strlen(markdown), &text) == NOTE_TEXT_ACCEPTED, markdown);
    struct markdown_rtf *rtf = nullptr;
    struct markdown_rtf_locale locale = {.face = ui_font_face(language), .language = language};
    require(markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT), locale, &rtf) ==
                MARKDOWN_RTF_CONVERTED,
            "code block converts");
    note_text_destroy(text);
    return rtf;
}

/* 変換した本体に expected がちょうど含まれ、集めた原文が codes（nullptr で終わる）と同じか。 */
static void expect_blocks(const char *_Nonnull markdown, const char *_Nonnull expected,
                          const char *_Nullable const *_Nonnull codes)
{
    struct markdown_rtf *rtf = render(markdown, FOLIO_LANGUAGE_JA);
    if (strstr(markdown_rtf_text(rtf), expected) == nullptr)
    {
        fprintf(stderr, "  markdown: %s\n  rtf: %s\n", markdown, markdown_rtf_text(rtf));
        require(false, "code header rtf");
    }
    const struct markdown_media *media = markdown_rtf_media(rtf);
    require(media != nullptr, "view rendering owns media");
    size_t count = 0;
    for (; codes[count] != nullptr; ++count)
    {
        require(count < markdown_media_code_count(media), "every expected block is collected");
        const char *code = markdown_media_code_bytes(media, count);
        if (!same_text(code, codes[count]) ||
            markdown_media_code_length(media, count) != strlen(codes[count]))
        {
            fprintf(stderr, "  markdown: %s\n  code: %s\n", markdown, code);
            require(false, "collected code text");
        }
    }
    require(markdown_media_code_count(media) == count, "no other block is collected");
    markdown_rtf_destroy(rtf);
}

static void verify_header_rows(void)
{
    expect_blocks("```c\nint a;\n```",
                  OPEN "\\pard\\qr\\li180" HEADER_TAB
                       "{\\f0\\fs18\\cf9 c}{\\f0\\fs18\\cf9 \\u8195?}" ACTION_OPEN
                       "0" ACTION_LABEL COPY_JA ACTION_CLOSE "\\par\n"
                       "\\pard\\li180" FIRST,
                  (const char *const[]){"int a;", nullptr});
    expect_blocks("```\nplain\n```",
                  OPEN "\\pard\\qr\\li180" HEADER_TAB ACTION_OPEN
                       "0" ACTION_LABEL COPY_JA ACTION_CLOSE "\\par\n\\pard\\li180" FIRST
                       "plain\\sa120\\par\n}",
                  (const char *const[]){"plain", nullptr});
    /* 空のfence: 未指定は何も足さず、指定ありはラベルだけ。どちらも操作と原文を持たない。 */
    expect_blocks("```\n```", OPEN "}", (const char *const[]){nullptr});
    expect_blocks("```c\n```", OPEN "\\pard\\li180" HEADER "{\\f0\\fs18\\cf9 c}\\sa120\\par\n}",
                  (const char *const[]){nullptr});
    /* 空行1行だけのブロックはコピーする文字が無い。2行なら改行1つを持つ。 */
    expect_blocks("```\n\n```",
                  OPEN "\\pard\\li180\\ri180\\sb120\\sa0\\sl240\\slmult1 \\sa120\\par\n}",
                  (const char *const[]){nullptr});
    expect_blocks("```\n\n\n```", ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"\n", nullptr});
    /* 閉じていないfenceは末尾まで。末尾の改行は付けない。 */
    expect_blocks("```py\na = 1\n\nb = 2\n", ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"a = 1\n\nb = 2", nullptr});
    expect_blocks("~~~\n```\n~~~\ntext\n```sh\nls\n```",
                  ACTION_OPEN "1" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"```", "ls", nullptr});
}

static void verify_container_blocks(void)
{
    /* 引用とリストの接頭辞、fenceの字下げは原文に入らない。containerを出たら閉じる。 */
    expect_blocks("> ```c\n> int a;\n>   b\n> ```\n", "\\pard\\qr\\li660" HEADER_TAB,
                  (const char *const[]){"int a;\n  b", nullptr});
    expect_blocks("- item\n\n  ```\n  x\n    y\n  ```\n",
                  ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"x\n  y", nullptr});
    expect_blocks("> ```\n> quoted\nplain\n", ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"quoted", nullptr});
    expect_blocks("  ```\n   a\n b\n  ```", ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){" a\nb", nullptr});
    expect_blocks("```\n\tx\n```", ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"\tx", nullptr});
    expect_blocks("```\r\none\r\ntwo\r\n```\r\n", ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"one\ntwo", nullptr});
    /* Mermaidの図は対象外で、後ろの通常のブロックが 0 番になる。 */
    expect_blocks("```mermaid\ngraph LR\nA-->B\n```\n```\nx\n```",
                  ACTION_OPEN "0" ACTION_LABEL COPY_JA ACTION_CLOSE,
                  (const char *const[]){"x", nullptr});
}

/* 札は言語ラベルの対の反転（地が色表の 9、文字が 6）で、色表は10色のまま。高コントラストでは
 * 塗らず、本文の文字色（色表の 1）と下線で示す。 */
static void verify_action_colors(void)
{
    struct markdown_rtf *rtf = render("```\nx\n```", FOLIO_LANGUAGE_JA);
    require(strstr(markdown_rtf_text(rtf), "\\red240\\green198\\blue125;}") != nullptr,
            "the view document keeps the ten body colours and adds none for the tab");
    markdown_rtf_destroy(rtf);
    struct appearance_snapshot snapshot = {.theme = FOLIO_THEME_LIGHT,
                                           .contrast = FOLIO_CONTRAST_HIGH,
                                           .window = {0, 0, 0},
                                           .window_text = {255, 255, 255},
                                           .button = {1, 2, 3},
                                           .button_text = {4, 5, 6},
                                           .highlight = {255, 255, 0},
                                           .highlight_text = {0, 0, 0}};
    struct theme_palette high = appearance_snapshot_palette(&snapshot, FOLIO_THEME_CHOICE_SYSTEM);
    struct note_text *text = nullptr;
    require(note_text_create("```c\nx\n```", 10, &text) == NOTE_TEXT_ACCEPTED, "contrast source");
    struct markdown_rtf_locale locale = {.face = "Arial", .language = FOLIO_LANGUAGE_EN};
    require(markdown_rtf_create(text, &high, locale, &rtf) == MARKDOWN_RTF_CONVERTED,
            "contrast converts");
    const char *body = markdown_rtf_text(rtf);
    require(strstr(body, "\\red255\\green255\\blue255;}") != nullptr &&
                strstr(body, "\\red1\\green2\\blue3;") == nullptr,
            "high contrast keeps the window pair and never the button fill");
    require(strstr(body, "{\\fldrslt{\\f0\\fs18\\cf1\\ul \\~\\~Copy\\~\\~}}}") != nullptr &&
                strstr(body, "\\pard\\qr\\li180") != nullptr,
            "high contrast marks the right-aligned action with an underline");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

static void verify_languages(void)
{
    struct markdown_rtf *english = render("```\nx\n```", FOLIO_LANGUAGE_EN);
    require(strstr(markdown_rtf_text(english), ACTION_OPEN "0" ACTION_LABEL "Copy" ACTION_CLOSE) !=
                nullptr,
            "the English label");
    markdown_rtf_destroy(english);
    struct markdown_rtf *chinese = render("```\nx\n```", FOLIO_LANGUAGE_ZH_HANS);
    require(strstr(markdown_rtf_text(chinese), ACTION_OPEN "0" ACTION_LABEL COPY_ZH ACTION_CLOSE) !=
                nullptr,
            "the Chinese label");
    markdown_rtf_destroy(chinese);
}

/* 利用者の Markdown から内部の宛先は作れない。 */
static void verify_forgery(void)
{
    const char *const forged[] = {
        "[x](nenefolio-code:0)\n\n```\nsecret\n```",
        "<nenefolio-code:0>\n\n```\nsecret\n```",
        "[nenefolio-code:0](nenefolio-code:0)\n\n```\nsecret\n```",
        "nenefolio-code:0 HYPERLINK \"nenefolio-code:0\"\n\n```\nsecret\n```",
        "![x](nenefolio-code:0)\n\n```\nsecret\n```",
        "| [x](nenefolio-code:0) |\n| -- |\n\n```\nsecret\n```",
    };
    for (size_t index = 0; index < sizeof forged / sizeof forged[0]; ++index)
    {
        struct markdown_rtf *rtf = render(forged[index], FOLIO_LANGUAGE_JA);
        const char *text = markdown_rtf_text(rtf);
        const char *field = strstr(text, "fldinst{HYPERLINK \"nenefolio-code:");
        require(field != nullptr &&
                    strstr(field + 1, "fldinst{HYPERLINK \"nenefolio-code:") == nullptr,
                "only the renderer's own block action is a field");
        require(strstr(text, "\\fldinst") == field - 1,
                "user text never becomes an internal field");
        markdown_rtf_destroy(rtf);
    }
}

static void verify_target(void)
{
    struct text_buffer *buffer = nullptr;
    require(text_buffer_create(&buffer) == TEXT_BUFFER_ACCEPTED, "target buffer");
    code_copy_target_append(buffer, 255);
    require(same_text(text_buffer_bytes(buffer), "nenefolio-code:255"), "canonical target");
    size_t slot = 99;
    require(code_copy_target_parse(text_buffer_bytes(buffer), text_buffer_length(buffer), &slot) ==
                    CODE_COPY_TARGET_SLOT &&
                slot == 255,
            "the last slot round-trips");
    text_buffer_destroy(buffer);
    require(code_copy_target_parse("nenefolio-code:0", 16, &slot) == CODE_COPY_TARGET_SLOT &&
                slot == 0,
            "the first slot");
    const char *const others[] = {"",
                                  "nenefolio-code:",
                                  "nenefolio-code:256",
                                  "nenefolio-code:00",
                                  "nenefolio-code:01",
                                  "nenefolio-code:1000",
                                  "nenefolio-code:-1",
                                  "nenefolio-code:1a",
                                  "nenefolio-code: 1",
                                  "NENEFOLIO-CODE:1",
                                  "nenefolio-image:1",
                                  "https://example.com/nenefolio-code:1",
                                  "\"nenefolio-code:1\""};
    for (size_t index = 0; index < sizeof others / sizeof others[0]; ++index)
    {
        slot = 7;
        require(code_copy_target_parse(others[index], strlen(others[index]), &slot) ==
                        CODE_COPY_TARGET_OTHER &&
                    slot == 7,
                others[index]);
    }
}

/* 個数の上限と原文の上限。超えたブロックは表示だけを行い、操作と原文を持たない。 */
static void verify_limits(void)
{
    struct text_buffer *source = nullptr;
    require(text_buffer_create(&source) == TEXT_BUFFER_ACCEPTED, "limit source");
    for (size_t index = 0; index < markdown_media_display_limit + 1; ++index)
        text_buffer_append_text(source, "```\nx\n```\n");
    require(text_buffer_finish(source) == TEXT_BUFFER_ACCEPTED, "limit source complete");
    struct markdown_rtf *rtf = render(text_buffer_bytes(source), FOLIO_LANGUAGE_JA);
    const struct markdown_media *media = markdown_rtf_media(rtf);
    require(media != nullptr && markdown_media_code_count(media) == markdown_media_display_limit &&
                !markdown_media_overflow(media) && markdown_media_count(media) == 0,
            "code blocks stop at the limit without touching display slots");
    require(strstr(markdown_rtf_text(rtf), "nenefolio-code:255") != nullptr &&
                strstr(markdown_rtf_text(rtf), "nenefolio-code:256") == nullptr,
            "the block past the limit has no action");
    markdown_rtf_destroy(rtf);
    text_buffer_destroy(source);

    constexpr size_t large = 256 * 1024;
    char *body = malloc(large + 64);
    require(body != nullptr, "large block");
    memcpy(body, "```\n", 4);
    memset(body + 4, 'a', large + 1);
    memcpy(body + 4 + large + 1, "\n```\n```\nok\n```\n", 17);
    rtf = render(body, FOLIO_LANGUAGE_JA);
    media = markdown_rtf_media(rtf);
    require(media != nullptr && markdown_media_code_count(media) == 1 &&
                same_text(markdown_media_code_bytes(media, 0), "ok"),
            "an oversized block is displayed without a copy action");
    markdown_rtf_destroy(rtf);
    body[4 + large] = '\n'; /* ちょうど上限の長さ（large 文字）にする。 */
    memcpy(body + 4 + large + 1, "```\n", 5);
    rtf = render(body, FOLIO_LANGUAGE_JA);
    media = markdown_rtf_media(rtf);
    require(media != nullptr && markdown_media_code_count(media) == 1 &&
                markdown_media_code_length(media, 0) == large,
            "a block of exactly the limit is collected");
    markdown_rtf_destroy(rtf);
    free(body);
}

/* 原文の総量はMermaidの原文と同じ上限を共有する。超えるブロックは集めない。 */
static void verify_total_bytes(void)
{
    constexpr size_t block = 256 * 1024;
    constexpr size_t blocks = 16;
    struct markdown_media *media = nullptr;
    require(markdown_media_create("", &media) == IMAGE_READY, "bounded code collection");
    size_t slot = 99;
    for (size_t index = 0; index < blocks; ++index)
        require(markdown_media_begin_code(media, block, &slot) == IMAGE_READY && slot == index &&
                    markdown_media_end_code(media, slot) == IMAGE_READY,
                "blocks fill the shared source budget");
    require(markdown_media_begin_code(media, 1, &slot) == IMAGE_TOO_LARGE && slot == blocks - 1 &&
                markdown_media_code_count(media) == blocks && !markdown_media_overflow(media),
            "a block past the shared source budget is not collected and raises no media notice");
    markdown_media_destroy(media);
}

static void verify_render_allocation(void)
{
    /* 2つ目のブロックは集める入れ物の伸長も通る長さにする。 */
    char source[512] = "```c\nint a;\n  b\n```\n> ```\n> ";
    size_t length = strlen(source);
    memset(source + length, 'q', 300);
    length += 300;
    source[length++] = '\n';
    struct note_text *text = nullptr;
    require(note_text_create(source, length, &text) == NOTE_TEXT_ACCEPTED, "OOM source");
    struct markdown_rtf_locale locale = {.face = "Arial", .language = FOLIO_LANGUAGE_JA};
    size_t allocations = 0;
    for (size_t nth = 0; nth <= allocations; ++nth)
    {
        struct markdown_rtf *rtf = nullptr;
        allocation_probe_fail_at(nth);
        enum markdown_rtf_outcome outcome =
            markdown_rtf_create(text, folio_theme_palette(FOLIO_THEME_LIGHT), locale, &rtf);
        if (nth == 0)
            allocations = allocation_probe_calls();
        allocation_probe_fail_at(0);
        require(nth == 0 ? outcome == MARKDOWN_RTF_CONVERTED
                         : outcome == MARKDOWN_RTF_OUT_OF_MEMORY && rtf == nullptr,
                "code collection allocation failure is never partial success");
        markdown_rtf_destroy(rtf);
    }
    note_text_destroy(text);
}

static struct folio_state *_Nonnull create_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port persistence = test_adapter_port(adapter);
    struct appearance_port appearance = test_appearance_port();
    struct regex_port regex = test_regex_port();
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&persistence, &appearance, &regex), &state) ==
                FOLIO_STATE_READY,
            "state for copy intents");
    return state;
}

static void expect_copy(struct folio_state *_Nonnull state, enum clipboard_outcome port,
                        enum folio_state_outcome expected)
{
    size_t before = clipboard.writes;
    clipboard.result = port;
    require(folio_state_copy_code(state, 1) == expected && clipboard.writes == before + 1 &&
                same_text(clipboard.text, "second\n  line") && clipboard.length == 13,
            "the port receives exactly the block once");
    clipboard.result = CLIPBOARD_WRITTEN;
}

static void verify_state(void)
{
    struct persistence_adapter *adapter = test_adapter_create(
        "{\"version\":1,\"categories\":[{\"name\":\"A\",\"color\":\"#112233\",\"expanded\":true}]}",
        "{\"version\":1,\"notes\":[\"one\",\"two\",\"three\"]}");
    struct folio_state *state = create_state(adapter);
    clipboard.writes = 0;
    require(folio_state_copy_code(state, 0) == FOLIO_STATE_CODE_COPY_REJECTED,
            "no document has no block");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "editing a note");
    const char16_t body[] = u"```c\nfirst\n```\n\n> ```\n> second\n>   line\n> ```\n";
    size_t count = sizeof body / sizeof body[0] - 1;
    require(folio_state_copy_code(state, 0) == FOLIO_STATE_CODE_COPY_REJECTED,
            "editing never copies a block");
    require(folio_state_end_edit(state, body, count) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "the saved body is viewed");
    size_t publications = test_adapter_body_publications(adapter);
    uint64_t revision = folio_state_store_revision(state);
    require(clipboard.writes == 0, "rejections never reach the port");
    require(folio_state_copy_code(state, 0) == FOLIO_STATE_READY && clipboard.writes == 1 &&
                same_text(clipboard.text, "first"),
            "the first block reaches the port once");
    expect_copy(state, CLIPBOARD_WRITTEN, FOLIO_STATE_READY);
    expect_copy(state, CLIPBOARD_BUSY, FOLIO_STATE_CLIPBOARD_BUSY);
    expect_copy(state, CLIPBOARD_UNAVAILABLE, FOLIO_STATE_CLIPBOARD_UNAVAILABLE);
    expect_copy(state, CLIPBOARD_OUT_OF_MEMORY, FOLIO_STATE_OUT_OF_MEMORY);
    size_t writes = clipboard.writes;
    require(folio_state_copy_code(state, 2) == FOLIO_STATE_CODE_COPY_REJECTED &&
                folio_state_copy_code(state, (size_t)-1) == FOLIO_STATE_CODE_COPY_REJECTED &&
                clipboard.writes == writes,
            "a slot outside the current rendering never reaches the port");
    require(test_adapter_body_publications(adapter) == publications &&
                folio_state_store_revision(state) == revision &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                same_text(folio_state_pane_text(state),
                          "```c\nfirst\n```\n\n> ```\n> second\n>   line\n> ```\n"),
            "copying never changes the document, the mode or storage");
    /* 別のノートへ切り替えたら、前の描画の slot は残らない。 */
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_copy_code(state, 0) == FOLIO_STATE_CODE_COPY_REJECTED &&
                clipboard.writes == writes,
            "a stale slot of the previous rendering is rejected");
    folio_state_destroy(state);
    test_adapter_destroy(adapter);
}

void run_code_copy_tests(void)
{
    verify_header_rows();
    verify_container_blocks();
    verify_languages();
    verify_action_colors();
    verify_forgery();
    verify_target();
    verify_limits();
    verify_total_bytes();
    verify_render_allocation();
    verify_state();
    printf("code block copy header, source, target, limit and intent tests passed\n");
}
