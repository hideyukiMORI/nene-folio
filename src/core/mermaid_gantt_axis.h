#ifndef NENEFOLIO_MERMAID_GANTT_AXIS_H
#define NENEFOLIO_MERMAID_GANTT_AXIS_H
#include "diagram_model.h"
#include "diagram_outcome.h"
#include "mermaid_gantt_chart.h"
/* #992: the marks of the time axis and their labels, into the model as ticks. Weeks start on
 * Monday. The default label is `M/D`, with the year in front of the first mark and of a mark in
 * a new year; `axisFormat` takes %Y %y %m %d %e %-m %-d and plain characters (`%%` starts a
 * Mermaid comment, so it never arrives here). */
[[nodiscard]] enum diagram_outcome
mermaid_gantt_axis_build(const struct mermaid_gantt_chart *_Nonnull chart,
                         struct diagram_model *_Nonnull model);
#endif
