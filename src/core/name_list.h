/* カテゴリ名・ノート名の順序付き集合。名前の規則はここで 1 度だけ検証する（ARC-008 / C-007）。
 * 規則: 1〜name_list_max_length バイトの妥当な UTF-8、制御文字と \ / : * ? " < > | を含まず、
 * "." と ".." ではなく、末尾が空白や "." ではない（Windows のファイル名として成立する範囲）。
 * Windows予約デバイス名（拡張子付きとCOM/LPTの上付き数字も含む）を拒否する。
 * 同じ名前は 2 度入らない。 */
#ifndef NENEFOLIO_NAME_LIST_H
#define NENEFOLIO_NAME_LIST_H

#include "name_list_outcome.h"

#include <stddef.h>

struct name_list;

constexpr size_t name_list_max_length = 255;

[[nodiscard]] enum name_list_outcome name_list_create(struct name_list *_Nullable *_Nonnull out);
[[nodiscard]] enum name_list_outcome name_list_append(struct name_list *_Nonnull list,
                                                      const char *_Nonnull text, size_t length);
[[nodiscard]] size_t name_list_count(const struct name_list *_Nonnull list);
/* 終端付き。list が生きている間だけ有効。index は count 未満であること。 */
[[nodiscard]] const char *_Nonnull name_list_at(const struct name_list *_Nonnull list,
                                                size_t index);
[[nodiscard]] bool name_list_contains(const struct name_list *_Nonnull list,
                                      const char *_Nonnull text);
void name_list_destroy(struct name_list *_Nullable list);

#endif
