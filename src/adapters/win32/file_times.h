#ifndef NENEFOLIO_FILE_TIMES_H
#define NENEFOLIO_FILE_TIMES_H

#include "persistence_outcome.h"
#include <stddef.h>

struct note_file_times;

[[nodiscard]] enum persistence_outcome
file_times_read(const wchar_t *_Nonnull path, struct note_file_times *_Nullable *_Nonnull out);

#endif
