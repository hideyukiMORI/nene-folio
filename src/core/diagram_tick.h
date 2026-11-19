#ifndef NENEFOLIO_DIAGRAM_TICK_H
#define NENEFOLIO_DIAGRAM_TICK_H
#include <stddef.h>
/* #992: a mark on a gantt's time axis: the day after the chart's first day, and its label (a
 * model text, so it is measured with the others). */
struct diagram_tick
{
    size_t day;
    size_t label;
};
#endif
