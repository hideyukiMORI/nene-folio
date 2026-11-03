#ifndef NENEFOLIO_NOTE_TIMES_VIEW_H
#define NENEFOLIO_NOTE_TIMES_VIEW_H

#include "note_times_state.h"

/* Borrowed display values, valid until the next state intent. No file access in the query. */
struct note_times_view
{
    enum note_times_state state;
    const char *_Nonnull created;
    const char *_Nonnull updated;
};

#endif
