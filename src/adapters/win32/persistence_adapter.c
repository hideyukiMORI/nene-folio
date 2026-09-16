#include "persistence_adapter.h"

#include "category_ledger.h"
#include "file_bytes.h"
#include "json_writer.h"
#include "name_list.h"
#include "note_history.h"
#include "note_ledger.h"
#include "note_rename.h"
#include "note_text.h"
#include "rename_guards.h"
#include "rename_journal.h"
#include "rename_paths.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* パスの上限（UTF-16 単位・終端込み）。超える場所に置かれた実行ファイルは読めないとして扱う。 */
constexpr size_t path_capacity = 1024;

struct persistence_adapter
{
    wchar_t root[path_capacity]; /* <実行ファイルの場所>\data。終端付き */
    size_t root_length;
    /* data/.nenefolio.lock を共有なしで開いた寿命中のハンドル（ADR 0022 の決定 3）。
     * 取れなかった（読み取り専用・アクセス拒否）ときは nullptr のままで、書込だけが失敗する。 */
    HANDLE lock;
};

static const wchar_t data_folder[] = L"data";
/* 同じ data/ を使うプロセスを 1 つに直列化する錠（ADR 0022 の決定 3）。異常終了で残ってよい。 */
static const wchar_t lock_leaf[] = L".nenefolio.lock";
/* 改名の復旧記録（ADR 0022 の決定 5）。`.` 始まりなのでカテゴリの走査には出ない。 */
static const wchar_t journal_leaf[] = L".rename.json";
static const wchar_t note_extension[] = L".md";
constexpr size_t note_extension_length = 3;
/* 履歴の置き場所（ADR 0012 の決定 1）。`.` で始まるのでカテゴリの走査には出ない（決定 4）。 */
static const wchar_t history_folder[] = L".history";
constexpr size_t history_folder_length = 8;
/* 版の葉は `\<番号>.md` の 5 単位（終端を除く）。番号が 1 桁で足りることを言語で確かめる。 */
constexpr size_t history_leaf_length = 5;
/* 書き切ってから 1.md へ改名する控えの葉 `\1.md.tmp`（9 単位・ADR 0012 の決定 3）。 */
constexpr size_t history_pending_length = 9;
static_assert(note_history_depth >= 1 && note_history_depth <= 9,
              "ADR 0012: the history depth must fit one digit");

/* すべての走査・保存・履歴が共有するrootだけを長い絶対パスへ揃える（ADR0020）。 */
static enum persistence_adapter_outcome acquire_lock(struct persistence_adapter *_Nonnull adapter);

static bool module_path(wchar_t *_Nonnull out, size_t *_Nonnull length)
{
    wchar_t module[path_capacity];
    DWORD count = GetModuleFileNameW(nullptr, module, (DWORD)path_capacity);
    if (count == 0 || count >= path_capacity)
    {
        return false;
    }
    const wchar_t *_Nonnull prefix = L"\\\\?\\";
    size_t prefix_length = 4;
    size_t offset = 0;
    if (count >= 4 && memcmp(module, L"\\\\?\\", 4 * sizeof *module) == 0)
    {
        prefix_length = 0;
    }
    else if (count >= 2 && module[0] == L'\\' && module[1] == L'\\')
    {
        prefix = L"\\\\?\\UNC\\";
        prefix_length = 8;
        offset = 2;
    }
    *length = prefix_length + count - offset;
    if (*length >= path_capacity)
    {
        return false;
    }
    memcpy(out, prefix, prefix_length * sizeof *out);
    memcpy(out + prefix_length, module + offset, (count - offset + 1) * sizeof *out);
    return true;
}

