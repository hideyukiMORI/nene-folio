#ifndef NENEFOLIO_DIAGRAM_DATE_H
#define NENEFOLIO_DIAGRAM_DATE_H
/* #992: a day of the proleptic Gregorian calendar, as written in a Mermaid gantt (YYYY-MM-DD). */
struct diagram_date
{
    int year;  /* 1..9999 */
    int month; /* 1..12 */
    int day;   /* 1..31 */
};
#endif
