/* 新しく付けるカテゴリ名（ADR 0039 決定 10）。name_list の規則に次の 2 つを足したもの。
 *   (a) 先頭のバイトが "." でない
 *   (b) categories.json・settings.json と ASCII の大小を畳んで一致しない
 * ノート名と違い、末尾の ".md" は剥がさない（"議事.md" は "議事.md" のまま）。 */
#ifndef NENEFOLIO_CATEGORY_NAME_H
#define NENEFOLIO_CATEGORY_NAME_H
#include "category_name_outcome.h"
#include <stddef.h>
struct category_name;
[[nodiscard]] enum category_name_outcome
category_name_create(const char *_Nonnull text, size_t length,
                     struct category_name *_Nullable *_Nonnull out);
/* 終端付き。name が生きている間だけ有効。 */
[[nodiscard]] const char *_Nonnull category_name_text(const struct category_name *_Nonnull name);
void category_name_destroy(struct category_name *_Nullable name);
#endif
