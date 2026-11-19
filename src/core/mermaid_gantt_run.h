#ifndef NENEFOLIO_MERMAID_GANTT_RUN_H
#define NENEFOLIO_MERMAID_GANTT_RUN_H
#include <stdint.h>
/* #992: a run of marks on a gantt's axis: every `step` days from `first`, or the first day of
 * every `months`th month (then `step` is 0). */
struct mermaid_gantt_run
{
    int32_t first;
    int32_t step;
    int32_t months;
};
#endif