enum persistence_adapter_outcome
persistence_adapter_create(struct persistence_adapter *_Nullable *_Nonnull out)
{
    struct persistence_adapter *_Nullable adapter = calloc(1, sizeof *adapter);
    if (adapter == nullptr)
    {
        return PERSISTENCE_ADAPTER_OUT_OF_MEMORY;
    }
    size_t length = 0;
    if (!module_path(adapter->root, &length))
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
    enum persistence_adapter_outcome locked = acquire_lock(adapter);
    if (locked != PERSISTENCE_ADAPTER_CREATED)
    {
        free(adapter);
        return locked;
    }
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

/* <履歴のディレクトリ>\1.md.tmp を out へ組み立てる。新しい版はここへ書き切ってから 1.md にする。
 */
static bool compose_pending(const wchar_t *_Nonnull directory, size_t length, wchar_t *_Nonnull out)
{
    size_t position = 0;
    return append_units(out, &position, directory, length) &&
           append_units(out, &position, L"\\1.md.tmp", history_pending_length);
}

/* 最古の版を消し、残りを 1 つずつ後ろへずらす（ADR 0012 の決定 3）。無い版は飛ばす。
 * 改名に MOVEFILE_REPLACE_EXISTING を付けるので、最古の削除が効かなくても改名が相手を上書きし、
 * 連鎖が止まって 1.md だけが失われることにならない。
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
            (void)MoveFileExW(newer, older, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
        }
    }
}

/* 履歴のディレクトリを用意し、**先に新しい版を 1.md.tmp へ書き切ってから**番号をずらし、
 * 最後に 1.md.tmp を 1.md へ改名する（ADR 0012 の決定 3）。
 * 書き切れなければ履歴も md も 1 つも動かない。最後の改名が落ちると 2.md〜5.md だけが残りうる。 */
static enum persistence_outcome store_history(const struct persistence_adapter *_Nonnull adapter,
                                              const char *_Nonnull category,
                                              const char *_Nonnull note,
                                              const struct file_bytes *_Nonnull bytes)
{
    wchar_t directory[path_capacity];
    wchar_t pending[path_capacity];
    wchar_t newest[path_capacity];
    if (!ensure_history_directory(adapter, category, note, directory))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    size_t length = wide_length(directory);
    if (!compose_pending(directory, length, pending) ||
        !compose_version(directory, length, 1, newest))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    enum persistence_outcome written =
        file_bytes_store(pending, file_bytes_data(bytes), file_bytes_length(bytes));
    if (written != PERSISTENCE_STORED)
    {
        return written;
    }
    rotate_history(directory, length);
    return MoveFileExW(pending, newest, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)
               ? PERSISTENCE_STORED
               : PERSISTENCE_UNWRITABLE;
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

/* 初回保存は同じパス生成と原子的書込を使い、既存mdの置換を許さない（ADR 0020）。 */
static enum persistence_outcome create_note(struct persistence_adapter *_Nonnull adapter,
                                            const char *_Nonnull category,
                                            const char *_Nonnull note,
                                            const struct note_text *_Nonnull body)
{
    wchar_t leaf[MAX_PATH];
    wchar_t path[path_capacity];
    if (!note_leaf(note, leaf, MAX_PATH) || !compose(adapter, category, leaf, path))
    {
        return PERSISTENCE_UNWRITABLE;
    }
    return file_bytes_create(path, note_text_bytes(body), note_text_length(body));
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

/* ここから下は改名と復旧（ADR 0022 の決定 3〜6）。
 * 内部の段階の判定は enum rename_outcome を借り、RENAME_COMPLETED を「この段は満たされた」に使う。
 * 実際に完了を答えるのは rename_note / recover_rename の戻り値だけである。 */

static_assert(rename_path_capacity == path_capacity,
              "ADR 0022: the rename paths share the adapter's path limit");

static bool compose_note_path(const struct persistence_adapter *_Nonnull adapter,
                              const char *_Nonnull category, const char *_Nonnull note,
                              wchar_t *_Nonnull out)
{
    wchar_t leaf[MAX_PATH];
    return note_leaf(note, leaf, MAX_PATH) && compose(adapter, category, leaf, out);
}

static bool compose_history_root(const struct persistence_adapter *_Nonnull adapter,
                                 wchar_t *_Nonnull out, size_t *_Nonnull length)
{
    *length = 0;
    return append_units(out, length, adapter->root, adapter->root_length) &&
           append_units(out, length, L"\\", 1) &&
           append_units(out, length, history_folder, history_folder_length);
}

static bool compose_history_category(const struct persistence_adapter *_Nonnull adapter,
                                     const char *_Nonnull category, wchar_t *_Nonnull out)
{
    size_t length = 0;
    return compose_history_root(adapter, out, &length) && append_utf8_name(out, &length, category);
}

static bool compose_history_note(const struct persistence_adapter *_Nonnull adapter,
                                 const char *_Nonnull category, const char *_Nonnull note,
                                 wchar_t *_Nonnull out)
{
    size_t length = 0;
    return compose_history_root(adapter, out, &length) &&
           append_utf8_name(out, &length, category) && append_utf8_name(out, &length, note);
}

static bool compose_rename_paths(const struct persistence_adapter *_Nonnull adapter,
                                 const struct note_rename *_Nonnull plan,
                                 struct rename_paths *_Nonnull out)
{
    const char *_Nonnull category = note_rename_category(plan);
    const char *_Nonnull from = note_rename_from(plan);
    const char *_Nonnull to = note_rename_to(plan);
    return compose_note_path(adapter, category, from, out->note_from) &&
           compose_note_path(adapter, category, to, out->note_to) &&
           compose_history_note(adapter, category, from, out->history_from) &&
           compose_history_note(adapter, category, to, out->history_to);
}

/* 無ければ present を偽にして true。照会そのものができなければ false。 */
static bool entry_exists(const wchar_t *_Nonnull path, bool *_Nonnull present)
{
    DWORD attributes = GetFileAttributesW(path);
    *present = attributes != INVALID_FILE_ATTRIBUTES;
    if (*present)
    {
        return true;
    }
    DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}

/* 対象そのもの（リンクの先ではない）を開く。共有は読みだけで、書込と削除は許さない。 */
static HANDLE open_entry(const wchar_t *_Nonnull path, DWORD access, bool directory)
{
    DWORD flags = FILE_FLAG_OPEN_REPARSE_POINT | (directory ? FILE_FLAG_BACKUP_SEMANTICS : 0U);
    return CreateFileW(path, access, FILE_SHARE_READ, nullptr, OPEN_EXISTING, flags, nullptr);
}

static bool plain_entry(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION information;
    return GetFileInformationByHandle(handle, &information) &&
           (information.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
}

/* 初版はローカル NTFS だけを対象にする（決定 4）。照会できなければ対象にしない。 */
static bool ntfs_volume(HANDLE handle)
{
    wchar_t name[16];
    DWORD capacity = (DWORD)(sizeof name / sizeof name[0]);
    if (!GetVolumeInformationByHandleW(handle, nullptr, 0, nullptr, nullptr, nullptr, name,
                                       capacity))
    {
        return false;
    }
    return memcmp(name, L"NTFS", 5 * sizeof *name) == 0;
}

/* volume64 の 16 桁と FILE_ID_128 の 32 桁を小文字 hex でつないだ 48 桁（決定 5）。 */
static void format_identity(const FILE_ID_INFO *_Nonnull info, char *_Nonnull out)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t index = 0; index < 16; ++index)
    {
        out[index] = digits[(info->VolumeSerialNumber >> ((15 - index) * 4)) & 15];
    }
    for (size_t index = 0; index < 16; ++index)
    {
        unsigned char value = info->FileId.Identifier[index];
        out[16 + index * 2] = digits[value >> 4];
        out[17 + index * 2] = digits[value & 15];
    }
    out[rename_journal_id_length] = '\0';
}

/* 開いたハンドルで reparse point と NTFS を確かめ、識別子を読む。 */
static enum rename_outcome identity_of(HANDLE handle, char *_Nonnull out)
{
    if (!plain_entry(handle) || !ntfs_volume(handle))
    {
        return RENAME_UNSUPPORTED;
    }
    FILE_ID_INFO info;
    if (!GetFileInformationByHandleEx(handle, FileIdInfo, &info, (DWORD)sizeof info))
    {
        return RENAME_IDENTITY_FAILED;
    }
    format_identity(&info, out);
    return RENAME_COMPLETED;
}

static enum rename_outcome read_identity(const wchar_t *_Nonnull path, bool directory,
                                         char *_Nonnull out)
{
    HANDLE handle = open_entry(path, FILE_READ_ATTRIBUTES, directory);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return RENAME_IDENTITY_FAILED;
    }
    enum rename_outcome outcome = identity_of(handle, out);
    CloseHandle(handle);
    return outcome;
}

/* 既に移り終えた側が記録と同じ実体か確かめる（決定 5）。 */
static enum rename_outcome verify_entry(const wchar_t *_Nonnull path, bool directory,
                                        const char *_Nonnull identity)
{
    char actual[rename_journal_id_length + 1];
    enum rename_outcome read = read_identity(path, directory, actual);
    if (read != RENAME_COMPLETED)
    {
        return read;
    }
    return strcmp(actual, identity) == 0 ? RENAME_COMPLETED : RENAME_MISMATCHED;
}

/* 置換なしの rename。事前確認の後に同名が現れても OS が拒む（決定 4）。 */
static enum rename_outcome rename_by_handle(HANDLE handle, const wchar_t *_Nonnull target)
{
    size_t units = wide_length(target);
    size_t bytes = offsetof(FILE_RENAME_INFO, FileName) + (units + 1) * sizeof(wchar_t);
    FILE_RENAME_INFO *_Nullable info = calloc(1, bytes);
    if (info == nullptr)
    {
        return RENAME_OUT_OF_MEMORY;
    }
    info->ReplaceIfExists = FALSE;
    info->RootDirectory = nullptr;
    info->FileNameLength = (DWORD)(units * sizeof(wchar_t));
    memcpy(info->FileName, target, (units + 1) * sizeof(wchar_t));
    bool renamed = SetFileInformationByHandle(handle, FileRenameInfo, info, (DWORD)bytes) != 0;
    free(info);
    return renamed ? RENAME_COMPLETED : RENAME_PENDING;
}

/* DELETE を持つハンドルで開き、識別子が一致した実体だけを動かす（決定 4）。 */
static enum rename_outcome move_entry(const wchar_t *_Nonnull from, const wchar_t *_Nonnull to,
                                      bool directory, const char *_Nonnull identity)
{
    HANDLE handle = open_entry(from, DELETE | FILE_READ_ATTRIBUTES, directory);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return RENAME_PENDING;
    }
    char actual[rename_journal_id_length + 1];
    enum rename_outcome outcome = identity_of(handle, actual);
    if (outcome == RENAME_COMPLETED)
    {
        outcome = strcmp(actual, identity) == 0 ? rename_by_handle(handle, to) : RENAME_MISMATCHED;
    }
    CloseHandle(handle);
    return outcome;
}

