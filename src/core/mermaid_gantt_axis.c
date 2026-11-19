/* #992 / ADR0115: the marks of a gantt's time axis. The labels are made here, as model texts, so
 * that they are measured with every other text (the layout cannot add text). Without
 * `tickInterval`, the finest of a day, a week, two weeks, a month, a quarter and a year that gives
 * at most ten marks is used. Weeks start on Monday. */
#include "mermaid_gantt_axis.h"
#include "diagram_calendar.h"
#include "diagram_limits.h"
#include "mermaid_gantt_run.h"
constexpr size_t automatic_mark_limit = 10;
constexpr size_t label_capacity = 64;

struct mermaid_gantt_axis
{
    const struct mermaid_gantt_chart *_Nonnull chart;
    struct diagram_model *_Nonnull model;
    char label[label_capacity];
    size_t used;
    int previous_year;
    enum diagram_outcome outcome;
};

static int32_t month_start(int year, int month)
{
    int32_t day = 0;
    return diagram_calendar_day((struct diagram_date){year, month, 1}, &day) ? day : INT32_MAX;
}

/* The first day of the month `months` after the month of `day`. */
static int32_t next_month(int32_t day, int32_t months)
{
    if (day == INT32_MAX)
        return INT32_MAX;
    struct diagram_date date = diagram_calendar_date(day);
    int index = date.year * 12 + (date.month - 1) + (int)months;
    return month_start(index / 12, index % 12 + 1);
}

static int32_t next_mark(struct mermaid_gantt_run run, int32_t day)
{
    return run.months == 0 ? day + run.step : next_month(day, run.months);
}

/* A run of whole months starts on the first first-of-month at or after the origin whose month
 * is a multiple of the run (quarters in January, April, July, October). */
static struct mermaid_gantt_run month_run(int32_t origin, int32_t months)
{
    struct diagram_date date = diagram_calendar_date(origin);
    int32_t first = date.day == 1 ? origin : next_month(origin, 1);
    while (first != INT32_MAX && (diagram_calendar_date(first).month - 1) % months != 0)
        first = next_month(first, 1);
    return (struct mermaid_gantt_run){.first = first, .months = months};
}

/* Week marks fall on the chart's `weekday` (Monday unless written, #1030). */
static struct mermaid_gantt_run week_run(const struct mermaid_gantt_chart *_Nonnull chart,
                                         int32_t weeks)
{
    int32_t ahead = (7 + chart->week_start - diagram_calendar_weekday(chart->origin)) % 7;
    return (struct mermaid_gantt_run){.first = chart->origin + ahead, .step = 7 * weeks};
}

static size_t mark_count(const struct mermaid_gantt_chart *_Nonnull chart,
                         struct mermaid_gantt_run run)
{
    size_t count = 0;
    for (int32_t day = run.first; day <= chart->origin + chart->span && count <= diagram_tick_limit;
         day = next_mark(run, day))
        ++count;
    return count;
}

static struct mermaid_gantt_run written_run(const struct mermaid_gantt_chart *_Nonnull chart)
{
    switch (chart->interval)
    {
    case MERMAID_GANTT_INTERVAL_DAY:
        return (struct mermaid_gantt_run){.first = chart->origin, .step = chart->interval_count};
    case MERMAID_GANTT_INTERVAL_AUTO:
        break;
    case MERMAID_GANTT_INTERVAL_WEEK:
        return week_run(chart, chart->interval_count);
    case MERMAID_GANTT_INTERVAL_MONTH:
        return month_run(chart->origin, chart->interval_count);
    }
    return (struct mermaid_gantt_run){.first = chart->origin, .step = 1};
}

static struct mermaid_gantt_run chosen_run(const struct mermaid_gantt_chart *_Nonnull chart)
{
    if (chart->interval != MERMAID_GANTT_INTERVAL_AUTO)
        return written_run(chart);
    const struct mermaid_gantt_run runs[] = {{.first = chart->origin, .step = 1},
                                             week_run(chart, 1),
                                             week_run(chart, 2),
                                             month_run(chart->origin, 1),
                                             month_run(chart->origin, 3)};
    for (size_t at = 0; at < sizeof runs / sizeof runs[0]; ++at)
        if (mark_count(chart, runs[at]) <= automatic_mark_limit)
            return runs[at];
    return month_run(chart->origin, 12);
}

static bool append(struct mermaid_gantt_axis *_Nonnull axis, char value)
{
    if (axis->used == label_capacity)
    {
        axis->outcome = DIAGRAM_TOO_LARGE;
        return false;
    }
    axis->label[axis->used++] = value;
    return true;
}

