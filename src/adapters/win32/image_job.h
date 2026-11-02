#ifndef NENEFOLIO_IMAGE_JOB_H
#define NENEFOLIO_IMAGE_JOB_H
#include "image_request.h"
#include "image_source.h"
#include <stdint.h>
struct image_job
{
    char category[256];
    struct image_source *_Nullable source;
    uint64_t ticket;
};
[[nodiscard]] enum image_outcome image_job_copy(struct image_job *_Nonnull job,
                                                const struct image_request *_Nonnull request);
void image_job_destroy(struct image_job *_Nonnull job);
#endif
