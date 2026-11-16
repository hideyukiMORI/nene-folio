/* 閲覧RTFのコードブロックが持つ「コピー」操作の内部宛先（ADR0098）。
 * 形は `nenefolio-code:<slot>`（slot は先頭ゼロの無い10進）。書くのは markdown_blocks だけで、
 * 利用者のリンクは web_uri の検証（http/https）を通ったものしか field にならないので、
 * Markdown からこの宛先は作れない。読む側（UI）はここの parse だけを使う。 */
#ifndef NENEFOLIO_CODE_COPY_TARGET_H
#define NENEFOLIO_CODE_COPY_TARGET_H
#include "code_copy_target_outcome.h"
#include <stddef.h>
struct text_buffer;
/* HYPERLINK の宛先として書く。RTF で escape が要る文字を含まない。 */
void code_copy_target_append(struct text_buffer *_Nonnull out, size_t slot);
/* SLOT のときだけ slot を書く。正規形でない・上限以上の番号は OTHER。 */
[[nodiscard]] enum code_copy_target_outcome
code_copy_target_parse(const char *_Nonnull bytes, size_t length, size_t *_Nonnull slot);
#endif
