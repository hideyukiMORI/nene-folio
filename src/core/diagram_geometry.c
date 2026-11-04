#include "diagram_geometry.h"
#include "diagram_style.h"
#include <stdint.h>
static int diagonal(struct diagram_size text)
{
    int64_t squared = (int64_t)text.width * text.width + (int64_t)text.height * text.height;
    int lower = 0, upper = text.width + text.height;
    while (lower < upper)
    {
        int middle = lower + (upper - lower) / 2;
        if ((int64_t)middle * middle < squared)
            lower = middle + 1;
        else
            upper = middle;
    }
    return lower;
}
static int stadium_width(struct diagram_size text, int height)
{
    int ends = diagram_node_padding_x * 2 + 24;
    /* Tall labels need the full end caps outside the text's central strip. */
    return text.width + (ends > height ? ends : height);
}
static int sloped_width(int padded)
{
    int central = diagram_polygon_slope_divisor - 2;
    int width = (padded * diagram_polygon_slope_divisor + central - 1) / central;
    int compact = padded + 24;
    return width > compact ? width : compact;
}
struct diagram_size diagram_geometry_node(struct diagram_size text, enum diagram_shape shape)
{
    struct diagram_size size = {text.width + diagram_node_padding_x * 2,
                                text.height + diagram_node_padding_y * 2};
    switch (shape)
    {
    case DIAGRAM_RECTANGLE:
    case DIAGRAM_ROUNDED:
        break;
    case DIAGRAM_STADIUM:
        size.width = stadium_width(text, size.height);
        break;
    case DIAGRAM_SUBROUTINE:
        size.width += 24;
        break;
    case DIAGRAM_HEXAGON:
    case DIAGRAM_PARALLELOGRAM:
    case DIAGRAM_PARALLELOGRAM_REVERSE:
    case DIAGRAM_TRAPEZOID:
    case DIAGRAM_TRAPEZOID_REVERSE:
    case DIAGRAM_ASYMMETRIC:
        size.width = sloped_width(size.width);
        break;
    case DIAGRAM_CYLINDER:
        size.height += diagram_cylinder_extra;
        break;
    case DIAGRAM_CIRCLE:
    case DIAGRAM_DOUBLE_CIRCLE:
        size.width = diagonal(text) + diagram_circle_padding;
        if (shape == DIAGRAM_DOUBLE_CIRCLE)
            size.width += diagram_double_circle_inset * 2;
        if (size.width < diagram_node_min_width)
            size.width = diagram_node_min_width;
        size.height = size.width;
        break;
    case DIAGRAM_DIAMOND:
        size.width = text.width * 2 + 40;
        size.height = text.height * 2 + 40;
        break;
    case DIAGRAM_ACTOR:
        size.width = (text.width > diagram_actor_width ? text.width : diagram_actor_width) + 16;
        size.height = diagram_actor_height + diagram_actor_name_gap + text.height;
        break;
    }
    if (size.width < diagram_node_min_width)
        size.width = diagram_node_min_width;
    if (size.height < diagram_node_min_height)
        size.height = diagram_node_min_height;
    return size;
}
int diagram_geometry_cylinder_curve(int height)
{
    int curve = height / 6;
    return curve < diagram_cylinder_curve_max ? curve : diagram_cylinder_curve_max;
}
struct diagram_rect diagram_geometry_label(struct diagram_rect bounds, struct diagram_size text,
                                           enum diagram_shape shape)
{
    int top = bounds.y + (bounds.height - text.height) / 2;
    if (shape == DIAGRAM_CYLINDER)
    {
        int curve = diagram_geometry_cylinder_curve(bounds.height);
        top = bounds.y + curve * 2 + (bounds.height - curve * 3 - text.height) / 2;
    }
    if (shape == DIAGRAM_ACTOR)
        top = bounds.y + diagram_actor_height + diagram_actor_name_gap;
    return (struct diagram_rect){bounds.x + (bounds.width - text.width) / 2, top, text.width,
                                 text.height};
}
struct diagram_point diagram_geometry_point(struct diagram_point point,
                                            enum diagram_direction direction,
                                            struct diagram_size extent)
{
    switch (direction)
    {
    case DIAGRAM_TOP_BOTTOM:
        return point;
    case DIAGRAM_BOTTOM_TOP:
        return (struct diagram_point){point.x, extent.height - point.y};
    case DIAGRAM_LEFT_RIGHT:
        return (struct diagram_point){point.y, point.x};
    case DIAGRAM_RIGHT_LEFT:
        return (struct diagram_point){extent.height - point.y, point.x};
    }
    return point;
}
struct diagram_rect diagram_geometry_rect(struct diagram_rect rect,
                                          enum diagram_direction direction,
                                          struct diagram_size extent)
{
    struct diagram_point first =
        diagram_geometry_point((struct diagram_point){rect.x, rect.y}, direction, extent);
    struct diagram_point last = diagram_geometry_point(
        (struct diagram_point){rect.x + rect.width, rect.y + rect.height}, direction, extent);
    return (struct diagram_rect){first.x < last.x ? first.x : last.x,
                                 first.y < last.y ? first.y : last.y,
                                 first.x < last.x ? last.x - first.x : first.x - last.x,
                                 first.y < last.y ? last.y - first.y : first.y - last.y};
}
struct diagram_size diagram_geometry_extent(struct diagram_size extent,
                                            enum diagram_direction direction)
{
    switch (direction)
    {
    case DIAGRAM_TOP_BOTTOM:
    case DIAGRAM_BOTTOM_TOP:
        return extent;
    case DIAGRAM_LEFT_RIGHT:
    case DIAGRAM_RIGHT_LEFT:
        return (struct diagram_size){extent.height, extent.width};
    }
    return extent;
}
