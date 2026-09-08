/* 伸びるバイト列（終端付き）。途中の記憶不足は記憶され、text_buffer_finish がまとめて返す。 */
#ifndef NENEFOLIO_TEXT_BUFFER_H
#define NENEFOLIO_TEXT_BUFFER_H

#include "text_buffer_outcome.h"

#include <stddef.h>

struct text_buffer;

[[nodiscard]] enum text_buffer_outcome
text_buffer_create(struct text_buffer *_Nullable *_Nonnull out);
void text_buffer_append(struct text_buffer *_Nonnull buffer, const char *_Nonnull bytes,
                        size_t count);
/* 終端付きの文字列を足す。 */
void text_buffer_append_text(struct text_buffer *_Nonnull buffer, const char *_Nonnull text);
[[nodiscard]] enum text_buffer_outcome
text_buffer_finish(const struct text_buffer *_Nonnull buffer);
/* 終端付き。buffer が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull text_buffer_bytes(const struct text_buffer *_Nonnull buffer);
[[nodiscard]] size_t text_buffer_length(const struct text_buffer *_Nonnull buffer);
void text_buffer_destroy(struct text_buffer *_Nullable buffer);

#endif
