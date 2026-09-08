#include "name_list.h"

#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

struct name_list
{
    char *_Nonnull *_Nullable items; /* 各要素は終端付きの所有文字列 */
    size_t count;
    size_t capacity;
};

static bool reserved_character(uint32_t code_point)
{
    static const char reserved[] = "\\/:*?\"<>|";
    if (code_point < 0x20)
    {
        return true;
    }
    for (size_t index = 0; reserved[index] != '\0'; ++index)
    {
        if (code_point == (unsigned char)reserved[index])
        {
            return true;
        }
    }
    return false;
}

static bool dot_only(const char *_Nonnull text, size_t length)
{
    return (length == 1 && text[0] == '.') || (length == 2 && text[0] == '.' && text[1] == '.');
}

static bool acceptable(const char *_Nonnull text, size_t length)
{
    if (length == 0 || length > name_list_max_length || dot_only(text, length))
    {
        return false;
    }
    if (text[length - 1] == ' ' || text[length - 1] == '.')
    {
        return false;
    }
    for (size_t index = 0; index < length;)
    {
        uint32_t code_point = 0;
        size_t consumed = utf8_text_decode(text + index, length - index, &code_point);
        if (consumed == 0 || reserved_character(code_point))
        {
            return false;
        }
        index += consumed;
    }
    return true;
}

enum name_list_outcome name_list_create(struct name_list *_Nullable *_Nonnull out)
{
    struct name_list *_Nullable list = calloc(1, sizeof *list);
    if (list == nullptr)
    {
        return NAME_LIST_OUT_OF_MEMORY;
    }
    *out = list;
    return NAME_LIST_ACCEPTED;
}

static bool reserve(struct name_list *_Nonnull list)
{
    if (list->count < list->capacity)
    {
        return true;
    }
    size_t capacity = list->capacity == 0 ? 4 : list->capacity * 2;
    char *_Nonnull *_Nullable grown = realloc(list->items, capacity * sizeof *grown);
    if (grown == nullptr)
    {
        return false;
    }
    list->items = grown;
    list->capacity = capacity;
    return true;
}

static bool same_text(const char *_Nonnull item, const char *_Nonnull text, size_t length)
{
    return strlen(item) == length && memcmp(item, text, length) == 0;
}

enum name_list_outcome name_list_append(struct name_list *_Nonnull list, const char *_Nonnull text,
                                        size_t length)
{
    if (!acceptable(text, length))
    {
        return NAME_LIST_INVALID_NAME;
    }
    for (size_t index = 0; index < list->count; ++index)
    {
        if (same_text(list->items[index], text, length))
        {
            return NAME_LIST_DUPLICATE;
        }
    }
    if (!reserve(list))
    {
        return NAME_LIST_OUT_OF_MEMORY;
    }
    char *_Nullable copy = malloc(length + 1);
    if (copy == nullptr)
    {
        return NAME_LIST_OUT_OF_MEMORY;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    list->items[list->count] = copy;
    list->count += 1;
    return NAME_LIST_ACCEPTED;
}

size_t name_list_count(const struct name_list *_Nonnull list)
{
    return list->count;
}

const char *_Nonnull name_list_at(const struct name_list *_Nonnull list, size_t index)
{
    return list->items[index];
}

bool name_list_contains(const struct name_list *_Nonnull list, const char *_Nonnull text)
{
    size_t length = strlen(text);
    for (size_t index = 0; index < list->count; ++index)
    {
        if (same_text(list->items[index], text, length))
        {
            return true;
        }
    }
    return false;
}

void name_list_destroy(struct name_list *_Nullable list)
{
    if (list == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < list->count; ++index)
    {
        free(list->items[index]);
    }
    free(list->items);
    free(list);
}
