#ifndef NENEFOLIO_MERMAID_CLASS_H
#define NENEFOLIO_MERMAID_CLASS_H
#include "diagram_model.h"
#include "diagram_text.h"
/* #890 / ADR0107: a class diagram as a flowchart model (classes, members, UML relations). */
[[nodiscard]] enum diagram_outcome
mermaid_class_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
