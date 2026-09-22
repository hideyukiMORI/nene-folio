#include "replace_edit.h"

#include <stdlib.h>
#include <string.h>

struct replace_edit
{
    struct note_search_span span;
    char16_t *_Nonnull units; /* 終端付き */
    size_t capacity;
    size_t length;
};

enum replace_edit_outcome replace_edit_create(struct note_search_span span, size_t capacity,
                                              struct replace_edit *_Nullable *_Nonnull out)
{
    struct replace_edit *_Nullable edit = calloc(1, sizeof *edit);
    if (edit == nullptr)
    {
        return REPLACE_EDIT_OUT_OF_MEMORY;
    }
    edit->units = calloc(capacity + 1, sizeof *edit->units);
    if (edit->units == nullptr)
    {
        free(edit);
        return REPLACE_EDIT_OUT_OF_MEMORY;
    }
    edit->span = span;
    edit->capacity = capacity;
    *out = edit;
    return REPLACE_EDIT_READY;
}

void replace_edit_append(struct replace_edit *_Nonnull edit, const char16_t *_Nonnull units,
                         size_t count)
{
    size_t room = edit->capacity - edit->length;
    size_t taken = count < room ? count : room;
    memcpy(edit->units + edit->length, units, taken * sizeof *edit->units);
    edit->length += taken;
    edit->units[edit->length] = u'\0';
}

struct note_search_span replace_edit_span(const struct replace_edit *_Nonnull edit)
{
    return edit->span;
}

const char16_t *_Nonnull replace_edit_units(const struct replace_edit *_Nonnull edit)
{
    return edit->units;
}

size_t replace_edit_length(const struct replace_edit *_Nonnull edit)
{
    return edit->length;
}

void replace_edit_destroy(struct replace_edit *_Nullable edit)
{
    if (edit == nullptr)
    {
        return;
    }
    free(edit->units);
    free(edit);
}
