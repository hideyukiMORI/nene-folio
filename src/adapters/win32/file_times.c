#include "file_times.h"

#include "note_file_times.h"

#include <stdio.h>
#include <windows.h>

static bool local_timestamp(FILETIME value, char *_Nonnull text)
{
    SYSTEMTIME utc = {0};
    SYSTEMTIME local = {0};
    if ((value.dwLowDateTime == 0 && value.dwHighDateTime == 0) ||
        !FileTimeToSystemTime(&value, &utc) ||
        !SystemTimeToTzSpecificLocalTimeEx(nullptr, &utc, &local))
        return false;
    return snprintf(text, 17, "%04u-%02u-%02u %02u:%02u", (unsigned int)local.wYear,
                    (unsigned int)local.wMonth, (unsigned int)local.wDay, (unsigned int)local.wHour,
                    (unsigned int)local.wMinute) == 16;
}

enum persistence_outcome file_times_read(const wchar_t *_Nonnull path,
                                         struct note_file_times *_Nullable *_Nonnull out)
{
    HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return PERSISTENCE_UNREADABLE;
    FILETIME created = {0};
    FILETIME updated = {0};
    bool read = GetFileTime(file, &created, nullptr, &updated) != 0;
    CloseHandle(file);
    char creation[17];
    char modification[17];
    if (!read || !local_timestamp(created, creation) || !local_timestamp(updated, modification))
        return PERSISTENCE_UNREADABLE;
    switch (note_file_times_create(creation, modification, out))
    {
    case NOTE_FILE_TIMES_ACCEPTED:
        return PERSISTENCE_LOADED;
    case NOTE_FILE_TIMES_INVALID:
        return PERSISTENCE_UNREADABLE;
    case NOTE_FILE_TIMES_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_UNREADABLE;
}