/* 旧と新の存在と保存済み識別子で段階を確定する（決定 5 の表）。 */
static enum rename_outcome advance_entry(const wchar_t *_Nonnull from, const wchar_t *_Nonnull to,
                                         bool directory, const char *_Nonnull identity)
{
    bool from_present = false;
    bool to_present = false;
    if (!entry_exists(from, &from_present) || !entry_exists(to, &to_present))
    {
        return RENAME_UNSUPPORTED;
    }
    if (identity[0] == '\0')
    {
        /* 履歴を持たない意図は、旧・新の両側不在だけを受理する（決定 5）。 */
        return from_present || to_present ? RENAME_MISMATCHED : RENAME_COMPLETED;
    }
    if (from_present && !to_present)
    {
        return move_entry(from, to, directory, identity);
    }
    if (!from_present && to_present)
    {
        return verify_entry(to, directory, identity);
    }
    return RENAME_MISMATCHED;
}

static enum rename_outcome guard_parent(struct rename_guards *_Nonnull guards,
                                        const wchar_t *_Nonnull path, bool optional)
{
    HANDLE handle = open_entry(path, FILE_READ_ATTRIBUTES, true);
    if (handle == INVALID_HANDLE_VALUE)
    {
        DWORD error = GetLastError();
        bool absent = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        return optional && absent ? RENAME_COMPLETED : RENAME_UNSUPPORTED;
    }
    guards->items[guards->count] = handle;
    guards->count += 1;
    return plain_entry(handle) ? RENAME_COMPLETED : RENAME_UNSUPPORTED;
}

