#ifndef NENEFOLIO_IMAGE_ENCODED_H
#define NENEFOLIO_IMAGE_ENCODED_H
#include "image_limits.h"
#include <stddef.h>
struct image_encoded
{
    const unsigned char *_Nonnull bytes;
    size_t length;
};
#endif
