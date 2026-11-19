#ifndef NENEFOLIO_DIAGRAM_TASK_STATE_H
#define NENEFOLIO_DIAGRAM_TASK_STATE_H
/* #992: how far a gantt task has come (Mermaid's `done` and `active` tags). */
enum diagram_task_state : unsigned char
{
    DIAGRAM_TASK_PLANNED,
    DIAGRAM_TASK_ACTIVE,
    DIAGRAM_TASK_DONE
};
#endif
