#ifndef NENEFOLIO_DIAGRAM_KIND_H
#define NENEFOLIO_DIAGRAM_KIND_H
enum diagram_kind : unsigned char
{
    DIAGRAM_FLOWCHART,
    DIAGRAM_SEQUENCE,
    DIAGRAM_PIE,  /* #955: slices as nodes; each node's share is its sector's sweep */
    DIAGRAM_GANTT /* #992: tasks as nodes, sections as groups, the time axis as ticks */
};
#endif
