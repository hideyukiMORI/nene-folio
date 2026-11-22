/* #1062 / ADR0120: 窓へ落とされたファイル（HDROP）を、application へ渡すパスの並びにする。
 * ファイルは開かない（開くのは image_port の先の adapter だけ）。 */
#ifndef NENEFOLIO_DROPPED_FILES_H
#define NENEFOLIO_DROPPED_FILES_H
#include <stddef.h>
#include <windows.h>

#include <shellapi.h> /* windows.h の後で読む（並べ替えさせないために、段を分けている） */
struct storage_path;
struct dropped_files;
/* drop の先頭から limit 個までのパスを写して持つ。drop は呼んだ側のもの（DragFinish しない）。
 * 記憶が足りなければ偽で、out は変えない。 */
[[nodiscard]] bool dropped_files_create(HDROP _Nonnull drop, size_t limit,
                                        struct dropped_files *_Nullable *_Nonnull out);
/* 落とされた数から、パスにできなかった数（lost）を引いたもの。limit より大きいことがある:
 * その分のパスは持たない（application が「上限を超えた数」として数えるだけで、読まない）。 */
[[nodiscard]] size_t dropped_files_count(const struct dropped_files *_Nonnull files);
/* limit 個のパスがそろうまでに読んだ中で、パスにできなかった数（長すぎる・文字として正しくない・
 * 絶対パスでない）。 */
[[nodiscard]] size_t dropped_files_lost(const struct dropped_files *_Nonnull files);
/* 持っているパス。dropped_files_count と limit の小さいほうの個数だけ並ぶ。 */
[[nodiscard]] struct storage_path *_Nonnull const *_Nonnull dropped_files_paths(
    const struct dropped_files *_Nonnull files);
void dropped_files_destroy(struct dropped_files *_Nullable files);
#endif
