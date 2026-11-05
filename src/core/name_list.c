#include "name_list.h"

#include "utf8_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct name_list
{
    /* 前半capacityは挿入順の所有文字列、後半capacityは辞書順の借用参照。 */
    char *_Nonnull *_Nullable items;
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

static char upper_ascii(char value)
{
    return value >= 'a' && value <= 'z' ? (char)(value - 'a' + 'A') : value;
}

static bool device_digit(const char *_Nonnull text, size_t length)
{
    return (length == 1 && text[0] >= '1' && text[0] <= '9') ||
           (length == 2 && (unsigned char)text[0] == 0xC2 &&
            ((unsigned char)text[1] == 0xB9 || (unsigned char)text[1] == 0xB2 ||
             (unsigned char)text[1] == 0xB3));
}

static bool reserved_device(const char *_Nonnull text, size_t length)
{
    size_t base = 0;
    while (base < length && text[base] != '.')
    {
        base += 1;
    }
    if (base < 3)
    {
        return false;
    }
    char prefix[] = {upper_ascii(text[0]), upper_ascii(text[1]), upper_ascii(text[2]), '\0'};
    if (base == 3)
    {
        return strcmp(prefix, "CON") == 0 || strcmp(prefix, "PRN") == 0 ||
               strcmp(prefix, "AUX") == 0 || strcmp(prefix, "NUL") == 0;
    }
    return (strcmp(prefix, "COM") == 0 || strcmp(prefix, "LPT") == 0) &&
           device_digit(text + 3, base - 3);
}

static bool acceptable(const char *_Nonnull text, size_t length)
{
    if (length == 0 || length > name_list_max_length || dot_only(text, length) ||
        reserved_device(text, length))
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
    if (list->capacity > SIZE_MAX / (4 * sizeof *list->items))
    {
        return false;
    }
    size_t capacity = list->capacity == 0 ? 4 : list->capacity * 2;
    char *_Nonnull *_Nullable grown = realloc(list->items, 2 * capacity * sizeof *grown);
    if (grown == nullptr)
    {
        return false;
    }
    memmove(grown + capacity, grown + list->capacity, list->count * sizeof *grown);
    list->items = grown;
    list->capacity = capacity;
    return true;
}

static int compare(const char *_Nonnull item, const char *_Nonnull text, size_t length)
{
    size_t item_length = strlen(item);
    size_t common = item_length < length ? item_length : length;
    int order = memcmp(item, text, common);
    return order != 0 ? order : (item_length > length) - (item_length < length);
}

/* 挿入順の配列を走査せず、同じ辞書順参照から存在と挿入位置を決める。 */
static bool locate(const struct name_list *_Nonnull list, const char *_Nonnull text, size_t length,
                   size_t *_Nonnull position)
{
    size_t low = 0;
    size_t high = list->count;
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        if (compare(list->items[list->capacity + middle], text, length) < 0)
            low = middle + 1;
        else
            high = middle;
    }
    *position = low;
    return low < list->count && compare(list->items[list->capacity + low], text, length) == 0;
}

enum name_list_outcome name_list_append(struct name_list *_Nonnull list, const char *_Nonnull text,
                                        size_t length)
{
    if (!acceptable(text, length))
    {
        return NAME_LIST_INVALID_NAME;
    }
    size_t position = 0;
    if (locate(list, text, length, &position))
    {
        return NAME_LIST_DUPLICATE;
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
    char *_Nonnull *_Nonnull ordered = list->items + list->capacity;
    memmove(ordered + position + 1, ordered + position, (list->count - position) * sizeof *ordered);
    ordered[position] = copy;
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
    size_t position = 0;
    return locate(list, text, strlen(text), &position);
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
