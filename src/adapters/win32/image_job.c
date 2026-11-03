#include "image_job.h"
#include "diagram_limits.h"
#include "note_text.h"
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
    job->kind = IMAGE_JOB_FILE;
    return IMAGE_READY;
}
enum image_outcome image_job_diagram(struct image_job *_Nonnull job,
                                     const struct diagram_request *_Nonnull request)
{
    size_t length = strlen(request->face);
    if (length == 0 || length >= sizeof job->face)
        return IMAGE_UNSUPPORTED;
    if (note_text_length(request->source) > diagram_source_limit)
        return IMAGE_TOO_LARGE;
    struct note_text *_Nullable source = nullptr;
    if (note_text_copy(request->source, &source) != NOTE_TEXT_ACCEPTED)
        return IMAGE_OUT_OF_MEMORY;
    image_job_destroy(job);
    job->kind = IMAGE_JOB_DIAGRAM;
    job->diagram = source;
    job->palette = request->palette;
    memcpy(job->face, request->face, length + 1);
    return IMAGE_READY;
}
enum image_outcome image_job_clone(struct image_job *_Nonnull job,
                                   const struct image_job *_Nonnull source)
{
    switch (source->kind)
    {
    case IMAGE_JOB_EMPTY:
        return IMAGE_CANCELLED;
    case IMAGE_JOB_FILE:
        return image_job_copy(job, &(struct image_request){source->category, source->source});
    case IMAGE_JOB_DIAGRAM:
        return image_job_diagram(
            job, &(struct diagram_request){source->diagram, source->palette, source->face});
    }
    return IMAGE_FAILED;
}
bool image_job_equal(const struct image_job *_Nonnull first,
                     const struct image_job *_Nonnull second)
{
    if (first->kind != second->kind)
        return false;
    switch (first->kind)
    {
    case IMAGE_JOB_EMPTY:
        return false;
    case IMAGE_JOB_FILE:
        return strcmp(first->category, second->category) == 0 &&
               image_source_kind(first->source) == image_source_kind(second->source) &&
               strcmp(image_source_target(first->source), image_source_target(second->source)) == 0;
    case IMAGE_JOB_DIAGRAM:
        return note_text_equals(first->diagram, second->diagram) &&
               strcmp(first->face, second->face) == 0 &&
               diagram_palette_equal(&first->palette, &second->palette);
    }
    return false;
}
size_t image_job_bytes(const struct image_job *_Nonnull job)
{
    switch (job->kind)
    {
    case IMAGE_JOB_EMPTY:
        return 0;
    case IMAGE_JOB_FILE:
        return strlen(image_source_target(job->source)) + 1;
    case IMAGE_JOB_DIAGRAM:
        return note_text_length(job->diagram) + 1;
    }
    return 0;
}
void image_job_destroy(struct image_job *_Nonnull job)
{
    image_source_destroy(job->source);
    note_text_destroy(job->diagram);
    job->source = nullptr;
    job->diagram = nullptr;
    job->kind = IMAGE_JOB_EMPTY;
}
