#ifndef NENEFOLIO_DIAGRAM_PRIMITIVE_H
#define NENEFOLIO_DIAGRAM_PRIMITIVE_H
#include "diagram_arrow.h"
#include "diagram_ink.h"
#include "diagram_point.h"
#include "diagram_primitive_kind.h"
#include "diagram_rect.h"
#include "diagram_shape.h"
#include "diagram_stroke.h"
#include "diagram_text_alignment.h"
#include "diagram_text_role.h"
#include <stddef.h>
/* #955: the steps a fill can take from its ink toward the background (pie slices). */
constexpr unsigned char diagram_shade_count = 5;
/* Read-only drawing values; text is an index into the immutable diagram model. */
struct diagram_primitive
{
    enum diagram_primitive_kind kind;
    enum diagram_shape shape;
    enum diagram_stroke stroke;
    enum diagram_arrow start;
    enum diagram_arrow end;
    enum diagram_ink ink;
    enum diagram_ink fill;
    /* #955: how far the fill is stepped from its ink toward the background; 0 is the ink. */
    unsigned char shade;
    enum diagram_text_role role;
    enum diagram_text_alignment alignment;
    int radius;
    int text_inset;
    struct diagram_rect bounds;
    struct diagram_point points[8];
    size_t count;
    size_t text;
};
#endif
