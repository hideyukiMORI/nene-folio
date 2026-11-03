#ifndef NENEFOLIO_DIAGRAM_GROUP_H
#define NENEFOLIO_DIAGRAM_GROUP_H
#include "diagram_direction.h"
#include <stddef.h>
struct diagram_group
{
    size_t id;
    size_t label;
    size_t parent;
    enum diagram_direction direction;
};
#endif
