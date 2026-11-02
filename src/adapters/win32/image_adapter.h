#ifndef NENEFOLIO_IMAGE_ADAPTER_H
#define NENEFOLIO_IMAGE_ADAPTER_H
#include "image_port.h"
struct storage_path;
struct image_adapter;
[[nodiscard]] enum image_outcome
image_adapter_create(const struct storage_path *_Nonnull root,
                     struct image_adapter *_Nullable *_Nonnull out);
[[nodiscard]] struct image_port image_adapter_port(struct image_adapter *_Nonnull adapter);
void image_adapter_destroy(struct image_adapter *_Nullable adapter);
#endif
