#ifndef NENEFOLIO_IMAGE_ASSET_INPUT_H
#define NENEFOLIO_IMAGE_ASSET_INPUT_H
#include <stddef.h>
struct image_asset_input
{
    const unsigned char *_Nonnull bytes;
    size_t length;
    const char *_Nonnull identity;
    /* Empty means outside the note's images/ directory. Otherwise images/... . */
    const char *_Nonnull relative;
};
#endif
