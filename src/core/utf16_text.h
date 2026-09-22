/* UTF-8 から作った UTF-16 の所有文字列（C-014）。Win32 の W 系 API へ渡す直前にだけ作る。 */
#ifndef NENEFOLIO_UTF16_TEXT_H
#define NENEFOLIO_UTF16_TEXT_H

#include "utf16_text_fill_outcome.h"
#include "utf16_text_outcome.h"

#include <stddef.h>
#include <uchar.h>

struct utf16_text;

[[nodiscard]] enum utf16_text_outcome utf16_text_create(const char *_Nonnull bytes, size_t length,
                                                        struct utf16_text *_Nullable *_Nonnull out);
/* 終端付きの UTF-16。text が生きている間だけ有効。 */
[[nodiscard]] const char16_t *_Nonnull utf16_text_units(const struct utf16_text *_Nonnull text);
[[nodiscard]] size_t utf16_text_length(const struct utf16_text *_Nonnull text);
void utf16_text_destroy(struct utf16_text *_Nullable text);
/* 確保せずに、終端付きの UTF-8 を呼び出し側の入れ物へ写す（ADR 0030 の決定 5）。
 * capacity は終端を含む単位数、written は終端を除く書いた単位数。
 * 収まらない・壊れている場合は out も written も触らない。 */
[[nodiscard]] enum utf16_text_fill_outcome utf16_text_fill(const char *_Nonnull utf8,
                                                           char16_t *_Nonnull out, size_t capacity,
                                                           size_t *_Nonnull written);

#endif
