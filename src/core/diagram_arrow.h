#ifndef NENEFOLIO_DIAGRAM_ARROW_H
#define NENEFOLIO_DIAGRAM_ARROW_H
enum diagram_arrow : unsigned char
{
    DIAGRAM_ARROW_NONE,
    DIAGRAM_ARROW_CLOSED,
    DIAGRAM_ARROW_OPEN,
    DIAGRAM_ARROW_CIRCLE,
    DIAGRAM_ARROW_CROSS,
    DIAGRAM_ARROW_HOLLOW,        /* #890: UML inheritance / realization */
    DIAGRAM_ARROW_DIAMOND,       /* #890: UML aggregation */
    DIAGRAM_ARROW_DIAMOND_FILLED /* #890: UML composition */
};
#endif
