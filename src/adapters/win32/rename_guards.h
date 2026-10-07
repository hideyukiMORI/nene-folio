/* 改名のあいだ開いたまま持つ親のハンドル（ADR 0022 の決定 4）。
 * NOTE は data / カテゴリ / .history / 履歴カテゴリの 4 つが上限。
 * CATEGORY は data / 存在する .history だけ。移動する対象自身を親として保持しない。 */
#ifndef NENEFOLIO_RENAME_GUARDS_H
#define NENEFOLIO_RENAME_GUARDS_H

#include <stddef.h>
#include <windows.h>

constexpr size_t rename_guard_capacity = 4;

struct rename_guards
{
    HANDLE items[rename_guard_capacity];
    size_t count;
};

#endif
