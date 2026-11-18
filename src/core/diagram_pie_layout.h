#ifndef NENEFOLIO_DIAGRAM_PIE_LAYOUT_H
#define NENEFOLIO_DIAGRAM_PIE_LAYOUT_H
#include "diagram_metrics.h"
#include "diagram_model.h"
#include "diagram_scene.h"
/* #955 / ADR0112: a pie model as a scene: the title, one sector per slice clockwise from the
 * top, and a legend (a swatch and the slice's line) to the right of the circle. */
[[nodiscard]] enum diagram_outcome
diagram_pie_layout_create(const struct diagram_model *_Nonnull model,
                          const struct diagram_metrics *_Nonnull metrics,
                          struct diagram_scene *_Nullable *_Nonnull out);
#endif
