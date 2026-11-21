#include "image_file.h"
#include "file_bytes.h"
#include "image_limits.h"
#include "image_link.h"
#include "image_signature.h"
#include "image_source.h"
#include "storage_native_path.h"
#include "text_buffer.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct image_file
{
    unsigned char *_Nullable bytes;
    size_t length;
    char identity[storage_native_capacity * 4];
    char relative[storage_native_capacity * 4];
};
static bool local_disk(const wchar_t *_Nonnull path)
{
    if (wcslen(path) < 3 || path[0] == L'\\' || path[1] != L':' ||
        (path[2] != L'/' && path[2] != L'\\'))
        return false;
    wchar_t drive[] = {path[0], L':', L'\\', L'\0'};
    UINT type = GetDriveTypeW(drive);
    return type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_RAMDISK ||
           type == DRIVE_CDROM;
}
/* Inspect parents before crossing them; reject junctions, symlinks and cloud placeholders.
 * The final opened handle is checked again. */
static bool regular_components(const wchar_t *_Nonnull path)
{
    wchar_t prefix[storage_native_capacity];
    size_t length = wcslen(path);
    if (length >= storage_native_capacity || !local_disk(path))
        return false;
    memcpy(prefix, path, (length + 1) * sizeof *path);
    for (size_t index = 3; index <= length; ++index)
    {
        if (path[index] != L'\\' && path[index] != L'\0')
            continue;
        prefix[index] = L'\0';
        DWORD attributes = GetFileAttributesW(prefix);
        prefix[index] = path[index];
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            return false;
    }
    return true;
}
/* The paths below are built from names a note or another program supplies. swprintf_s is not
 * used for them: when the text does not fit it does not return an error, it ends the process
 * (#1129). These two return false instead, and the image is one that cannot be read. */
static bool copied(wchar_t *_Nonnull out, const wchar_t *_Nonnull text)
{
    size_t length = wcslen(text);
    if (length + 1 > storage_native_capacity)
        return false;
    memmove(out, text, (length + 1) * sizeof *out);
    return true;
}

/* head\tail into out (storage_native_capacity units); out may be head. */
static bool joined(wchar_t *_Nonnull out, const wchar_t *_Nonnull head,
                   const wchar_t *_Nonnull tail)
{
    size_t first = wcslen(head);
    size_t second = wcslen(tail);
    if (first + 1 + second + 1 > storage_native_capacity)
        return false;
    memmove(out, head, first * sizeof *out);
    out[first] = L'\\';
    memcpy(out + first + 1, tail, (second + 1) * sizeof *out);
    return true;
}

