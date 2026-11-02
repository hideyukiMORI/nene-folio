#ifndef NENEFOLIO_IMAGE_CACHE_H
#define NENEFOLIO_IMAGE_CACHE_H
#include "image_bitmap.h"
#include "image_encoded.h"
#include "image_job.h"
struct image_cache;
[[nodiscard]] enum image_outcome image_cache_create(struct image_cache *_Nullable *_Nonnull out);
[[nodiscard]] enum image_outcome image_cache_get(const struct image_cache *_Nonnull cache,
                                                 const struct image_job *_Nonnull job,
                                                 const struct image_encoded *_Nullable encoded,
                                                 struct image_bitmap *_Nullable *_Nonnull out);
void image_cache_store(struct image_cache *_Nonnull cache, const struct image_job *_Nonnull job,
                       const struct image_encoded *_Nullable encoded,
                       const struct image_bitmap *_Nonnull bitmap);
void image_cache_destroy(struct image_cache *_Nullable cache);
#endif
