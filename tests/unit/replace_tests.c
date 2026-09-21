/* 置換文字列の文法と本文の組み立て（FR-023 / ADR 0028 の決定 5 / 6 / 8(b)）。
 * ICU は使わない純関数だけを見る（一致の列はここで手で作る）。 */
#include "note_replace.h"
#include "regex_matches.h"
#include "replace_edit.h"
#include "replace_template.h"
#include "unit_tests.h"

#include <string.h>

constexpr size_t match_capacity = 64;
/* 上限の反例に使う本文の長さ。`&` を body + 1 個並べると出力が note_replace_limit を超える。 */
constexpr size_t limit_body = 2000;

static size_t units_length(const char16_t *_Nonnull units)
{
    size_t length = 0;
    while (units[length] != u'\0')
    {
        length += 1;
    }
    return length;
}

static bool same_units(const char16_t *_Nonnull actual, const char16_t *_Nonnull expected)
{
    size_t index = 0;
    while (actual[index] != u'\0' && actual[index] == expected[index])
    {
        index += 1;
    }
    return actual[index] == expected[index];
}

/* 空の一致の列。呼び出し側が items を詰める。 */
static struct regex_matches empty_list(struct regex_match *_Nonnull items)
{
    struct regex_matches list = {.items = items, .capacity = match_capacity, .count = 0};
    return list;
}

static void add_match(struct regex_matches *_Nonnull list, size_t start, size_t end)
{
    struct regex_match match = {.whole = {.start = start, .end = end}, .groups = {}};
    list->items[list->count] = match;
    list->count += 1;
}

/* 語をそのまま探して重ならない一致にする（ICU の代わり。判断は測らない）。 */
static void scan_literal(struct regex_matches *_Nonnull list, const char16_t *_Nonnull text,
                         const char16_t *_Nonnull needle)
{
    size_t length = units_length(text);
    size_t width = units_length(needle);
    size_t at = 0;
    while (at + width <= length)
    {
        if (memcmp(text + at, needle, width * sizeof *text) == 0)
        {
            add_match(list, at, at + width);
            at += width;
            continue;
        }
        at += 1;
    }
}

static struct note_replace_plan plan_for(const char16_t *_Nonnull text,
                                         const struct regex_matches *_Nonnull list,
                                         const struct replace_template *_Nonnull replacement,
                                         enum replace_scope scope)
{
    struct note_replace_plan plan = {.text = text,
                                     .length = units_length(text),
                                     .matches = list,
                                     .replacement = replacement,
                                     .scope = scope,
                                     .anchor = {.start = 0, .end = 0}};
    return plan;
}

static struct replace_template *_Nonnull template_for(const char16_t *_Nonnull units)
{
    struct replace_template *replacement = nullptr;
    require(replace_template_create(units, units_length(units), &replacement) ==
                REPLACE_TEMPLATE_READY,
            "the replacement text is accepted");
    require(replacement != nullptr, "an accepted replacement is owned");
    return replacement;
}

/* 置換文字列の文法（決定 5）。断片の列を 1 本の literal に畳んで確かめる。 */
static void expect_template(const char16_t *_Nonnull units, const char16_t *_Nonnull literals,
                            size_t groups)
{
    struct replace_template *replacement = template_for(units);
    char16_t joined[64] = {u'\0'};
    size_t used = 0;
    size_t referenced = 0;
    for (size_t index = 0; index < replace_template_count(replacement); ++index)
    {
        size_t length = replace_template_literal_length(replacement, index);
        memcpy(joined + used, replace_template_literal(replacement, index),
               length * sizeof *joined);
        used += length;
        size_t group = 0;
        referenced += replace_template_group(replacement, index, &group) ? 1 : 0;
    }
    joined[used] = u'\0';
    require(same_units(joined, literals), "the literal text of the template");
    require(referenced == groups, "the number of group references");
    replace_template_destroy(replacement);
}

static void verify_template_grammar(void)
{
    expect_template(u"", u"", 0);
    expect_template(u"x", u"x", 0);
    expect_template(u"&", u"", 1);
    expect_template(u"[&]", u"[]", 1);
    expect_template(u"\\0", u"", 1);
    expect_template(u"<\\1-\\9>", u"<->", 2);
    expect_template(u"\\r", u"\r", 0);
    expect_template(u"\\n", u"\r", 0);
    expect_template(u"\\\\", u"\\", 0);
    expect_template(u"\\&", u"&", 0);
    expect_template(u"\\/", u"/", 0);
    /* `$` は普通の文字。ICU の文法なら `$100` はエラーになる（決定 5 の理由）。 */
    expect_template(u"価格は$100", u"価格は$100", 0);
    expect_template(u"a&b\\1c", u"abc", 2);
}

