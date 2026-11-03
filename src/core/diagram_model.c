#include "diagram_model.h"
#include "diagram_build_state.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>
struct diagram_model
{
    enum diagram_kind kind;
    enum diagram_direction direction;
    enum diagram_build_state state;
    size_t title;
    char *_Nullable texts[diagram_text_limit];
    size_t text_count;
    struct diagram_node nodes[diagram_node_limit];
    size_t node_count;
    struct diagram_link links[diagram_link_limit];
    size_t link_count;
    struct diagram_group groups[diagram_group_limit];
    size_t group_count;
    struct diagram_event events[diagram_event_limit];
    size_t event_count;
};
enum diagram_outcome diagram_model_create(enum diagram_kind kind,
                                          struct diagram_model *_Nullable *_Nonnull out)
{
    struct diagram_model *_Nullable model = calloc(1, sizeof *model);
    if (model == nullptr)
        return DIAGRAM_OUT_OF_MEMORY;
    model->kind = kind;
    model->direction = DIAGRAM_TOP_BOTTOM;
    model->title = diagram_none;
    model->state = DIAGRAM_BUILD_OPEN;
    *out = model;
    return DIAGRAM_ACCEPTED;
}
static bool valid_text(const char *_Nonnull text, size_t length)
{
    size_t cursor = 0;
    while (cursor < length)
    {
        uint32_t point = 0;
        size_t used = utf8_text_decode(text + cursor, length - cursor, &point);
        if (used == 0 || point == 0 || (point < 32 && point != '\n' && point != '\t'))
            return false;
        cursor += used;
    }
    return true;
}
enum diagram_outcome diagram_model_text(struct diagram_model *_Nonnull model,
                                        const char *_Nonnull text, size_t length,
                                        size_t *_Nonnull index)
{
    if (model->state != DIAGRAM_BUILD_OPEN)
        return DIAGRAM_INVALID;
    if (length > diagram_label_limit)
        return DIAGRAM_TOO_LARGE;
    if (!valid_text(text, length))
        return DIAGRAM_INVALID;
    for (size_t at = 0; at < model->text_count; ++at)
        if (strlen(model->texts[at]) == length && memcmp(model->texts[at], text, length) == 0)
        {
            *index = at;
            return DIAGRAM_ACCEPTED;
        }
    if (model->text_count == diagram_text_limit)
        return DIAGRAM_TOO_LARGE;
    char *_Nullable copied = malloc(length + 1);
    if (copied == nullptr)
        return DIAGRAM_OUT_OF_MEMORY;
    memcpy(copied, text, length);
    copied[length] = '\0';
    *index = model->text_count;
    model->texts[model->text_count++] = copied;
    return DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_model_node(struct diagram_model *_Nonnull model, size_t index,
                                        struct diagram_node value)
{
    if (model->state != DIAGRAM_BUILD_OPEN || index > model->node_count ||
        value.id >= model->text_count || value.label >= model->text_count ||
        (value.group != diagram_none && value.group >= model->group_count))
        return DIAGRAM_INVALID;
    if (index == diagram_node_limit)
        return DIAGRAM_TOO_LARGE;
    model->nodes[index] = value;
    if (index == model->node_count)
        ++model->node_count;
    return DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_model_link(struct diagram_model *_Nonnull model,
                                        struct diagram_link value)
{
    if (model->state != DIAGRAM_BUILD_OPEN || value.from >= model->node_count ||
        value.to >= model->node_count || value.label >= model->text_count || value.length == 0 ||
        value.length > 16)
        return DIAGRAM_INVALID;
    if (model->link_count == diagram_link_limit)
        return DIAGRAM_TOO_LARGE;
    model->links[model->link_count++] = value;
    return DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_model_group(struct diagram_model *_Nonnull model, size_t index,
                                         struct diagram_group value)
{
    if (model->state != DIAGRAM_BUILD_OPEN || index > model->group_count ||
        value.id >= model->text_count || value.label >= model->text_count ||
        (value.parent != diagram_none && value.parent >= index))
        return DIAGRAM_INVALID;
    if (index == diagram_group_limit)
        return DIAGRAM_TOO_LARGE;
    model->groups[index] = value;
    if (index == model->group_count)
        ++model->group_count;
    return DIAGRAM_ACCEPTED;
}
enum diagram_outcome diagram_model_event(struct diagram_model *_Nonnull model,
                                         struct diagram_event value)
{
    if (model->state != DIAGRAM_BUILD_OPEN || value.link.label >= model->text_count)
        return DIAGRAM_INVALID;
    switch (value.kind)
    {
    case DIAGRAM_MESSAGE:
    case DIAGRAM_NOTE:
    case DIAGRAM_ACTIVATE:
    case DIAGRAM_DEACTIVATE:
        if (value.link.from >= model->node_count || value.link.to >= model->node_count)
            return DIAGRAM_INVALID;
        break;
    case DIAGRAM_FRAME_OPEN:
    case DIAGRAM_FRAME_SECTION:
    case DIAGRAM_FRAME_CLOSE:
        break;
    }
    if (model->event_count == diagram_event_limit)
        return DIAGRAM_TOO_LARGE;
    model->events[model->event_count++] = value;
    return DIAGRAM_ACCEPTED;
}
void diagram_model_direction(struct diagram_model *_Nonnull model, enum diagram_direction direction)
{
    if (model->state == DIAGRAM_BUILD_OPEN)
        model->direction = direction;
}
void diagram_model_title(struct diagram_model *_Nonnull model, size_t title)
{
    if (model->state == DIAGRAM_BUILD_OPEN && title < model->text_count)
        model->title = title;
}
void diagram_model_seal(struct diagram_model *_Nonnull model)
{
    model->state = DIAGRAM_BUILD_SEALED;
}
enum diagram_kind diagram_model_kind(const struct diagram_model *_Nonnull model)
{
    return model->kind;
}
enum diagram_direction diagram_model_flow(const struct diagram_model *_Nonnull model)
{
    return model->direction;
}
size_t diagram_model_title_text(const struct diagram_model *_Nonnull model)
{
    return model->title;
}
size_t diagram_model_text_count(const struct diagram_model *_Nonnull model)
{
    return model->text_count;
}
const char *_Nonnull diagram_model_text_at(const struct diagram_model *_Nonnull model, size_t index)
{
    return index < model->text_count ? model->texts[index] : "";
}
size_t diagram_model_node_count(const struct diagram_model *_Nonnull model)
{
    return model->node_count;
}
struct diagram_node diagram_model_node_at(const struct diagram_model *_Nonnull model, size_t index)
{
    return model->nodes[index];
}
size_t diagram_model_find_node(const struct diagram_model *_Nonnull model, const char *_Nonnull id,
                               size_t length)
{
    for (size_t index = 0; index < model->node_count; ++index)
    {
        const char *name = model->texts[model->nodes[index].id];
        if (strlen(name) == length && memcmp(name, id, length) == 0)
            return index;
    }
    return diagram_none;
}
size_t diagram_model_link_count(const struct diagram_model *_Nonnull model)
{
    return model->link_count;
}
struct diagram_link diagram_model_link_at(const struct diagram_model *_Nonnull model, size_t index)
{
    return model->links[index];
}
size_t diagram_model_group_count(const struct diagram_model *_Nonnull model)
{
    return model->group_count;
}
struct diagram_group diagram_model_group_at(const struct diagram_model *_Nonnull model,
                                            size_t index)
{
    return model->groups[index];
}
size_t diagram_model_find_group(const struct diagram_model *_Nonnull model, const char *_Nonnull id,
                                size_t length)
{
    for (size_t index = 0; index < model->group_count; ++index)
    {
        const char *name = model->texts[model->groups[index].id];
        if (strlen(name) == length && memcmp(name, id, length) == 0)
            return index;
    }
    return diagram_none;
}
size_t diagram_model_event_count(const struct diagram_model *_Nonnull model)
{
    return model->event_count;
}
struct diagram_event diagram_model_event_at(const struct diagram_model *_Nonnull model,
                                            size_t index)
{
    return model->events[index];
}
void diagram_model_destroy(struct diagram_model *_Nullable model)
{
    if (model != nullptr)
        for (size_t index = 0; index < model->text_count; ++index)
            free(model->texts[index]);
    free(model);
}
