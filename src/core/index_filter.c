#include "index_filter.h"

#include "ascii_fold.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

/* 不変条件: (categories[i], notes[i]) は i の昇順に厳密に増える（ADR 0024 の補正 6）。
 * 入力の列が台帳の順（カテゴリ番号・ノート番号の昇順）で来るので、作るときに並べ替えは要らない。
 * 所属の判定はこの順序に依る二分探索で、配置を作り直すたびの走査を対数にする。 */
struct index_filter
{
    size_t *_Nullable categories; /* 見えるノートのカテゴリ番号 */
    size_t *_Nullable notes;      /* 同じ数のノート番号 */
    size_t count;
};

/* text の at から語が始まるか。 */
static bool matches_at(const char *_Nonnull text, const char *_Nonnull term, size_t term_length,
                       size_t at)
{
    for (size_t index = 0; index < term_length; ++index)
    {
        if (ascii_fold_byte(text[at + index]) != ascii_fold_byte(term[index]))
        {
            return false;
        }
    }
    return true;
}

/* text のどこかに語があるか。語が本文より長ければ無い。
 * 打鍵ごとに全本文を走るので、頭の 1 バイトが合う位置だけを比べに行く。 */
static bool contains(const char *_Nonnull text, size_t length, const char *_Nonnull term,
                     size_t term_length)
{
    if (term_length > length)
    {
        return false;
    }
    char head = ascii_fold_byte(term[0]);
    size_t limit = length - term_length + 1;
    for (size_t at = 0; at < limit; ++at)
    {
        if (ascii_fold_byte(text[at]) == head && matches_at(text, term, term_length, at))
        {
            return true;
        }
    }
    return false;
}

/* 整形式の UTF-8 か（語だけを確かめる。名前と本文は境界で検証済み）。 */
static bool well_formed(const char *_Nonnull bytes, size_t length)
{
    size_t index = 0;
    while (index < length)
    {
        uint32_t code_point = 0;
        size_t step = utf8_text_decode(bytes + index, length - index, &code_point);
        if (step == 0)
        {
            return false;
        }
        index += step;
    }
    return true;
}

/* このノートが見えるか。本文の写しが無ければ名前によらず見えない（決定 1）。 */
static bool visible(const struct index_filter_query *_Nonnull query,
                    const struct index_filter_entry *_Nonnull entry)
{
    if (entry->body == nullptr)
    {
        return false;
    }
    return contains(entry->name, strlen(entry->name), query->term, query->term_length) ||
           contains(entry->body, entry->length, query->term, query->term_length);
}

static void admit(struct index_filter *_Nonnull filter, struct note_ref ref)
{
    filter->categories[filter->count] = ref.category;
    filter->notes[filter->count] = ref.note;
    filter->count += 1;
}

enum index_filter_outcome index_filter_create(const struct index_filter_query *_Nonnull query,
                                              struct index_filter *_Nullable *_Nonnull out)
{
    if (query->term_length == 0)
    {
        return INDEX_FILTER_NO_TERM;
    }
    if (!well_formed(query->term, query->term_length))
    {
        return INDEX_FILTER_MALFORMED;
    }
    struct index_filter *_Nullable filter = calloc(1, sizeof *filter);
    if (filter == nullptr)
    {
        return INDEX_FILTER_OUT_OF_MEMORY;
    }
    /* 一致が 0 でも 1 要素ぶん確保し、確保の失敗と空の区別を残す（drawer_layout と同じ流儀）。 */
    filter->categories = malloc((query->count + 1) * sizeof *filter->categories);
    filter->notes = malloc((query->count + 1) * sizeof *filter->notes);
    if (filter->categories == nullptr || filter->notes == nullptr)
    {
        index_filter_destroy(filter);
        return INDEX_FILTER_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < query->count; ++index)
    {
        if (visible(query, &query->entries[index]))
        {
            admit(filter, query->entries[index].ref);
        }
    }
    *out = filter;
    return INDEX_FILTER_ACCEPTED;
}

/* (category, note) 以上の最初の位置（無ければ count）。並びは昇順という不変条件に依る。 */
static size_t lower_bound(const struct index_filter *_Nonnull filter, size_t category, size_t note)
{
    size_t low = 0;
    size_t high = filter->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        bool earlier = filter->categories[middle] < category ||
                       (filter->categories[middle] == category && filter->notes[middle] < note);
        low = earlier ? middle + 1 : low;
        high = earlier ? high : middle;
    }
    return low;
}

bool index_filter_note(const struct index_filter *_Nonnull filter, struct note_ref ref)
{
    size_t at = lower_bound(filter, ref.category, ref.note);
    return at < filter->count && filter->categories[at] == ref.category &&
           filter->notes[at] == ref.note;
}

bool index_filter_category(const struct index_filter *_Nonnull filter, size_t category)
{
    /* そのカテゴリの最初のノートが来る位置。そこがまだ同じカテゴリなら見えるノートがある。 */
    size_t at = lower_bound(filter, category, 0);
    return at < filter->count && filter->categories[at] == category;
}

size_t index_filter_count(const struct index_filter *_Nonnull filter)
{
    return filter->count;
}

void index_filter_destroy(struct index_filter *_Nullable filter)
{
    if (filter == nullptr)
    {
        return;
    }
    free(filter->categories);
    free(filter->notes);
    free(filter);
}
