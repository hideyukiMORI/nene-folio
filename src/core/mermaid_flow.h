#ifndef NENEFOLIO_MERMAID_FLOW_H
#define NENEFOLIO_MERMAID_FLOW_H
#include "diagram_model.h"
#include "diagram_text.h"
[[nodiscard]] enum diagram_outcome
mermaid_flow_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
