#ifndef NENEFOLIO_DIAGRAM_CANVAS_H
#define NENEFOLIO_DIAGRAM_CANVAS_H
#include "diagram_metrics.h"
#include "diagram_model.h"
#include "diagram_request.h"
#include "diagram_scene.h"
#include "image_bitmap.h"
#include "image_work.h"
struct diagram_canvas;
[[nodiscard]] enum image_outcome
diagram_canvas_create(const struct diagram_request *_Nonnull request,
                      struct diagram_canvas *_Nullable *_Nonnull out);
[[nodiscard]] enum image_outcome diagram_canvas_measure(
    struct diagram_canvas *_Nonnull canvas, const struct diagram_model *_Nonnull model,
    const struct image_work *_Nonnull work, struct diagram_metrics *_Nullable *_Nonnull out);
[[nodiscard]] enum image_outcome diagram_canvas_draw(struct diagram_canvas *_Nonnull canvas,
                                                     const struct diagram_scene *_Nonnull scene,
                                                     const struct image_work *_Nonnull work,
                                                     struct image_bitmap *_Nullable *_Nonnull out);
void diagram_canvas_destroy(struct diagram_canvas *_Nullable canvas);
#endif
