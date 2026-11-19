/* #992 / ADR0115: the layout of a gantt, in integers. Days map to x in sixteenths of a pixel so
 * that long charts still fit and short ones are not stretched past a readable width. */
#include "diagram_gantt_layout.h"
#include "diagram_style.h"
#include <stdint.h>
constexpr int gantt_chart_width = 560; /* the width a chart aims at */
constexpr int gantt_day_limit = 40;    /* at most this many pixels a day */
constexpr int gantt_fraction = 16;     /* x is kept in sixteenths of a pixel */
constexpr int gantt_bar = 16;
constexpr int gantt_row_padding = 8;
constexpr int gantt_section_gap = 8;
constexpr int gantt_section_padding = 12;
constexpr int gantt_label_gap = 6;
constexpr int gantt_tick_gap = 8;
constexpr int gantt_axis_gap = 4;
constexpr int gantt_title_gap = 12;
constexpr int gantt_bar_minimum = 2;
constexpr int gantt_width_limit = 16000;

struct diagram_gantt_layout
{
    const struct diagram_model *_Nonnull model;
    const struct diagram_metrics *_Nonnull metrics;
    struct diagram_scene *_Nonnull scene;
    enum diagram_outcome outcome;
    int axis_top;    /* the top of the axis labels */
    int tick_height; /* the tallest axis label */
    int chart_top;   /* the top of the first row */
    int chart_left;  /* x of the chart's first day */
    int scale;       /* sixteenths of a pixel per day */
    int bottom;      /* the bottom of the last row */
    int right;       /* the rightmost thing drawn */
    int lowest;      /* the lowest thing drawn */
};

static void emit(struct diagram_gantt_layout *_Nonnull layout, struct diagram_primitive primitive)
{
    if (layout->outcome == DIAGRAM_ACCEPTED)
        layout->outcome = diagram_scene_append(layout->scene, &primitive);
}

static struct diagram_size size_of(const struct diagram_gantt_layout *_Nonnull layout, size_t text,
                                   enum diagram_text_role role)
{
    return diagram_metrics_at(layout->metrics, text, role);
}

static int day_x(const struct diagram_gantt_layout *_Nonnull layout, size_t day)
{
    return layout->chart_left + (int)((int64_t)day * layout->scale / gantt_fraction);
}

static size_t span_of(const struct diagram_model *_Nonnull model)
{
    size_t span = 1;
    for (size_t at = 0; at < diagram_model_node_count(model); ++at)
    {
        struct diagram_node node = diagram_model_node_at(model, at);
        if (node.start + node.length > span)
            span = node.start + node.length;
    }
    return span;
}

/* The widest axis label and the fewest days between two marks. */
static void ticks_measure(struct diagram_gantt_layout *_Nonnull layout, int *_Nonnull widest,
                          size_t *_Nonnull closest)
{
    *widest = 0;
    *closest = SIZE_MAX;
    for (size_t at = 0; at < diagram_model_tick_count(layout->model); ++at)
    {
        struct diagram_tick tick = diagram_model_tick_at(layout->model, at);
        struct diagram_size size = size_of(layout, tick.label, DIAGRAM_TEXT_CAPTION);
        if (size.width > *widest)
            *widest = size.width;
        if (size.height > layout->tick_height)
            layout->tick_height = size.height;
        if (at != 0 && tick.day - diagram_model_tick_at(layout->model, at - 1).day < *closest)
            *closest = tick.day - diagram_model_tick_at(layout->model, at - 1).day;
    }
}

/* Aim at the chart width, never more than the day limit, and widen until labels do not touch. */
static void choose_scale(struct diagram_gantt_layout *_Nonnull layout)
{
    size_t span = span_of(layout->model);
    int widest = 0;
    size_t closest = SIZE_MAX;
    ticks_measure(layout, &widest, &closest);
    int64_t scale = (int64_t)gantt_chart_width * gantt_fraction / (int64_t)span;
    if (scale > gantt_day_limit * gantt_fraction)
        scale = gantt_day_limit * gantt_fraction;
    if (closest != SIZE_MAX)
    {
        int64_t needed =
            ((int64_t)(widest + gantt_tick_gap) * gantt_fraction + (int64_t)closest - 1) /
            (int64_t)closest;
        if (needed > scale)
            scale = needed;
    }
    if (scale < 1)
        scale = 1;
    if ((int64_t)span * scale / gantt_fraction > gantt_width_limit)
        layout->outcome = DIAGRAM_TOO_LARGE;
    layout->scale = (int)scale;
}

static int section_width(const struct diagram_gantt_layout *_Nonnull layout)
{
    int widest = 0;
    for (size_t at = 0; at < diagram_model_group_count(layout->model); ++at)
    {
        struct diagram_size size =
            size_of(layout, diagram_model_group_at(layout->model, at).label, DIAGRAM_TEXT_BODY);
        if (size.width > widest)
            widest = size.width;
    }
    return widest == 0 ? 0 : widest + gantt_section_padding;
}

