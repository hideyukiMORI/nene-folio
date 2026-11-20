/* #1062 / ADR0120: 取り込んだ画像を指す Markdown を作る。ファイル名から代替の文とパスを決める
 * 唯一の場所（ARC-001）。ファイルには触らない純粋な関数。 */
#ifndef NENEFOLIO_IMAGE_LINK_H
#define NENEFOLIO_IMAGE_LINK_H
#include <stddef.h>
struct text_buffer;
/* 取り込んだ画像を置くフォルダ（md のあるフォルダからの相対。ADR0085）。 */
constexpr char image_link_folder[] = "images";
/* `![代替の文](images/名前)` を out へ足す。leaf は images/ の中のファイル名（UTF-8、区切りを
 * 含まない）。代替の文は、最後の拡張子を除いた名前から `[` `]` `\` を除いたもの。パスに空白か
 * 丸括弧があれば山括弧で囲み、`%` は `%25` と書く（画像の読みは %XX を戻すため）。
 * 失敗は out が覚える（text_buffer_finish）。 */
void image_link_append(struct text_buffer *_Nonnull out, const char *_Nonnull leaf, size_t length);
/* 同じ名前で中身の違う画像があるときの名前: `名前-N.拡張子`（拡張子が無ければ `名前-N`）。 */
void image_link_numbered(struct text_buffer *_Nonnull out, const char *_Nonnull leaf, size_t length,
                         size_t number);
#endif
