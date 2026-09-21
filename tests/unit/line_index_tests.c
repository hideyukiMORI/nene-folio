/* core の line_index（ADR 0026 の決定 2）。論理行の数え上げと、位置 ↔ 番号の対応を確かめる。
 * 対象は RichEdit が表示している平文なので、段落区切りは CR 1 つである（ADR 0023 の決定 1）。 */
#include "line_index.h"
#include "unit_tests.h"

#include <stdint.h>

static size_t units_of(const char16_t *_Nonnull units)
{
    size_t count = 0;
    while (units[count] != u'\0')
    {
        count += 1;
    }
    return count;
}

static struct line_index *_Nonnull indexed(const char16_t *_Nonnull units)
{
    struct line_index *index = nullptr;
    require(line_index_create(units, units_of(units), &index) == LINE_INDEX_READY,
            "index is built");
    return index;
}

/* 期待する印を値で渡す（引数は 4 つまで・C-012）。 */
static void expect_mark(const struct line_index *_Nonnull index, size_t position,
                        struct line_mark expected, const char *_Nonnull description)
{
    struct line_mark mark = {.number = 0, .first = false};
    require(line_index_at(index, position, &mark) == LINE_INDEX_READY &&
                mark.number == expected.number && mark.first == expected.first,
            description);
}

static struct line_mark starting(size_t number)
{
    struct line_mark mark = {.number = number, .first = true};
    return mark;
}

static struct line_mark inside(size_t number)
{
    struct line_mark mark = {.number = number, .first = false};
    return mark;
}

static void expect_start(const struct line_index *_Nonnull index, size_t number, size_t position,
                         const char *_Nonnull description)
{
    size_t start = SIZE_MAX;
    require(line_index_start(index, number, &start) == LINE_INDEX_READY && start == position,
            description);
}

/* 空の本文は 1 行で、位置 0 はその先頭（決定 2）。 */
static void verify_empty(void)
{
    struct line_index *index = indexed(u"");
    require(line_index_count(index) == 1, "an empty body is one logical line");
    expect_mark(index, 0, starting(1), "position 0 starts line 1");
    expect_start(index, 1, 0, "line 1 starts at 0");
    require(line_index_digits(index) == 3, "the gutter never narrows below three digits");
    line_index_destroy(index);
}

static void verify_single_line(void)
{
    struct line_index *index = indexed(u"abc");
    require(line_index_count(index) == 1, "a body without CR is one logical line");
    expect_mark(index, 0, starting(1), "the first character starts the line");
    expect_mark(index, 1, inside(1), "the middle of the line is not a start");
    expect_mark(index, 3, inside(1), "the end of the body is not a start");
    line_index_destroy(index);
}

/* "a\rb\r\rc" は 4 行（空行を 1 行と数える）。 */
static void verify_lines_and_starts(void)
{
    struct line_index *index = indexed(u"a\rb\r\rc");
    require(line_index_count(index) == 4, "three CR make four logical lines");
    expect_mark(index, 0, starting(1), "line 1 starts at 0");
    expect_mark(index, 1, inside(1), "the CR itself still belongs to its line");
    expect_mark(index, 2, starting(2), "line 2 starts after the first CR");
    expect_mark(index, 4, starting(3), "the empty line 3 has a start of its own");
    expect_mark(index, 5, starting(4), "line 4 starts after the empty line");
    expect_mark(index, 6, inside(4), "the end of the body is on the last line");
    expect_start(index, 1, 0, "start of line 1");
    expect_start(index, 2, 2, "start of line 2");
    expect_start(index, 3, 4, "start of the empty line 3");
    expect_start(index, 4, 5, "start of line 4");
    line_index_destroy(index);
}

/* 末尾が CR で終わる本文の、空の最終行も 1 行（決定 2）。 */
static void verify_trailing_break(void)
{
    struct line_index *index = indexed(u"a\r");
    require(line_index_count(index) == 2, "a trailing CR leaves an empty final line");
    expect_mark(index, 2, starting(2), "the empty final line starts at the end of the body");
    expect_start(index, 2, 2, "the final line starts one past the CR");
    line_index_destroy(index);
}

/* 段落区切りは CR 1 つだけで、LF は普通の文字である（ADR 0023 の決定 1）。
 * CRLF の入力が来ても行を 2 度数えず、LF は次の行の 1 文字目になる。 */
static void verify_line_feed_is_ordinary(void)
{
    struct line_index *index = indexed(u"a\r\nb");
    require(line_index_count(index) == 2, "CR LF makes one break, not two");
    expect_mark(index, 1, inside(1), "the CR still belongs to line 1");
    expect_mark(index, 2, starting(2), "the LF is the first character of line 2");
    expect_mark(index, 3, inside(2), "the character after the LF is inside the same line");
    expect_start(index, 2, 2, "line 2 starts at the LF, not after it");
    line_index_destroy(index);
}

static void verify_out_of_range(void)
{
    struct line_index *index = indexed(u"a\rb");
    struct line_mark mark = {.number = 99, .first = true};
    require(line_index_at(index, 4, &mark) == LINE_INDEX_OUT_OF_RANGE &&
                line_index_at(index, 400, &mark) == LINE_INDEX_OUT_OF_RANGE,
            "a position past the body is refused");
    require(mark.number == 99 && mark.first, "a refused position leaves the mark alone");
    size_t start = 77;
    require(line_index_start(index, 0, &start) == LINE_INDEX_OUT_OF_RANGE &&
                line_index_start(index, 3, &start) == LINE_INDEX_OUT_OF_RANGE,
            "line numbers outside 1..count are refused");
    require(start == 77, "a refused number leaves the position alone");
    expect_mark(index, 3, inside(2), "the position equal to the length is still inside");
    line_index_destroy(index);
}

/* 折り返しに依らず、長い 1 行は 1 行のままである。 */
static void verify_long_line(void)
{
    static char16_t body[3002];
    for (size_t at = 0; at < 3000; ++at)
    {
        body[at] = u'x';
    }
    body[3000] = u'\r';
    body[3001] = u'\0';
    struct line_index *index = indexed(body);
    require(line_index_count(index) == 2, "a 3000 unit line is still one logical line");
    expect_mark(index, 1500, inside(1), "the middle of a long line keeps its number");
    expect_mark(index, 3001, starting(2), "the empty line after it starts at 3001");
    line_index_destroy(index);
}

/* 桁数は行数だけで決まり、最小は 3（決定 2 の (d) / 決定 6）。 */
static void verify_digits(size_t lines, size_t digits)
{
    static char16_t body[10001];
    require(lines >= 1 && lines <= 10000, "the fixture fits");
    for (size_t at = 0; at + 1 < lines; ++at)
    {
        body[at] = u'\r';
    }
    body[lines - 1] = u'\0';
    struct line_index *index = indexed(body);
    require(line_index_count(index) == lines, "the fixture has the wanted number of lines");
    require(line_index_digits(index) == digits, "the digit count follows the line count");
    line_index_destroy(index);
}

void run_line_index_tests(void)
{
    verify_empty();
    verify_single_line();
    verify_lines_and_starts();
    verify_trailing_break();
    verify_line_feed_is_ordinary();
    verify_out_of_range();
    verify_long_line();
    verify_digits(9, 3);
    verify_digits(10, 3);
    verify_digits(999, 3);
    verify_digits(1000, 4);
    verify_digits(9999, 4);
    verify_digits(10000, 5);
}
