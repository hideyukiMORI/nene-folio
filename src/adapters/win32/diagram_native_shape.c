#include "diagram_native_shape.h"
#include "diagram_geometry.h"
#include "diagram_style.h"
static void line(HDC _Nonnull dc, int x, int y, POINT end)
{
    MoveToEx(dc, x, y, nullptr);
    LineTo(dc, end.x, end.y);
}
static void cylinder(HDC _Nonnull dc, RECT r, int scale)
{
    int curve = diagram_geometry_cylinder_curve((r.bottom - r.top) / scale) * scale;
    HGDIOBJ pen = SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, r.left, r.bottom - curve * 2, r.right, r.bottom);
    Rectangle(dc, r.left, r.top + curve, r.right, r.bottom - curve + 1);
    Ellipse(dc, r.left, r.top, r.right, r.top + curve * 2);
    SelectObject(dc, pen);
    HGDIOBJ brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    Ellipse(dc, r.left, r.top, r.right, r.top + curve * 2);
    int direction = SetArcDirection(dc, AD_COUNTERCLOCKWISE);
    Arc(dc, r.left, r.bottom - curve * 2, r.right, r.bottom, r.left, r.bottom - curve, r.right,
        r.bottom - curve);
    SetArcDirection(dc, direction);
    line(dc, r.left, r.top + curve, (POINT){r.left, r.bottom - curve});
    line(dc, r.right - 1, r.top + curve, (POINT){r.right - 1, r.bottom - curve});
    SelectObject(dc, brush);
}
static void actor(HDC _Nonnull dc, RECT r, int scale)
{
    int center = (r.left + r.right) / 2;
    int head = diagram_actor_head * scale;
    int half = diagram_actor_width * scale / 2;
    int bottom = r.top + diagram_actor_height * scale;
    Ellipse(dc, center - head / 2, r.top, center + head / 2, r.top + head);
    line(dc, center, r.top + head, (POINT){center, r.top + diagram_actor_waist * scale});
    line(dc, center - half, r.top + diagram_actor_shoulder * scale,
         (POINT){center + half, r.top + diagram_actor_shoulder * scale});
    line(dc, center, r.top + diagram_actor_waist * scale,
         (POINT){center - diagram_actor_foot * scale, bottom});
    line(dc, center, r.top + diagram_actor_waist * scale,
         (POINT){center + diagram_actor_foot * scale, bottom});
}
static void quadrilateral(HDC _Nonnull dc, enum diagram_shape shape, RECT r)
{
    int slope = (r.right - r.left) / diagram_polygon_slope_divisor;
    POINT points[4] = {{r.left, r.top}, {r.right, r.top}, {r.right, r.bottom}, {r.left, r.bottom}};
    if (shape == DIAGRAM_PARALLELOGRAM || shape == DIAGRAM_TRAPEZOID)
        points[0].x += slope;
    if (shape == DIAGRAM_PARALLELOGRAM_REVERSE || shape == DIAGRAM_TRAPEZOID)
        points[1].x -= slope;
    if (shape == DIAGRAM_PARALLELOGRAM || shape == DIAGRAM_TRAPEZOID_REVERSE)
        points[2].x -= slope;
    if (shape == DIAGRAM_PARALLELOGRAM_REVERSE || shape == DIAGRAM_TRAPEZOID_REVERSE)
        points[3].x += slope;
    Polygon(dc, points, 4);
}
static void angular(HDC _Nonnull dc, enum diagram_shape shape, RECT r)
{
    int center_x = (r.left + r.right) / 2;
    int center_y = (r.top + r.bottom) / 2;
    if (shape == DIAGRAM_DIAMOND)
    {
        POINT points[4] = {
            {center_x, r.top}, {r.right, center_y}, {center_x, r.bottom}, {r.left, center_y}};
        Polygon(dc, points, 4);
        return;
    }
    int slope = (r.right - r.left) / diagram_polygon_slope_divisor;
    POINT points[6] = {{r.left + slope, r.top},     {r.right - slope, r.top},   {r.right, center_y},
                       {r.right - slope, r.bottom}, {r.left + slope, r.bottom}, {r.left, center_y}};
    if (shape == DIAGRAM_ASYMMETRIC)
    {
        points[0].x = points[4].x = r.left;
        points[5].x = r.left + slope;
        points[1].x = points[3].x = r.right;
    }
    Polygon(dc, points, 6);
}
void diagram_native_shape_draw(HDC _Nonnull dc, const struct diagram_primitive *_Nonnull primitive,
                               RECT r, int scale)
{
    enum diagram_shape shape = primitive->shape;
    switch (shape)
    {
    case DIAGRAM_RECTANGLE:
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        break;
    case DIAGRAM_ROUNDED:
        RoundRect(dc, r.left, r.top, r.right, r.bottom, primitive->radius * scale * 2,
                  primitive->radius * scale * 2);
        break;
    case DIAGRAM_STADIUM:
        RoundRect(dc, r.left, r.top, r.right, r.bottom, r.bottom - r.top, r.bottom - r.top);
        break;
    case DIAGRAM_SUBROUTINE:
        Rectangle(dc, r.left, r.top, r.right, r.bottom);
        line(dc, r.left + diagram_subroutine_inset * scale, r.top,
             (POINT){r.left + diagram_subroutine_inset * scale, r.bottom});
        line(dc, r.right - diagram_subroutine_inset * scale, r.top,
             (POINT){r.right - diagram_subroutine_inset * scale, r.bottom});
        break;
    case DIAGRAM_CYLINDER:
        cylinder(dc, r, scale);
        break;
    case DIAGRAM_CIRCLE:
    case DIAGRAM_DOUBLE_CIRCLE:
        Ellipse(dc, r.left, r.top, r.right, r.bottom);
        if (shape == DIAGRAM_DOUBLE_CIRCLE)
            Ellipse(dc, r.left + diagram_double_circle_inset * scale,
                    r.top + diagram_double_circle_inset * scale,
                    r.right - diagram_double_circle_inset * scale,
                    r.bottom - diagram_double_circle_inset * scale);
        break;
    case DIAGRAM_DIAMOND:
    case DIAGRAM_HEXAGON:
    case DIAGRAM_ASYMMETRIC:
        angular(dc, shape, r);
        break;
    case DIAGRAM_PARALLELOGRAM:
    case DIAGRAM_PARALLELOGRAM_REVERSE:
    case DIAGRAM_TRAPEZOID:
    case DIAGRAM_TRAPEZOID_REVERSE:
        quadrilateral(dc, shape, r);
        break;
    case DIAGRAM_ACTOR:
        actor(dc, r, scale);
        break;
    }
}
static POINT arrow_base(POINT tip, POINT previous, int size)
{
    int x = previous.x == tip.x ? 0 : (previous.x > tip.x ? size : -size);
    int y = previous.y == tip.y ? 0 : (previous.y > tip.y ? size : -size);
    return (POINT){tip.x + x, tip.y + y};
}
static void arrow_triangle(POINT tip, POINT previous, int size, POINT *_Nonnull points)
{
    POINT base = arrow_base(tip, previous, size);
    int x = tip.y == previous.y ? 0 : size / 2;
    int y = tip.x == previous.x ? 0 : size / 2;
    points[0] = (POINT){base.x + x, base.y + y};
    points[1] = tip;
    points[2] = (POINT){base.x - x, base.y - y};
}
static void arrow_head(HDC _Nonnull dc, enum diagram_arrow arrow, const POINT *_Nonnull points,
                       int mark)
{
    POINT tip = points[1];
    switch (arrow)
    {
    case DIAGRAM_ARROW_NONE:
        break;
    case DIAGRAM_ARROW_CLOSED:
        Polygon(dc, points, 3);
        break;
    case DIAGRAM_ARROW_OPEN:
        Polyline(dc, points, 3);
        break;
    case DIAGRAM_ARROW_CIRCLE:
        Ellipse(dc, tip.x - mark, tip.y - mark, tip.x + mark, tip.y + mark);
        break;
    case DIAGRAM_ARROW_CROSS:
        line(dc, tip.x - mark, tip.y - mark, (POINT){tip.x + mark, tip.y + mark});
        line(dc, tip.x - mark, tip.y + mark, (POINT){tip.x + mark, tip.y - mark});
        break;
    }
}
static size_t previous_point(const struct diagram_primitive *_Nonnull primitive, size_t side)
{
    size_t tip = side == 0 ? 0 : primitive->count - 1;
    size_t previous = side == 0 ? 1 : primitive->count - 2;
    while (previous > 0 && previous + 1 < primitive->count &&
           primitive->points[tip].x == primitive->points[previous].x &&
           primitive->points[tip].y == primitive->points[previous].y)
        previous = side == 0 ? previous + 1 : previous - 1;
    return previous;
}
void diagram_native_shape_arrows(HDC _Nonnull dc,
                                 const struct diagram_primitive *_Nonnull primitive, int scale)
{
    if (primitive->count < 2)
        return;
    int size =
        (primitive->stroke == DIAGRAM_THICK ? diagram_thick_arrow_size : diagram_arrow_size) *
        scale;
    for (size_t side = 0; side < 2; ++side)
    {
        struct diagram_point tip = primitive->points[side == 0 ? 0 : primitive->count - 1];
        struct diagram_point previous = primitive->points[previous_point(primitive, side)];
        if (tip.x == previous.x && tip.y == previous.y)
            continue;
        POINT triangle[3];
        arrow_triangle((POINT){tip.x * scale, tip.y * scale},
                       (POINT){previous.x * scale, previous.y * scale}, size, triangle);
        arrow_head(dc, side == 0 ? primitive->start : primitive->end, triangle,
                   diagram_endpoint_size * scale / 2);
    }
}
