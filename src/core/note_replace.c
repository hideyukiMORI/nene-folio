#include "note_replace.h"

#include "regex_matches.h"
#include "replace_edit.h"
#include "replace_template.h"

/* 本文の段落区切り（ADR 0023 の決定 1 と同じ形）。論理行の境目はこれ 1 つである。 */
constexpr char16_t paragraph_break = u'\r';

/* 列に入っている一致の数。総数が入れ物を超える列は note_replace_build が先に断るので、
 * ここへ来る列では総数がそのまま入っている数である（レビュー D2）。 */
static size_t stored_count(const struct note_replace_plan *_Nonnull plan)
{
    return plan->matches->count;
}

static const struct regex_match *_Nonnull match_at(const struct note_replace_plan *_Nonnull plan,
                                                   size_t index)
{
    return &plan->matches->items[index];
}

/* 群の文字が本文のどこにあるか。参加しなかった群は空になる（決定 5）。 */
static struct note_search_span group_span(const struct regex_match *_Nonnull match, size_t group)
{
    struct note_search_span empty = {.start = 0, .end = 0};
    if (group == 0)
    {
        return match->whole;
    }
    return match->groups[group - 1].present ? match->groups[group - 1].span : empty;
}

/* この一致に対して置換文字列が生む長さ（UTF-16 単位）。残容量を超える前に断る。 */
static enum note_replace_outcome measure_match(const struct note_replace_plan *_Nonnull plan,
                                               const struct regex_match *_Nonnull match,
                                               size_t available, size_t *_Nonnull needed)
{
    size_t total = 0;
    size_t pieces = replace_template_count(plan->replacement);
    for (size_t index = 0; index < pieces; ++index)
    {
        size_t group = 0;
        size_t literal = replace_template_literal_length(plan->replacement, index);
        if (literal > available - total)
        {
            return NOTE_REPLACE_TOO_LARGE;
        }
        total += literal;
        size_t length = 0;
        if (replace_template_group(plan->replacement, index, &group))
        {
            struct note_search_span span = group_span(match, group);
            length = span.end - span.start;
        }
        if (length > available - total)
        {
            return NOTE_REPLACE_TOO_LARGE;
        }
        total += length;
    }
    *needed = total;
    return NOTE_REPLACE_READY;
}

static void write_match(const struct note_replace_plan *_Nonnull plan,
                        const struct regex_match *_Nonnull match,
                        struct replace_edit *_Nonnull edit)
{
    size_t pieces = replace_template_count(plan->replacement);
    for (size_t index = 0; index < pieces; ++index)
    {
        size_t group = 0;
        replace_edit_append(edit, replace_template_literal(plan->replacement, index),
                            replace_template_literal_length(plan->replacement, index));
        if (replace_template_group(plan->replacement, index, &group))
        {
            struct note_search_span span = group_span(match, group);
            replace_edit_append(edit, plan->text + span.start, span.end - span.start);
        }
    }
}

/* [from, upto) に段落区切りがあるか。 */
static bool crosses_line(const struct note_replace_plan *_Nonnull plan, size_t from, size_t upto)
{
    for (size_t index = from; index < upto; ++index)
    {
        if (plan->text[index] == paragraph_break)
        {
            return true;
        }
    }
    return false;
}

/* 論理行ごとに 1 件だけ選ぶか。1 件だけの REPLACE_ONE は pick_one が別に選ぶので、
 * ここを通るのは全部と行ごとの 2 つである（決定 8(b)）。 */
static bool line_first_only(enum replace_scope scope)
{
    switch (scope)
    {
    case REPLACE_LINE_FIRST:
        return true;
    case REPLACE_ONE:
    case REPLACE_ALL:
        return false;
    }
    return false;
}

/* index 番目の一致を適用するか。一致は昇順で重ならないので、「論理行の最初の一致」は
 * 直前の一致との間に段落区切りがあるかだけで決まる。 */
static bool applies(const struct note_replace_plan *_Nonnull plan, size_t index)
{
    if (!line_first_only(plan->scope) || index == 0)
    {
        return true;
    }
    return crosses_line(plan, match_at(plan, index - 1)->whole.start,
                        match_at(plan, index)->whole.start);
}

/* 適用する一致を順に辿る。edit があれば書きながら、いずれにせよ生む長さを数えて返す。
 * 数えるのと書くのが同じ道を通るので、必要長と実際に書く量が食い違わない。 */
