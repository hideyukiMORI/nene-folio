#include "image_asset.h"
#include "image_limits.h"
#include <stdlib.h>
#include <string.h>
struct image_asset
{
    struct image_asset_input content;
    unsigned char *_Nonnull bytes;
    char *_Nonnull identity;
    char *_Nonnull relative;
};
static void destroy_prepared(struct image_asset *_Nullable asset, unsigned char *_Nullable bytes,
                             char *_Nullable identity, char *_Nullable relative)
{
    free(asset);
    free(bytes);
    free(identity);
    free(relative);
}
enum image_outcome image_asset_create(const struct image_asset_input *_Nonnull input,
                                      struct image_asset *_Nullable *_Nonnull out)
{
    if (input->length == 0 || input->length > image_input_byte_limit)
        return IMAGE_TOO_LARGE;
    struct image_asset *_Nullable asset = malloc(sizeof *asset);
    unsigned char *_Nullable bytes = malloc(input->length);
    char *_Nullable identity = malloc(strlen(input->identity) + 1);
    char *_Nullable relative = malloc(strlen(input->relative) + 1);
    if (asset == nullptr || bytes == nullptr || identity == nullptr || relative == nullptr)
    {
        destroy_prepared(asset, bytes, identity, relative);
        return IMAGE_OUT_OF_MEMORY;
    }
    memcpy(bytes, input->bytes, input->length);
    memcpy(identity, input->identity, strlen(input->identity) + 1);
    memcpy(relative, input->relative, strlen(input->relative) + 1);
    *asset = (struct image_asset){.content = {.bytes = bytes,
                                              .length = input->length,
                                              .identity = identity,
                                              .relative = relative},
                                  .bytes = bytes,
                                  .identity = identity,
                                  .relative = relative};
    *out = asset;
    return IMAGE_READY;
}
const struct image_asset_input *_Nonnull image_asset_content(
    const struct image_asset *_Nonnull asset)
{
    return &asset->content;
}
void image_asset_destroy(struct image_asset *_Nullable asset)
{
    if (asset != nullptr)
    {
        free(asset->bytes);
        free(asset->identity);
        free(asset->relative);
    }
    free(asset);
}
