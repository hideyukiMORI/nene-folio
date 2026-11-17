#ifndef NENEFOLIO_MERMAID_FLOW_H
#define NENEFOLIO_MERMAID_FLOW_H
#include "diagram_model.h"
#include "diagram_text.h"
/* `TD`/`TB`/`BT`/`LR`/`RL`; false for anything else (#872: shared with state diagrams). */
[[nodiscard]] bool mermaid_flow_direction(struct diagram_text text,
                                          enum diagram_direction *_Nonnull out);
[[nodiscard]] enum diagram_outcome
mermaid_flow_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