static bool path_for(const wchar_t *_Nonnull root, const struct image_request *_Nonnull request,
                     wchar_t *_Nonnull path, wchar_t *_Nonnull directory)
{
    wchar_t category[256];
    wchar_t destination[image_source_byte_limit + 1];
    size_t length = 0;
    if (utf16_text_fill(request->category, category, 256, &length) != UTF16_TEXT_FILL_READY ||
        utf16_text_fill(image_source_target(request->source), destination,
                        image_source_byte_limit + 1, &length) != UTF16_TEXT_FILL_READY)
        return false;
    if (wcsncmp(root, L"\\\\?\\", 4) == 0)
        root += 4;
    if (!joined(directory, root, category))
        return false;
    wchar_t combined[storage_native_capacity];
    bool fits = destination[1] == L':' ? copied(combined, destination)
                                       : joined(combined, directory, destination);
    if (!fits || !local_disk(combined))
        return false;
    DWORD resolved = GetFullPathNameW(combined, storage_native_capacity, path, nullptr);
    return resolved != 0 && resolved < storage_native_capacity;
}
static bool final_path(HANDLE _Nonnull file, wchar_t *_Nonnull target)
{
    FILE_ATTRIBUTE_TAG_INFO attributes;
    if (!GetFileInformationByHandleEx(file, FileAttributeTagInfo, &attributes, sizeof attributes) ||
        (attributes.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        return false;
    DWORD length =
        GetFinalPathNameByHandleW(file, target, storage_native_capacity, FILE_NAME_NORMALIZED);
    if (length == 0 || length >= storage_native_capacity || wcsncmp(target, L"\\\\?\\", 4) != 0)
        return false;
    memmove(target, target + 4, (length - 3) * sizeof *target);
    return local_disk(target);
}
static size_t inside_images(const wchar_t *_Nonnull directory, const wchar_t *_Nonnull target)
{
    wchar_t images[storage_native_capacity];
    if (!joined(images, directory, L"images") || !regular_components(images))
        return 0;
    HANDLE folder = CreateFileW(
        images, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (folder == INVALID_HANDLE_VALUE)
        return 0;
    wchar_t resolved[storage_native_capacity];
    bool ready = final_path(folder, resolved);
    CloseHandle(folder);
    size_t length = ready ? wcslen(resolved) : 0;
    return ready && _wcsnicmp(resolved, target, length) == 0 && target[length] == L'\\' ? length
                                                                                        : 0;
}
static bool copy_utf8(const wchar_t *_Nonnull source, char *_Nonnull target, size_t capacity)
{
    struct utf8_text *_Nullable value = nullptr;
    if (utf8_text_create(source, wcslen(source), &value) != UTF8_TEXT_CONVERTED)
        return false;
    size_t length = utf8_text_length(value);
    bool fits = length < capacity;
    if (fits)
        memcpy(target, utf8_text_bytes(value), length + 1);
    utf8_text_destroy(value);
    return fits;
}
static bool describe(struct image_file *_Nonnull file, const wchar_t *_Nonnull resolved,
                     size_t prefix)
{
    if (!copy_utf8(resolved, file->identity, sizeof file->identity))
        return false;
    if (prefix == 0)
        return true;
    if (!copy_utf8(resolved + prefix, file->relative + 6, sizeof file->relative - 6))
        return false;
    memcpy(file->relative, "images", 6);
    for (size_t index = 0; file->relative[index] != '\0'; ++index)
        if (file->relative[index] == '\\')
            file->relative[index] = '/';
    return true;
}
static enum image_outcome read_bytes(HANDLE _Nonnull handle, struct image_file *_Nonnull file)
{
    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size) || size.QuadPart <= 0 ||
        size.QuadPart > (LONGLONG)image_input_byte_limit)
        return IMAGE_TOO_LARGE;
    file->bytes = malloc((size_t)size.QuadPart);
    if (file->bytes == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    while (file->length < (size_t)size.QuadPart)
    {
        size_t remaining = (size_t)size.QuadPart - file->length;
        DWORD wanted = (DWORD)(remaining < 65536 ? remaining : 65536);
        DWORD read = 0;
        if (!ReadFile(handle, file->bytes + file->length, wanted, &read, nullptr) || read == 0)
            return IMAGE_FAILED;
        file->length += read;
    }
    return IMAGE_READY;
}
static enum image_outcome read_opened(HANDLE _Nonnull handle, const wchar_t *_Nonnull directory,
                                      enum image_export_policy policy,
                                      struct image_file *_Nonnull file)
{
    wchar_t resolved[storage_native_capacity];
    if (!final_path(handle, resolved))
        return IMAGE_UNSUPPORTED;
    size_t prefix = inside_images(directory, resolved);
    if (policy != IMAGE_EXPORT_ALL_LOCAL && prefix == 0)
        return IMAGE_SKIPPED;
    if (!describe(file, resolved, prefix))
        return IMAGE_OUT_OF_MEMORY;
    return read_bytes(handle, file);
}
enum image_outcome image_file_read(const wchar_t *_Nonnull root,
                                   const struct image_request *_Nonnull request,
                                   enum image_export_policy policy,
                                   struct image_file *_Nullable *_Nonnull out)
{
    wchar_t path[storage_native_capacity];
    wchar_t directory[storage_native_capacity];
    if (image_source_kind(request->source) != IMAGE_SOURCE_LOCAL ||
        !path_for(root, request, path, directory))
        return IMAGE_UNSUPPORTED;
    if (!regular_components(path))
        return policy != IMAGE_EXPORT_ALL_LOCAL && !image_source_recommended(request->source)
                   ? IMAGE_SKIPPED
                   : IMAGE_FAILED;
    HANDLE handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return IMAGE_FAILED;
    struct image_file *_Nullable file = calloc(1, sizeof *file);
    enum image_outcome outcome =
        file == nullptr ? IMAGE_OUT_OF_MEMORY : read_opened(handle, directory, policy, file);
    CloseHandle(handle);
    if (outcome != IMAGE_READY)
    {
        image_file_destroy(file);
        return outcome;
    }
    *out = file;
    return IMAGE_READY;
}
const unsigned char *_Nonnull image_file_bytes(const struct image_file *_Nonnull file)
{
    return file->bytes;
}
size_t image_file_length(const struct image_file *_Nonnull file)
{
    return file->length;
}
bool image_file_contained(const struct image_file *_Nonnull file)
{
    return file->relative[0] != '\0';
}
const char *_Nonnull image_file_identity(const struct image_file *_Nonnull file)
{
    return file->identity;
}
const char *_Nonnull image_file_relative(const struct image_file *_Nonnull file)
{
    return file->relative;
}
void image_file_destroy(struct image_file *_Nullable file)
{
    if (file != nullptr)
        free(file->bytes);
    free(file);
}

/* ---- #1062 / ADR0120: taking a dropped image into images\ ---- */

/* head\tail into out (storage_native_capacity units). False when it does not fit: the names
 * come from outside (a dropped file's own name), and swprintf_s would end the process. */
static bool joined(wchar_t *_Nonnull out, const wchar_t *_Nonnull head, const wchar_t *_Nonnull tail)
{
    size_t first = wcslen(head);
    size_t second = wcslen(tail);
    if (first + 1 + second + 1 > storage_native_capacity)
        return false;
    wchar_t built[storage_native_capacity];
    memcpy(built, head, first * sizeof *built);
    built[first] = L'\\';
    memcpy(built + first + 1, tail, (second + 1) * sizeof *built);
    memcpy(out, built, (first + 1 + second + 1) * sizeof *out); /* out may be head */
    return true;
}

/* A symbolic link or a junction: its target is not followed (ADR0120 decision 4). */
static bool link_source(const wchar_t *_Nonnull source)
{
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(source, &found);
    if (search == INVALID_HANDLE_VALUE)
        return false;
    FindClose(search);
    return (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
           (found.dwReserved0 == IO_REPARSE_TAG_SYMLINK ||
            found.dwReserved0 == IO_REPARSE_TAG_MOUNT_POINT);
}

/* The category's folder, without the long-path prefix (as path_for builds it). */
static bool category_directory(const wchar_t *_Nonnull root, const char *_Nonnull category,
                               wchar_t *_Nonnull directory)
{
    wchar_t name[256];
    size_t length = 0;
    if (utf16_text_fill(category, name, 256, &length) != UTF16_TEXT_FILL_READY)
        return false;
    if (wcsncmp(root, L"\\\\?\\", 4) == 0)
        root += 4;
    return joined(directory, root, name);
}

/* The opened source's bytes, within the size an image may have. An empty file is no image. */
static enum image_adopt_outcome read_opened_source(HANDLE _Nonnull handle,
                                                   struct image_file *_Nonnull file)
{
    LARGE_INTEGER size;
    if (!GetFileSizeEx(handle, &size))
        return IMAGE_ADOPT_UNREADABLE;
    if (size.QuadPart == 0)
        return IMAGE_ADOPT_NOT_IMAGE;
    if (size.QuadPart > (LONGLONG)image_input_byte_limit)
        return IMAGE_ADOPT_TOO_LARGE;
    switch (read_bytes(handle, file))
    {
    case IMAGE_READY:
        return IMAGE_ADOPT_COPIED;
    case IMAGE_OUT_OF_MEMORY:
        return IMAGE_ADOPT_OUT_OF_MEMORY;
    case IMAGE_PENDING:
    case IMAGE_FAILED:
    case IMAGE_UNSUPPORTED:
    case IMAGE_TOO_LARGE:
    case IMAGE_SKIPPED:
    case IMAGE_CANCELLED:
        return IMAGE_ADOPT_UNREADABLE;
    }
    return IMAGE_ADOPT_UNREADABLE;
}

/* Reads the whole source. A directory (it exists but does not open as a file) is not an image.
 * COPIED here means "read": the handle stays open for the caller. */
static enum image_adopt_outcome read_source(const wchar_t *_Nonnull source,
                                            struct image_file *_Nonnull file,
                                            HANDLE _Nonnull *_Nonnull opened)
{
    if (link_source(source))
        return IMAGE_ADOPT_NOT_IMAGE;
    HANDLE handle = CreateFileW(source, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return GetFileAttributesW(source) != INVALID_FILE_ATTRIBUTES ? IMAGE_ADOPT_NOT_IMAGE
                                                                     : IMAGE_ADOPT_UNREADABLE;
    enum image_adopt_outcome outcome = read_opened_source(handle, file);
    if (outcome != IMAGE_ADOPT_COPIED)
    {
        CloseHandle(handle);
        return outcome;
    }
    *opened = handle;
    return IMAGE_ADOPT_COPIED;
}

/* Whether the file at `path` holds exactly these bytes. A file that cannot be read, or is larger
 * than an image may be, is another file. */
static bool same_content(const wchar_t *_Nonnull path, const struct image_file *_Nonnull wanted)
{
    HANDLE handle = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    struct image_file existing = {0};
    bool same = read_bytes(handle, &existing) == IMAGE_READY && existing.length == wanted->length &&
                memcmp(existing.bytes, wanted->bytes, wanted->length) == 0;
    CloseHandle(handle);
    free(existing.bytes);
    return same;
}

/* One name in images\: taken with the same bytes (PRESENT), free and written (COPIED), or not
 * writable. When the name is held by another file, *taken is set and the caller tries the
 * next name; the outcome then says nothing. */
static enum image_adopt_outcome place_as(const wchar_t *_Nonnull images, const char *_Nonnull name,
                                         const struct image_file *_Nonnull file,
                                         bool *_Nonnull taken)
{
    *taken = false;
    wchar_t wide[512];
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    if (utf16_text_fill(name, wide, 512, &length) != UTF16_TEXT_FILL_READY ||
        !joined(path, images, wide))
        return IMAGE_ADOPT_NOT_STORED;
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES)
    {
        *taken = !same_content(path, file);
        return IMAGE_ADOPT_PRESENT;
    }
    switch (file_bytes_create_within(path, (const char *)file->bytes, file->length,
                                     image_input_byte_limit))
    {
    case PERSISTENCE_STORED:
        return IMAGE_ADOPT_COPIED;
    case PERSISTENCE_NAME_TAKEN:
        *taken = true; /* made by someone else just now: the next name */
        return IMAGE_ADOPT_PRESENT;
    case PERSISTENCE_OUT_OF_MEMORY:
        return IMAGE_ADOPT_OUT_OF_MEMORY;
    case PERSISTENCE_LOADED:
    case PERSISTENCE_ABSENT:
    case PERSISTENCE_UNREADABLE:
    case PERSISTENCE_MALFORMED:
    case PERSISTENCE_UNWRITABLE:
        return IMAGE_ADOPT_NOT_STORED;
    }
    return IMAGE_ADOPT_NOT_STORED;
}

/* The `number`th name to try: the file's own for 1, then name-2, name-3 ... `file->identity`
 * holds the file's own name. The caller destroys it; nullptr when there is no memory. */
static struct text_buffer *_Nullable numbered_name(const struct image_file *_Nonnull file,
                                                   size_t number)
{
    struct text_buffer *_Nullable candidate = nullptr;
    if (text_buffer_create(&candidate) != TEXT_BUFFER_ACCEPTED)
        return nullptr;
    if (number == 1)
        text_buffer_append_text(candidate, file->identity);
    else
        image_link_numbered(candidate, file->identity, strlen(file->identity), number);
    if (text_buffer_finish(candidate) != TEXT_BUFFER_ACCEPTED)
    {
        text_buffer_destroy(candidate);
        return nullptr;
    }
    return candidate;
}

/* The file is in images\ under some name: written now, or its bytes were there already. */
static bool in_images(enum image_adopt_outcome outcome)
{
    return outcome == IMAGE_ADOPT_COPIED || outcome == IMAGE_ADOPT_PRESENT;
}

/* The first free name (ADR0120 decision 3). The name the file ends under is added to `leaf`. */
static enum image_adopt_outcome place(const wchar_t *_Nonnull images,
                                      const struct image_file *_Nonnull file,
                                      struct text_buffer *_Nonnull leaf)
{
    constexpr size_t attempts = 1000;
    for (size_t number = 1; number <= attempts; ++number)
    {
        struct text_buffer *_Nullable candidate = numbered_name(file, number);
        if (candidate == nullptr)
            return IMAGE_ADOPT_OUT_OF_MEMORY;
        bool taken = false;
        enum image_adopt_outcome placed =
            place_as(images, text_buffer_bytes(candidate), file, &taken);
        if (!taken && in_images(placed))
            text_buffer_append_text(leaf, text_buffer_bytes(candidate));
        text_buffer_destroy(candidate);
        if (!taken)
            return placed;
    }
    return IMAGE_ADOPT_NOT_STORED;
}

/* A source that already lies in images\ is pointed at as it is: its path from images\, with `/`. */
static bool already_inside(HANDLE _Nonnull source, const wchar_t *_Nonnull directory,
                           struct text_buffer *_Nonnull leaf)
{
    wchar_t resolved[storage_native_capacity];
    if (!final_path(source, resolved))
        return false;
    size_t prefix = inside_images(directory, resolved);
    char relative[storage_native_capacity * 3];
    if (prefix == 0 || !copy_utf8(resolved + prefix + 1, relative, sizeof relative))
        return false;
    for (size_t index = 0; relative[index] != '\0'; ++index)
        if (relative[index] == '\\')
            relative[index] = '/';
    text_buffer_append_text(leaf, relative);
    return true;
}

/* images\ of the category, made if it is not there. It must be a plain folder on a local disk. */
static bool images_folder(const wchar_t *_Nonnull directory, wchar_t *_Nonnull images)
{
    if (!joined(images, directory, L"images"))
        return false;
    if (!CreateDirectoryW(images, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    return regular_components(images);
}

enum image_adopt_outcome image_file_adopt(const wchar_t *_Nonnull root,
                                          const char *_Nonnull category,
                                          const wchar_t *_Nonnull source,
                                          struct text_buffer *_Nonnull leaf)
{
    struct image_file file = {0};
    HANDLE opened = INVALID_HANDLE_VALUE;
    enum image_adopt_outcome outcome = read_source(source, &file, &opened);
    if (outcome != IMAGE_ADOPT_COPIED)
    {
        free(file.bytes);
        return outcome;
    }
    wchar_t directory[storage_native_capacity];
    wchar_t images[storage_native_capacity];
    const wchar_t *last = wcsrchr(source, L'\\');
    if (!image_signature_supported(file.bytes, file.length))
        outcome = IMAGE_ADOPT_NOT_IMAGE;
    else if (!category_directory(root, category, directory) || !images_folder(directory, images) ||
             !copy_utf8(last != nullptr ? last + 1 : source, file.identity, sizeof file.identity))
        outcome = IMAGE_ADOPT_NOT_STORED;
    else if (already_inside(opened, directory, leaf))
        outcome = IMAGE_ADOPT_PRESENT;
    else
        outcome = place(images, &file, leaf);
    CloseHandle(opened);
    free(file.bytes);
    return outcome;
}
