/* #992 / ADR0115: Mermaid `gantt`. The lines are directives (`title`, `dateFormat`, `axisFormat`,
 * `tickInterval`, `excludes`, `todayMarker`, `section`) or tasks. What this reader does not draw
 * is UNSUPPORTED as a whole; a chart is never drawn in part. */
#include "mermaid_gantt.h"
#include "diagram_label.h"
#include "diagram_statement.h"
#include "mermaid_gantt_axis.h"
#include "mermaid_gantt_chart.h"
#include "mermaid_gantt_line.h"
#include "mermaid_gantt_schedule.h"
#include <stdlib.h>
#include <string.h>
constexpr int32_t interval_limit = 999;

struct mermaid_gantt
{
    struct diagram_model *_Nonnull model;
    enum diagram_outcome outcome;
    bool titled;
    struct mermaid_gantt_chart chart;
};

static char lower(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value - 'A' + 'a') : value;
}

/* A directive word, compared as Mermaid's gantt lexer does: ASCII letters in any case. */
static bool keyword(struct diagram_text text, const char *_Nonnull word)
{
    size_t at = 0;
    while (at < text.length && word[at] != '\0' && lower(text.bytes[at]) == lower(word[at]))
        ++at;
    return at == text.length && word[at] == '\0';
}

static bool reject(struct mermaid_gantt *_Nonnull parser, enum diagram_outcome outcome)
{
    parser->outcome = outcome;
    return false;
}

