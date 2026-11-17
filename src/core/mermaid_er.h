#ifndef NENEFOLIO_MERMAID_ER_H
#define NENEFOLIO_MERMAID_ER_H
#include "diagram_model.h"
#include "diagram_text.h"
/* #882 / ADR0105: an ER diagram as a flowchart model (entities, relationships, attributes). */
[[nodiscard]] enum diagram_outcome mermaid_er_create(struct diagram_text source,
                                                     struct diagram_model *_Nullable *_Nonnull out);
#endif
