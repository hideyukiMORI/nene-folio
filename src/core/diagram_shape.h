#ifndef NENEFOLIO_DIAGRAM_SHAPE_H
#define NENEFOLIO_DIAGRAM_SHAPE_H
enum diagram_shape : unsigned char
{
    DIAGRAM_RECTANGLE,
    DIAGRAM_ROUNDED,
    DIAGRAM_STADIUM,
    DIAGRAM_SUBROUTINE,
    DIAGRAM_CYLINDER,
    DIAGRAM_CIRCLE,
    DIAGRAM_DOUBLE_CIRCLE,
    DIAGRAM_DIAMOND,
    DIAGRAM_HEXAGON,
    DIAGRAM_PARALLELOGRAM,
    DIAGRAM_PARALLELOGRAM_REVERSE,
    DIAGRAM_TRAPEZOID,
    DIAGRAM_TRAPEZOID_REVERSE,
    DIAGRAM_ASYMMETRIC,
    DIAGRAM_ACTOR,
    DIAGRAM_STATE_START, /* #876: a small dot filled with the line ink */
    DIAGRAM_STATE_END,   /* #876: a small ring around the same dot */
    DIAGRAM_ENTITY,      /* #888: a box with its name (the id) over its attributes (the label) */
    /* #955: a pie's sector. The bounds are the whole circle; points[0] holds the start and the
     * sweep, clockwise from the top, in hundredths of a degree. */
    DIAGRAM_SECTOR
};
#endif
