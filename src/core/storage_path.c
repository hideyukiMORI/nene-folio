#include "storage_path.h"
#include "utf8_text.h"
#include <stdlib.h>
#include <string.h>

struct storage_path
{
    char *_Nonnull text;
    size_t length;
};

static bool separator(char value)
{
    return value == '\\' || value == '/';
}

static bool letter(char value)
{
    return (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

static size_t root_start(const char *_Nonnull text, size_t length)
{
    if (length >= 3 && letter(text[0]) && text[1] == ':' && separator(text[2]))
        return 3;
    if (length >= 5 && separator(text[0]) && separator(text[1]))
        return 2;
    return 0;
}

static bool valid_characters(const char *_Nonnull text, size_t length, size_t start)
{
    size_t at = start;
    while (at < length)
    {
        uint32_t point = 0;
        size_t used = utf8_text_decode(text + at, length - at, &point);
        if (used == 0 || point < 32 || point == 127 || point == ':' || point == '*' ||
            point == '?' || point == '"' || point == '<' || point == '>' || point == '|')
            return false;
        at += used;
    }
    return true;
}

static bool valid_components(const char *_Nonnull text, size_t length, size_t start)
{
    size_t at = start;
    size_t components = 0;
    while (at < length)
    {
        size_t end = at;
        while (end < length && !separator(text[end]))
            end += 1;
        if (end == at || text[end - 1] == '.' || text[end - 1] == ' ')
            return false;
        components += 1;
        at = end + 1;
    }
    return start != 2 || components >= 2;
}

static void destroy_prepared(struct storage_path *_Nullable path, char *_Nullable text)
{
    free(path);
    free(text);
}

static enum storage_path_outcome copy_text(const char *_Nonnull text, size_t length,
                                           struct storage_path *_Nullable *_Nonnull out)
{
    struct storage_path *_Nullable path = malloc(sizeof *path);
    char *_Nullable owned = malloc(length + 1);
    if (path == nullptr || owned == nullptr)
    {
        destroy_prepared(path, owned);
        return STORAGE_PATH_OUT_OF_MEMORY;
    }
    memcpy(owned, text, length);
    owned[length] = '\0';
    for (size_t index = 0; index < length; ++index)
    {
        if (owned[index] == '/')
            owned[index] = '\\';
    }
    if (length >= 2 && owned[1] == ':' && owned[0] >= 'a' && owned[0] <= 'z')
        owned[0] = (char)(owned[0] - ('a' - 'A'));
    *path = (struct storage_path){.text = owned, .length = length};
    *out = path;
    return STORAGE_PATH_READY;
}

enum storage_path_outcome storage_path_create(const char *_Nonnull text, size_t length,
                                              struct storage_path *_Nullable *_Nonnull out)
{
    if (length > storage_path_byte_limit)
        return STORAGE_PATH_INVALID;
    size_t start = root_start(text, length);
    if (start == 0 || !valid_characters(text, length, start) ||
        !valid_components(text, length, start))
        return STORAGE_PATH_INVALID;
    if (length > start && separator(text[length - 1]))
        length -= 1;
    return copy_text(text, length, out);
}

enum storage_path_outcome storage_path_copy(const struct storage_path *_Nonnull path,
                                            struct storage_path *_Nullable *_Nonnull out)
{
    return copy_text(path->text, path->length, out);
}

const char *_Nonnull storage_path_text(const struct storage_path *_Nonnull path)
{
    return path->text;
}

size_t storage_path_length(const struct storage_path *_Nonnull path)
{
    return path->length;
}

bool storage_path_equal(const struct storage_path *_Nonnull left,
                        const struct storage_path *_Nonnull right)
{
    return strcmp(left->text, right->text) == 0;
}

void storage_path_destroy(struct storage_path *_Nullable path)
{
    if (path != nullptr)
        free(path->text);
    free(path);
}
