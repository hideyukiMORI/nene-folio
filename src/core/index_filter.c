#include "index_filter.h"

#include "ascii_fold.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

constexpr size_t byte_values = 256;

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

/* 語の末尾以外の右端出現位置から移動量を作る。末尾と同じ byte は 0 で完全確認へ送る。
 * 最後に raw byte 全体を唯一の ASCII fold へ写し、本文走査では fold 呼出しを避ける（#331）。 */
static void prepare_steps(const struct index_filter_query *_Nonnull query, size_t *_Nonnull steps)
{
    for (size_t byte = 0; byte < byte_values; ++byte)
    {
        steps[byte] = query->term_length;
    }
    for (size_t index = 0; index < query->term_length - 1; ++index)
    {
        steps[(unsigned char)ascii_fold_byte(query->term[index])] = query->term_length - 1 - index;
    }
    steps[(unsigned char)ascii_fold_byte(query->term[query->term_length - 1])] = 0;
    for (size_t byte = 0; byte < byte_values; ++byte)
    {
        steps[byte] = steps[(unsigned char)ascii_fold_byte((char)byte)];
    }
}

/* 1 byte の語と同じ raw byte を、既存の表の 0 から問い合わせごとに一度だけ取り出す（#331）。 */
static void prepare_bytes(const struct index_filter_query *_Nonnull query,
                          const size_t *_Nonnull steps, char *_Nonnull matches)
{
    matches[0] = query->term[0];
    matches[1] = query->term[0];
    if (query->term_length != 1)
    {
        return;
    }
    for (size_t byte = 0; byte < byte_values; ++byte)
    {
        if (steps[byte] == 0 && (char)byte != matches[0])
        {
            matches[1] = (char)byte;
        }
    }
}

static bool contains_byte(char first, char second, const char *_Nonnull text, size_t length)
{
    for (size_t at = 0; at < length; ++at)
    {
        char byte = text[at];
        if (byte == first || byte == second)
        {
            return true;
        }
    }
    return false;
}

/* 正の移動量の間には、現在の末尾 byte を置ける語の位置がない。末尾一致後は 1 byte だけ進む。
 * 先頭も選別して逆向きの密な候補を避けるが、完全比較の最悪 O(n*m) は残る。 */
static bool contains(const struct index_filter_query *_Nonnull query, const size_t *_Nonnull steps,
                     const char *_Nonnull text, size_t length)
{
    if (query->term_length > length)
    {
        return false;
    }
    char head = ascii_fold_byte(query->term[0]);
    size_t last_start = length - query->term_length;
    for (size_t at = 0;;)
    {
        size_t step = steps[(unsigned char)text[at + query->term_length - 1]];
        if (step == 0 && ascii_fold_byte(text[at]) == head &&
            matches_at(text, query->term, query->term_length, at))
        {
            return true;
        }
        step = step == 0 ? 1 : step;
        if (step > last_start - at)
        {
            return false;
        }
        at += step;
    }
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
                    const struct index_filter_entry *_Nonnull entry, const size_t *_Nonnull steps,
                    const char *_Nonnull byte_matches)
{
    if (entry->body == nullptr)
    {
        return false;
    }
    if (query->term_length == 1)
    {
        return contains_byte(byte_matches[0], byte_matches[1], entry->name, strlen(entry->name)) ||
               contains_byte(byte_matches[0], byte_matches[1], entry->body, entry->length);
    }
    return contains(query, steps, entry->name, strlen(entry->name)) ||
           contains(query, steps, entry->body, entry->length);
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
    size_t steps[byte_values];
    prepare_steps(query, steps);
    char byte_matches[2];
    prepare_bytes(query, steps, byte_matches);
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
        if (visible(query, &query->entries[index], steps, byte_matches))
        {
            admit(filter, query->entries[index].ref);
        }
    }
    *out = filter;
    return INDEX_FILTER_ACCEPTED;
}

enum index_filter_outcome index_filter_removed(const struct index_filter *_Nonnull filter,
                                               struct note_ref target,
                                               struct index_filter *_Nullable *_Nonnull out)
{
    struct index_filter *_Nullable removed = calloc(1, sizeof *removed);
    if (removed == nullptr)
    {
        return INDEX_FILTER_OUT_OF_MEMORY;
    }
    removed->categories = malloc((filter->count + 1) * sizeof *removed->categories);
    removed->notes = malloc((filter->count + 1) * sizeof *removed->notes);
    if (removed->categories == nullptr || removed->notes == nullptr)
    {
        index_filter_destroy(removed);
        return INDEX_FILTER_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < filter->count; ++index)
    {
        struct note_ref ref = {.category = filter->categories[index], .note = filter->notes[index]};
        if (ref.category == target.category && ref.note == target.note)
        {
            continue;
        }
        if (ref.category == target.category && ref.note > target.note)
        {
            ref.note -= 1;
        }
        admit(removed, ref);
    }
    *out = removed;
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
