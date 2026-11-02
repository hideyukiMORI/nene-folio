#include "image_job.h"
#include <string.h>
enum image_outcome image_job_copy(struct image_job *_Nonnull job,
                                  const struct image_request *_Nonnull request)
{
    size_t length = strlen(request->category);
    if (length >= sizeof job->category)
        return IMAGE_UNSUPPORTED;
    struct image_source *_Nullable source = nullptr;
    enum image_outcome result = image_source_copy(request->source, &source);
    if (result != IMAGE_READY)
        return result;
    image_job_destroy(job);
    memcpy(job->category, request->category, length + 1);
    job->source = source;
    return IMAGE_READY;
}
void image_job_destroy(struct image_job *_Nonnull job)
{
    image_source_destroy(job->source);
    job->source = nullptr;
}
