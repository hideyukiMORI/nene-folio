#ifndef NENEFOLIO_STORAGE_NATIVE_PATH_H
#define NENEFOLIO_STORAGE_NATIVE_PATH_H
#include "storage_outcome.h"
#include <stddef.h>
struct storage_path;
/* 既存persistenceのUTF-16容量を共有する。終端と長いパスprefixを含む。 */
constexpr size_t storage_native_capacity = 1024;
[[nodiscard]] bool storage_native_path_fill(const struct storage_path *_Nonnull path,
                                            wchar_t *_Nonnull out, size_t *_Nonnull length);
[[nodiscard]] enum storage_outcome
storage_native_path_from_units(const wchar_t *_Nonnull units,
                               struct storage_path *_Nullable *_Nonnull out);
#endif