static bool append_number(struct mermaid_gantt_axis *_Nonnull axis, int value, size_t width)
{
    char digits[8];
    size_t count = 0;
    do
        digits[count++] = (char)('0' + value % 10);
    while ((value /= 10) != 0 && count < sizeof digits);
    while (count < width && count < sizeof digits)
        digits[count++] = '0';
    while (count != 0)
        if (!append(axis, digits[--count]))
            return false;
    return true;
}

/* One `%` directive of axisFormat. Month and weekday names are a locale's, so UNSUPPORTED. */
static bool directive(struct mermaid_gantt_axis *_Nonnull axis, struct diagram_date date,
                      char value, bool bare)
{
    switch (value)
    {
    case 'Y':
        return !bare && append_number(axis, date.year, 4);
    case 'y':
        return !bare && append_number(axis, date.year % 100, 2);
    case 'm':
        return append_number(axis, date.month, bare ? 1 : 2);
    case 'd':
        return append_number(axis, date.day, bare ? 1 : 2);
    case 'e':
        return !bare && append_number(axis, date.day, 1);
    case '%':
        return !bare && append(axis, '%');
    default:
        axis->outcome = DIAGRAM_UNSUPPORTED; /* format letters are an open set */
        return false;
    }
}

/* One `%` directive at `*at` (which moves past it). */
static bool format_directive(struct mermaid_gantt_axis *_Nonnull axis, struct diagram_date date,
                             size_t *_Nonnull at)
{
    struct diagram_text format = axis->chart->axis_format;
    bool bare = *at + 1 < format.length && format.bytes[*at + 1] == '-';
    *at += bare ? 2 : 1;
    if (*at < format.length && directive(axis, date, format.bytes[*at], bare))
        return true;
    if (axis->outcome == DIAGRAM_ACCEPTED)
        axis->outcome = DIAGRAM_UNSUPPORTED;
    return false;
}

static bool formatted(struct mermaid_gantt_axis *_Nonnull axis, struct diagram_date date)
{
    struct diagram_text format = axis->chart->axis_format;
    for (size_t at = 0; at < format.length; ++at)
    {
        bool written = format.bytes[at] == '%' ? format_directive(axis, date, &at)
                                               : append(axis, format.bytes[at]);
        if (!written)
            return false;
    }
    return true;
}

/* `M/D`, and `YYYY/M/D` for the first mark and the first mark of a new year. */
static bool default_label(struct mermaid_gantt_axis *_Nonnull axis, struct diagram_date date)
{
    bool year = axis->previous_year != date.year;
    axis->previous_year = date.year;
    return (!year || (append_number(axis, date.year, 4) && append(axis, '/'))) &&
           append_number(axis, date.month, 1) && append(axis, '/') &&
           append_number(axis, date.day, 1);
}

static void mark(struct mermaid_gantt_axis *_Nonnull axis, int32_t day)
{
    struct diagram_date date = diagram_calendar_date(day);
    axis->used = 0;
    bool written =
        axis->chart->axis_format.length == 0 ? default_label(axis, date) : formatted(axis, date);
    if (!written)
    {
        if (axis->outcome == DIAGRAM_ACCEPTED)
            axis->outcome = DIAGRAM_INVALID;
        return;
    }
    size_t label = 0;
    axis->outcome = diagram_model_text(axis->model, axis->label, axis->used, &label);
    if (axis->outcome == DIAGRAM_ACCEPTED)
        axis->outcome = diagram_model_tick(
            axis->model, (struct diagram_tick){(size_t)(day - axis->chart->origin), label});
}

enum diagram_outcome mermaid_gantt_axis_build(const struct mermaid_gantt_chart *_Nonnull chart,
                                              struct diagram_model *_Nonnull model)
{
    struct mermaid_gantt_axis axis = {
        .chart = chart, .model = model, .previous_year = -1, .outcome = DIAGRAM_ACCEPTED};
    struct mermaid_gantt_run run = chosen_run(chart);
    if (mark_count(chart, run) > diagram_tick_limit)
        return DIAGRAM_TOO_LARGE;
    if (run.first > chart->origin + chart->span)
        run.first = chart->origin; /* a chart shorter than its interval still has one mark */
    for (int32_t day = run.first;
         day <= chart->origin + chart->span && axis.outcome == DIAGRAM_ACCEPTED;
         day = next_mark(run, day))
        mark(&axis, day);
    return axis.outcome;
}
