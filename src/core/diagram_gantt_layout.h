#ifndef NENEFOLIO_DIAGRAM_GANTT_LAYOUT_H
#define NENEFOLIO_DIAGRAM_GANTT_LAYOUT_H
#include "diagram_metrics.h"
#include "diagram_model.h"
#include "diagram_scene.h"
/* #992 / ADR0115: a gantt model as a scene: the title, the axis labels and grid lines, section
 * names on the left, one bar (or a milestone's diamond) per task row with its name to the right.
 * Sections take steps of the accent colour; a done task is pale with a muted edge. */
[[nodiscard]] enum diagram_outcome
diagram_gantt_layout_create(const struct diagram_model *_Nonnull model,
                            const struct diagram_metrics *_Nonnull metrics,
                            struct diagram_scene *_Nullable *_Nonnull out);
#endif
