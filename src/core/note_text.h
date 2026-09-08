/* ノート 1 つの本文（FR-005）。UTF-8 として検証し、先頭の BOM を捨てて所有する（ARC-008）。
 * 改行は元のまま（LF / CRLF）。解釈は markdown_rtf が行う。 */
#ifndef NENEFOLIO_NOTE_TEXT_H
#define NENEFOLIO_NOTE_TEXT_H

#include "note_text_outcome.h"

#include <stddef.h>

struct note_text;

[[nodiscard]] enum note_text_outcome note_text_create(const char *_Nonnull bytes, size_t length,
                                                      struct note_text *_Nullable *_Nonnull out);
/* 終端付き。text が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull note_text_bytes(const struct note_text *_Nonnull text);
[[nodiscard]] size_t note_text_length(const struct note_text *_Nonnull text);
void note_text_destroy(struct note_text *_Nullable text);

#endif
