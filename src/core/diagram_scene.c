#include "diagram_scene.h"
#include "diagram_build_state.h"
#include "diagram_limits.h"
#include <stdlib.h>
struct diagram_scene
{
    struct diagram_primitive *_Nonnull items;
    size_t count;
    size_t capacity;
    struct diagram_size size;
    enum diagram_build_state state;
};
static void prepared_destroy(struct diagram_scene *_Nullable scene,
                             struct diagram_primitive *_Nullable items)
{
    free(items);
    free(scene);
}
enum diagram_outcome diagram_scene_create(struct diagram_scene *_Nullable *_Nonnull out)
{
    struct diagram_scene *_Nullable scene = malloc(sizeof *scene);
    struct diagram_primitive *_Nullable items = malloc(32 * sizeof *items);
    if (scene == nullptr || items == nullptr)
    {
        prepared_destroy(scene, items);
        return DIAGRAM_OUT_OF_MEMORY;
    }
    *scene = (struct diagram_scene){.items = items, .capacity = 32, .state = DIAGRAM_BUILD_OPEN};
    *out = scene;
    return DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_scene_append(struct diagram_scene *_Nonnull scene,
                                          const struct diagram_primitive *_Nonnull primitive)
{
    if (scene->state != DIAGRAM_BUILD_OPEN || primitive->count > 8)
        return DIAGRAM_INVALID;
    if (scene->count == diagram_primitive_limit)
        return DIAGRAM_TOO_LARGE;
    if (scene->count == scene->capacity)
    {
        size_t capacity = scene->capacity * 2;
        struct diagram_primitive *_Nullable grown = realloc(scene->items, capacity * sizeof *grown);
        if (grown == nullptr)
            return DIAGRAM_OUT_OF_MEMORY;
        scene->items = grown;
        scene->capacity = capacity;
    }
    scene->items[scene->count++] = *primitive;
    return DIAGRAM_ACCEPTED;
}
static bool inside(const struct diagram_primitive *_Nonnull primitive, struct diagram_size size)
{
    switch (primitive->kind)
    {
    case DIAGRAM_PRIMITIVE_PATH:
        if (primitive->count < 2)
            return false;
        for (size_t index = 0; index < primitive->count; ++index)
            if (primitive->points[index].x < 0 || primitive->points[index].y < 0 ||
                primitive->points[index].x > size.width || primitive->points[index].y > size.height)
                return false;
        return true;
    case DIAGRAM_PRIMITIVE_SHAPE:
    case DIAGRAM_PRIMITIVE_TEXT:
        return primitive->bounds.x >= 0 && primitive->bounds.y >= 0 &&
               primitive->bounds.width >= 0 && primitive->bounds.height >= 0 &&
               primitive->bounds.width <= size.width && primitive->bounds.height <= size.height &&
               primitive->bounds.x <= size.width - primitive->bounds.width &&
               primitive->bounds.y <= size.height - primitive->bounds.height;
    }
    return false;
}
enum diagram_outcome diagram_scene_seal(struct diagram_scene *_Nonnull scene,
                                        struct diagram_size size)
{
    if (scene->state != DIAGRAM_BUILD_OPEN)
        return DIAGRAM_INVALID;
    if (size.width < 1 || size.height < 1 || size.width > diagram_coordinate_limit ||
        size.height > diagram_coordinate_limit)
        return DIAGRAM_TOO_LARGE;
    for (size_t index = 0; index < scene->count; ++index)
        if (!inside(&scene->items[index], size))
            return DIAGRAM_TOO_LARGE;
    scene->size = size;
    scene->state = DIAGRAM_BUILD_SEALED;
    return DIAGRAM_ACCEPTED;
}
size_t diagram_scene_count(const struct diagram_scene *_Nonnull scene)
{
    return scene->count;
}
const struct diagram_primitive *_Nonnull diagram_scene_at(
    const struct diagram_scene *_Nonnull scene, size_t index)
{
    return &scene->items[index];
}
struct diagram_size diagram_scene_size(const struct diagram_scene *_Nonnull scene)
{
    return scene->size;
}
void diagram_scene_destroy(struct diagram_scene *_Nullable scene)
{
    if (scene != nullptr)
        free(scene->items);
    free(scene);
}
