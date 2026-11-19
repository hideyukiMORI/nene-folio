#ifndef NENEFOLIO_MERMAID_GANTT_START_H
#define NENEFOLIO_MERMAID_GANTT_START_H
/* #992: where a gantt task begins, as written. */
enum mermaid_gantt_start : unsigned char
{
    MERMAID_GANTT_START_PREVIOUS, /* no start: where the task above ends */
    MERMAID_GANTT_START_DATE,     /* YYYY-MM-DD */
    MERMAID_GANTT_START_AFTER     /* `after a b`: where the last of the named tasks ends */
};
#endif
