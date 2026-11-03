#ifndef NENEFOLIO_MERMAID_SEQUENCE_H
#define NENEFOLIO_MERMAID_SEQUENCE_H
#include "diagram_model.h"
#include "diagram_text.h"
[[nodiscard]] enum diagram_outcome
mermaid_sequence_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
