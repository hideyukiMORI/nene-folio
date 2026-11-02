#ifndef NENEFOLIO_IMAGE_DECODE_H
#define NENEFOLIO_IMAGE_DECODE_H
#include "image_bitmap.h"
#include "image_encoded.h"
#include "image_work.h"
[[nodiscard]] enum image_outcome image_decode_read(struct image_encoded encoded,
                                                   const struct image_work *_Nonnull work,
                                                   struct image_bitmap *_Nullable *_Nonnull out);
#endif
