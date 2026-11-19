/* #992 / ADR0115: when each task begins and ends. Tasks are resolved in passes, as Mermaid does,
 * so `after` may name a task written further down. Nothing reads the clock: where Mermaid would
 * fall back to today, the chart is UNSUPPORTED. */
#include "mermaid_gantt_schedule.h"
#include "diagram_calendar.h"
#include "diagram_limits.h"
#include <string.h>

static bool excluded(const struct mermaid_gantt_chart *_Nonnull chart, int32_t day)
{
    if (chart->weekends && diagram_calendar_weekday(day) >= 5)
        return true;
    for (size_t at = 0; at < chart->excluded_count; ++at)
        if (chart->excluded[at] == day)
            return true;
    return false;
}

/* The first day after `days` working days from `begin`. */
static enum diagram_outcome advance(const struct mermaid_gantt_chart *_Nonnull chart, int32_t begin,
                                    int32_t days, int32_t *_Nonnull out)
{
    int32_t end = begin;
    for (int32_t counted = 0; counted < days; ++end)
    {
        if (end - begin > mermaid_gantt_span_limit)
            return DIAGRAM_TOO_LARGE;
        if (!excluded(chart, end))
            ++counted;
    }
    *out = end;
    return DIAGRAM_ACCEPTED;
}

static size_t find(const struct mermaid_gantt_chart *_Nonnull chart, struct diagram_text id)
{
    for (size_t at = 0; at < chart->count; ++at)
    {
        struct diagram_text other = chart->tasks[at].id;
        if (other.length == id.length && other.length != 0 &&
            memcmp(other.bytes, id.bytes, id.length) == 0)
            return at;
    }
    return diagram_none;
}

/* The latest end of the tasks named after `after`. False while one of them is unresolved. */
static enum diagram_outcome after(const struct mermaid_gantt_chart *_Nonnull chart,
                                  struct diagram_text ids, bool *_Nonnull ready,
                                  int32_t *_Nonnull out)
{
    *ready = true;
    *out = 0;
    struct diagram_text rest = ids;
    while (rest.length != 0)
    {
        struct diagram_text id = diagram_text_word(&rest);
        size_t at = find(chart, id);
        if (at == diagram_none)
            return DIAGRAM_UNSUPPORTED;
        if (!chart->tasks[at].resolved)
            *ready = false;
        else if (chart->tasks[at].finish > *out)
            *out = chart->tasks[at].finish;
        rest = diagram_text_trim(rest);
    }
    return DIAGRAM_ACCEPTED;
}

static enum diagram_outcome begin_of(const struct mermaid_gantt_chart *_Nonnull chart, size_t at,
                                     bool *_Nonnull ready, int32_t *_Nonnull out)
{
    const struct mermaid_gantt_task *task = &chart->tasks[at];
    *ready = true;
    switch (task->start)
    {
    case MERMAID_GANTT_START_DATE:
        *out = task->start_day;
        return DIAGRAM_ACCEPTED;
    case MERMAID_GANTT_START_PREVIOUS:
        if (at == 0)
            return DIAGRAM_UNSUPPORTED;
        *ready = chart->tasks[at - 1].resolved;
        *out = chart->tasks[at - 1].finish;
        return DIAGRAM_ACCEPTED;
    case MERMAID_GANTT_START_AFTER:
        return after(chart, task->after, ready, out);
    }
    return DIAGRAM_INVALID;
}

static enum diagram_outcome finish_of(const struct mermaid_gantt_chart *_Nonnull chart,
                                      const struct mermaid_gantt_task *_Nonnull task, int32_t begin,
                                      int32_t *_Nonnull out)
{
    switch (task->end)
    {
    case MERMAID_GANTT_END_DATE:
        *out = task->end_day;
        return task->end_day < begin ? DIAGRAM_INVALID : DIAGRAM_ACCEPTED;
    case MERMAID_GANTT_END_DAYS:
        return advance(chart, begin, task->days, out);
    }
    return DIAGRAM_INVALID;
}

/* A duration that ends where days are excluded draws to its last working day, and the task
 * after it begins after the excluded days (Mermaid's renderEndTime and endTime). */
static enum diagram_outcome settle(const struct mermaid_gantt_chart *_Nonnull chart,
                                   struct mermaid_gantt_task *_Nonnull task, int32_t begin)
{
    task->drawn = task->finish;
    if (task->end != MERMAID_GANTT_END_DAYS)
        return DIAGRAM_ACCEPTED;
    while (excluded(chart, task->finish))
        if (++task->finish - begin > mermaid_gantt_span_limit)
            return DIAGRAM_TOO_LARGE;
    return DIAGRAM_ACCEPTED;
}

/* A task whose start is known: its ends, and it is resolved. */
static enum diagram_outcome resolve(const struct mermaid_gantt_chart *_Nonnull chart,
                                    struct mermaid_gantt_task *_Nonnull task, int32_t begin)
{
    enum diagram_outcome outcome = finish_of(chart, task, begin, &task->finish);
    if (outcome == DIAGRAM_ACCEPTED)
        outcome = settle(chart, task, begin);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    task->begin = begin;
    task->resolved = true;
    return DIAGRAM_ACCEPTED;
}

/* One pass over the tasks not yet resolved. `progress` tells whether any was. */
static enum diagram_outcome pass(struct mermaid_gantt_chart *_Nonnull chart,
                                 bool *_Nonnull progress)
{
    *progress = false;
    for (size_t at = 0; at < chart->count; ++at)
    {
        struct mermaid_gantt_task *task = &chart->tasks[at];
        bool ready = false;
        int32_t begin = 0;
        enum diagram_outcome outcome =
            task->resolved ? DIAGRAM_ACCEPTED : begin_of(chart, at, &ready, &begin);
        if (outcome != DIAGRAM_ACCEPTED)
            return outcome;
        if (task->resolved || !ready)
            continue;
        outcome = resolve(chart, task, begin);
        if (outcome != DIAGRAM_ACCEPTED)
            return outcome;
        *progress = true;
    }
    return DIAGRAM_ACCEPTED;
}

/* The first day and the length of the chart. */
static enum diagram_outcome extent(struct mermaid_gantt_chart *_Nonnull chart)
{
    int32_t first = chart->tasks[0].begin;
    int32_t last = chart->tasks[0].drawn;
    for (size_t at = 1; at < chart->count; ++at)
    {
        if (chart->tasks[at].begin < first)
            first = chart->tasks[at].begin;
        if (chart->tasks[at].drawn > last)
            last = chart->tasks[at].drawn;
    }
    int32_t final_day = 0;
    if (!diagram_calendar_day((struct diagram_date){diagram_calendar_last_year, 12, 31},
                              &final_day) ||
        last - first > mermaid_gantt_span_limit || last > final_day + 1)
        return DIAGRAM_TOO_LARGE;
    chart->origin = first;
    chart->span = last > first ? last - first : 1;
    return DIAGRAM_ACCEPTED;
}

enum diagram_outcome mermaid_gantt_schedule_resolve(struct mermaid_gantt_chart *_Nonnull chart)
{
    if (chart->count == 0)
        return DIAGRAM_INVALID;
    for (size_t round = 0; round <= chart->count; ++round)
    {
        bool progress = false;
        enum diagram_outcome outcome = pass(chart, &progress);
        if (outcome != DIAGRAM_ACCEPTED)
            return outcome;
        if (!progress)
            break;
    }
    for (size_t at = 0; at < chart->count; ++at)
        if (!chart->tasks[at].resolved)
            return DIAGRAM_INVALID; /* `after` in a cycle */
    return extent(chart);
}