static void release_guards(struct rename_guards *_Nonnull guards)
{
    for (size_t index = 0; index < guards->count; ++index)
    {
        CloseHandle(guards->items[index]);
    }
    guards->count = 0;
}

/* data / data\<カテゴリ> / data\.history / data\.history\<カテゴリ> を開いたまま持つ。
 * 履歴側はまだ無くてよい。exe の置き場までの祖先は対象にしない（決定 4）。 */
static enum rename_outcome guard_parents(const struct persistence_adapter *_Nonnull adapter,
                                         const struct note_rename *_Nonnull plan,
                                         struct rename_guards *_Nonnull guards)
{
    const char *_Nonnull category = note_rename_category(plan);
    wchar_t paths[rename_guard_capacity][path_capacity];
    size_t length = 0;
    size_t nested = 0;
    if (!append_units(paths[0], &length, adapter->root, adapter->root_length) ||
        !compose(adapter, category, nullptr, paths[1]) ||
        !compose_history_root(adapter, paths[2], &nested) ||
        !compose_history_category(adapter, category, paths[3]))
    {
        return RENAME_UNSUPPORTED;
    }
    for (size_t index = 0; index < rename_guard_capacity; ++index)
    {
        enum rename_outcome outcome = guard_parent(guards, paths[index], index >= 2);
        if (outcome != RENAME_COMPLETED)
        {
            return outcome;
        }
    }
    return RENAME_COMPLETED;
}

