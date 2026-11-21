/* #1071 / ADR0118: 外から来た「開く」依頼の検証。 */
#include "allocation_probe.h"
#include "category_name.h"
#include "note_name.h"
#include "open_request.h"
#include "unit_tests.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct open_request *_Nonnull accepted(const char *_Nonnull link)
{
    struct open_request *request = nullptr;
    require(open_request_create(link, strlen(link), &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            link);
    return request;
}

static void expect(const char *_Nonnull link, enum open_request_outcome outcome)
{
    struct open_request *request = nullptr;
    enum open_request_outcome actual = open_request_create(link, strlen(link), &request);
    if (actual != outcome)
        fprintf(stderr, "open request: %s -> %d\n", link, (int)actual);
    require(actual == outcome && request == nullptr, link);
}

static void verify_full_link(void)
{
    struct open_request *request =
        accepted("nenefolio://open?category=work&note=minutes&lines=12-18&text=next%20step");
    struct open_request_lines lines = {0, 0};
    require(same_text(category_name_text(open_request_category(request)), "work") &&
                same_text(note_name_stem(open_request_note(request)), "minutes") &&
                open_request_lines(request, &lines) && lines.first == 12 && lines.last == 18 &&
                same_text(open_request_text(request), "next step") &&
                open_request_text_length(request) == 9,
            "every field of a full link is read");
    open_request_destroy(request);
}

static void verify_loose_forms(void)
{
    struct open_request *request =
        accepted("NeNeFolio:OPEN/?note=%E8%AD%B0%E4%BA%8B%E9%8C%B2.md&category=%E4%BB%95%E4%BA%8B");
    struct open_request_lines lines = {7, 7};
    require(same_text(category_name_text(open_request_category(request)), "仕事") &&
                same_text(note_name_stem(open_request_note(request)), "議事録") &&
                !open_request_lines(request, &lines) && lines.first == 7 &&
                same_text(open_request_text(request), "") && open_request_text_length(request) == 0,
            "scheme and action in any case, no slashes, any key order, UTF-8 names, .md dropped; "
            "lines and text are optional");
    open_request_destroy(request);

    request = accepted("nenefolio://open?category=a&note=b&lines=3&text=&future=%zz&x=1");
    require(open_request_lines(request, &lines) && lines.first == 3 && lines.last == 3 &&
                open_request_text_length(request) == 0,
            "one line is a range of one; an empty text is none; unknown keys are skipped unread");
    open_request_destroy(request);

    request = accepted("nenefolio://open?category=a&note=b&text=1+1%3D2%26more");
    require(same_text(open_request_text(request), "1+1=2&more"),
            "a plus stays a plus; = and & come through percent-encoding");
    open_request_destroy(request);
}

static void verify_malformed_shape(void)
{
    static const char *const links[] = {
        "",
        "nenefolio",
        "nenefolio://open",
        "nenefolio://open?",
        "nenefolio://close?category=a&note=b",
        "nenefolio://openx?category=a&note=b",
        "nenefolio:///open?category=a&note=b",
        "https://open?category=a&note=b",
        "nenefolio://open?category=a",
        "nenefolio://open?note=b",
        "nenefolio://open?category=a&note=b&",
        "nenefolio://open?category=a&&note=b",
        "nenefolio://open?category=a&note=b&=x",
        "nenefolio://open?category=a&note=b&flag",
        "nenefolio://open?category=a&note=b#top",
        "nenefolio://open?category=a&note=b&category=c",
        "nenefolio://open?category=a&note=b&note=c",
        "nenefolio://open?category=a&note=b&lines=1&lines=2",
        "nenefolio://open?category=a&note=b&text=x&text=y",
        "nenefolio://open?category=a&note=b&text=&text=y",
    };
    for (size_t index = 0; index < sizeof links / sizeof links[0]; ++index)
        expect(links[index], OPEN_REQUEST_MALFORMED);
}

static void verify_malformed_values(void)
{
    static const char *const links[] = {
        "nenefolio://open?category=a&note=b&text=%",
        "nenefolio://open?category=a&note=b&text=%4",
        "nenefolio://open?category=a&note=b&text=%4g",
        "nenefolio://open?category=a&note=b&text=%g4",
        "nenefolio://open?category=a&note=b&text=a%00b",
        "nenefolio://open?category=a&note=b&text=a%0Ab",
        "nenefolio://open?category=a&note=b&text=a%7Fb",
        "nenefolio://open?category=a&note=b&text=%FF",
        "nenefolio://open?category=a&note=b&text=%E3%81",
        "nenefolio://open?category=a&note=b&lines=",
        "nenefolio://open?category=a&note=b&lines=0",
        "nenefolio://open?category=a&note=b&lines=-3",
        "nenefolio://open?category=a&note=b&lines=3-",
        "nenefolio://open?category=a&note=b&lines=8-3",
        "nenefolio://open?category=a&note=b&lines=3-8-9",
        "nenefolio://open?category=a&note=b&lines=3,8",
        "nenefolio://open?category=a&note=b&lines=1x",
        "nenefolio://open?category=a&note=b&lines=10000000",
        "nenefolio://open?category=a&note=b&lines=1-10000000",
        "nenefolio://open?category=a&note=b&lines=99999999999999999999999999",
    };
    for (size_t index = 0; index < sizeof links / sizeof links[0]; ++index)
        expect(links[index], OPEN_REQUEST_MALFORMED);
}

/* 入力は長さ付きのバイト列で、終端を当てにしない。ちょうどの大きさの確保で端を確かめる
 * （AddressSanitizer が読み越しを止める）。 */
static void expect_exact(const char *_Nonnull link, size_t length,
                         enum open_request_outcome outcome)
{
    char *exact = malloc(length == 0 ? 1 : length);
    require(exact != nullptr, "exact buffer");
    memcpy(exact, link, length);
    struct open_request *request = nullptr;
    enum open_request_outcome actual = open_request_create(exact, length, &request);
    open_request_destroy(request);
    free(exact);
    if (actual != outcome)
        fprintf(stderr, "open request (exact): %.*s -> %d\n", (int)length, link, (int)actual);
    require(actual == outcome, "a link read from a buffer of exactly its length");
}

static void verify_buffer_ends(void)
{
    static const char *const cut[] = {
        "nenefolio://open?category=a&note=b&text=%",
        "nenefolio://open?category=a&note=b&text=%4",
        "nenefolio://open?category=a&note=b&text=x%",
        "nenefolio:",
        "nenefolio://",
        "nenefolio://open",
        "nenefolio://ope",
        "n",
    };
    for (size_t index = 0; index < sizeof cut / sizeof cut[0]; ++index)
        expect_exact(cut[index], strlen(cut[index]), OPEN_REQUEST_MALFORMED);
    static const char full[] = "nenefolio://open?category=a&note=b&text=%41";
    expect_exact(full, sizeof full - 1, OPEN_REQUEST_ACCEPTED);
    static const char nul[] = "nenefolio://open?category=a&note=b&text=a\0b";
    expect_exact(nul, sizeof nul - 1, OPEN_REQUEST_MALFORMED);
    static const char control[] = "nenefolio://open?category=a&note=b&text=a\tb";
    expect_exact(control, sizeof control - 1, OPEN_REQUEST_MALFORMED);
}

/* 形はリンクでも、動作・相手・キーの書き方が違うもの。 */
static void verify_strict_forms(void)
{
    static const char *const links[] = {
        "nenefolio://open:80?category=a&note=b",
        "nenefolio://user@open?category=a&note=b",
        "nenefolio://open//?category=a&note=b",
        "nenefolio://open?Category=a&note=b",
        "nenefolio://open?category=a&NOTE=b",
        "nenefolio://open?category=a&note=b&text=%C0%AF",
        "nenefolio://open?category=a&note=b&text=%ED%A0%80",
        "nenefolio://open?category=a&note=b&text=a%C2%85b",
        "nenefolio://open?category=a&note=b&text=a%E2%80%A8b",
        "nenefolio://open?category=a&note=b&text=a%E2%80%A9b",
    };
    for (size_t index = 0; index < sizeof links / sizeof links[0]; ++index)
        expect(links[index], OPEN_REQUEST_MALFORMED);
    expect("nenefolio://open?category=a&note=.md", OPEN_REQUEST_NAME_INVALID);
    struct open_request *request = accepted("nenefolio://open?category=a&note=x.md.md&lines=4-4");
    struct open_request_lines lines = {0, 0};
    require(same_text(note_name_stem(open_request_note(request)), "x.md") &&
                open_request_lines(request, &lines) && lines.first == 4 && lines.last == 4,
            "only one .md is dropped; a range of one line written N-N is accepted");
    open_request_destroy(request);
    request = accepted("nenefolio://open?category=a&note=b&text=%e3%81%82&pad=1&pad=2");
    require(same_text(open_request_text(request), "あ"),
            "hex digits in either case; an unknown key may repeat");
    open_request_destroy(request);
}

static void verify_names(void)
{
    static const char *const links[] = {
        "nenefolio://open?category=..&note=b",    "nenefolio://open?category=a%2Fb&note=b",
        "nenefolio://open?category=a%5Cb&note=b", "nenefolio://open?category=.hidden&note=b",
        "nenefolio://open?category=&note=b",      "nenefolio://open?category=a&note=..%2Fsecret",
        "nenefolio://open?category=a&note=C%3A",  "nenefolio://open?category=a&note=con",
        "nenefolio://open?category=a&note=",      "nenefolio://open?category=a&note=b.",
    };
    for (size_t index = 0; index < sizeof links / sizeof links[0]; ++index)
        expect(links[index], OPEN_REQUEST_NAME_INVALID);
}

static void verify_limits(void)
{
    char link[open_request_byte_limit + 64] = "nenefolio://open?category=a&note=b&text=";
    size_t head = strlen(link);
    memset(link + head, 'x', open_request_text_limit);
    link[head + open_request_text_limit] = '\0';
    struct open_request *request = accepted(link);
    require(open_request_text_length(request) == open_request_text_limit,
            "a text at the limit is accepted");
    open_request_destroy(request);
    link[head + open_request_text_limit] = 'x';
    link[head + open_request_text_limit + 1] = '\0';
    expect(link, OPEN_REQUEST_TOO_LONG);

    char longest[open_request_byte_limit + 2] = "nenefolio://open?category=a&note=b&pad=";
    head = strlen(longest);
    memset(longest + head, 'p', open_request_byte_limit - head);
    longest[open_request_byte_limit] = '\0';
    request = accepted(longest);
    open_request_destroy(request);
    longest[open_request_byte_limit] = 'p';
    longest[open_request_byte_limit + 1] = '\0';
    expect(longest, OPEN_REQUEST_TOO_LONG);
    request = accepted("nenefolio://open?category=a&note=b&lines=9999999");
    open_request_destroy(request);
}

/* 確保の失敗をどこで起こしても、型のある結果で戻り、何も漏らさない。 */
static void verify_allocation(void)
{
    static const char link[] = "nenefolio://open?category=work&note=minutes&lines=2-4&text=step";
    allocation_probe_fail_at(0);
    struct open_request *request = accepted(link);
    size_t calls = allocation_probe_calls();
    open_request_destroy(request);
    for (size_t nth = 1; nth <= calls; ++nth)
    {
        request = nullptr;
        allocation_probe_fail_at(nth);
        enum open_request_outcome outcome = open_request_create(link, sizeof link - 1, &request);
        allocation_probe_fail_at(0);
        require(outcome == OPEN_REQUEST_OUT_OF_MEMORY && request == nullptr,
                "an allocation failure while reading a link is typed");
    }
    printf("open request: %zu injected allocation failures\n", calls);
}

/* ADR0119 の決定 6: 依頼のファイルの中身。リンクと同じ型になり、同じ規則を通る。 */
static struct open_request *_Nonnull accepted_file(const char *_Nonnull json)
{
    struct open_request *request = nullptr;
    require(open_request_create_json(json, strlen(json), &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            json);
    return request;
}

static void expect_file(const char *_Nonnull json, enum open_request_outcome outcome)
{
    struct open_request *request = nullptr;
    enum open_request_outcome actual = open_request_create_json(json, strlen(json), &request);
    if (actual != outcome)
        fprintf(stderr, "open request file: %s -> %d\n", json, (int)actual);
    require(actual == outcome && request == nullptr, json);
}

static void verify_file_accepted(void)
{
    struct open_request *request = accepted_file(
        "{\"version\":1,\"action\":\"open\",\"category\":\"work\",\"note\":\"minutes\","
        "\"lines\":\"3-8\",\"text\":\"\xE6\xB1\xBA\xE5\xAE\x9A step\"}");
    struct open_request_lines lines = {0, 0};
    require(strcmp(category_name_text(open_request_category(request)), "work") == 0 &&
                strcmp(note_name_stem(open_request_note(request)), "minutes") == 0 &&
                open_request_lines(request, &lines) && lines.first == 3 && lines.last == 8 &&
                strcmp(open_request_text(request), "\xE6\xB1\xBA\xE5\xAE\x9A step") == 0,
            "a request file gives the same request as its link");
    open_request_destroy(request);
    request = accepted_file(
        " { \"note\" : \"b.md\", \"by\" : {\"tool\": [1, {\"x\": null}], \"n\": 2.5},"
        " \"category\" : \"a\", \"action\" : \"open\", \"seen\": true, \"version\" : 1 } ");
    require(!open_request_lines(request, &lines) && open_request_text_length(request) == 0 &&
                strcmp(note_name_stem(open_request_note(request)), "b") == 0,
            "the order is free, unknown keys are skipped with their values, lines and text may "
            "be absent");
    open_request_destroy(request);
    request = accepted_file("{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\","
                            "\"text\":\"100% a+b \\u65e5\"}");
    require(strcmp(open_request_text(request), "100% a+b \xE6\x97\xA5") == 0,
            "values are JSON strings as they are: no percent decoding");
    open_request_destroy(request);
}

static void verify_file_refused(void)
{
    static const char *const malformed[] = {
        "",
        "[]",
        "\"open\"",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"} x",
        "{\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":2,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":\"1\",\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"action\":\"delete\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"action\":\"OPEN\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"action\":\"open\",\"action\":\"open\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"action\":\"open\",\"note\":\"b\"}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\"}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"category\":\"a\",\"note\":\"b\"}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":7}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"lines\":3}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"lines\":\"8-3\"}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"text\":\"a\\nb\"}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"text\":\"a\\u0000b\"}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"text\":[\"a\"]}",
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"extra\":[1,}",
    };
    for (size_t at = 0; at < sizeof malformed / sizeof malformed[0]; ++at)
        expect_file(malformed[at], OPEN_REQUEST_MALFORMED);
    expect_file("{\"version\":1,\"action\":\"open\",\"category\":\"..\",\"note\":\"b\"}",
                OPEN_REQUEST_NAME_INVALID);
    expect_file("{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"a/b\"}",
                OPEN_REQUEST_NAME_INVALID);
}

static void verify_file_limits(void)
{
    static char json[open_request_file_limit + 64];
    static const char head[] =
        "{\"version\":1,\"action\":\"open\",\"category\":\"a\",\"note\":\"b\",\"text\":\"";
    size_t at = sizeof head - 1;
    memcpy(json, head, at);
    memset(json + at, 't', open_request_text_limit + 1);
    memcpy(json + at + open_request_text_limit + 1, "\"}", 3);
    expect_file(json, OPEN_REQUEST_TOO_LONG);
    memset(json, ' ', open_request_file_limit + 1);
    json[open_request_file_limit + 1] = '\0';
    expect_file(json, OPEN_REQUEST_TOO_LONG);
}

static void verify_file_allocation(void)
{
    static const char json[] = "{\"version\":1,\"action\":\"open\",\"category\":\"work\","
                               "\"note\":\"minutes\",\"lines\":\"2-4\",\"text\":\"step\"}";
    allocation_probe_fail_at(0);
    struct open_request *request = accepted_file(json);
    size_t calls = allocation_probe_calls();
    open_request_destroy(request);
    for (size_t nth = 1; nth <= calls; ++nth)
    {
        request = nullptr;
        allocation_probe_fail_at(nth);
        enum open_request_outcome outcome =
            open_request_create_json(json, sizeof json - 1, &request);
        allocation_probe_fail_at(0);
        require(outcome == OPEN_REQUEST_OUT_OF_MEMORY && request == nullptr,
                "an allocation failure while reading a request file is typed");
    }
    printf("open request file: %zu injected allocation failures\n", calls);
}

void run_open_request_tests(void)
{
    verify_file_accepted();
    verify_file_refused();
    verify_file_limits();
    verify_file_allocation();
    verify_full_link();
    verify_loose_forms();
    verify_malformed_shape();
    verify_malformed_values();
    verify_buffer_ends();
    verify_strict_forms();
    verify_names();
    verify_limits();
    verify_allocation();
    run_open_place_tests();
}
