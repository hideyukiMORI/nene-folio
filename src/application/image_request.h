#ifndef NENEFOLIO_IMAGE_REQUEST_H
#define NENEFOLIO_IMAGE_REQUEST_H
struct image_source;
struct image_request
{
    const char *_Nonnull category;
    const struct image_source *_Nonnull source;
};
#endif
