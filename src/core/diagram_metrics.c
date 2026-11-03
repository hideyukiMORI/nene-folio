#include "diagram_metrics.h"
#include "diagram_limits.h"
#include <stdlib.h>
#include <string.h>
struct diagram_metrics
{
    size_t count;
    struct diagram_label_metrics sizes[];
};
static bool valid_size(struct diagram_size size)
{
    return size.width >= 0 && size.height > 0 && size.width <= 4096 && size.height <= 4096;
}
enum diagram_outcome diagram_metrics_create(const struct diagram_label_metrics *_Nonnull sizes,
                                            size_t count,
                                            struct diagram_metrics *_Nullable *_Nonnull out)
{
    if (count > diagram_text_limit)
        return DIAGRAM_TOO_LARGE;
    for (size_t index = 0; index < count; ++index)
        if (!valid_size(sizes[index].body) || !valid_size(sizes[index].caption))
            return DIAGRAM_TOO_LARGE;
    struct diagram_metrics *_Nullable metrics = malloc(sizeof *metrics + count * sizeof *sizes);
    if (metrics == nullptr)
        return DIAGRAM_OUT_OF_MEMORY;
    metrics->count = count;
    memcpy(metrics->sizes, sizes, count * sizeof *sizes);
    *out = metrics;
    return DIAGRAM_ACCEPTED;
}
struct diagram_size diagram_metrics_at(const struct diagram_metrics *_Nonnull metrics, size_t index,
                                       enum diagram_text_role role)
{
    if (index >= metrics->count)
        return (struct diagram_size){0, 0};
    switch (role)
    {
    case DIAGRAM_TEXT_BODY:
        return metrics->sizes[index].body;
    case DIAGRAM_TEXT_CAPTION:
        return metrics->sizes[index].caption;
    }
    return (struct diagram_size){0, 0};
}
size_t diagram_metrics_count(const struct diagram_metrics *_Nonnull metrics)
{
    return metrics->count;
}
void diagram_metrics_destroy(struct diagram_metrics *_Nullable metrics)
{
    free(metrics);
}
