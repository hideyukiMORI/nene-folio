/* 閲覧RTFの見出しの前に置く、非表示の印（#975 / ADR0114）。
 * 形は `<nenefolio:heading:L>`（L は見出しの階層 1〜6）を非表示の文字として書いたもの。画像の印
 * （`<nenefolio:image:N>`）と同じく、見える文字・検索・コピーには現れない。書くのは markdown の描画
 * だけで、読む側（UI）はここの parse だけを使い、区間が非表示であることを自分で確かめる
 * （本文に同じ文字を書いても印にはならない）。 */
#ifndef NENEFOLIO_HEADING_TARGET_H
#define NENEFOLIO_HEADING_TARGET_H
#include <stddef.h>
struct text_buffer;
constexpr size_t heading_target_levels = 6;
/* 印の文字数（どの階層でも同じ）。 */
constexpr size_t heading_target_length = 21;
/* 見出しの段落の先頭に書く。level は 1〜6。 */
void heading_target_append(struct text_buffer *_Nonnull out, size_t level);
/* bytes[0, length) がちょうど1つの印なら階層（1〜6）、そうでなければ 0。 */
[[nodiscard]] size_t heading_target_parse(const char *_Nonnull bytes, size_t length);
#endif
