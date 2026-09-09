/* ノート 1 つの本文（FR-005 / FR-006）。UTF-8 として検証し、先頭の BOM を捨てて所有する。
 * 改行は元のまま（LF / CRLF）。解釈は markdown_rtf が行う（ARC-008）。
 * 編集した本文は note_text_from_editor で元の改行の形へ揃えてから書き戻す（ADR 0006）。 */
#ifndef NENEFOLIO_NOTE_TEXT_H
#define NENEFOLIO_NOTE_TEXT_H

#include "line_ending.h"
#include "note_text_outcome.h"

#include <stddef.h>

struct note_text;

[[nodiscard]] enum note_text_outcome note_text_create(const char *_Nonnull bytes, size_t length,
                                                      struct note_text *_Nullable *_Nonnull out);
/* 編集した本文を受け取り、CR / CRLF / LF を ending の形に畳んで所有する。末尾改行の有無は触らない。
 */
[[nodiscard]] enum note_text_outcome
note_text_from_editor(const char *_Nonnull bytes, size_t length, enum line_ending ending,
                      struct note_text *_Nullable *_Nonnull out);
/* 最初の改行で決まる形。改行が無ければ LF。 */
[[nodiscard]] enum line_ending note_text_line_ending(const struct note_text *_Nonnull text);
/* バイト列として同じか。同じなら書き戻さない（ADR 0006）。 */
[[nodiscard]] bool note_text_equals(const struct note_text *_Nonnull text,
                                    const struct note_text *_Nonnull other);
/* 終端付き。text が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull note_text_bytes(const struct note_text *_Nonnull text);
[[nodiscard]] size_t note_text_length(const struct note_text *_Nonnull text);
void note_text_destroy(struct note_text *_Nullable text);

#endif
