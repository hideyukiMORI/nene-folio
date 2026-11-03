#ifndef NENEFOLIO_DIAGRAM_LABEL_H
#define NENEFOLIO_DIAGRAM_LABEL_H
#include "diagram_model.h"
#include "diagram_text.h"
[[nodiscard]] enum diagram_outcome diagram_label_store(struct diagram_model *_Nonnull model,
                                                       struct diagram_text text,
                                                       size_t *_Nonnull index);
#endif
