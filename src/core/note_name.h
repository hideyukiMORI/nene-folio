/* 新しく付けるmd名。拡張子を除く名前を所有し、name_listの検証を共有する（ADR0020）。 */
#ifndef NENEFOLIO_NOTE_NAME_H
#define NENEFOLIO_NOTE_NAME_H
#include "note_name_outcome.h"
#include <stddef.h>
struct note_name;
struct text_buffer;
[[nodiscard]] enum note_name_outcome note_name_create(const char *_Nonnull text, size_t length,
                                                      struct note_name *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_name_stem(const struct note_name *_Nonnull name);
void note_name_destroy(struct note_name *_Nullable name);
/* #1152 / ADR0119: 編集中の本文を別の名前で残すときの名前 `N_stem` を out へ足す（stem は拡張子を
 * 除いた名前）。名前として使えるかどうかは、作った名前を note_name_create へ渡して確かめる。 */
void note_name_numbered(struct text_buffer *_Nonnull out, const char *_Nonnull stem, size_t number);
#endif
