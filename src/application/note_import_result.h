#ifndef NENEFOLIO_NOTE_IMPORT_RESULT_H
#define NENEFOLIO_NOTE_IMPORT_RESULT_H
#include "folio_state_outcome.h"
#include <stddef.h>
struct note_import_result
{
    size_t added;
    size_t skipped;
    size_t remaining;
    enum folio_state_outcome outcome;
};
#endif
