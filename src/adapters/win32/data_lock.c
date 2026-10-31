#include "data_lock.h"
#include "storage_native_path.h"
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

struct data_lock
{
    HANDLE handle;
};

enum data_lock_outcome data_lock_open(const wchar_t *_Nonnull root,
                                      struct data_lock *_Nullable *_Nonnull out)
{
    wchar_t path[storage_native_capacity];
    static const wchar_t leaf[] = L".nenefolio.lock";
    size_t length = wcslen(root);
    size_t gap = length > 0 && root[length - 1] != L'\\' ? 1 : 0;
    if (length + gap + sizeof leaf / sizeof leaf[0] > storage_native_capacity)
        return DATA_LOCK_UNAVAILABLE;
    memcpy(path, root, length * sizeof *path);
    if (gap != 0)
        path[length] = L'\\';
    memcpy(path + length + gap, leaf, sizeof leaf);
    HANDLE handle = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return GetLastError() == ERROR_SHARING_VIOLATION ? DATA_LOCK_IN_USE : DATA_LOCK_UNAVAILABLE;
    struct data_lock *_Nullable lock = malloc(sizeof *lock);
    if (lock == nullptr)
    {
        CloseHandle(handle);
        return DATA_LOCK_OUT_OF_MEMORY;
    }
    lock->handle = handle;
    *out = lock;
    return DATA_LOCK_ACQUIRED;
}

void data_lock_destroy(struct data_lock *_Nullable lock)
{
    if (lock != nullptr)
        CloseHandle(lock->handle);
    free(lock);
}
