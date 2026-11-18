#include "diagram_native_shape.h"
#include "diagram_geometry.h"
#include "diagram_style.h"
#include <math.h>
#include <stdlib.h>
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
/* #955: a point far out on the radial at `angle` (hundredths of a degree, clockwise from the
 * top). GDI only needs the direction, and a long radial keeps small sweeps apart. */
static POINT radial(RECT r, int angle)
{
    double radians = (double)angle * 3.14159265358979323846 / 18000.0;
    double reach = (double)(r.right - r.left) * 8.0;
    return (POINT){(r.left + r.right) / 2 + (LONG)lround(sin(radians) * reach),
                   (r.top + r.bottom) / 2 - (LONG)lround(cos(radians) * reach)};
}
/* The sector from `start` over `sweep`, clockwise. GDI draws counter-clockwise from its first
 * radial to its second, so the end comes first. A whole circle is an ellipse. When the radials
 * cannot be told apart (GDI would draw the whole circle), a sweep under half is not drawn and a
 * sweep over half is the circle. */
static void sector(HDC _Nonnull dc, RECT r, int start, int sweep)
{
    if (sweep >= 36000)
    {
        Ellipse(dc, r.left, r.top, r.right, r.bottom);
        return;
    }
    POINT from = radial(r, start + sweep);
    POINT to = radial(r, start);
    bool same = from.x == to.x && from.y == to.y;
    if (same && sweep > 18000)
        Ellipse(dc, r.left, r.top, r.right, r.bottom);
    if (sweep <= 0 || same)
        return;
    Pie(dc, r.left, r.top, r.right, r.bottom, from.x, from.y, to.x, to.y);
}
void diagram_native_shape_draw(HDC _Nonnull dc, const struct diagram_primitive *_Nonnull primitive,
                               RECT r, int scale)
{
    enum diagram_shape shape = primitive->shape;
    switch (shape)
    {
    case DIAGRAM_RECTANGLE:
    case DIAGRAM_ENTITY:
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
    case DIAGRAM_STATE_START:
    case DIAGRAM_STATE_END:
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
    case DIAGRAM_SECTOR:
        sector(dc, r, primitive->points[0].x, primitive->points[0].y);
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
/* #890: a diamond from the tip back along the line, as wide as the arrowhead and twice as
 * long. */
static void arrow_diamond(HDC _Nonnull dc, const POINT *_Nonnull points)
{
    POINT tip = points[1];
    POINT middle = {(points[0].x + points[2].x) / 2, (points[0].y + points[2].y) / 2};
    POINT diamond[4] = {tip, points[0], {2 * middle.x - tip.x, 2 * middle.y - tip.y}, points[2]};
    Polygon(dc, diamond, 4);
}
/* The hollow markers are filled with the diagram's ground, so the line does not show through.
 * False when the marker is not a hollow one. */
static bool arrow_hollow(HDC _Nonnull dc, enum diagram_arrow arrow, const POINT *_Nonnull points,
                         COLORREF hollow)
{
    if (arrow != DIAGRAM_ARROW_HOLLOW && arrow != DIAGRAM_ARROW_DIAMOND)
        return false;
    COLORREF ink = SetDCBrushColor(dc, hollow);
    HGDIOBJ previous = SelectObject(dc, GetStockObject(DC_BRUSH));
    if (arrow == DIAGRAM_ARROW_HOLLOW)
        Polygon(dc, points, 3);
    else
        arrow_diamond(dc, points);
    SelectObject(dc, previous);
    SetDCBrushColor(dc, ink);
    return true;
}
/* #898: the ER cardinalities, drawn from the entity's edge (the tip) back along the line.
 * `points` is the arrowhead's triangle: its base corners and the tip between them. */
static POINT er_along(const POINT *_Nonnull points, int percent)
{
    POINT tip = points[1];
    POINT base = {(points[0].x + points[2].x) / 2, (points[0].y + points[2].y) / 2};
    return (POINT){tip.x + (base.x - tip.x) * percent / 100,
                   tip.y + (base.y - tip.y) * percent / 100};
}
static POINT er_half(const POINT *_Nonnull points)
{
    return (POINT){(points[0].x - points[2].x) / 2, (points[0].y - points[2].y) / 2};
}
static void er_bar(HDC _Nonnull dc, const POINT *_Nonnull points, int percent)
{
    POINT centre = er_along(points, percent);
    POINT half = er_half(points);
    line(dc, centre.x + half.x, centre.y + half.y, (POINT){centre.x - half.x, centre.y - half.y});
}
static void er_foot(HDC _Nonnull dc, const POINT *_Nonnull points)
{
    POINT apex = er_along(points, 100);
    POINT tip = points[1];
    POINT half = er_half(points);
    line(dc, apex.x, apex.y, (POINT){tip.x + half.x, tip.y + half.y});
    line(dc, apex.x, apex.y, tip);
    line(dc, apex.x, apex.y, (POINT){tip.x - half.x, tip.y - half.y});
}
static void er_circle(HDC _Nonnull dc, const POINT *_Nonnull points, int percent, COLORREF hollow)
{
    POINT centre = er_along(points, percent);
    POINT half = er_half(points);
    int radius = (abs(half.x) + abs(half.y)) * 7 / 10;
    COLORREF ink = SetDCBrushColor(dc, hollow);
    HGDIOBJ previous = SelectObject(dc, GetStockObject(DC_BRUSH));
    Ellipse(dc, centre.x - radius, centre.y - radius, centre.x + radius + 1, centre.y + radius + 1);
    SelectObject(dc, previous);
    SetDCBrushColor(dc, ink);
}
static bool er_marker(enum diagram_arrow arrow)
{
    return arrow == DIAGRAM_ARROW_ER_ONE || arrow == DIAGRAM_ARROW_ER_ZERO_ONE ||
           arrow == DIAGRAM_ARROW_ER_MANY || arrow == DIAGRAM_ARROW_ER_ZERO_MANY;
}
/* False when the marker is not an ER cardinality. */
static bool arrow_er(HDC _Nonnull dc, enum diagram_arrow arrow, const POINT *_Nonnull points,
                     COLORREF hollow)
{
    switch (arrow)
    {
    case DIAGRAM_ARROW_NONE:
    case DIAGRAM_ARROW_CLOSED:
    case DIAGRAM_ARROW_OPEN:
    case DIAGRAM_ARROW_CIRCLE:
    case DIAGRAM_ARROW_CROSS:
    case DIAGRAM_ARROW_HOLLOW:
    case DIAGRAM_ARROW_DIAMOND:
    case DIAGRAM_ARROW_DIAMOND_FILLED:
        return false;
    case DIAGRAM_ARROW_ER_ONE:
        er_bar(dc, points, 60);
        er_bar(dc, points, 110);
        return true;
    case DIAGRAM_ARROW_ER_ZERO_ONE:
        er_bar(dc, points, 60);
        er_circle(dc, points, 150, hollow);
        return true;
    case DIAGRAM_ARROW_ER_MANY:
        er_foot(dc, points);
        er_bar(dc, points, 130);
        return true;
    case DIAGRAM_ARROW_ER_ZERO_MANY:
        er_foot(dc, points);
        er_circle(dc, points, 160, hollow);
        return true;
    }
    return false;
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
    case DIAGRAM_ARROW_DIAMOND_FILLED:
        arrow_diamond(dc, points);
        break;
    case DIAGRAM_ARROW_HOLLOW:
    case DIAGRAM_ARROW_DIAMOND:
        break; /* drawn by arrow_hollow with the ground colour */
    case DIAGRAM_ARROW_ER_ONE:
    case DIAGRAM_ARROW_ER_ZERO_ONE:
    case DIAGRAM_ARROW_ER_MANY:
    case DIAGRAM_ARROW_ER_ZERO_MANY:
        break; /* drawn by arrow_er */
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
/* The marker's triangle at one end of the path; false when the path has no direction there. */
static bool marker_points(const struct diagram_primitive *_Nonnull primitive, size_t side,
                          int scale, POINT *_Nonnull triangle)
{
    int size =
        (primitive->stroke == DIAGRAM_THICK ? diagram_thick_arrow_size : diagram_arrow_size) *
        scale;
    /* #898: bars, circles and the crow's foot need more room than an arrowhead to be read. */
    if (er_marker(side == 0 ? primitive->start : primitive->end))
        size = size * 8 / 5;
    struct diagram_point tip = primitive->points[side == 0 ? 0 : primitive->count - 1];
    struct diagram_point previous = primitive->points[previous_point(primitive, side)];
    if (tip.x == previous.x && tip.y == previous.y)
        return false;
    arrow_triangle((POINT){tip.x * scale, tip.y * scale},
                   (POINT){previous.x * scale, previous.y * scale}, size, triangle);
    return true;
}
void diagram_native_shape_arrows(HDC _Nonnull dc,
                                 const struct diagram_primitive *_Nonnull primitive, int scale,
                                 COLORREF hollow)
{
    if (primitive->count < 2)
        return;
    for (size_t side = 0; side < 2; ++side)
    {
        POINT triangle[3];
        if (!marker_points(primitive, side, scale, triangle))
            continue;
        enum diagram_arrow arrow = side == 0 ? primitive->start : primitive->end;
        if (!arrow_hollow(dc, arrow, triangle, hollow) && !arrow_er(dc, arrow, triangle, hollow))
            arrow_head(dc, arrow, triangle, diagram_endpoint_size * scale / 2);
    }
}
