#ifndef NENEFOLIO_DIAGRAM_NODE_H
#define NENEFOLIO_DIAGRAM_NODE_H
#include "diagram_shape.h"
#include "diagram_task_state.h"
#include <stddef.h>
/* Immutable borrowed projection; the model owns IDs, labels and membership validation. */
struct diagram_node
{
    size_t id;
    size_t label;
    size_t group;
    enum diagram_shape shape;
    /* #955: a pie slice's sweep in hundredths of a degree; 0 for every other node. */
    size_t share;
    /* #992: a gantt task's first day after the chart's first day and its length in days (0 for
     * a milestone of no length), its state and its `crit` tag; 0 for every other node. */
    size_t start;
    size_t length;
    enum diagram_task_state state;
    bool critical;
};
#endif
