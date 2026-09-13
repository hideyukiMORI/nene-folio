#include "note_name.h"
#include "name_list.h"
#include <stdlib.h>

struct note_name
{
    struct name_list *_Nullable value;
};

static bool extension(const char *_Nonnull text, size_t length)
{
    return length >= 3 && text[length - 3] == '.' &&
           (text[length - 2] == 'm' || text[length - 2] == 'M') &&
           (text[length - 1] == 'd' || text[length - 1] == 'D');
}

enum note_name_outcome note_name_create(const char *_Nonnull text, size_t length,
                                        struct note_name *_Nullable *_Nonnull out)
{
    size_t stem = extension(text, length) ? length - 3 : length;
    if (stem == 0 || stem > name_list_max_length - 3)
    {
        return NOTE_NAME_INVALID;
    }
    struct note_name *_Nullable name = calloc(1, sizeof *name);
    if (name == nullptr)
    {
        return NOTE_NAME_OUT_OF_MEMORY;
    }
    enum name_list_outcome accepted = name_list_create(&name->value);
    if (accepted == NAME_LIST_ACCEPTED)
    {
        accepted = name_list_append(name->value, text, stem);
    }
    if (accepted != NAME_LIST_ACCEPTED)
    {
        note_name_destroy(name);
        return accepted == NAME_LIST_OUT_OF_MEMORY ? NOTE_NAME_OUT_OF_MEMORY : NOTE_NAME_INVALID;
    }
    *out = name;
    return NOTE_NAME_ACCEPTED;
}

const char *_Nonnull note_name_stem(const struct note_name *_Nonnull name)
{
    return name_list_at(name->value, 0);
}

void note_name_destroy(struct note_name *_Nullable name)
{
    if (name == nullptr)
    {
        return;
    }
    name_list_destroy(name->value);
    free(name);
}
