/* Adapter-observed local file times. This immutable display value never reads a clock. */
#ifndef NENEFOLIO_NOTE_FILE_TIMES_H
#define NENEFOLIO_NOTE_FILE_TIMES_H

#include "note_file_times_outcome.h"

struct note_file_times;

/* Both inputs have the fixed YYYY-MM-DD HH:mm representation. Failed creation leaves out alone. */
[[nodiscard]] enum note_file_times_outcome
note_file_times_create(const char *_Nonnull created, const char *_Nonnull updated,
                       struct note_file_times *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_file_times_created(
    const struct note_file_times *_Nonnull times);
[[nodiscard]] const char *_Nonnull note_file_times_updated(
    const struct note_file_times *_Nonnull times);
void note_file_times_destroy(struct note_file_times *_Nullable times);

#endif
