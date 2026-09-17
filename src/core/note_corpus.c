#include "note_corpus.h"

#include "note_text.h"

#include <stdlib.h>
#include <string.h>

struct note_corpus
{
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

/* その宛先の写しの番号。無ければ count。 */
static size_t locate(const struct note_corpus *_Nonnull corpus, const char *_Nonnull category,
                     const char *_Nonnull note)
{
    for (size_t index = 0; index < corpus->count; ++index)
    {
        if (strcmp(corpus->categories[index], category) == 0 &&
            strcmp(corpus->notes[index], note) == 0)
        {
            return index;
        }
    }
    return corpus->count;
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

/* index の写しを捨て、最後の写しを穴へ移す。index が count 以上なら何もしない。 */
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
    corpus->categories[index] = corpus->categories[corpus->count];
    corpus->notes[index] = corpus->notes[corpus->count];
    corpus->bodies[index] = corpus->bodies[corpus->count];
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
    free(corpus->categories[index]);
    free(corpus->notes[index]);
    corpus->categories[index] = left;
    corpus->notes[index] = right;
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
    corpus->categories[corpus->count] = left;
    corpus->notes[corpus->count] = right;
    corpus->bodies[corpus->count] = body;
    corpus->count += 1;
    return NOTE_CORPUS_ACCEPTED;
}

enum note_corpus_outcome note_corpus_put(struct note_corpus *_Nonnull corpus,
                                         const char *_Nonnull category, const char *_Nonnull note,
                                         const struct note_text *_Nonnull body)
{
    struct note_text *_Nullable copy = nullptr;
    if (note_text_create(note_text_bytes(body), note_text_length(body), &copy) !=
        NOTE_TEXT_ACCEPTED)
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
