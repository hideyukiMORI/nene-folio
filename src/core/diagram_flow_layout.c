#include "diagram_flow_layout.h"
#include "diagram_geometry.h"
#include "diagram_ranks.h"
#include "diagram_style.h"
#include <stdlib.h>
constexpr size_t rank_limit = diagram_node_limit * 16;
struct diagram_flow_layout
{
    const struct diagram_model *_Nonnull model;
    const struct diagram_metrics *_Nonnull metrics;
    const struct diagram_ranks *_Nonnull ranks;
    struct diagram_scene *_Nonnull scene;
    enum diagram_outcome outcome;
    enum diagram_direction direction;
    size_t order[diagram_node_limit];
    size_t position[diagram_node_limit];
    size_t sum[diagram_node_limit];
    size_t neighbors[diagram_node_limit];
    struct diagram_rect nodes[diagram_node_limit];
    struct diagram_rect groups[diagram_group_limit];
    int row_height[rank_limit + 1];
    int row_y[rank_limit + 1];
    int row_gap[rank_limit + 1];
    size_t tracks[rank_limit + 1];
    size_t edge_level[diagram_link_limit];
    struct diagram_point labels[diagram_link_limit];
    int direct_width[diagram_group_limit + 1];
    int band_width[diagram_group_limit + 1];
    int band_x[diagram_group_limit + 1];
    int band_used[diagram_group_limit + 1];
    int gap;
    int group_before;
    int group_after;
    int content_width;
    int margin;
    int label_unit;
    int channel_width;
    size_t channels;
    struct diagram_size extent;
};
static size_t lane(const struct diagram_flow_layout *_Nonnull layout, size_t node)
{
    size_t group = diagram_model_node_at(layout->model, node).group;
    return group == diagram_none ? 0 : group + 1;
}
static size_t parent_lane(const struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    size_t parent = diagram_model_group_at(layout->model, group).parent;
    return parent == diagram_none ? 0 : parent + 1;
}
static struct diagram_size axis_size(const struct diagram_flow_layout *_Nonnull layout,
                                     struct diagram_size size)
{
    return diagram_geometry_extent(size, layout->direction);
}
static int group_heading(const struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    size_t label = diagram_model_group_at(layout->model, group).label;
    return diagram_metrics_at(layout->metrics, label, DIAGRAM_TEXT_BODY).height +
           diagram_group_title_padding * 2;
}
static int cross_start(const struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    switch (layout->direction)
    {
    case DIAGRAM_TOP_BOTTOM:
    case DIAGRAM_BOTTOM_TOP:
        return diagram_group_padding;
    case DIAGRAM_LEFT_RIGHT:
    case DIAGRAM_RIGHT_LEFT:
        return group_heading(layout, group);
    }
    return diagram_group_padding;
}
static int main_start(const struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    switch (layout->direction)
    {
    case DIAGRAM_TOP_BOTTOM:
        return group_heading(layout, group);
    case DIAGRAM_BOTTOM_TOP:
    case DIAGRAM_LEFT_RIGHT:
    case DIAGRAM_RIGHT_LEFT:
        return diagram_group_padding;
    }
    return diagram_group_padding;
}
static int main_end(const struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    switch (layout->direction)
    {
    case DIAGRAM_BOTTOM_TOP:
        return group_heading(layout, group);
    case DIAGRAM_TOP_BOTTOM:
    case DIAGRAM_LEFT_RIGHT:
    case DIAGRAM_RIGHT_LEFT:
        return diagram_group_padding;
    }
    return diagram_group_padding;
}
static void emit(struct diagram_flow_layout *_Nonnull layout, struct diagram_primitive primitive)
{
    if (layout->outcome == DIAGRAM_ACCEPTED)
        layout->outcome = diagram_scene_append(layout->scene, &primitive);
}
static void measure_nodes(struct diagram_flow_layout *_Nonnull layout)
{
    for (size_t index = 0; index < diagram_model_node_count(layout->model); ++index)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, index);
        struct diagram_size size =
            axis_size(layout, diagram_geometry_node(diagram_metrics_at(layout->metrics, node.label,
                                                                       DIAGRAM_TEXT_BODY),
                                                    node.shape));
        layout->nodes[index] = (struct diagram_rect){.width = size.width, .height = size.height};
        size_t rank = diagram_ranks_at(layout->ranks, index);
        if (layout->row_height[rank] < size.height)
            layout->row_height[rank] = size.height;
        layout->order[index] = layout->position[index] = index;
    }
}
static void measure_edge_labels(struct diagram_flow_layout *_Nonnull layout)
{
    layout->gap = diagram_flow_gap;
    layout->margin = diagram_outer_margin;
    layout->label_unit = 32;
    layout->channel_width = 32;
    for (size_t index = 0; index < diagram_model_link_count(layout->model); ++index)
    {
        struct diagram_link link = diagram_model_link_at(layout->model, index);
        struct diagram_size size = axis_size(
            layout, diagram_metrics_at(layout->metrics, link.label, DIAGRAM_TEXT_CAPTION));
        if (layout->gap < size.height + 32)
            layout->gap = size.height + 32;
        if (layout->margin < size.width / 2 + diagram_outer_margin)
            layout->margin = size.width / 2 + diagram_outer_margin;
        if (layout->label_unit < size.height + 16)
            layout->label_unit = size.height + 16;
        if (layout->channel_width < size.width + 24)
            layout->channel_width = size.width + 24;
    }
}
static void measure_labels(struct diagram_flow_layout *_Nonnull layout)
{
    measure_edge_labels(layout);
    for (size_t index = 0; index < diagram_model_group_count(layout->model); ++index)
    {
        struct diagram_group group = diagram_model_group_at(layout->model, index);
        int before = main_start(layout, index), after = main_end(layout, index);
        if (layout->group_before < before)
            layout->group_before = before;
        if (layout->group_after < after)
            layout->group_after = after;
        if (group.direction != layout->direction)
            layout->outcome = DIAGRAM_UNSUPPORTED;
    }
}
static void neighbor_scores(struct diagram_flow_layout *_Nonnull layout, size_t sweep)
{
    for (size_t index = 0; index < diagram_model_node_count(layout->model); ++index)
    {
        layout->sum[index] = layout->position[index];
        layout->neighbors[index] = 1;
    }
    for (size_t index = 0; index < diagram_model_link_count(layout->model); ++index)
    {
        struct diagram_link link = diagram_model_link_at(layout->model, index);
        size_t earlier = link.from;
        size_t later = link.to;
        if (diagram_ranks_reversed(layout->ranks, index))
        {
            earlier = link.to;
            later = link.from;
        }
        size_t node = sweep % 2 == 0 ? later : earlier;
        size_t neighbor = sweep % 2 == 0 ? earlier : later;
        layout->sum[node] += layout->position[neighbor];
        ++layout->neighbors[node];
    }
}
static bool follows(const struct diagram_flow_layout *_Nonnull layout, size_t first, size_t second)
{
    return lane(layout, first) == lane(layout, second) &&
           diagram_ranks_at(layout->ranks, first) == diagram_ranks_at(layout->ranks, second) &&
           layout->sum[first] * layout->neighbors[second] >
               layout->sum[second] * layout->neighbors[first];
}
static void reorder(struct diagram_flow_layout *_Nonnull layout)
{
    size_t count = diagram_model_node_count(layout->model);
    for (size_t first = 0; first < count; ++first)
        for (size_t next = first + 1; next < count; ++next)
            if (follows(layout, layout->order[first], layout->order[next]))
            {
                size_t saved = layout->order[first];
                layout->order[first] = layout->order[next];
                layout->order[next] = saved;
            }
    for (size_t index = 0; index < count; ++index)
        layout->position[layout->order[index]] = index;
}
static int row_width(const struct diagram_flow_layout *_Nonnull layout, size_t group, size_t rank)
{
    int width = 0;
    for (size_t index = 0; index < diagram_model_node_count(layout->model); ++index)
        if (lane(layout, index) == group && diagram_ranks_at(layout->ranks, index) == rank)
            width += layout->nodes[index].width + diagram_node_gap;
    return width == 0 ? 0 : width - diagram_node_gap;
}
static void measure_direct(struct diagram_flow_layout *_Nonnull layout)
{
    size_t count = diagram_model_group_count(layout->model);
    for (size_t node = 0; node < diagram_model_node_count(layout->model); ++node)
    {
        size_t group = lane(layout, node);
        int width = row_width(layout, group, diagram_ranks_at(layout->ranks, node));
        if (layout->direct_width[group] < width)
            layout->direct_width[group] = width;
    }
    for (size_t group = 0; group <= count; ++group)
        layout->band_width[group] = layout->direct_width[group];
}
static void collect_group_widths(struct diagram_flow_layout *_Nonnull layout)
{
    size_t count = diagram_model_group_count(layout->model);
    for (size_t group = count; group != 0; --group)
    {
        struct diagram_group record = diagram_model_group_at(layout->model, group - 1);
        int label =
            axis_size(layout, diagram_metrics_at(layout->metrics, record.label, DIAGRAM_TEXT_BODY))
                .width;
        if (layout->band_width[group] < label)
            layout->band_width[group] = label;
        layout->band_width[group] += cross_start(layout, group - 1) + diagram_group_padding;
        size_t parent = parent_lane(layout, group - 1);
        if (layout->band_width[parent] != 0)
            layout->band_width[parent] += diagram_node_gap;
        layout->band_width[parent] += layout->band_width[group];
    }
}
static void place_bands(struct diagram_flow_layout *_Nonnull layout)
{
    size_t count = diagram_model_group_count(layout->model);
    layout->band_x[0] = layout->margin;
    for (size_t group = 0; group <= count; ++group)
    {
        layout->band_used[group] = layout->direct_width[group];
        if (group == 0)
            continue;
        size_t parent = parent_lane(layout, group - 1);
        int padding = parent == 0 ? 0 : cross_start(layout, parent - 1);
        int gap = layout->band_used[parent] == 0 ? 0 : diagram_node_gap;
        layout->band_x[group] = layout->band_x[parent] + padding + layout->band_used[parent] + gap;
        layout->band_used[parent] += gap + layout->band_width[group];
    }
    layout->content_width = layout->band_width[0] + layout->margin * 2;
}
static void measure_bands(struct diagram_flow_layout *_Nonnull layout)
{
    measure_direct(layout);
    collect_group_widths(layout);
    place_bands(layout);
}
static size_t group_depth(const struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    size_t depth = 0;
    while (group != diagram_none)
    {
        ++depth;
        group = diagram_model_group_at(layout->model, group).parent;
    }
    return depth;
}
static void place_rows(struct diagram_flow_layout *_Nonnull layout)
{
    size_t depth = 0;
    for (size_t group = 0; group < diagram_model_group_count(layout->model); ++group)
    {
        size_t candidate = group_depth(layout, group);
        if (candidate > depth)
            depth = candidate;
    }
    int y = layout->gap / 2 + diagram_outer_margin + (int)depth * layout->group_before;
    size_t maximum = diagram_ranks_maximum(layout->ranks);
    for (size_t rank = 0; rank <= maximum; ++rank)
    {
        layout->row_y[rank] = y;
        y += layout->row_height[rank] + layout->row_gap[rank];
    }
    layout->extent.height = y - layout->row_gap[maximum] + layout->gap / 2 +
                            (int)depth * layout->group_after + diagram_outer_margin;
}
static void place_node(struct diagram_flow_layout *_Nonnull layout, size_t at)
{
    size_t node = layout->order[at];
    size_t group = lane(layout, node);
    size_t rank = diagram_ranks_at(layout->ranks, node);
    int x = layout->band_x[group] + (group == 0 ? 0 : cross_start(layout, group - 1));
    x += (layout->direct_width[group] - row_width(layout, group, rank)) / 2;
    for (size_t index = 0; index < at; ++index)
    {
        size_t before = layout->order[index];
        if (lane(layout, before) == group && diagram_ranks_at(layout->ranks, before) == rank)
            x += layout->nodes[before].width + diagram_node_gap;
    }
    layout->nodes[node].x = x;
    layout->nodes[node].y =
        layout->row_y[rank] + (layout->row_height[rank] - layout->nodes[node].height) / 2;
}
static void include_y(struct diagram_rect box, int *_Nonnull top, int *_Nonnull bottom)
{
    if (*top > box.y)
        *top = box.y;
    if (*bottom < box.y + box.height)
        *bottom = box.y + box.height;
}
static void place_group(struct diagram_flow_layout *_Nonnull layout, size_t group)
{
    int top = layout->extent.height;
    int bottom = 0;
    for (size_t node = 0; node < diagram_model_node_count(layout->model); ++node)
        if (lane(layout, node) == group + 1)
            include_y(layout->nodes[node], &top, &bottom);
    for (size_t child = group + 1; child < diagram_model_group_count(layout->model); ++child)
        if (parent_lane(layout, child) == group + 1)
            include_y(layout->groups[child], &top, &bottom);
    if (bottom == 0)
    {
        top = layout->row_y[0];
        bottom = top + 40;
    }
    struct diagram_group record = diagram_model_group_at(layout->model, group);
    int title_extent =
        axis_size(layout, diagram_metrics_at(layout->metrics, record.label, DIAGRAM_TEXT_BODY))
            .height +
        diagram_group_padding * 2;
    int before = main_start(layout, group), after = main_end(layout, group);
    if (bottom - top + before + after < title_extent)
        bottom = top + title_extent - before - after;
    layout->groups[group] =
        (struct diagram_rect){layout->band_x[group + 1], top - before,
                              layout->band_width[group + 1], bottom - top + before + after};
    if (layout->extent.height < bottom + after + diagram_outer_margin)
        layout->extent.height = bottom + after + diagram_outer_margin;
}
static struct diagram_rect physical(const struct diagram_flow_layout *_Nonnull layout,
                                    struct diagram_rect bounds)
{
    return diagram_geometry_rect(bounds, layout->direction, layout->extent);
}
static void emit_group(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_group group = diagram_model_group_at(layout->model, index);
    struct diagram_rect bounds = physical(layout, layout->groups[index]);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = DIAGRAM_ROUNDED,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_MUTED,
                                            .fill = DIAGRAM_INK_GROUP,
                                            .radius = diagram_group_radius,
                                            .bounds = bounds,
                                            .text = diagram_none});
    struct diagram_size size = diagram_metrics_at(layout->metrics, group.label, DIAGRAM_TEXT_BODY);
    bounds = (struct diagram_rect){bounds.x + diagram_group_title_inset,
                                   bounds.y + diagram_group_title_padding,
                                   bounds.width - diagram_group_title_inset * 2, size.height};
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .alignment = DIAGRAM_TEXT_LEFT,
                                            .bounds = bounds,
                                            .text = group.label});
}
static void emit_node(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_node node = diagram_model_node_at(layout->model, index);
    struct diagram_rect bounds = physical(layout, layout->nodes[index]);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = node.shape,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_ACCENT,
                                            .fill = DIAGRAM_INK_NODE,
                                            .radius = diagram_node_radius,
                                            .bounds = bounds,
                                            .text = diagram_none});
    struct diagram_size size = diagram_metrics_at(layout->metrics, node.label, DIAGRAM_TEXT_BODY);
    bounds = diagram_geometry_label(bounds, size, node.shape);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .bounds = bounds,
                                            .text = node.label});
}
static bool parallel_edge(const struct diagram_flow_layout *_Nonnull layout,
                          struct diagram_link link, size_t edge)
{
    for (size_t index = 0; index < edge; ++index)
    {
        struct diagram_link before = diagram_model_link_at(layout->model, index);
        if (before.from == link.from && before.to == link.to)
            return true;
    }
    return false;
}
static bool channel_needed(const struct diagram_flow_layout *_Nonnull layout,
                           struct diagram_link link, size_t edge)
{
    size_t from = diagram_ranks_at(layout->ranks, link.from);
    size_t to = diagram_ranks_at(layout->ranks, link.to);
    return from >= to || to - from > 1 || parallel_edge(layout, link, edge);
}
static void reserve_tracks(struct diagram_flow_layout *_Nonnull layout)
{
    for (size_t rank = 0; rank <= diagram_ranks_maximum(layout->ranks); ++rank)
        layout->row_gap[rank] = layout->gap;
    for (size_t edge = 0; edge < diagram_model_link_count(layout->model); ++edge)
    {
        struct diagram_link link = diagram_model_link_at(layout->model, edge);
        if (link.stroke == DIAGRAM_INVISIBLE || channel_needed(layout, link, edge))
            continue;
        size_t rank = diagram_ranks_at(layout->ranks, link.from);
        layout->edge_level[edge] = layout->tracks[rank]++;
        int required = 32 + (int)layout->tracks[rank] * layout->label_unit;
        if (layout->row_gap[rank] < required)
            layout->row_gap[rank] = required;
    }
}
static void channel_path(struct diagram_flow_layout *_Nonnull layout, struct diagram_link link,
                         struct diagram_primitive *_Nonnull path)
{
    struct diagram_rect from = layout->nodes[link.from];
    struct diagram_rect to = layout->nodes[link.to];
    int outside = layout->content_width + layout->channel_width / 2 +
                  (int)layout->channels++ * layout->channel_width;
    size_t from_rank = diagram_ranks_at(layout->ranks, link.from);
    size_t to_rank = diagram_ranks_at(layout->ranks, link.to);
    bool forward =
        diagram_ranks_at(layout->ranks, link.from) <= diagram_ranks_at(layout->ranks, link.to);
    int start_y = forward ? from.y + from.height : from.y;
    int end_y = forward ? to.y : to.y + to.height;
    int start_gap = layout->row_y[from_rank] +
                    (forward ? layout->row_height[from_rank] + layout->gap / 3 : -layout->gap / 3);
    int end_gap = layout->row_y[to_rank] +
                  (forward ? -layout->gap / 3 : layout->row_height[to_rank] + layout->gap / 3);
    path->points[0] = (struct diagram_point){from.x + from.width / 2, start_y};
    path->points[1] = (struct diagram_point){path->points[0].x, start_gap};
    path->points[2] = (struct diagram_point){outside, start_gap};
    path->points[3] = (struct diagram_point){outside, end_gap};
    path->points[4] = (struct diagram_point){to.x + to.width / 2, end_gap};
    path->points[5] = (struct diagram_point){path->points[4].x, end_y};
    path->count = 6;
}
static void forward_path(const struct diagram_flow_layout *_Nonnull layout, size_t edge,
                         struct diagram_primitive *_Nonnull path)
{
    struct diagram_link link = diagram_model_link_at(layout->model, edge);
    struct diagram_rect from = layout->nodes[link.from];
    struct diagram_rect to = layout->nodes[link.to];
    size_t rank = diagram_ranks_at(layout->ranks, link.from);
    int middle = layout->row_y[rank] + layout->row_height[rank] + 16 +
                 (int)layout->edge_level[edge] * layout->label_unit + layout->label_unit / 2;
    path->points[0] = (struct diagram_point){from.x + from.width / 2, from.y + from.height};
    path->points[1] = (struct diagram_point){path->points[0].x, middle};
    path->points[2] = (struct diagram_point){to.x + to.width / 2, middle};
    path->points[3] = (struct diagram_point){path->points[2].x, to.y};
    path->count = 4;
}
static struct diagram_point attach(const struct diagram_flow_layout *_Nonnull layout, size_t node,
                                   struct diagram_point point)
{
    return diagram_geometry_attachment(physical(layout, layout->nodes[node]),
                                       diagram_model_node_at(layout->model, node).shape, point);
}
static void emit_link(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_link link = diagram_model_link_at(layout->model, index);
    if (link.stroke == DIAGRAM_INVISIBLE)
        return;
    struct diagram_primitive path = {.kind = DIAGRAM_PRIMITIVE_PATH,
                                     .stroke = link.stroke,
                                     .start = link.start,
                                     .end = link.end,
                                     .ink = DIAGRAM_INK_TEXT,
                                     .text = diagram_none};
    if (channel_needed(layout, link, index))
        channel_path(layout, link, &path);
    else
        forward_path(layout, index, &path);
    size_t segment = path.count == 6 ? 2 : 1;
    struct diagram_point label = {(path.points[segment].x + path.points[segment + 1].x) / 2,
                                  (path.points[segment].y + path.points[segment + 1].y) / 2};
    for (size_t point = 0; point < path.count; ++point)
        path.points[point] =
            diagram_geometry_point(path.points[point], layout->direction, layout->extent);
    path.points[0] = attach(layout, link.from, path.points[0]);
    path.points[path.count - 1] = attach(layout, link.to, path.points[path.count - 1]);
    emit(layout, path);
    layout->labels[index] = diagram_geometry_point(label, layout->direction, layout->extent);
}
static void emit_link_label(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_link link = diagram_model_link_at(layout->model, index);
    if (link.stroke == DIAGRAM_INVISIBLE)
        return;
    struct diagram_size size =
        diagram_metrics_at(layout->metrics, link.label, DIAGRAM_TEXT_CAPTION);
    if (size.width == 0)
        return;
    struct diagram_point label = layout->labels[index];
    emit(layout,
         (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                    .ink = DIAGRAM_INK_TEXT,
                                    .fill = DIAGRAM_INK_BACKGROUND,
                                    .role = DIAGRAM_TEXT_CAPTION,
                                    .text = link.label,
                                    .bounds = {label.x - size.width / 2 - diagram_edge_padding_x,
                                               label.y - size.height / 2 - diagram_edge_padding_y,
                                               size.width + diagram_edge_padding_x * 2,
                                               size.height + diagram_edge_padding_y * 2}});
}
static void reserve_channels(struct diagram_flow_layout *_Nonnull layout)
{
    size_t count = 0;
    int label_width = 0;
    for (size_t index = 0; index < diagram_model_link_count(layout->model); ++index)
    {
        struct diagram_link link = diagram_model_link_at(layout->model, index);
        if (link.stroke != DIAGRAM_INVISIBLE && channel_needed(layout, link, index))
            ++count;
        int width =
            axis_size(layout, diagram_metrics_at(layout->metrics, link.label, DIAGRAM_TEXT_CAPTION))
                .width;
        if (label_width < width)
            label_width = width;
    }
    layout->extent.width = layout->content_width + (int)count * layout->channel_width +
                           label_width / 2 + diagram_outer_margin * 2;
}
static enum diagram_outcome construct(struct diagram_flow_layout *_Nonnull layout)
{
    measure_nodes(layout);
    measure_labels(layout);
    for (size_t sweep = 0; sweep < 6; ++sweep)
    {
        neighbor_scores(layout, sweep);
        reorder(layout);
    }
    measure_bands(layout);
    reserve_tracks(layout);
    place_rows(layout);
    reserve_channels(layout);
    if (layout->extent.width > diagram_coordinate_limit ||
        layout->extent.height > diagram_coordinate_limit)
        return DIAGRAM_TOO_LARGE;
    for (size_t node = 0; node < diagram_model_node_count(layout->model); ++node)
        place_node(layout, node);
    for (size_t group = diagram_model_group_count(layout->model); group != 0; --group)
        place_group(layout, group - 1);
    for (size_t group = 0; group < diagram_model_group_count(layout->model); ++group)
        emit_group(layout, group);
    for (size_t link = 0; link < diagram_model_link_count(layout->model); ++link)
        emit_link(layout, link);
    for (size_t link = 0; link < diagram_model_link_count(layout->model); ++link)
        emit_link_label(layout, link);
    for (size_t node = 0; node < diagram_model_node_count(layout->model); ++node)
        emit_node(layout, node);
    if (layout->outcome == DIAGRAM_ACCEPTED)
        layout->outcome = diagram_scene_seal(layout->scene, axis_size(layout, layout->extent));
    return layout->outcome;
}
static void workspace_destroy(struct diagram_flow_layout *_Nullable layout)
{
    free(layout);
}
enum diagram_outcome diagram_flow_layout_create(const struct diagram_model *_Nonnull model,
                                                const struct diagram_metrics *_Nonnull metrics,
                                                struct diagram_scene *_Nullable *_Nonnull out)
{
    struct diagram_ranks *_Nullable ranks = nullptr;
    enum diagram_outcome outcome = diagram_ranks_create(model, &ranks);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct diagram_scene *_Nullable scene = nullptr;
    outcome = diagram_scene_create(&scene);
    struct diagram_flow_layout *_Nullable layout = calloc(1, sizeof *layout);
    if (layout == nullptr)
        outcome = DIAGRAM_OUT_OF_MEMORY;
    if (outcome == DIAGRAM_ACCEPTED)
    {
        layout->model = model;
        layout->metrics = metrics;
        layout->ranks = ranks;
        layout->scene = scene;
        layout->direction = diagram_model_flow(model);
        layout->outcome = DIAGRAM_ACCEPTED;
        outcome = construct(layout);
    }
    workspace_destroy(layout);
    diagram_ranks_destroy(ranks);
    if (outcome != DIAGRAM_ACCEPTED)
    {
        diagram_scene_destroy(scene);
        return outcome;
    }
    *out = scene;
    return DIAGRAM_ACCEPTED;
}
