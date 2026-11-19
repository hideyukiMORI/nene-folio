/* #992 / ADR0115: when each task begins and ends. Tasks are resolved in passes, as Mermaid does,
 * so `after` may name a task written further down. Nothing reads the clock: where Mermaid would
 * fall back to today, the chart is UNSUPPORTED. */
#include "mermaid_gantt_schedule.h"
#include "diagram_calendar.h"
#include "diagram_limits.h"
#include <string.h>

static bool excluded(const struct mermaid_gantt_chart *_Nonnull chart, int32_t day)
{
    int weekday = diagram_calendar_weekday(day);
    if ((chart->weekends && weekday >= 5) || ((chart->weekdays >> weekday) & 1U) != 0)
        return true;
    for (size_t at = 0; at < chart->excluded_count; ++at)
        if (chart->excluded[at] == day)
            return true;
    return false;
}

/* The first day after `days` working days from `begin`. The day a task begins on is counted
 * even when it is excluded: Mermaid walks from the day after the start (#1022 review). */
static enum diagram_outcome advance(const struct mermaid_gantt_chart *_Nonnull chart, int32_t begin,
                                    int32_t days, int32_t *_Nonnull out)
{
    int32_t end = begin;
    for (int32_t counted = 0; counted < days; ++end)
    {
        if (end - begin > mermaid_gantt_span_limit)
            return DIAGRAM_TOO_LARGE;
        if (end == begin || !excluded(chart, end))
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

/* #1022: `until a b` ends where the first of the named tasks begins. It is kept as that many
 * days from the start, so excluded days move the end as they move a duration's (Mermaid
 * fixes both the same way). Not ready while a named task's start is unknown. */
static enum diagram_outcome earliest(const struct mermaid_gantt_chart *_Nonnull chart,
                                     struct diagram_text ids, bool *_Nonnull ready,
                                     int32_t *_Nonnull out)
{
    *out = INT32_MAX;
    struct diagram_text rest = ids;
    while (rest.length != 0)
    {
        struct diagram_text id = diagram_text_word(&rest);
        size_t at = find(chart, id);
        if (at == diagram_none)
            return DIAGRAM_UNSUPPORTED;
        if (!chart->tasks[at].begun)
            *ready = false;
        else if (chart->tasks[at].begin < *out)
            *out = chart->tasks[at].begin;
        rest = diagram_text_trim(rest);
    }
    return DIAGRAM_ACCEPTED;
}

static enum diagram_outcome until(const struct mermaid_gantt_chart *_Nonnull chart,
                                  struct mermaid_gantt_task *_Nonnull task, bool *_Nonnull ready)
{
    *ready = true;
    if (task->end != MERMAID_GANTT_END_UNTIL)
        return DIAGRAM_ACCEPTED;
    int32_t first = 0;
    enum diagram_outcome outcome = earliest(chart, task->until, ready, &first);
    if (outcome != DIAGRAM_ACCEPTED || !*ready)
        return outcome;
    if (first < task->begin)
        return DIAGRAM_INVALID;
    task->days = first - task->begin;
    return task->days > mermaid_gantt_span_limit ? DIAGRAM_TOO_LARGE : DIAGRAM_ACCEPTED;
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
    case MERMAID_GANTT_END_UNTIL:
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
    if (task->end == MERMAID_GANTT_END_DATE || task->days == 0)
        return DIAGRAM_ACCEPTED; /* nothing to walk: a task of no days ends where it begins */
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
    task->resolved = true;
    return DIAGRAM_ACCEPTED;
}

/* One task not yet resolved: its start as soon as that is known, then its end. */
static enum diagram_outcome attempt(struct mermaid_gantt_chart *_Nonnull chart, size_t at,
                                    bool *_Nonnull progress)
{
    struct mermaid_gantt_task *task = &chart->tasks[at];
    bool ready = false;
    int32_t begin = 0;
    enum diagram_outcome outcome = begin_of(chart, at, &ready, &begin);
    if (outcome != DIAGRAM_ACCEPTED || !ready)
        return outcome;
    *progress = *progress || !task->begun;
    task->begin = begin;
    task->begun = true;
    outcome = until(chart, task, &ready);
    if (outcome != DIAGRAM_ACCEPTED || !ready)
        return outcome;
    *progress = true;
    return resolve(chart, task, begin);
}

/* One pass over the tasks not yet resolved. `progress` tells whether any was. */
static enum diagram_outcome pass(struct mermaid_gantt_chart *_Nonnull chart,
                                 bool *_Nonnull progress)
{
    *progress = false;
    for (size_t at = 0; at < chart->count; ++at)
    {
        enum diagram_outcome outcome =
            chart->tasks[at].resolved ? DIAGRAM_ACCEPTED : attempt(chart, at, progress);
        if (outcome != DIAGRAM_ACCEPTED)
            return outcome;
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
    for (size_t round = 0; round <= 2 * chart->count; ++round) /* a start, then an end */
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
            return DIAGRAM_INVALID; /* `after` or `until` in a cycle */
    return extent(chart);
}
