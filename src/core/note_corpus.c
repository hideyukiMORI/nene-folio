#include "note_corpus.h"

#include "note_text.h"

#include <stdlib.h>
#include <string.h>

struct note_corpus
{
    /* 宛先はカテゴリ名、ノート名の strcmp 順で厳密昇順。台帳の表示順は持たない。 */
    char *_Nullable *_Nullable categories;         /* 写しごとのカテゴリ名 */
    char *_Nullable *_Nullable notes;              /* 同じ数のノート名 */
    struct note_text *_Nullable *_Nullable bodies; /* 同じ数の本文 */
    size_t count;
    size_t capacity;
};

enum note_corpus_outcome note_corpus_create(struct note_corpus *_Nullable *_Nonnull out)
{
    struct note_corpus *_Nullable corpus = calloc(1, sizeof *corpus);
    if (corpus == nullptr)
    {
        return NOTE_CORPUS_OUT_OF_MEMORY;
    }
    *out = corpus;
    return NOTE_CORPUS_ACCEPTED;
}

/* 名前を終端ごと複製する。長さを持たない strcpy 系は使わない（C-016）。 */
static char *_Nullable duplicate(const char *_Nonnull name)
{
    size_t length = strlen(name) + 1;
    char *_Nullable copy = malloc(length);
    if (copy != nullptr)
    {
        memcpy(copy, name, length);
    }
    return copy;
}

static int compare(const struct note_corpus *_Nonnull corpus, size_t index,
                   const char *_Nonnull category, const char *_Nonnull note)
{
    int category_order = strcmp(corpus->categories[index], category);
    return category_order == 0 ? strcmp(corpus->notes[index], note) : category_order;
}

/* その宛先以上の最初の位置。無ければ count。 */
static size_t lower_bound(const struct note_corpus *_Nonnull corpus, const char *_Nonnull category,
                          const char *_Nonnull note)
{
    size_t low = 0;
    size_t high = corpus->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        bool earlier = compare(corpus, middle, category, note) < 0;
        low = earlier ? middle + 1 : low;
        high = earlier ? high : middle;
    }
    return low;
}

/* その宛先の写しの番号。無ければ count。 */
static size_t locate(const struct note_corpus *_Nonnull corpus, const char *_Nonnull category,
                     const char *_Nonnull note)
{
    size_t index = lower_bound(corpus, category, note);
    return index < corpus->count && compare(corpus, index, category, note) == 0 ? index
                                                                                : corpus->count;
}

/* 3 配列の対応を保って位置だけを移す。本文そのものは動かさない。 */
static void shift(struct note_corpus *_Nonnull corpus, size_t to, size_t from, size_t count)
{
    if (count == 0)
    {
        return;
    }
    memmove(corpus->categories + to, corpus->categories + from, count * sizeof *corpus->categories);
    memmove(corpus->notes + to, corpus->notes + from, count * sizeof *corpus->notes);
    memmove(corpus->bodies + to, corpus->bodies + from, count * sizeof *corpus->bodies);
}

static bool reserve(struct note_corpus *_Nonnull corpus)
{
    if (corpus->count < corpus->capacity)
    {
        return true;
    }
    size_t capacity = corpus->capacity == 0 ? 4 : corpus->capacity * 2;
    char *_Nullable *_Nullable categories =
        realloc(corpus->categories, capacity * sizeof *categories);
    if (categories == nullptr)
    {
        return false;
    }
    corpus->categories = categories;
    char *_Nullable *_Nullable notes = realloc(corpus->notes, capacity * sizeof *notes);
    if (notes == nullptr)
    {
        return false;
    }
    corpus->notes = notes;
    struct note_text *_Nullable *_Nullable bodies =
        realloc(corpus->bodies, capacity * sizeof *bodies);
    if (bodies == nullptr)
    {
        return false;
    }
    corpus->bodies = bodies;
    corpus->capacity = capacity;
    return true;
}

/* index の写しを捨て、後続を詰めて宛先順を保つ。index が count 以上なら何もしない。 */
static void discard(struct note_corpus *_Nonnull corpus, size_t index)
{
    if (index >= corpus->count)
    {
        return;
    }
    free(corpus->categories[index]);
    free(corpus->notes[index]);
    note_text_destroy(corpus->bodies[index]);
    corpus->count -= 1;
    shift(corpus, index, index + 1, corpus->count - index);
}

void note_corpus_remove(struct note_corpus *_Nonnull corpus, const char *_Nonnull category,
                        const char *_Nonnull note)
{
    discard(corpus, locate(corpus, category, note));
}

