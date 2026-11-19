#ifndef NENEFOLIO_MERMAID_GANTT_TASK_H
#define NENEFOLIO_MERMAID_GANTT_TASK_H
#include "diagram_text.h"
#include "mermaid_gantt_end.h"
#include "mermaid_gantt_start.h"
#include <stdint.h>
/* #992: one task line as it was read (the texts borrow the source), and its days once resolved.
 * Days are diagram_calendar day numbers; `finish` is the first day after the task. */
struct mermaid_gantt_task
{
    struct diagram_text name;
    struct diagram_text id;    /* empty when the line names none */
    struct diagram_text after; /* the ids after `after` */
    enum mermaid_gantt_start start;
    enum mermaid_gantt_end end;
    int32_t start_day;
    int32_t end_day;
    int32_t days;
    size_t section; /* diagram_none before the first section */
    bool done;
    bool active;
    bool critical;
    bool milestone;
    bool resolved;
    int32_t begin;
    int32_t drawn;  /* where the bar ends */
    int32_t finish; /* where a task after it begins: past excluded days that follow the bar */
};
#endif
