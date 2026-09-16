/* 一度の問い合わせで見る本文と語（ADR 0023 の決定 1 / 2）。どちらも借りるだけで所有しない。
 * 本文は RichEdit が今表示している平文で、UI が呼び出しの間だけ貸す。 */
#ifndef NENEFOLIO_NOTE_SEARCH_QUERY_H
#define NENEFOLIO_NOTE_SEARCH_QUERY_H

#include <stddef.h>
#include <uchar.h>

struct note_search_query
{
    const char16_t *_Nonnull text;
    size_t length;
    const char16_t *_Nonnull term;
    size_t term_length;
};

#endif
