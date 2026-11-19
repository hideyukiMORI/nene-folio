/* #992 / ADR0115: the civil-from-days algorithm (H. Hinnant), counted from 0000-03-01 so that the
 * leap day ends a year and every quotient and remainder is of a non-negative number. */
#include "diagram_calendar.h"
constexpr int32_t days_per_era = 146097; /* 400 years */
constexpr int32_t days_per_year = 365;

static bool leap(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

static int month_length(int year, int month)
{
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return month == 2 && leap(year) ? 29 : lengths[month - 1];
}

bool diagram_calendar_day(struct diagram_date date, int32_t *_Nonnull out)
{
    if (date.year < diagram_calendar_first_year || date.year > diagram_calendar_last_year ||
        date.month < 1 || date.month > 12 || date.day < 1 ||
        date.day > month_length(date.year, date.month))
        return false;
    int32_t year = date.month <= 2 ? date.year - 1 : date.year;
    int32_t era = year / 400;
    int32_t of_era = year - era * 400;
    int32_t shifted = (date.month + 9) % 12; /* March is 0 */
    int32_t of_year = (153 * shifted + 2) / 5 + date.day - 1;
    int32_t days = of_era * days_per_year + of_era / 4 - of_era / 100 + of_year;
    *out = era * days_per_era + days;
    return true;
}

struct diagram_date diagram_calendar_date(int32_t day)
{
    int32_t era = day / days_per_era;
    int32_t of_era = day - era * days_per_era;
    int32_t year = (of_era - of_era / 1460 + of_era / 36524 - of_era / 146096) / days_per_year;
    int32_t of_year = of_era - (days_per_year * year + year / 4 - year / 100);
    int32_t shifted = (5 * of_year + 2) / 153;
    int month = (int)(shifted < 10 ? shifted + 3 : shifted - 9);
    int civil_year = (int)(year + era * 400) + (month <= 2 ? 1 : 0);
    return (struct diagram_date){civil_year, month, (int)(of_year - (153 * shifted + 2) / 5 + 1)};
}

/* 0000-03-01 was a Wednesday. */
int diagram_calendar_weekday(int32_t day)
{
    return (int)((day + 2) % 7);
}
