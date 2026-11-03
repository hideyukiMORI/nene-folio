#ifndef NENEFOLIO_MERMAID_DIAGRAM_H
#define NENEFOLIO_MERMAID_DIAGRAM_H
#include "diagram_model.h"
struct note_text;
[[nodiscard]] enum diagram_outcome
mermaid_diagram_create(const struct note_text *_Nonnull source,
                       struct diagram_model *_Nullable *_Nonnull out);
#endif
