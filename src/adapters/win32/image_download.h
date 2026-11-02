#ifndef NENEFOLIO_IMAGE_DOWNLOAD_H
#define NENEFOLIO_IMAGE_DOWNLOAD_H
#include "image_encoded.h"
#include "image_outcome.h"
#include "image_work.h"
struct image_download;
[[nodiscard]] enum image_outcome
image_download_read(const char *_Nonnull url, const struct image_work *_Nonnull work,
                    struct image_download *_Nullable *_Nonnull out);
[[nodiscard]] struct image_encoded
image_download_bytes(const struct image_download *_Nonnull download);
void image_download_destroy(struct image_download *_Nullable download);
#endif
