/* 改名のあいだ開いたまま持つ親のハンドル（ADR 0022 の決定 4）。
 * data / data\<カテゴリ> / data\.history / data\.history\<カテゴリ> の 4 つが上限。 */
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
