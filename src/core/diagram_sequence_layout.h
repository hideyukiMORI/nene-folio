#ifndef NENEFOLIO_DIAGRAM_SEQUENCE_LAYOUT_H
#define NENEFOLIO_DIAGRAM_SEQUENCE_LAYOUT_H
#include "diagram_metrics.h"
#include "diagram_model.h"
#include "diagram_scene.h"
[[nodiscard]] enum diagram_outcome
diagram_sequence_layout_create(const struct diagram_model *_Nonnull model,
                               const struct diagram_metrics *_Nonnull metrics,
                               struct diagram_scene *_Nullable *_Nonnull out);
#endif
