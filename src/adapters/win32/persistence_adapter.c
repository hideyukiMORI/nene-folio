#include "persistence_adapter.h"

#include "category_ledger.h"
#include "file_bytes.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_history.h"
#include "note_ledger.h"
#include "note_text.h"
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
/* 履歴の置き場所（ADR 0012 の決定 1）。`.` で始まるのでカテゴリの走査には出ない（決定 4）。 */
static const wchar_t history_folder[] = L".history";
constexpr size_t history_folder_length = 8;
/* 版の葉は `\<番号>.md` の 5 単位（終端を除く）。番号が 1 桁で足りることを言語で確かめる。 */
constexpr size_t history_leaf_length = 5;
static_assert(note_history_depth >= 1 && note_history_depth <= 9,
              "ADR 0012: the history depth must fit one digit");

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

/* 終端付きの UTF-16 の長さ（組み立てた道の上限まで）。 */
static size_t wide_length(const wchar_t *_Nonnull units)
{
    size_t length = 0;
    while (length < path_capacity && units[length] != L'\0')
    {
        length += 1;
    }
    return length;
}

/* UTF-8 の名前を `\<名前>` として out の position から書き足す（C-014 の変換の境界）。 */
static bool append_utf8_name(wchar_t *_Nonnull out, size_t *_Nonnull position,
                             const char *_Nonnull name)
{
    struct utf16_text *_Nullable units = nullptr;
    if (utf16_text_create(name, strlen(name), &units) != UTF16_TEXT_CONVERTED)
    {
        return false;
    }
    bool fits = append_units(out, position, L"\\", 1) &&
                append_units(out, position, utf16_text_units(units), utf16_text_length(units));
    utf16_text_destroy(units);
    return fits;
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
        fits = append_utf8_name(out, &position, category);
    }
    if (fits && leaf != nullptr)
    {
        size_t leaf_length = wide_length(leaf);
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
    /* `.` で始まるディレクトリはカテゴリにしない（ADR 0012 の決定 4）。`.` / `..` のほか
     * `.history` や `.git` もここで落ちる。ファイル（ノート）の規則は変えない。 */
    if (is_directory != directories || (is_directory && entry->cFileName[0] == L'.'))
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

/* <note>.md を UTF-16 の葉として組み立てる。名前は name_list の規則で 255 バイト以下。 */
static bool note_leaf(const char *_Nonnull note, wchar_t *_Nonnull out, size_t capacity)
{
    struct utf16_text *_Nullable name = nullptr;
    if (utf16_text_create(note, strlen(note), &name) != UTF16_TEXT_CONVERTED)
    {
        return false;
    }
    size_t length = utf16_text_length(name);
    bool fits = length + note_extension_length + 1 <= capacity;
    if (fits)
    {
        memcpy(out, utf16_text_units(name), length * sizeof *out);
        memcpy(out + length, note_extension, (note_extension_length + 1) * sizeof *out);
    }
    utf16_text_destroy(name);
    return fits;
}

static enum persistence_outcome read_note(struct persistence_adapter *_Nonnull adapter,
                                          const char *_Nonnull category, const char *_Nonnull note,
                                          struct note_text *_Nullable *_Nonnull out)
{
    wchar_t leaf[MAX_PATH];
    wchar_t path[path_capacity];
    if (!note_leaf(note, leaf, MAX_PATH) || !compose(adapter, category, leaf, path))
    {
        return PERSISTENCE_UNREADABLE;
    }
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome outcome = file_bytes_read(path, &bytes);
    if (outcome != PERSISTENCE_LOADED)
    {
        return outcome;
    }
    enum note_text_outcome accepted =
        note_text_create(file_bytes_data(bytes), file_bytes_length(bytes), out);
    file_bytes_destroy(bytes);
    switch (accepted)
    {
    case NOTE_TEXT_ACCEPTED:
        return PERSISTENCE_LOADED;
    case NOTE_TEXT_INVALID_UTF8:
        return PERSISTENCE_MALFORMED;
    case NOTE_TEXT_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_MALFORMED;
}

/* 無ければ作る。既にあれば作れたものとして扱う。 */
static bool ensure_directory(const wchar_t *_Nonnull path)
{
    return CreateDirectoryW(path, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

/* data\.history\<category>\<note> を段階的に作りながら out へ組み立てる。 */
static bool ensure_history_directory(const struct persistence_adapter *_Nonnull adapter,
                                     const char *_Nonnull category, const char *_Nonnull note,
                                     wchar_t *_Nonnull out)
{
    size_t length = 0;
    if (!append_units(out, &length, adapter->root, adapter->root_length) ||
        !append_units(out, &length, L"\\", 1) ||
        !append_units(out, &length, history_folder, history_folder_length) ||
        !ensure_directory(out))
    {
        return false;
    }
    if (!append_utf8_name(out, &length, category) || !ensure_directory(out))
    {
        return false;
    }
    return append_utf8_name(out, &length, note) && ensure_directory(out);
}

/* <履歴のディレクトリ>\<version>.md を out へ組み立てる。version は 1〜note_history_depth。 */
static bool compose_version(const wchar_t *_Nonnull directory, size_t length, size_t version,
                            wchar_t *_Nonnull out)
{
    const wchar_t leaf[] = {L'\\', (wchar_t)(L'0' + version), L'.', L'm', L'd', L'\0'};
    size_t position = 0;
    return append_units(out, &position, directory, length) &&
           append_units(out, &position, leaf, history_leaf_length);
}

/* 最古の版を消し、残りを 1 つずつ後ろへずらす（ADR 0012 の決定 3）。無い版は飛ばす。
 * 原子的ではないので、途中で落ちれば番号が欠けた履歴が残りうる。md はまだ無傷。 */
static void rotate_history(const wchar_t *_Nonnull directory, size_t length)
{
    wchar_t older[path_capacity];
    wchar_t newer[path_capacity];
    if (compose_version(directory, length, note_history_depth, older))
    {
        (void)DeleteFileW(older);
    }
    for (size_t version = note_history_depth; version > 1; --version)
    {
        if (compose_version(directory, length, version, older) &&
            compose_version(directory, length, version - 1, newer))
        {
            (void)MoveFileExW(newer, older, MOVEFILE_WRITE_THROUGH);
        }
    }
}

/* 履歴のディレクトリを用意し、番号をずらして、渡された中身を 1.md へ原子的に書く。 */
static enum persistence_outcome store_history(const struct persistence_adapter *_Nonnull adapter,
                                              const char *_Nonnull category,
                                              const char *_Nonnull note,
                                              const struct file_bytes *_Nonnull bytes)
{
    wchar_t directory[path_capacity];
    wchar_t newest[path_capacity];
    if (!ensure_history_directory(adapter, category, note, directory))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    size_t length = wide_length(directory);
    if (!compose_version(directory, length, 1, newest))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    rotate_history(directory, length);
    return file_bytes_store(newest, file_bytes_data(bytes), file_bytes_length(bytes));
}

/* いま md にある本文を履歴へ写す（FR-017 / ADR 0012）。バイト列はそのまま写すので、
 * 改行の形も BOM の無さも元のままになる。md が無ければ写すものが無い（ABSENT）。 */
static enum persistence_outcome archive_note(struct persistence_adapter *_Nonnull adapter,
                                             const char *_Nonnull category,
                                             const char *_Nonnull note)
{
    wchar_t leaf[MAX_PATH];
    wchar_t source[path_capacity];
    if (!note_leaf(note, leaf, MAX_PATH) || !compose(adapter, category, leaf, source))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome read = file_bytes_read(source, &bytes);
    if (read == PERSISTENCE_ABSENT || read == PERSISTENCE_OUT_OF_MEMORY)
    {
        return read;
    }
    if (read != PERSISTENCE_LOADED)
    {
        return PERSISTENCE_UNWRITABLE;
    }
    enum persistence_outcome outcome = store_history(adapter, category, note, bytes);
    file_bytes_destroy(bytes);
    return outcome;
}

/* 本文を同じ md へ原子的に書き戻す（FR-006）。改行の形は core が既に揃えている。 */
static enum persistence_outcome write_note(struct persistence_adapter *_Nonnull adapter,
                                           const char *_Nonnull category, const char *_Nonnull note,
                                           const struct note_text *_Nonnull body)
{
    wchar_t leaf[MAX_PATH];
    wchar_t path[path_capacity];
    if (!note_leaf(note, leaf, MAX_PATH) || !compose(adapter, category, leaf, path))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    return file_bytes_store(path, note_text_bytes(body), note_text_length(body));
}

/* md を別のカテゴリのディレクトリへ移す（ADR 0008 の決定 4）。同じボリューム内なので rename で、
 * MOVEFILE_REPLACE_EXISTING は付けない。移動先に同名（大文字小文字だけ違うものを含む）があれば
 * OS が拒み、利用者のノートは上書きされない。 */
static enum persistence_outcome move_note(struct persistence_adapter *_Nonnull adapter,
                                          const char *_Nonnull from_category,
                                          const char *_Nonnull note,
                                          const char *_Nonnull to_category)
{
    wchar_t leaf[MAX_PATH];
    wchar_t from[path_capacity];
    wchar_t to[path_capacity];
    if (!note_leaf(note, leaf, MAX_PATH) || !compose(adapter, from_category, leaf, from) ||
        !compose(adapter, to_category, leaf, to))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    return MoveFileExW(from, to, MOVEFILE_WRITE_THROUGH) ? PERSISTENCE_STORED
                                                         : PERSISTENCE_UNWRITABLE;
}

/* 組み立て終えた文書を path へ原子的に置き換え、writer を片付ける。 */
static enum persistence_outcome store_document(const wchar_t *_Nonnull path,
                                               struct json_writer *_Nonnull writer)
{
    enum json_writer_outcome finished = json_writer_finish(writer);
    enum persistence_outcome outcome = PERSISTENCE_OUT_OF_MEMORY;
    if (finished == JSON_WRITER_ACCEPTED)
    {
        outcome = file_bytes_store(path, json_writer_text(writer), json_writer_length(writer));
    }
    json_writer_destroy(writer);
    return outcome;
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
    return store_document(path, writer);
}

/* 索引を同じカテゴリの index.json へ書き戻す（FR-008 / ADR 0007 の決定 5）。 */
static enum persistence_outcome write_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                  const char *_Nonnull category,
                                                  const struct note_ledger *_Nonnull ledger)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, category, L"index.json", path))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    struct json_writer *_Nullable writer = nullptr;
    if (json_writer_create(&writer) != JSON_WRITER_ACCEPTED)
    {
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    note_ledger_write(ledger, writer);
    return store_document(path, writer);
}

struct persistence_port persistence_adapter_port(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = {
        .adapter = adapter,
        .scan_categories = scan_categories,
        .scan_notes = scan_notes,
        .read_category_ledger = read_category_ledger,
        .write_category_ledger = write_category_ledger,
        .read_note = read_note,
        .archive_note = archive_note,
        .write_note = write_note,
        .move_note = move_note,
        .read_note_ledger = read_note_ledger,
        .write_note_ledger = write_note_ledger,
    };
    return port;
}

void persistence_adapter_destroy(struct persistence_adapter *_Nullable adapter)
{
    free(adapter);
}
