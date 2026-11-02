#include "image_cache.h"
#include <stdlib.h>
#include <string.h>
constexpr size_t capacity = 4;
constexpr size_t byte_limit = 32 * 1024 * 1024;
struct image_cache
{
    struct image_job keys[capacity];
    struct image_bitmap *_Nullable bitmaps[capacity];
    unsigned char *_Nullable encoded[capacity];
    size_t lengths[capacity];
    size_t next;
    size_t bytes;
};
enum image_outcome image_cache_create(struct image_cache *_Nullable *_Nonnull out)
{
    struct image_cache *_Nullable cache = calloc(1, sizeof *cache);
    if (cache == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    *out = cache;
    return IMAGE_READY;
}
enum image_outcome image_cache_get(const struct image_cache *_Nonnull cache,
                                   const struct image_job *_Nonnull job,
                                   const struct image_encoded *_Nullable encoded,
                                   struct image_bitmap *_Nullable *_Nonnull out)
{
    for (size_t index = 0; index < capacity; ++index)
    {
        const struct image_job *key = &cache->keys[index];
        if (key->source != nullptr && strcmp(key->category, job->category) == 0 &&
            strcmp(image_source_target(key->source), image_source_target(job->source)) == 0 &&
            (encoded == nullptr ||
             (cache->encoded[index] != nullptr && cache->lengths[index] == encoded->length &&
              memcmp(cache->encoded[index], encoded->bytes, encoded->length) == 0)))
            return image_bitmap_clone(cache->bitmaps[index], out);
    }
    return IMAGE_PENDING;
}
static void destroy_entry(struct image_cache *_Nonnull cache, size_t index)
{
    if (cache->bitmaps[index] != nullptr)
        cache->bytes -= image_bitmap_length(cache->bitmaps[index]);
    image_bitmap_destroy(cache->bitmaps[index]);
    cache->bitmaps[index] = nullptr;
    cache->bytes -= cache->lengths[index];
    free(cache->encoded[index]);
    cache->encoded[index] = nullptr;
    cache->lengths[index] = 0;
    image_job_destroy(&cache->keys[index]);
}
void image_cache_store(struct image_cache *_Nonnull cache, const struct image_job *_Nonnull job,
                       const struct image_encoded *_Nullable encoded,
                       const struct image_bitmap *_Nonnull bitmap)
{
    size_t encoded_length = encoded == nullptr ? 0 : encoded->length;
    size_t length = image_bitmap_length(bitmap) + encoded_length;
    if (length > byte_limit)
        return;
    while (cache->bytes + length > byte_limit || cache->bitmaps[cache->next] != nullptr)
    {
        destroy_entry(cache, cache->next);
        cache->next = (cache->next + 1) % capacity;
    }
    unsigned char *_Nullable bytes = nullptr;
    if (encoded != nullptr && encoded_length != 0)
    {
        bytes = malloc(encoded_length);
        if (bytes == nullptr)
            return;
        memcpy(bytes, encoded->bytes, encoded_length);
    }
    struct image_request request = {.category = job->category, .source = job->source};
    if (image_job_copy(&cache->keys[cache->next], &request) != IMAGE_READY)
    {
        free(bytes);
        return;
    }
    if (image_bitmap_clone(bitmap, &cache->bitmaps[cache->next]) != IMAGE_READY)
    {
        image_job_destroy(&cache->keys[cache->next]);
        free(bytes);
        return;
    }
    cache->bytes += length;
    cache->encoded[cache->next] = bytes;
    cache->lengths[cache->next] = encoded_length;
    cache->next = (cache->next + 1) % capacity;
}
void image_cache_destroy(struct image_cache *_Nullable cache)
{
    if (cache != nullptr)
        for (size_t index = 0; index < capacity; ++index)
            destroy_entry(cache, index);
    free(cache);
}
