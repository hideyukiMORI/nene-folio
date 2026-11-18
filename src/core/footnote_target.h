/* 閲覧RTFの脚注の番号が持つ内部宛先（#945 / ADR0103の補足）。
 * 形は `nenefolio-note:<n>`（本文の番号 → 脚注 n）と `nenefolio-noteref:<n>`（脚注の番号 → 本文の
 * 最初の参照）。n は 1 以上・脚注の上限以下の、先頭ゼロの無い10進。書くのは markdown の描画だけで、
 * 利用者のリンクは web_uri の検証（http/https）を通ったものしか field にならないので、
 * Markdown からこの宛先は作れない。読む側（UI）はここの parse だけを使う。 */
#ifndef NENEFOLIO_FOOTNOTE_TARGET_H
#define NENEFOLIO_FOOTNOTE_TARGET_H
#include "footnote_target_outcome.h"
#include <stddef.h>
struct text_buffer;
/* HYPERLINK の宛先として書く。back なら脚注から本文へ戻る側。RTF で escape が要る文字を含まない。
 */
void footnote_target_append(struct text_buffer *_Nonnull out, bool back, size_t number);
/* NOTE / REFERENCE のときだけ number を書く。正規形でない・範囲外の番号は OTHER。 */
[[nodiscard]] enum footnote_target_outcome
footnote_target_parse(const char *_Nonnull bytes, size_t length, size_t *_Nonnull number);
#endif
