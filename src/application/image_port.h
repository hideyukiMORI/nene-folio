#ifndef NENEFOLIO_IMAGE_PORT_H
#define NENEFOLIO_IMAGE_PORT_H
#include "image_asset.h"
#include "image_export_policy.h"
#include "image_outcome.h"
#include "image_phase.h"
#include "image_request.h"
#include <stdint.h>
struct image_adapter;
struct image_bitmap;
struct image_port
{
    struct image_adapter *_Nonnull adapter;
    enum image_outcome (*_Nonnull request)(struct image_adapter *_Nonnull adapter,
                                           const struct image_request *_Nonnull request,
                                           uint64_t *_Nonnull ticket);
    enum image_outcome (*_Nonnull poll)(struct image_adapter *_Nonnull adapter, uint64_t ticket,
                                        struct image_bitmap *_Nullable *_Nonnull out);
    enum image_phase (*_Nonnull phase)(struct image_adapter *_Nonnull adapter);
    void (*_Nonnull cancel)(struct image_adapter *_Nonnull adapter);
    enum image_outcome (*_Nonnull asset)(struct image_adapter *_Nonnull adapter,
                                         const struct image_request *_Nonnull request,
                                         enum image_export_policy policy,
                                         struct image_asset *_Nullable *_Nonnull out);
};
#endif
