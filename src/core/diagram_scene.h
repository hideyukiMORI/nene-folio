#ifndef NENEFOLIO_DIAGRAM_SCENE_H
#define NENEFOLIO_DIAGRAM_SCENE_H
#include "diagram_outcome.h"
#include "diagram_primitive.h"
#include "diagram_size.h"
struct diagram_scene;
/* Construction is confined to the synchronous layout factories. */
[[nodiscard]] enum diagram_outcome
diagram_scene_create(struct diagram_scene *_Nullable *_Nonnull out);
[[nodiscard]] enum diagram_outcome
diagram_scene_append(struct diagram_scene *_Nonnull scene,
                     const struct diagram_primitive *_Nonnull primitive);
[[nodiscard]] enum diagram_outcome diagram_scene_seal(struct diagram_scene *_Nonnull scene,
                                                      struct diagram_size size);
[[nodiscard]] size_t diagram_scene_count(const struct diagram_scene *_Nonnull scene);
[[nodiscard]] const struct diagram_primitive *_Nonnull diagram_scene_at(
    const struct diagram_scene *_Nonnull scene, size_t index);
[[nodiscard]] struct diagram_size diagram_scene_size(const struct diagram_scene *_Nonnull scene);
void diagram_scene_destroy(struct diagram_scene *_Nullable scene);
#endif