/* 移動先の md と履歴ディレクトリの両不在を先に確かめる（決定 4）。 */
static enum rename_outcome targets_absent(const struct rename_paths *_Nonnull paths)
{
    bool note_present = false;
    bool history_present = false;
    if (!entry_exists(paths->note_to, &note_present) ||
        !entry_exists(paths->history_to, &history_present))
    {
        return RENAME_UNSUPPORTED;
    }
    return note_present || history_present ? RENAME_NAME_TAKEN : RENAME_COMPLETED;
}

/* 元 md の識別子と、あれば元履歴の識別子。履歴が無ければ空を記録する（決定 5）。 */
static enum rename_outcome source_identities(const struct rename_paths *_Nonnull paths,
                                             char *_Nonnull file_id, char *_Nonnull history_id)
{
    enum rename_outcome outcome = read_identity(paths->note_from, false, file_id);
    if (outcome != RENAME_COMPLETED)
    {
        return outcome;
    }
    bool history_present = false;
    if (!entry_exists(paths->history_from, &history_present))
    {
        return RENAME_UNSUPPORTED;
    }
    if (!history_present)
    {
        history_id[0] = '\0';
        return RENAME_COMPLETED;
    }
    return read_identity(paths->history_from, true, history_id);
}

/* 版 1 の記録を新規公開し、flush が終わってからだけ実体を動かす（決定 5）。 */
static enum rename_outcome publish_journal(const struct persistence_adapter *_Nonnull adapter,
                                           const struct note_rename *_Nonnull plan,
                                           const char *_Nonnull file_id,
                                           const char *_Nonnull history_id)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, nullptr, journal_leaf, path))
    {
        return RENAME_JOURNAL_FAILED;
    }
    struct json_writer *_Nullable writer = nullptr;
    if (json_writer_create(&writer) != JSON_WRITER_ACCEPTED)
    {
        return RENAME_OUT_OF_MEMORY;
    }
    enum rename_journal_outcome written = rename_journal_write(plan, file_id, history_id, writer);
    enum rename_outcome outcome = RENAME_JOURNAL_FAILED;
    if (written == RENAME_JOURNAL_OUT_OF_MEMORY)
    {
        outcome = RENAME_OUT_OF_MEMORY;
    }
    else if (written == RENAME_JOURNAL_ACCEPTED &&
             file_bytes_create(path, json_writer_text(writer), json_writer_length(writer)) ==
                 PERSISTENCE_STORED)
    {
        outcome = RENAME_COMPLETED;
    }
    json_writer_destroy(writer);
    return outcome;
}

