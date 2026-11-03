#ifndef NENEFOLIO_DIAGRAM_MODEL_H
#define NENEFOLIO_DIAGRAM_MODEL_H
#include "diagram_event.h"
#include "diagram_group.h"
#include "diagram_kind.h"
#include "diagram_limits.h"
#include "diagram_link.h"
#include "diagram_node.h"
#include "diagram_outcome.h"
struct diagram_model;
/* The parse factories alone own this transient construction protocol. */
[[nodiscard]] enum diagram_outcome
diagram_model_create(enum diagram_kind kind, struct diagram_model *_Nullable *_Nonnull out);
[[nodiscard]] enum diagram_outcome diagram_model_text(struct diagram_model *_Nonnull model,
                                                      const char *_Nonnull text, size_t length,
                                                      size_t *_Nonnull index);
[[nodiscard]] enum diagram_outcome diagram_model_node(struct diagram_model *_Nonnull model,
                                                      size_t index, struct diagram_node value);
[[nodiscard]] enum diagram_outcome diagram_model_link(struct diagram_model *_Nonnull model,
                                                      struct diagram_link value);
[[nodiscard]] enum diagram_outcome diagram_model_group(struct diagram_model *_Nonnull model,
                                                       size_t index, struct diagram_group value);
[[nodiscard]] enum diagram_outcome diagram_model_event(struct diagram_model *_Nonnull model,
                                                       struct diagram_event value);
void diagram_model_direction(struct diagram_model *_Nonnull model,
                             enum diagram_direction direction);
void diagram_model_title(struct diagram_model *_Nonnull model, size_t title);
void diagram_model_seal(struct diagram_model *_Nonnull model);
/* Queries are immutable; no record or writable array is returned. */
[[nodiscard]] enum diagram_kind diagram_model_kind(const struct diagram_model *_Nonnull model);
[[nodiscard]] enum diagram_direction diagram_model_flow(const struct diagram_model *_Nonnull model);
[[nodiscard]] size_t diagram_model_title_text(const struct diagram_model *_Nonnull model);
[[nodiscard]] size_t diagram_model_text_count(const struct diagram_model *_Nonnull model);
[[nodiscard]] const char *_Nonnull diagram_model_text_at(const struct diagram_model *_Nonnull model,
                                                         size_t index);
[[nodiscard]] size_t diagram_model_node_count(const struct diagram_model *_Nonnull model);
[[nodiscard]] struct diagram_node diagram_model_node_at(const struct diagram_model *_Nonnull model,
                                                        size_t index);
[[nodiscard]] size_t diagram_model_find_node(const struct diagram_model *_Nonnull model,
                                             const char *_Nonnull id, size_t length);
[[nodiscard]] size_t diagram_model_link_count(const struct diagram_model *_Nonnull model);
[[nodiscard]] struct diagram_link diagram_model_link_at(const struct diagram_model *_Nonnull model,
                                                        size_t index);
[[nodiscard]] size_t diagram_model_group_count(const struct diagram_model *_Nonnull model);
[[nodiscard]] struct diagram_group
diagram_model_group_at(const struct diagram_model *_Nonnull model, size_t index);
[[nodiscard]] size_t diagram_model_find_group(const struct diagram_model *_Nonnull model,
                                              const char *_Nonnull id, size_t length);
[[nodiscard]] size_t diagram_model_event_count(const struct diagram_model *_Nonnull model);
[[nodiscard]] struct diagram_event
diagram_model_event_at(const struct diagram_model *_Nonnull model, size_t index);
void diagram_model_destroy(struct diagram_model *_Nullable model);
#endif
