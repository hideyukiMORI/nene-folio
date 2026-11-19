/* #992 / ADR0115: one gantt task line. The name is what comes before the first `:`; after it,
 * comma-separated items: tags (`done`, `active`, `crit`, `milestone`) first, then one to three
 * of `[id,] [start,] end`, as Mermaid reads them. */
#include "mermaid_gantt_line.h"
#include "diagram_calendar.h"
#include "diagram_limits.h"
#include "mermaid_gantt.h"
constexpr size_t item_limit = 8;
constexpr int32_t duration_digit_limit = 5;

static bool digit(char value)
{
    return value >= '0' && value <= '9';
}

static int number(struct diagram_text text, size_t at, size_t count)
{
    int value = 0;
    for (size_t index = at; index < at + count; ++index)
        value = value * 10 + (text.bytes[index] - '0');
    return value;
}

static bool date_shape(struct diagram_text text)
{
    if (text.length != 10 || text.bytes[4] != '-' || text.bytes[7] != '-')
        return false;
    for (size_t at = 0; at < text.length; ++at)
        if (at != 4 && at != 7 && !digit(text.bytes[at]))
            return false;
    return true;
}

enum diagram_outcome mermaid_gantt_line_date(struct diagram_text text, int32_t *_Nonnull out)
{
    if (!date_shape(text))
        return DIAGRAM_INVALID;
    struct diagram_date date = {number(text, 0, 4), number(text, 5, 2), number(text, 8, 2)};
    return diagram_calendar_day(date, out) ? DIAGRAM_ACCEPTED : DIAGRAM_INVALID;
}

/* Splits at commas; every item is trimmed. */
static size_t split(struct diagram_text text, struct diagram_text *_Nonnull items)
{
    size_t count = 0;
    size_t first = 0;
    for (size_t at = 0; at <= text.length; ++at)
    {
        if (at < text.length && text.bytes[at] != ',')
            continue;
        if (count == item_limit)
            return item_limit + 1;
        items[count++] = diagram_text_trim((struct diagram_text){text.bytes + first, at - first});
        first = at + 1;
    }
    return count;
}

static bool tag(struct diagram_text item, struct mermaid_gantt_task *_Nonnull task)
{
    if (diagram_text_equal(item, "done"))
        task->done = true;
    else if (diagram_text_equal(item, "active"))
        task->active = true;
    else if (diagram_text_equal(item, "crit"))
        task->critical = true;
    else if (diagram_text_equal(item, "milestone"))
        task->milestone = true;
    else
        return false;
    return true;
}

static enum diagram_outcome start(struct diagram_text item,
                                  struct mermaid_gantt_task *_Nonnull task)
{
    struct diagram_text rest = item;
    struct diagram_text word = diagram_text_word(&rest);
    if (diagram_text_equal(word, "after"))
    {
        task->start = MERMAID_GANTT_START_AFTER;
        task->after = diagram_text_trim(rest);
        return task->after.length == 0 ? DIAGRAM_INVALID : DIAGRAM_ACCEPTED;
    }
    task->start = MERMAID_GANTT_START_DATE;
    /* Other date formats and times of day belong to another dateFormat; a date of the right
     * shape that does not exist is INVALID. */
    if (!date_shape(item))
        return item.length > 0 && digit(item.bytes[0]) ? DIAGRAM_UNSUPPORTED : DIAGRAM_INVALID;
    return mermaid_gantt_line_date(item, &task->start_day);
}

/* The shape of a duration of `count` digits: one unit letter after them. A fraction or a
 * longer unit (`ms`) is UNSUPPORTED; so is a unit other than days and weeks. */
static enum diagram_outcome duration_shape(struct diagram_text item, size_t count)
{
    if (count == 0 || count == item.length)
        return DIAGRAM_INVALID;
    if (count + 1 < item.length || item.bytes[count] == '.')
        return DIAGRAM_UNSUPPORTED;
    if (count > duration_digit_limit)
        return DIAGRAM_TOO_LARGE;
    char unit = item.bytes[count];
    return unit == 'd' || unit == 'w' ? DIAGRAM_ACCEPTED : DIAGRAM_UNSUPPORTED;
}

/* `3d` or `2w`. Hours, minutes, months, years and fractions are UNSUPPORTED. */
static enum diagram_outcome duration(struct diagram_text item,
                                     struct mermaid_gantt_task *_Nonnull task)
{
    size_t count = 0;
    while (count < item.length && digit(item.bytes[count]))
        ++count;
    enum diagram_outcome shape = duration_shape(item, count);
    if (shape != DIAGRAM_ACCEPTED)
        return shape;
    int32_t value = (int32_t)number(item, 0, count);
    char unit = item.bytes[count];
    task->days = unit == 'w' ? value * 7 : value;
    task->end = MERMAID_GANTT_END_DAYS;
    return task->days > mermaid_gantt_span_limit ? DIAGRAM_TOO_LARGE : DIAGRAM_ACCEPTED;
}

static enum diagram_outcome end(struct diagram_text item, struct mermaid_gantt_task *_Nonnull task)
{
    struct diagram_text rest = item;
    if (diagram_text_equal(diagram_text_word(&rest), "until"))
    {
        task->end = MERMAID_GANTT_END_UNTIL;
        task->until = diagram_text_trim(rest);
        return task->until.length == 0 ? DIAGRAM_INVALID : DIAGRAM_ACCEPTED;
    }
    if (date_shape(item))
    {
        task->end = MERMAID_GANTT_END_DATE;
        return mermaid_gantt_line_date(item, &task->end_day);
    }
    return duration(item, task);
}

/* The items after the tags: `end`, `start, end` or `id, start, end`. */
static enum diagram_outcome schedule(const struct diagram_text *_Nonnull items, size_t count,
                                     struct mermaid_gantt_task *_Nonnull task)
{
    if (count == 0 || count > 3)
        return DIAGRAM_INVALID;
    if (count == 3)
    {
        task->id = items[0];
        if (task->id.length == 0)
            return DIAGRAM_INVALID;
    }
    task->start = MERMAID_GANTT_START_PREVIOUS;
    if (count >= 2)
    {
        enum diagram_outcome outcome = start(items[count - 2], task);
        if (outcome != DIAGRAM_ACCEPTED)
            return outcome;
    }
    return end(items[count - 1], task);
}

enum diagram_outcome mermaid_gantt_line_read(struct diagram_text text,
                                             struct mermaid_gantt_task *_Nonnull out)
{
    size_t colon = 0;
    while (colon < text.length && text.bytes[colon] != ':')
        ++colon;
    struct diagram_text name = diagram_text_trim((struct diagram_text){text.bytes, colon});
    if (colon == text.length || name.length == 0)
        return DIAGRAM_INVALID;
    *out = (struct mermaid_gantt_task){.name = name, .section = diagram_none};
    struct diagram_text items[item_limit];
    size_t count = split(diagram_text_skip(text, colon + 1), items);
    if (count > item_limit)
        return DIAGRAM_INVALID;
    size_t first = 0;
    while (first < count && tag(items[first], out))
        ++first;
    if (first < count && diagram_text_equal(items[first], "vert"))
        return DIAGRAM_UNSUPPORTED; /* a vertical marker line, not a bar */
    if (out->done)
        out->active = false; /* done wins, as Mermaid draws it */
    return schedule(items + first, count - first, out);
}
