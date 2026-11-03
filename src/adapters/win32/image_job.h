#ifndef NENEFOLIO_IMAGE_JOB_H
#define NENEFOLIO_IMAGE_JOB_H
#include "diagram_request.h"
#include "image_job_kind.h"
#include "image_request.h"
#include "image_source.h"
#include <stdint.h>
struct image_job
{
    enum image_job_kind kind;
    char category[256];
    struct image_source *_Nullable source;
    struct note_text *_Nullable diagram;
    struct diagram_palette palette;
    char face[64];
    uint64_t ticket;
};
[[nodiscard]] enum image_outcome image_job_copy(struct image_job *_Nonnull job,
                                                const struct image_request *_Nonnull request);
[[nodiscard]] enum image_outcome image_job_diagram(struct image_job *_Nonnull job,
                                                   const struct diagram_request *_Nonnull request);
[[nodiscard]] enum image_outcome image_job_clone(struct image_job *_Nonnull job,
                                                 const struct image_job *_Nonnull source);
[[nodiscard]] bool image_job_equal(const struct image_job *_Nonnull first,
                                   const struct image_job *_Nonnull second);
[[nodiscard]] size_t image_job_bytes(const struct image_job *_Nonnull job);
void image_job_destroy(struct image_job *_Nonnull job);
#endif
