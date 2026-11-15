#include "note_identity.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct note_identity
{
    size_t note_offset;
    char bytes[];
};
enum note_identity_outcome note_identity_create(const char *_Nonnull category,
                                                const char *_Nonnull note,
                                                struct note_identity *_Nullable *_Nonnull out)
{
    size_t category_size = strlen(category) + 1;
    size_t note_size = strlen(note) + 1;
    if (category_size > SIZE_MAX - sizeof(struct note_identity) ||
        note_size > SIZE_MAX - sizeof(struct note_identity) - category_size)
        return NOTE_IDENTITY_OUT_OF_MEMORY;
    struct note_identity *_Nullable value = malloc(sizeof *value + category_size + note_size);
    if (value == nullptr)
        return NOTE_IDENTITY_OUT_OF_MEMORY;
    value->note_offset = category_size;
    memcpy(value->bytes, category, category_size);
    memcpy(value->bytes + category_size, note, note_size);
    *out = value;
    return NOTE_IDENTITY_READY;
}
const char *_Nonnull note_identity_category(const struct note_identity *_Nonnull identity)
{
    return identity->bytes;
}
const char *_Nonnull note_identity_note(const struct note_identity *_Nonnull identity)
{
    return identity->bytes + identity->note_offset;
}
void note_identity_destroy(struct note_identity *_Nullable identity)
{
    free(identity);
}
