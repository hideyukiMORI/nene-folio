#ifndef NENEFOLIO_DIAGRAM_GEOMETRY_H
#define NENEFOLIO_DIAGRAM_GEOMETRY_H
#include "diagram_direction.h"
#include "diagram_point.h"
#include "diagram_rect.h"
#include "diagram_shape.h"
#include "diagram_size.h"
[[nodiscard]] struct diagram_size diagram_geometry_node(struct diagram_size text,
                                                        enum diagram_shape shape);
[[nodiscard]] int diagram_geometry_cylinder_curve(int height);
/* Map a side midpoint of a positive layout box to its physical shape contour. */
[[nodiscard]] struct diagram_point diagram_geometry_attachment(struct diagram_rect bounds,
                                                               enum diagram_shape shape,
                                                               struct diagram_point point);
[[nodiscard]] struct diagram_rect diagram_geometry_label(struct diagram_rect bounds,
                                                         struct diagram_size text,
                                                         enum diagram_shape shape);
[[nodiscard]] struct diagram_point diagram_geometry_point(struct diagram_point point,
                                                          enum diagram_direction direction,
                                                          struct diagram_size extent);
[[nodiscard]] struct diagram_rect diagram_geometry_rect(struct diagram_rect rect,
                                                        enum diagram_direction direction,
                                                        struct diagram_size extent);
[[nodiscard]] struct diagram_size diagram_geometry_extent(struct diagram_size extent,
                                                          enum diagram_direction direction);
#endif
