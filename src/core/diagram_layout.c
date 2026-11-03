#include "diagram_layout.h"
#include "diagram_flow_layout.h"
#include "diagram_sequence_layout.h"
enum diagram_outcome diagram_layout_create(const struct diagram_model *_Nonnull model,
                                           const struct diagram_metrics *_Nonnull metrics,
                                           struct diagram_scene *_Nullable *_Nonnull out)
{
    if (diagram_metrics_count(metrics) != diagram_model_text_count(model))
        return DIAGRAM_INVALID;
    switch (diagram_model_kind(model))
    {
    case DIAGRAM_FLOWCHART:
        return diagram_flow_layout_create(model, metrics, out);
    case DIAGRAM_SEQUENCE:
        return diagram_sequence_layout_create(model, metrics, out);
    }
    return DIAGRAM_INVALID;
}
