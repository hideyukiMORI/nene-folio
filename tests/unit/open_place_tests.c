/* #1071 / ADR0118: 依頼の箇所を原文の上で決める。 */
#include "open_place.h"
#include "open_request.h"
#include "unit_tests.h"
#include <stdio.h>
#include <string.h>

/* 12 行。5 行目と 11 行目に同じ文 "target" がある。 */
static const char note[] = "# title\n"      /* 1 */
                           "\n"             /* 2 */
                           "alpha line\n"   /* 3 */
                           "beta line\n"    /* 4 */
                           "the target A\n" /* 5 */
                           "gamma\n"        /* 6 */
                           "delta\n"        /* 7 */
                           "epsilon\n"      /* 8 */
                           "zeta\n"         /* 9 */
                           "eta\n"          /* 10 */
                           "the target B\n" /* 11 */
                           "last line\n";   /* 12 */

/* link の箇所を探し、種類と、見つけた範囲の文字列を確かめる。 */
static void expect_in(const char *_Nonnull source, const char *_Nonnull link,
                      enum open_place_kind kind, const char *_Nonnull found)
{
    struct open_request *request = nullptr;
    require(open_request_create(link, strlen(link), &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            link);
    struct note_byte_span span = {777, 777};
    enum open_place_kind actual = open_place_find(source, strlen(source), request, &span);
    open_request_destroy(request);
    if (actual == OPEN_PLACE_NONE)
    {
        require(kind == OPEN_PLACE_NONE && span.start == 777 && span.end == 777, link);
        return;
    }
    size_t length = span.end - span.start;
    bool same = actual == kind && span.end <= strlen(source) && strlen(found) == length &&
                memcmp(source + span.start, found, length) == 0;
    if (!same)
        fprintf(stderr, "open place: %s -> kind %d [%zu, %zu)\n", link, (int)actual, span.start,
                span.end);
    require(same, link);
}

static void expect(const char *_Nonnull link, enum open_place_kind kind, const char *_Nonnull found)
{
    expect_in(note, link, kind, found);
}

static void verify_text(void)
{
    expect("nenefolio://open?category=a&note=b&lines=11-12&text=target", OPEN_PLACE_TEXT_NEAR,
           "target");
    struct open_request *request = nullptr;
    static const char link[] = "nenefolio://open?category=a&note=b&lines=11-12&text=target";
    require(open_request_create(link, sizeof link - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "near request");
    struct note_byte_span span = {0, 0};
    require(open_place_find(note, sizeof note - 1, request, &span) == OPEN_PLACE_TEXT_NEAR &&
                span.start == (size_t)(strstr(note, "target B") - note),
            "near the given lines the second occurrence is taken, not the first in the note");
    open_request_destroy(request);
    expect("nenefolio://open?category=a&note=b&lines=1-2&text=the%20target%20B",
           OPEN_PLACE_TEXT_ELSEWHERE, "the target B");
    expect("nenefolio://open?category=a&note=b&text=target", OPEN_PLACE_TEXT_ELSEWHERE, "target");
    expect("nenefolio://open?category=a&note=b&lines=3&text=Target", OPEN_PLACE_LINES,
           "alpha line");
    expect("nenefolio://open?category=a&note=b&text=absent", OPEN_PLACE_NONE, "");
    expect("nenefolio://open?category=a&note=b&lines=40&text=last%20line",
           OPEN_PLACE_TEXT_ELSEWHERE, "last line");
}

/* 「あたり」は前後 5 行まで。 */
static void verify_margin(void)
{
    struct open_request *request = nullptr;
    static const char near[] = "nenefolio://open?category=a&note=b&lines=6&text=target";
    require(open_request_create(near, sizeof near - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "margin request");
    struct note_byte_span span = {0, 0};
    require(open_place_find(note, sizeof note - 1, request, &span) == OPEN_PLACE_TEXT_NEAR &&
                span.start == (size_t)(strstr(note, "target A") - note),
            "line 6 with a margin of five reaches lines 1 to 11: the first match inside is taken");
    open_request_destroy(request);
    static const char far[] = "nenefolio://open?category=a&note=b&lines=1&text=target%20B";
    require(open_request_create(far, sizeof far - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "far request");
    require(open_place_find(note, sizeof note - 1, request, &span) == OPEN_PLACE_TEXT_ELSEWHERE,
            "line 1 reaches line 6 only: a match on line 11 is elsewhere");
    open_request_destroy(request);
    require(open_place_margin == 5, "the margin the ADR names");
}

/* 「あたり」の端: 指定の行からちょうど 5 行は中、6 行は外。前にも後ろにも。 */
static void verify_margin_edges(void)
{
    static const char twenty[] =
        "l1\nl2\nl3\nl4 mark\nl5 mark\nl6\nl7\nl8\nl9\nl10\nl11\nl12\nl13\n"
        "l14\nl15 mark\nl16 mark\nl17\nl18\nl19\nl20\n";
    struct open_request *request = nullptr;
    static const char link[] = "nenefolio://open?category=a&note=b&lines=10&text=mark";
    require(open_request_create(link, sizeof link - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "edge request");
    struct note_byte_span span = {0, 0};
    require(open_place_find(twenty, sizeof twenty - 1, request, &span) == OPEN_PLACE_TEXT_NEAR &&
                span.start == (size_t)(strstr(twenty, "l5 mark") - twenty) + 3,
            "five lines before is near; six lines before is not");
    open_request_destroy(request);
    static const char after[] = "nenefolio://open?category=a&note=b&lines=10&text=l15%20mark";
    require(open_request_create(after, sizeof after - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "after request");
    require(open_place_find(twenty, sizeof twenty - 1, request, &span) == OPEN_PLACE_TEXT_NEAR,
            "five lines after is near");
    open_request_destroy(request);
    static const char beyond[] = "nenefolio://open?category=a&note=b&lines=10&text=l16%20mark";
    require(open_request_create(beyond, sizeof beyond - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "beyond request");
    require(open_place_find(twenty, sizeof twenty - 1, request, &span) == OPEN_PLACE_TEXT_ELSEWHERE,
            "six lines after is elsewhere");
    open_request_destroy(request);
    static const char edge[] = "nenefolio://open?category=a&note=b&lines=10&text=l15%20mar";
    require(open_request_create(edge, sizeof edge - 1, &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            "window end request");
    require(open_place_find(twenty, sizeof twenty - 1, request, &span) == OPEN_PLACE_TEXT_NEAR,
            "a match ending at the window's last line is near");
    open_request_destroy(request);
}

static void verify_lines(void)
{
    expect("nenefolio://open?category=a&note=b&lines=1", OPEN_PLACE_LINES, "# title");
    expect("nenefolio://open?category=a&note=b&lines=3-4", OPEN_PLACE_LINES,
           "alpha line\nbeta line");
    expect("nenefolio://open?category=a&note=b&lines=2", OPEN_PLACE_LINES, "");
    expect("nenefolio://open?category=a&note=b&lines=12", OPEN_PLACE_LINES, "last line");
    expect("nenefolio://open?category=a&note=b&lines=11-99", OPEN_PLACE_LINES,
           "the target B\nlast line");
    expect("nenefolio://open?category=a&note=b&lines=13", OPEN_PLACE_NONE, "");
    expect("nenefolio://open?category=a&note=b&lines=99-100", OPEN_PLACE_NONE, "");
    expect("nenefolio://open?category=a&note=b", OPEN_PLACE_NONE, "");
}

static void verify_edges(void)
{
    expect_in("", "nenefolio://open?category=a&note=b&lines=1&text=x", OPEN_PLACE_NONE, "");
    expect_in("one\r\ntwo\r\nthree", "nenefolio://open?category=a&note=b&lines=2", OPEN_PLACE_LINES,
              "two");
    expect_in("one\r\ntwo\r\nthree", "nenefolio://open?category=a&note=b&lines=3-9",
              OPEN_PLACE_LINES, "three");
    expect_in("one\r\ntwo\r\n", "nenefolio://open?category=a&note=b&lines=1-9", OPEN_PLACE_LINES,
              "one\r\ntwo");
    expect_in("\nsecond", "nenefolio://open?category=a&note=b&lines=1", OPEN_PLACE_LINES, "");
    expect_in("\nsecond", "nenefolio://open?category=a&note=b&lines=2", OPEN_PLACE_LINES, "second");
    expect_in("a\rb\nc", "nenefolio://open?category=a&note=b&lines=1", OPEN_PLACE_LINES, "a\rb");
    expect_in("ab", "nenefolio://open?category=a&note=b&text=abc", OPEN_PLACE_NONE, "");
    expect_in("xxab", "nenefolio://open?category=a&note=b&text=ab", OPEN_PLACE_TEXT_ELSEWHERE,
              "ab");
    expect_in("日本語の本文\n二行目",
              "nenefolio://open?category=a&note=b&lines=2&text=%E4%BA%8C%E8%A1%8C",
              OPEN_PLACE_TEXT_NEAR, "二行");
}

void run_open_place_tests(void)
{
    verify_text();
    verify_margin();
    verify_margin_edges();
    verify_lines();
    verify_edges();
}
