#ifndef NENEFOLIO_IMAGE_FILE_H
#define NENEFOLIO_IMAGE_FILE_H
#include "image_export_policy.h"
#include "image_outcome.h"
#include "image_request.h"
#include <windows.h>
struct image_file;
/* Reads only local disk files. Final target containment determines automatic ZIP inclusion. */
[[nodiscard]] enum image_outcome image_file_read(const wchar_t *_Nonnull root,
                                                 const struct image_request *_Nonnull request,
                                                 enum image_export_policy policy,
                                                 struct image_file *_Nullable *_Nonnull out);
[[nodiscard]] const unsigned char *_Nonnull image_file_bytes(
    const struct image_file *_Nonnull file);
[[nodiscard]] size_t image_file_length(const struct image_file *_Nonnull file);
[[nodiscard]] bool image_file_contained(const struct image_file *_Nonnull file);
[[nodiscard]] const char *_Nonnull image_file_identity(const struct image_file *_Nonnull file);
[[nodiscard]] const char *_Nonnull image_file_relative(const struct image_file *_Nonnull file);
void image_file_destroy(struct image_file *_Nullable file);
#endif
