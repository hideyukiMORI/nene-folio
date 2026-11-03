#ifndef NENEFOLIO_DIAGRAM_LINK_H
#define NENEFOLIO_DIAGRAM_LINK_H
#include "diagram_arrow.h"
#include "diagram_stroke.h"
#include <stddef.h>
struct diagram_link
{
    size_t from;
    size_t to;
    size_t label;
    size_t length;
    enum diagram_stroke stroke;
    enum diagram_arrow start;
    enum diagram_arrow end;
};
#endif
