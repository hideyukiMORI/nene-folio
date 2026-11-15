#include "tag_update.h"
#include "note_metadata.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "tag_update_request.h"
#include <stdlib.h>
#include <string.h>

struct tag_update
{
    char *_Nonnull category;
    char *_Nonnull note;
    struct note_text *_Nullable source;
    struct note_text *_Nullable body;
    enum tag_update_kind kind;
};

static char *_Nullable copy_name(const char *_Nonnull name)
{
    size_t length = strlen(name) + 1;
    char *_Nullable copied = malloc(length);
    if (copied != nullptr)
        memcpy(copied, name, length);
    return copied;
}

static void parts_destroy(struct tag_update *_Nullable update, char *_Nullable category,
                          char *_Nullable note)
{
    free(category);
    free(note);
    free(update);
}

static void owned_destroy(struct tag_update *_Nonnull update)
{
    note_text_destroy(update->source);
    note_text_destroy(update->body);
    parts_destroy(update, update->category, update->note);
}

static enum note_tags_outcome desired_tags(const struct note_tags *_Nonnull current,
                                           const struct tag_update_request *_Nonnull request,
                                           struct note_tags *_Nullable *_Nonnull out)
{
    struct note_tags *_Nullable removed = nullptr;
    enum note_tags_outcome made = note_tags_with(current, request->from, NOTE_TAG_ABSENT, &removed);
    if (made != NOTE_TAGS_READY)
        return made;
    if (request->to == nullptr)
    {
        *out = removed;
        return NOTE_TAGS_READY;
    }
    made = note_tags_with(removed, request->to, NOTE_TAG_PRESENT, out);
    note_tags_destroy(removed);
    return made;
}

static enum tag_update_outcome prepare_change(struct tag_update *_Nonnull update,
                                              const struct tag_update_request *_Nonnull request,
                                              const struct note_text *_Nonnull source)
{
    const struct note_metadata *_Nonnull metadata = note_text_metadata(source);
    struct note_tags *_Nullable desired = nullptr;
    if (desired_tags(note_metadata_tags(metadata), request, &desired) != NOTE_TAGS_READY)
        return TAG_UPDATE_OUT_OF_MEMORY;
    enum note_tag_edit_outcome made = note_metadata_rewrite(metadata, desired, &update->body);
    note_tags_destroy(desired);
    switch (made)
    {
    case NOTE_TAG_EDIT_READY:
        update->kind = TAG_UPDATE_CHANGE;
        return note_text_copy(source, &update->source) == NOTE_TEXT_ACCEPTED
                   ? TAG_UPDATE_READY
                   : TAG_UPDATE_OUT_OF_MEMORY;
    case NOTE_TAG_EDIT_UNCHANGED:
        update->kind = TAG_UPDATE_NO_MATCH;
        return TAG_UPDATE_READY;
    case NOTE_TAG_EDIT_UNAVAILABLE:
        update->kind = TAG_UPDATE_UNAVAILABLE;
        return TAG_UPDATE_READY;
    case NOTE_TAG_EDIT_LIMIT:
        update->kind = TAG_UPDATE_LIMIT;
        return TAG_UPDATE_READY;
    case NOTE_TAG_EDIT_OUT_OF_MEMORY:
        return TAG_UPDATE_OUT_OF_MEMORY;
    }
    return TAG_UPDATE_OUT_OF_MEMORY;
}

static enum tag_update_outcome prepare_update(struct tag_update *_Nonnull update,
                                              const struct tag_update_request *_Nonnull request)
{
    if (request->source == nullptr)
    {
        update->kind = TAG_UPDATE_UNREADABLE;
        return TAG_UPDATE_READY;
    }
    const struct note_metadata *_Nonnull metadata = note_text_metadata(request->source);
    if (!note_metadata_available(metadata))
    {
        update->kind = TAG_UPDATE_UNAVAILABLE;
        return TAG_UPDATE_READY;
    }
    if (!note_tags_contains(note_metadata_tags(metadata), request->from))
        return TAG_UPDATE_READY;
    return prepare_change(update, request, request->source);
}

enum tag_update_outcome tag_update_create(const struct tag_update_request *_Nonnull request,
                                          struct tag_update *_Nullable *_Nonnull out)
{
    struct tag_update *_Nullable update = malloc(sizeof *update);
    char *_Nullable category = copy_name(request->category);
    char *_Nullable note = copy_name(request->note);
    if (update == nullptr || category == nullptr || note == nullptr)
    {
        parts_destroy(update, category, note);
        return TAG_UPDATE_OUT_OF_MEMORY;
    }
    *update = (struct tag_update){category, note, nullptr, nullptr, TAG_UPDATE_NO_MATCH};
    enum tag_update_outcome prepared = prepare_update(update, request);
    if (prepared != TAG_UPDATE_READY)
    {
        owned_destroy(update);
        return prepared;
    }
    *out = update;
    return TAG_UPDATE_READY;
}

enum tag_update_kind tag_update_kind(const struct tag_update *_Nonnull update)
{
    return update->kind;
}

const char *_Nonnull tag_update_category(const struct tag_update *_Nonnull update)
{
    return update->category;
}

const char *_Nonnull tag_update_note(const struct tag_update *_Nonnull update)
{
    return update->note;
}

const struct note_text *_Nullable tag_update_source(const struct tag_update *_Nonnull update)
{
    return update->source;
}

const struct note_text *_Nullable tag_update_body(const struct tag_update *_Nonnull update)
{
    return update->body;
}

void tag_update_destroy(struct tag_update *_Nullable update)
{
    if (update == nullptr)
        return;
    owned_destroy(update);
}