static void measure(struct diagram_gantt_layout *_Nonnull layout)
{
    size_t title = diagram_model_title_text(layout->model);
    layout->axis_top = diagram_outer_margin;
    if (title != diagram_none)
        layout->axis_top += size_of(layout, title, DIAGRAM_TEXT_BODY).height + gantt_title_gap;
    choose_scale(layout);
    layout->chart_top = layout->axis_top + layout->tick_height + gantt_axis_gap;
    layout->chart_left = diagram_outer_margin + section_width(layout);
}

static int row_height(const struct diagram_gantt_layout *_Nonnull layout, struct diagram_node node)
{
    int label = size_of(layout, node.label, DIAGRAM_TEXT_CAPTION).height;
    return (label > gantt_bar ? label : gantt_bar) + gantt_row_padding;
}

/* #1006: the space after task `at`. Where the next task starts a new section, a gap, widened
 * when this section's name (wrapped) is taller than its rows so the next name never meets it. */
static int gap_after(const struct diagram_gantt_layout *_Nonnull layout, size_t at)
{
    struct diagram_node node = diagram_model_node_at(layout->model, at);
    if (at + 1 >= diagram_model_node_count(layout->model) ||
        diagram_model_node_at(layout->model, at + 1).group == node.group)
        return 0;
    if (node.group == diagram_none)
        return gantt_section_gap;
    int rows = 0;
    for (size_t back = at + 1;
         back > 0 && diagram_model_node_at(layout->model, back - 1).group == node.group; --back)
        rows += row_height(layout, diagram_model_node_at(layout->model, back - 1));
    size_t label = diagram_model_group_at(layout->model, node.group).label;
    int name = size_of(layout, label, DIAGRAM_TEXT_BODY).height;
    return gantt_section_gap + (name > rows ? name - rows : 0);
}

/* The top of a task's row. */
static int row_top(const struct diagram_gantt_layout *_Nonnull layout, size_t index)
{
    int top = layout->chart_top;
    for (size_t at = 0; at < index; ++at)
        top += row_height(layout, diagram_model_node_at(layout->model, at)) + gap_after(layout, at);
    return top;
}

static void emit_title(struct diagram_gantt_layout *_Nonnull layout, int width)
{
    size_t title = diagram_model_title_text(layout->model);
    if (title == diagram_none)
        return;
    struct diagram_size size = size_of(layout, title, DIAGRAM_TEXT_BODY);
    emit(layout,
         (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                    .ink = DIAGRAM_INK_TEXT,
                                    .text = title,
                                    .bounds = {diagram_outer_margin, diagram_outer_margin,
                                               width - diagram_outer_margin * 2, size.height}});
}

static void emit_axis(struct diagram_gantt_layout *_Nonnull layout)
{
    for (size_t at = 0; at < diagram_model_tick_count(layout->model); ++at)
    {
        struct diagram_tick tick = diagram_model_tick_at(layout->model, at);
        struct diagram_size size = size_of(layout, tick.label, DIAGRAM_TEXT_CAPTION);
        int x = day_x(layout, tick.day);
        emit(layout,
             (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_PATH,
                                        .stroke = DIAGRAM_SOLID,
                                        .ink = DIAGRAM_INK_NODE,
                                        .text = diagram_none,
                                        .points = {{x, layout->chart_top}, {x, layout->bottom}},
                                        .count = 2});
        emit(layout,
             (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                        .ink = DIAGRAM_INK_MUTED,
                                        .role = DIAGRAM_TEXT_CAPTION,
                                        .alignment = DIAGRAM_TEXT_LEFT,
                                        .text = tick.label,
                                        .bounds = {x, layout->axis_top, size.width, size.height}});
        if (x + size.width > layout->right)
            layout->right = x + size.width;
    }
}

static void emit_sections(struct diagram_gantt_layout *_Nonnull layout)
{
    size_t previous = diagram_none;
    for (size_t at = 0; at < diagram_model_node_count(layout->model); ++at)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, at);
        if (node.group == previous || node.group == diagram_none)
        {
            previous = node.group;
            continue;
        }
        previous = node.group;
        size_t label = diagram_model_group_at(layout->model, node.group).label;
        struct diagram_size size = size_of(layout, label, DIAGRAM_TEXT_BODY);
        emit(layout,
             (struct diagram_primitive){
                 .kind = DIAGRAM_PRIMITIVE_TEXT,
                 .ink = DIAGRAM_INK_TEXT,
                 .alignment = DIAGRAM_TEXT_LEFT,
                 .text = label,
                 .bounds = {diagram_outer_margin, row_top(layout, at), size.width, size.height}});
        if (row_top(layout, at) + size.height > layout->lowest)
            layout->lowest = row_top(layout, at) + size.height;
    }
}

/* The look of a bar: a step of the accent for its section; a done task pale with a muted edge;
 * crit a thick accent edge; active a thin accent edge. */
