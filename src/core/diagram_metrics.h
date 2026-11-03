#ifndef NENEFOLIO_DIAGRAM_METRICS_H
#define NENEFOLIO_DIAGRAM_METRICS_H
#include "diagram_label_metrics.h"
#include "diagram_outcome.h"
#include "diagram_text_role.h"
#include <stddef.h>
struct diagram_metrics;
[[nodiscard]] enum diagram_outcome
diagram_metrics_create(const struct diagram_label_metrics *_Nonnull sizes, size_t count,
                       struct diagram_metrics *_Nullable *_Nonnull out);
[[nodiscard]] struct diagram_size diagram_metrics_at(const struct diagram_metrics *_Nonnull metrics,
                                                     size_t index, enum diagram_text_role role);
[[nodiscard]] size_t diagram_metrics_count(const struct diagram_metrics *_Nonnull metrics);
void diagram_metrics_destroy(struct diagram_metrics *_Nullable metrics);
#endif
