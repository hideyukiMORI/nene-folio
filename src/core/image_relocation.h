#ifndef NENEFOLIO_IMAGE_RELOCATION_H
#define NENEFOLIO_IMAGE_RELOCATION_H
#include "note_search_span.h"
struct image_relocation
{
    struct note_search_span source;
    const char *_Nonnull destination;
};
#endif