static void verify_template_refusals(void)
{
    static const char16_t *const refused[] = {u"\\q", u"\\", u"a\\", u"\\t", u"\\x41", u"&\\"};
    for (size_t index = 0; index < sizeof refused / sizeof refused[0]; ++index)
    {
        struct replace_template *replacement = nullptr;
        require(replace_template_create(refused[index], units_length(refused[index]),
                                        &replacement) == REPLACE_TEMPLATE_MALFORMED,
                "an unknown escape or a lone trailing backslash is refused");
        require(replacement == nullptr, "a refused replacement owns nothing");
    }
}

/* 組み立ての結果を 1 つ確かめる。 */
static void expect_edit(const struct note_replace_plan *_Nonnull plan, size_t start, size_t end,
                        const char16_t *_Nonnull units)
{
    struct replace_edit *edit = nullptr;
    require(note_replace_build(plan, &edit) == NOTE_REPLACE_READY, "the replacement is built");
    require(edit != nullptr, "a built replacement is owned");
    struct note_search_span span = replace_edit_span(edit);
    require(span.start == start && span.end == end, "the span the pane must select");
    require(replace_edit_length(edit) == units_length(units), "the length of the new text");
    require(same_units(replace_edit_units(edit), units), "the new text");
    replace_edit_destroy(edit);
}

static void verify_replace_all(void)
{
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    const char16_t *text = u"abcabc";
    scan_literal(&list, text, u"b");
    require(list.count == 2, "two literal matches");
    struct replace_template *plain = template_for(u"X");
    struct note_replace_plan plan = plan_for(text, &list, plain, REPLACE_ALL);
    expect_edit(&plan, 0, 6, u"aXcaXc");
    replace_template_destroy(plain);
    struct replace_template *whole = template_for(u"[&]");
    plan = plan_for(text, &list, whole, REPLACE_ALL);
    expect_edit(&plan, 0, 6, u"a[b]ca[b]c");
    replace_template_destroy(whole);
    /* 置換文字列の `\r` は本文の段落区切り 1 つになる（決定 5）。 */
    struct replace_template *broken = template_for(u"\\r");
    plan = plan_for(text, &list, broken, REPLACE_ALL);
    expect_edit(&plan, 0, 6, u"a\rca\rc");
    replace_template_destroy(broken);
}

static void verify_replace_edges(void)
{
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    const char16_t *text = u"abc";
    /* ゼロ幅の一致も 1 件として数える（probe の §7 と同じ数え方）。 */
    for (size_t at = 0; at <= 3; ++at)
    {
        add_match(&list, at, at);
    }
    struct replace_template *dash = template_for(u"-");
    struct note_replace_plan plan = plan_for(text, &list, dash, REPLACE_ALL);
    expect_edit(&plan, 0, 3, u"-a-b-c-");
    replace_template_destroy(dash);
    /* 入れ物より多い総数のときは、入っているぶんだけ適用する（決定 2）。 */
    struct regex_matches tail = {.items = items, .capacity = 2, .count = 4};
    struct replace_template *plus = template_for(u"+");
    plan = plan_for(text, &tail, plus, REPLACE_ALL);
    expect_edit(&plan, 0, 3, u"+a+bc");
    replace_template_destroy(plus);
}

static void verify_replace_groups(void)
{
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    const char16_t *text = u"ab-cd";
    struct regex_match match = {.whole = {.start = 0, .end = 5}, .groups = {}};
    match.groups[0].span.start = 0;
    match.groups[0].span.end = 2;
    match.groups[0].present = true;
    match.groups[1].span.start = 3;
    match.groups[1].span.end = 5;
    match.groups[1].present = true;
    /* 群 3 は参加していないので空になる（決定 5）。 */
    items[0] = match;
    list.count = 1;
    struct replace_template *swap = template_for(u"\\2=\\1[\\3]");
    struct note_replace_plan plan = plan_for(text, &list, swap, REPLACE_ALL);
    expect_edit(&plan, 0, 5, u"cd=ab[]");
    replace_template_destroy(swap);
}

