#include "note_archive.h"
#include "archive_assets.h"
#include "archive_contents.h"
#include "name_list.h"
#include "note_text.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct note_archive
{
    char *_Nonnull bytes;
    size_t length;
};

static enum note_archive_outcome valid_names(const struct archive_note *_Nonnull note)
{
    struct name_list *_Nullable names = nullptr;
    if (name_list_create(&names) != NAME_LIST_ACCEPTED)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    enum name_list_outcome category =
        name_list_append(names, note->category, strlen(note->category));
    name_list_destroy(names);
    names = nullptr;
    if (name_list_create(&names) != NAME_LIST_ACCEPTED)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    enum name_list_outcome name = name_list_append(names, note->name, strlen(note->name));
    name_list_destroy(names);
    if (category == NAME_LIST_OUT_OF_MEMORY || name == NAME_LIST_OUT_OF_MEMORY)
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    return category == NAME_LIST_ACCEPTED && name == NAME_LIST_ACCEPTED ? NOTE_ARCHIVE_READY
                                                                        : NOTE_ARCHIVE_INVALID;
}

static size_t path_length(const struct archive_note *_Nonnull note)
{
    return strlen(note->category) + 1 + strlen(note->name) + 3;
}

static enum note_archive_outcome measure_notes(const struct archive_note *_Nonnull notes,
                                               size_t count, size_t *_Nonnull length)
{
    if (count > note_archive_count_limit)
        return NOTE_ARCHIVE_TOO_LARGE;
    *length = 22;
    for (size_t index = 0; index < count; ++index)
    {
        enum note_archive_outcome valid = valid_names(&notes[index]);
        if (valid != NOTE_ARCHIVE_READY)
            return valid;
        size_t size =
            76 + 2 * path_length(&notes[index]) + note_text_file_length(notes[index].body);
        if (size > note_archive_byte_limit - *length)
            return NOTE_ARCHIVE_TOO_LARGE;
        *length += size;
    }
    return NOTE_ARCHIVE_READY;
}

static void word(char *_Nonnull target, size_t offset, uint16_t value)
{
    target[offset] = (char)(value & 255);
    target[offset + 1] = (char)(value >> 8);
}

static void dword(char *_Nonnull target, size_t offset, uint32_t value)
{
    word(target, offset, (uint16_t)(value & 65535));
    word(target, offset + 2, (uint16_t)(value >> 16));
}

static uint32_t crc32(const char *_Nonnull bytes, size_t length)
{
    static constexpr uint32_t table[] = {0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
                                         0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
                                         0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
                                         0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c};
    uint32_t crc = UINT32_MAX;
    for (size_t index = 0; index < length; ++index)
    {
        crc ^= (unsigned char)bytes[index];
        crc = (crc >> 4) ^ table[crc & 15];
        crc = (crc >> 4) ^ table[crc & 15];
    }
    return ~crc;
}

static void path(char *_Nonnull target, const struct archive_note *_Nonnull note)
{
    size_t category = strlen(note->category);
    size_t name = strlen(note->name);
    memcpy(target, note->category, category);
    target[category] = '/';
    memcpy(target + category + 1, note->name, name);
    memcpy(target + category + 1 + name, ".md", 3);
    target[category + 1 + name + 3] = '\0';
}

static void local_record(char *_Nonnull target, const struct archive_file *_Nonnull file,
                         uint32_t crc)
{
    dword(target, 0, UINT32_C(0x04034b50));
    word(target, 4, 20);
    word(target, 6, 0x0800);
    word(target, 12, 33); /* 1980-01-01; no environmental timestamp. */
    dword(target, 14, crc);
    dword(target, 18, (uint32_t)file->length);
    dword(target, 22, (uint32_t)file->length);
    size_t name = strlen(file->path);
    word(target, 26, (uint16_t)name);
    memcpy(target + 30, file->path, name);
    memcpy(target + 30 + name, file->bytes, file->length);
}

static void central_record(char *_Nonnull target, const struct archive_file *_Nonnull file,
                           uint32_t crc, uint32_t offset)
{
    dword(target, 0, UINT32_C(0x02014b50));
    word(target, 4, 20);
    word(target, 6, 20);
    word(target, 8, 0x0800);
    word(target, 14, 33);
    dword(target, 16, crc);
    dword(target, 20, (uint32_t)file->length);
    dword(target, 24, (uint32_t)file->length);
    size_t name = strlen(file->path);
    word(target, 28, (uint16_t)name);
    dword(target, 42, offset);
    memcpy(target + 46, file->path, name);
}

