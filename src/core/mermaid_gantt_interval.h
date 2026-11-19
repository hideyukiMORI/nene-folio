#ifndef NENEFOLIO_MERMAID_GANTT_INTERVAL_H
#define NENEFOLIO_MERMAID_GANTT_INTERVAL_H
/* #992: the unit of `tickInterval`. AUTO picks the finest interval that gives at most ten marks. */
enum mermaid_gantt_interval : unsigned char
{
    MERMAID_GANTT_INTERVAL_AUTO,
    MERMAID_GANTT_INTERVAL_DAY,
    MERMAID_GANTT_INTERVAL_WEEK,
    MERMAID_GANTT_INTERVAL_MONTH
};
#endif
