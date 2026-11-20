#include "image_signature.h"
#include <string.h>

bool image_signature_supported(const unsigned char *_Nonnull bytes, size_t length)
{
    static const unsigned char png[] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
    static const unsigned char jpeg[] = {0xFF, 0xD8, 0xFF};
    if (length >= sizeof png && memcmp(bytes, png, sizeof png) == 0)
        return true;
    return length >= sizeof jpeg && memcmp(bytes, jpeg, sizeof jpeg) == 0;
}
