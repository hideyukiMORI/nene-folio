#include "persistence_adapter.h"

#include "category_ledger.h"
#include "file_bytes.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_ledger.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* パスの上限（UTF-16 単位・終端込み）。超える場所に置かれた実行ファイルは読めないとして扱う。 */
constexpr size_t path_capacity = 1024;

struct persistence_adapter
{
    wchar_t root[path_capacity]; /* <実行ファイルの場所>\data。終端付き */
    size_t root_length;
};

static const wchar_t data_folder[] = L"data";
static const wchar_t note_extension[] = L".md";
constexpr size_t note_extension_length = 3;

enum persistence_adapter_outcome
persistence_adapter_create(struct persistence_adapter *_Nullable *_Nonnull out)
{
    struct persistence_adapter *_Nullable adapter = calloc(1, sizeof *adapter);
    if (adapter == nullptr)
    {
        return PERSISTENCE_ADAPTER_OUT_OF_MEMORY;
    }
    DWORD length = GetModuleFileNameW(nullptr, adapter->root, (DWORD)path_capacity);
    if (length == 0 || length >= path_capacity)
    {
        free(adapter);
        return PERSISTENCE_ADAPTER_NO_MODULE_PATH;
    }
    size_t directory = length;
    while (directory > 0 && adapter->root[directory - 1] != L'\\')
    {
        directory -= 1;
    }
    size_t folder_length = sizeof data_folder / sizeof data_folder[0] - 1;
    if (directory == 0 || directory + folder_length + 1 > path_capacity)
    {
        free(adapter);
        return PERSISTENCE_ADAPTER_NO_MODULE_PATH;
    }
    memcpy(adapter->root + directory, data_folder, sizeof data_folder);
    adapter->root_length = directory + folder_length;
    *out = adapter;
    return PERSISTENCE_ADAPTER_CREATED;
}

/* 終端付きの UTF-16 を out の position から写す。収まらなければ false。 */
static bool append_units(wchar_t *_Nonnull out, size_t *_Nonnull position,
                         const wchar_t *_Nonnull units, size_t count)
{
    if (*position + count + 1 > path_capacity)
    {
        return false;
    }
    memcpy(out + *position, units, count * sizeof *units);
    *position += count;
    out[*position] = L'\0';
    return true;
}

/* root[\category][\leaf] を out（path_capacity 単位）へ組み立てる。 */
static bool compose(const struct persistence_adapter *_Nonnull adapter,
                    const char *_Nullable category, const wchar_t *_Nullable leaf,
                    wchar_t *_Nonnull out)
{
    size_t position = 0;
    bool fits = append_units(out, &position, adapter->root, adapter->root_length);
    if (fits && category != nullptr)
    {
        struct utf16_text *_Nullable name = nullptr;
        if (utf16_text_create(category, strlen(category), &name) != UTF16_TEXT_CONVERTED)
        {
            return false;
        }
        fits = append_units(out, &position, L"\\", 1) &&
               append_units(out, &position, utf16_text_units(name), utf16_text_length(name));
        utf16_text_destroy(name);
    }
    if (fits && leaf != nullptr)
    {
        size_t leaf_length = 0;
        while (leaf[leaf_length] != L'\0')
        {
            leaf_length += 1;
        }
        fits = append_units(out, &position, L"\\", 1) &&
               append_units(out, &position, leaf, leaf_length);
    }
    return fits;
}

