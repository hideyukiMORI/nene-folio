#include "note_search.h"

#include "ascii_fold.h"
#include "note_search_order.h"

/* 呼出中だけの借用と走査位置。公開 owner・語長に比例する領域は持たない。 */
struct note_search
{
    const struct note_search_query *_Nonnull query;
    size_t split;
    size_t period;
    size_t after_shift;
    size_t at;
    size_t memory;
    size_t limit;
};

constexpr char16_t high_surrogate_first = 0xD800;
constexpr char16_t high_surrogate_last = 0xDBFF;
constexpr char16_t low_surrogate_first = 0xDC00;
constexpr char16_t low_surrogate_last = 0xDFFF;

static bool high_surrogate(char16_t unit)
{
    return unit >= high_surrogate_first && unit <= high_surrogate_last;
}

static bool low_surrogate(char16_t unit)
{
    return unit >= low_surrogate_first && unit <= low_surrogate_last;
}

/* index の文字が占める単位数。孤立サロゲートなら 0。 */
static size_t encoded_units(const char16_t *_Nonnull units, size_t length, size_t index)
{
    if (high_surrogate(units[index]))
    {
        return index + 1 < length && low_surrogate(units[index + 1]) ? 2 : 0;
    }
    return low_surrogate(units[index]) ? 0 : 1;
}

/* 孤立サロゲートを含まないか。整形式なら一致はサロゲート対を跨がない。 */
static bool well_formed(const char16_t *_Nonnull units, size_t length)
{
    size_t index = 0;
    while (index < length)
    {
        size_t step = encoded_units(units, length, index);
        if (step == 0)
        {
            return false;
        }
        index += step;
    }
    return true;
}

/* 探せる問い合わせか。探せなければ rejected に理由を書く。 */
static bool searchable(const struct note_search_query *_Nonnull query,
                       enum note_search_outcome *_Nonnull rejected)
{
    if (query->term_length == 0)
    {
        *rejected = NOTE_SEARCH_NO_TERM;
        return false;
    }
    if (!well_formed(query->term, query->term_length) || !well_formed(query->text, query->length))
    {
        *rejected = NOTE_SEARCH_MALFORMED;
        return false;
    }
    return true;
}

/* 語が始まり得る位置の数（0 以上 limit 未満）。語が本文より長ければ 0。 */
static size_t candidate_limit(const struct note_search_query *_Nonnull query)
{
    return query->term_length > query->length ? 0 : query->length - query->term_length + 1;
}

static bool precedes(char16_t left, char16_t right, enum note_search_order order)
{
    switch (order)
    {
    case NOTE_SEARCH_ORDER_NORMAL:
        return left < right;
    case NOTE_SEARCH_ORDER_REVERSED:
        return left > right;
    }
    return false;
}

/* left < right <= m、matched < m-right のときだけ比較する。
 * left+matched < right+matched < m なので番兵 -1 や範囲外加算は要らない。 */
static size_t maximal_suffix(const struct note_search_query *_Nonnull query,
                             enum note_search_order order, size_t *_Nonnull period)
{
    size_t left = 0;
    size_t right = 1;
    size_t matched = 0;
    *period = 1;
    while (matched < query->term_length - right)
    {
        char16_t first = ascii_fold_unit(query->term[left + matched]);
        char16_t second = ascii_fold_unit(query->term[right + matched]);
        if (first == second && matched + 1 != *period)
        {
            matched += 1;
            continue;
        }
        if (first == second)
        {
            right += *period;
            matched = 0;
            continue;
        }
        if (precedes(second, first, order))
        {
            right += matched + 1;
            matched = 0;
            *period = right - left;
            continue;
        }
        left = right;
        right += 1;
        matched = 0;
        *period = 1;
    }
    return left;
}

static bool same_unit(char16_t left, char16_t right)
{
    return ascii_fold_unit(left) == ascii_fold_unit(right);
}

static bool periodic_prefix(const struct note_search *_Nonnull iterator)
{
    size_t length = iterator->query->term_length;
    /* 奇数長でも数学上の split < m/2 と同じ。整数除算は使わない。 */
    if (iterator->split >= length - iterator->split)
    {
        return false;
    }
    for (size_t index = 0; index < iterator->split; ++index)
    {
        if (!same_unit(iterator->query->term[index],
                       iterator->query->term[iterator->period + index]))
        {
            return false;
        }
    }
    return true;
}

static struct note_search iterator_of(const struct note_search_query *_Nonnull query)
{
    size_t period = 1;
    size_t split = maximal_suffix(query, NOTE_SEARCH_ORDER_NORMAL, &period);
    size_t other_period = 1;
    size_t other_split = maximal_suffix(query, NOTE_SEARCH_ORDER_REVERSED, &other_period);
    if (other_split > split)
    {
        split = other_split;
        period = other_period;
    }
    struct note_search iterator = {.query = query,
                                   .split = split,
                                   .period = period,
                                   .after_shift = 0,
                                   .at = 0,
                                   .memory = 0,
                                   .limit = candidate_limit(query)};
    if (periodic_prefix(&iterator))
    {
        iterator.after_shift = query->term_length - period;
    }
    else
    {
        size_t right = query->term_length - split;
        /* 非周期なら split > 0。両半分が m-1 以下なので +1 は m 以下。 */
        iterator.period = (split > right ? split : right) + 1;
    }
    return iterator;
}

