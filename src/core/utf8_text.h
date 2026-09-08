/* UTF-16 から作った UTF-8 の所有文字列と、UTF-8 の符号化・復号（C-014）。
 * 内部文字列は UTF-8 で持ち、UTF-16 は Win32 境界（ui / adapters）でだけ現れる。変換は純関数で
 * ここに 1 つだけ置く（ARC-001）。C では wchar_t と char16_t は同じ型（2026-09-09 実測・ADR
 * 0004）。 */
#ifndef NENEFOLIO_UTF8_TEXT_H
#define NENEFOLIO_UTF8_TEXT_H

#include "utf8_text_outcome.h"

#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

struct utf8_text;

/* 1 コードポイントの UTF-8 符号化に要る最大バイト数。 */
constexpr size_t utf8_text_max_encoded = 4;

[[nodiscard]] enum utf8_text_outcome utf8_text_create(const char16_t *_Nonnull units, size_t length,
                                                      struct utf8_text *_Nullable *_Nonnull out);
/* 終端付きの UTF-8。text が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull utf8_text_bytes(const struct utf8_text *_Nonnull text);
[[nodiscard]] size_t utf8_text_length(const struct utf8_text *_Nonnull text);
void utf8_text_destroy(struct utf8_text *_Nullable text);

/* bytes の先頭 1 文字を復号し、消費したバイト数を返す。不正な列（過長符号化・サロゲート・
 * U+10FFFF 超・切れた列）は 0。 */
[[nodiscard]] size_t utf8_text_decode(const char *_Nonnull bytes, size_t length,
                                      uint32_t *_Nonnull code_point);
/* code_point を out（utf8_text_max_encoded バイト以上）へ符号化し、書いたバイト数を返す。
 * サロゲートと U+10FFFF 超は 0。 */
[[nodiscard]] size_t utf8_text_encode(uint32_t code_point, char *_Nonnull out);

#endif