static size_t entry_count(const struct archive_contents *_Nonnull contents)
{
    return contents->count +
           (contents->assets == nullptr ? 0 : archive_assets_count(contents->assets));
}
static struct archive_file entry_at(const struct archive_contents *_Nonnull contents, size_t index,
                                    char *_Nonnull name)
{
    if (index < contents->count)
    {
        const struct archive_note *note = &contents->notes[index];
        path(name, note);
        return (struct archive_file){.path = name,
                                     .bytes = note_text_file_bytes(note->body),
                                     .length = note_text_file_length(note->body)};
    }
    return *archive_assets_at(contents->assets, index - contents->count);
}
static size_t central_start(const struct archive_contents *_Nonnull contents)
{
    size_t length = 0;
    for (size_t index = 0; index < entry_count(contents); ++index)
    {
        char name[2 * name_list_max_length + 5];
        struct archive_file file = entry_at(contents, index, name);
        length += 30 + strlen(file.path) + file.length;
    }
    return length;
}
static void records(char *_Nonnull target, const struct archive_contents *_Nonnull contents)
{
    size_t start = central_start(contents);
    size_t local = 0;
    size_t central = start;
    size_t count = entry_count(contents);
    for (size_t index = 0; index < count; ++index)
    {
        char name[2 * name_list_max_length + 5];
        struct archive_file file = entry_at(contents, index, name);
        uint32_t crc = crc32(file.bytes, file.length);
        local_record(target + local, &file, crc);
        central_record(target + central, &file, crc, (uint32_t)local);
        local += 30 + strlen(file.path) + file.length;
        central += 46 + strlen(file.path);
    }
    dword(target + central, 0, UINT32_C(0x06054b50));
    word(target + central, 8, (uint16_t)count);
    word(target + central, 10, (uint16_t)count);
    dword(target + central, 12, (uint32_t)(central - start));
    dword(target + central, 16, (uint32_t)start);
}
static enum note_archive_outcome measure(const struct archive_contents *_Nonnull contents,
                                         size_t *_Nonnull length)
{
    enum note_archive_outcome outcome = measure_notes(contents->notes, contents->count, length);
    if (outcome != NOTE_ARCHIVE_READY || contents->assets == nullptr)
        return outcome;
    if (archive_assets_count(contents->assets) > note_archive_count_limit - contents->count)
        return NOTE_ARCHIVE_TOO_LARGE;
    for (size_t index = 0; index < archive_assets_count(contents->assets); ++index)
    {
        const struct archive_file *file = archive_assets_at(contents->assets, index);
        size_t overhead = 76 + 2 * strlen(file->path);
        if (file->length > note_archive_byte_limit - overhead ||
            overhead + file->length > note_archive_byte_limit - *length)
            return NOTE_ARCHIVE_TOO_LARGE;
        *length += overhead + file->length;
    }
    return NOTE_ARCHIVE_READY;
}

static void destroy_prepared(struct note_archive *_Nullable archive, char *_Nullable bytes)
{
    free(archive);
    free(bytes);
}

enum note_archive_outcome note_archive_create(const struct archive_note *_Nonnull notes,
                                              size_t count,
                                              const struct archive_assets *_Nullable assets,
                                              struct note_archive *_Nullable *_Nonnull out)
{
    size_t length = 0;
    struct archive_contents contents = {.notes = notes, .count = count, .assets = assets};
    enum note_archive_outcome measured = measure(&contents, &length);
    if (measured != NOTE_ARCHIVE_READY)
        return measured;
    struct note_archive *_Nullable archive = malloc(sizeof *archive);
    char *_Nullable bytes = calloc(length, 1);
    if (archive == nullptr || bytes == nullptr)
    {
        destroy_prepared(archive, bytes);
        return NOTE_ARCHIVE_OUT_OF_MEMORY;
    }
    records(bytes, &contents);
    *archive = (struct note_archive){.bytes = bytes, .length = length};
    *out = archive;
    return NOTE_ARCHIVE_READY;
}

const char *_Nonnull note_archive_bytes(const struct note_archive *_Nonnull archive)
{
    return archive->bytes;
}

size_t note_archive_length(const struct note_archive *_Nonnull archive)
{
    return archive->length;
}

void note_archive_destroy(struct note_archive *_Nullable archive)
{
    if (archive != nullptr)
        free(archive->bytes);
    free(archive);
}
