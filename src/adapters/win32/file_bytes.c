#include "file_bytes.h"

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
