/* 現在ノートの検索（FR-011 / ADR 0023 の決定 2）。core の純関数だけを測る。 */
#include "note_search.h"
#include "unit_tests.h"

static size_t units_of(const char16_t *_Nonnull units)
{
    size_t length = 0;
    while (units[length] != u'\0')
    {
        length += 1;
    }
    return length;
}

static struct note_search_query query_of(const char16_t *_Nonnull text,
                                         const char16_t *_Nonnull term)
{
    return (struct note_search_query){
        .text = text, .length = units_of(text), .term = term, .term_length = units_of(term)};
}

static struct note_search_span span_of(size_t start, size_t end)
{
    return (struct note_search_span){.start = start, .end = end};
}

/* anchor から 1 回探して、一致の開始位置を返す。見つからなければ本文の長さを返す。 */
static size_t found_at(const struct note_search_query *_Nonnull query,
                       struct note_search_span anchor, enum search_direction direction)
{
    struct note_search_span match = span_of(0, 0);
    if (note_search_next(query, anchor, direction, &match) != NOTE_SEARCH_FOUND)
    {
        return query->length;
    }
    require(match.end == match.start + query->term_length, "the match is as long as the term");
    return match.start;
}

static void verify_ascii_case(void)
{
    struct note_search_query query = query_of(u"Hello WORLD hello", u"hello");
    require(found_at(&query, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 0,
            "ASCII letters match without regard to case");
    require(found_at(&query, span_of(0, 5), SEARCH_DIRECTION_FORWARD) == 12, "the second hello");
    struct note_search_query upper = query_of(u"Hello WORLD hello", u"World");
    require(found_at(&upper, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 6,
            "the term is folded as well as the text");
}

static void verify_non_ascii(void)
{
    struct note_search_query wide = query_of(u"ハイク ﾊｲｸ はいく", u"ハイク");
    require(found_at(&wide, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 0, "katakana matches");
    require(found_at(&wide, span_of(0, 3), SEARCH_DIRECTION_FORWARD) == 0,
            "half width and hiragana are different text, so the search wraps to the same match");
    struct note_search_query narrow = query_of(u"ハイク ﾊｲｸ はいく", u"ﾊｲｸ");
    require(found_at(&narrow, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 4,
            "half width katakana is its own text");
    struct note_search_query kana = query_of(u"ハイク ﾊｲｸ はいく", u"はいく");
    require(found_at(&kana, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 8,
            "hiragana is not folded into katakana");
    struct note_search_query turkish = query_of(u"İstanbul", u"istanbul");
    require(turkish.length > 0 &&
                found_at(&turkish, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == turkish.length,
            "only ASCII A-Z is folded");
}

static void verify_directions(void)
{
    struct note_search_query query = query_of(u"ab ab ab", u"ab");
    require(found_at(&query, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 0,
            "forward from the beginning");
    require(found_at(&query, span_of(0, 2), SEARCH_DIRECTION_FORWARD) == 3, "forward past a match");
    require(found_at(&query, span_of(3, 5), SEARCH_DIRECTION_FORWARD) == 6, "forward again");
    require(found_at(&query, span_of(6, 8), SEARCH_DIRECTION_BACKWARD) == 3,
            "backward starts before the anchor");
    require(found_at(&query, span_of(3, 5), SEARCH_DIRECTION_BACKWARD) == 0, "backward again");
}

static void verify_wrapping(void)
{
    struct note_search_query query = query_of(u"ab ab ab", u"ab");
    require(found_at(&query, span_of(6, 8), SEARCH_DIRECTION_FORWARD) == 0,
            "the end wraps to the beginning");
    require(found_at(&query, span_of(0, 2), SEARCH_DIRECTION_BACKWARD) == 6,
            "the beginning wraps to the end");
    struct note_search_query only = query_of(u"..ab..", u"ab");
    require(found_at(&only, span_of(2, 4), SEARCH_DIRECTION_FORWARD) == 2 &&
                found_at(&only, span_of(2, 4), SEARCH_DIRECTION_BACKWARD) == 2,
            "a single match is reached again from either direction");
    struct note_search_query beyond = query_of(u"ab", u"ab");
    require(found_at(&beyond, span_of(99, 99), SEARCH_DIRECTION_FORWARD) == 0 &&
                found_at(&beyond, span_of(99, 99), SEARCH_DIRECTION_BACKWARD) == 0,
            "an anchor past the end is clamped");
}

/* いまの一致を選んだ状態から次を探す（欄の Enter と同じ anchor）。 */
static size_t stepped(const struct note_search_query *_Nonnull query, size_t at,
                      enum search_direction direction)
{
    return found_at(query, span_of(at, at + query->term_length), direction);
}

/* 重なった一致も飛ばさず、note_search_count が数える一致を全部通る（2026-09-17 の補正）。 */
static void verify_overlapping_steps(void)
{
    struct note_search_query query = query_of(u"aaaa", u"aa"); /* 候補は 0 / 1 / 2 */
    size_t total = 0;
    size_t ordinal = 0;
    require(note_search_count(&query, 0, &total, &ordinal) == NOTE_SEARCH_FOUND && total == 3,
            "three overlapping matches are counted");
    size_t at = found_at(&query, span_of(0, 0), SEARCH_DIRECTION_FORWARD);
    require(at == 0, "the first match is at the beginning");
    at = stepped(&query, at, SEARCH_DIRECTION_FORWARD);
    require(at == 1, "forward reaches the overlapping match");
    at = stepped(&query, at, SEARCH_DIRECTION_FORWARD);
    require(at == 2, "forward reaches the last match");
    at = stepped(&query, at, SEARCH_DIRECTION_FORWARD);
    require(at == 0, "forward wraps to the first match");
    at = stepped(&query, 2, SEARCH_DIRECTION_BACKWARD);
    require(at == 1, "backward reaches the overlapping match");
    at = stepped(&query, at, SEARCH_DIRECTION_BACKWARD);
    require(at == 0, "backward reaches the first match");
    at = stepped(&query, at, SEARCH_DIRECTION_BACKWARD);
    require(at == 2, "backward wraps to the last match");
    for (size_t index = 0; index < 3; ++index)
    {
        require(note_search_count(&query, index, &total, &ordinal) == NOTE_SEARCH_FOUND &&
                    ordinal == index + 1,
                "every reachable match has its own ordinal");
    }
}

/* 折り畳むのは A-Z だけ。前後の記号（[ \ ] ^ _ ` { | } ~）は畳まない。 */
static void verify_folding_boundary(void)
{
    struct note_search_query symbols = query_of(u"[\\]^_`{|}~", u"{|}");
    require(found_at(&symbols, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 6,
            "symbols match themselves");
    const char16_t *_Nonnull const pairs[] = {u"[", u"{", u"\\", u"|", u"]",
                                              u"}", u"^", u"~",  u"_", u"`"};
    for (size_t index = 0; index < sizeof pairs / sizeof pairs[0]; index += 2)
    {
        struct note_search_query query = query_of(pairs[index], pairs[index + 1]);
        require(found_at(&query, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == query.length,
                "the neighbours of A-Z and a-z are not folded into each other");
    }
    struct note_search_query edges = query_of(u"AZaz", u"az");
    require(found_at(&edges, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 0,
            "A and Z are the ends of the folded range");
}

/* 語が本文と同じ長さ・1 文字の語の巡回。 */
static void verify_short_texts(void)
{
    struct note_search_query whole = query_of(u"日報", u"日報");
    require(found_at(&whole, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 0 &&
                found_at(&whole, span_of(0, 2), SEARCH_DIRECTION_FORWARD) == 0 &&
                found_at(&whole, span_of(0, 2), SEARCH_DIRECTION_BACKWARD) == 0,
            "a term as long as the text is reached from either direction");
    struct note_search_query single = query_of(u"aba", u"a");
    require(found_at(&single, span_of(0, 1), SEARCH_DIRECTION_FORWARD) == 2 &&
                found_at(&single, span_of(2, 3), SEARCH_DIRECTION_FORWARD) == 0 &&
                found_at(&single, span_of(0, 1), SEARCH_DIRECTION_BACKWARD) == 2,
            "a one unit term wraps at both ends");
}

static void verify_counting(void)
{
    struct note_search_query query = query_of(u"ab ab ab", u"ab");
    size_t total = 0;
    size_t ordinal = 0;
    require(note_search_count(&query, 3, &total, &ordinal) == NOTE_SEARCH_FOUND && total == 3 &&
                ordinal == 2,
            "the middle match is the second of three");
    require(note_search_count(&query, 0, &total, &ordinal) == NOTE_SEARCH_FOUND && ordinal == 1,
            "the first match is the first");
    require(note_search_count(&query, 6, &total, &ordinal) == NOTE_SEARCH_FOUND && ordinal == 3,
            "the last match is the last");
    struct note_search_query overlapping = query_of(u"aaaa", u"aa");
    require(note_search_count(&overlapping, 2, &total, &ordinal) == NOTE_SEARCH_FOUND &&
                total == 3 && ordinal == 3,
            "overlapping occurrences are counted");
    struct note_search_query missing = query_of(u"ab ab", u"zz");
    require(note_search_count(&missing, 0, &total, &ordinal) == NOTE_SEARCH_NOT_FOUND &&
                total == 0 && ordinal == 0,
            "no match counts as zero of zero");
    /* 一致の開始でない位置は「そこまでに現れた一致の数」。丸めも推測もしない契約。 */
    require(note_search_count(&query, 1, &total, &ordinal) == NOTE_SEARCH_FOUND && ordinal == 1,
            "a position inside the first match counts the matches up to it");
    require(note_search_count(&query, 5, &total, &ordinal) == NOTE_SEARCH_FOUND && ordinal == 2,
            "a position between matches counts the ones before it");
}

static void verify_surrogates(void)
{
    /* U+20BB7（𠮷）はサロゲート対 1 組。 */
    const char16_t pair[] = {0xD842, 0xDFB7, u'a', 0xD842, 0xDFB7, u'\0'};
    struct note_search_query query = query_of(pair, u"a");
    require(found_at(&query, span_of(0, 0), SEARCH_DIRECTION_FORWARD) == 2,
            "a match never starts inside a surrogate pair");
    const char16_t term[] = {0xD842, 0xDFB7, u'\0'};
    struct note_search_query supplementary = query_of(pair, term);
    struct note_search_span match = span_of(0, 0);
    require(note_search_next(&supplementary, span_of(0, 0), SEARCH_DIRECTION_FORWARD, &match) ==
                NOTE_SEARCH_FOUND,
            "a supplementary character can be searched");
    require(match.start == 0 && match.end == 2, "the match covers the whole pair");
    require(found_at(&supplementary, span_of(0, 2), SEARCH_DIRECTION_FORWARD) == 3,
            "the second pair follows");
}

static void verify_refusals(void)
{
    struct note_search_span match = span_of(9, 9);
    size_t total = 9;
    size_t ordinal = 9;
    struct note_search_query empty_term = query_of(u"ab", u"");
    require(note_search_next(&empty_term, span_of(0, 0), SEARCH_DIRECTION_FORWARD, &match) ==
                    NOTE_SEARCH_NO_TERM &&
                note_search_count(&empty_term, 0, &total, &ordinal) == NOTE_SEARCH_NO_TERM,
            "an empty term does nothing");
    const char16_t lone_low[] = {u'a', 0xDC00, u'\0'};
    const char16_t lone_high[] = {0xD800, u'a', u'\0'};
    struct note_search_query broken_text = query_of(lone_low, u"a");
    struct note_search_query broken_term = query_of(u"aa", lone_high);
    require(note_search_next(&broken_text, span_of(0, 0), SEARCH_DIRECTION_FORWARD, &match) ==
                    NOTE_SEARCH_MALFORMED &&
                note_search_next(&broken_term, span_of(0, 0), SEARCH_DIRECTION_BACKWARD, &match) ==
                    NOTE_SEARCH_MALFORMED,
            "a lone surrogate in either side is refused");
    require(note_search_count(&broken_text, 0, &total, &ordinal) == NOTE_SEARCH_MALFORMED,
            "counting refuses the same input");
    const char16_t truncated[] = {u'a', 0xD842, u'\0'};
    struct note_search_query cut = query_of(truncated, u"a");
    require(note_search_next(&cut, span_of(0, 0), SEARCH_DIRECTION_FORWARD, &match) ==
                NOTE_SEARCH_MALFORMED,
            "a surrogate pair cut off by the end of the text is refused");
    require(match.start == 9 && match.end == 9 && total == 9 && ordinal == 9,
            "a refusal never writes an output");
    /* 反転した anchor は公開契約の違反。黙って直さず拒む。 */
    struct note_search_query sane = query_of(u"ab ab", u"ab");
    require(note_search_next(&sane, span_of(3, 1), SEARCH_DIRECTION_FORWARD, &match) ==
                    NOTE_SEARCH_BAD_SPAN &&
                note_search_next(&sane, span_of(3, 1), SEARCH_DIRECTION_BACKWARD, &match) ==
                    NOTE_SEARCH_BAD_SPAN,
            "an inverted anchor is refused in both directions");
    require(match.start == 9 && match.end == 9, "a refused anchor never writes an output");
    struct note_search_query longer = query_of(u"ab", u"abc");
    struct note_search_query blank = query_of(u"", u"a");
    require(note_search_next(&longer, span_of(0, 0), SEARCH_DIRECTION_FORWARD, &match) ==
                    NOTE_SEARCH_NOT_FOUND &&
                note_search_next(&blank, span_of(0, 0), SEARCH_DIRECTION_BACKWARD, &match) ==
                    NOTE_SEARCH_NOT_FOUND,
            "a term longer than the text and an empty text find nothing");
}

void run_search_tests(void)
{
    verify_ascii_case();
    verify_non_ascii();
    verify_directions();
    verify_wrapping();
    verify_overlapping_steps();
    verify_folding_boundary();
    verify_short_texts();
    verify_counting();
    verify_surrogates();
    verify_refusals();
}
