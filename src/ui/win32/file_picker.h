#ifndef NENEFOLIO_FILE_PICKER_H
#define NENEFOLIO_FILE_PICKER_H
#include "file_picker_outcome.h"
#include "file_picker_request.h"
#include <stddef.h>
#include <windows.h>
struct file_picker;
struct storage_path;
[[nodiscard]] enum file_picker_outcome
file_picker_open(HWND _Nonnull owner, const struct file_picker_request *_Nonnull request,
                 struct file_picker *_Nullable *_Nonnull out);
[[nodiscard]] enum file_picker_outcome
file_picker_save(HWND _Nonnull owner, const struct file_picker_request *_Nonnull request,
                 struct storage_path *_Nullable *_Nonnull out);
[[nodiscard]] size_t file_picker_count(const struct file_picker *_Nonnull picker);
[[nodiscard]] struct storage_path *_Nonnull const *_Nonnull file_picker_paths(
    const struct file_picker *_Nonnull picker);
void file_picker_destroy(struct file_picker *_Nullable picker);
#endif
