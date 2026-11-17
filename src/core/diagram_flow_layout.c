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
    struct diagram_point starts[diagram_link_limit]; /* #916: centres of the texts at the ends */
    struct diagram_point ends[diagram_link_limit];
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
/* #888: an entity's box holds its name (the id) over its attributes (the label). */
static struct diagram_size node_text(const struct diagram_flow_layout *_Nonnull layout,
                                     struct diagram_node node)
{
    struct diagram_size text = diagram_metrics_at(layout->metrics, node.label, DIAGRAM_TEXT_BODY);
    if (node.shape != DIAGRAM_ENTITY)
        return text;
    struct diagram_size name = diagram_metrics_at(layout->metrics, node.id, DIAGRAM_TEXT_BODY);
    return (struct diagram_size){name.width > text.width ? name.width : text.width,
                                 name.height + text.height};
}
static void measure_nodes(struct diagram_flow_layout *_Nonnull layout)
{
    for (size_t index = 0; index < diagram_model_node_count(layout->model); ++index)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, index);
        struct diagram_size size =
            axis_size(layout, diagram_geometry_node(node_text(layout, node), node.shape));
        layout->nodes[index] = (struct diagram_rect){.width = size.width, .height = size.height};
        size_t rank = diagram_ranks_at(layout->ranks, index);
        if (layout->row_height[rank] < size.height)
            layout->row_height[rank] = size.height;
        layout->order[index] = layout->position[index] = index;
    }
}
/* #916: a text beside a line's end hangs past the node's middle, and two of them (one at each
 * end) share the space between two rows. */
static void measure_end_text(struct diagram_flow_layout *_Nonnull layout, size_t text)
{
    struct diagram_size size =
        axis_size(layout, diagram_metrics_at(layout->metrics, text, DIAGRAM_TEXT_CAPTION));
    if (size.width == 0)
        return;
    if (layout->margin < size.width + diagram_end_text_gap + diagram_outer_margin)
        layout->margin = size.width + diagram_end_text_gap + diagram_outer_margin;
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
        measure_end_text(layout, link.start_text);
        measure_end_text(layout, link.end_text);
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
    struct diagram_rect bounds = physical(layout, layout->groups[index]);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = DIAGRAM_ROUNDED,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_MUTED,
                                            .fill = DIAGRAM_INK_GROUP,
                                            .radius = diagram_group_radius,
                                            .bounds = bounds,
                                            .text = diagram_none});
}
static void emit_group_heading(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_group group = diagram_model_group_at(layout->model, index);
    struct diagram_rect bounds = physical(layout, layout->groups[index]);
    struct diagram_size size = diagram_metrics_at(layout->metrics, group.label, DIAGRAM_TEXT_BODY);
    bounds = (struct diagram_rect){bounds.x + diagram_group_title_inset,
                                   bounds.y + diagram_group_title_padding, size.width, size.height};
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .fill = DIAGRAM_INK_GROUP,
                                            .alignment = DIAGRAM_TEXT_LEFT,
                                            .bounds = bounds,
                                            .text = group.label});
}
/* #876: the end ring holds the same dot as the start, centred. */
static void emit_state_dot(struct diagram_flow_layout *_Nonnull layout, struct diagram_rect ring)
{
    int inset = (ring.width - diagram_state_dot) / 2;
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = DIAGRAM_STATE_START,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_ACCENT,
                                            .fill = DIAGRAM_INK_ACCENT,
                                            .bounds = {ring.x + inset, ring.y + inset,
                                                       diagram_state_dot, diagram_state_dot},
                                            .text = diagram_none});
}

