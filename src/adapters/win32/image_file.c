#include "image_file.h"
#include "image_limits.h"
#include "image_source.h"
#include "storage_native_path.h"
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
    if (swprintf_s(directory, storage_native_capacity, L"%ls\\%ls", root, category) < 0)
        return false;
    wchar_t combined[storage_native_capacity];
    int copied =
        destination[1] == L':'
            ? swprintf_s(combined, storage_native_capacity, L"%ls", destination)
            : swprintf_s(combined, storage_native_capacity, L"%ls\\%ls", directory, destination);
    if (copied < 0 || !local_disk(combined))
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
    if (swprintf_s(images, storage_native_capacity, L"%ls\\images", directory) < 0 ||
        !regular_components(images))
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
