#include "note_search.h"

#include "ascii_fold.h"

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

static bool matches_at(const struct note_search_query *_Nonnull query, size_t at)
{
    for (size_t index = 0; index < query->term_length; ++index)
    {
        if (ascii_fold_unit(query->text[at + index]) != ascii_fold_unit(query->term[index]))
        {
            return false;
        }
    }
    return true;
}

/* [from, upto) を昇順に見る。 */
static bool scan_forward(const struct note_search_query *_Nonnull query, size_t from, size_t upto,
                         size_t *_Nonnull found)
{
    for (size_t at = from; at < upto; ++at)
    {
        if (matches_at(query, at))
        {
            *found = at;
            return true;
        }
    }
    return false;
}

/* [downto, from) を降順に見る。 */
static bool scan_backward(const struct note_search_query *_Nonnull query, size_t from,
                          size_t downto, size_t *_Nonnull found)
{
    for (size_t at = from; at > downto; --at)
    {
        if (matches_at(query, at - 1))
        {
            *found = at - 1;
            return true;
        }
    }
    return false;
}

/* 前方の探し始め。**一致の開始の次**から見るので、重なった候補も飛ばさない
 * （2026-09-17 の補正。anchor が空＝caret ならその位置から）。 */
static size_t forward_start(struct note_search_span anchor, size_t limit)
{
    size_t from = anchor.start + (anchor.end > anchor.start ? 1 : 0);
    return from < limit ? from : limit;
}

/* anchor から始めて、端まで行ったら反対の端から続ける（巡回）。候補はちょうど 1 回ずつ見る。 */
static bool seek(const struct note_search_query *_Nonnull query, struct note_search_span anchor,
                 enum search_direction direction, size_t *_Nonnull found)
{
    size_t limit = candidate_limit(query);
    switch (direction)
    {
    case SEARCH_DIRECTION_FORWARD:
    {
        size_t from = forward_start(anchor, limit);
        return scan_forward(query, from, limit, found) || scan_forward(query, 0, from, found);
    }
    case SEARCH_DIRECTION_BACKWARD:
    {
        size_t from = anchor.start < limit ? anchor.start : limit;
        return scan_backward(query, from, 0, found) || scan_backward(query, limit, from, found);
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
    size_t limit = candidate_limit(query);
    size_t count = 0;
    size_t rank = 0;
    for (size_t at = 0; at < limit; ++at)
    {
        if (!matches_at(query, at))
        {
            continue;
        }
        count += 1;
        rank = at <= position ? count : rank;
    }
    *total = count;
    *ordinal = rank;
    return count == 0 ? NOTE_SEARCH_NOT_FOUND : NOTE_SEARCH_FOUND;
}
