#ifndef NENEFOLIO_IMAGE_BITMAP_H
#define NENEFOLIO_IMAGE_BITMAP_H
#include "image_outcome.h"
#include "image_size.h"
#include <stddef.h>
struct image_bitmap;
constexpr uint32_t image_bitmap_edge_limit = 2048;
constexpr size_t image_bitmap_total_limit = 64 * 1024 * 1024;
/* BGRA pixels, tightly packed and top-down, produced by the platform decoder. */
[[nodiscard]] enum image_outcome image_bitmap_create(const unsigned char *_Nonnull pixels,
                                                     struct image_size size,
                                                     struct image_bitmap *_Nullable *_Nonnull out);
[[nodiscard]] enum image_outcome image_bitmap_clone(const struct image_bitmap *_Nonnull bitmap,
                                                    struct image_bitmap *_Nullable *_Nonnull out);
[[nodiscard]] const unsigned char *_Nonnull image_bitmap_bytes(
    const struct image_bitmap *_Nonnull bitmap);
[[nodiscard]] size_t image_bitmap_length(const struct image_bitmap *_Nonnull bitmap);
[[nodiscard]] struct image_size image_bitmap_size(const struct image_bitmap *_Nonnull bitmap);
void image_bitmap_destroy(struct image_bitmap *_Nullable bitmap);
#endif