static enum persistence_outcome translate_names(enum name_list_outcome outcome)
{
    switch (outcome)
    {
    case NAME_LIST_ACCEPTED:
        return PERSISTENCE_LOADED;
    case NAME_LIST_DUPLICATE:
    case NAME_LIST_INVALID_NAME:
        return PERSISTENCE_MALFORMED;
    case NAME_LIST_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_MALFORMED;
}

static size_t name_length(const wchar_t *_Nonnull name)
{
    size_t length = 0;
    while (length < MAX_PATH && name[length] != L'\0')
    {
        length += 1;
    }
    return length;
}

static bool ends_with_note_extension(const wchar_t *_Nonnull name, size_t length)
{
    if (length <= note_extension_length)
    {
        return false;
    }
    for (size_t index = 0; index < note_extension_length; ++index)
    {
        wchar_t unit = name[length - note_extension_length + index];
        wchar_t lowered = (unit >= L'A' && unit <= L'Z') ? (wchar_t)(unit + 32) : unit;
        if (lowered != note_extension[index])
        {
            return false;
        }
    }
    return true;
}

/* 走査で見つけた 1 件を名前として受け入れるか判定し、受け入れるなら list へ足す。 */
static enum persistence_outcome accept_entry(const WIN32_FIND_DATAW *_Nonnull entry,
                                             bool directories, struct name_list *_Nonnull list)
{
    bool is_directory = (entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    size_t length = name_length(entry->cFileName);
    if (is_directory != directories || (length == 1 && entry->cFileName[0] == L'.') ||
        (length == 2 && entry->cFileName[0] == L'.' && entry->cFileName[1] == L'.'))
    {
        return PERSISTENCE_LOADED;
    }
    if (!directories)
    {
        if (!ends_with_note_extension(entry->cFileName, length))
        {
            return PERSISTENCE_LOADED;
        }
        length -= note_extension_length;
    }
    struct utf8_text *_Nullable name = nullptr;
    if (utf8_text_create(entry->cFileName, length, &name) != UTF8_TEXT_CONVERTED)
    {
        return PERSISTENCE_MALFORMED;
    }
    enum persistence_outcome outcome =
        translate_names(name_list_append(list, utf8_text_bytes(name), utf8_text_length(name)));
    utf8_text_destroy(name);
    return outcome;
}

/* FindFirstFileExW が失敗したときの意味。
 * data/ やカテゴリが無いことと、一致が無いことを区別する。 */
static enum persistence_outcome first_entry_outcome(void)
{
    DWORD error = GetLastError();
    if (error == ERROR_PATH_NOT_FOUND)
    {
        return PERSISTENCE_ABSENT;
    }
    return error == ERROR_FILE_NOT_FOUND ? PERSISTENCE_LOADED : PERSISTENCE_UNREADABLE;
}

/* 最初の 1 件から順に受け入れ、列挙が尽きるまで続ける。 */
static enum persistence_outcome collect(HANDLE search, WIN32_FIND_DATAW *_Nonnull entry,
                                        bool directories, struct name_list *_Nonnull list)
{
    for (;;)
    {
        enum persistence_outcome outcome = accept_entry(entry, directories, list);
        if (outcome != PERSISTENCE_LOADED)
        {
            return outcome;
        }
        if (!FindNextFileW(search, entry))
        {
            return GetLastError() == ERROR_NO_MORE_FILES ? PERSISTENCE_LOADED
                                                         : PERSISTENCE_UNREADABLE;
        }
    }
}

/* pattern に一致する項目を列挙する。directories が真ならディレクトリだけ、偽なら .md だけ。 */
static enum persistence_outcome scan(const wchar_t *_Nonnull pattern, bool directories,
                                     struct name_list *_Nullable *_Nonnull out)
{
    struct name_list *_Nullable list = nullptr;
    if (name_list_create(&list) != NAME_LIST_ACCEPTED)
    {
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    WIN32_FIND_DATAW entry;
    HANDLE search =
        FindFirstFileExW(pattern, FindExInfoBasic, &entry, FindExSearchNameMatch, nullptr, 0);
    enum persistence_outcome outcome = search == INVALID_HANDLE_VALUE
                                           ? first_entry_outcome()
                                           : collect(search, &entry, directories, list);
    if (search != INVALID_HANDLE_VALUE)
    {
        FindClose(search);
    }
    if (outcome != PERSISTENCE_LOADED)
    {
        name_list_destroy(list);
        return outcome;
    }
    *out = list;
    return PERSISTENCE_LOADED;
}

static enum persistence_outcome scan_categories(struct persistence_adapter *_Nonnull adapter,
                                                struct name_list *_Nullable *_Nonnull out)
{
    wchar_t pattern[path_capacity];
    if (!compose(adapter, nullptr, L"*", pattern))
    {
        return PERSISTENCE_UNREADABLE;
    }
    return scan(pattern, true, out);
}

static enum persistence_outcome scan_notes(struct persistence_adapter *_Nonnull adapter,
                                           const char *_Nonnull category,
                                           struct name_list *_Nullable *_Nonnull out)
{
    wchar_t pattern[path_capacity];
    if (!compose(adapter, category, L"*.md", pattern))
    {
        return PERSISTENCE_UNREADABLE;
    }
    return scan(pattern, false, out);
}

static enum persistence_outcome
read_category_ledger(struct persistence_adapter *_Nonnull adapter,
                     struct category_ledger *_Nullable *_Nonnull out)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, nullptr, L"categories.json", path))
    {
        return PERSISTENCE_UNREADABLE;
    }
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome outcome = file_bytes_read(path, &bytes);
    if (outcome != PERSISTENCE_LOADED)
    {
        return outcome;
    }
    enum category_ledger_outcome parsed =
        category_ledger_parse(file_bytes_data(bytes), file_bytes_length(bytes), out);
    file_bytes_destroy(bytes);
    switch (parsed)
    {
    case CATEGORY_LEDGER_ACCEPTED:
        return PERSISTENCE_LOADED;
    case CATEGORY_LEDGER_MALFORMED:
    case CATEGORY_LEDGER_UNSUPPORTED_VERSION:
        return PERSISTENCE_MALFORMED;
    case CATEGORY_LEDGER_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_MALFORMED;
}

static enum persistence_outcome read_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                 const char *_Nonnull category,
                                                 struct note_ledger *_Nullable *_Nonnull out)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, category, L"index.json", path))
    {
        return PERSISTENCE_UNREADABLE;
    }
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome outcome = file_bytes_read(path, &bytes);
    if (outcome != PERSISTENCE_LOADED)
    {
        return outcome;
    }
    enum note_ledger_outcome parsed =
        note_ledger_parse(file_bytes_data(bytes), file_bytes_length(bytes), out);
    file_bytes_destroy(bytes);
    switch (parsed)
    {
    case NOTE_LEDGER_ACCEPTED:
        return PERSISTENCE_LOADED;
    case NOTE_LEDGER_MALFORMED:
    case NOTE_LEDGER_UNSUPPORTED_VERSION:
        return PERSISTENCE_MALFORMED;
    case NOTE_LEDGER_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_MALFORMED;
}

static enum persistence_outcome write_category_ledger(struct persistence_adapter *_Nonnull adapter,
                                                      const struct category_ledger *_Nonnull ledger)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, nullptr, L"categories.json", path))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    struct json_writer *_Nullable writer = nullptr;
    if (json_writer_create(&writer) != JSON_WRITER_ACCEPTED)
    {
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    category_ledger_write(ledger, writer);
    enum json_writer_outcome finished = json_writer_finish(writer);
    enum persistence_outcome outcome = PERSISTENCE_OUT_OF_MEMORY;
    if (finished == JSON_WRITER_ACCEPTED)
    {
        outcome = file_bytes_store(path, json_writer_text(writer), json_writer_length(writer));
    }
    json_writer_destroy(writer);
    return outcome;
}

struct persistence_port persistence_adapter_port(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = {
        .adapter = adapter,
        .scan_categories = scan_categories,
        .scan_notes = scan_notes,
        .read_category_ledger = read_category_ledger,
        .write_category_ledger = write_category_ledger,
        .read_note_ledger = read_note_ledger,
    };
    return port;
}

void persistence_adapter_destroy(struct persistence_adapter *_Nullable adapter)
{
    free(adapter);
}
