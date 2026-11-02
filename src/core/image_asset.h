#ifndef NENEFOLIO_IMAGE_ASSET_H
#define NENEFOLIO_IMAGE_ASSET_H
#include "image_asset_input.h"
#include "image_outcome.h"
struct image_asset;
[[nodiscard]] enum image_outcome image_asset_create(const struct image_asset_input *_Nonnull input,
                                                    struct image_asset *_Nullable *_Nonnull out);
[[nodiscard]] const struct image_asset_input *_Nonnull image_asset_content(
    const struct image_asset *_Nonnull asset);
void image_asset_destroy(struct image_asset *_Nullable asset);
#endif
