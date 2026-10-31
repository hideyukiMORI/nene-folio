#include "storage_adapter.h"
#include "data_lock.h"
#include "file_bytes.h"
#include "json_writer.h"
#include "startup_settings.h"
#include "storage_native_path.h"
#include "storage_path.h"
#include <windows.h>

#include <appmodel.h>
#include <shlobj.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct storage_adapter
{
    enum storage_distribution distribution;
    struct storage_path *_Nonnull default_directory;
    wchar_t config_directory[storage_native_capacity];
    wchar_t config_file[storage_native_capacity];
};

static bool append_leaf(wchar_t *_Nonnull path, const wchar_t *_Nonnull leaf)
{
    size_t length = wcslen(path);
    size_t added = wcslen(leaf);
    size_t gap = length > 0 && path[length - 1] != L'\\' ? 1 : 0;
    if (length + gap + added >= storage_native_capacity)
        return false;
    if (gap != 0)
        path[length] = L'\\';
    memcpy(path + length + gap, leaf, (added + 1) * sizeof *path);
    return true;
}

static bool module_directory(wchar_t *_Nonnull out)
{
    DWORD length = GetModuleFileNameW(nullptr, out, (DWORD)storage_native_capacity);
    if (length == 0 || length >= storage_native_capacity)
        return false;
    size_t end = length;
    while (end > 0 && out[end - 1] != L'\\')
        end -= 1;
    if (end == 0)
        return false;
    out[end] = L'\0';
    return true;
}

static bool known_directory(const GUID *_Nonnull id, wchar_t *_Nonnull out)
{
    PWSTR units = nullptr;
    HRESULT result = SHGetKnownFolderPath(id, KF_FLAG_NO_PACKAGE_REDIRECTION, nullptr, &units);
    bool ready = SUCCEEDED(result) && units != nullptr && wcslen(units) < storage_native_capacity;
    if (ready)
        memcpy(out, units, (wcslen(units) + 1) * sizeof *out);
    CoTaskMemFree(units);
    return ready;
}

static enum storage_outcome distribution(enum storage_distribution *_Nonnull out)
{
    UINT32 length = 0;
    LONG result = GetCurrentPackageFullName(&length, nullptr);
    if (result == APPMODEL_ERROR_NO_PACKAGE)
    {
        *out = STORAGE_PORTABLE;
        return STORAGE_READY;
    }
    if (result == ERROR_INSUFFICIENT_BUFFER)
    {
        *out = STORAGE_INSTALLED;
        return STORAGE_READY;
    }
    return STORAGE_LOCATION_UNAVAILABLE;
}

static bool paths_for(enum storage_distribution kind, wchar_t *_Nonnull config,
                      wchar_t *_Nonnull root)
{
    switch (kind)
    {
    case STORAGE_PORTABLE:
        if (!module_directory(config))
            return false;
        memcpy(root, config, (wcslen(config) + 1) * sizeof *root);
        return append_leaf(root, L"data");
    case STORAGE_INSTALLED:
        return known_directory(&FOLDERID_LocalAppData, config) &&
               append_leaf(config, L"NeNeFolio") && known_directory(&FOLDERID_Documents, root) &&
               append_leaf(root, L"NeNeFolio");
    }
    return false;
}

static enum storage_outcome native_directory(const wchar_t *_Nonnull units, wchar_t *_Nonnull out)
{
    struct storage_path *_Nullable path = nullptr;
    enum storage_outcome result = storage_native_path_from_units(units, &path);
    if (result != STORAGE_READY)
        return result;
    size_t length = 0;
    bool filled = storage_native_path_fill(path, out, &length);
    storage_path_destroy(path);
    return filled ? STORAGE_READY : STORAGE_INVALID_PATH;
}

enum storage_outcome storage_adapter_create(struct storage_adapter *_Nullable *_Nonnull out)
{
    enum storage_distribution kind = STORAGE_PORTABLE;
    enum storage_outcome located = distribution(&kind);
    if (located != STORAGE_READY)
        return located;
    wchar_t config[storage_native_capacity];
    wchar_t root[storage_native_capacity];
    if (!paths_for(kind, config, root))
        return STORAGE_LOCATION_UNAVAILABLE;
    struct storage_path *_Nullable path = nullptr;
    located = storage_native_path_from_units(root, &path);
    if (located != STORAGE_READY)
        return located;
    struct storage_adapter *_Nullable adapter = malloc(sizeof *adapter);
    if (adapter == nullptr)
    {
        storage_path_destroy(path);
        return STORAGE_OUT_OF_MEMORY;
    }
    *adapter = (struct storage_adapter){.distribution = kind, .default_directory = path};
    located = native_directory(config, adapter->config_directory);
    if (located != STORAGE_READY)
    {
        storage_adapter_destroy(adapter);
        return located;
    }
    memcpy(adapter->config_file, adapter->config_directory,
           (wcslen(adapter->config_directory) + 1) * sizeof *config);
    if (!append_leaf(adapter->config_file, L"nenefolio-startup.json"))
    {
        storage_adapter_destroy(adapter);
        return STORAGE_LOCATION_UNAVAILABLE;
    }
    *out = adapter;
    return STORAGE_READY;
}

