/* Borrowed immutable conditions for the one index matcher (ADR0092). */
#ifndef NENEFOLIO_INDEX_FILTER_QUERY_H
#define NENEFOLIO_INDEX_FILTER_QUERY_H
#include "index_match_scope.h"
#include "tag_match_mode.h"
#include <stddef.h>
struct note_tags;
struct index_filter_query
{
    const char *_Nonnull term;
    size_t length;
    const struct note_tags *_Nullable tags; /* Omitted means no tag restriction. */
    enum tag_match_mode mode;
    enum index_match_scope scope;
    bool favorites_only;  /* NAMES only. NAME_BODY preserves its existing empty-query contract. */
    bool favorites_first; /* NAMES only. */
};
#endif
