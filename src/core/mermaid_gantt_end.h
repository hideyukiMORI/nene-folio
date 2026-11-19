#ifndef NENEFOLIO_MERMAID_GANTT_END_H
#define NENEFOLIO_MERMAID_GANTT_END_H
/* #992: where a gantt task ends, as written. */
enum mermaid_gantt_end : unsigned char
{
    MERMAID_GANTT_END_DATE, /* YYYY-MM-DD, the first day after the task (as Mermaid reads it) */
    MERMAID_GANTT_END_DAYS  /* `3d`, `2w`: working days from the start */
};
#endif
