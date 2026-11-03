#ifndef NENEFOLIO_DIAGRAM_NODE_H
#define NENEFOLIO_DIAGRAM_NODE_H
#include "diagram_shape.h"
#include <stddef.h>
/* Immutable borrowed projection; the model owns IDs, labels and membership validation. */
struct diagram_node
{
    size_t id;
    size_t label;
    size_t group;
    enum diagram_shape shape;
};
#endif
