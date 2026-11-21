#include "data_watch.h"
#include "storage_native_path.h"
#include <stdlib.h>
#include <windows.h>

struct data_watch
{
    HANDLE _Nullable handle; /* nullptr once the notices could not be asked for again */
};

enum data_watch_outcome data_watch_open(const struct storage_path *_Nonnull root,
                                        struct data_watch *_Nullable *_Nonnull out)
{
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    if (!storage_native_path_fill(root, path, &length))
        return DATA_WATCH_UNAVAILABLE;
    HANDLE handle = FindFirstChangeNotificationW(path, TRUE,
                                                 FILE_NOTIFY_CHANGE_FILE_NAME |
                                                     FILE_NOTIFY_CHANGE_DIR_NAME |
                                                     FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (handle == INVALID_HANDLE_VALUE)
        return DATA_WATCH_UNAVAILABLE;
    struct data_watch *_Nullable watch = malloc(sizeof *watch);
    if (watch == nullptr)
    {
        FindCloseChangeNotification(handle);
        return DATA_WATCH_OUT_OF_MEMORY;
    }
    watch->handle = handle;
    *out = watch;
    return DATA_WATCH_OPENED;
}

/* No more notices from here on: the wait goes on with messages alone. */
static void stop_notices(struct data_watch *_Nonnull watch)
{
    if (watch->handle != nullptr)
        FindCloseChangeNotification(watch->handle);
    watch->handle = nullptr;
}

enum data_watch_wake data_watch_wait(struct data_watch *_Nullable watch)
{
    if (watch == nullptr || watch->handle == nullptr)
        return WaitMessage() != 0 ? DATA_WATCH_MESSAGE : DATA_WATCH_STOPPED;
    HANDLE handle = watch->handle;
    DWORD woke =
        MsgWaitForMultipleObjectsEx(1, &handle, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    if (woke == WAIT_OBJECT_0)
    {
        /* The handle stays signalled until the next notice is asked for. */
        if (FindNextChangeNotification(handle) == 0)
            stop_notices(watch);
        return DATA_WATCH_CHANGED;
    }
    if (woke != WAIT_OBJECT_0 + 1)
        stop_notices(watch); /* the wait failed with this handle: go on without it */
    return DATA_WATCH_MESSAGE;
}

void data_watch_destroy(struct data_watch *_Nullable watch)
{
    if (watch != nullptr)
        stop_notices(watch);
    free(watch);
}