static bool title(struct mermaid_gantt *_Nonnull parser, struct diagram_text text)
{
    text = diagram_text_trim(text);
    if (parser->titled || text.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    size_t index = 0;
    parser->outcome = diagram_label_store(parser->model, text, &index);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    diagram_model_title(parser->model, index);
    parser->titled = true;
    return true;
}

/* `1day`, `2week`, `1month`. Shorter units need a time of day on the axis. */
static bool interval(struct mermaid_gantt *_Nonnull parser, struct diagram_text text)
{
    static const struct
    {
        const char *_Nonnull unit;
        enum mermaid_gantt_interval interval;
    } units[] = {{"day", MERMAID_GANTT_INTERVAL_DAY},
                 {"week", MERMAID_GANTT_INTERVAL_WEEK},
                 {"month", MERMAID_GANTT_INTERVAL_MONTH}};
    size_t digits = 0;
    int32_t count = 0;
    for (; digits < text.length && text.bytes[digits] >= '0' && text.bytes[digits] <= '9' &&
           digits <= 3;
         ++digits)
        count = count * 10 + (text.bytes[digits] - '0');
    if (digits == 0 || digits > 3 || text.bytes[0] == '0' || count > interval_limit)
        return reject(parser, DIAGRAM_INVALID);
    struct diagram_text unit = diagram_text_skip(text, digits);
    for (size_t at = 0; at < sizeof units / sizeof units[0]; ++at)
        if (diagram_text_equal(unit, units[at].unit))
        {
            parser->chart.interval = units[at].interval;
            parser->chart.interval_count = count;
            return true;
        }
    return reject(parser, DIAGRAM_UNSUPPORTED);
}

/* One item of `excludes`: `weekends` or a date. Weekday names are UNSUPPORTED. */
static bool exclude(struct mermaid_gantt *_Nonnull parser, struct diagram_text item)
{
    if (keyword(item, "weekends"))
    {
        parser->chart.weekends = true;
        return true;
    }
    if (parser->chart.excluded_count == mermaid_gantt_excluded_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    enum diagram_outcome outcome =
        mermaid_gantt_line_date(item, &parser->chart.excluded[parser->chart.excluded_count]);
    if (outcome == DIAGRAM_ACCEPTED)
    {
        ++parser->chart.excluded_count;
        return true;
    }
    bool numeric = item.bytes[0] >= '0' && item.bytes[0] <= '9';
    return reject(parser, numeric ? outcome : DIAGRAM_UNSUPPORTED);
}

static bool separator(char value)
{
    return value == ' ' || value == '\t' || value == ',';
}

/* `excludes weekends`, `excludes 2026-11-23,2026-12-31`: items apart by spaces or commas, as
 * Mermaid splits them (`[\s,]+`). */
static bool excludes(struct mermaid_gantt *_Nonnull parser, struct diagram_text text)
{
    size_t at = 0;
    while (at < text.length)
    {
        while (at < text.length && separator(text.bytes[at]))
            ++at;
        size_t first = at;
        while (at < text.length && !separator(text.bytes[at]))
            ++at;
        if (at > first && !exclude(parser, (struct diagram_text){text.bytes + first, at - first}))
            return false;
    }
    return true;
}

static bool section(struct mermaid_gantt *_Nonnull parser, struct diagram_text text)
{
    text = diagram_text_trim(text);
    if (text.length == 0)
        return reject(parser, DIAGRAM_INVALID);
    if (parser->chart.section_count == mermaid_gantt_section_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    parser->chart.sections[parser->chart.section_count++] = text;
    return true;
}

static bool known_id(const struct mermaid_gantt_chart *_Nonnull chart, struct diagram_text id)
{
    for (size_t at = 0; at < chart->count; ++at)
        if (chart->tasks[at].id.length == id.length && id.length != 0 &&
            memcmp(chart->tasks[at].id.bytes, id.bytes, id.length) == 0)
            return true;
    return false;
}

static bool task(struct mermaid_gantt *_Nonnull parser, struct diagram_text text)
{
    struct mermaid_gantt_chart *chart = &parser->chart;
    if (chart->count == mermaid_gantt_task_limit)
        return reject(parser, DIAGRAM_TOO_LARGE);
    struct mermaid_gantt_task *read = &chart->tasks[chart->count];
    parser->outcome = mermaid_gantt_line_read(text, read);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    if (known_id(chart, read->id))
        return reject(parser, DIAGRAM_INVALID);
    read->section = chart->section_count == 0 ? diagram_none : chart->section_count - 1;
    ++chart->count;
    return true;
}

/* Directives the reader does not draw: the whole chart is UNSUPPORTED. */
static bool unsupported(struct diagram_text word)
{
    static const char *const words[] = {
        "includes", "weekend", "inclusiveEndDates", "topAxis",  "displayMode", "weekday",
        "click",    "call",    "accTitle",          "accDescr", "accTitle:",   "accDescr:",
        "accDescr{"};
    for (size_t at = 0; at < sizeof words / sizeof words[0]; ++at)
        if (keyword(word, words[at]))
            return true;
    return false;
}

static bool directive(struct mermaid_gantt *_Nonnull parser, struct diagram_text word,
                      struct diagram_text rest, bool *_Nonnull handled)
{
    *handled = true;
    rest = diagram_text_trim(rest);
    if (keyword(word, "title"))
        return title(parser, rest);
    if (keyword(word, "dateFormat"))
        return diagram_text_equal(rest, "YYYY-MM-DD") || reject(parser, DIAGRAM_UNSUPPORTED);
    if (keyword(word, "axisFormat"))
    {
        parser->chart.axis_format = rest;
        return rest.length != 0 || reject(parser, DIAGRAM_INVALID);
    }
    if (keyword(word, "tickInterval"))
        return interval(parser, rest);
    if (keyword(word, "excludes"))
        return excludes(parser, rest);
    if (keyword(word, "section"))
        return section(parser, rest);
    if (keyword(word, "todayMarker"))
        return true; /* nothing is drawn for today */
    *handled = false;
    return true;
}

static bool statement(struct mermaid_gantt *_Nonnull parser, struct diagram_text text)
{
    text = diagram_text_trim(text);
    if (text.length == 0)
        return true;
    struct diagram_text rest = text;
    struct diagram_text word = diagram_text_word(&rest);
    if (unsupported(word))
        return reject(parser, DIAGRAM_UNSUPPORTED);
    bool handled = false;
    bool kept = directive(parser, word, rest, &handled);
    return handled ? kept : task(parser, text);
}

static bool groups(struct mermaid_gantt *_Nonnull parser)
{
    for (size_t at = 0; at < parser->chart.section_count; ++at)
    {
        struct diagram_group group = {.parent = diagram_none};
        parser->outcome = diagram_label_store(parser->model, parser->chart.sections[at], &group.id);
        group.label = group.id;
        if (parser->outcome == DIAGRAM_ACCEPTED)
            parser->outcome = diagram_model_group(parser->model, at, group);
        if (parser->outcome != DIAGRAM_ACCEPTED)
            return false;
    }
    return true;
}

static enum diagram_task_state state_of(const struct mermaid_gantt_task *_Nonnull read)
{
    if (read->done)
        return DIAGRAM_TASK_DONE;
    return read->active ? DIAGRAM_TASK_ACTIVE : DIAGRAM_TASK_PLANNED;
}

static bool nodes(struct mermaid_gantt *_Nonnull parser)
{
    const struct mermaid_gantt_chart *chart = &parser->chart;
    for (size_t at = 0; at < chart->count; ++at)
    {
        const struct mermaid_gantt_task *read = &chart->tasks[at];
        struct diagram_node node = {.group = read->section,
                                    .shape = read->milestone ? DIAGRAM_DIAMOND : DIAGRAM_RECTANGLE,
                                    .start = (size_t)(read->begin - chart->origin),
                                    .length = (size_t)(read->drawn - read->begin),
                                    .state = state_of(read),
                                    .critical = read->critical};
        parser->outcome = diagram_label_store(parser->model, read->name, &node.label);
        node.id = node.label;
        if (parser->outcome == DIAGRAM_ACCEPTED && read->id.length != 0)
            parser->outcome = diagram_label_store(parser->model, read->id, &node.id);
        if (parser->outcome == DIAGRAM_ACCEPTED)
            parser->outcome = diagram_model_node(parser->model, at, node);
        if (parser->outcome != DIAGRAM_ACCEPTED)
            return false;
    }
    return true;
}

static bool build(struct mermaid_gantt *_Nonnull parser)
{
    parser->outcome = mermaid_gantt_schedule_resolve(&parser->chart);
    if (parser->outcome != DIAGRAM_ACCEPTED || !groups(parser) || !nodes(parser))
        return false;
    parser->outcome = mermaid_gantt_axis_build(&parser->chart, parser->model);
    return parser->outcome == DIAGRAM_ACCEPTED;
}

static bool parse_document(struct mermaid_gantt *_Nonnull parser, struct diagram_text source)
{
    struct diagram_input input = {.text = source, .kind = DIAGRAM_GANTT};
    struct diagram_statement first;
    parser->outcome = diagram_statement_read(input, 0, &first);
    if (parser->outcome != DIAGRAM_ACCEPTED)
        return false;
    struct diagram_text header = diagram_text_word(&first.text);
    if (!diagram_text_equal(header, "gantt") || diagram_text_trim(first.text).length != 0)
        return reject(parser, DIAGRAM_INVALID);
    size_t cursor = first.next;
    while (cursor < source.length)
    {
        struct diagram_statement next;
        parser->outcome = diagram_statement_read(input, cursor, &next);
        if (parser->outcome != DIAGRAM_ACCEPTED || !statement(parser, next.text))
            return false;
        cursor = next.next;
    }
    if (parser->chart.count == 0)
        return reject(parser, DIAGRAM_INVALID);
    return build(parser);
}

enum diagram_outcome mermaid_gantt_create(struct diagram_text source,
                                          struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = nullptr;
    enum diagram_outcome outcome = diagram_model_create(DIAGRAM_GANTT, &model);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct mermaid_gantt *_Nullable parser = calloc(1, sizeof *parser);
    if (parser == nullptr)
    {
        diagram_model_destroy(model);
        return DIAGRAM_OUT_OF_MEMORY;
    }
    parser->model = model;
    parser->outcome = DIAGRAM_ACCEPTED;
    (void)parse_document(parser, source);
    outcome = parser->outcome;
    free(parser);
    if (outcome != DIAGRAM_ACCEPTED)
    {
        diagram_model_destroy(model);
        return outcome;
    }
    diagram_model_seal(model);
    *out = model;
    return DIAGRAM_ACCEPTED;
}
