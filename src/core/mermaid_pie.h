#ifndef NENEFOLIO_MERMAID_PIE_H
#define NENEFOLIO_MERMAID_PIE_H
#include "diagram_model.h"
#include "diagram_text.h"
/* #955 / ADR0112: a Mermaid `pie` as a model of slices. Each slice is a node: its id is the name,
 * its label is the legend line (name and percentage), and its share is the sweep of its sector in
 * hundredths of a degree; the shares add up to the whole circle. */
constexpr size_t mermaid_pie_slice_limit = 64;
constexpr size_t mermaid_pie_circle = 36000;
[[nodiscard]] enum diagram_outcome
mermaid_pie_create(struct diagram_text source, struct diagram_model *_Nullable *_Nonnull out);
#endif
