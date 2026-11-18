/* #955 / ADR0112: the layout of a pie. No trigonometry here: a sector is a circle's bounds with
 * a start and a sweep in hundredths of a degree, and the adapter turns them into the outline. */
#include "diagram_pie_layout.h"
#include "diagram_geometry.h"
#include "diagram_style.h"
constexpr int pie_diameter = 200;
constexpr int pie_legend_gap = 32;
constexpr int pie_swatch = 12;
constexpr int pie_swatch_gap = 8;
constexpr int pie_row_gap = 8;
constexpr int pie_title_gap = 12;
struct diagram_pie_layout
{
    const struct diagram_model *_Nonnull model;
    const struct diagram_metrics *_Nonnull metrics;
    struct diagram_scene *_Nonnull scene;
    enum diagram_outcome outcome;
    int top;          /* the top of the circle, below the title */
    int row;          /* the height of one legend row */
    int legend_width; /* the widest legend line */
};
static void emit(struct diagram_pie_layout *_Nonnull layout, struct diagram_primitive primitive)
{
    if (layout->outcome == DIAGRAM_ACCEPTED)
        layout->outcome = diagram_scene_append(layout->scene, &primitive);
}
/* The shades cycle in the order written. The last slice meets the first at the top, so when
 * the cycle would give it the first shade again it takes a middle one: its other neighbour has
 * the last shade. */
static unsigned char shade_of(const struct diagram_pie_layout *_Nonnull layout, size_t at)
{
    size_t shade = at % diagram_shade_count;
    if (shade == 0 && at != 0 && at + 1 == diagram_model_node_count(layout->model))
        shade = diagram_shade_count / 2;
    return (unsigned char)shade;
}
static void measure(struct diagram_pie_layout *_Nonnull layout)
{
    size_t title = diagram_model_title_text(layout->model);
    layout->top = diagram_outer_margin;
    if (title != diagram_none)
        layout->top +=
            diagram_metrics_at(layout->metrics, title, DIAGRAM_TEXT_BODY).height + pie_title_gap;
    layout->row = pie_swatch;
    for (size_t at = 0; at < diagram_model_node_count(layout->model); ++at)
    {
        struct diagram_size size = diagram_metrics_at(
            layout->metrics, diagram_model_node_at(layout->model, at).label, DIAGRAM_TEXT_BODY);
        if (size.height > layout->row)
            layout->row = size.height;
        if (size.width > layout->legend_width)
            layout->legend_width = size.width;
    }
}
static int legend_height(const struct diagram_pie_layout *_Nonnull layout)
{
    int count = (int)diagram_model_node_count(layout->model);
    return count * layout->row + (count - 1) * pie_row_gap;
}
static struct diagram_size extent(const struct diagram_pie_layout *_Nonnull layout)
{
    int legend = legend_height(layout);
    int body = legend > pie_diameter ? legend : pie_diameter;
    return (struct diagram_size){diagram_outer_margin + pie_diameter + pie_legend_gap + pie_swatch +
                                     pie_swatch_gap + layout->legend_width + diagram_outer_margin,
                                 layout->top + body + diagram_outer_margin};
}
static void emit_title(struct diagram_pie_layout *_Nonnull layout, struct diagram_size whole)
{
    size_t title = diagram_model_title_text(layout->model);
    if (title == diagram_none)
        return;
    struct diagram_size size = diagram_metrics_at(layout->metrics, title, DIAGRAM_TEXT_BODY);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .text = title,
                                            .bounds = {diagram_outer_margin, diagram_outer_margin,
                                                       whole.width - diagram_outer_margin * 2,
                                                       size.height}});
}
/* Clockwise from the top. The outline is in the background ink, so neighbours stay apart. */
static void emit_sectors(struct diagram_pie_layout *_Nonnull layout)
{
    int start = 0;
    for (size_t at = 0; at < diagram_model_node_count(layout->model); ++at)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, at);
        emit(layout, (struct diagram_primitive){
                         .kind = DIAGRAM_PRIMITIVE_SHAPE,
                         .shape = DIAGRAM_SECTOR,
                         .stroke = DIAGRAM_SOLID,
                         .ink = DIAGRAM_INK_BACKGROUND,
                         .fill = DIAGRAM_INK_ACCENT,
                         .shade = shade_of(layout, at),
                         .text = diagram_none,
                         .bounds = {diagram_outer_margin, layout->top, pie_diameter, pie_diameter},
                         .points = {{start, (int)node.share}},
                         .count = 1});
        start += (int)node.share;
    }
}
static void emit_legend(struct diagram_pie_layout *_Nonnull layout)
{
    int legend = legend_height(layout);
    int left = diagram_outer_margin + pie_diameter + pie_legend_gap;
    int top = layout->top + (legend < pie_diameter ? (pie_diameter - legend) / 2 : 0);
    for (size_t at = 0; at < diagram_model_node_count(layout->model); ++at)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, at);
        int y = top + (int)at * (layout->row + pie_row_gap);
        emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                                .shape = DIAGRAM_RECTANGLE,
                                                .stroke = DIAGRAM_SOLID,
                                                .ink = DIAGRAM_INK_ACCENT,
                                                .fill = DIAGRAM_INK_ACCENT,
                                                .shade = shade_of(layout, at),
                                                .text = diagram_none,
                                                .bounds = {left, y + (layout->row - pie_swatch) / 2,
                                                           pie_swatch, pie_swatch}});
        emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                                .ink = DIAGRAM_INK_TEXT,
                                                .alignment = DIAGRAM_TEXT_LEFT,
                                                .text = node.label,
                                                .bounds = {left + pie_swatch + pie_swatch_gap, y,
                                                           layout->legend_width, layout->row}});
    }
}
enum diagram_outcome diagram_pie_layout_create(const struct diagram_model *_Nonnull model,
                                               const struct diagram_metrics *_Nonnull metrics,
                                               struct diagram_scene *_Nullable *_Nonnull out)
{
    if (diagram_model_node_count(model) == 0 || diagram_model_kind(model) != DIAGRAM_PIE)
        return DIAGRAM_INVALID;
    struct diagram_scene *_Nullable scene = nullptr;
    enum diagram_outcome outcome = diagram_scene_create(&scene);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct diagram_pie_layout layout = {
        .model = model, .metrics = metrics, .scene = scene, .outcome = DIAGRAM_ACCEPTED};
    measure(&layout);
    struct diagram_size whole = extent(&layout);
    emit_title(&layout, whole);
    emit_sectors(&layout);
    emit_legend(&layout);
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
