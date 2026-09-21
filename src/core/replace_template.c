#include "replace_template.h"

#include "regex_match.h"

#include <stdlib.h>

/* RichEdit の段落区切り。`\r` も `\n` もこれ 1 つになる（ADR 0028 の決定 5）。 */
constexpr char16_t paragraph_break = u'\r';
/* 「この断片の後ろには何も差し込まない」印。0〜9 は本物の群番号なので範囲の外に置く。 */
constexpr size_t no_group = regex_match_groups + 1;

struct replace_template
{
    /* 断片 1 つ。literals の [start, start + length) を置き、その後ろに group を差し込む。 */
    struct replace_piece
    {
        size_t start;
        size_t length;
        size_t group; /* 0 = 一致の全体、1〜9 = 群、no_group = 差し込まない */
    } *_Nonnull pieces;
    size_t count;
    char16_t *_Nonnull literals;
    size_t literal_count;
};

/* エスケープが置く 1 文字。未知の `\x` なら false（決定 5）。 */
static bool escape_literal(char16_t escaped, char16_t *_Nonnull out)
{
    if (escaped == u'r' || escaped == u'n')
    {
        *out = paragraph_break;
        return true;
    }
    if (escaped == u'\\' || escaped == u'&' || escaped == u'/')
    {
        *out = escaped;
        return true;
    }
    return false;
}

/* いま置いた literal の続きを 1 つの断片にして、後ろに差し込む群を記録する。
 * 断片の開始は直前の断片の終わりなので、呼び出し側は位置を覚えなくてよい。 */
static void close_piece(struct replace_template *_Nonnull replacement, size_t group)
{
    size_t start = 0;
    if (replacement->count > 0)
    {
        const struct replace_piece *_Nonnull previous =
            &replacement->pieces[replacement->count - 1];
        start = previous->start + previous->length;
    }
    replacement->pieces[replacement->count].start = start;
    replacement->pieces[replacement->count].length = replacement->literal_count - start;
    replacement->pieces[replacement->count].group = group;
    replacement->count += 1;
}

static void put_literal(struct replace_template *_Nonnull replacement, char16_t unit)
{
    replacement->literals[replacement->literal_count] = unit;
    replacement->literal_count += 1;
}

/* `\` の次の 1 文字。数字なら群として断片を閉じ、それ以外は 1 文字置く。未知なら false。 */
static bool read_escape(struct replace_template *_Nonnull replacement, char16_t escaped)
{
    if (escaped >= u'0' && escaped <= u'9')
    {
        close_piece(replacement, (size_t)(escaped - u'0'));
        return true;
    }
    char16_t literal = 0;
    if (!escape_literal(escaped, &literal))
    {
        return false;
    }
    put_literal(replacement, literal);
    return true;
}

/* 記号を 1 つ読み、index を読んだぶんだけ進める。解けなければ false。 */
static bool read_token(struct replace_template *_Nonnull replacement,
                       const char16_t *_Nonnull units, size_t count, size_t *_Nonnull index)
{
    char16_t unit = units[*index];
    *index += 1;
    if (unit == u'&')
    {
        close_piece(replacement, 0);
        return true;
    }
    if (unit != u'\\')
    {
        put_literal(replacement, unit);
        return true;
    }
    if (*index == count)
    {
        return false; /* 末尾の単独の `\` は黙って消さずに断る（決定 5） */
    }
    char16_t escaped = units[*index];
    *index += 1;
    return read_escape(replacement, escaped);
}

static enum replace_template_outcome parse(struct replace_template *_Nonnull replacement,
                                           const char16_t *_Nonnull units, size_t count)
{
    size_t index = 0;
    while (index < count)
    {
        if (!read_token(replacement, units, count, &index))
        {
            return REPLACE_TEMPLATE_MALFORMED;
        }
    }
    close_piece(replacement, no_group);
    return REPLACE_TEMPLATE_READY;
}

enum replace_template_outcome
replace_template_create(const char16_t *_Nonnull units, size_t count,
                        struct replace_template *_Nullable *_Nonnull out)
{
    struct replace_template *_Nullable replacement = calloc(1, sizeof *replacement);
    if (replacement == nullptr)
    {
        return REPLACE_TEMPLATE_OUT_OF_MEMORY;
    }
    /* literal は入力より長くならず、断片は「群の数 + 1」なので count + 1 で必ず足りる。 */
    replacement->literals = calloc(count + 1, sizeof *replacement->literals);
    replacement->pieces = calloc(count + 1, sizeof *replacement->pieces);
    enum replace_template_outcome parsed = REPLACE_TEMPLATE_OUT_OF_MEMORY;
    if (replacement->literals != nullptr && replacement->pieces != nullptr)
    {
        parsed = parse(replacement, units, count);
    }
    if (parsed != REPLACE_TEMPLATE_READY)
    {
        replace_template_destroy(replacement);
        return parsed;
    }
    *out = replacement;
    return REPLACE_TEMPLATE_READY;
}

size_t replace_template_count(const struct replace_template *_Nonnull replacement)
{
    return replacement->count;
}

const char16_t *_Nonnull replace_template_literal(
    const struct replace_template *_Nonnull replacement, size_t index)
{
    return replacement->literals + replacement->pieces[index].start;
}

size_t replace_template_literal_length(const struct replace_template *_Nonnull replacement,
                                       size_t index)
{
    return replacement->pieces[index].length;
}

bool replace_template_group(const struct replace_template *_Nonnull replacement, size_t index,
                            size_t *_Nonnull out)
{
    if (replacement->pieces[index].group == no_group)
    {
        return false;
    }
    *out = replacement->pieces[index].group;
    return true;
}

void replace_template_destroy(struct replace_template *_Nullable replacement)
{
    if (replacement == nullptr)
    {
        return;
    }
    free(replacement->literals);
    free(replacement->pieces);
    free(replacement);
}
