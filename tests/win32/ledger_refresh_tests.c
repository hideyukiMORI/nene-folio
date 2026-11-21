/* #1069 / ADR0117 第7段: 実ファイルでの台帳の読み直し。
 * アプリの状態が生きている間に、外から data/ の .md と index.json を書き換え、
 * 台帳を書く意図の後でディスクの index.json を読んで、外の変更が消えていないことを確かめる。
 * 窓は作らない。保存先は本物のアダプタで、data/ はこの試験の実行ファイルの隣に閉じる。 */
#include "appearance_adapter.h"
#include "category_ledger.h"
#include "category_name.h"
#include "category_rename_target.h"
#include "clipboard_adapter.h"
#include "drawer_layout.h"
#include "drawer_metrics.h"
#include "drawer_row.h"
#include "folio_state.h"
#include "image_adapter.h"
#include "note_destination.h"
#include "note_ledger.h"
#include "note_name.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "storage_adapter.h"
#include "storage_native_path.h"
#include "storage_state.h"
#include "test_window_file.h"
#include "transfer_adapter.h"
#include "web_link_adapter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

static void require(bool condition, const char *_Nonnull message)
{
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

struct fixture
{
    struct storage_adapter *_Nullable storage;
    struct storage_state *_Nullable location;
    struct persistence_adapter *_Nullable files;
    struct appearance_adapter *_Nullable appearance;
    struct regex_adapter *_Nullable regex;
    struct clipboard_adapter *_Nullable clipboard;
    struct image_adapter *_Nullable images;
    struct folio_state *_Nullable state;
    wchar_t data[storage_native_capacity];
};

/* data/ の下の path を組む。category が nullptr なら data/ の直下。 */
static void compose(const struct fixture *_Nonnull fixture, const wchar_t *_Nullable category,
                    const wchar_t *_Nonnull leaf, wchar_t *_Nonnull out)
{
    int written = category == nullptr
                      ? swprintf_s(out, storage_native_capacity, L"%ls\\%ls", fixture->data, leaf)
                      : swprintf_s(out, storage_native_capacity, L"%ls\\%ls\\%ls", fixture->data,
                                   category, leaf);
    require(written > 0, "fixture path fits");
}

static void remove_tree(const wchar_t *_Nonnull folder);

/* folder の中の1つを消す。フォルダなら中身ごと。 */
static void remove_entry(const wchar_t *_Nonnull folder, const WIN32_FIND_DATAW *_Nonnull entry)
{
    if (wcscmp(entry->cFileName, L".") == 0 || wcscmp(entry->cFileName, L"..") == 0)
        return;
    wchar_t path[storage_native_capacity];
    require(swprintf_s(path, storage_native_capacity, L"%ls\\%ls", folder, entry->cFileName) > 0,
            "tree entry path");
    /* A junction or link is removed as itself, never followed (independent review of #1085). */
    if ((entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        (entry->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
    {
        remove_tree(path);
        require(RemoveDirectoryW(path) != 0, "remove a folder left by the previous run");
        return;
    }
    if ((entry->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
    {
        require(RemoveDirectoryW(path) != 0, "remove a folder link left by the previous run");
        return;
    }
    (void)SetFileAttributesW(path, FILE_ATTRIBUTE_NORMAL);
    require(DeleteFileW(path) != 0, "remove a file left by the previous run");
}

/* この試験だけの data/ を、前の実行の残りごと消す。消すのは実行ファイルの隣の data/ に限る
 * （open_state が、場所がこの試験の出力先であることを先に確かめる）。 */
static void remove_tree(const wchar_t *_Nonnull folder)
{
    wchar_t pattern[storage_native_capacity];
    require(swprintf_s(pattern, storage_native_capacity, L"%ls\\*", folder) > 0, "tree pattern");
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do
        remove_entry(folder, &entry);
    while (FindNextFileW(search, &entry));
    FindClose(search);
}

static void write_file(const struct fixture *_Nonnull fixture, const wchar_t *_Nullable category,
                       const wchar_t *_Nonnull leaf, const char *_Nonnull text)
{
    wchar_t path[storage_native_capacity];
    compose(fixture, category, leaf, path);
    HANDLE file =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "outside writer opens the file");
    DWORD length = (DWORD)strlen(text);
    DWORD written = 0;
    bool stored = WriteFile(file, text, length, &written, nullptr) != 0 && written == length;
    CloseHandle(file);
    require(stored, "outside writer stores every byte");
}

static void delete_file(const struct fixture *_Nonnull fixture, const wchar_t *_Nonnull category,
                        const wchar_t *_Nonnull leaf)
{
    wchar_t path[storage_native_capacity];
    compose(fixture, category, leaf, path);
    require(DeleteFileW(path) != 0, "outside writer deletes the note");
}

static bool file_exists(const struct fixture *_Nonnull fixture, const wchar_t *_Nonnull category,
                        const wchar_t *_Nonnull leaf)
{
    wchar_t path[storage_native_capacity];
    compose(fixture, category, leaf, path);
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

/* ファイル全体を読む。無ければ nullptr。呼び出し側が free する。 */
static char *_Nullable read_file(const struct fixture *_Nonnull fixture,
                                 const wchar_t *_Nullable category, const wchar_t *_Nonnull leaf,
                                 size_t *_Nonnull length)
{
    wchar_t path[storage_native_capacity];
    compose(fixture, category, leaf, path);
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return nullptr;
    LARGE_INTEGER size;
    require(GetFileSizeEx(file, &size) != 0 && size.QuadPart < 1024 * 1024, "small ledger file");
    char *text = malloc((size_t)size.QuadPart + 1);
    require(text != nullptr, "ledger buffer");
    DWORD read = 0;
    bool whole = ReadFile(file, text, (DWORD)size.QuadPart, &read, nullptr) != 0 &&
                 read == (DWORD)size.QuadPart;
    CloseHandle(file);
    require(whole, "read the whole ledger");
    text[read] = '\0';
    *length = read;
    return text;
}

static struct note_ledger *_Nonnull disk_index(const struct fixture *_Nonnull fixture,
                                               const wchar_t *_Nonnull category)
{
    size_t length = 0;
    char *text = read_file(fixture, category, L"index.json", &length);
    require(text != nullptr, "index.json is on disk");
    struct note_ledger *ledger = nullptr;
    require(note_ledger_parse(text, length, &ledger) == NOTE_LEDGER_ACCEPTED,
            "index.json on disk is a valid ledger");
    free(text);
    return ledger;
}

/* ディスクの index.json の並びを "a/b/c" で、星を "*" の後ろに付けて返す。 */
static void disk_order(const struct fixture *_Nonnull fixture, const wchar_t *_Nonnull category,
                       char *_Nonnull out, size_t capacity)
{
    struct note_ledger *ledger = disk_index(fixture, category);
    size_t used = 0;
    out[0] = '\0';
    for (size_t index = 0; index < note_ledger_count(ledger); ++index)
    {
        int written = snprintf(out + used, capacity - used, "%s%s%s", index == 0 ? "" : "/",
                               note_ledger_name(ledger, index),
                               note_ledger_favorite(ledger, index) ? "*" : "");
        require(written > 0 && (size_t)written < capacity - used, "order text fits");
        used += (size_t)written;
    }
    note_ledger_destroy(ledger);
}

static void expect_disk(const struct fixture *_Nonnull fixture, const wchar_t *_Nonnull category,
                        const char *_Nonnull expected, const char *_Nonnull message)
{
    char order[256];
    disk_order(fixture, category, order, sizeof order);
    if (strcmp(order, expected) != 0)
        fprintf(stderr, "index.json: \"%s\" expected \"%s\"\n", order, expected);
    require(strcmp(order, expected) == 0, message);
}

static size_t category_at(const struct fixture *_Nonnull fixture, const char *_Nonnull name)
{
    size_t count = folio_state_category_count(fixture->state);
    for (size_t index = 0; index < count; ++index)
        if (strcmp(folio_state_category_name(fixture->state, index), name) == 0)
            return index;
    require(false, "the category is listed");
    return 0;
}

/* 状態の一覧（ドロワーの行）での位置。無ければ false。カテゴリは展開している。 */
static bool find_note(const struct fixture *_Nonnull fixture, size_t category,
                      const char *_Nonnull name, struct note_ref *_Nonnull out)
{
    static const struct drawer_metrics metrics = {.top_padding = 0,
                                                  .row_height = 10,
                                                  .category_height = 10,
                                                  .category_gap = 0,
                                                  .category_indent = 1,
                                                  .note_indent = 2,
                                                  .viewport_height = 100};
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(fixture->state, metrics, &layout) == FOLIO_STATE_READY,
            "drawer rows");
    bool found = false;
    size_t count = drawer_layout_row_count(layout);
    for (size_t index = 0; index < count && !found; ++index)
    {
        struct drawer_row row = drawer_layout_row(layout, index);
        if (row.kind == DRAWER_ROW_NOTE && row.category == category && strcmp(row.text, name) == 0)
        {
            *out = (struct note_ref){row.category, row.note};
            found = true;
        }
    }
    drawer_layout_destroy(layout);
    return found;
}

static struct note_ref note_at(const struct fixture *_Nonnull fixture,
                               const char *_Nonnull category, const char *_Nonnull name)
{
    struct note_ref ref = {0};
    require(find_note(fixture, category_at(fixture, category), name, &ref), "the note is listed");
    return ref;
}

/* 台帳を書く意図の結果を、窓と同じく1回だけ知らせへ通す。 */
static enum folio_state_outcome told(struct fixture *_Nonnull fixture,
                                     enum folio_state_outcome outcome)
{
    return folio_state_ledger_outcome(fixture->state, outcome);
}

static void seed(struct fixture *_Nonnull fixture)
{
    remove_tree(fixture->data);
    require(CreateDirectoryW(fixture->data, nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS,
            "data folder");
    wchar_t folder[storage_native_capacity];
    compose(fixture, nullptr, L"inbox", folder);
    require(CreateDirectoryW(folder, nullptr) != 0, "inbox folder");
    compose(fixture, nullptr, L"other", folder);
    require(CreateDirectoryW(folder, nullptr) != 0, "other folder");
    write_file(fixture, nullptr, L"categories.json",
               "{\"version\": 1, \"categories\": ["
               "{\"name\": \"inbox\", \"color\": \"#3A7BD5\", \"expanded\": true}, "
               "{\"name\": \"other\", \"color\": \"#D5803A\", \"expanded\": true}]}");
    write_file(fixture, L"inbox", L"index.json",
               "{\"version\": 2, \"notes\": [\"alpha\", \"beta\", \"gamma\"], \"favorites\": []}");
    write_file(fixture, L"other", L"index.json",
               "{\"version\": 2, \"notes\": [\"delta\"], \"favorites\": []}");
    write_file(fixture, L"inbox", L"alpha.md", "# alpha\n");
    write_file(fixture, L"inbox", L"beta.md", "# beta\n");
    write_file(fixture, L"inbox", L"gamma.md", "# gamma\n");
    write_file(fixture, L"other", L"delta.md", "# delta\n");
}

static void open_state(struct fixture *_Nonnull fixture)
{
    require(storage_adapter_create(&fixture->storage) == STORAGE_READY, "storage adapter");
    struct storage_port storage = storage_adapter_port(fixture->storage);
    test_window_file_attach(&storage);
    require(storage_state_create(&storage, &fixture->location) == STORAGE_READY,
            "portable storage beside the test");
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), fixture->data,
                                     &length),
            "data path");
    require(length > 30 &&
                wcsstr(fixture->data, L"\\verification-ledger-refresh\\data") ==
                    fixture->data + length - wcslen(L"\\verification-ledger-refresh\\data"),
            "the data folder is this test's own");
    seed(fixture);
    require(persistence_adapter_create(storage_state_directory(fixture->location),
                                       &fixture->files) == PERSISTENCE_ADAPTER_CREATED,
            "real persistence adapter");
    require(appearance_adapter_create(&fixture->appearance) == APPEARANCE_ADAPTER_CREATED,
            "appearance adapter");
    require(regex_adapter_create(&fixture->regex) == REGEX_ADAPTER_CREATED, "regex adapter");
    require(clipboard_adapter_create(&fixture->clipboard) == CLIPBOARD_ADAPTER_CREATED,
            "clipboard adapter");
    require(image_adapter_create(storage_state_directory(fixture->location), &fixture->images) ==
                IMAGE_READY,
            "image adapter");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct appearance_port appearance = appearance_adapter_port(fixture->appearance);
    struct regex_port regex = regex_adapter_port(fixture->regex);
    struct web_link_port web = web_link_adapter_port();
    struct clipboard_port clipboard = clipboard_adapter_port(fixture->clipboard);
    struct transfer_port transfer = transfer_adapter_port();
    struct image_port images = image_adapter_port(fixture->images);
    struct folio_ports ports = {.persistence = &disk,
                                .appearance = &appearance,
                                .regex = &regex,
                                .web_link = &web,
                                .clipboard = &clipboard,
                                .transfer = &transfer,
                                .images = &images};
    require(folio_state_create(&ports, &fixture->state) == FOLIO_STATE_READY, "application state");
    require(folio_state_set_note_order(fixture->state, NOTE_ORDER_MANUAL) == FOLIO_STATE_READY,
            "manual order, so that a category with stars can be reordered");
}

static void close_state(struct fixture *_Nonnull fixture)
{
    folio_state_destroy(fixture->state);
    image_adapter_destroy(fixture->images);
    clipboard_adapter_destroy(fixture->clipboard);
    regex_adapter_destroy(fixture->regex);
    appearance_adapter_destroy(fixture->appearance);
    persistence_adapter_destroy(fixture->files);
    storage_state_destroy(fixture->location);
    storage_adapter_destroy(fixture->storage);
}

/* other\delta.md holds exactly this text. */
static void expect_delta(const struct fixture *_Nonnull fixture, const char *_Nonnull expected,
                         const char *_Nonnull message)
{
    size_t length = 0;
    char *_Nullable text = read_file(fixture, L"other", L"delta.md", &length);
    require(text != nullptr && strcmp(text, expected) == 0, message);
    free(text);
}

/* ADR0119 決定4 の続き: テキストでなくなったファイル・開けないファイル・BOM と CRLF のファイル、
 * そして破棄の出口（ファイルから読み直してから保存し直す）。 */
static void verify_guarded_save_files(struct fixture *_Nonnull fixture)
{
    struct note_ref delta = note_at(fixture, "other", "delta");
    write_file(fixture, L"other", L"delta.md", "\xEF\xBB\xBF# crlf\r\nbody\r\n");
    require(folio_state_select_note(fixture->state, delta.category, delta.note,
                                    FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(fixture->state) == FOLIO_STATE_READY &&
                folio_state_store_note(fixture->state, u"first", 5) == FOLIO_STATE_READY &&
                folio_state_store_note(fixture->state, u"second", 6) == FOLIO_STATE_READY,
            "a note read with a BOM and CRLF is saved twice: its own form is not an outside change");
    write_file(fixture, L"other", L"delta.md", "\xFF\xFE not text\n");
    require(folio_state_store_note(fixture->state, u"third", 5) == FOLIO_STATE_NOTE_SOURCE_CHANGED,
            "bytes written outside that are no text stop the save as an outside change");
    expect_delta(fixture, "\xFF\xFE not text\n", "and they stay on disk");
    write_file(fixture, L"other", L"delta.md", "# theirs\n");
    wchar_t path[storage_native_capacity];
    compose(fixture, L"other", L"delta.md", path);
    HANDLE held = CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
    require(held != INVALID_HANDLE_VALUE, "another program holds the file open");
    enum folio_state_outcome locked = folio_state_store_note(fixture->state, u"third", 5);
    CloseHandle(held);
    require(locked == FOLIO_STATE_HISTORY_FAILED,
            "a file that cannot be opened keeps the ordinary save's outcome");
    expect_delta(fixture, "# theirs\n", "nothing was written over it");
    require(folio_state_reload_document(fixture->state) == FOLIO_STATE_READY &&
                strcmp(folio_state_pane_text(fixture->state), "# theirs\n") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "discarding reads the outside version from the file");
    require(folio_state_store_note(fixture->state, u"# theirs\r\nmine", 14) == FOLIO_STATE_READY,
            "and the next save, made from it, goes through");
    expect_delta(fixture, "# theirs\nmine", "the file is the outside version with the edit");
}

/* ADR0119 決定4: 編集中のノートが外で書き換えられていたら、保存は止まり、外の版が残る。 */
static void verify_guarded_save(struct fixture *_Nonnull fixture)
{
    struct note_ref delta = note_at(fixture, "other", "delta");
    bool stale = true;
    require(folio_state_select_note(fixture->state, delta.category, delta.note,
                                    FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(fixture->state) == FOLIO_STATE_READY &&
                folio_state_document_stale(fixture->state, &stale) == FOLIO_STATE_READY && !stale,
            "the note is open for editing and its file is what was loaded");
    write_file(fixture, L"other", L"delta.md", "# theirs\n");
    require(folio_state_document_stale(fixture->state, &stale) == FOLIO_STATE_READY && stale,
            "the outside write is noticed");
    require(folio_state_store_note(fixture->state, u"mine", 4) == FOLIO_STATE_NOTE_SOURCE_CHANGED,
            "an ordinary save does not replace a file that changed outside");
    expect_delta(fixture, "# theirs\n", "the outside version stays on disk");
    delete_file(fixture, L"other", L"delta.md");
    require(folio_state_store_note(fixture->state, u"mine", 4) == FOLIO_STATE_READY,
            "a file removed outside is written again: nothing is overwritten");
    expect_delta(fixture, "mine", "the edited text is the file");
    require(folio_state_store_note(fixture->state, u"mine again", 10) == FOLIO_STATE_READY &&
                folio_state_document_stale(fixture->state, &stale) == FOLIO_STATE_READY && !stale,
            "the app's own save is not an outside change for the next save");
    write_file(fixture, L"other", L"delta.md", "# theirs again\n");
    require(folio_state_store_note(fixture->state, u"mine again", 10) == FOLIO_STATE_READY,
            "unedited text is not saved and not compared");
    expect_delta(fixture, "# theirs again\n", "nothing was written");
    require(folio_state_store_note_checked(fixture->state, u"mine again", 10) ==
                FOLIO_STATE_NOTE_SOURCE_CHANGED,
            "the checked save compares even unedited text");
    require(folio_state_select_note(fixture->state, delta.category, delta.note,
                                    FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_document_stale(fixture->state, &stale) == FOLIO_STATE_READY && !stale,
            "opening the note again reads the outside version");
}

/* 星: 外で足したノートが、星を書いた index.json に残る。 */
static void verify_star(struct fixture *_Nonnull fixture)
{
    expect_disk(fixture, L"inbox", "alpha/beta/gamma", "the seeded ledger is untouched at start");
    write_file(fixture, L"inbox", L"outside1.md", "# outside 1\n");
    require(told(fixture,
                 folio_state_toggle_favorite(fixture->state, note_at(fixture, "inbox", "alpha"))) ==
                FOLIO_STATE_LEDGER_REFRESHED,
            "the star is stored and the fresh list is told once");
    expect_disk(fixture, L"inbox", "alpha*/beta/gamma/outside1",
                "a note added outside stays in the ledger the star writes");
    struct note_ref added = {0};
    require(find_note(fixture, category_at(fixture, "inbox"), "outside1", &added),
            "the note added outside is listed");
}

/* 並べ替え: 外で index.json の順を変えても、利用者の動かした1つだけが当たる。 */
static void verify_reorder(struct fixture *_Nonnull fixture)
{
    write_file(fixture, L"inbox", L"index.json",
               "{\"version\": 2, \"notes\": [\"gamma\", \"alpha\", \"beta\", \"outside1\"], "
               "\"favorites\": [\"alpha\"]}");
    write_file(fixture, L"inbox", L"outside2.md", "# outside 2\n");
    /* メモリの並びは alpha/beta/gamma/outside1。outside1 を alpha の場所へ動かす。行き先も名前で
     * 引き直すので、外の順では alpha の前（2番目）に入る（ADR0117）。 */
    struct note_ref from = note_at(fixture, "inbox", "outside1");
    struct note_ref to = note_at(fixture, "inbox", "alpha");
    require(told(fixture, folio_state_move_note(fixture->state, from, to)) ==
                FOLIO_STATE_LEDGER_REFRESHED,
            "the reorder is applied to the order written outside");
    expect_disk(fixture, L"inbox", "gamma/outside1/alpha*/beta/outside2",
                "the outside order, the added note and the star survive the reorder");
}

/* 初回保存: 外で足したノートと、新しいノートの両方が台帳に入る。
 * 台帳はフォルダを変えた後に、読み直した index.json とフォルダの走査を突き合わせて作る
 * （ADR0117 決定の第4段）。index.json に無い名前（外のノートと、いま作ったノート）は、走査の順
 * （NTFS では名前の順）で末尾に並ぶ。だから新しいノートが最後とは限らない（第7段で確かめた）。 */
static void verify_first_save(struct fixture *_Nonnull fixture)
{
    write_file(fixture, L"inbox", L"outside3.md", "# outside 3\n");
    size_t inbox = category_at(fixture, "inbox");
    struct note_name *name = nullptr;
    require(note_name_create("fresh", 5, &name) == NOTE_NAME_ACCEPTED, "new note name");
    struct note_destination destination = {.category = inbox, .name = name};
    require(folio_state_new_note(fixture->state, inbox, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "an untitled note in inbox");
    enum folio_state_outcome outcome =
        told(fixture, folio_state_store_new(fixture->state, &destination, u"# fresh\n", 8));
    note_name_destroy(name);
    require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_LEDGER_REFRESHED,
            "the first save is stored");
    require(file_exists(fixture, L"inbox", L"fresh.md"), "the new note is on disk");
    expect_disk(fixture, L"inbox", "gamma/outside1/alpha*/beta/outside2/fresh/outside3",
                "the first save keeps the notes added outside");
}

/* 名前の衝突: 外で作った同じ名前のファイルは上書きしない。 */
static void verify_name_taken(struct fixture *_Nonnull fixture)
{
    write_file(fixture, L"inbox", L"taken.md", "outside owns this\n");
    size_t inbox = category_at(fixture, "inbox");
    struct note_name *name = nullptr;
    require(note_name_create("taken", 5, &name) == NOTE_NAME_ACCEPTED, "taken name");
    struct note_destination destination = {.category = inbox, .name = name};
    require(folio_state_new_note(fixture->state, inbox, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "another untitled note");
    enum folio_state_outcome outcome =
        told(fixture, folio_state_store_new(fixture->state, &destination, u"mine\n", 5));
    note_name_destroy(name);
    require(outcome == FOLIO_STATE_NAME_TAKEN, "a name taken outside is refused");
    size_t length = 0;
    char *text = read_file(fixture, L"inbox", L"taken.md", &length);
    require(text != nullptr && strcmp(text, "outside owns this\n") == 0,
            "the file made outside is not overwritten");
    free(text);
    struct note_ref shown = {0};
    require(find_note(fixture, inbox, "taken", &shown),
            "the note that holds the name is listed after the refusal");
    require(folio_state_new_note(fixture->state, inbox, FOLIO_UNTITLED_DISCARD) ==
                FOLIO_STATE_READY,
            "drop the refused draft");
}

/* ごみ箱: 消したノートだけが台帳から抜け、外で足したノートは残る。taken は前の段で一覧に出たが
 * index.json にはまだ無いので、outside4 と一緒に走査の順で並ぶ（ディスクが正本）。 */
static void verify_trash(struct fixture *_Nonnull fixture)
{
    write_file(fixture, L"inbox", L"outside4.md", "# outside 4\n");
    enum folio_state_outcome outcome =
        told(fixture,
             folio_state_trash_note(fixture->state, note_at(fixture, "inbox", "gamma"), u"", 0));
    require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_LEDGER_REFRESHED,
            "gamma goes to the bin");
    require(!file_exists(fixture, L"inbox", L"gamma.md"), "gamma left the category folder");
    expect_disk(fixture, L"inbox", "outside1/alpha*/beta/outside2/fresh/outside3/outside4/taken",
                "the bin removes only gamma; every note made outside stays");
}

/* 別カテゴリへの移動: 移動元と移動先の両方で、外で足したノートが残る。 */
static void verify_transfer(struct fixture *_Nonnull fixture)
{
    write_file(fixture, L"inbox", L"outside5.md", "# outside 5\n");
    write_file(fixture, L"other", L"outside6.md", "# outside 6\n");
    enum folio_state_outcome outcome =
        told(fixture,
             folio_state_move_note_to_category(fixture->state, note_at(fixture, "inbox", "beta"),
                                               category_at(fixture, "other")));
    require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_LEDGER_REFRESHED,
            "beta moves to other");
    require(file_exists(fixture, L"other", L"beta.md") &&
                !file_exists(fixture, L"inbox", L"beta.md"),
            "the file moved");
    expect_disk(fixture, L"inbox",
                "outside1/alpha*/outside2/fresh/outside3/outside4/taken/outside5",
                "the source ledger keeps the note added outside");
    expect_disk(fixture, L"other", "delta/outside6/beta",
                "the target ledger keeps its note added outside, with beta at the end");
}

/* 外で消えたノートへの操作は断り、ディスクの台帳は書かない。 */
static void verify_refusal(struct fixture *_Nonnull fixture)
{
    struct note_ref target = note_at(fixture, "inbox", "outside2");
    delete_file(fixture, L"inbox", L"outside2.md");
    size_t before_length = 0;
    char *before = read_file(fixture, L"inbox", L"index.json", &before_length);
    require(before != nullptr, "index.json before the refusal");
    require(told(fixture, folio_state_toggle_favorite(fixture->state, target)) ==
                FOLIO_STATE_LEDGER_CHANGED_OUTSIDE,
            "a star on a note deleted outside is refused");
    size_t after_length = 0;
    char *after = read_file(fixture, L"inbox", L"index.json", &after_length);
    require(after != nullptr && after_length == before_length &&
                memcmp(before, after, before_length) == 0,
            "the refusal writes nothing");
    free(before);
    free(after);
    struct note_ref gone = {0};
    require(!find_note(fixture, category_at(fixture, "inbox"), "outside2", &gone),
            "the list follows the deletion");
    require(told(fixture, folio_state_toggle_favorite(fixture->state,
                                                      note_at(fixture, "inbox", "outside3"))) ==
                FOLIO_STATE_READY,
            "the next star works on the fresh list without a second notice");
    expect_disk(fixture, L"inbox", "outside1/alpha*/fresh/outside3*/outside4/taken/outside5",
                "the next write drops the note deleted outside");
}

/* categories.json on disk as "name+#RRGGBB/…" ("+" open, "-" closed). */
static void expect_categories(const struct fixture *_Nonnull fixture, const char *_Nonnull expected,
                              const char *_Nonnull message)
{
    size_t length = 0;
    char *text = read_file(fixture, nullptr, L"categories.json", &length);
    require(text != nullptr, "categories.json is on disk");
    struct category_ledger *ledger = nullptr;
    require(category_ledger_parse(text, length, &ledger) == CATEGORY_LEDGER_ACCEPTED,
            "categories.json on disk is a valid ledger");
    free(text);
    char order[256] = {0};
    size_t used = 0;
    for (size_t index = 0; index < category_ledger_count(ledger); ++index)
    {
        char hex[rgb_color_hex_length + 1] = {0};
        rgb_color_write_hex(category_ledger_color(ledger, index), hex);
        int written = snprintf(order + used, sizeof order - used, "%s%s%c%s", index == 0 ? "" : "/",
                               category_ledger_name(ledger, index),
                               category_ledger_expanded(ledger, index) ? '+' : '-', hex);
        require(written > 0 && (size_t)written < sizeof order - used, "category text fits");
        used += (size_t)written;
    }
    category_ledger_destroy(ledger);
    if (strcmp(order, expected) != 0)
        fprintf(stderr, "categories.json: \"%s\" expected \"%s\"\n", order, expected);
    require(strcmp(order, expected) == 0, message);
}

static void make_folder(const struct fixture *_Nonnull fixture, const wchar_t *_Nonnull category)
{
    wchar_t folder[storage_native_capacity];
    compose(fixture, nullptr, category, folder);
    require(CreateDirectoryW(folder, nullptr) != 0, "outside writer makes a category folder");
}

/* #1069 stage 5: a category folder made outside is in the categories.json the next toggle
 * writes, and its notes are listed. */
static void verify_category_added(struct fixture *_Nonnull fixture)
{
    make_folder(fixture, L"later");
    write_file(fixture, L"later", L"note.md", "# note\n");
    require(
        told(fixture, folio_state_toggle_category(fixture->state, category_at(fixture, "other"))) ==
            FOLIO_STATE_LEDGER_REFRESHED,
        "other closes, and the fresh list is told once");
    expect_categories(fixture, "inbox+#3A7BD5/other-#D5803A/later+#7F8FA6",
                      "the category made outside is in the ledger the toggle writes");
    struct note_ref listed = {0};
    require(find_note(fixture, category_at(fixture, "later"), "note", &listed),
            "the note of the category made outside is listed");
}

/* categories.json rewritten outside: the order and the other categories' state survive. */
static void verify_category_rewrite(struct fixture *_Nonnull fixture)
{
    write_file(fixture, nullptr, L"categories.json",
               "{\"version\": 1, \"categories\": ["
               "{\"name\": \"later\", \"color\": \"#112233\", \"expanded\": true}, "
               "{\"name\": \"inbox\", \"color\": \"#3A7BD5\", \"expanded\": true}, "
               "{\"name\": \"other\", \"color\": \"#D5803A\", \"expanded\": true}]}");
    require(
        told(fixture, folio_state_recolor_category(fixture->state, category_at(fixture, "inbox"),
                                                   (struct rgb_color){0x10, 0x20, 0x30})) ==
            FOLIO_STATE_LEDGER_REFRESHED,
        "inbox takes its new colour on the order written outside");
    expect_categories(fixture, "later+#112233/inbox+#102030/other+#D5803A",
                      "the outside order, colour and opening stay; only inbox's colour changed");
}

/* A category that gained a note outside is not removed. */
static void verify_category_remove_refused(struct fixture *_Nonnull fixture)
{
    struct category_name *name = nullptr;
    require(category_name_create("spare", 5, &name) == CATEGORY_NAME_ACCEPTED, "category name");
    enum folio_state_outcome made =
        told(fixture, folio_state_create_category(fixture->state, name));
    category_name_destroy(name);
    require(made == FOLIO_STATE_READY, "an empty category is made");
    write_file(fixture, L"spare", L"sneaked.md", "# sneaked\n");
    require(folio_state_delete_category(fixture->state, category_at(fixture, "spare")) ==
                FOLIO_STATE_CATEGORY_NOT_EMPTY,
            "the note put in outside is counted before removing");
    require(file_exists(fixture, L"spare", L"sneaked.md"), "the note made outside is kept");
    struct note_ref listed = {0};
    require(find_note(fixture, category_at(fixture, "spare"), "sneaked", &listed),
            "and it is listed");
}

/* #1069 stage 6: a note rename keeps the order, star and note written outside (the rename's
 * record holds the finished ledger, made after the ledger is read again). */
static void verify_note_rename(struct fixture *_Nonnull fixture)
{
    struct note_ref alpha = note_at(fixture, "inbox", "alpha");
    /* The draft left by the name-taken stage is dropped. */
    require(folio_state_select_note(fixture->state, alpha.category, alpha.note,
                                    FOLIO_UNTITLED_DISCARD) == FOLIO_STATE_READY,
            "alpha is open");
    write_file(fixture, L"inbox", L"index.json",
               "{\"version\": 2, \"notes\": [\"outside5\", \"taken\", \"outside4\", \"outside3\", "
               "\"fresh\", \"alpha\", \"outside1\"], \"favorites\": [\"alpha\"]}");
    write_file(fixture, L"inbox", L"outside7.md", "# outside 7\n");
    struct note_name *name = nullptr;
    require(note_name_create("renamed", 7, &name) == NOTE_NAME_ACCEPTED, "new name");
    enum folio_state_outcome outcome = folio_state_rename_note(fixture->state, name, u"", 0);
    note_name_destroy(name);
    require(outcome == FOLIO_STATE_READY, "alpha is renamed");
    require(file_exists(fixture, L"inbox", L"renamed.md") &&
                !file_exists(fixture, L"inbox", L"alpha.md"),
            "the file carries the new name");
    expect_disk(fixture, L"inbox",
                "outside5/taken/outside4/outside3/fresh/renamed*/outside1/outside7",
                "the rename keeps the order and star written outside and the note added outside");
}

/* A category rename keeps a category folder made outside. */
static void verify_category_rename(struct fixture *_Nonnull fixture)
{
    make_folder(fixture, L"extra");
    struct category_name *name = nullptr;
    require(category_name_create("others", 6, &name) == CATEGORY_NAME_ACCEPTED, "category name");
    struct category_rename_target target = {.category = category_at(fixture, "other"),
                                            .name = name};
    enum folio_state_outcome outcome = folio_state_rename_category(fixture->state, &target, u"", 0);
    category_name_destroy(name);
    require(outcome == FOLIO_STATE_READY, "other is renamed");
    require(file_exists(fixture, L"others", L"delta.md") &&
                !file_exists(fixture, L"other", L"delta.md"),
            "the folder carries the new name");
    expect_categories(fixture,
                      "later+#112233/inbox+#102030/others+#D5803A/spare+#7F8FA6/extra+#7F8FA6",
                      "the rename keeps the category made outside");
    require(folio_state_category_count(fixture->state) == 5, "and it is listed");
}

int main(void)
{
    struct fixture fixture = {0};
    open_state(&fixture);
    puts("ledger refresh: guarded save");
    verify_guarded_save(&fixture);
    verify_guarded_save_files(&fixture);
    puts("ledger refresh: star");
    verify_star(&fixture);
    puts("ledger refresh: reorder");
    verify_reorder(&fixture);
    puts("ledger refresh: first save");
    verify_first_save(&fixture);
    puts("ledger refresh: name taken");
    verify_name_taken(&fixture);
    puts("ledger refresh: trash");
    verify_trash(&fixture);
    puts("ledger refresh: transfer");
    verify_transfer(&fixture);
    puts("ledger refresh: refusal");
    verify_refusal(&fixture);
    puts("ledger refresh: category added");
    verify_category_added(&fixture);
    puts("ledger refresh: category rewrite");
    verify_category_rewrite(&fixture);
    puts("ledger refresh: category remove refused");
    verify_category_remove_refused(&fixture);
    puts("ledger refresh: note rename");
    verify_note_rename(&fixture);
    puts("ledger refresh: category rename");
    verify_category_rename(&fixture);
    close_state(&fixture);
    puts("ledger refresh: passed");
    return EXIT_SUCCESS;
}