/* at==limit が終端。差分との比較を先に行い、本文外の位置を加算しない。 */
static void advance(struct note_search *_Nonnull iterator, size_t shift)
{
    size_t remaining = iterator->limit - iterator->at;
    iterator->at = shift >= remaining ? iterator->limit : iterator->at + shift;
}

static size_t right_end(const struct note_search *_Nonnull iterator)
{
    size_t index = iterator->split > iterator->memory ? iterator->split : iterator->memory;
    const struct note_search_query *_Nonnull query = iterator->query;
    while (index < query->term_length &&
           same_unit(query->text[iterator->at + index], query->term[index]))
    {
        index += 1;
    }
    return index;
}

static bool left_matches(const struct note_search *_Nonnull iterator)
{
    size_t index = iterator->split;
    const struct note_search_query *_Nonnull query = iterator->query;
    while (index > iterator->memory)
    {
        if (!same_unit(query->text[iterator->at + index - 1], query->term[index - 1]))
        {
            return false;
        }
        index -= 1;
    }
    return true;
}

static size_t mismatch_shift(const struct note_search *_Nonnull iterator, size_t index)
{
    size_t shift = index - iterator->split + 1;
    size_t remembered =
        iterator->memory >= iterator->period ? iterator->memory - iterator->period + 1 : 0;
    return shift > remembered ? shift : remembered;
}

/* 一致を返す前に前進し、次の呼出しでも同じ前処理と一致記憶を使う。 */
static bool next_match(struct note_search *_Nonnull iterator, size_t *_Nonnull found)
{
    while (iterator->at < iterator->limit)
    {
        size_t index = right_end(iterator);
        if (index < iterator->query->term_length)
        {
            advance(iterator, mismatch_shift(iterator, index));
            iterator->memory = 0;
            continue;
        }
        size_t at = iterator->at;
        bool matched = left_matches(iterator);
        advance(iterator, iterator->period);
        iterator->memory = iterator->after_shift;
        if (matched)
        {
            *found = at;
            return true;
        }
    }
    return false;
}

static bool pick_forward(struct note_search *_Nonnull iterator, size_t from, size_t *_Nonnull found)
{
    size_t first = 0;
    if (!next_match(iterator, &first))
    {
        return false;
    }
    *found = first;
    if (first >= from)
    {
        return true;
    }
    size_t next = 0;
    while (next_match(iterator, &next))
    {
        if (next >= from)
        {
            *found = next;
            return true;
        }
    }
    return true;
}

static bool pick_backward(struct note_search *_Nonnull iterator, size_t from,
                          size_t *_Nonnull found)
{
    size_t chosen = 0;
    if (!next_match(iterator, &chosen))
    {
        return false;
    }
    size_t next = 0;
    while (next_match(iterator, &next))
    {
        if (chosen < from && next >= from)
        {
            *found = chosen;
            return true;
        }
        chosen = next;
    }
    *found = chosen;
    return true;
}

/* 前方の探し始め。**一致の開始の次**から見るので、重なった候補も飛ばさない
 * （2026-09-17 の補正。anchor が空＝caret ならその位置から）。 */
static size_t forward_start(struct note_search_span anchor, size_t limit)
{
    size_t from = anchor.start + (anchor.end > anchor.start ? 1 : 0);
    return from < limit ? from : limit;
}

/* 同じ昇順の一致列から、anchor 前後の巡回先だけを選ぶ。 */
static bool seek(const struct note_search_query *_Nonnull query, struct note_search_span anchor,
                 enum search_direction direction, size_t *_Nonnull found)
{
    struct note_search iterator = iterator_of(query);
    switch (direction)
    {
    case SEARCH_DIRECTION_FORWARD:
    {
        return pick_forward(&iterator, forward_start(anchor, iterator.limit), found);
    }
    case SEARCH_DIRECTION_BACKWARD:
    {
        size_t from = anchor.start < iterator.limit ? anchor.start : iterator.limit;
        return pick_backward(&iterator, from, found);
    }
    }
    return false;
}

enum note_search_outcome note_search_next(const struct note_search_query *_Nonnull query,
                                          struct note_search_span anchor,
                                          enum search_direction direction,
                                          struct note_search_span *_Nonnull out)
{
    if (anchor.start > anchor.end)
    {
        return NOTE_SEARCH_BAD_SPAN;
    }
    enum note_search_outcome rejected = NOTE_SEARCH_NO_TERM;
    if (!searchable(query, &rejected))
    {
        return rejected;
    }
    size_t found = 0;
    if (!seek(query, anchor, direction, &found))
    {
        return NOTE_SEARCH_NOT_FOUND;
    }
    out->start = found;
    out->end = found + query->term_length;
    return NOTE_SEARCH_FOUND;
}

enum note_search_outcome note_search_count(const struct note_search_query *_Nonnull query,
                                           size_t position, size_t *_Nonnull total,
                                           size_t *_Nonnull ordinal)
{
    enum note_search_outcome rejected = NOTE_SEARCH_NO_TERM;
    if (!searchable(query, &rejected))
    {
        return rejected;
    }
    struct note_search iterator = iterator_of(query);
    size_t count = 0;
    size_t rank = 0;
    size_t at = 0;
    while (next_match(&iterator, &at))
    {
        count += 1;
        rank = at <= position ? count : rank;
    }
    *total = count;
    *ordinal = rank;
    return count == 0 ? NOTE_SEARCH_NOT_FOUND : NOTE_SEARCH_FOUND;
}
