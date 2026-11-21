#include "request_file.h"
#include "file_bytes.h"
#include <string.h>
#include <wchar.h>
#include <windows.h>

constexpr size_t candidate_limit = 32;
constexpr size_t request_path_capacity = 1024;
/* A request not handled within five minutes is no longer what its writer is waiting for. */
constexpr ULONGLONG stale_after = 5ULL * 60ULL * 10000000ULL; /* in 100 ns */

struct request_file
{
    wchar_t name[MAX_PATH];
    ULONGLONG written;
    ULONGLONG size;
};

static ULONGLONG wide(DWORD high, DWORD low)
{
    return ((ULONGLONG)high << 32) | (ULONGLONG)low;
}

/* Keeps the oldest candidate_limit files, oldest first. */
static size_t keep_oldest(struct request_file *_Nonnull kept, size_t count,
                          const WIN32_FIND_DATAW *_Nonnull found)
{
    ULONGLONG written =
        wide(found->ftLastWriteTime.dwHighDateTime, found->ftLastWriteTime.dwLowDateTime);
    size_t at = count;
    while (at > 0 && kept[at - 1].written > written)
        at -= 1;
    if (at == candidate_limit)
        return count; /* newer than everything kept, and the list is full */
    size_t last = count < candidate_limit ? count : candidate_limit - 1;
    memmove(kept + at + 1, kept + at, (last - at) * sizeof *kept);
    wcsncpy_s(kept[at].name, MAX_PATH, found->cFileName, _TRUNCATE);
    kept[at].written = written;
    kept[at].size = wide(found->nFileSizeHigh, found->nFileSizeLow);
    return last + 1;
}

static size_t gather(const wchar_t *_Nonnull folder, struct request_file *_Nonnull kept)
{
    wchar_t pattern[request_path_capacity];
    if (swprintf_s(pattern, request_path_capacity, L"%ls\\open-*.json", folder) < 0)
        return 0;
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return 0;
    size_t count = 0;
    do
    {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            count = keep_oldest(kept, count, &found);
    } while (FindNextFileW(search, &found) != 0);
    FindClose(search);
    return count;
}

static ULONGLONG now(void)
{
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    return wide(time.dwHighDateTime, time.dwLowDateTime);
}

/* Too old or too large to be read at all. */
static bool unwanted(const struct request_file *_Nonnull file, ULONGLONG current, size_t capacity)
{
    bool stale = current > file->written && current - file->written > stale_after;
    return stale || file->size > capacity;
}

/* Removes the file; NONE when it cannot be removed (the caller looks at the next one). */
static enum request_outcome discard(const wchar_t *_Nonnull path)
{
    return DeleteFileW(path) != 0 ? REQUEST_DISCARDED : REQUEST_NONE;
}

/* Reads and removes one file. NONE means "not this one, look at the next". */
static enum request_outcome consume(const wchar_t *_Nonnull path, char *_Nonnull buffer,
                                    size_t capacity, size_t *_Nonnull length)
{
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome read = file_bytes_read(path, &bytes);
    if (read == PERSISTENCE_OUT_OF_MEMORY)
        return REQUEST_OUT_OF_MEMORY;
    if (read != PERSISTENCE_LOADED || bytes == nullptr)
        return REQUEST_NONE; /* being written, or gone a moment ago */
    size_t size = file_bytes_length(bytes);
    enum request_outcome outcome = REQUEST_NONE;
    if (size > capacity)
        outcome = discard(path); /* it grew since it was listed */
    else if (DeleteFileW(path) != 0)
    {
        /* Only what could be removed is handed over: a file that stays would be opened again
         * by every later look. */
        memcpy(buffer, file_bytes_data(bytes), size);
        *length = size;
        outcome = REQUEST_TAKEN;
    }
    file_bytes_destroy(bytes);
    return outcome;
}

enum request_outcome request_file_take(const wchar_t *_Nonnull folder, char *_Nonnull buffer,
                                       size_t capacity, size_t *_Nonnull length)
{
    struct request_file kept[candidate_limit];
    size_t count = gather(folder, kept);
    ULONGLONG current = now();
    enum request_outcome outcome = REQUEST_NONE;
    for (size_t at = 0; at < count && outcome == REQUEST_NONE; ++at)
    {
        wchar_t path[request_path_capacity];
        if (swprintf_s(path, request_path_capacity, L"%ls\\%ls", folder, kept[at].name) < 0)
            continue;
        outcome = unwanted(&kept[at], current, capacity) ? discard(path)
                                                         : consume(path, buffer, capacity, length);
    }
    return outcome;
}
