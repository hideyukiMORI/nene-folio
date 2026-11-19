#ifndef NENEFOLIO_MERMAID_GANTT_H
#define NENEFOLIO_MERMAID_GANTT_H
#include "diagram_model.h"
#include "diagram_text.h"
#include <stdint.h>
/* #992 / ADR0115: a Mermaid `gantt` as a model. Each task is a node (label = its name, start and
 * length in days from the chart's first day, state, crit); each section is a group; the time
 * axis is a list of ticks with their labels. Nothing reads the clock: a task that Mermaid would
 * place at "today" is UNSUPPORTED. */
constexpr size_t mermaid_gantt_task_limit = 128;
constexpr size_t mermaid_gantt_section_limit = 64;
constexpr size_t mermaid_gantt_excluded_limit = 64;
constexpr int32_t mermaid_gantt_span_limit = 3660; /* about ten years */
[[nodiscard]] enum diagram_outcome
mermaid_gantt_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
