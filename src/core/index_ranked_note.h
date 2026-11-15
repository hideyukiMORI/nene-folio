/* Completed name-search projection; values never borrow the query or source text (ADR0094). */
#ifndef NENEFOLIO_INDEX_RANKED_NOTE_H
#define NENEFOLIO_INDEX_RANKED_NOTE_H
#include "index_match_kind.h"
#include "note_ref.h"
constexpr size_t index_ranked_limit = 200;
struct index_ranked_note
{
    struct note_ref ref;
    enum index_match_kind kind;
    bool favorite;
};
#endif
