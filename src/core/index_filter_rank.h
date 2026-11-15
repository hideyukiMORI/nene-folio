/* Private completed-match record owned by index_filter_build; no mutable view is returned. */
#ifndef NENEFOLIO_INDEX_FILTER_RANK_H
#define NENEFOLIO_INDEX_FILTER_RANK_H
#include "index_match_kind.h"
#include <stddef.h>
struct index_filter_rank
{
    enum index_match_kind kind;
    bool favorite;
    size_t next;
};
#endif