static enum storage_outcome parsed_settings(enum startup_settings_outcome parsed)
{
    switch (parsed)
    {
    case STARTUP_SETTINGS_READY:
        return STORAGE_READY;
    case STARTUP_SETTINGS_MALFORMED:
        return STORAGE_SETTINGS_MALFORMED;
    case STARTUP_SETTINGS_UNSUPPORTED_VERSION:
        return STORAGE_SETTINGS_UNSUPPORTED;
    case STARTUP_SETTINGS_OUT_OF_MEMORY:
        return STORAGE_OUT_OF_MEMORY;
    }
    return STORAGE_SETTINGS_MALFORMED;
}

static enum storage_outcome read_settings(struct storage_adapter *_Nonnull adapter,
                                          struct startup_settings *_Nullable *_Nonnull out)
{
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome read = file_bytes_read(adapter->config_file, &bytes);
    if (read == PERSISTENCE_ABSENT)
        return STORAGE_ABSENT;
    if (read != PERSISTENCE_LOADED)
        return read == PERSISTENCE_OUT_OF_MEMORY ? STORAGE_OUT_OF_MEMORY
                                                 : STORAGE_SETTINGS_UNREADABLE;
    enum storage_outcome result = parsed_settings(
        startup_settings_parse(file_bytes_data(bytes), file_bytes_length(bytes), out));
    file_bytes_destroy(bytes);
    return result;
}

static enum storage_outcome prepare_directory(const wchar_t *_Nonnull path, enum storage_check kind)
{
    DWORD attributes = GetFileAttributesW(path);
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? STORAGE_READY : STORAGE_INVALID_PATH;
    DWORD error = GetLastError();
    if (error != ERROR_FILE_NOT_FOUND && error != ERROR_PATH_NOT_FOUND)
        return STORAGE_DIRECTORY_UNREADABLE;
    if (kind == STORAGE_CHECK_EXISTING)
        return STORAGE_DIRECTORY_MISSING;
    if (CreateDirectoryW(path, nullptr))
        return STORAGE_READY;
    return STORAGE_DIRECTORY_UNWRITABLE;
}

