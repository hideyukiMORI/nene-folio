#include "diagram_sequence_layout.h"
#include "diagram_geometry.h"
#include "diagram_style.h"
#include <stdlib.h>
struct diagram_sequence_layout
{
    const struct diagram_model *_Nonnull model;
    const struct diagram_metrics *_Nonnull metrics;
    struct diagram_scene *_Nonnull scene;
    enum diagram_outcome outcome;
    struct diagram_size actors[diagram_node_limit];
    int left[diagram_node_limit];
    int right[diagram_node_limit];
    int x[diagram_node_limit];
    int y[diagram_event_limit];
    int end[diagram_event_limit];
    size_t depth[diagram_event_limit];
    size_t active[diagram_node_limit][diagram_depth_limit];
    size_t active_count[diagram_node_limit];
    size_t frames[diagram_depth_limit];
    size_t frame_count;
    size_t maximum_depth;
    int header_y;
    int header_height;
    int bottom;
    int content_left;
    int content_right;
    struct diagram_size extent;
};
static struct diagram_size event_size(const struct diagram_sequence_layout *_Nonnull layout,
                                      struct diagram_event event)
{
    switch (event.kind)
    {
    case DIAGRAM_MESSAGE:
    case DIAGRAM_NOTE:
    case DIAGRAM_ACTIVATE:
    case DIAGRAM_DEACTIVATE:
        return diagram_metrics_at(layout->metrics, event.link.label, DIAGRAM_TEXT_BODY);
    case DIAGRAM_FRAME_OPEN:
    case DIAGRAM_FRAME_SECTION:
    case DIAGRAM_FRAME_CLOSE:
        return diagram_metrics_at(layout->metrics, event.link.label, DIAGRAM_TEXT_CAPTION);
    }
    return (struct diagram_size){0, 0};
}
static void emit(struct diagram_sequence_layout *_Nonnull layout,
                 struct diagram_primitive primitive)
{
    if (layout->outcome == DIAGRAM_ACCEPTED)
        layout->outcome = diagram_scene_append(layout->scene, &primitive);
}
static void reserve(int *_Nonnull extent, int required)
{
    if (*extent < required)
        *extent = required;
}
static void measure_actors(struct diagram_sequence_layout *_Nonnull layout)
{
    layout->header_y = diagram_outer_margin;
    size_t title = diagram_model_title_text(layout->model);
    if (title != diagram_none)
        layout->header_y += diagram_metrics_at(layout->metrics, title, DIAGRAM_TEXT_BODY).height +
                            diagram_outer_margin;
    for (size_t actor = 0; actor < diagram_model_node_count(layout->model); ++actor)
    {
        struct diagram_node node = diagram_model_node_at(layout->model, actor);
        struct diagram_size size = diagram_geometry_node(
            diagram_metrics_at(layout->metrics, node.label, DIAGRAM_TEXT_BODY), node.shape);
        if (node.shape != DIAGRAM_ACTOR && size.width < diagram_participant_min_width)
            size.width = diagram_participant_min_width;
        layout->actors[actor] = size;
        layout->left[actor] = layout->right[actor] = (size.width + 1) / 2;
        reserve(&layout->header_height, size.height);
    }
}
static void reserve_note(struct diagram_sequence_layout *_Nonnull layout,
                         struct diagram_event event, struct diagram_size size)
{
    if (event.link.from != event.link.to)
        return;
    switch (event.side)
    {
    case DIAGRAM_NOTE_LEFT:
        reserve(&layout->left[event.link.from], size.width + 40);
        break;
    case DIAGRAM_NOTE_RIGHT:
        reserve(&layout->right[event.link.from], size.width + 40);
        break;
    case DIAGRAM_NOTE_OVER:
        reserve(&layout->left[event.link.from], size.width / 2 + 16);
        reserve(&layout->right[event.link.from], size.width / 2 + 16);
        break;
    }
}
static void measure_event(struct diagram_sequence_layout *_Nonnull layout,
                          struct diagram_event event)
{
    struct diagram_size size = event_size(layout, event);
    if (event.kind == DIAGRAM_NOTE)
        reserve_note(layout, event, size);
    if (event.kind == DIAGRAM_MESSAGE && event.link.from == event.link.to)
        reserve(&layout->right[event.link.from], size.width + 64);
}
static void measure_events(struct diagram_sequence_layout *_Nonnull layout)
{
    size_t depth = 0;
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
    {
        struct diagram_event event = diagram_model_event_at(layout->model, index);
        measure_event(layout, event);
        if (event.kind == DIAGRAM_FRAME_OPEN)
        {
            ++depth;
            if (layout->maximum_depth < depth)
                layout->maximum_depth = depth;
        }
        if (event.kind == DIAGRAM_FRAME_CLOSE && depth != 0)
            --depth;
    }
}
static void widen_span(struct diagram_sequence_layout *_Nonnull layout, struct diagram_event event)
{
    if (event.link.from == event.link.to)
        return;
    size_t first = event.link.from < event.link.to ? event.link.from : event.link.to;
    size_t last = event.link.from > event.link.to ? event.link.from : event.link.to;
    int width = event_size(layout, event).width + 32;
    int missing = width - (layout->x[last] - layout->x[first]);
    if (missing <= 0)
        return;
    int step = (missing + (int)(last - first) - 1) / (int)(last - first);
    for (size_t actor = first + 1; actor < diagram_model_node_count(layout->model); ++actor)
    {
        size_t steps = actor < last ? actor - first : last - first;
        layout->x[actor] += step * (int)steps;
    }
}
static void place_actors(struct diagram_sequence_layout *_Nonnull layout)
{
    int margin = diagram_outer_margin + (int)layout->maximum_depth * 12;
    int x = margin;
    size_t count = diagram_model_node_count(layout->model);
    for (size_t actor = 0; actor < count; ++actor)
    {
        layout->x[actor] = x + layout->left[actor];
        x += layout->left[actor] + layout->right[actor] + 40;
    }
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
    {
        struct diagram_event event = diagram_model_event_at(layout->model, index);
        if (event.kind == DIAGRAM_MESSAGE || event.kind == DIAGRAM_NOTE)
            widen_span(layout, event);
    }
    layout->content_left = margin;
    layout->content_right = layout->x[count - 1] + layout->right[count - 1];
    layout->extent.width = layout->content_right + margin;
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
    {
        struct diagram_event event = diagram_model_event_at(layout->model, index);
        if (event.kind == DIAGRAM_FRAME_OPEN || event.kind == DIAGRAM_FRAME_SECTION)
            reserve(&layout->extent.width, event_size(layout, event).width + margin * 2 + 32);
    }
    size_t title = diagram_model_title_text(layout->model);
    if (title != diagram_none)
        reserve(&layout->extent.width,
                diagram_metrics_at(layout->metrics, title, DIAGRAM_TEXT_BODY).width +
                    diagram_outer_margin * 2);
}
static void activate(struct diagram_sequence_layout *_Nonnull layout, size_t index, int anchor)
{
    struct diagram_event event = diagram_model_event_at(layout->model, index);
    size_t actor = event.link.from;
    layout->y[index] = anchor;
    if (event.kind == DIAGRAM_ACTIVATE)
    {
        size_t depth = layout->active_count[actor];
        if (depth == diagram_depth_limit)
        {
            layout->outcome = DIAGRAM_TOO_LARGE;
            return;
        }
        layout->depth[index] = depth;
        layout->active[actor][layout->active_count[actor]++] = index;
    }
    if (event.kind == DIAGRAM_DEACTIVATE)
    {
        if (layout->active_count[actor] == 0)
        {
            layout->outcome = DIAGRAM_INVALID;
            return;
        }
        size_t opening = layout->active[actor][--layout->active_count[actor]];
        layout->end[opening] = anchor;
    }
}
static void frame(struct diagram_sequence_layout *_Nonnull layout, size_t index, int *_Nonnull y)
{
    struct diagram_event event = diagram_model_event_at(layout->model, index);
    layout->y[index] = *y;
    if (event.kind == DIAGRAM_FRAME_OPEN)
    {
        if (layout->frame_count == diagram_depth_limit)
        {
            layout->outcome = DIAGRAM_TOO_LARGE;
            return;
        }
        layout->depth[index] = layout->frame_count;
        layout->frames[layout->frame_count++] = index;
    }
    if (event.kind == DIAGRAM_FRAME_CLOSE)
    {
        if (layout->frame_count == 0)
        {
            layout->outcome = DIAGRAM_INVALID;
            return;
        }
        layout->end[layout->frames[--layout->frame_count]] = *y + 8;
        *y += 24;
        return;
    }
    if (event.kind == DIAGRAM_FRAME_SECTION)
        layout->depth[index] = layout->frame_count == 0 ? 0 : layout->frame_count - 1;
    *y +=
        event_size(layout, event).height + diagram_frame_padding_y * 2 + diagram_frame_heading_gap;
}
static void place_events(struct diagram_sequence_layout *_Nonnull layout)
{
    int y = layout->header_y + layout->header_height + 32;
    int anchor = y;
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
    {
        struct diagram_event event = diagram_model_event_at(layout->model, index);
        int height = event_size(layout, event).height;
        layout->y[index] = y;
        switch (event.kind)
        {
        case DIAGRAM_MESSAGE:
            anchor = y + height + diagram_message_line_gap;
            if (event.link.from == event.link.to)
                anchor += diagram_self_height;
            y = anchor + diagram_message_gap;
            break;
        case DIAGRAM_NOTE:
            y += height + diagram_note_padding_y * 2 + diagram_note_gap;
            anchor = y - diagram_note_gap;
            break;
        case DIAGRAM_ACTIVATE:
        case DIAGRAM_DEACTIVATE:
            activate(layout, index, anchor);
            break;
        case DIAGRAM_FRAME_OPEN:
        case DIAGRAM_FRAME_SECTION:
        case DIAGRAM_FRAME_CLOSE:
            frame(layout, index, &y);
            anchor = y;
            break;
        }
    }
    layout->bottom = y;
    layout->extent.height = y + layout->header_height + diagram_outer_margin;
    if (layout->frame_count != 0)
        layout->outcome = DIAGRAM_INVALID;
}
static void emit_title(struct diagram_sequence_layout *_Nonnull layout)
{
    size_t title = diagram_model_title_text(layout->model);
    if (title == diagram_none)
        return;
    struct diagram_size size = diagram_metrics_at(layout->metrics, title, DIAGRAM_TEXT_BODY);
    emit(layout, (struct diagram_primitive){
                     .kind = DIAGRAM_PRIMITIVE_TEXT,
                     .ink = DIAGRAM_INK_TEXT,
                     .text = title,
                     .bounds = {diagram_outer_margin, diagram_outer_margin,
                                layout->extent.width - diagram_outer_margin * 2, size.height}});
}
static void emit_actor(struct diagram_sequence_layout *_Nonnull layout, size_t actor, int y)
{
    struct diagram_node node = diagram_model_node_at(layout->model, actor);
    struct diagram_size size = layout->actors[actor];
    struct diagram_rect bounds = {layout->x[actor] - size.width / 2, y, size.width, size.height};
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = node.shape,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_ACCENT,
                                            .fill = DIAGRAM_INK_NODE,
                                            .text = diagram_none,
                                            .bounds = bounds});
    struct diagram_size text = diagram_metrics_at(layout->metrics, node.label, DIAGRAM_TEXT_BODY);
    struct diagram_rect label = diagram_geometry_label(bounds, text, node.shape);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .text = node.label,
                                            .bounds = label});
}
static void emit_lifelines(struct diagram_sequence_layout *_Nonnull layout)
{
    for (size_t actor = 0; actor < diagram_model_node_count(layout->model); ++actor)
        emit(layout, (struct diagram_primitive){
                         .kind = DIAGRAM_PRIMITIVE_PATH,
                         .stroke = DIAGRAM_DASHED,
                         .ink = DIAGRAM_INK_MUTED,
                         .text = diagram_none,
                         .count = 2,
                         .points = {{layout->x[actor], layout->header_y + layout->header_height},
                                    {layout->x[actor], layout->bottom}}});
}
static void emit_frame(struct diagram_sequence_layout *_Nonnull layout, size_t index)
{
    struct diagram_event event = diagram_model_event_at(layout->model, index);
    int inset = 12 + (int)layout->depth[index] * 12;
    struct diagram_rect bounds = {inset, layout->y[index], layout->extent.width - inset * 2,
                                  layout->end[index] - layout->y[index]};
    if (event.kind == DIAGRAM_FRAME_OPEN)
        emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                                .shape = DIAGRAM_RECTANGLE,
                                                .stroke = DIAGRAM_SOLID,
                                                .ink = DIAGRAM_INK_MUTED,
                                                .bounds = bounds,
                                                .text = diagram_none});
    if (event.kind == DIAGRAM_FRAME_SECTION)
        emit(layout, (struct diagram_primitive){
                         .kind = DIAGRAM_PRIMITIVE_PATH,
                         .stroke = DIAGRAM_DASHED,
                         .ink = DIAGRAM_INK_MUTED,
                         .text = diagram_none,
                         .count = 2,
                         .points = {{bounds.x, bounds.y}, {bounds.x + bounds.width, bounds.y}}});
    struct diagram_size text = event_size(layout, event);
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .fill = DIAGRAM_INK_GROUP,
                                            .role = DIAGRAM_TEXT_CAPTION,
                                            .alignment = DIAGRAM_TEXT_LEFT,
                                            .text_inset = diagram_frame_padding_x,
                                            .text = event.link.label,
                                            .bounds = {bounds.x, bounds.y,
                                                       text.width + diagram_frame_padding_x * 2,
                                                       text.height + diagram_frame_padding_y * 2}});
}
static void emit_activation(struct diagram_sequence_layout *_Nonnull layout, size_t index)
{
    struct diagram_event event = diagram_model_event_at(layout->model, index);
    int bottom = layout->end[index] == 0 ? layout->bottom : layout->end[index];
    int height = bottom - layout->y[index];
    if (height < 8)
        height = 8;
    emit(layout, (struct diagram_primitive){
                     .kind = DIAGRAM_PRIMITIVE_SHAPE,
                     .shape = DIAGRAM_RECTANGLE,
                     .stroke = DIAGRAM_SOLID,
                     .ink = DIAGRAM_INK_ACCENT,
                     .fill = DIAGRAM_INK_NODE,
                     .text = diagram_none,
                     .bounds = {layout->x[event.link.from] - diagram_activation_width / 2 +
                                    (int)layout->depth[index] * diagram_activation_step,
                                layout->y[index], diagram_activation_width, height}});
}
static void emit_message(struct diagram_sequence_layout *_Nonnull layout, size_t index)
{
    struct diagram_event event = diagram_model_event_at(layout->model, index);
    struct diagram_link link = event.link;
    struct diagram_size text = diagram_metrics_at(layout->metrics, link.label, DIAGRAM_TEXT_BODY);
    int from = layout->x[link.from];
    int to = layout->x[link.to];
    int y = layout->y[index] + text.height + diagram_message_line_gap;
    struct diagram_primitive path = {.kind = DIAGRAM_PRIMITIVE_PATH,
                                     .stroke = link.stroke,
                                     .start = link.start,
                                     .end = link.end,
                                     .ink = DIAGRAM_INK_TEXT,
                                     .text = diagram_none,
                                     .count = 2,
                                     .points = {{from, y}, {to, y}}};
    int center = (from + to) / 2;
    if (link.from == link.to)
    {
        path.points[1].x += diagram_self_width;
        path.points[2] = (struct diagram_point){from + diagram_self_width, y + diagram_self_height};
        path.points[3] = (struct diagram_point){from, y + diagram_self_height};
        path.count = 4;
        center = from + 24 + text.width / 2;
    }
    emit(layout, path);
    emit(layout,
         (struct diagram_primitive){
             .kind = DIAGRAM_PRIMITIVE_TEXT,
             .ink = DIAGRAM_INK_TEXT,
             .text = link.label,
             .alignment = link.from == link.to ? DIAGRAM_TEXT_LEFT : DIAGRAM_TEXT_CENTER,
             .bounds = {center - text.width / 2, layout->y[index], text.width, text.height}});
}
static void emit_note(struct diagram_sequence_layout *_Nonnull layout, size_t index)
{
    struct diagram_event event = diagram_model_event_at(layout->model, index);
    struct diagram_size text = event_size(layout, event);
    int from = layout->x[event.link.from];
    int to = layout->x[event.link.to];
    int width = text.width + diagram_note_padding_x * 2;
    int x = (from + to - width) / 2;
    switch (event.side)
    {
    case DIAGRAM_NOTE_OVER:
        break;
    case DIAGRAM_NOTE_LEFT:
        x = from - width - 16;
        break;
    case DIAGRAM_NOTE_RIGHT:
        x = from + 16;
        break;
    }
    struct diagram_rect bounds = {x, layout->y[index], width,
                                  text.height + diagram_note_padding_y * 2};
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_SHAPE,
                                            .shape = DIAGRAM_RECTANGLE,
                                            .stroke = DIAGRAM_SOLID,
                                            .ink = DIAGRAM_INK_MUTED,
                                            .fill = DIAGRAM_INK_NOTE,
                                            .text = diagram_none,
                                            .bounds = bounds});
    bounds = (struct diagram_rect){x + diagram_note_padding_x, bounds.y + diagram_note_padding_y,
                                   text.width, text.height};
    emit(layout, (struct diagram_primitive){.kind = DIAGRAM_PRIMITIVE_TEXT,
                                            .ink = DIAGRAM_INK_TEXT,
                                            .text = event.link.label,
                                            .bounds = bounds});
}
static void emit_backgrounds(struct diagram_sequence_layout *_Nonnull layout)
{
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
    {
        struct diagram_event event = diagram_model_event_at(layout->model, index);
        if (event.kind == DIAGRAM_FRAME_OPEN || event.kind == DIAGRAM_FRAME_SECTION)
            emit_frame(layout, index);
    }
    emit_lifelines(layout);
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
        if (diagram_model_event_at(layout->model, index).kind == DIAGRAM_ACTIVATE)
            emit_activation(layout, index);
}
static enum diagram_outcome construct(struct diagram_sequence_layout *_Nonnull layout)
{
    measure_actors(layout);
    measure_events(layout);
    place_actors(layout);
    place_events(layout);
    if (layout->extent.width > diagram_coordinate_limit ||
        layout->extent.height > diagram_coordinate_limit)
        return DIAGRAM_TOO_LARGE;
    if (layout->outcome != DIAGRAM_ACCEPTED)
        return layout->outcome;
    emit_title(layout);
    emit_backgrounds(layout);
    for (size_t index = 0; index < diagram_model_event_count(layout->model); ++index)
    {
        struct diagram_event event = diagram_model_event_at(layout->model, index);
        if (event.kind == DIAGRAM_MESSAGE)
            emit_message(layout, index);
        if (event.kind == DIAGRAM_NOTE)
            emit_note(layout, index);
    }
    for (size_t actor = 0; actor < diagram_model_node_count(layout->model); ++actor)
    {
        emit_actor(layout, actor, layout->header_y);
        emit_actor(layout, actor, layout->bottom);
    }
    if (layout->outcome == DIAGRAM_ACCEPTED)
        layout->outcome = diagram_scene_seal(layout->scene, layout->extent);
    return layout->outcome;
}
static void workspace_destroy(struct diagram_sequence_layout *_Nullable layout)
{
    free(layout);
}
enum diagram_outcome diagram_sequence_layout_create(const struct diagram_model *_Nonnull model,
                                                    const struct diagram_metrics *_Nonnull metrics,
                                                    struct diagram_scene *_Nullable *_Nonnull out)
{
    if (diagram_model_node_count(model) == 0 || diagram_model_kind(model) != DIAGRAM_SEQUENCE)
        return DIAGRAM_INVALID;
    struct diagram_scene *_Nullable scene = nullptr;
    enum diagram_outcome outcome = diagram_scene_create(&scene);
    if (outcome != DIAGRAM_ACCEPTED)
        return outcome;
    struct diagram_sequence_layout *_Nullable layout = calloc(1, sizeof *layout);
    if (layout == nullptr)
    {
        diagram_scene_destroy(scene);
        return DIAGRAM_OUT_OF_MEMORY;
    }
    layout->model = model;
    layout->metrics = metrics;
    layout->scene = scene;
    layout->outcome = DIAGRAM_ACCEPTED;
    outcome = construct(layout);
    workspace_destroy(layout);
    if (outcome != DIAGRAM_ACCEPTED)
    {
        diagram_scene_destroy(scene);
        return outcome;
    }
    *out = scene;
    return DIAGRAM_ACCEPTED;
}
