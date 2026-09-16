/* 改名が触る 4 つの道（ADR 0022 の決定 4）。組み立てるだけで、存在は問わない。
 * 長さは persistence_adapter の道と同じ上限に揃える。 */
#ifndef NENEFOLIO_RENAME_PATHS_H
#define NENEFOLIO_RENAME_PATHS_H

#include <wchar.h>

constexpr size_t rename_path_capacity = 1024;

struct rename_paths
{
    wchar_t note_from[rename_path_capacity];
    wchar_t note_to[rename_path_capacity];
    wchar_t history_from[rename_path_capacity];
    wchar_t history_to[rename_path_capacity];
};

#endif
