#ifndef NENEFOLIO_MERMAID_STATE_H
#define NENEFOLIO_MERMAID_STATE_H
#include "diagram_model.h"
#include "diagram_text.h"
/* #872 / ADR0104: a state diagram as a flowchart model (states, transitions, [*], composites). */
[[nodiscard]] enum diagram_outcome
mermaid_state_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