/* index の写しの宛先を新しい名前へ差し替える。確保に失敗したら古い名前のまま。 */
static enum note_corpus_outcome rekey(struct note_corpus *_Nonnull corpus, size_t index,
                                      const char *_Nonnull category, const char *_Nonnull note)
{
    char *_Nullable left = duplicate(category);
    char *_Nullable right = duplicate(note);
    if (left == nullptr || right == nullptr)
    {
        free(left);
        free(right);
        return NOTE_CORPUS_OUT_OF_MEMORY;
    }
    size_t destination = lower_bound(corpus, category, note);
    destination -= index < destination ? 1 : 0;
    struct note_text *_Nonnull body = corpus->bodies[index];
    free(corpus->categories[index]);
    free(corpus->notes[index]);
    if (destination < index)
    {
        shift(corpus, destination + 1, destination, index - destination);
    }
    if (destination > index)
    {
        shift(corpus, index, index + 1, destination - index);
    }
    corpus->categories[destination] = left;
    corpus->notes[destination] = right;
    corpus->bodies[destination] = body;
    return NOTE_CORPUS_ACCEPTED;
}

/* 新しい宛先の写しを足す。名前も本文も複製するので、どれか 1 つでも作れなければ全部捨てる。 */
static enum note_corpus_outcome append(struct note_corpus *_Nonnull corpus,
                                       const char *_Nonnull category, const char *_Nonnull note,
                                       struct note_text *_Nonnull body)
{
    char *_Nullable left = duplicate(category);
    char *_Nullable right = duplicate(note);
    if (left == nullptr || right == nullptr || !reserve(corpus))
    {
        free(left);
        free(right);
        note_text_destroy(body);
        return NOTE_CORPUS_OUT_OF_MEMORY;
    }
    size_t index = lower_bound(corpus, category, note);
    shift(corpus, index + 1, index, corpus->count - index);
    corpus->categories[index] = left;
    corpus->notes[index] = right;
    corpus->bodies[index] = body;
    corpus->count += 1;
    return NOTE_CORPUS_ACCEPTED;
}

enum note_corpus_outcome note_corpus_put(struct note_corpus *_Nonnull corpus,
                                         const char *_Nonnull category, const char *_Nonnull note,
                                         const struct note_text *_Nonnull body)
{
    /* bodyは唯一のnote_text生成でUTF-8/共通上限を検証済み。複製で失敗し得るのは確保だけ。 */
    struct note_text *_Nullable copy = nullptr;
    if (note_text_copy(body, &copy) != NOTE_TEXT_ACCEPTED)
    {
        return NOTE_CORPUS_OUT_OF_MEMORY;
    }
    size_t index = locate(corpus, category, note);
    if (index < corpus->count)
    {
        note_text_destroy(corpus->bodies[index]);
        corpus->bodies[index] = copy;
        return NOTE_CORPUS_ACCEPTED;
    }
    return append(corpus, category, note, copy);
}

enum note_corpus_outcome note_corpus_rename(struct note_corpus *_Nonnull corpus,
                                            const char *_Nonnull category,
                                            const char *_Nonnull from, const char *_Nonnull to)
{
    /* 新しい名前に古い写しが残っていれば先に捨てる（同名は上の層が断るので普段は無い）。 */
    discard(corpus, locate(corpus, category, to));
    size_t index = locate(corpus, category, from);
    if (index == corpus->count)
    {
        return NOTE_CORPUS_ACCEPTED;
    }
    return rekey(corpus, index, category, to);
}

enum note_corpus_outcome note_corpus_relocate(struct note_corpus *_Nonnull corpus,
                                              const char *_Nonnull from_category,
                                              const char *_Nonnull note,
                                              const char *_Nonnull to_category)
{
    discard(corpus, locate(corpus, to_category, note));
    size_t index = locate(corpus, from_category, note);
    if (index == corpus->count)
    {
        return NOTE_CORPUS_ACCEPTED;
    }
    return rekey(corpus, index, to_category, note);
}

const struct note_text *_Nullable note_corpus_body(const struct note_corpus *_Nonnull corpus,
                                                   const char *_Nonnull category,
                                                   const char *_Nonnull note)
{
    size_t index = locate(corpus, category, note);
    return index < corpus->count ? corpus->bodies[index] : nullptr;
}

void note_corpus_destroy(struct note_corpus *_Nullable corpus)
{
    if (corpus == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < corpus->count; ++index)
    {
        free(corpus->categories[index]);
        free(corpus->notes[index]);
        note_text_destroy(corpus->bodies[index]);
    }
    free(corpus->categories);
    free(corpus->notes);
    free(corpus->bodies);
    free(corpus);
}
