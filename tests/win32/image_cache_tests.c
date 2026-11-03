/* #544: cache capacity, byte accounting and ownership without network or windows. */
#include "folio_theme.h"
#include "image_cache.h"
#include "note_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static void require(bool value, const char *_Nonnull description)
{
    ++checks;
    if (!value)
    {
        fprintf(stderr, "FAIL: %s\n", description);
        exit(EXIT_FAILURE);
    }
}
static struct image_cache *_Nonnull new_cache(void)
{
    struct image_cache *_Nullable cache = nullptr;
    require(image_cache_create(&cache) == IMAGE_READY, "cache created");
    return cache;
}
static struct image_bitmap *_Nonnull bitmap_for(struct image_size size)
{
    size_t length = (size_t)size.width * size.height * 4;
    unsigned char *_Nullable pixels = malloc(length);
    require(pixels != nullptr, "fixture pixels allocated");
    memset(pixels, 0x37, length);
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(image_bitmap_create(pixels, size, &bitmap) == IMAGE_READY, "bounded bitmap created");
    free(pixels);
    return bitmap;
}
static struct image_job file_key(const char *_Nonnull target)
{
    struct image_source *_Nullable source = nullptr;
    require(image_source_create(target, (struct note_search_span){0, 1}, &source) == IMAGE_READY,
            "file key source");
    struct image_job job = {0};
    struct image_request request = {.category = "Fixture", .source = source};
    require(image_job_copy(&job, &request) == IMAGE_READY, "owned file key");
    image_source_destroy(source);
    return job;
}
static struct image_job key_for(size_t index)
{
    char source[80];
    int length = snprintf(source, sizeof source, "https://example.invalid/%zu.png", index);
    require(length > 0 && (size_t)length < sizeof source, "fixture key fits");
    if (index % 2 != 0)
        return file_key(source);
    length = snprintf(source, sizeof source, "flowchart TB\nNode%zu", index);
    require(length > 0 && (size_t)length < sizeof source, "diagram key fits");
    struct note_text *_Nullable text = nullptr;
    require(note_text_create(source, (size_t)length, &text) == NOTE_TEXT_ACCEPTED,
            "diagram key source");
    struct diagram_request request = {
        .source = text,
        .palette = diagram_palette_for(folio_theme_palette(FOLIO_THEME_LIGHT)),
        .face = "Noto Sans JP"};
    struct image_job job = {0};
    require(image_job_diagram(&job, &request) == IMAGE_READY, "owned diagram key");
    note_text_destroy(text);
    return job;
}
static void store_key(struct image_cache *_Nonnull cache, size_t index,
                      const struct image_bitmap *_Nonnull bitmap)
{
    struct image_job job = key_for(index);
    image_cache_store(cache, &job, nullptr, bitmap);
    image_job_destroy(&job);
}
static void expect_key(const struct image_cache *_Nonnull cache, size_t index,
                       enum image_outcome expected)
{
    struct image_job job = key_for(index);
    struct image_bitmap *_Nullable copy = nullptr;
    enum image_outcome actual = image_cache_get(cache, &job, nullptr, &copy);
    if (actual != expected)
        fprintf(stderr, "key %zu expected %u, actual %u\n", index, (unsigned)expected,
                (unsigned)actual);
    require(actual == expected && (copy != nullptr) == (expected == IMAGE_READY),
            "only entries required by capacity or byte pressure are evicted");
    image_bitmap_destroy(copy);
    image_job_destroy(&job);
}
static void request_trace(void)
{
    static const size_t requests[] = {0, 1, 2, 3, 4, 1, 2, 3, 4};
    struct image_cache *cache = new_cache();
    struct image_bitmap *bitmap = bitmap_for((struct image_size){2, 2});
    size_t hits = 0, misses = 0;
    for (size_t index = 0; index < sizeof requests / sizeof requests[0]; ++index)
    {
        struct image_job job = key_for(requests[index]);
        struct image_bitmap *_Nullable copy = nullptr;
        enum image_outcome outcome = image_cache_get(cache, &job, nullptr, &copy);
        require(outcome == IMAGE_READY || outcome == IMAGE_PENDING, "trace lookup result");
        if (outcome == IMAGE_READY)
            ++hits;
        if (outcome == IMAGE_PENDING)
        {
            ++misses;
            image_cache_store(cache, &job, nullptr, bitmap);
        }
        image_bitmap_destroy(copy);
        image_job_destroy(&job);
    }
    printf("cache request trace: 9 requests, %zu hits, %zu misses\n", hits, misses);
    image_bitmap_destroy(bitmap);
    image_cache_destroy(cache);
    require(hits == 4 && misses == 5, "the four most recent small images avoid regeneration");
}
static void capacity_eviction(void)
{
    struct image_cache *cache = new_cache();
    struct image_bitmap *bitmap = bitmap_for((struct image_size){2, 2});
    for (size_t index = 0; index < 5; ++index)
        store_key(cache, index, bitmap);
    expect_key(cache, 0, IMAGE_PENDING);
    for (size_t index = 1; index < 5; ++index)
        expect_key(cache, index, IMAGE_READY);
    store_key(cache, 5, bitmap);
    expect_key(cache, 1, IMAGE_PENDING);
    for (size_t index = 2; index < 6; ++index)
        expect_key(cache, index, IMAGE_READY);
    image_bitmap_destroy(bitmap);
    image_cache_destroy(cache);
}
static void byte_eviction_and_holes(void)
{
    struct image_cache *cache = new_cache();
    struct image_bitmap *medium = bitmap_for((struct image_size){1024, 1536});
    for (size_t index = 0; index < 4; ++index)
        store_key(cache, index, medium);
    image_bitmap_destroy(medium);
    struct image_bitmap *large = bitmap_for((struct image_size){2048, 2048});
    store_key(cache, 4, large);
    image_bitmap_destroy(large);
    expect_key(cache, 0, IMAGE_PENDING);
    expect_key(cache, 1, IMAGE_PENDING);
    for (size_t index = 2; index < 5; ++index)
        expect_key(cache, index, IMAGE_READY);
    struct image_bitmap *small = bitmap_for((struct image_size){1, 1});
    store_key(cache, 5, small);
    image_bitmap_destroy(small);
    for (size_t index = 2; index < 6; ++index)
        expect_key(cache, index, IMAGE_READY);
    image_cache_destroy(cache);
}
static void exact_byte_limit(void)
{
    struct image_cache *cache = new_cache();
    struct image_bitmap *bitmap = bitmap_for((struct image_size){2048, 1024});
    struct image_job first = key_for(1), second = key_for(3);
    size_t length = 16 * 1024 * 1024 - image_bitmap_length(bitmap) - image_job_bytes(&first);
    unsigned char *_Nullable bytes = calloc(length, 1);
    require(bytes != nullptr && image_job_bytes(&first) == image_job_bytes(&second),
            "equal sized keys and encoded fixtures");
    struct image_encoded encoded = {.bytes = bytes, .length = length};
    image_cache_store(cache, &first, &encoded, bitmap);
    image_cache_store(cache, &second, &encoded, bitmap);
    expect_key(cache, 1, IMAGE_READY);
    expect_key(cache, 3, IMAGE_READY);
    struct image_bitmap *small = bitmap_for((struct image_size){1, 1});
    store_key(cache, 5, small);
    expect_key(cache, 1, IMAGE_PENDING);
    expect_key(cache, 3, IMAGE_READY);
    expect_key(cache, 5, IMAGE_READY);
    image_bitmap_destroy(small);
    image_job_destroy(&first);
    image_job_destroy(&second);
    image_bitmap_destroy(bitmap);
    free(bytes);
    image_cache_destroy(cache);
}
static void oversized_entry(void)
{
    struct image_cache *cache = new_cache();
    struct image_bitmap *small = bitmap_for((struct image_size){1, 1});
    for (size_t index = 0; index < 4; ++index)
        store_key(cache, index, small);
    image_bitmap_destroy(small);
    struct image_bitmap *large = bitmap_for((struct image_size){2048, 4096});
    store_key(cache, 4, large);
    image_bitmap_destroy(large);
    expect_key(cache, 4, IMAGE_PENDING);
    for (size_t index = 0; index < 4; ++index)
        expect_key(cache, index, IMAGE_READY);
    image_cache_destroy(cache);
}
static void ownership_and_encoded_match(void)
{
    struct image_cache *cache = new_cache();
    struct image_bitmap *bitmap = bitmap_for((struct image_size){2, 1});
    struct image_job key = file_key("images/fixture.png");
    unsigned char bytes[] = {1, 2, 3};
    struct image_encoded encoded = {.bytes = bytes, .length = sizeof bytes};
    image_cache_store(cache, &key, &encoded, bitmap);
    image_bitmap_destroy(bitmap);
    struct image_bitmap *_Nullable copy = nullptr;
    bytes[0] = 9;
    require(image_cache_get(cache, &key, &encoded, &copy) == IMAGE_PENDING && copy == nullptr,
            "stored encoded bytes are owned and a changed local original misses");
    bytes[0] = 1;
    encoded.length = 2;
    require(image_cache_get(cache, &key, &encoded, &copy) == IMAGE_PENDING && copy == nullptr,
            "changed original length misses");
    encoded.length = sizeof bytes;
    require(image_cache_get(cache, &key, &encoded, &copy) == IMAGE_READY,
            "matching original returns an independently owned bitmap");
    image_cache_destroy(cache);
    image_job_destroy(&key);
    struct image_size size = image_bitmap_size(copy);
    size_t length = image_bitmap_length(copy);
    require(size.width == 2 && size.height == 1 && image_bitmap_bytes(copy)[length - 1] == 0x37,
            "returned bitmap survives cache and caller source destruction");
    image_bitmap_destroy(copy);
}
int main(void)
{
    request_trace();
    capacity_eviction();
    byte_eviction_and_holes();
    exact_byte_limit();
    oversized_entry();
    ownership_and_encoded_match();
    printf("image cache: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
