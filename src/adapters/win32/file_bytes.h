/* ファイル 1 つの中身をまとめて読んだ所有バッファと、ファイル 1 つの原子的な置き換え。
 * data/ の json を読み書きする唯一の経路（ARC-007 / ARC-009）。
 * 先頭の UTF-8 BOM は境界で取り除く（メモ帳が付けるため）。 */
#ifndef NENEFOLIO_FILE_BYTES_H
#define NENEFOLIO_FILE_BYTES_H

#include "persistence_outcome.h"

#include <stddef.h>

struct file_bytes;

/* 無ければ ABSENT、開けない・大きすぎる・読み切れないなら UNREADABLE。 */
[[nodiscard]] enum persistence_outcome file_bytes_read(const wchar_t *_Nonnull path,
                                                       struct file_bytes *_Nullable *_Nonnull out);
/* path を新しい中身で置き換える。隣の一時ファイルへ書いてから MoveFileExW で差し替えるので、
 * 途中で落ちても元のファイルは残る。書けなければ UNWRITABLE。 */
[[nodiscard]] enum persistence_outcome file_bytes_store(const wchar_t *_Nonnull path,
                                                        const char *_Nonnull data, size_t length);
[[nodiscard]] const char *_Nonnull file_bytes_data(const struct file_bytes *_Nonnull bytes);
[[nodiscard]] size_t file_bytes_length(const struct file_bytes *_Nonnull bytes);
void file_bytes_destroy(struct file_bytes *_Nullable bytes);

#endif