/* #888: the name centred in its own band, a line under it, the attributes left-aligned below. */
static void emit_entity(struct diagram_flow_layout *_Nonnull layout, struct diagram_node node,
                        struct diagram_rect bounds)
{
    struct diagram_size name = diagram_metrics_at(layout->metrics, node.id, DIAGRAM_TEXT_BODY);
    int band = name.height + diagram_node_padding_y * 2;
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .bounds = {bounds.x + (bounds.width - name.width) / 2,
                                                       bounds.y + diagram_node_padding_y,
                                                       name.width, name.height},
                                            .text = node.id});
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_PATH,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_ACCENT,
                                            .points = {{bounds.x, bounds.y + band},
                                                       {bounds.x + bounds.width, bounds.y + band}},
                                            .count = 2,
                                            .text = diagram_none});
    emit(layout, (struct diagram_primitive){
                     .kind = DIAGRAM_PRIMITIVE_TEXT,
                     .ink = DIAGRAM_INK_TEXT,
                     .alignment = DIAGRAM_TEXT_LEFT,
                     .text_inset = diagram_node_padding_x,
                     .bounds = {bounds.x, bounds.y + band, bounds.width, bounds.height - band},
                     .text = node.label});
}
static void emit_node(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_node node = diagram_model_node_at(layout->model, index);
    struct diagram_rect bounds = physical(layout, layout->nodes[index]);
    bool dot = node.shape == DIAGRAM_STATE_START;
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = node.shape,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_ACCENT,
                                            .fill = dot ? DIAGRAM_INK_ACCENT : DIAGRAM_INK_NODE,
                                            .radius = diagram_node_radius,
                                            .bounds = bounds,
                                            .text = diagram_none});
    if (node.shape == DIAGRAM_STATE_END)
        emit_state_dot(layout, bounds);
    if (node.shape == DIAGRAM_ENTITY)
    {
        emit_entity(layout, node, bounds);
        return;
    }
    struct diagram_size size = diagram_metrics_at(layout->metrics, node.label, DIAGRAM_TEXT_BODY);
    bounds = diagram_geometry_label(bounds, size, node.shape);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .bounds = bounds,
                                            .text = node.label});
}
static bool has_edge_content(const struct diagram_flow_layout *_Nonnull layout,
                             struct diagram_link link)
{
    return link.stroke != DIAGRAM_INVISIBLE ||
           diagram_metrics_at(layout->metrics, link.label, DIAGRAM_TEXT_CAPTION).width != 0;
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
        if (!has_edge_content(layout, link) || channel_needed(layout, link, edge))
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
/* #890: the UML markers (triangle, diamonds) are as wide as an arrowhead and differ in meaning,
 * so the ones that share a node's edge sit side by side instead of on top of each other. */
static bool uml_marker(enum diagram_arrow arrow)
{
    switch (arrow)
    {
    case DIAGRAM_ARROW_NONE:
    case DIAGRAM_ARROW_CLOSED:
    case DIAGRAM_ARROW_OPEN:
    case DIAGRAM_ARROW_CIRCLE:
    case DIAGRAM_ARROW_CROSS:
        return false;
    case DIAGRAM_ARROW_HOLLOW:
    case DIAGRAM_ARROW_DIAMOND:
    case DIAGRAM_ARROW_DIAMOND_FILLED:
    case DIAGRAM_ARROW_ER_ONE:
    case DIAGRAM_ARROW_ER_ZERO_ONE:
    case DIAGRAM_ARROW_ER_MANY:
    case DIAGRAM_ARROW_ER_ZERO_MANY:
        return true; /* #898: the ER cardinalities are read at the entity's edge too */
    }
    return false;
}
/* #916: an end with a text beside it needs its own place on the edge just like a UML marker. */
static bool spread_end(const struct diagram_flow_layout *_Nonnull layout, struct diagram_link link,
                       bool start)
{
    size_t text = start ? link.start_text : link.end_text;
    return uml_marker(start ? link.start : link.end) ||
           diagram_metrics_at(layout->metrics, text, DIAGRAM_TEXT_CAPTION).width != 0;
}
static bool marks_node(const struct diagram_flow_layout *_Nonnull layout, size_t edge, size_t node,
                       bool start)
{
    struct diagram_link link = diagram_model_link_at(layout->model, edge);
    return (start ? link.from : link.to) == node && spread_end(layout, link, start) &&
           !channel_needed(layout, link, edge);
}
/* The widest text beside the ends that share this node's edge: they sit that much further
 * apart, so one line's text does not run into the next line. */
static int end_text_width(const struct diagram_flow_layout *_Nonnull layout, size_t node,
                          bool start)
{
    int widest = 0;
    for (size_t other = 0; other < diagram_model_link_count(layout->model); ++other)
    {
        struct diagram_link link = diagram_model_link_at(layout->model, other);
        if (!marks_node(layout, other, node, start))
            continue;
        int width = axis_size(layout, diagram_metrics_at(layout->metrics,
                                                         start ? link.start_text : link.end_text,
                                                         DIAGRAM_TEXT_CAPTION))
                        .width;
        if (widest < width)
            widest = width;
    }
    return widest;
}
static int marker_shift(const struct diagram_flow_layout *_Nonnull layout, size_t edge, bool start)
{
    struct diagram_link link = diagram_model_link_at(layout->model, edge);
    size_t node = start ? link.from : link.to;
    if (!spread_end(layout, link, start))
        return 0;
    int before = 0;
    int count = 0;
    for (size_t other = 0; other < diagram_model_link_count(layout->model); ++other)
    {
        if (!marks_node(layout, other, node, start))
            continue;
        before += other < edge ? 1 : 0;
        ++count;
    }
    int step = diagram_arrow_size * 2 + 4 + end_text_width(layout, node, start);
    int room = layout->nodes[node].width - diagram_node_padding_x * 2;
    if (count > 1 && step * (count - 1) > room)
        step = room / (count - 1);
    return (2 * before - (count - 1)) * step / 2;
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
    path->points[0] = (struct diagram_point){
        from.x + from.width / 2 + marker_shift(layout, edge, true), from.y + from.height};
    path->points[1] = (struct diagram_point){path->points[0].x, middle};
    path->points[2] =
        (struct diagram_point){to.x + to.width / 2 + marker_shift(layout, edge, false), middle};
    path->points[3] = (struct diagram_point){path->points[2].x, to.y};
    path->count = 4;
}
static struct diagram_point attach(const struct diagram_flow_layout *_Nonnull layout, size_t node,
                                   struct diagram_point point)
{
    return diagram_geometry_attachment(physical(layout, layout->nodes[node]),
                                       diagram_model_node_at(layout->model, node).shape, point);
}
/* #916: where the text beside an end goes: next to the line's first stretch, clear of the
 * marker, on the side the layout's cross axis grows towards. */
static struct diagram_point end_text_centre(const struct diagram_flow_layout *_Nonnull layout,
                                            size_t text, struct diagram_point end,
                                            struct diagram_point next)
{
    struct diagram_size size =
        axis_size(layout, diagram_metrics_at(layout->metrics, text, DIAGRAM_TEXT_CAPTION));
    int along_x = (next.x > end.x) - (next.x < end.x);
    int along_y = (next.y > end.y) - (next.y < end.y);
    struct diagram_point centre = {end.x + along_x * (diagram_end_text_gap + size.width / 2),
                                   end.y + along_y * (diagram_end_text_gap / 2 + size.height / 2)};
    if (along_x == 0)
        centre.x += diagram_end_text_gap + size.width / 2;
    else
        centre.y += diagram_end_text_gap + size.height / 2;
    return diagram_geometry_point(centre, layout->direction, layout->extent);
}
static void emit_link(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_link link = diagram_model_link_at(layout->model, index);
    if (!has_edge_content(layout, link))
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
    layout->labels[index] = diagram_geometry_point(label, layout->direction, layout->extent);
    layout->starts[index] =
        end_text_centre(layout, link.start_text, path.points[0], path.points[1]);
    layout->ends[index] = end_text_centre(layout, link.end_text, path.points[path.count - 1],
                                          path.points[path.count - 2]);
    if (link.stroke == DIAGRAM_INVISIBLE)
        return;
    for (size_t point = 0; point < path.count; ++point)
        path.points[point] =
            diagram_geometry_point(path.points[point], layout->direction, layout->extent);
    path.points[0] = attach(layout, link.from, path.points[0]);
    path.points[path.count - 1] = attach(layout, link.to, path.points[path.count - 1]);
    emit(layout, path);
}
static void emit_link_label(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_link link = diagram_model_link_at(layout->model, index);
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
static void emit_end_text(struct diagram_flow_layout *_Nonnull layout, size_t text,
                          struct diagram_point centre)
{
    struct diagram_size size = diagram_metrics_at(layout->metrics, text, DIAGRAM_TEXT_CAPTION);
    if (size.width == 0)
        return;
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .role = DIAGRAM_TEXT_CAPTION,
                                            .text = text,
                                            .bounds = {centre.x - size.width / 2,
                                                       centre.y - size.height / 2, size.width,
                                                       size.height}});
}
static void emit_end_texts(struct diagram_flow_layout *_Nonnull layout, size_t index)
{
    struct diagram_link link = diagram_model_link_at(layout->model, index);
    if (link.stroke == DIAGRAM_INVISIBLE)
        return;
    emit_end_text(layout, link.start_text, layout->starts[index]);
    emit_end_text(layout, link.end_text, layout->ends[index]);
}
static void reserve_channels(struct diagram_flow_layout *_Nonnull layout)
{
    size_t count = 0;
    int label_width = 0;
    for (size_t index = 0; index < diagram_model_link_count(layout->model); ++index)
    {
        struct diagram_link link = diagram_model_link_at(layout->model, index);
        if (has_edge_content(layout, link) && channel_needed(layout, link, index))
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
static void emit_scene(struct diagram_flow_layout *_Nonnull layout)
{
    for (size_t group = 0; group < diagram_model_group_count(layout->model); ++group)
        emit_group(layout, group);
    for (size_t link = 0; link < diagram_model_link_count(layout->model); ++link)
        emit_link(layout, link);
    for (size_t link = 0; link < diagram_model_link_count(layout->model); ++link)
        emit_link_label(layout, link);
    for (size_t link = 0; link < diagram_model_link_count(layout->model); ++link)
        emit_end_texts(layout, link);
    for (size_t group = 0; group < diagram_model_group_count(layout->model); ++group)
        emit_group_heading(layout, group);
    for (size_t node = 0; node < diagram_model_node_count(layout->model); ++node)
        emit_node(layout, node);
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
    emit_scene(layout);
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
