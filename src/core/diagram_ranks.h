#ifndef NENEFOLIO_DIAGRAM_RANKS_H
#define NENEFOLIO_DIAGRAM_RANKS_H
#include "diagram_model.h"
struct diagram_ranks;
[[nodiscard]] enum diagram_outcome
diagram_ranks_create(const struct diagram_model *_Nonnull model,
                     struct diagram_ranks *_Nullable *_Nonnull out);
[[nodiscard]] size_t diagram_ranks_at(const struct diagram_ranks *_Nonnull ranks, size_t node);
[[nodiscard]] size_t diagram_ranks_maximum(const struct diagram_ranks *_Nonnull ranks);
[[nodiscard]] bool diagram_ranks_reversed(const struct diagram_ranks *_Nonnull ranks, size_t edge);
void diagram_ranks_destroy(struct diagram_ranks *_Nullable ranks);
#endif
