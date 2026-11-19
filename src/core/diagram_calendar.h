#ifndef NENEFOLIO_DIAGRAM_CALENDAR_H
#define NENEFOLIO_DIAGRAM_CALENDAR_H
#include "diagram_date.h"
#include <stdint.h>
/* #992 / ADR0115: dates of a gantt as day numbers, with integers only. No clock, time zone or
 * locale is read (ARC-007): a chart is drawn from what its source says. Day 0 is 0000-03-01, so
 * every day of the years 1..9999 is a non-negative number. */
constexpr int diagram_calendar_first_year = 1;
constexpr int diagram_calendar_last_year = 9999;
/* The day number of a date; false when the date does not exist (2026-02-30, month 13, year 0). */
[[nodiscard]] bool diagram_calendar_day(struct diagram_date date, int32_t *_Nonnull out);
/* The date of a day number made by diagram_calendar_day (or a sum of one and a day count). */
[[nodiscard]] struct diagram_date diagram_calendar_date(int32_t day);
/* 0 for Monday through 6 for Sunday. */
[[nodiscard]] int diagram_calendar_weekday(int32_t day);
#endif