/* index.json が書けた後にだけ記録を消して完了とする（決定 6）。削除失敗も未完了。 */
static enum rename_outcome finish_rename(struct persistence_adapter *_Nonnull adapter,
                                         const struct note_rename *_Nonnull plan)
{
    if (write_note_ledger(adapter, note_rename_category(plan), note_rename_ledger(plan)) !=
        PERSISTENCE_STORED)
    {
        return RENAME_PENDING;
    }
    wchar_t path[path_capacity];
    if (!compose(adapter, nullptr, journal_leaf, path) || !DeleteFileW(path))
    {
        return RENAME_PENDING;
    }
    return RENAME_COMPLETED;
}

/* 記録の公開後の段。履歴 → md → index.json → 記録の削除の順にだけ進む（決定 5 / 6）。 */
static enum rename_outcome advance_rename(struct persistence_adapter *_Nonnull adapter,
                                          const struct note_rename *_Nonnull plan,
                                          const char *_Nonnull file_id,
                                          const char *_Nonnull history_id)
{
    struct rename_paths paths;
    if (!compose_rename_paths(adapter, plan, &paths))
    {
        return RENAME_UNSUPPORTED;
    }
    enum rename_outcome outcome =
        advance_entry(paths.history_from, paths.history_to, true, history_id);
    if (outcome == RENAME_COMPLETED)
    {
        outcome = advance_entry(paths.note_from, paths.note_to, false, file_id);
    }
    return outcome == RENAME_COMPLETED ? finish_rename(adapter, plan) : outcome;
}

/* 記録を読んで型のある値にする。無ければ NONE、読めない・形が違うなら消さずに BROKEN。 */
static enum rename_outcome read_journal(const struct persistence_adapter *_Nonnull adapter,
                                        struct rename_journal *_Nullable *_Nonnull out)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, nullptr, journal_leaf, path))
    {
        return RENAME_JOURNAL_BROKEN;
    }
    struct file_bytes *_Nullable bytes = nullptr;
    enum persistence_outcome read = file_bytes_read(path, &bytes);
    if (read == PERSISTENCE_ABSENT)
    {
        return RENAME_NONE;
    }
    if (read != PERSISTENCE_LOADED)
    {
        return read == PERSISTENCE_OUT_OF_MEMORY ? RENAME_OUT_OF_MEMORY : RENAME_JOURNAL_BROKEN;
    }
    enum rename_journal_outcome parsed =
        rename_journal_parse(file_bytes_data(bytes), file_bytes_length(bytes), out);
    file_bytes_destroy(bytes);
    switch (parsed)
    {
    case RENAME_JOURNAL_ACCEPTED:
        return RENAME_COMPLETED;
    case RENAME_JOURNAL_INVALID:
    case RENAME_JOURNAL_UNSUPPORTED_VERSION:
        return RENAME_JOURNAL_BROKEN;
    case RENAME_JOURNAL_OUT_OF_MEMORY:
        return RENAME_OUT_OF_MEMORY;
    }
    return RENAME_JOURNAL_BROKEN;
}

/* 既に公開された記録の続きだけを行う。親は同じように守る。 */
static enum rename_outcome resume_journal(struct persistence_adapter *_Nonnull adapter,
                                          const struct rename_journal *_Nonnull journal)
{
    const struct note_rename *_Nonnull plan = rename_journal_rename(journal);
    struct rename_guards guards = {.items = {nullptr}, .count = 0};
    enum rename_outcome outcome = guard_parents(adapter, plan, &guards);
    if (outcome == RENAME_COMPLETED)
    {
        outcome = advance_rename(adapter, plan, rename_journal_file_id(journal),
                                 rename_journal_history_id(journal));
    }
    release_guards(&guards);
    return outcome;
}

