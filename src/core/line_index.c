#include "line_index.h"

#include <stdlib.h>

struct line_index
{
    size_t *_Nullable breaks; /* CR の位置の昇順。論理行の数 - 1 個 */
    size_t break_count;
    size_t capacity;
    size_t length; /* 本文の長さ（UTF-16 単位）。位置の範囲の上限 */
};

/* 番号の帯が最初から動かないよう、9 行の本文でも 3 桁ぶんを取る（決定 2 の (d)）。 */
constexpr size_t minimum_digits = 3;
constexpr char16_t paragraph_break = u'\r';

static bool reserve(struct line_index *_Nonnull index)
{
    if (index->break_count < index->capacity)
    {
        return true;
    }
    size_t capacity = index->capacity == 0 ? 16 : index->capacity * 2;
    size_t *_Nullable grown = realloc(index->breaks, capacity * sizeof *grown);
    if (grown == nullptr)
    {
        return false;
    }
    index->breaks = grown;
    index->capacity = capacity;
    return true;
}

enum line_index_outcome line_index_create(const char16_t *_Nonnull units, size_t count,
                                          struct line_index *_Nullable *_Nonnull out)
{
    struct line_index *_Nullable index = calloc(1, sizeof *index);
    if (index == nullptr)
    {
        return LINE_INDEX_OUT_OF_MEMORY;
    }
    index->length = count;
    for (size_t position = 0; position < count; ++position)
    {
        if (units[position] != paragraph_break)
        {
            continue;
        }
        if (!reserve(index))
        {
            line_index_destroy(index);
            return LINE_INDEX_OUT_OF_MEMORY;
        }
        index->breaks[index->break_count] = position;
        index->break_count += 1;
    }
    *out = index;
    return LINE_INDEX_READY;
}

size_t line_index_count(const struct line_index *_Nonnull index)
{
    return index->break_count + 1;
}

/* position より前にある CR の数（二分探索）。そのまま 0 始まりの論理行番号になる。 */
static size_t breaks_before(const struct line_index *_Nonnull index, size_t position)
{
    size_t low = 0;
    size_t high = index->break_count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (index->breaks[middle] < position)
        {
            low = middle + 1;
            continue;
        }
        high = middle;
    }
    return low;
}

enum line_index_outcome line_index_at(const struct line_index *_Nonnull index, size_t position,
                                      struct line_mark *_Nonnull out)
{
    if (position > index->length)
    {
        return LINE_INDEX_OUT_OF_RANGE;
    }
    size_t before = breaks_before(index, position);
    /* 先頭は、位置 0 か、直前が CR のとき（表の最後の CR が position - 1 なら真）。 */
    bool first = position == 0 || (before > 0 && index->breaks[before - 1] == position - 1);
    out->number = before + 1;
    out->first = first;
    return LINE_INDEX_READY;
}

enum line_index_outcome line_index_start(const struct line_index *_Nonnull index, size_t number,
                                         size_t *_Nonnull out)
{
    if (number == 0 || number > line_index_count(index))
    {
        return LINE_INDEX_OUT_OF_RANGE;
    }
    *out = number == 1 ? 0 : index->breaks[number - 2] + 1;
    return LINE_INDEX_READY;
}

size_t line_index_digits(const struct line_index *_Nonnull index)
{
    size_t digits = 1;
    for (size_t remaining = line_index_count(index); remaining >= 10; remaining /= 10)
    {
        digits += 1;
    }
    return digits < minimum_digits ? minimum_digits : digits;
}

void line_index_destroy(struct line_index *_Nullable index)
{
    if (index == nullptr)
    {
        return;
    }
    free(index->breaks);
    free(index);
}
