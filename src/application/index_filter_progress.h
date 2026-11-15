#ifndef NENEFOLIO_INDEX_FILTER_PROGRESS_H
#define NENEFOLIO_INDEX_FILTER_PROGRESS_H
#include "note_ref.h"
/* Call-local driver input/output; the core owner retains all comparison progress. */
struct index_filter_progress
{
    struct note_ref cursor;
    size_t work;
    bool uses_body;
};
#endif