/* 新しい意図。移動先の不在・親と対象の健全さ・識別子を確かめてから記録を公開する（決定 4 / 5）。 */
static enum rename_outcome start_rename(struct persistence_adapter *_Nonnull adapter,
                                        const struct note_rename *_Nonnull plan)
{
    struct rename_paths paths;
    if (!compose_rename_paths(adapter, plan, &paths))
    {
        return RENAME_UNSUPPORTED;
    }
    struct rename_guards guards = {.items = {nullptr}, .count = 0};
    char file_id[rename_journal_id_length + 1] = {'\0'};
    char history_id[rename_journal_id_length + 1] = {'\0'};
    enum rename_outcome outcome = targets_absent(&paths);
    if (outcome == RENAME_COMPLETED)
    {
        outcome = guard_parents(adapter, plan, &guards);
    }
    if (outcome == RENAME_COMPLETED)
    {
        outcome = source_identities(&paths, file_id, history_id);
    }
    if (outcome == RENAME_COMPLETED)
    {
        outcome = publish_journal(adapter, plan, file_id, history_id);
    }
    if (outcome == RENAME_COMPLETED)
    {
        outcome = advance_rename(adapter, plan, file_id, history_id);
    }
    release_guards(&guards);
    return outcome;
}

static enum rename_outcome rename_note(struct persistence_adapter *_Nonnull adapter,
                                       const struct note_rename *_Nonnull plan)
{
    if (adapter->lock == nullptr)
    {
        /* 書けない data/ では記録を公開する前に断る（決定 3 の補正）。意図は残らない。 */
        return RENAME_UNLOCKED;
    }
    struct rename_journal *_Nullable journal = nullptr;
    enum rename_outcome read = read_journal(adapter, &journal);
    if (read == RENAME_NONE)
    {
        return start_rename(adapter, plan);
    }
    if (read != RENAME_COMPLETED)
    {
        return read;
    }
    enum rename_outcome outcome = note_rename_equals(plan, rename_journal_rename(journal))
                                      ? resume_journal(adapter, journal)
                                      : RENAME_MISMATCHED;
    rename_journal_destroy(journal);
    return outcome;
}

static enum rename_outcome recover_rename(struct persistence_adapter *_Nonnull adapter)
{
    struct rename_journal *_Nullable journal = nullptr;
    enum rename_outcome read = read_journal(adapter, &journal);
    if (read != RENAME_COMPLETED)
    {
        return read;
    }
    /* 記録があって錠が無ければ生成の時点で断っているので、ここは持っている（決定 3 の補正）。 */
    enum rename_outcome outcome =
        adapter->lock == nullptr ? RENAME_UNLOCKED : resume_journal(adapter, journal);
    rename_journal_destroy(journal);
    return outcome;
}

/* 同じ data/ を使うプロセスを 1 つに直列化する（決定 3 と 2026-09-16 の補正）。
 * 共有違反だけが起動の拒否で、アクセス拒否・読み取り専用・data/ の不在は錠無しの起動を許す。
 * ただし復旧すべき記録があるのに錠を取れないなら、復旧できないので起動しない。 */
static enum persistence_adapter_outcome acquire_lock(struct persistence_adapter *_Nonnull adapter)
{
    wchar_t path[path_capacity];
    if (!compose(adapter, nullptr, lock_leaf, path))
    {
        return PERSISTENCE_ADAPTER_NO_MODULE_PATH;
    }
    HANDLE lock = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (lock != INVALID_HANDLE_VALUE)
    {
        adapter->lock = lock;
        return PERSISTENCE_ADAPTER_CREATED;
    }
    if (GetLastError() == ERROR_SHARING_VIOLATION)
    {
        return PERSISTENCE_ADAPTER_DATA_IN_USE;
    }
    wchar_t journal[path_capacity];
    bool present = false;
    if (compose(adapter, nullptr, journal_leaf, journal) && entry_exists(journal, &present) &&
        !present)
    {
        return PERSISTENCE_ADAPTER_CREATED;
    }
    return PERSISTENCE_ADAPTER_RECOVERY_LOCKED;
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
        .create_note = create_note,
        .move_note = move_note,
        .rename_note = rename_note,
        .recover_rename = recover_rename,
        .read_note_ledger = read_note_ledger,
        .write_note_ledger = write_note_ledger,
    };
    return port;
}

void persistence_adapter_destroy(struct persistence_adapter *_Nullable adapter)
{
    if (adapter != nullptr && adapter->lock != nullptr)
    {
        /* 錠のファイルは残してよい。所有は OS のハンドルで判定する（ADR 0022 の決定 3）。 */
        CloseHandle(adapter->lock);
    }
    free(adapter);
}