static struct diagram_primitive bar_style(struct diagram_node node)
{
    unsigned char shade = node.group == diagram_none ? 1 : (unsigned char)(1 + node.group % 3);
    struct diagram_primitive bar = {.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                    .shape = node.shape,
                                    .stroke = DIAGRAM_INVISIBLE,
                                    .ink = DIAGRAM_INK_ACCENT,
                                    .fill = DIAGRAM_INK_ACCENT,
                                    .shade = node.shape == DIAGRAM_DIAMOND ? 0 : shade,
                                    .text = diagram_none};
    switch (node.state)
    {
    case DIAGRAM_TASK_DONE:
        bar.fill = DIAGRAM_INK_NODE;
        bar.shade = 0;
        bar.ink = DIAGRAM_INK_MUTED;
        bar.stroke = DIAGRAM_SOLID;
        break;
    case DIAGRAM_TASK_ACTIVE:
        bar.stroke = DIAGRAM_SOLID;
        break;
    case DIAGRAM_TASK_PLANNED:
        break;
    }
    if (node.critical)
    {
        bar.ink = DIAGRAM_INK_ACCENT;
        bar.stroke = DIAGRAM_THICK;
    }
    return bar;
}

static struct diagram_rect bar_bounds(const struct diagram_gantt_layout *_Nonnull layout,
                                      struct diagram_node node, int top, int height)
{
    int y = top + (height - gantt_bar) / 2;
    if (node.shape == DIAGRAM_DIAMOND)
    {
        int middle = layout->chart_left + (int)(((int64_t)node.start * 2 + (int64_t)node.length) *
                                                layout->scale / (gantt_fraction * 2));
        return (struct diagram_rect){middle - gantt_bar / 2, y, gantt_bar, gantt_bar};
    }
    int left = day_x(layout, node.start);
    int width = day_x(layout, node.start + node.length) - left;
    return (struct diagram_rect){left, y, width < gantt_bar_minimum ? gantt_bar_minimum : width,
                                 gantt_bar};
}

static void emit_tasks(struct diagram_gantt_layout *_Nonnull layout)
{
    int top = layout->chart_top;
    for (size_t at = 0; at < diagram_model_node_count(layout->model); ++at)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, at);
        int height = row_height(layout, node);
        struct diagram_primitive bar = bar_style(node);
        bar.bounds = bar_bounds(layout, node, top, height);
        emit(layout, bar);
        struct diagram_size size = size_of(layout, node.label, DIAGRAM_TEXT_CAPTION);
        int x = bar.bounds.x + bar.bounds.width + gantt_label_gap;
        emit(layout, (struct diagram_primitive){
                         .kind = DIAGRAM_PRIMITIVE_TEXT,
                         .ink = DIAGRAM_INK_TEXT,
                         .role = DIAGRAM_TEXT_CAPTION,
                         .alignment = DIAGRAM_TEXT_LEFT,
                         .text = node.label,
                         .bounds = {x, top + (height - size.height) / 2, size.width, size.height}});
        if (x + size.width > layout->right)
            layout->right = x + size.width;
        top += height + gap_after(layout, at);
    }
    layout->bottom = top;
}

/* The rows first, so the grid lines know where the chart ends, then the rest on top. */
static void lay_out(struct diagram_gantt_layout *_Nonnull layout)
{
    measure(layout);
    layout->right = day_x(layout, span_of(layout->model));
    size_t title = diagram_model_title_text(layout->model);
    if (title != diagram_none &&
        diagram_outer_margin + size_of(layout, title, DIAGRAM_TEXT_BODY).width > layout->right)
        layout->right = diagram_outer_margin + size_of(layout, title, DIAGRAM_TEXT_BODY).width;
    size_t node_count = diagram_model_node_count(layout->model);
    layout->bottom = row_top(layout, node_count - 1) +
                     row_height(layout, diagram_model_node_at(layout->model, node_count - 1));
    emit_axis(layout);
    emit_sections(layout);
    emit_tasks(layout);
    if (layout->bottom > layout->lowest)
        layout->lowest = layout->bottom;
}

enum diagram_outcome diagram_gantt_layout_create(const struct diagram_model *_Nonnull model,
                                                 const struct diagram_metrics *_Nonnull metrics,
                                                 struct diagram_scene *_Nullable *_Nonnull out)
{
    if (diagram_model_node_count(model) == 0 || diagram_model_kind(model) != DIAGRAM_GANTT)
        return DIAGRAM_INVALID;
    struct diagram_scene *_Nullable scene = nullptr;
    enum diagram_outcome outcome = diagram_scene_create(&scene);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct diagram_gantt_layout layout = {
        .model = model, .metrics = metrics, .scene = scene, .outcome = DIAGRAM_ACCEPTED};
    lay_out(&layout);
    struct diagram_size whole = {layout.right + diagram_outer_margin,
                                 layout.lowest + diagram_outer_margin};
    emit_title(&layout, whole.width);
    if (layout.outcome == DIAGRAM_ACCEPTED)
        layout.outcome = diagram_scene_seal(scene, whole);
    if (layout.outcome != DIAGRAM_ACCEPTED)
    {
        diagram_scene_destroy(scene);
        return layout.outcome;
    }
    *out = scene;
    return DIAGRAM_ACCEPTED;
}