static enum note_replace_outcome sweep(const struct note_replace_plan *_Nonnull plan,
                                       struct replace_edit *_Nullable edit, size_t *_Nonnull needed)
{
    size_t total = 0;
    size_t cursor = 0;
    size_t count = stored_count(plan);
    for (size_t index = 0; index < count; ++index)
    {
        if (!applies(plan, index))
        {
            continue;
        }
        const struct regex_match *_Nonnull match = match_at(plan, index);
        size_t prefix = match->whole.start - cursor;
        size_t replacement = 0;
        if (prefix > note_replace_limit - total ||
            measure_match(plan, match, note_replace_limit - total - prefix, &replacement) !=
                NOTE_REPLACE_READY)
        {
            return NOTE_REPLACE_TOO_LARGE;
        }
        total += prefix + replacement;
        if (edit != nullptr)
        {
            replace_edit_append(edit, plan->text + cursor, match->whole.start - cursor);
            write_match(plan, match, edit);
        }
        cursor = match->whole.end;
    }
    size_t tail = plan->length - cursor;
    if (tail > note_replace_limit - total)
    {
        return NOTE_REPLACE_TOO_LARGE;
    }
    if (edit != nullptr)
    {
        replace_edit_append(edit, plan->text + cursor, plan->length - cursor);
    }
    *needed = total + tail;
    return NOTE_REPLACE_READY;
}

static enum note_replace_outcome build_many(const struct note_replace_plan *_Nonnull plan,
                                            struct replace_edit *_Nullable *_Nonnull out)
{
    /* 全部・各行最初はいずれも、列が空でなければ最初の一致を選ぶ。 */
    if (stored_count(plan) == 0)
    {
        return NOTE_REPLACE_NOT_FOUND;
    }
    size_t needed = 0;
    enum note_replace_outcome measured = sweep(plan, nullptr, &needed);
    if (measured != NOTE_REPLACE_READY)
    {
        return measured;
    }
    struct note_search_span whole = {.start = 0, .end = plan->length};
    struct replace_edit *_Nullable edit = nullptr;
    if (replace_edit_create(whole, needed, &edit) != REPLACE_EDIT_READY)
    {
        return NOTE_REPLACE_OUT_OF_MEMORY;
    }
    measured = sweep(plan, edit, &needed);
    if (measured != NOTE_REPLACE_READY)
    {
        replace_edit_destroy(edit);
        return measured;
    }
    *out = edit;
    return NOTE_REPLACE_READY;
}

/* anchor の開始以降で最初の一致。無ければ先頭から巡回して最初の一致（決定 6）。 */
static bool pick_one(const struct note_replace_plan *_Nonnull plan, size_t *_Nonnull out)
{
    size_t count = stored_count(plan);
    if (count == 0)
    {
        return false;
    }
    *out = 0;
    for (size_t index = 0; index < count; ++index)
    {
        if (match_at(plan, index)->whole.start >= plan->anchor.start)
        {
            *out = index;
            return true;
        }
    }
    return true;
}

static enum note_replace_outcome build_one(const struct note_replace_plan *_Nonnull plan,
                                           struct replace_edit *_Nullable *_Nonnull out)
{
    size_t index = 0;
    if (!pick_one(plan, &index))
    {
        return NOTE_REPLACE_NOT_FOUND;
    }
    const struct regex_match *_Nonnull match = match_at(plan, index);
    size_t needed = 0;
    enum note_replace_outcome measured = measure_match(plan, match, note_replace_limit, &needed);
    if (measured != NOTE_REPLACE_READY)
    {
        return measured;
    }
    struct replace_edit *_Nullable edit = nullptr;
    if (replace_edit_create(match->whole, needed, &edit) != REPLACE_EDIT_READY)
    {
        return NOTE_REPLACE_OUT_OF_MEMORY;
    }
    write_match(plan, match, edit);
    *out = edit;
    return NOTE_REPLACE_READY;
}

enum note_replace_outcome note_replace_build(const struct note_replace_plan *_Nonnull plan,
                                             struct replace_edit *_Nullable *_Nonnull out)
{
    if (plan->anchor.start > plan->anchor.end)
    {
        return NOTE_REPLACE_BAD_SPAN;
    }
    /* 入れ物に収まらなかった列は「入っているぶんだけ」当てると黙って部分適用になる。
     * 走査をやり直すのは呼び出し側の仕事なので、ここでは組み立てずに断る（レビュー D2）。 */
    if (plan->matches->count > plan->matches->capacity)
    {
        return NOTE_REPLACE_PARTIAL_MATCHES;
    }
    return plan->scope == REPLACE_ONE ? build_one(plan, out) : build_many(plan, out);
}
