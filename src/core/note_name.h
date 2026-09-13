/* 新しく付けるmd名。拡張子を除く名前を所有し、name_listの検証を共有する（ADR0020）。 */
#ifndef NENEFOLIO_NOTE_NAME_H
#define NENEFOLIO_NOTE_NAME_H
#include "note_name_outcome.h"
#include <stddef.h>
struct note_name;
[[nodiscard]] enum note_name_outcome note_name_create(const char *_Nonnull text, size_t length,
                                                      struct note_name *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_name_stem(const struct note_name *_Nonnull name);
void note_name_destroy(struct note_name *_Nullable name);
#endif
