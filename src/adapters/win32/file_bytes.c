#include "file_bytes.h"
#include "file_write_kind.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

struct file_bytes
{
    char *_Nonnull data; /* 終端付き */
    size_t length;
    size_t offset; /* BOM の分だけ読み飛ばす */
};

/* 台帳としてあり得る上限。これを超えるファイルは壊れているとみなして読まない。 */
constexpr LONGLONG file_bytes_limit = 16 * 1024 * 1024;

static enum persistence_outcome open_outcome(void)
{
    DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
    {
        return PERSISTENCE_ABSENT;
    }
    return PERSISTENCE_UNREADABLE;
}

static bool read_all(HANDLE file, char *_Nonnull data, size_t length)
{
    size_t done = 0;
    while (done < length)
    {
        DWORD chunk = 0;
        DWORD wanted = (DWORD)(length - done > 0x10000000 ? 0x10000000 : length - done);
        if (!ReadFile(file, data + done, wanted, &chunk, nullptr) || chunk == 0)
        {
            return false;
        }
        done += chunk;
    }
    return true;
}

static enum persistence_outcome read_open_file(HANDLE file, struct file_bytes *_Nonnull bytes)
{
    LARGE_INTEGER size = {0};
    if (!GetFileSizeEx(file, &size) || size.QuadPart > file_bytes_limit)
    {
        return PERSISTENCE_UNREADABLE;
    }
    size_t length = (size_t)size.QuadPart;
    bytes->data = malloc(length + 1);
    if (bytes->data == nullptr)
    {
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    if (!read_all(file, bytes->data, length))
    {
        return PERSISTENCE_UNREADABLE;
    }
    bytes->data[length] = '\0';
    bytes->length = length;
    if (length >= 3 && memcmp(bytes->data, "\xEF\xBB\xBF", 3) == 0)
    {
        bytes->offset = 3;
    }
    return PERSISTENCE_LOADED;
}

enum persistence_outcome file_bytes_read(const wchar_t *_Nonnull path,
                                         struct file_bytes *_Nullable *_Nonnull out)
{
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return open_outcome();
    }
    struct file_bytes *_Nullable bytes = calloc(1, sizeof *bytes);
    enum persistence_outcome outcome =
        bytes == nullptr ? PERSISTENCE_OUT_OF_MEMORY : read_open_file(file, bytes);
    CloseHandle(file);
    if (outcome != PERSISTENCE_LOADED)
    {
        file_bytes_destroy(bytes);
        return outcome;
    }
    *out = bytes;
    return PERSISTENCE_LOADED;
}

static const wchar_t temporary_suffix[] = L".tmp";

/* mdが最大長でも、一時名の葉は固定長。存在する候補は奪わず別の番号で試す。 */
static bool temporary_name(const wchar_t *_Nonnull path, wchar_t *_Nonnull out, size_t capacity,
                           size_t attempt)
{
    size_t directory = 0;
    for (size_t index = 0; path[index] != L'\0'; ++index)
    {
        if (path[index] == L'\\' || path[index] == L'/')
        {
            directory = index + 1;
        }
    }
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t index = directory; path[index] != L'\0'; ++index)
    {
        hash ^= (uint16_t)path[index];
        hash *= UINT64_C(1099511628211);
    }
    static const wchar_t prefix[] = L".nenefolio-write-";
    static const wchar_t digits[] = L"0123456789abcdef";
    constexpr size_t prefix_length = sizeof prefix / sizeof prefix[0] - 1;
    constexpr size_t suffix_length = sizeof temporary_suffix / sizeof temporary_suffix[0];
    if (directory + prefix_length + 19 + suffix_length > capacity)
    {
        return false;
    }
    memcpy(out, path, directory * sizeof *out);
    memcpy(out + directory, prefix, prefix_length * sizeof *out);
    size_t position = directory + prefix_length;
    for (size_t index = 0; index < 16; ++index)
    {
        out[position + index] = digits[(hash >> ((15 - index) * 4)) & 15];
    }
    out[position + 16] = L'-';
    out[position + 17] = digits[attempt / 16];
    out[position + 18] = digits[attempt % 16];
    memcpy(out + position + 19, temporary_suffix, suffix_length * sizeof *out);
    return true;
}

static HANDLE temporary_file(const wchar_t *_Nonnull path, wchar_t *_Nonnull temporary,
                             size_t capacity)
{
    for (size_t attempt = 0; attempt < 256; ++attempt)
    {
        if (!temporary_name(path, temporary, capacity, attempt))
        {
            return INVALID_HANDLE_VALUE;
        }
        HANDLE file = CreateFileW(temporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file != INVALID_HANDLE_VALUE)
        {
            return file;
        }
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
        {
            return INVALID_HANDLE_VALUE;
        }
    }
    return INVALID_HANDLE_VALUE;
}

static bool write_all(HANDLE file, const char *_Nonnull data, size_t length)
{
    size_t done = 0;
    while (done < length)
    {
        DWORD chunk = 0;
        DWORD wanted = (DWORD)(length - done > 0x10000000 ? 0x10000000 : length - done);
        if (!WriteFile(file, data + done, wanted, &chunk, nullptr) || chunk == 0)
        {
            return false;
        }
        done += chunk;
    }
    return true;
}

static DWORD publish_flags(enum file_write_kind kind)
{
    switch (kind)
    {
    case FILE_WRITE_REPLACE:
        return MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    case FILE_WRITE_CREATE:
        return MOVEFILE_WRITE_THROUGH;
    }
    return MOVEFILE_WRITE_THROUGH;
}

static enum persistence_outcome publish(const wchar_t *_Nonnull temporary,
                                        const wchar_t *_Nonnull path, enum file_write_kind kind)
{
    if (MoveFileExW(temporary, path, publish_flags(kind)))
    {
        return PERSISTENCE_STORED;
    }
    DWORD error = GetLastError();
    DeleteFileW(temporary);
    return kind == FILE_WRITE_CREATE &&
                   (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS)
               ? PERSISTENCE_NAME_TAKEN
               : PERSISTENCE_UNWRITABLE;
}

static enum persistence_outcome store(const wchar_t *_Nonnull path, const char *_Nonnull data,
                                      size_t length, enum file_write_kind kind)
{
    wchar_t temporary[MAX_PATH * 4];
    HANDLE file = temporary_file(path, temporary, sizeof temporary / sizeof temporary[0]);
    if (file == INVALID_HANDLE_VALUE)
    {
        return PERSISTENCE_UNWRITABLE;
    }
    bool written = write_all(file, data, length) && FlushFileBuffers(file);
    CloseHandle(file);
    if (!written)
    {
        DeleteFileW(temporary);
        return PERSISTENCE_UNWRITABLE;
    }
    return publish(temporary, path, kind);
}

enum persistence_outcome file_bytes_store(const wchar_t *_Nonnull path, const char *_Nonnull data,
                                          size_t length)
{
    return store(path, data, length, FILE_WRITE_REPLACE);
}

enum persistence_outcome file_bytes_create(const wchar_t *_Nonnull path, const char *_Nonnull data,
                                           size_t length)
{
    return store(path, data, length, FILE_WRITE_CREATE);
}

const char *_Nonnull file_bytes_data(const struct file_bytes *_Nonnull bytes)
{
    return bytes->data + bytes->offset;
}

size_t file_bytes_length(const struct file_bytes *_Nonnull bytes)
{
    return bytes->length - bytes->offset;
}

void file_bytes_destroy(struct file_bytes *_Nullable bytes)
{
    if (bytes == nullptr)
    {
        return;
    }
    free(bytes->data);
    free(bytes);
}