static void verify_replace_one(void)
{
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    const char16_t *text = u"a-a-a";
    scan_literal(&list, text, u"a");
    require(list.count == 3, "three matches");
    struct replace_template *mark = template_for(u"X");
    struct note_replace_plan plan = plan_for(text, &list, mark, REPLACE_ONE);
    /* anchor の開始以降で最初の一致。範囲は一致そのもので、本文全体ではない。 */
    expect_edit(&plan, 0, 1, u"X");
    plan.anchor.start = 1;
    plan.anchor.end = 1;
    expect_edit(&plan, 2, 3, u"X");
    plan.anchor.start = 3;
    plan.anchor.end = 4;
    expect_edit(&plan, 4, 5, u"X");
    /* 末尾より後ろの anchor は先頭へ巡回する（決定 6）。 */
    plan.anchor.start = 5;
    plan.anchor.end = 5;
    expect_edit(&plan, 0, 1, u"X");
    replace_template_destroy(mark);
}

static void verify_replace_line_first(void)
{
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    const char16_t *text = u"a-a\ra-a";
    scan_literal(&list, text, u"a");
    require(list.count == 4, "four matches over two logical lines");
    struct replace_template *mark = template_for(u"Z");
    struct note_replace_plan plan = plan_for(text, &list, mark, REPLACE_LINE_FIRST);
    expect_edit(&plan, 0, 7, u"Z-a\rZ-a");
    /* 同じ一致でも `g` があれば全部（決定 8(b)）。 */
    plan.scope = REPLACE_ALL;
    expect_edit(&plan, 0, 7, u"Z-Z\rZ-Z");
    replace_template_destroy(mark);
}

static void verify_replace_refusals(void)
{
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    const char16_t *text = u"abc";
    struct replace_template *mark = template_for(u"X");
    struct note_replace_plan plan = plan_for(text, &list, mark, REPLACE_ALL);
    struct replace_edit *edit = nullptr;
    require(note_replace_build(&plan, &edit) == NOTE_REPLACE_NOT_FOUND, "no match changes nothing");
    plan.scope = REPLACE_ONE;
    require(note_replace_build(&plan, &edit) == NOTE_REPLACE_NOT_FOUND,
            "one match to replace needs a match");
    plan.anchor.start = 2;
    plan.anchor.end = 1;
    require(note_replace_build(&plan, &edit) == NOTE_REPLACE_BAD_SPAN,
            "an inverted anchor is refused, not quietly fixed");
    require(edit == nullptr, "a refused replacement owns nothing");
    replace_template_destroy(mark);
}

/* 出力の上限は確保の前に効く（決定 4(c)）。`&` を並べて一致の全体を何度も写す。 */
static void verify_replace_limit(void)
{
    static char16_t text[limit_body + 1];
    static char16_t wide[limit_body + 2];
    for (size_t index = 0; index < limit_body; ++index)
    {
        text[index] = u'a';
        wide[index] = u'&';
    }
    wide[limit_body] = u'&';
    struct regex_match items[match_capacity];
    struct regex_matches list = empty_list(items);
    add_match(&list, 0, limit_body);
    struct replace_template *many = template_for(wide);
    struct note_replace_plan plan = plan_for(text, &list, many, REPLACE_ALL);
    struct replace_edit *edit = nullptr;
    require((limit_body + 1) * limit_body > note_replace_limit,
            "the case really crosses the limit");
    require(note_replace_build(&plan, &edit) == NOTE_REPLACE_TOO_LARGE,
            "an output beyond the limit is refused before anything is allocated");
    plan.scope = REPLACE_ONE;
    require(note_replace_build(&plan, &edit) == NOTE_REPLACE_TOO_LARGE,
            "one match is measured against the same limit");
    require(edit == nullptr, "nothing was built");
    replace_template_destroy(many);
    require(regex_match_limit == 1000000 && note_replace_limit == 4000000,
            "the two core limits stay where the ADR put them");
}

void run_replace_tests(void)
{
    verify_template_grammar();
    verify_template_refusals();
    verify_replace_all();
    verify_replace_edges();
    verify_replace_groups();
    verify_replace_one();
    verify_replace_line_first();
    verify_replace_refusals();
    verify_replace_limit();
}
