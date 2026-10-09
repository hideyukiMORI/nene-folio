/* カテゴリ改名の対象と検証済み新名（ADR0041）。呼び出しの間だけ借りる。
 * 範囲の検証と改名準備の所有は folio_state が持ち、入力を保持しない。 */
#ifndef NENEFOLIO_CATEGORY_RENAME_TARGET_H
#define NENEFOLIO_CATEGORY_RENAME_TARGET_H

#include <stddef.h>

struct category_name;
struct category_rename_target
{
    size_t category;
    const struct category_name *_Nonnull name;
};

#endif
