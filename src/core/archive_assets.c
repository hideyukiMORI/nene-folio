#include "archive_assets.h"
#include "ascii_fold.h"
#include "name_list.h"
#include "note_archive.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
struct archive_assets
{
    struct archive_file *_Nullable files;
    char *_Nullable *_Nullable paths;
    size_t count;
    size_t capacity;
    size_t bytes;
};
enum note_archive_outcome archive_assets_create(struct archive_assets *_Nullable *_Nonnull out)
{
    struct archive_assets *_Nullable assets = calloc(1, sizeof *assets);
    if (assets == nullptr)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    *out = assets;
    return NOTE_ARCHIVE_READY;
}
static size_t segment_length(const char *_Nonnull path)
{
    size_t length = 0;
    while (path[length] != '\0' && path[length] != '/')
        ++length;
    return length;
}
static enum name_list_outcome validate_segments(struct name_list *_Nonnull names,
                                                const char *_Nonnull cursor)
{
    enum name_list_outcome valid = NAME_LIST_ACCEPTED;
    do
    {
        size_t length = segment_length(cursor);
        valid = name_list_append(names, cursor, length);
        if (valid == NAME_LIST_DUPLICATE)
            valid = NAME_LIST_ACCEPTED;
        cursor += length;
        if (*cursor == '/')
            ++cursor;
    } while (*cursor != '\0' && valid == NAME_LIST_ACCEPTED);
    return valid;
}
static enum note_archive_outcome validate_path(const char *_Nonnull path)
{
    size_t path_length = strlen(path);
    if (path_length > UINT16_MAX)
        return NOTE_ARCHIVE_TOO_LARGE;
    if (path_length == 0 || path[path_length - 1] == '/')
        return NOTE_ARCHIVE_INVALID;
    struct name_list *_Nullable names = nullptr;
    if (name_list_create(&names) != NAME_LIST_ACCEPTED)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    enum name_list_outcome valid = validate_segments(names, path);
    name_list_destroy(names);
    if (valid == NAME_LIST_OUT_OF_MEMORY)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    return valid == NAME_LIST_ACCEPTED ? NOTE_ARCHIVE_READY : NOTE_ARCHIVE_INVALID;
}
bool archive_assets_contains(const struct archive_assets *_Nonnull assets,
                             const char *_Nonnull path)
{
    for (size_t index = 0; index < assets->count; ++index)
    {
        const char *existing = assets->files[index].path;
        size_t cursor = 0;
        while (existing[cursor] != '\0' &&
               ascii_fold_byte(existing[cursor]) == ascii_fold_byte(path[cursor]))
            ++cursor;
        if (existing[cursor] == '\0' && path[cursor] == '\0')
            return true;
    }
    return false;
}
static bool grow(struct archive_assets *_Nonnull assets)
{
    if (assets->count < assets->capacity)
        return true;
    size_t capacity = assets->capacity == 0 ? 8 : assets->capacity * 2;
    struct archive_file *_Nullable files = realloc(assets->files, capacity * sizeof *files);
    if (files == nullptr)
        return false;
    assets->files = files;
    char *_Nullable *_Nullable paths = realloc(assets->paths, capacity * sizeof *paths);
    if (paths == nullptr)
        return false;
    assets->paths = paths;
    assets->capacity = capacity;
    return true;
}
enum note_archive_outcome archive_assets_append(struct archive_assets *_Nonnull assets,
                                                struct archive_file file)
{
    if (assets->count == note_archive_count_limit ||
        file.length > note_archive_byte_limit - assets->bytes)
        return NOTE_ARCHIVE_TOO_LARGE;
    enum note_archive_outcome valid = validate_path(file.path);
    if (valid != NOTE_ARCHIVE_READY)
        return valid;
    if (archive_assets_contains(assets, file.path))
        return NOTE_ARCHIVE_INVALID;
    if (!grow(assets))
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    char *_Nullable path = malloc(strlen(file.path) + 1);
    if (path == nullptr)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    memcpy(path, file.path, strlen(file.path) + 1);
    assets->paths[assets->count] = path;
    file.path = path;
    assets->files[assets->count++] = file;
    assets->bytes += file.length;
    return NOTE_ARCHIVE_READY;
}
size_t archive_assets_count(const struct archive_assets *_Nonnull assets)
{
    return assets->count;
}
const struct archive_file *_Nonnull archive_assets_at(const struct archive_assets *_Nonnull assets,
                                                      size_t index)
{
    return &assets->files[index];
}
void archive_assets_destroy(struct archive_assets *_Nullable assets)
{
    if (assets != nullptr)
    {
        for (size_t index = 0; index < assets->count; ++index)
            free(assets->paths[index]);
        free(assets->paths);
        free(assets->files);
    }
    free(assets);
}
