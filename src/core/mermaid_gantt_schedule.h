#ifndef NENEFOLIO_MERMAID_GANTT_SCHEDULE_H
#define NENEFOLIO_MERMAID_GANTT_SCHEDULE_H
#include "diagram_outcome.h"
#include "mermaid_gantt_chart.h"
/* #992: every task's first and last day, the chart's origin and span. `after` may name a task
 * further down; a cycle is INVALID; an unknown id and a first task with no start are UNSUPPORTED
 * (Mermaid would use today). Durations skip excluded days; written end dates do not move. */
[[nodiscard]] enum diagram_outcome
mermaid_gantt_schedule_resolve(struct mermaid_gantt_chart *_Nonnull chart);
#endif
