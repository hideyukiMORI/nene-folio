#include "diagram_render.h"
#include "diagram_canvas.h"
#include "diagram_layout.h"
#include "mermaid_diagram.h"
static enum image_outcome converted(enum diagram_outcome outcome)
{
    switch (outcome)
    {
    case DIAGRAM_ACCEPTED:
        return IMAGE_READY;
    case DIAGRAM_INVALID:
        return IMAGE_FAILED;
    case DIAGRAM_UNSUPPORTED:
        return IMAGE_UNSUPPORTED;
    case DIAGRAM_TOO_LARGE:
        return IMAGE_TOO_LARGE;
    case DIAGRAM_OUT_OF_MEMORY:
        return IMAGE_OUT_OF_MEMORY;
    }
    return IMAGE_FAILED;
}
static enum image_outcome render_model(const struct diagram_request *_Nonnull request,
                                       const struct diagram_model *_Nonnull model,
                                       const struct image_work *_Nonnull work,
                                       struct image_bitmap *_Nullable *_Nonnull out)
{
    if (work->cancelled(work))
        return IMAGE_CANCELLED;
    struct diagram_canvas *_Nullable canvas = nullptr;
    enum image_outcome outcome = diagram_canvas_create(request, &canvas);
    if (outcome != IMAGE_READY)
        return outcome;
    work->progress(work, IMAGE_PHASE_DIAGRAM_LAYOUT);
    struct diagram_metrics *_Nullable metrics = nullptr;
    outcome = diagram_canvas_measure(canvas, model, work, &metrics);
    struct diagram_scene *_Nullable scene = nullptr;
    if (outcome == IMAGE_READY)
        outcome = converted(diagram_layout_create(model, metrics, &scene));
    if (outcome == IMAGE_READY && work->cancelled(work))
        outcome = IMAGE_CANCELLED;
    if (outcome == IMAGE_READY)
    {
        work->progress(work, IMAGE_PHASE_DIAGRAM_DRAWING);
        outcome = diagram_canvas_draw(canvas, scene, work, out);
    }
    diagram_scene_destroy(scene);
    diagram_metrics_destroy(metrics);
    diagram_canvas_destroy(canvas);
    return outcome;
}
enum image_outcome diagram_render_read(const struct diagram_request *_Nonnull request,
                                       const struct image_work *_Nonnull work,
                                       struct image_bitmap *_Nullable *_Nonnull out)
{
    if (work->cancelled(work))
        return IMAGE_CANCELLED;
    work->progress(work, IMAGE_PHASE_DIAGRAM_PARSING);
    struct diagram_model *_Nullable model = nullptr;
    enum image_outcome outcome = converted(mermaid_diagram_create(request->source, &model));
    if (outcome == IMAGE_READY)
        outcome = render_model(request, model, work, out);
    diagram_model_destroy(model);
    return outcome;
}
