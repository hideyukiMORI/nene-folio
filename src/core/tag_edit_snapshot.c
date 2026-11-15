#include "tag_edit_snapshot.h"
#include "note_text.h"
#include <stdlib.h>
#include <string.h>

struct tag_edit_snapshot
{
    struct note_text *_Nonnull body;
    char *_Nonnull category;
    char *_Nonnull note;
};

static char *_Nullable copy_bytes(const char *_Nonnull bytes)
{
    size_t length = strlen(bytes) + 1;
    char *_Nullable copied = malloc(length);
    if (copied != nullptr)
        memcpy(copied, bytes, length);
    return copied;
}

static void parts_destroy(struct tag_edit_snapshot *_Nullable snapshot, char *_Nullable category,
                          char *_Nullable note)
{
    free(category);
    free(note);
    free(snapshot);
}

enum tag_edit_snapshot_outcome
tag_edit_snapshot_create(struct note_text *_Nonnull body,
                         const struct tag_edit_identity *_Nonnull identity,
                         struct tag_edit_snapshot *_Nullable *_Nonnull out)
{
    struct tag_edit_snapshot *_Nullable snapshot = malloc(sizeof *snapshot);
    char *_Nullable category = copy_bytes(identity->category);
    char *_Nullable note = copy_bytes(identity->note);
    if (snapshot == nullptr || category == nullptr || note == nullptr)
    {
        parts_destroy(snapshot, category, note);
        return TAG_EDIT_SNAPSHOT_OUT_OF_MEMORY;
    }
    snapshot->body = body;
    snapshot->category = category;
    snapshot->note = note;
    *out = snapshot;
    return TAG_EDIT_SNAPSHOT_READY;
}

bool tag_edit_snapshot_holds(const struct tag_edit_snapshot *_Nonnull snapshot,
                             const struct note_text *_Nonnull current,
                             const struct tag_edit_identity *_Nonnull identity)
{
    return strcmp(snapshot->category, identity->category) == 0 &&
           strcmp(snapshot->note, identity->note) == 0 && note_text_equals(snapshot->body, current);
}

const struct note_text *_Nonnull tag_edit_snapshot_text(
    const struct tag_edit_snapshot *_Nonnull snapshot)
{
    return snapshot->body;
}

void tag_edit_snapshot_destroy(struct tag_edit_snapshot *_Nullable snapshot)
{
    if (snapshot == nullptr)
        return;
    note_text_destroy(snapshot->body);
    parts_destroy(snapshot, snapshot->category, snapshot->note);
}
