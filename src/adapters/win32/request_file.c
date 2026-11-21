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
};

static ULONGLONG wide(DWORD high, DWORD low)
{
    return ((ULONGLONG)high << 32) | (ULONGLONG)low;
}

/* folder\leaf into out (request_path_capacity units). False when it does not fit: the names
 * come from outside, and nothing here may fail harder than "not this file". */
static bool join(wchar_t *_Nonnull out, const wchar_t *_Nonnull folder,
                 const wchar_t *_Nonnull leaf)
{
    size_t head = wcslen(folder);
    size_t tail = wcslen(leaf);
    if (head + 1 + tail + 1 > request_path_capacity)
        return false;
    memcpy(out, folder, head * sizeof *out);
    out[head] = L'\\';
    memcpy(out + head + 1, leaf, (tail + 1) * sizeof *out);
    return true;
}

/* Removes the file, also one marked read-only (a writer on another system may leave that). */
static bool remove_file(const wchar_t *_Nonnull path)
{
    if (DeleteFileW(path) != 0)
        return true;
    return SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL) != 0 && DeleteFileW(path) != 0;
}

static ULONGLONG now(void)
{
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    return wide(time.dwHighDateTime, time.dwLowDateTime);
}

/* Keeps the oldest candidate_limit files, oldest first. */
static size_t keep_oldest(struct request_file *_Nonnull kept, size_t count,
                          const wchar_t *_Nonnull name, ULONGLONG written)
{
    size_t at = count;
    while (at > 0 && kept[at - 1].written > written)
        at -= 1;
    if (at == candidate_limit)
        return count; /* newer than everything kept, and the list is full */
    size_t last = count < candidate_limit ? count : candidate_limit - 1;
    memmove(kept + at + 1, kept + at, (last - at) * sizeof *kept);
    wcsncpy_s(kept[at].name, MAX_PATH, name, _TRUNCATE);
    kept[at].written = written;
    return last + 1;
}

/* Too old or too large to be opened: such a file is removed unread while the folder is listed,
 * so it never takes the place of a request that can be opened. */
static bool unwanted(const WIN32_FIND_DATAW *_Nonnull found, ULONGLONG written, size_t capacity)
{
    ULONGLONG current = now();
    bool stale = current > written && current - written > stale_after;
    return stale || wide(found->nFileSizeHigh, found->nFileSizeLow) > capacity;
}

static size_t gather(const wchar_t *_Nonnull folder, size_t capacity,
                     struct request_file *_Nonnull kept)
{
    wchar_t path[request_path_capacity];
    if (!join(path, folder, L"open-*.json"))
        return 0;
    WIN32_FIND_DATAW found;
    HANDLE search = FindFirstFileW(path, &found);
    if (search == INVALID_HANDLE_VALUE)
        return 0;
    size_t count = 0;
    do
    {
        ULONGLONG written =
            wide(found.ftLastWriteTime.dwHighDateTime, found.ftLastWriteTime.dwLowDateTime);
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            continue;
        if (!unwanted(&found, written, capacity))
            count = keep_oldest(kept, count, found.cFileName, written);
        else if (join(path, folder, found.cFileName))
            (void)remove_file(path); /* one that cannot be removed is met again, and ignored */
    } while (FindNextFileW(search, &found) != 0);
    FindClose(search);
    return count;
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
    /* Only what could be removed is handed over: a file that stays would be opened again by
     * every later look. One that grew past the limit since it was listed is only removed. */
    if (remove_file(path) && size <= capacity)
    {
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
    size_t count = gather(folder, capacity, kept);
    enum request_outcome outcome = REQUEST_NONE;
    for (size_t at = 0; at < count && outcome == REQUEST_NONE; ++at)
    {
        wchar_t path[request_path_capacity];
        if (join(path, folder, kept[at].name))
            outcome = consume(path, buffer, capacity, length);
    }
    return outcome;
}
