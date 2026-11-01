/* 保存資源の絶対DOS/UNCパス。rootと外部ファイルで同じ形/UTF-8検証を使う（ADR0077/0082）。
 * 環境・存在・ファイル種別は読まない。 */
#ifndef NENEFOLIO_STORAGE_PATH_H
#define NENEFOLIO_STORAGE_PATH_H
#include "storage_path_outcome.h"
#include <stddef.h>
struct storage_path;
constexpr size_t storage_path_byte_limit = 4092;
[[nodiscard]] enum storage_path_outcome
storage_path_create(const char *_Nonnull text, size_t length,
                    struct storage_path *_Nullable *_Nonnull out);
[[nodiscard]] enum storage_path_outcome
storage_path_copy(const struct storage_path *_Nonnull path,
                  struct storage_path *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull storage_path_text(const struct storage_path *_Nonnull path);
[[nodiscard]] size_t storage_path_length(const struct storage_path *_Nonnull path);
[[nodiscard]] bool storage_path_equal(const struct storage_path *_Nonnull left,
                                      const struct storage_path *_Nonnull right);
void storage_path_destroy(struct storage_path *_Nullable path);
#endif