static HANDLE open_probe(const wchar_t *_Nonnull root)
{
    static const wchar_t digits[] = L"0123456789abcdef";
    for (size_t attempt = 0; attempt < 256; ++attempt)
    {
        wchar_t path[storage_native_capacity];
        wchar_t leaf[] = L".nenefolio-storage-00.tmp";
        leaf[19] = digits[attempt / 16];
        leaf[20] = digits[attempt % 16];
        memcpy(path, root, (wcslen(root) + 1) * sizeof *path);
        if (!append_leaf(path, leaf))
            return INVALID_HANDLE_VALUE;
        HANDLE probe =
            CreateFileW(path, GENERIC_READ | GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW,
                        FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
        if (probe != INVALID_HANDLE_VALUE)
            return probe;
        DWORD error = GetLastError();
        if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
            return INVALID_HANDLE_VALUE;
    }
    return INVALID_HANDLE_VALUE;
}

static enum storage_outcome check_probe(const wchar_t *_Nonnull root)
{
    HANDLE probe = open_probe(root);
    if (probe == INVALID_HANDLE_VALUE)
        return STORAGE_DIRECTORY_UNWRITABLE;
    const unsigned char value = 0x6e;
    DWORD count = 0;
    bool wrote =
        WriteFile(probe, &value, 1, &count, nullptr) && count == 1 && FlushFileBuffers(probe);
    LARGE_INTEGER start = {0};
    unsigned char read = 0;
    bool readable = wrote && SetFilePointerEx(probe, start, nullptr, FILE_BEGIN) &&
                    ReadFile(probe, &read, 1, &count, nullptr) && count == 1 && read == value;
    CloseHandle(probe);
    if (!wrote)
        return STORAGE_DIRECTORY_UNWRITABLE;
    return readable ? STORAGE_READY : STORAGE_DIRECTORY_UNREADABLE;
}

static enum storage_outcome check_locked(const wchar_t *_Nonnull root)
{
    struct data_lock *_Nullable lock = nullptr;
    switch (data_lock_open(root, &lock))
    {
    case DATA_LOCK_IN_USE:
        return STORAGE_DIRECTORY_IN_USE;
    case DATA_LOCK_UNAVAILABLE:
        return STORAGE_DIRECTORY_UNWRITABLE;
    case DATA_LOCK_OUT_OF_MEMORY:
        return STORAGE_OUT_OF_MEMORY;
    case DATA_LOCK_ACQUIRED:
        break;
    }
    enum storage_outcome result = check_probe(root);
    data_lock_destroy(lock);
    return result;
}

static enum storage_outcome check_directory(struct storage_adapter *_Nonnull adapter,
                                            const struct storage_path *_Nonnull path,
                                            enum storage_check kind)
{
    (void)adapter;
    wchar_t root[storage_native_capacity];
    size_t length = 0;
    if (!storage_native_path_fill(path, root, &length))
        return STORAGE_INVALID_PATH;
    enum storage_outcome prepared = prepare_directory(root, kind);
    if (prepared != STORAGE_READY)
        return prepared;
    HANDLE directory = CreateFileW(root, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                   OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (directory == INVALID_HANDLE_VALUE)
        return STORAGE_DIRECTORY_UNREADABLE;
    enum storage_outcome result = check_locked(root);
    CloseHandle(directory);
    return result;
}

static enum storage_outcome unchanged_settings(struct storage_adapter *_Nonnull adapter,
                                               const struct startup_settings *_Nullable expected)
{
    struct startup_settings *_Nullable current = nullptr;
    enum storage_outcome read = read_settings(adapter, &current);
    if (read != STORAGE_READY && read != STORAGE_ABSENT)
        return read;
    bool matches = expected == nullptr
                       ? read == STORAGE_ABSENT
                       : current != nullptr && startup_settings_equal(expected, current);
    startup_settings_destroy(current);
    return matches ? STORAGE_READY : STORAGE_SETTINGS_CHANGED;
}

static enum storage_outcome write_settings(struct storage_adapter *_Nonnull adapter,
                                           const struct startup_settings *_Nonnull settings)
{
    struct json_writer *_Nullable writer = nullptr;
    if (json_writer_create(&writer) != JSON_WRITER_ACCEPTED)
        return STORAGE_OUT_OF_MEMORY;
    startup_settings_write(settings, writer);
    enum storage_outcome result = STORAGE_OUT_OF_MEMORY;
    if (json_writer_finish(writer) == JSON_WRITER_ACCEPTED)
        result = file_bytes_store(adapter->config_file, json_writer_text(writer),
                                  json_writer_length(writer)) == PERSISTENCE_STORED
                     ? STORAGE_READY
                     : STORAGE_SETTINGS_STORE_FAILED;
    json_writer_destroy(writer);
    return result;
}

static enum storage_outcome store_settings(struct storage_adapter *_Nonnull adapter,
                                           const struct startup_settings *_Nullable expected,
                                           const struct startup_settings *_Nonnull replacement)
{
    if (prepare_directory(adapter->config_directory, STORAGE_CHECK_CHOICE) != STORAGE_READY)
        return STORAGE_SETTINGS_STORE_FAILED;
    wchar_t path[storage_native_capacity];
    memcpy(path, adapter->config_file, (wcslen(adapter->config_file) + 1) * sizeof *path);
    /* .lockはconfig fileの隣で、ノートrootの錠とは異なる資源。 */
    static const wchar_t suffix[] = L".lock";
    size_t length = wcslen(path);
    if (length + sizeof suffix / sizeof suffix[0] > storage_native_capacity)
        return STORAGE_SETTINGS_STORE_FAILED;
    memcpy(path + length, suffix, sizeof suffix);
    HANDLE lock = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock == INVALID_HANDLE_VALUE)
        return STORAGE_SETTINGS_STORE_FAILED;
    enum storage_outcome result = unchanged_settings(adapter, expected);
    if (result == STORAGE_READY)
        result = write_settings(adapter, replacement);
    CloseHandle(lock);
    return result;
}

struct storage_port storage_adapter_port(struct storage_adapter *_Nonnull adapter)
{
    return (struct storage_port){.adapter = adapter,
                                 .distribution = adapter->distribution,
                                 .default_directory = adapter->default_directory,
                                 .read = read_settings,
                                 .check = check_directory,
                                 .store = store_settings};
}

void storage_adapter_destroy(struct storage_adapter *_Nullable adapter)
{
    if (adapter != nullptr)
        storage_path_destroy(adapter->default_directory);
    free(adapter);
}
