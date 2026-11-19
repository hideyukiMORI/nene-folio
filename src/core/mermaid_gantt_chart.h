#ifndef NENEFOLIO_MERMAID_GANTT_CHART_H
#define NENEFOLIO_MERMAID_GANTT_CHART_H
#include "diagram_text.h"
#include "mermaid_gantt.h"
#include "mermaid_gantt_interval.h"
#include "mermaid_gantt_task.h"
#include <stdint.h>
/* #992: a gantt as it was read, shared by the reader, the schedule and the axis. */
struct mermaid_gantt_chart
{
    struct mermaid_gantt_task tasks[mermaid_gantt_task_limit];
    size_t count;
    struct diagram_text sections[mermaid_gantt_section_limit];
    size_t section_count;
    bool weekends;
    int32_t excluded[mermaid_gantt_excluded_limit];
    size_t excluded_count;
    struct diagram_text axis_format; /* empty for the default `M/D` */
    enum mermaid_gantt_interval interval;
    int32_t interval_count;
    int32_t origin; /* the first day of the chart */
    int32_t span;   /* days from the origin to the end of the last task, at least 1 */
};
#endif
