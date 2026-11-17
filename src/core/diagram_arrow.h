#ifndef NENEFOLIO_DIAGRAM_ARROW_H
#define NENEFOLIO_DIAGRAM_ARROW_H
enum diagram_arrow : unsigned char
{
    DIAGRAM_ARROW_NONE,
    DIAGRAM_ARROW_CLOSED,
    DIAGRAM_ARROW_OPEN,
    DIAGRAM_ARROW_CIRCLE,
    DIAGRAM_ARROW_CROSS,
    DIAGRAM_ARROW_HOLLOW,         /* #890: UML inheritance / realization */
    DIAGRAM_ARROW_DIAMOND,        /* #890: UML aggregation */
    DIAGRAM_ARROW_DIAMOND_FILLED, /* #890: UML composition */
    DIAGRAM_ARROW_ER_ONE,         /* #898: exactly one (two bars) */
    DIAGRAM_ARROW_ER_ZERO_ONE,    /* #898: zero or one (circle and bar) */
    DIAGRAM_ARROW_ER_MANY,        /* #898: one or more (bar and crow's foot) */
    DIAGRAM_ARROW_ER_ZERO_MANY    /* #898: zero or more (circle and crow's foot) */
};
#endif
