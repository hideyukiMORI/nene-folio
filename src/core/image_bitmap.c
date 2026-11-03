#include "image_bitmap.h"
#include <stdlib.h>
#include <string.h>
struct image_bitmap
{
    unsigned char *_Nonnull bytes;
    size_t length;
    struct image_size size;
};
static constexpr size_t header_size = 138;
static void dword(unsigned char *_Nonnull target, size_t at, uint32_t value)
{
    for (size_t index = 0; index < 4; ++index)
        target[at + index] = (unsigned char)((value >> (index * 8)) & 255);
}
static void header(unsigned char *_Nonnull bytes, struct image_size size, size_t length)
{
    bytes[0] = 'B';
    bytes[1] = 'M';
    dword(bytes, 2, (uint32_t)length);
    dword(bytes, 10, (uint32_t)header_size);
    dword(bytes, 14, 124);
    dword(bytes, 18, size.width);
    dword(bytes, 22, UINT32_C(0) - size.height);
    bytes[26] = 1;
    bytes[28] = 32;
    dword(bytes, 30, 3); /* BI_BITFIELDS with explicit alpha in BITMAPV5HEADER. */
    dword(bytes, 34, (uint32_t)(length - header_size));
    dword(bytes, 54, UINT32_C(0x00FF0000));
    dword(bytes, 58, UINT32_C(0x0000FF00));
    dword(bytes, 62, UINT32_C(0x000000FF));
    dword(bytes, 66, UINT32_C(0xFF000000));
    dword(bytes, 70, UINT32_C(0x73524742)); /* LCS_sRGB. */
}
static void destroy_prepared(struct image_bitmap *_Nullable bitmap, unsigned char *_Nullable bytes)
{
    free(bitmap);
    free(bytes);
}
enum image_outcome image_bitmap_create(const unsigned char *_Nonnull pixels, struct image_size size,
                                       struct image_bitmap *_Nullable *_Nonnull out)
{
    if (size.width == 0 || size.height == 0 || size.width > image_bitmap_edge_limit ||
        size.height > image_bitmap_edge_limit)
        return IMAGE_TOO_LARGE;
    size_t length = header_size + (size_t)size.width * size.height * 4;
    if (length > image_bitmap_total_limit)
        return IMAGE_TOO_LARGE;
    struct image_bitmap *_Nullable bitmap = malloc(sizeof *bitmap);
    unsigned char *_Nullable bytes = calloc(length, 1);
    if (bitmap == nullptr || bytes == nullptr)
    {
        destroy_prepared(bitmap, bytes);
        return IMAGE_OUT_OF_MEMORY;
    }
    header(bytes, size, length);
    memcpy(bytes + header_size, pixels, length - header_size);
    *bitmap = (struct image_bitmap){.bytes = bytes, .length = length, .size = size};
    *out = bitmap;
    return IMAGE_READY;
}
enum image_outcome image_bitmap_clone(const struct image_bitmap *_Nonnull bitmap,
                                      struct image_bitmap *_Nullable *_Nonnull out)
{
    return image_bitmap_create(bitmap->bytes + header_size, bitmap->size, out);
}
const unsigned char *_Nonnull image_bitmap_bytes(const struct image_bitmap *_Nonnull bitmap)
{
    return bitmap->bytes;
}
size_t image_bitmap_length(const struct image_bitmap *_Nonnull bitmap)
{
    return bitmap->length;
}
struct image_size image_bitmap_size(const struct image_bitmap *_Nonnull bitmap)
{
    return bitmap->size;
}
void image_bitmap_destroy(struct image_bitmap *_Nullable bitmap)
{
    if (bitmap != nullptr)
        free(bitmap->bytes);
    free(bitmap);
}
