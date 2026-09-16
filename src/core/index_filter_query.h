/* 絞り込みの問い合わせ（ADR 0024 の決定 2）。語と、索引にある全ノートの名前・本文を借りる。
 * note_search_query と同じ流儀で、core は何も所有しないまま判定に必要なものだけを受ける。 */
#ifndef NENEFOLIO_INDEX_FILTER_QUERY_H
#define NENEFOLIO_INDEX_FILTER_QUERY_H

#include "index_filter_entry.h"

#include <stddef.h>

struct index_filter_query
{
    const char *_Nonnull term;                         /* 語（UTF-8・終端は要らない） */
    size_t term_length;                                /* 語のバイト数。0 なら絞り込みなし */
    const struct index_filter_entry *_Nonnull entries; /* 索引にある全ノート（台帳の順） */
    size_t count;                                      /* entries の数 */
};

#endif
