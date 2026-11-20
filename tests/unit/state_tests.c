#include "allocation_probe.h"
#include "appearance_port.h"
#include "category_ledger.h"
#include "category_name.h"
#include "discovery_target.h"
#include "drawer_layout.h"
#include "folio_settings.h"
#include "folio_state.h"
#include "markdown_rtf.h"
#include "name_list.h"
#include "note_excerpt.h"
#include "note_file_times.h"
#include "note_identity.h"
#include "note_ledger.h"
#include "note_name.h"
#include "note_store_request.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "open_place_kind.h"
#include "open_request.h"
#include "persisted_size_limit.h"
#include "persistence_port.h"
#include "regex_port.h"
#include "rename_plan.h"
#include "replace_edit.h"
#include "rtf_palette.h"
#include "tag_catalog.h"
#include "tag_update.h"
#include "ui_font.h"
#include "unit_tests.h"
#include "web_link_port.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* テスト用のポート実装。application が不完全型として知る persistence_adapter をここで定義する。 */
/* One category's index.json as the fake holds it once it was written (#1069). */
struct stored_index
{
    char category[64];
    char text[512];
};

/* One category's folder as the fake holds it once a note was made, binned, moved or renamed
 * there: the names its scan returns from then on (#1069, stage 4). */
struct stored_folder
{
    char category[64];
    char names[16][48];
    size_t count;
    const char *_Nonnull const *_Nullable seed; /* the fixed list it was first filled from */
    size_t seed_count;
};

struct persistence_adapter
{
    struct stored_index stored[8];
    size_t stored_count;
    struct stored_folder folders[8];
    size_t folder_count;
    /* #1069 stage 5: categories.json as last written (read back from then on) and the data
     * folder's category folders once the product changed them, as on a real disk. */
    char categories_store[1024];
    bool categories_stored;
    char category_folders[8][64];
    size_t category_folder_count;
    bool category_folders_owned;
    bool times_ready;
    const char *_Nullable updated_time;
    size_t times_reads;
    enum persistence_outcome categories_outcome;
    const char *_Nonnull categories_text;
    enum persistence_outcome scan_outcome;
    const char *_Nonnull const *_Nullable scanned_categories;
    size_t scanned_category_count;
    enum persistence_outcome notes_outcome;
    const char *_Nonnull notes_text;
    enum persistence_outcome notes_scan_outcome;
    const char *_Nonnull const *_Nullable scanned_notes;
    size_t scanned_note_count;
    size_t note_scans; /* scan_notes が呼ばれた回数 */
    enum persistence_outcome note_outcome;
    const char *_Nonnull note_body;
    const char *_Nullable last_note; /* 最後に read_note で求められたノート名 */
    size_t note_reads;               /* read_note が呼ばれた回数（遅延読み込みの証拠） */
    size_t old_category_reads;
    char read_category[64];
    const char *_Nullable rare_note; /* この名前のノートだけ別の本文を返す */
    const char *_Nullable rare_body;
    const char *_Nullable missing_note; /* この名前のノートだけ読めない（写しを持たない） */
    enum persistence_outcome write_outcome;
    size_t writes;                      /* write_category_ledger が呼ばれた回数 */
    size_t written_count;               /* 最後に書かれた台帳のカテゴリ数 */
    bool written_expanded[8];           /* 最後に書かれた台帳の展開状態 */
    struct rgb_color written_colors[8]; /* 最後に書かれた台帳の色 */
    enum persistence_outcome archive_outcome;
    size_t archives;                         /* archive_note が呼ばれた回数 */
    const char *_Nullable archived_category; /* 最後に履歴を求められたカテゴリ名 */
    const char *_Nullable archived_note;     /* 最後に履歴を求められたノート名 */
    enum persistence_outcome note_write_outcome;
    size_t note_writes;  /* write_note が呼ばれた回数 */
    size_t copy_fail_at; /* md公開後のcache確保だけを注入で失敗させる。0なら未使用。 */
    enum persistence_outcome create_outcome;
    size_t creates;
    char created_note[256];
    enum persistence_outcome category_create_outcome; /* create_category が返す結果 */
    size_t category_creates;                          /* create_category が呼ばれた回数 */
    enum category_remove_outcome category_remove_outcome;
    size_t category_removes;
    char removed_category[64];
    enum trash_outcome trash_outcome;
    size_t trashes;
    char trashed_category[64];
    char trashed_note[256];
    char created_category[64]; /* 最後に作るよう求められたカテゴリ名（複製） */
    char written_body[256];    /* 最後に書かれた本文（終端付き） */
    bool remember_note_writes; /* Batch retry reads actual successful publications. */
    size_t saved_count;
    char saved_categories[16][64];
    char saved_notes[16][64];
    char saved_bodies[16][256];
    const char *_Nullable written_note; /* 最後に書かれたノート名 */
    const char *_Nullable written_category;
    char written_names[64]; /* 最後に書かれたカテゴリ台帳の名前（'/' 区切り） */
    enum persistence_outcome ledger_write_outcome;
    size_t written_favorite_count;
    size_t move_favorite_count;
    size_t ledger_writes;                  /* write_note_ledger が呼ばれた回数 */
    const char *_Nullable ledger_category; /* 最後に索引を書かれたカテゴリ名 */
    char ledger_order[64];                 /* 最後に書かれた索引の名前（'/' 区切り） */
    enum persistence_outcome move_outcome;
    size_t moves;                    /* move_note が呼ばれた回数 */
    const char *_Nullable move_from; /* 最後の移動元のカテゴリ名 */
    /* 最後に移されたノート名。名前は移動元の台帳が持っており、意図の中で捨てられるので複製する。 */
    char moved_note[64];
    const char *_Nullable move_to; /* 最後の移動先のカテゴリ名 */
    char calls[256];               /* 呼び出しの順（'/' 区切り。move・<カテゴリ>・書いた名前） */
    enum rename_outcome rename_outcome;  /* apply_rename が返す結果 */
    enum rename_outcome recover_outcome; /* 起動時の recover_rename が返す結果 */
    size_t renames;                      /* apply_rename が呼ばれた回数 */
    size_t recovers;                     /* recover_rename が呼ばれた回数 */
    /* 最後の意図の名前。意図は呼び出しの間だけ借りるので複製して持つ。 */
    char renamed_category[64];
    char renamed_from[64];
    char renamed_to[64];
    bool journal_published;           /* 記録を公開したまま終わっているか（:q! で残る意図の証拠） */
    enum rename_attempt last_attempt; /* 最後の apply_rename が START か RESUME か */
    size_t ledger_fail_at; /* この番号（1 始まり）の索引の書き戻しだけ失敗させる。0 なら使わない */
    /* data/settings.json（ADR 0025）。既定は「ファイルが無い」 */
    enum persistence_outcome settings_outcome;
    const char *_Nonnull settings_text;
    size_t settings_reads;
    enum persistence_outcome settings_write_outcome;
    size_t settings_writes; /* write_settings が呼ばれた回数 */
    bool written_minimap;
    bool written_motion;
    bool written_number;                   /* 最後に書かれた設定の number */
    enum folio_theme_choice written_theme; /* 最後に書かれた設定の theme */
    enum folio_language written_language;  /* 最後に書かれた設定の language */
    const char *_Nullable alt_category;    /* この名前のカテゴリだけ別の索引と md を返す */
    const char *_Nonnull alt_notes_text;
    const char *_Nonnull const *_Nullable alt_scanned; /* nullptr で終わる名前の並び */
    /* data/.history の版（ADR 0038）。添字が版番号で、本文が nullptr の版は無い（ABSENT）。
     * history_outcomes が LOADED 以外の版は本文を見ずにその結果を返す */
    const char *_Nullable history[8];
    enum persistence_outcome history_outcomes[8];
    size_t history_reads;                   /* read_history が呼ばれた回数 */
    const char *_Nullable history_category; /* 最後に版を求められたカテゴリ名 */
    const char *_Nullable history_note;     /* 最後に版を求められたノート名 */
};

/* 名前を '/' でつないで out（終端付き）へ書き足す。 */
static void append_name(char *_Nonnull out, size_t capacity, size_t *_Nonnull position,
                        const char *_Nonnull name)
{
    size_t length = strlen(name);
    require(*position + length + 2 < capacity, "written names fit the fake");
    if (*position > 0)
    {
        out[*position] = '/';
        *position += 1;
    }
    memcpy(out + *position, name, length);
    *position += length;
    out[*position] = '\0';
}

/* 呼び出しの順を 1 つ記録する（ADR 0008 の決定 3 の順序の検証に使う）。 */
static void record_call(struct persistence_adapter *_Nonnull adapter, const char *_Nonnull name)
{
    size_t position = strlen(adapter->calls);
    append_name(adapter->calls, sizeof adapter->calls, &position, name);
}

static enum persistence_outcome list_names(const char *_Nonnull const *_Nullable items,
                                           size_t count, struct name_list *_Nullable *_Nonnull out)
{
    struct name_list *list = nullptr;
    if (name_list_create(&list) != NAME_LIST_ACCEPTED)
    {
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < count; ++index)
    {
        enum name_list_outcome outcome = name_list_append(list, items[index], strlen(items[index]));
        if (outcome != NAME_LIST_ACCEPTED)
        {
            name_list_destroy(list);
            return outcome == NAME_LIST_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY
                                                      : PERSISTENCE_MALFORMED;
        }
    }
    *out = list;
    return PERSISTENCE_LOADED;
}

static enum persistence_outcome fake_scan_categories(struct persistence_adapter *_Nonnull adapter,
                                                     struct name_list *_Nullable *_Nonnull out)
{
    if (adapter->scan_outcome != PERSISTENCE_LOADED)
    {
        return adapter->scan_outcome;
    }
    if (!adapter->category_folders_owned)
        return list_names(adapter->scanned_categories, adapter->scanned_category_count, out);
    const char *names[8] = {nullptr};
    for (size_t at = 0; at < adapter->category_folder_count; ++at)
        names[at] = adapter->category_folders[at];
    return list_names(names, adapter->category_folder_count, out);
}

static void copy_name(char *_Nonnull out, size_t capacity, const char *_Nonnull name);

/* From the product's first change to the category folders, the fake holds them (#1069). */
static void own_category_folders(struct persistence_adapter *_Nonnull adapter)
{
    if (adapter->category_folders_owned)
        return;
    require(adapter->scanned_category_count <= 8, "category folders fit the fake");
    for (size_t at = 0; at < adapter->scanned_category_count; ++at)
        copy_name(adapter->category_folders[at], sizeof adapter->category_folders[at],
                  adapter->scanned_categories[at]);
    adapter->category_folder_count = adapter->scanned_category_count;
    adapter->category_folders_owned = true;
}

static void category_folder_add(struct persistence_adapter *_Nonnull adapter,
                                const char *_Nonnull category)
{
    own_category_folders(adapter);
    for (size_t at = 0; at < adapter->category_folder_count; ++at)
        if (strcmp(adapter->category_folders[at], category) == 0)
            return;
    require(adapter->category_folder_count < 8, "a new category folder fits the fake");
    copy_name(adapter->category_folders[adapter->category_folder_count],
              sizeof adapter->category_folders[0], category);
    adapter->category_folder_count += 1;
}

static void category_folder_remove(struct persistence_adapter *_Nonnull adapter,
                                   const char *_Nonnull category)
{
    own_category_folders(adapter);
    for (size_t at = 0; at < adapter->category_folder_count; ++at)
    {
        if (strcmp(adapter->category_folders[at], category) != 0)
            continue;
        for (size_t next = at + 1; next < adapter->category_folder_count; ++next)
            memcpy(adapter->category_folders[next - 1], adapter->category_folders[next],
                   sizeof adapter->category_folders[0]);
        adapter->category_folder_count -= 1;
        return;
    }
}

/* 別の索引を持たせたカテゴリか（別カテゴリへの移動を試すために要る）。 */
static bool is_alt(const struct persistence_adapter *_Nonnull adapter,
                   const char *_Nonnull category)
{
    return adapter->alt_category != nullptr && strcmp(adapter->alt_category, category) == 0;
}

/* #1069 / ADR0117 (stage 4): a ledger is read again with its folder before it is written, so
 * the fake's folders must hold what the product did to them, as real folders do. A category's
 * folder starts as the fixed scan list and is the fake's own from its first change. */
static struct stored_folder *_Nullable folder_slot(struct persistence_adapter *_Nonnull adapter,
                                                   const char *_Nonnull category)
{
    for (size_t at = 0; at < adapter->folder_count; ++at)
        if (strcmp(adapter->folders[at].category, category) == 0)
            return &adapter->folders[at];
    return nullptr;
}

static void folder_put(struct stored_folder *_Nonnull folder, const char *_Nonnull name)
{
    for (size_t at = 0; at < folder->count; ++at)
        if (strcmp(folder->names[at], name) == 0)
            return;
    require(folder->count < sizeof folder->names / sizeof folder->names[0] &&
                strlen(name) < sizeof folder->names[0],
            "the fake's folder holds the name");
    memcpy(folder->names[folder->count++], name, strlen(name) + 1);
}

static struct stored_folder *_Nonnull folder_of(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category)
{
    struct stored_folder *slot = folder_slot(adapter, category);
    if (slot != nullptr)
        return slot;
    require(adapter->folder_count < sizeof adapter->folders / sizeof adapter->folders[0] &&
                strlen(category) < sizeof slot->category,
            "the fake holds this category's folder");
    slot = &adapter->folders[adapter->folder_count++];
    memcpy(slot->category, category, strlen(category) + 1);
    slot->count = 0;
    bool alt = is_alt(adapter, category);
    const char *_Nonnull const *_Nullable names =
        alt ? adapter->alt_scanned : adapter->scanned_notes;
    slot->seed = names;
    slot->seed_count = adapter->scanned_note_count;
    for (size_t at = 0;
         names != nullptr && (alt ? names[at] != nullptr : at < adapter->scanned_note_count); ++at)
        folder_put(slot, names[at]);
    return slot;
}

static bool folder_holds(const struct stored_folder *_Nonnull folder, const char *_Nonnull name)
{
    for (size_t at = 0; at < folder->count; ++at)
        if (strcmp(folder->names[at], name) == 0)
            return true;
    return false;
}

/* A note is in the folder from now on: made by the product, or put there from outside by a test. */
static void folder_add(struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category,
                       const char *_Nonnull name)
{
    folder_put(folder_of(adapter, category), name);
}

static void folder_remove(struct persistence_adapter *_Nonnull adapter,
                          const char *_Nonnull category, const char *_Nonnull name)
{
    struct stored_folder *folder = folder_of(adapter, category);
    for (size_t at = 0; at < folder->count; ++at)
    {
        if (strcmp(folder->names[at], name) != 0)
            continue;
        for (size_t next = at + 1; next < folder->count; ++next)
            memcpy(folder->names[next - 1], folder->names[next], sizeof folder->names[0]);
        folder->count -= 1;
        return;
    }
}

static enum persistence_outcome list_folder(const struct stored_folder *_Nonnull folder,
                                            struct name_list *_Nullable *_Nonnull out)
{
    const char *names[sizeof folder->names / sizeof folder->names[0]];
    for (size_t at = 0; at < folder->count; ++at)
        names[at] = folder->names[at];
    return list_names(names, folder->count, out);
}

static enum persistence_outcome fake_scan_notes(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category,
                                                struct name_list *_Nullable *_Nonnull out)
{
    adapter->note_scans += 1;
    if (adapter->notes_scan_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_scan_outcome;
    }
    const struct stored_folder *folder = folder_slot(adapter, category);
    if (folder != nullptr)
    {
        /* Once the fake holds a folder, the fixed lists no longer speak for it. A test that
         * swaps them afterwards would be ignored without a word: it must use folder_add and
         * folder_remove (review of #1069, stage 4). */
        bool alt = is_alt(adapter, category);
        require(folder->seed == nullptr ||
                    (alt ? folder->seed == adapter->alt_scanned
                         : folder->seed == adapter->scanned_notes &&
                               folder->seed_count == adapter->scanned_note_count),
                "a folder the fake holds is changed with folder_add / folder_remove");
        return list_folder(folder, out);
    }
    if (!is_alt(adapter, category))
    {
        return list_names(adapter->scanned_notes, adapter->scanned_note_count, out);
    }
    const char *_Nonnull const *_Nullable names = adapter->alt_scanned;
    size_t count = 0;
    while (names != nullptr && names[count] != nullptr)
    {
        count += 1;
    }
    return list_names(names, count, out);
}

static enum persistence_outcome
fake_read_category_ledger(struct persistence_adapter *_Nonnull adapter,
                          struct category_ledger *_Nullable *_Nonnull out)
{
    if (adapter->categories_outcome != PERSISTENCE_LOADED)
    {
        return adapter->categories_outcome;
    }
    const char *_Nonnull text =
        adapter->categories_stored ? adapter->categories_store : adapter->categories_text;
    enum category_ledger_outcome parsed = category_ledger_parse(text, strlen(text), out);
    if (parsed == CATEGORY_LEDGER_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return parsed == CATEGORY_LEDGER_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY
                                                   : PERSISTENCE_MALFORMED;
}

/* #1069 / ADR0117: the fake keeps what was written. A ledger is read again before every write,
 * so a write must be what the next read returns, as with real files. The text is put together
 * by hand: no allocation of the fake's own, so the allocation sweeps count the product's only. */
static struct stored_index *_Nullable stored_slot(struct persistence_adapter *_Nonnull adapter,
                                                  const char *_Nonnull category)
{
    for (size_t at = 0; at < adapter->stored_count; ++at)
        if (strcmp(adapter->stored[at].category, category) == 0)
            return &adapter->stored[at];
    return nullptr;
}

/* What index.json of `category` holds from now on: a write of the product's, or an edit from
 * outside the app that a test makes. */
static void store_index(struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category,
                        const char *_Nonnull text)
{
    struct stored_index *slot = stored_slot(adapter, category);
    if (slot == nullptr)
    {
        require(adapter->stored_count < sizeof adapter->stored / sizeof adapter->stored[0] &&
                    strlen(category) < sizeof slot->category,
                "the fake holds this category's index");
        slot = &adapter->stored[adapter->stored_count++];
        memcpy(slot->category, category, strlen(category) + 1);
    }
    require(strlen(text) < sizeof slot->text, "the index text fits the fake");
    memcpy(slot->text, text, strlen(text) + 1);
}

static void append_text(char *_Nonnull out, size_t capacity, const char *_Nonnull text)
{
    size_t used = strlen(out);
    require(used + strlen(text) < capacity, "the index text fits the fake");
    memcpy(out + used, text, strlen(text) + 1);
}

/* `"a", "b"`: every name, or only the starred ones. */
static void append_names(char *_Nonnull out, size_t capacity,
                         const struct note_ledger *_Nonnull ledger, bool starred)
{
    bool first = true;
    for (size_t index = 0; index < note_ledger_count(ledger); ++index)
    {
        if (starred && !note_ledger_favorite(ledger, index))
            continue;
        append_text(out, capacity, first ? "\"" : ", \"");
        append_text(out, capacity, note_ledger_name(ledger, index));
        append_text(out, capacity, "\"");
        first = false;
    }
}

static void store_ledger(struct persistence_adapter *_Nonnull adapter,
                         const char *_Nonnull category, const struct note_ledger *_Nonnull ledger)
{
    char text[sizeof adapter->stored[0].text] = "{\"version\": 2, \"notes\": [";
    append_names(text, sizeof text, ledger, false);
    append_text(text, sizeof text, "], \"favorites\": [");
    append_names(text, sizeof text, ledger, true);
    append_text(text, sizeof text, "]}");
    store_index(adapter, category, text);
}

static enum persistence_outcome fake_read_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                      const char *_Nonnull category,
                                                      struct note_ledger *_Nullable *_Nonnull out)
{
    if (adapter->notes_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_outcome;
    }
    const struct stored_index *slot = stored_slot(adapter, category);
    if (slot != nullptr && slot->text[0] == '\0')
    {
        return PERSISTENCE_ABSENT;
    }
    const char *_Nonnull fixed =
        is_alt(adapter, category) ? adapter->alt_notes_text : adapter->notes_text;
    const char *_Nonnull text = slot != nullptr ? slot->text : fixed;
    enum note_ledger_outcome parsed = note_ledger_parse(text, strlen(text), out);
    if (parsed == NOTE_LEDGER_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return parsed == NOTE_LEDGER_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY : PERSISTENCE_MALFORMED;
}

/* categories.json as the real adapter writes it: what the next read returns (#1069 stage 5). */
static void store_categories(struct persistence_adapter *_Nonnull adapter,
                             const struct category_ledger *_Nonnull ledger)
{
    char text[sizeof adapter->categories_store] = "{\"version\": 1, \"categories\": [";
    for (size_t index = 0; index < category_ledger_count(ledger); ++index)
    {
        char hex[rgb_color_hex_length + 1] = {0};
        rgb_color_write_hex(category_ledger_color(ledger, index), hex);
        append_text(text, sizeof text, index == 0 ? "{\"name\": \"" : ", {\"name\": \"");
        append_text(text, sizeof text, category_ledger_name(ledger, index));
        append_text(text, sizeof text, "\", \"color\": \"");
        append_text(text, sizeof text, hex);
        append_text(text, sizeof text,
                    category_ledger_expanded(ledger, index) ? "\", \"expanded\": true}"
                                                            : "\", \"expanded\": false}");
    }
    append_text(text, sizeof text, "]}");
    memcpy(adapter->categories_store, text, strlen(text) + 1);
    adapter->categories_stored = true;
}

static enum persistence_outcome
fake_write_category_ledger(struct persistence_adapter *_Nonnull adapter,
                           const struct category_ledger *_Nonnull ledger)
{
    adapter->writes += 1;
    if (adapter->write_outcome != PERSISTENCE_STORED)
    {
        return adapter->write_outcome;
    }
    store_categories(adapter, ledger);
    adapter->written_count = category_ledger_count(ledger);
    size_t position = 0;
    adapter->written_names[0] = '\0';
    for (size_t index = 0; index < adapter->written_count && index < 8; ++index)
    {
        adapter->written_expanded[index] = category_ledger_expanded(ledger, index);
        adapter->written_colors[index] = category_ledger_color(ledger, index);
        append_name(adapter->written_names, sizeof adapter->written_names, &position,
                    category_ledger_name(ledger, index));
    }
    return PERSISTENCE_STORED;
}

static const char *_Nonnull fake_note_body(const struct persistence_adapter *_Nonnull adapter,
                                           const char *_Nonnull category, const char *_Nonnull note)
{
    for (size_t at = 0; at < adapter->saved_count; ++at)
        if (strcmp(adapter->saved_categories[at], category) == 0 &&
            strcmp(adapter->saved_notes[at], note) == 0)
            return adapter->saved_bodies[at];
    bool rare = adapter->rare_note != nullptr && adapter->rare_body != nullptr &&
                strcmp(adapter->rare_note, note) == 0;
    return rare ? adapter->rare_body : adapter->note_body;
}

static enum persistence_outcome fake_read_note(struct persistence_adapter *_Nonnull adapter,
                                               const char *_Nonnull category,
                                               const char *_Nonnull note,
                                               struct note_text *_Nullable *_Nonnull out)
{
    adapter->old_category_reads += strcmp(category, "B") == 0 ? 1 : 0;
    require(strlen(category) < sizeof adapter->read_category, "read category fits");
    memcpy(adapter->read_category, category, strlen(category) + 1);
    adapter->last_note = note;
    adapter->note_reads += 1;
    if (adapter->missing_note != nullptr && strcmp(adapter->missing_note, note) == 0)
    {
        return PERSISTENCE_ABSENT;
    }
    if (adapter->note_outcome != PERSISTENCE_LOADED)
    {
        return adapter->note_outcome;
    }
    const char *_Nonnull body = fake_note_body(adapter, category, note);
    enum note_text_outcome accepted = note_text_create(body, strlen(body), out);
    if (accepted == NOTE_TEXT_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return accepted == NOTE_TEXT_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY : PERSISTENCE_MALFORMED;
}

/* 履歴への写し。呼び出しの順を記録する（archive が write より先である証拠・ADR 0012 の決定 2）。
 */
static enum persistence_outcome fake_archive_note(struct persistence_adapter *_Nonnull adapter,
                                                  const char *_Nonnull category,
                                                  const char *_Nonnull note)
{
    adapter->archives += 1;
    adapter->archived_category = category;
    adapter->archived_note = note;
    record_call(adapter, "archive");
    return adapter->archive_outcome;
}

/* 履歴の 1 版（ADR 0038 の決定 2）。表にある版だけを返し、範囲の外は MALFORMED。 */
static enum persistence_outcome fake_read_history(struct persistence_adapter *_Nonnull adapter,
                                                  const struct history_version *_Nonnull which,
                                                  struct note_text *_Nullable *_Nonnull out)
{
    adapter->history_reads += 1;
    adapter->history_category = which->category;
    adapter->history_note = which->note;
    require(which->version >= 1 && which->version <= 5, "versions stay within the depth");
    if (adapter->history_outcomes[which->version] != PERSISTENCE_LOADED)
    {
        return adapter->history_outcomes[which->version];
    }
    const char *_Nullable body = adapter->history[which->version];
    if (body == nullptr)
    {
        return PERSISTENCE_ABSENT;
    }
    enum note_text_outcome accepted = note_text_create(body, strlen(body), out);
    if (accepted == NOTE_TEXT_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return accepted == NOTE_TEXT_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY : PERSISTENCE_MALFORMED;
}

static void remember_published_note(struct persistence_adapter *_Nonnull adapter,
                                    const char *_Nonnull category, const char *_Nonnull note,
                                    const struct note_text *_Nonnull body)
{
    if (!adapter->remember_note_writes)
        return;
    size_t at = 0;
    while (at < adapter->saved_count && (strcmp(adapter->saved_categories[at], category) != 0 ||
                                         strcmp(adapter->saved_notes[at], note) != 0))
        ++at;
    require(at < 16 && strlen(category) < 64 && strlen(note) < 64 && note_text_length(body) < 256,
            "published fake row fits");
    memcpy(adapter->saved_categories[at], category, strlen(category) + 1);
    memcpy(adapter->saved_notes[at], note, strlen(note) + 1);
    memcpy(adapter->saved_bodies[at], note_text_bytes(body), note_text_length(body) + 1);
    if (at == adapter->saved_count)
        adapter->saved_count += 1;
}

static enum persistence_outcome fake_write_note(struct persistence_adapter *_Nonnull adapter,
                                                const char *_Nonnull category,
                                                const char *_Nonnull note,
                                                const struct note_text *_Nonnull body)
{
    adapter->note_writes += 1;
    record_call(adapter, "write");
    adapter->written_category = category;
    adapter->written_note = note;
    if (adapter->note_write_outcome != PERSISTENCE_STORED)
    {
        return adapter->note_write_outcome;
    }
    size_t length = note_text_length(body);
    require(length + 1 < sizeof adapter->written_body, "written body fits the fake");
    memcpy(adapter->written_body, note_text_bytes(body), length + 1);
    remember_published_note(adapter, category, note, body);
    if (adapter->copy_fail_at > 0)
    {
        allocation_probe_fail_at(adapter->copy_fail_at);
    }
    return PERSISTENCE_STORED;
}

static enum note_store_outcome fake_publish_note(struct persistence_adapter *_Nonnull adapter,
                                                 const struct note_store_request *_Nonnull request)
{
    enum persistence_outcome archived =
        fake_archive_note(adapter, request->category, request->note);
    if (archived != PERSISTENCE_STORED && archived != PERSISTENCE_ABSENT)
        return archived == PERSISTENCE_OUT_OF_MEMORY ? NOTE_STORE_OUT_OF_MEMORY
                                                     : NOTE_STORE_HISTORY_FAILED;
    enum persistence_outcome written =
        fake_write_note(adapter, request->category, request->note, request->body);
    if (written == PERSISTENCE_STORED)
        return NOTE_STORE_STORED;
    return written == PERSISTENCE_OUT_OF_MEMORY ? NOTE_STORE_OUT_OF_MEMORY
                                                : NOTE_STORE_WRITE_FAILED;
}

static enum note_store_outcome fake_store_note(struct persistence_adapter *_Nonnull adapter,
                                               const struct note_store_request *_Nonnull request)
{
    if (request->expected == nullptr)
        return fake_publish_note(adapter, request);
    struct note_text *_Nullable current = nullptr;
    enum persistence_outcome read =
        fake_read_note(adapter, request->category, request->note, &current);
    if (read == PERSISTENCE_ABSENT)
        return NOTE_STORE_SOURCE_CHANGED;
    if (read != PERSISTENCE_LOADED)
        return read == PERSISTENCE_OUT_OF_MEMORY ? NOTE_STORE_OUT_OF_MEMORY
                                                 : NOTE_STORE_SOURCE_UNREADABLE;
    bool same = note_text_equals(current, request->expected);
    note_text_destroy(current);
    if (!same)
        return NOTE_STORE_SOURCE_CHANGED;
    if (note_text_equals(request->expected, request->body))
        return NOTE_STORE_UNCHANGED;
    return fake_publish_note(adapter, request);
}

static enum trash_outcome fake_trash_note(struct persistence_adapter *_Nonnull adapter,
                                          const char *_Nonnull category, const char *_Nonnull note)
{
    adapter->trashes += 1;
    record_call(adapter, "trash_note");
    require(strlen(category) < sizeof adapter->trashed_category, "trashed category fits");
    require(strlen(note) < sizeof adapter->trashed_note, "trashed note fits");
    memcpy(adapter->trashed_category, category, strlen(category) + 1);
    memcpy(adapter->trashed_note, note, strlen(note) + 1);
    if (adapter->trash_outcome == TRASH_TRASHED || adapter->trash_outcome == TRASH_ABSENT)
        folder_remove(adapter, category, note);
    return adapter->trash_outcome;
}

static enum category_remove_outcome
fake_remove_category(struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category)
{
    adapter->category_removes += 1;
    record_call(adapter, "remove_category");
    require(strlen(category) < sizeof adapter->removed_category, "removed category fits");
    memcpy(adapter->removed_category, category, strlen(category) + 1);
    if (adapter->category_remove_outcome == CATEGORY_REMOVE_REMOVED ||
        adapter->category_remove_outcome == CATEGORY_REMOVE_ABSENT)
        category_folder_remove(adapter, category);
    return adapter->category_remove_outcome;
}

static enum persistence_outcome fake_write_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                       const char *_Nonnull category,
                                                       const struct note_ledger *_Nonnull ledger);

static enum persistence_outcome fake_create_note(struct persistence_adapter *_Nonnull adapter,
                                                 const char *_Nonnull category,
                                                 const char *_Nonnull note,
                                                 const struct note_text *_Nonnull body)
{
    adapter->creates += 1;
    record_call(adapter, "create");
    adapter->written_category = category;
    require(strlen(note) < sizeof adapter->created_note, "created name fits");
    memcpy(adapter->created_note, note, strlen(note) + 1);
    if (adapter->create_outcome != PERSISTENCE_STORED)
    {
        return adapter->create_outcome;
    }
    if (folder_holds(folder_of(adapter, category), note))
    {
        return PERSISTENCE_NAME_TAKEN; /* as the real adapter: an existing file is not replaced */
    }
    require(note_text_length(body) < sizeof adapter->written_body, "created body fits");
    memcpy(adapter->written_body, note_text_bytes(body), note_text_length(body) + 1);
    folder_add(adapter, category, note);
    if (adapter->copy_fail_at > 0)
    {
        allocation_probe_fail_at(adapter->copy_fail_at);
    }
    return PERSISTENCE_STORED;
}

static enum persistence_outcome fake_create_category(struct persistence_adapter *_Nonnull adapter,
                                                     const char *_Nonnull category)
{
    adapter->category_creates += 1;
    record_call(adapter, "create_category");
    require(strlen(category) < sizeof adapter->created_category, "created category fits");
    memcpy(adapter->created_category, category, strlen(category) + 1);
    if (adapter->category_create_outcome == PERSISTENCE_STORED)
    {
        /* A folder just made is empty and has no index, whatever the fixed lists say (#1069). */
        category_folder_add(adapter, category);
        struct stored_folder *made = folder_of(adapter, category);
        made->count = 0;
        made->seed = nullptr;
        store_index(adapter, category, ""); /* no index.json yet: reads as ABSENT */
    }
    return adapter->category_create_outcome;
}

static enum persistence_outcome fake_write_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                       const char *_Nonnull category,
                                                       const struct note_ledger *_Nonnull ledger)
{
    adapter->ledger_writes += 1;
    adapter->ledger_category = category;
    record_call(adapter, category);
    if (adapter->ledger_fail_at == adapter->ledger_writes)
    {
        return PERSISTENCE_UNWRITABLE;
    }
    if (adapter->ledger_write_outcome != PERSISTENCE_STORED)
    {
        return adapter->ledger_write_outcome;
    }
    adapter->written_favorite_count = note_ledger_favorite_count(ledger);
    size_t position = 0;
    adapter->ledger_order[0] = '\0';
    size_t count = note_ledger_count(ledger);
    for (size_t index = 0; index < count; ++index)
    {
        append_name(adapter->ledger_order, sizeof adapter->ledger_order, &position,
                    note_ledger_name(ledger, index));
        record_call(adapter, note_ledger_name(ledger, index));
    }
    store_ledger(adapter, category, ledger);
    return PERSISTENCE_STORED;
}

/* md の移動。指定された結果を返し、呼び出しの順を記録する（rename が先であることの証拠）。 */
static enum persistence_outcome fake_move_note(struct persistence_adapter *_Nonnull adapter,
                                               const char *_Nonnull from_category,
                                               const char *_Nonnull note,
                                               const char *_Nonnull to_category)
{
    adapter->moves += 1;
    adapter->move_favorite_count = adapter->written_favorite_count;
    adapter->move_from = from_category;
    size_t length = strlen(note);
    require(length + 1 < sizeof adapter->moved_note, "the moved name fits the fake");
    memcpy(adapter->moved_note, note, length + 1);
    adapter->move_to = to_category;
    record_call(adapter, "move");
    if (adapter->move_outcome == PERSISTENCE_STORED)
    {
        if (folder_holds(folder_of(adapter, to_category), note))
        {
            return PERSISTENCE_UNWRITABLE; /* the real move does not replace a file either */
        }
        folder_remove(adapter, from_category, note);
        folder_add(adapter, to_category, note);
    }
    return adapter->move_outcome;
}

static void copy_name(char *_Nonnull out, size_t capacity, const char *_Nonnull name)
{
    size_t length = strlen(name);
    require(length + 1 < capacity, "the renamed name fits the fake");
    memcpy(out, name, length + 1);
}

/* A completed rename is in the fake's folders and indexes as in real ones: a note's file has
 * its new name and its category's index is the plan's; a category's folder and index answer
 * to the new name (#1069, stage 4). */
static void rekey(char *_Nonnull key, size_t capacity, const struct rename_plan *_Nonnull plan)
{
    if (strcmp(key, rename_plan_from(plan)) == 0)
        copy_name(key, capacity, rename_plan_to(plan));
}

static void complete_rename(struct persistence_adapter *_Nonnull adapter,
                            const struct rename_plan *_Nonnull plan)
{
    if (rename_plan_kind(plan) == RENAME_KIND_NOTE)
    {
        folder_remove(adapter, rename_plan_category(plan), rename_plan_from(plan));
        folder_add(adapter, rename_plan_category(plan), rename_plan_to(plan));
        store_ledger(adapter, rename_plan_category(plan), rename_plan_note_ledger(plan));
        return;
    }
    for (size_t at = 0; at < adapter->folder_count; ++at)
        rekey(adapter->folders[at].category, sizeof adapter->folders[at].category, plan);
    for (size_t at = 0; at < adapter->stored_count; ++at)
        rekey(adapter->stored[at].category, sizeof adapter->stored[at].category, plan);
    own_category_folders(adapter);
    for (size_t at = 0; at < adapter->category_folder_count; ++at)
        rekey(adapter->category_folders[at], sizeof adapter->category_folders[at], plan);
    store_categories(adapter, rename_plan_category_ledger(plan));
}

/* 偽の改名。どの段階で止めるかは rename_outcome が決める（ADR 0022）。
 * 記録の公開後に止めた PENDING だけが journal_published を真にし、完了で消える。 */
static enum rename_outcome fake_apply_rename(struct persistence_adapter *_Nonnull adapter,
                                             const struct rename_plan *_Nonnull plan,
                                             enum rename_attempt attempt)
{
    adapter->renames += 1;
    adapter->last_attempt = attempt;
    record_call(adapter, "rename");
    switch (rename_plan_kind(plan))
    {
    case RENAME_KIND_NOTE:
        copy_name(adapter->renamed_category, sizeof adapter->renamed_category,
                  rename_plan_category(plan));
        break;
    case RENAME_KIND_CATEGORY:
        copy_name(adapter->renamed_category, sizeof adapter->renamed_category, "");
        break;
    }
    copy_name(adapter->renamed_from, sizeof adapter->renamed_from, rename_plan_from(plan));
    copy_name(adapter->renamed_to, sizeof adapter->renamed_to, rename_plan_to(plan));
    /* 公開後の 2 値だけが記録を残し、完了で消える。公開前の拒否は data/ を変えない。 */
    if (adapter->rename_outcome == RENAME_PENDING || adapter->rename_outcome == RENAME_HALTED)
    {
        adapter->journal_published = true;
    }
    if (adapter->rename_outcome == RENAME_COMPLETED)
    {
        adapter->journal_published = false;
        complete_rename(adapter, plan);
    }
    return adapter->rename_outcome;
}

static enum rename_outcome fake_recover_rename(struct persistence_adapter *_Nonnull adapter)
{
    adapter->recovers += 1;
    return adapter->recover_outcome;
}

/* テスト用の外観ポート。application が不完全型として知る appearance_adapter をここで定義する。 */
struct appearance_adapter
{
    enum folio_theme theme;
    enum appearance_outcome outcome;
    struct appearance_snapshot snapshot;
};

static enum appearance_outcome fake_read_appearance(struct appearance_adapter *_Nonnull adapter,
                                                    struct appearance_snapshot *_Nonnull out)
{
    if (adapter->outcome != APPEARANCE_READY)
    {
        return adapter->outcome;
    }
    *out = adapter->snapshot;
    out->theme = adapter->theme;
    return APPEARANCE_READY;
}

static struct appearance_adapter dark_adapter = {.theme = FOLIO_THEME_DARK};

static struct appearance_port looks_for(struct appearance_adapter *_Nonnull adapter)
{
    struct appearance_port port = {.adapter = adapter, .read = fake_read_appearance};
    return port;
}

/* 置換の偽ポート（ADR 0028 の決定 2）。この単位では走査を使わないので既定の台本のまま結ぶ。
 * 実体は replace_state_tests.c が持ち、run_state_tests が最初に写す。 */
static struct regex_port finder;

static const char *const scanned_categories[] = {"A", "B", "C"};
static const char *const scanned_notes[] = {"two", "one", "three"};

/* 偽のポートも実際に core の folio_settings を通す（確保点を踏ませるため）。 */
static enum persistence_outcome fake_read_settings(struct persistence_adapter *_Nonnull adapter,
                                                   struct folio_settings *_Nullable *_Nonnull out)
{
    adapter->settings_reads += 1;
    if (adapter->settings_outcome != PERSISTENCE_LOADED)
    {
        return adapter->settings_outcome;
    }
    switch (folio_settings_parse(adapter->settings_text, strlen(adapter->settings_text), out))
    {
    case FOLIO_SETTINGS_READY:
        return PERSISTENCE_LOADED;
    case FOLIO_SETTINGS_MALFORMED:
    case FOLIO_SETTINGS_UNSUPPORTED_VERSION:
        return PERSISTENCE_MALFORMED;
    case FOLIO_SETTINGS_OUT_OF_MEMORY:
        return PERSISTENCE_OUT_OF_MEMORY;
    }
    return PERSISTENCE_MALFORMED;
}

static enum persistence_outcome fake_write_settings(struct persistence_adapter *_Nonnull adapter,
                                                    const struct folio_settings *_Nonnull settings)
{
    adapter->settings_writes += 1;
    if (adapter->settings_write_outcome != PERSISTENCE_STORED)
    {
        return adapter->settings_write_outcome;
    }
    adapter->written_minimap = folio_settings_minimap(settings);
    adapter->written_motion = folio_settings_motion(settings);
    adapter->written_number = folio_settings_number(settings);
    adapter->written_theme = folio_settings_theme(settings);
    adapter->written_language = folio_settings_language(settings);
    return PERSISTENCE_STORED;
}

/* data/settings.json の既定（ファイルは無く、書けば成功する）。healthy_adapter を
 * C-012 の行数に収めるために分けてある（ADR 0025）。 */
static void healthy_settings(struct persistence_adapter *_Nonnull adapter)
{
    adapter->settings_outcome = PERSISTENCE_ABSENT;
    adapter->settings_text = "{\"version\": 1, \"number\": true}";
    adapter->settings_reads = 0;
    adapter->settings_write_outcome = PERSISTENCE_STORED;
    adapter->settings_writes = 0;
    adapter->written_number = false;
    adapter->written_theme = FOLIO_THEME_CHOICE_SYSTEM;
    adapter->written_language = FOLIO_LANGUAGE_JA;
}

static struct persistence_adapter healthy_adapter(void)
{
    struct persistence_adapter adapter = {
        .categories_outcome = PERSISTENCE_LOADED,
        .categories_text = "{\"version\": 1, \"categories\": ["
                           "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": true},"
                           "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": false}]}",
        .scan_outcome = PERSISTENCE_LOADED,
        .scanned_categories = scanned_categories,
        .scanned_category_count = 3,
        .notes_outcome = PERSISTENCE_LOADED,
        .notes_text = "{\"version\": 1, \"notes\": [\"one\", \"two\"]}",
        .notes_scan_outcome = PERSISTENCE_LOADED,
        .scanned_notes = scanned_notes,
        .scanned_note_count = 3,
        .note_outcome = PERSISTENCE_LOADED,
        .note_body = "# Hello\n\nbody",
        .last_note = nullptr,
        .write_outcome = PERSISTENCE_STORED,
        .writes = 0,
        .written_count = 0,
        .written_expanded = {false},
        .written_colors = {{0, 0, 0}},
        .archive_outcome = PERSISTENCE_STORED,
        .archives = 0,
        .archived_category = nullptr,
        .archived_note = nullptr,
        .note_write_outcome = PERSISTENCE_STORED,
        .note_writes = 0,
        .create_outcome = PERSISTENCE_STORED,
        .category_create_outcome = PERSISTENCE_STORED,
        .written_body = {'\0'},
        .written_note = nullptr,
        .written_category = nullptr,
        .written_names = {'\0'},
        .ledger_write_outcome = PERSISTENCE_STORED,
        .ledger_writes = 0,
        .ledger_category = nullptr,
        .ledger_order = {'\0'},
        .move_outcome = PERSISTENCE_STORED,
        .moves = 0,
        .move_from = nullptr,
        .moved_note = {'\0'},
        .move_to = nullptr,
        .calls = {'\0'},
        .rename_outcome = RENAME_COMPLETED,
        .recover_outcome = RENAME_NONE,
        .renames = 0,
        .recovers = 0,
        .renamed_category = {'\0'},
        .renamed_from = {'\0'},
        .renamed_to = {'\0'},
        .journal_published = false,
        .last_attempt = RENAME_START,
        .ledger_fail_at = 0,
        .alt_category = nullptr,
        .alt_notes_text = "{\"version\": 1, \"notes\": []}",
        .alt_scanned = nullptr,
    };
    healthy_settings(&adapter);
    return adapter;
}

static enum persistence_outcome
fake_read_note_times(struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category,
                     const char *_Nonnull note, struct note_file_times *_Nullable *_Nonnull out)
{
    (void)category;
    ++adapter->times_reads;
    if (!adapter->times_ready)
        return PERSISTENCE_UNREADABLE;
    const char *created = strcmp(note, "one") == 0 ? "2020-01-02 03:04" : "2021-02-03 04:05";
    const char *updated = adapter->updated_time == nullptr ? created : adapter->updated_time;
    return note_file_times_create(created, updated, out) == NOTE_FILE_TIMES_ACCEPTED
               ? PERSISTENCE_LOADED
               : PERSISTENCE_OUT_OF_MEMORY;
}

static struct persistence_port port_for(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = {
        .adapter = adapter,
        .scan_categories = fake_scan_categories,
        .scan_notes = fake_scan_notes,
        .read_category_ledger = fake_read_category_ledger,
        .write_category_ledger = fake_write_category_ledger,
        .read_note = fake_read_note,
        .read_note_times = fake_read_note_times,
        .store_note = fake_store_note,
        .read_history = fake_read_history,
        .create_note = fake_create_note,
        .create_category = fake_create_category,
        .remove_category = fake_remove_category,
        .trash_note = fake_trash_note,
        .move_note = fake_move_note,
        .apply_rename = fake_apply_rename,
        .recover_rename = fake_recover_rename,
        .read_note_ledger = fake_read_note_ledger,
        .write_note_ledger = fake_write_note_ledger,
        .read_settings = fake_read_settings,
        .write_settings = fake_write_settings,
    };
    return port;
}

static const struct drawer_metrics metrics = {.top_padding = 0,
                                              .row_height = 10,
                                              .category_height = 10,
                                              .category_gap = 0,
                                              .category_indent = 1,
                                              .note_indent = 2};

/* ノートの居場所を 1 つ作る。 */
static struct note_ref at(size_t category, size_t note)
{
    struct note_ref ref = {.category = category, .note = note};
    return ref;
}

static void verify_ready_state(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state ready");
    require(adapter.note_scans == 3, "one note scan per category");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "theme from the port");
    require(folio_state_note_count(state) == 9, "nine notes in three categories");
    require(!folio_state_pane_title(state).any, "no title before selection");
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    /* B（展開: one, two, three）・A（畳んだまま）・C（既定で展開: one, two, three） */
    require(drawer_layout_row_count(layout) == 9, "row count");
    require(same_text(drawer_layout_row(layout, 0).text, "B") &&
                same_text(drawer_layout_row(layout, 1).text, "one") &&
                same_text(drawer_layout_row(layout, 2).text, "two") &&
                same_text(drawer_layout_row(layout, 3).text, "three") &&
                same_text(drawer_layout_row(layout, 4).text, "A") &&
                same_text(drawer_layout_row(layout, 5).text, "C") &&
                same_text(drawer_layout_row(layout, 8).text, "three"),
            "row order follows ledgers then scan");
    require(drawer_layout_row(layout, 0).color.red == 0x11 &&
                drawer_layout_row(layout, 5).color.red == 0x7F,
            "row colors");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
    folio_state_destroy(nullptr);
}

static void verify_absent_data(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_ABSENT;
    adapter.scan_outcome = PERSISTENCE_ABSENT;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "absent data is empty");
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY &&
                drawer_layout_row_count(layout) == 0,
            "no rows without data");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.notes_outcome = PERSISTENCE_ABSENT;
    adapter.notes_scan_outcome = PERSISTENCE_ABSENT;
    port = port_for(&adapter);
    looks = looks_for(&dark_adapter);
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "absent notes");
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY &&
                drawer_layout_row_count(layout) == 3,
            "only category rows");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
}

static size_t row_count(const struct folio_state *_Nonnull state)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    size_t count = drawer_layout_row_count(layout);
    drawer_layout_destroy(layout);
    return count;
}

static void verify_toggle(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for toggle");
    require(row_count(state) == 9, "rows before toggle");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY, "collapse B");
    require(row_count(state) == 6, "B's notes are hidden");
    require(adapter.writes == 1 && adapter.written_count == 3 && !adapter.written_expanded[0] &&
                !adapter.written_expanded[1] && adapter.written_expanded[2],
            "toggled ledger was written");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    require(row_count(state) == 9 && adapter.writes == 2 && adapter.written_expanded[1],
            "A's notes appear and the write follows");
    require(folio_state_toggle_category(state, 3) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 2,
            "out of range is refused without writing");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_STORE_FAILED, "store failed");
    require(row_count(state) == 9, "state is unchanged when the write fails");
    adapter.write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_OUT_OF_MEMORY,
            "write out of memory");
    folio_state_destroy(state);
}

static bool has_color(struct rgb_color color, unsigned char red, unsigned char green,
                      unsigned char blue)
{
    return color.red == red && color.green == green && color.blue == blue;
}

static void verify_recolor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for recolor");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select in B");
    require(has_color(folio_state_pane_title(state).color, 0x11, 0x11, 0x11),
            "the title starts with B's color");
    struct rgb_color chosen = {.red = 0xAB, .green = 0xCD, .blue = 0xEF};
    require(folio_state_recolor_category(state, 0, chosen) == FOLIO_STATE_READY, "recolor B");
    require(adapter.writes == 1 && adapter.written_count == 3 &&
                has_color(adapter.written_colors[0], 0xAB, 0xCD, 0xEF) &&
                has_color(adapter.written_colors[1], 0x22, 0x22, 0x22),
            "only B's color was written");
    require(has_color(folio_state_pane_title(state).color, 0xAB, 0xCD, 0xEF),
            "the title follows the ledger");
    require(adapter.written_expanded[0] && !adapter.written_expanded[1],
            "expansion is kept across a recolor");
    require(folio_state_recolor_category(state, 0, chosen) == FOLIO_STATE_READY &&
                adapter.writes == 1,
            "the same color is not written");
    require(folio_state_recolor_category(state, 3, chosen) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 1,
            "out of range is refused without writing");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "enter edit");
    struct rgb_color other = {.red = 0x01, .green = 0x02, .blue = 0x03};
    require(folio_state_recolor_category(state, 1, other) == FOLIO_STATE_READY, "recolor A");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "edit mode survives a recolor");
    require(same_text(folio_state_pane_text(state), "# Hello\n\nbody"),
            "the body survives a recolor");
    require(adapter.writes == 2 && has_color(adapter.written_colors[1], 0x01, 0x02, 0x03),
            "A's color was written");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_recolor_category(state, 0, other) == FOLIO_STATE_STORE_FAILED,
            "store failed");
    require(has_color(folio_state_pane_title(state).color, 0xAB, 0xCD, 0xEF),
            "the color is unchanged when the write fails");
    adapter.write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_recolor_category(state, 0, other) == FOLIO_STATE_OUT_OF_MEMORY,
            "write out of memory");
    folio_state_destroy(state);
}

/* 50 px の窓で同じ索引を見る。B / A / C が展開なら下端は 90 で上限は 40、
 * B を折り畳めば下端は 60 で上限は 10（ADR 0009 の決定 2）。 */
static const struct drawer_metrics scroll_metrics = {.top_padding = 0,
                                                     .row_height = 10,
                                                     .category_height = 10,
                                                     .category_gap = 0,
                                                     .category_indent = 1,
                                                     .note_indent = 2,
                                                     .viewport_height = 50,
                                                     .bottom_padding = 0};

/* いまの有効なスクロール量を、最初の行の表示座標（0 か負の値）で読む。 */
static int scrolled_top(const struct folio_state *_Nonnull state)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, scroll_metrics, &layout) == FOLIO_STATE_READY,
            "layout for scroll");
    int top = drawer_layout_row(layout, 0).top;
    drawer_layout_destroy(layout);
    return top;
}

/* 丸めと、端での不変。要求量は配置より長く残る（ADR 0009 の「失う・残る」）。 */
static void verify_scroll_rounding(struct folio_state *_Nonnull state)
{
    require(scrolled_top(state) == 0, "a fresh state is at the top");
    require(folio_state_scroll_drawer(state, scroll_metrics, 15) == FOLIO_STATE_READY,
            "scrolling down is accepted");
    require(scrolled_top(state) == -15, "the drawer moved by the delta");
    require(folio_state_scroll_drawer(state, scroll_metrics, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == -15,
            "a zero delta changes nothing");
    require(folio_state_scroll_drawer(state, scroll_metrics, 100) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "past the reach it stops at the reach");
    require(folio_state_scroll_drawer(state, scroll_metrics, 10) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "at the end another step does not move");
    require(folio_state_scroll_drawer(state, scroll_metrics, -1000) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "a large negative delta lands at the top");
}

/* int 全域の差分でも、有効量から安全に両端へ飽和する。 */
static void verify_scroll_delta_bounds(struct folio_state *_Nonnull state)
{
    require(folio_state_scroll_drawer(state, scroll_metrics, 15) == FOLIO_STATE_READY &&
                scrolled_top(state) == -15,
            "an extreme delta starts at a positive effective amount");
    require(folio_state_scroll_drawer(state, scroll_metrics, INT_MAX) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "INT_MAX saturates at the reach without overflowing the sum");
    require(folio_state_scroll_drawer(state, scroll_metrics, INT_MIN) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "INT_MIN saturates at the top without negating the delta");
    require(folio_state_scroll_drawer(state, scroll_metrics, 40) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "an exact positive delta reaches the lower end");
    require(folio_state_scroll_drawer(state, scroll_metrics, -40) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "an exact negative delta reaches the upper end");
    require(folio_state_scroll_drawer(state, scroll_metrics, -1) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "at the top another negative step does not move");
    require(folio_state_scroll_drawer(state, scroll_metrics, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "a zero delta at the top leaves the amount alone");
}

/* 折り畳みで上限が縮んでも、有効量から数え直すので 1 回で必ず動く（ADR 0009 の決定 3）。 */
static void verify_scroll_recount(struct folio_state *_Nonnull state)
{
    require(folio_state_scroll_drawer(state, scroll_metrics, 40) == FOLIO_STATE_READY,
            "go to the end");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY, "collapse B");
    require(scrolled_top(state) == -10, "a smaller reach rounds the display, not the request");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "expanding again brings the request back");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                scrolled_top(state) == -10,
            "collapsed once more");
    require(folio_state_scroll_drawer(state, scroll_metrics, -10) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "one step counts from the effective amount, not the request");
}

/* 他の意図はスクロール量を触らない（見える位置へ寄せない）。 */
static void verify_scroll(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for scroll");
    verify_scroll_rounding(state);
    verify_scroll_delta_bounds(state);
    verify_scroll_recount(state);
    require(folio_state_scroll_drawer(state, scroll_metrics, 5) == FOLIO_STATE_READY,
            "scroll a little");
    require(folio_state_select_note(state, 1, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                scrolled_top(state) == -5,
            "selecting a note leaves the amount alone");
    require(folio_state_move_note(state, at(1, 0), at(1, 2)) == FOLIO_STATE_READY &&
                scrolled_top(state) == -5,
            "reordering leaves the amount alone");
    folio_state_destroy(state);
}

void run_drawer_scroll_delta_tests(void)
{
    finder = test_regex_port();
    verify_scroll();
}

/* 配置の行の名前を '/' でつないだ 1 行にする。折り畳んだカテゴリのノートは出ない。 */
static void row_order(const struct folio_state *_Nonnull state, char *_Nonnull out, size_t capacity)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    size_t position = 0;
    size_t count = drawer_layout_row_count(layout);
    out[0] = '\0';
    for (size_t index = 0; index < count; ++index)
    {
        append_name(out, capacity, &position, drawer_layout_row(layout, index).text);
    }
    drawer_layout_destroy(layout);
}

static struct folio_state *_Nonnull ready_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = port_for(adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for move");
    return state;
}

/* 索引はカテゴリと一緒に動く。B の索引を先に並べ替えてから B を末尾へ運ぶ。 */
static void verify_move_category(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"), "the order before the move");
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY,
            "reorder B's notes");
    require(folio_state_move_category(state, 0, 2) == FOLIO_STATE_READY, "B moves to the end");
    row_order(state, order, sizeof order);
    require(same_text(order, "A/C/one/two/three/B/three/one/two"),
            "the category and its own index travel together");
    require(adapter.writes == 1 && same_text(adapter.written_names, "A/C/B"),
            "the moved ledger was written once");
    require(!adapter.written_expanded[0] && adapter.written_expanded[1] &&
                adapter.written_expanded[2],
            "the expansion travels with the category");
    require(folio_state_move_category(state, 2, 0) == FOLIO_STATE_READY, "B moves back");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"), "moving up restores the order");
    require(adapter.writes == 2 && same_text(adapter.written_names, "B/A/C"), "the second write");
    require(folio_state_move_category(state, 1, 1) == FOLIO_STATE_READY && adapter.writes == 2,
            "the same position writes nothing");
    require(folio_state_move_category(state, 3, 0) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                folio_state_move_category(state, 0, 3) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 2,
            "out of range is refused without writing");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_STORE_FAILED, "store failed");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"),
            "the failed write leaves the order alone");
    adapter.write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_OUT_OF_MEMORY, "move oom");
    folio_state_destroy(state);
}

/* #1069 / ADR0117 stage 5: categories.json is read again with the data folder before it is
 * written. A test edits categories.json (`outside_categories`) or the folders
 * (`category_folder_add` / `_remove`) between intents, as a tool outside the app would. */
static struct category_name *_Nonnull accepted_category_name(const char *_Nonnull text);

static bool ledger_noticed(struct folio_state *_Nonnull state);

static void outside_categories(struct persistence_adapter *_Nonnull adapter,
                               const char *_Nonnull text)
{
    require(strlen(text) < sizeof adapter->categories_store, "outside categories fit the fake");
    memcpy(adapter->categories_store, text, strlen(text) + 1);
    adapter->categories_stored = true;
}

static void verify_category_refresh_adds(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY && !ledger_noticed(state),
            "nothing changed outside: no notice");
    category_folder_add(&adapter, "D");
    require(folio_state_ledger_outcome(state, folio_state_toggle_category(state, 1)) ==
                    FOLIO_STATE_LEDGER_REFRESHED &&
                adapter.writes == 2 && same_text(adapter.written_names, "B/A/C/D") &&
                adapter.written_expanded[1],
            "A opens, and the category made outside is in the ledger written");
    require(folio_state_category_count(state) == 4 &&
                same_text(folio_state_category_name(state, 3), "D"),
            "the category made outside is listed");
    folio_state_destroy(state);
}

/* categories.json rewritten outside: the operation finds its category again by name. */
static void verify_category_refresh_follows_names(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    outside_categories(&adapter, "{\"version\": 1, \"categories\": ["
                                 "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": false},"
                                 "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true},"
                                 "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": true}]}");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "A/C/B") && !adapter.written_expanded[2] &&
                adapter.written_expanded[1],
            "B (first in memory) is closed at its new place, the outside order stays");
    row_order(state, order, sizeof order);
    require(same_text(order, "A/C/one/two/three/B"), "the notes travel with their category");
    require(folio_state_move_category(state, 2, 0) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "B/A/C"),
            "a move on the fresh order");
    outside_categories(&adapter,
                       "{\"version\": 1, \"categories\": ["
                       "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true},"
                       "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": false},"
                       "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": false}]}");
    require(folio_state_move_category(state, 0, 2) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "B/C/A"),
            "B (first in memory) goes to C's place (last in memory), now first: both by name");
    folio_state_destroy(state);
}

/* The category the operation is about was removed outside: refused, the list is the fresh one. */
static void verify_category_refresh_refuses(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    category_folder_remove(&adapter, "C");
    require(folio_state_recolor_category(state, 2, (struct rgb_color){1, 2, 3}) ==
                    FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.writes == 0 && folio_state_category_count(state) == 2,
            "a colour for a category removed outside is refused, nothing is written");
    require(folio_state_recolor_category(state, 1, (struct rgb_color){1, 2, 3}) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 1 && same_text(adapter.written_names, "B/A"),
            "the next colour works on the fresh list");
    adapter.categories_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_DATA_UNREADABLE &&
                adapter.writes == 1,
            "categories.json that cannot be read again is not overwritten from memory");
    folio_state_destroy(state);
}

/* Opening checks the fresh ledger: a category already opened outside is not closed again. */
static void verify_category_refresh_expansion(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    outside_categories(&adapter, "{\"version\": 1, \"categories\": ["
                                 "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": true},"
                                 "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": true},"
                                 "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true}]}");
    require(folio_state_set_expansion(state, 1, true) == FOLIO_STATE_READY && adapter.writes == 0 &&
                row_count(state) == 12,
            "A was opened outside: nothing is written and it stays open");
    outside_categories(&adapter, "{\"version\": 1, \"categories\": ["
                                 "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": false},"
                                 "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": true},"
                                 "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true}]}");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY && adapter.writes == 0 &&
                row_count(state) == 9,
            "a click to close B, already closed outside, writes nothing and leaves it closed");
    outside_categories(&adapter, "{\"version\": 1, \"categories\": ["
                                 "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": true},"
                                 "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": false},"
                                 "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true}]}");
    require(folio_state_set_category_expanded(state, 0, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.writes == 0 && same_text(folio_state_category_name(state, 0), "A"),
            "the keyboard's opening is refused when the list was read again: the caller's save "
            "decision and places are of the old list");
    require(folio_state_set_category_expanded(state, 1, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 1 && adapter.written_expanded[1],
            "pressed again on the fresh list, B opens");
    folio_state_destroy(state);
}

/* Making and removing categories against the folders as they are now. */
static void verify_category_refresh_create_remove(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    category_folder_add(&adapter, "D");
    struct category_name *taken = accepted_category_name("D");
    require(folio_state_create_category(state, taken) == FOLIO_STATE_CATEGORY_NAME_TAKEN &&
                adapter.category_creates == 0 && folio_state_category_count(state) == 4,
            "a category made outside is listed and its name is refused before any folder is made");
    category_name_destroy(taken);
    struct category_name *empty = accepted_category_name("E");
    require(folio_state_create_category(state, empty) == FOLIO_STATE_READY &&
                folio_state_category_count(state) == 5,
            "an empty category is made");
    category_name_destroy(empty);
    folder_add(&adapter, "E", "late");
    require(folio_state_delete_category(state, 4) == FOLIO_STATE_CATEGORY_NOT_EMPTY &&
                adapter.category_removes == 0 &&
                same_text(folio_state_note_name(state, at(4, 0)), "late"),
            "a note put into the category outside is counted: the category is not removed");
    folio_state_destroy(state);
}

/* The open note's category stays in the list though its folder is gone outside. */
static void verify_category_refresh_keeps_open(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 2, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a note in C is open");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "B/A/C"),
            "categories.json now holds C (the fixture's C came from the folder only)");
    category_folder_remove(&adapter, "C");
    category_folder_add(&adapter, "D");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "B/A/C/D"),
            "C stays while its note is open; D made outside is added");
    struct note_ref selected = {0};
    require(folio_state_selection(state, &selected) && selected.category == 2 &&
                selected.note == 1 && same_text(folio_state_document_name(state), "two"),
            "the open note keeps its place");
    outside_categories(&adapter, "{\"version\": 1, \"categories\": ["
                                 "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true},"
                                 "{\"name\": \"D\", \"color\": \"#444444\", \"expanded\": true},"
                                 "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": false},"
                                 "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": true}]}");
    require(folio_state_recolor_category(state, 0, (struct rgb_color){9, 9, 9}) ==
                    FOLIO_STATE_READY &&
                same_text(adapter.written_names, "C/D/B/A") && adapter.written_colors[2].red == 9,
            "B (first in memory) takes the colour at its new place");
    require(folio_state_selection(state, &selected) && selected.category == 0 &&
                selected.note == 1 && same_text(folio_state_document_name(state), "two") &&
                same_text(folio_state_category_name(state, selected.category), "C"),
            "the open note's category is followed to its new place by name");
    folio_state_destroy(state);
}

/* A note ledger still waiting to be written follows its category through a category move. */
static void verify_category_pending_follows(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_LEDGER_STALE,
            "C's ledger waits: its index cannot be read again");
    require(folio_state_move_category(state, 2, 0) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "C/B/A"),
            "C moves to the front while its ledger waits");
    adapter.notes_outcome = PERSISTENCE_LOADED;
    size_t writes = adapter.ledger_writes;
    /* An intent that leaves categories.json alone: the fake borrows the category's name. */
    enum folio_state_outcome selected =
        folio_state_select_note(state, 1, 0, FOLIO_UNTITLED_PROTECT);
    require((selected == FOLIO_STATE_READY || selected == FOLIO_STATE_LEDGER_CHANGED_OUTSIDE) &&
                adapter.ledger_writes == writes + 1 && same_text(adapter.ledger_category, "C"),
            "the waiting ledger is written for C, at its new place");
    folio_state_destroy(state);
}

/* #1071 / ADR0118: a request from outside names a note; the lists are read again first, since
 * whoever wrote the request may have just made the note. */
static struct open_request *_Nonnull open_link(const char *_Nonnull link)
{
    struct open_request *request = nullptr;
    require(open_request_create(link, strlen(link), &request) == OPEN_REQUEST_ACCEPTED &&
                request != nullptr,
            link);
    return request;
}

static enum folio_state_outcome locate_link(struct folio_state *_Nonnull state,
                                            const char *_Nonnull link,
                                            struct note_ref *_Nonnull out)
{
    struct open_request *request = open_link(link);
    enum folio_state_outcome outcome = folio_state_locate_open_request(state, request, out);
    open_request_destroy(request);
    return outcome;
}

static void verify_open_request_located(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_ref ref = {9, 9};
    size_t writes = adapter.writes + adapter.ledger_writes;
    folder_add(&adapter, "C", "fresh");
    require(locate_link(state, "nenefolio://open?category=c&note=FRESH", &ref) ==
                    FOLIO_STATE_READY &&
                same_text(folio_state_category_name(state, ref.category), "C") &&
                same_text(folio_state_note_name(state, ref), "fresh"),
            "a note made outside just before is found; names match in any ASCII case");
    category_folder_add(&adapter, "D");
    require(locate_link(state, "nenefolio://open?category=D&note=two", &ref) == FOLIO_STATE_READY &&
                same_text(folio_state_category_name(state, ref.category), "D") &&
                same_text(folio_state_note_name(state, ref), "two"),
            "a category made outside just before is found with its notes");
    struct note_ref kept = ref;
    require(locate_link(state, "nenefolio://open?category=Z&note=one", &ref) ==
                    FOLIO_STATE_NO_SUCH_CATEGORY &&
                locate_link(state, "nenefolio://open?category=B&note=absent", &ref) ==
                    FOLIO_STATE_NO_SUCH_NOTE &&
                ref.category == kept.category && ref.note == kept.note,
            "a category or a note that is not listed is refused, and nothing is made");
    require(adapter.writes + adapter.ledger_writes == writes && adapter.creates == 0 &&
                adapter.category_creates == 0,
            "locating reads the lists again and writes nothing");
    adapter.categories_outcome = PERSISTENCE_UNREADABLE;
    require(locate_link(state, "nenefolio://open?category=B&note=one", &ref) ==
                FOLIO_STATE_DATA_UNREADABLE,
            "lists that cannot be read again refuse the request");
    folio_state_destroy(state);
}

/* #1070: every list is read again when the data folder is known to have changed. */
static void verify_refresh_lists(void)
{
    char order[200] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    bool changed = true;
    size_t writes = adapter.writes + adapter.ledger_writes;
    require(folio_state_refresh_lists(state, &changed) == FOLIO_STATE_READY && !changed,
            "nothing changed outside: nothing is taken");
    folder_add(&adapter, "B", "six");
    folder_add(&adapter, "C", "seven");
    category_folder_add(&adapter, "D");
    require(folio_state_refresh_lists(state, &changed) == FOLIO_STATE_READY && changed &&
                folio_state_category_count(state) == 4 && !ledger_noticed(state),
            "notes and a category made outside are listed without any intent, and no notice is "
            "left for a later intent");
    row_order(state, order, sizeof order);
    require(strstr(order, "B/one/two/three/six/A/C/one/two/three/seven/D/") == order,
            "every category was read again, the closed one too");
    require(folio_state_refresh_lists(state, &changed) == FOLIO_STATE_READY && !changed &&
                adapter.writes + adapter.ledger_writes == writes,
            "read again once more: the same, and nothing was ever written");
    store_index(&adapter, "B", "{not json");
    folder_add(&adapter, "C", "eight");
    require(folio_state_refresh_lists(state, &changed) == FOLIO_STATE_LEDGER_MALFORMED && changed,
            "a category that cannot be read is reported, and the others are still read again");
    row_order(state, order, sizeof order);
    require(strstr(order, "/C/one/two/three/seven/eight/") != nullptr &&
                strstr(order, "B/one/two/three/six/") == order,
            "the unreadable category keeps the list it had");
    folio_state_destroy(state);
}

/* #1070: whether the open note's file differs from the text it was loaded with. */
static void verify_document_stale(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    bool stale = true;
    require(folio_state_document_stale(state, &stale) == FOLIO_STATE_READY && !stale,
            "no document: nothing is stale");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_document_stale(state, &stale) == FOLIO_STATE_READY && !stale,
            "the file is the text that was loaded");
    adapter.note_body = "# Hello\n\nchanged outside";
    require(folio_state_document_stale(state, &stale) == FOLIO_STATE_READY && stale,
            "the file was written outside");
    require(same_text(folio_state_document_name(state), "one"), "asking changes nothing");
    adapter.note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_document_stale(state, &stale) == FOLIO_STATE_READY && !stale,
            "a file that cannot be read is not a newer text");
    adapter.note_outcome = PERSISTENCE_LOADED;
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_document_stale(state, &stale) == FOLIO_STATE_READY && !stale,
            "an untitled draft has no file to compare");
    folio_state_destroy(state);
}

static void verify_open_request_place(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\n\nfirst body\nsecond body\n";
    struct folio_state *state = ready_state(&adapter);
    struct open_request *request =
        open_link("nenefolio://open?category=B&note=one&lines=4&text=body");
    struct note_byte_span span = {7, 7};
    require(folio_state_open_place(state, request, &span) == OPEN_PLACE_NONE && span.start == 7,
            "no document, no place");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_open_place(state, request, &span) == OPEN_PLACE_TEXT_NEAR &&
                span.start == 15 && span.end == 19,
            "the place is found on the open note's saved source, near the given line");
    open_request_destroy(request);
    folio_state_destroy(state);
}

static void verify_rename_refresh(void);

/* The category and rename stages of #1069 and the open request of #1071 (one call from
 * run_state_tests). */
static void verify_category_refresh(void)
{
    verify_rename_refresh();
    verify_open_request_located();
    verify_open_request_place();
    verify_refresh_lists();
    verify_document_stale();
    verify_category_refresh_adds();
    verify_category_refresh_follows_names();
    verify_category_refresh_refuses();
    verify_category_refresh_expansion();
    verify_category_refresh_create_remove();
    verify_category_refresh_keeps_open();
    verify_category_pending_follows();
}

static struct note_name *_Nonnull accepted_note_name(const char *_Nonnull text);
static enum folio_state_outcome rename_category_to(struct folio_state *_Nonnull state,
                                                   size_t category, const char *_Nonnull text);

/* #1069 / ADR0117 stage 6: a rename's record holds the finished ledger, so the ledger is read
 * again before the plan is made, and once more when the rename is adopted. */
static void verify_rename_refresh_note(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_note_order(state, NOTE_ORDER_MANUAL) == FOLIO_STATE_READY &&
                folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "one in B is open; rows are shown in the ledger's own order");
    store_index(
        &adapter, "B",
        "{\"version\": 2, \"notes\": [\"three\", \"one\", \"two\"], \"favorites\": [\"two\"]}");
    folder_add(&adapter, "B", "six");
    struct note_name *name = accepted_note_name("uno");
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_READY &&
                adapter.renames == 1 && same_text(adapter.renamed_from, "one"),
            "the open note is renamed");
    row_order(state, order, sizeof order);
    require(strstr(order, "B/three/uno/two/six/") == order &&
                folio_state_note_favorite(state, at(0, 2)) &&
                same_text(folio_state_document_name(state), "uno"),
            "the record's ledger was made from the index written outside: its order, star and "
            "the note added outside are kept");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* A rename finished later, by the next intent: what was put in the folder meanwhile is listed. */
static void verify_rename_refresh_resumed(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "one in B is open");
    struct note_name *name = accepted_note_name("uno");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "the rename is left unfinished");
    folder_add(&adapter, "B", "six");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_store_note(state, u"", 0) == FOLIO_STATE_READY,
            "the next intent finishes the rename");
    row_order(state, order, sizeof order);
    require(strstr(order, "B/uno/two/three/A/") == order &&
                same_text(folio_state_document_name(state), "uno"),
            "a rename finished by another intent keeps every place: nothing is read again there");
    require(folio_state_ledger_outcome(state, folio_state_toggle_favorite(state, at(0, 1))) ==
                FOLIO_STATE_LEDGER_REFRESHED,
            "the next intent that writes the ledger reads it again");
    row_order(state, order, sizeof order);
    require(strstr(order, "B/two/uno/three/six/") == order,
            "and the note added outside while the rename waited is listed");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* A category rename finds its category again by name, and is refused when it is gone. */
static void verify_rename_refresh_category(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    outside_categories(&adapter, "{\"version\": 1, \"categories\": ["
                                 "{\"name\": \"A\", \"color\": \"#222222\", \"expanded\": false},"
                                 "{\"name\": \"C\", \"color\": \"#333333\", \"expanded\": true},"
                                 "{\"name\": \"B\", \"color\": \"#111111\", \"expanded\": true}]}");
    require(rename_category_to(state, 0, "Z") == FOLIO_STATE_READY &&
                same_text(adapter.renamed_from, "B") &&
                same_text(folio_state_category_name(state, 0), "A") &&
                same_text(folio_state_category_name(state, 1), "C") &&
                same_text(folio_state_category_name(state, 2), "Z"),
            "B (first in memory) is renamed at its place in the order written outside");
    category_folder_remove(&adapter, "C");
    size_t renames = adapter.renames;
    require(rename_category_to(state, 1, "Y") == FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.renames == renames && folio_state_category_count(state) == 2,
            "a category removed outside is not renamed; the list is the fresh one");
    size_t place = 9;
    require(folio_state_find_category(state, "Z", &place) && place == 1 &&
                !folio_state_find_category(state, "C", &place) && place == 1,
            "a surface that stays open finds its category again by name, or learns it is gone");
    folio_state_destroy(state);
}

/* While a rename is unfinished, a waiting note ledger is neither read again nor written: the
 * names on disk may already be the new ones. */
static void verify_rename_holds_pending_index(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_LEDGER_STALE,
            "C's ledger waits: its index cannot be read again");
    adapter.rename_outcome = RENAME_PENDING;
    require(rename_category_to(state, 2, "Z") == FOLIO_STATE_RENAME_PENDING,
            "C's rename is left unfinished");
    adapter.notes_outcome = PERSISTENCE_LOADED;
    size_t writes = adapter.ledger_writes;
    size_t scans = adapter.note_scans;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_RENAME_PENDING &&
                adapter.ledger_writes == writes && adapter.note_scans == scans,
            "the waiting ledger is not read under the old name while the rename is unfinished");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_category_name(state, 2), "Z") &&
                adapter.ledger_writes == writes,
            "the rename is adopted by the next intent, which keeps every place");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                adapter.ledger_writes == writes + 1 && same_text(adapter.ledger_category, "Z"),
            "and the waiting ledger is then written under the new name");
    folio_state_destroy(state);
}

static void verify_rename_refresh(void)
{
    verify_rename_refresh_note();
    verify_rename_refresh_resumed();
    verify_rename_refresh_category();
    verify_rename_holds_pending_index();
}

/* #1069 / ADR0117: a note ledger is read again (index.json and the folder) before it is
 * written, and the one operation is applied to what was read. The fake keeps what the product
 * wrote; a test edits the folder (`scanned_notes`) or the index (`store_index`) between intents
 * to stand for an editor, a script or an LLM working while the app is open. */
static const char *const with_four[] = {"two", "one", "three", "four"};
static const char *const without_two[] = {"one", "three"};

/* Whether the intent just made read the list again and took it; asking takes the notice. */
static bool ledger_noticed(struct folio_state *_Nonnull state)
{
    return folio_state_ledger_outcome(state, FOLIO_STATE_READY) == FOLIO_STATE_LEDGER_REFRESHED;
}

static void verify_refresh_adds(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                !ledger_noticed(state),
            "nothing changed outside: no notice");
    adapter.scanned_notes = with_four;
    adapter.scanned_note_count = 4;
    require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_READY &&
                adapter.ledger_writes == 2 &&
                same_text(adapter.ledger_order, "one/two/three/four") &&
                adapter.written_favorite_count == 2,
            "a note added outside is in the ledger this write stores, with the stars kept");
    require(ledger_noticed(state) && !ledger_noticed(state), "the list read again is told once");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/four/A/C/one/two/three"),
            "only the category that was written was read again");
    require(folio_state_move_note(state, at(2, 2), at(2, 0)) == FOLIO_STATE_READY &&
                same_text(adapter.ledger_order, "three/one/two/four") && ledger_noticed(state),
            "a reorder is applied to the fresh ledger of its own category");
    folio_state_destroy(state);
}

/* An index.json rewritten outside: the operation finds its note again by name. */
static void verify_refresh_follows_names(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_note_order(state, NOTE_ORDER_MANUAL) == FOLIO_STATE_READY,
            "manual order, so that a category with stars can be reordered");
    store_index(
        &adapter, "B",
        "{\"version\": 2, \"notes\": [\"three\", \"one\", \"two\"], \"favorites\": [\"two\"]}");
    require(
        folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
            same_text(adapter.ledger_order, "three/one/two") &&
            adapter.written_favorite_count == 2 && folio_state_note_favorite(state, at(0, 1)) &&
            folio_state_note_favorite(state, at(0, 2)) &&
            !folio_state_note_favorite(state, at(0, 0)),
        "the star goes to the note that was first, now second; the outside order and star stay");
    store_index(&adapter, "B",
                "{\"version\": 2, \"notes\": [\"two\", \"three\", \"one\"], \"favorites\": []}");
    require(
        folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_READY &&
            same_text(adapter.ledger_order, "three/two/one"),
        "three (first in memory) goes to two's place (last in memory), now first: both by name");
    store_index(&adapter, "B",
                "{\"version\": 2, \"notes\": [\"one\", \"two\", \"three\"], \"favorites\": []}");
    size_t writes = adapter.ledger_writes;
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_READY,
            "three onto one's place");
    require(adapter.ledger_writes == writes + 1 && same_text(adapter.ledger_order, "three/one/two"),
            "the move is the one the user made, on the fresh order");
    folio_state_destroy(state);
}

/* The note the operation is about was deleted outside: refused, the list is the fresh one. */
static void verify_refresh_refuses(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    adapter.scanned_notes = without_two;
    adapter.scanned_note_count = 2;
    require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.ledger_writes == 0 && ledger_noticed(state),
            "a star for a note that is gone writes nothing");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/C/one/two/three"), "the list shows what is there");
    adapter.scanned_notes = scanned_notes;
    adapter.scanned_note_count = 3;
    require(folio_state_move_note(state, at(0, 0), at(0, 1)) == FOLIO_STATE_READY,
            "two is back where the index lists it; one moves onto three's place");
    require(same_text(adapter.ledger_order, "two/three/one"), "on the fresh order");
    adapter.scanned_notes = without_two;
    adapter.scanned_note_count = 2;
    size_t writes = adapter.ledger_writes;
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) ==
                    FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.ledger_writes == writes,
            "a move of a note that is gone writes nothing");
    require(folio_state_move_note(state, at(2, 0), at(2, 1)) ==
                    FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.ledger_writes == writes,
            "nor a move onto the place of a note that is gone");
    folio_state_destroy(state);
}

/* The list cannot be read again: refused with the reading's own outcome, nothing written. */
static void verify_refresh_unreadable(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_DATA_UNREADABLE &&
                adapter.ledger_writes == 0,
            "an index that cannot be read is not overwritten");
    adapter.notes_outcome = PERSISTENCE_LOADED;
    adapter.notes_scan_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_move_note(state, at(0, 0), at(0, 1)) == FOLIO_STATE_DATA_UNREADABLE &&
                adapter.ledger_writes == 0,
            "nor when the folder cannot be read");
    adapter.notes_scan_outcome = PERSISTENCE_LOADED;
    store_index(&adapter, "B",
                "{\"version\": 2, \"notes\": [\"one\", \"one\"], \"favorites\": []}");
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_LEDGER_MALFORMED &&
                adapter.ledger_writes == 0 && !ledger_noticed(state),
            "a malformed index is reported, not replaced by the one in memory");
    folio_state_destroy(state);
}

/* The open note was deleted outside: it stays in the list, and stays the open note. */
static void verify_refresh_keeps_open(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "two is open");
    adapter.scanned_notes = without_two;
    adapter.scanned_note_count = 2;
    require(folio_state_toggle_favorite(state, at(0, 2)) == FOLIO_STATE_READY,
            "a star for three goes through");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three") &&
                same_text(folio_state_document_name(state), "two") &&
                same_text(adapter.ledger_order, "one/two/three") &&
                folio_state_note_favorite(state, at(0, 2)),
            "the open note is kept where the index lists it, still open, and three has the star "
            "(shown first: stars lead)");
    folio_state_destroy(state);
}

static enum folio_state_outcome finish_test_index_filter(struct folio_state *_Nonnull state);

/* Review of #1069: a note added outside is searched by its body too (the bodies are read again
 * after the list was taken), and the cursor follows its note by name. */
static void verify_refresh_search_and_cursor(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(0, 2)) == FOLIO_STATE_READY,
            "the cursor is on three");
    store_index(&adapter, "B",
                "{\"version\": 2, \"notes\": [\"three\", \"one\", \"two\"], \"favorites\": []}");
    require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_READY, "a star for two");
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref cursor = at(9, 9);
    require(folio_state_cursor(state, &kind, &cursor) && kind == FOLIO_CURSOR_NOTE &&
                cursor.category == 0 && cursor.note == 0,
            "the cursor is still on three, now first in the ledger");
    require(test_index_filter_complete(state, u"body", 4) == FOLIO_STATE_READY,
            "every note's body holds the word");
    adapter.scanned_notes = with_four;
    adapter.scanned_note_count = 4;
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                ledger_noticed(state),
            "a star with the filter on takes the list with four");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY, "the bodies are read again");
    row_order(state, order, sizeof order);
    require(strstr(order, "four") != nullptr,
            "the note added outside matches by its body, not only by its name");
    folio_state_destroy(state);
}

/* The allocations of reading a list again, with the open note kept and a changed list taken:
 * a failure anywhere leaves a state that works, and nothing was written before the list was
 * read. Under the plain build no allocation fails and the first round goes through. */
static void verify_refresh_allocation(void)
{
    for (size_t nth = 1; nth < 64; ++nth)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = ready_state(&adapter);
        require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
                "two is open");
        adapter.scanned_notes = with_four;
        adapter.scanned_note_count = 4;
        store_index(&adapter, "B", "{\"version\": 2, \"notes\": [\"one\"], \"favorites\": []}");
        allocation_probe_fail_at(nth);
        enum folio_state_outcome outcome = folio_state_toggle_favorite(state, at(0, 0));
        allocation_probe_fail_at(0);
        require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_OUT_OF_MEMORY,
                "a refresh under the probe goes through or reports memory");
        require(outcome == FOLIO_STATE_READY || adapter.ledger_writes == 0,
                "a refresh that failed wrote nothing");
        (void)ledger_noticed(state);
        require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                    same_text(folio_state_document_name(state), "two"),
                "the state works afterwards and the open note is the same");
        folio_state_destroy(state);
        if (outcome == FOLIO_STATE_READY)
            return;
    }
    require(false, "the refresh went through within the probed allocations");
}

static void verify_move_note(void)
{
    char order[128] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY,
            "three moves to the front");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"), "only B's notes moved");
    require(adapter.ledger_writes == 1 && same_text(adapter.ledger_category, "B") &&
                same_text(adapter.ledger_order, "three/one/two"),
            "the index of the right category was written once");
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_READY, "and back");
    require(adapter.ledger_writes == 2 && same_text(adapter.ledger_order, "one/two/three"),
            "moving down restores the order");
    require(folio_state_move_note(state, at(0, 1), at(0, 1)) == FOLIO_STATE_READY &&
                adapter.ledger_writes == 2,
            "the same position writes nothing");
    require(folio_state_move_note(state, at(3, 0), at(3, 1)) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "no such category");
    require(folio_state_move_note(state, at(0, 3), at(0, 0)) == FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_move_note(state, at(0, 0), at(0, 3)) == FOLIO_STATE_NO_SUCH_NOTE &&
                adapter.ledger_writes == 2,
            "out of range is refused without writing");
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_STORE_FAILED,
            "index not written");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"),
            "the failed write leaves the notes alone");
    adapter.ledger_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_OUT_OF_MEMORY,
            "index oom");
    folio_state_destroy(state);
}

/* 選択と編集中の本文は、番号が付け替わっても同じノートを指し続ける（ADR 0007 の決定 3 / 7）。 */
static void verify_move_selection(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / two");
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY,
            "three moves up");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 1 && same_text(title.category, "B") && same_text(title.note, "two"),
            "the selection follows the note when its neighbour moves");
    require(folio_state_move_note(state, at(0, 0), at(0, 2)) == FOLIO_STATE_READY,
            "three moves back");
    require(same_text(folio_state_pane_title(state).note, "two"), "and back again");
    require(folio_state_move_note(state, at(1, 0), at(1, 2)) == FOLIO_STATE_READY,
            "another category moves");
    require(same_text(adapter.ledger_category, "A") &&
                same_text(folio_state_pane_title(state).note, "two"),
            "a move in another category leaves the selection alone");
    require(folio_state_move_category(state, 0, 2) == FOLIO_STATE_READY, "B moves to the end");
    title = folio_state_pane_title(state);
    require(title.ordinal == 3 && same_text(title.category, "B") && same_text(title.note, "two"),
            "the selected note keeps its category through the move");
    folio_state_destroy(state);
}

/* A だけ別の索引（four / five）にして、別カテゴリへの移動を試せる状態を作る。
 * カテゴリは B（展開・one/two/three）・A（折り畳み・four/five）・C（展開・one/two/three）。 */
static struct folio_state *_Nonnull transfer_state(struct persistence_adapter *_Nonnull adapter)
{
    static const char *const alt_scanned[] = {"four", "five", nullptr};
    test_adapter_second_notes(adapter, "A", "{\"version\": 1, \"notes\": [\"four\", \"five\"]}",
                              alt_scanned);
    return ready_state(adapter);
}

static struct note_name *_Nonnull accepted_note_name(const char *_Nonnull text);

/* #1069 / ADR0117 (stage 4): the paths that change the folder first and write the ledger after
 * (a note made, sent to the bin, moved to another category) read the index and the folder again
 * before that write. A note put into a folder from outside (`folder_add`) and an index edited
 * outside (`store_index`) are in what gets written. B: one/two/three, A: four/five, C as B. */
static void verify_refresh_created_and_binned(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    folder_add(&adapter, "B", "six");
    struct note_name *name = accepted_note_name("new");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_READY,
            "a new note is saved into B");
    require(same_text(adapter.ledger_category, "B") &&
                same_text(adapter.ledger_order, "one/two/three/six/new") &&
                same_text(folio_state_document_name(state), "new"),
            "the index written after the save holds the note added outside, and the new note is "
            "still the open one");
    folder_add(&adapter, "C", "seven");
    store_index(
        &adapter, "C",
        "{\"version\": 2, \"notes\": [\"three\", \"two\", \"one\"], \"favorites\": [\"two\"]}");
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_READY &&
                same_text(adapter.trashed_note, "one") && same_text(adapter.ledger_category, "C") &&
                same_text(adapter.ledger_order, "three/two/seven") &&
                adapter.written_favorite_count == 1,
            "the index written after the bin keeps the outside order, star and note");
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    size_t writes = adapter.ledger_writes;
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_LEDGER_STALE &&
                adapter.ledger_writes == writes,
            "an index that cannot be read again is not overwritten: the write waits");
    adapter.notes_outcome = PERSISTENCE_LOADED;
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                adapter.ledger_writes == writes + 2,
            "the next intent writes the waiting index first, then its own");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* #1069 stage 7: a first save whose name was taken outside is refused, and the note that holds
 * the name is then in the list (the real-file test found it missing until the next write). */
static void verify_refresh_taken_shown(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    folder_add(&adapter, "B", "taken");
    struct note_name *name = accepted_note_name("taken");
    struct note_destination destination = {.category = 0, .name = name};
    size_t writes = adapter.ledger_writes;
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_ledger_outcome(state,
                                           folio_state_store_new(state, &destination, u"x", 1)) ==
                    FOLIO_STATE_NAME_TAKEN &&
                adapter.ledger_writes == writes,
            "a name taken outside is refused, and nothing is written");
    row_order(state, order, sizeof order);
    require(strstr(order, "B/one/two/three/taken/") == order,
            "the note made outside that holds the name is listed after the refusal");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED,
            "the draft is still the open document");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_refresh_transfer(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    folder_add(&adapter, "A", "eight");
    store_index(&adapter, "A",
                "{\"version\": 2, \"notes\": [\"five\", \"four\"], \"favorites\": []}");
    folder_add(&adapter, "B", "six");
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_READY &&
                adapter.moves == 1 && same_text(adapter.moved_note, "one") && ledger_noticed(state),
            "one is dropped before four, with both categories changed outside");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two/three/six/A/five/one/four/eight/C/one/two/three"),
            "it lands before four in the outside order; the notes added outside are in both lists");
    folder_add(&adapter, "A", "two");
    size_t writes = adapter.ledger_writes;
    require(
        folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_NAME_TAKEN &&
            adapter.ledger_writes == writes && adapter.moves == 1,
        "a name that came to the destination from outside is refused before anything is written");
    folder_remove(&adapter, "B", "six");
    require(folio_state_move_note_to_category(state, at(0, 2), 2) ==
                    FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.moves == 1,
            "a note that is gone is not moved");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two/three/A/five/one/four/eight/two/C/one/two/three"),
            "and the lists show what is there");
    folio_state_destroy(state);
}

/* Review of #1069, stage 4. A waiting index is written before the next intent; if the list it
 * is made from differs from the one in memory, the places the caller counted are of the old
 * list, and an intent that names a note by its place is refused instead of acting on another
 * note. B: one/two/three, A: four/five, C as B. */
static void verify_refresh_retry_places(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_LEDGER_STALE &&
                adapter.trashes == 1 && adapter.ledger_writes == 0,
            "one is binned in C; its index cannot be read again and waits");
    /* While it cannot be read: other work goes on, and nothing is written over it. */
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_document_name(state), "one") && adapter.ledger_writes == 0,
            "a note of another category is opened while C's index waits");
    require(folio_state_toggle_favorite(state, at(2, 0)) == FOLIO_STATE_DATA_UNREADABLE &&
                adapter.ledger_writes == 0,
            "an intent that needs that index says it cannot be read");
    adapter.notes_outcome = PERSISTENCE_LOADED;
    folder_remove(&adapter, "C", "two");
    require(folio_state_trash_note(state, at(2, 1), u"x", 1) ==
                    FOLIO_STATE_LEDGER_CHANGED_OUTSIDE &&
                adapter.trashes == 1,
            "the second row was three; the list read again has one row: nothing is binned");
    require(adapter.ledger_writes == 1 && same_text(adapter.ledger_category, "C") &&
                same_text(adapter.ledger_order, "three"),
            "the waiting index was written from what is there");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/three"), "and the list shows it");
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_READY &&
                same_text(adapter.trashed_note, "three"),
            "pointing again at the fresh list bins the note that is meant");
    folio_state_destroy(state);
}

/* The open note is binned while another note of its category was deleted outside: the note
 * that stands at the open note's old place is not kept as if it were the open one. */
static void verify_refresh_bins_open(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_select_note(state, 2, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "one of C is open");
    folder_remove(&adapter, "C", "two");
    require(folio_state_trash_note(state, at(2, 0), u"x", 1) == FOLIO_STATE_READY &&
                same_text(adapter.trashed_note, "one") &&
                same_text(adapter.ledger_order, "three") &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE,
            "the index holds only what is in the folder, and no note is open");
    /* Memory still lists three in C; it was deleted outside. Whether the name is free is asked
     * of the list read again, so the move is not refused. */
    folder_remove(&adapter, "C", "three");
    require(folio_state_move_note_to_category(state, at(0, 2), 2) == FOLIO_STATE_READY &&
                same_text(adapter.moved_note, "three") && same_text(adapter.move_to, "C") &&
                adapter.moves == 1,
            "three of B moves to C, where the three memory knew is gone");
    folio_state_destroy(state);
}

/* #941: 別のカテゴリの末尾へ。位置は状態が数える。同じカテゴリ・無いカテゴリ・絞り込み中。 */
static void verify_move_to_category(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note_to_category(state, at(0, 1), 0) == FOLIO_STATE_READY &&
                adapter.moves == 0,
            "the same category moves nothing");
    require(folio_state_move_note_to_category(state, at(0, 1), 9) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.moves == 0,
            "a category that does not exist is refused");
    require(folio_state_move_note_to_category(state, at(0, 1), 1) == FOLIO_STATE_READY &&
                adapter.moves == 1 && same_text(adapter.move_from, "B") &&
                same_text(adapter.moved_note, "two") && same_text(adapter.move_to, "A"),
            "two moves from B into A");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/four/five/two/C/one/two/three"),
            "the note lands at the end of the destination");
    /* 優先表示中に星があると、位置を指す移動は断るが、末尾への移動は断らない。 */
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                folio_state_set_note_order(state, NOTE_ORDER_FAVORITES_FIRST) == FOLIO_STATE_READY,
            "a star in B while favorites come first");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_ORDERED,
            "a positioned move is refused in the favorites-first order");
    require(folio_state_move_note_to_category(state, at(0, 1), 1) == FOLIO_STATE_READY &&
                same_text(adapter.moved_note, "three") && same_text(adapter.move_to, "A"),
            "the end of another category is not a position in that order, so it is allowed");
    folio_state_destroy(state);
}

/* B の two を A の 1 番目へ移す（ADR 0008 の決定 3 の (a)〜(e)）。 */
static void verify_transfer_note(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_READY,
            "two moves into A");
    require(adapter.moves == 1 && same_text(adapter.move_from, "B") &&
                same_text(adapter.moved_note, "two") && same_text(adapter.move_to, "A"),
            "the md was renamed once, from B to A");
    require(same_text(adapter.calls, "A/four/two/five/move/B/one/three"),
            "destination star is durable before rename, then source is pruned");
    require(adapter.ledger_writes == 2 && same_text(adapter.ledger_order, "one/three"),
            "the source ledger is the last write");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/four/two/five/C/one/two/three"),
            "both indexes follow the file");
    folio_state_destroy(state);
}

/* 折り畳んだカテゴリの末尾（ノート数と同じ番号）へ落とす。 */
static void verify_transfer_to_end(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 0), at(1, 2)) == FOLIO_STATE_READY,
            "one moves to the end of A");
    require(same_text(adapter.calls, "A/four/five/one/move/B/two/three"),
            "destination append is durable before rename, then source is pruned");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two/three/A/four/five/one/C/one/two/three"), "the new order");
    folio_state_destroy(state);
}

/* 選択中で編集中のノートを移しても、本文とモードはそのままで note_ref だけ付け替わる。 */
static void verify_transfer_selection(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / two");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "begin edit");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY,
            "move the selected note");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "A") && same_text(title.note, "two"),
            "the title follows the note into its new category");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody") &&
                adapter.note_writes == 0,
            "the body and the mode are untouched and nothing was saved");
    folio_state_destroy(state);
}

/* 移動元で後ろにあったノートは 1 つ詰み、移動先で挿入位置以降のノートは 1 つ進む。 */
static void verify_transfer_renumbering(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_select_note(state, 0, 2, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / three");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY, "two leaves B");
    require(same_text(folio_state_pane_title(state).note, "three"),
            "the note behind the moved one closes up");
    require(folio_state_select_note(state, 1, 2, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select A / five");
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_READY, "one joins A");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "A") && same_text(title.note, "five"),
            "the notes at and after the insertion move down");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / three again");
    require(folio_state_move_note(state, at(0, 0), at(1, 3)) == FOLIO_STATE_READY,
            "the selected note itself moves to the end of A");
    require(same_text(folio_state_pane_title(state).category, "A") &&
                same_text(folio_state_pane_title(state).note, "three"),
            "the selection travels with it");
    folio_state_destroy(state);
}

/* 同名・範囲外・ファイルを移せないときは何も変えない（ADR 0008 の決定 3 の (a) / (c)）。 */
static void verify_transfer_refusals(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 0), at(2, 0)) == FOLIO_STATE_NAME_TAKEN,
            "C already holds a note called one");
    require(adapter.moves == 0 && adapter.ledger_writes == 0, "a refused move never touches data/");
    require(folio_state_move_note(state, at(0, 0), at(3, 0)) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "no such destination category");
    require(folio_state_move_note(state, at(0, 3), at(1, 0)) == FOLIO_STATE_NO_SUCH_NOTE,
            "no such note");
    require(folio_state_move_note(state, at(0, 0), at(1, 3)) == FOLIO_STATE_NO_SUCH_NOTE,
            "the destination position is past the end");
    adapter.move_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_STORE_FAILED,
            "the file could not be moved");
    require(adapter.moves == 1 && adapter.ledger_writes == 2 &&
                same_text(adapter.calls, "A/one/four/five/move/A/four/five"),
            "a failed rename restores the destination ledger");
    adapter.move_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_OUT_OF_MEMORY,
            "the rename reports out of memory");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"),
            "the refused moves changed nothing");
    folio_state_destroy(state);
}

/* 台帳だけ書けないときは、索引はファイルに追随したまま LEDGER_STALE（決定 3 の (e)）。 */
static void verify_transfer_stale_ledger(void)
{
    char order[160] = {0};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.ledger_fail_at = 1;
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_STORE_FAILED,
            "destination persistence fails before moving md");
    require(adapter.moves == 0 && adapter.ledger_writes == 1 && same_text(adapter.calls, "A"),
            "failed preparation does not move or publish state");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"),
            "failed preparation keeps source");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.ledger_fail_at = 2;
    state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_LEDGER_STALE,
            "source prune failed after the move");
    require(same_text(adapter.calls, "A/four/two/five/move/B"),
            "destination was durable before move");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY &&
                adapter.ledger_writes == 3,
            "next action repairs the pending source index");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/four/two/five/C/one/two/three"),
            "state follows moved md");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.ledger_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_OUT_OF_MEMORY &&
                adapter.moves == 0,
            "preparation allocation failure never moves md");
    folio_state_destroy(state);
}

static void verify_select(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for select");
    require(strstr(folio_state_pane_rtf(state), "\\colortbl;") != nullptr &&
                strstr(folio_state_pane_rtf(state), "\\par") == nullptr &&
                folio_state_pane_rtf_length(state) == strlen(folio_state_pane_rtf(state)),
            "empty pane before selection");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / two");
    require(same_text(adapter.last_note, "two"), "asked the port for the note");
    const char *rtf = folio_state_pane_rtf(state);
    require(strstr(rtf, "\\b\\cf2\\fs44 Hello") != nullptr && strstr(rtf, "body\\par") != nullptr &&
                folio_state_pane_rtf_length(state) == strlen(rtf),
            "pane shows the rendered note");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.any && title.ordinal == 1 && same_text(title.category, "B") &&
                same_text(title.note, "two") && title.color.red == 0x11,
            "title follows the selection");
    struct drawer_layout *marked = nullptr;
    require(folio_state_drawer_layout(state, metrics, &marked) == FOLIO_STATE_READY &&
                drawer_layout_row(marked, 2).selected && drawer_layout_row(marked, 2).cursor &&
                !drawer_layout_row(marked, 1).selected && !drawer_layout_row(marked, 1).cursor,
            "layout marks the selected note and the cursor");
    drawer_layout_destroy(marked);
    require(folio_state_select_note(state, 5, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_NO_SUCH_CATEGORY,
            "bad category");
    require(folio_state_select_note(state, 0, 3, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_NO_SUCH_NOTE,
            "bad note");
    adapter.note_outcome = PERSISTENCE_ABSENT;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NOTE_UNREADABLE &&
                folio_state_pane_rtf(state) == rtf && folio_state_pane_title(state).note[0] == 't',
            "absent note keeps the pane and the selection");
    adapter.note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_NOTE_UNREADABLE,
            "unreadable note");
    adapter.note_outcome = PERSISTENCE_LOADED;
    adapter.note_body = "\xC3";
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_NOTE_UNREADABLE,
            "invalid utf8");
    adapter.note_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_OUT_OF_MEMORY,
            "note oom");
    folio_state_destroy(state);
}

/* 選択中のノートを "<カテゴリ>/<ノート>" の 1 行で読む。何も選んでいなければ "-"。 */
static void selected_name(const struct folio_state *_Nonnull state, char *_Nonnull out,
                          size_t capacity)
{
    struct pane_title_view title = folio_state_pane_title(state);
    out[0] = '\0';
    if (!title.any)
    {
        require(capacity > 1, "selection name fits");
        out[0] = '-';
        out[1] = '\0';
        return;
    }
    size_t position = 0;
    append_name(out, capacity, &position, title.category);
    append_name(out, capacity, &position, title.note);
}

static void expect_selection(const struct folio_state *_Nonnull state,
                             const char *_Nonnull expected, const char *_Nonnull description)
{
    char name[64] = {0};
    selected_name(state, name, sizeof name);
    require(same_text(name, expected), description);
}

/* カーソルを "-"（無し）・"c<カテゴリ>"（カテゴリ行）・"n<カテゴリ>.<ノート>"（ノート行）で読む。
 * 番号は 1 桁の索引でだけ使う。 */
static void cursor_name(const struct folio_state *_Nonnull state, char *_Nonnull out,
                        size_t capacity)
{
    require(capacity > 5, "cursor name fits");
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {.category = 0, .note = 0};
    out[0] = '-';
    out[1] = '\0';
    if (!folio_state_cursor(state, &kind, &cursor))
    {
        return;
    }
    require(cursor.category < 10 && cursor.note < 10, "cursor numbers are single digits");
    out[1] = (char)('0' + cursor.category);
    out[2] = '\0';
    switch (kind)
    {
    case FOLIO_CURSOR_CATEGORY:
        out[0] = 'c';
        break;
    case FOLIO_CURSOR_NOTE:
        out[0] = 'n';
        out[2] = '.';
        out[3] = (char)('0' + cursor.note);
        out[4] = '\0';
        break;
    }
}

static void expect_cursor(const struct folio_state *_Nonnull state, const char *_Nonnull expected,
                          const char *_Nonnull description)
{
    char name[8] = {0};
    cursor_name(state, name, sizeof name);
    require(same_text(name, expected), description);
}

/* 索引は B（展開・one/two/three）・A（折り畳み・one/two/three）・C（展開・one/two/three）。
 * 止まる行は n0.0 / n0.1 / n0.2 / c1（折り畳んだ A）/ n2.0 / n2.1 / n2.2（ADR 0015 の決定 2）。 */
static void verify_step_order(struct folio_state *_Nonnull state,
                              struct persistence_adapter *_Nonnull adapter)
{
    expect_cursor(state, "-", "no cursor before the first step");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "the first step stops at the first stop row");
    expect_selection(state, "B/one", "nothing selected means the first note");
    expect_cursor(state, "n0.0", "the cursor is the selected note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
            "two more steps");
    expect_selection(state, "B/three", "the last note of the first category");
    adapter->last_note = nullptr;
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "the next stop is the collapsed category row");
    expect_cursor(state, "c1", "the cursor stops on the collapsed category");
    expect_selection(state, "B/three", "a category row leaves the selection alone");
    require(adapter->last_note == nullptr, "a category row never reads a note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "past the collapsed category");
    expect_selection(state, "C/one", "the next note is in the following category");
    expect_cursor(state, "n2.0", "and the cursor follows it");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "backwards onto the category row");
    expect_cursor(state, "c1", "the collapsed category is a stop backwards too");
    expect_selection(state, "C/one", "the right pane is still the note it was");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "back");
    expect_selection(state, "B/three", "backwards past the category row selects again");
    expect_cursor(state, "n0.2", "and the cursor is that note");
}

/* 端では動かず、READY のまま読み直しもしない。 */
static void verify_step_edges(struct folio_state *_Nonnull state,
                              struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "gg");
    expect_selection(state, "B/one", "the first stop row");
    adapter->last_note = nullptr;
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "at the top it stops");
    expect_cursor(state, "n0.0", "the cursor did not move");
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter->last_note == nullptr,
            "the same note is never read again");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "G");
    expect_selection(state, "C/three", "the last stop row");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "at the end it stops");
    expect_cursor(state, "n2.2", "the cursor did not move");
}

/* 折り畳んだカテゴリの中に選択があるときは、その位置から次／前の止まる行へ
 * （自分のカテゴリ行へは戻らない）。 */
static void verify_step_from_hidden(struct folio_state *_Nonnull state)
{
    require(folio_state_select_note(state, 1, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select A / two");
    expect_cursor(state, "n1.1", "the cursor is the hidden note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "step out of the collapsed category");
    expect_selection(state, "C/one", "forward from a hidden note");
    require(folio_state_select_note(state, 1, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select A / two again");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "and backwards");
    expect_selection(state, "B/three", "backwards from a hidden note");
}

/* 全部畳んでもカテゴリ行が止まる行として残る。右ペインは動かない（ADR 0015 の決定 2）。 */
static void verify_step_all_collapsed(struct folio_state *_Nonnull state,
                                      struct persistence_adapter *_Nonnull adapter)
{
    for (size_t index = 0; index < 3; ++index)
    {
        require(folio_state_set_category_expanded(state, index, false, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
                "collapse everything");
    }
    adapter->last_note = nullptr;
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "gg");
    expect_cursor(state, "c0", "the first stop is the first category row");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
            "two steps down the category rows");
    expect_cursor(state, "c2", "every category row is a stop");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "at the end it stops");
    expect_cursor(state, "c2", "the cursor did not move");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "backwards");
    expect_cursor(state, "c1", "and back up the category rows");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "G");
    expect_cursor(state, "c2", "the last stop is the last category row");
    expect_selection(state, "B/three", "the selection never moved");
    require(adapter->last_note == nullptr && adapter->note_writes == 0,
            "walking the category rows reads and writes no note");
}

/* 止まる行の種類は意図の前に問い合わせられ、状態は変わらない（決定 2）。 */
static void verify_step_kind(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_ref target = {.category = 0, .note = 0};
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    require(folio_state_step_kind(state, FOLIO_STEP_NEXT, &kind, &target) &&
                kind == FOLIO_CURSOR_NOTE,
            "the first stop is a note row");
    expect_cursor(state, "-", "asking never moves the cursor");
    require(folio_state_select_note(state, 0, 2, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / three");
    require(folio_state_step_kind(state, FOLIO_STEP_NEXT, &kind, &target) &&
                kind == FOLIO_CURSOR_CATEGORY,
            "the next stop is the collapsed category row");
    expect_cursor(state, "n0.2", "and the cursor is still the note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "G");
    kind = FOLIO_CURSOR_CATEGORY;
    require(!folio_state_step_kind(state, FOLIO_STEP_NEXT, &kind, &target) &&
                kind == FOLIO_CURSOR_CATEGORY,
            "at the end there is no target and kind is untouched");
    folio_state_destroy(state);
}

/* カテゴリが 1 つも無ければ止まる行も無い（NO_SUCH_NOTE）。 */
static void verify_step_without_categories(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_ABSENT;
    adapter.scan_outcome = PERSISTENCE_ABSENT;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NO_SUCH_NOTE,
            "no stop row at all");
    expect_cursor(state, "-", "the refused step leaves no cursor");
    struct note_ref target = {.category = 0, .note = 0};
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    require(!folio_state_step_kind(state, FOLIO_STEP_FIRST, &kind, &target),
            "and no target to ask about");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NO_SUCH_CATEGORY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE,
            "new does not invent an initial category");
    folio_state_destroy(state);
}

/* 展開していてノートが 0 本のカテゴリ行にも止まる（決定 2）。A だけ空の索引にする。 */
static void verify_step_empty_category(void)
{
    static const char *const nothing[] = {nullptr};
    struct persistence_adapter adapter = healthy_adapter();
    test_adapter_second_notes(&adapter, "A", "{\"version\": 1, \"notes\": []}", nothing);
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 2, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / three");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j onto A");
    expect_cursor(state, "c1", "the collapsed empty category is a stop");
    require(folio_state_set_category_expanded(state, 1, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 1,
            "expanding an empty category writes the ledger");
    expect_cursor(state, "c1", "with no note to move to, the cursor stays on the row");
    expect_selection(state, "B/three", "and the right pane is untouched");
    require(row_count(state) == 9, "an expanded empty category adds no row");
    require(folio_state_set_category_expanded(state, 1, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 1,
            "expanding it again writes nothing");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j past it");
    expect_selection(state, "C/one", "the next stop is the following category's first note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "k back");
    expect_cursor(state, "c1", "an expanded category without notes is a stop backwards too");
    require(folio_state_set_category_expanded(state, 1, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "h collapses it again");
    expect_cursor(state, "c1", "and the cursor stays on the row");
    folio_state_destroy(state);
}

/* 末尾が折り畳んだカテゴリなら G はそこに止まる（決定 2）。 */
static void verify_step_to_collapsed_end(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(folio_state_set_category_expanded(state, 2, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "collapse C");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "G");
    expect_cursor(state, "c2", "the last stop is the collapsed last category");
    expect_selection(state, "B/one", "and the right pane is untouched");
    folio_state_destroy(state);
}

static void verify_select_adjacent(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    verify_step_order(state, &adapter);
    verify_step_edges(state, &adapter);
    verify_step_from_hidden(state);
    verify_step_all_collapsed(state, &adapter);
    folio_state_destroy(state);
    verify_step_kind();
    verify_step_without_categories();
    verify_step_empty_category();
    verify_step_to_collapsed_end();
}

/* h / l は同じなら書かず、違えば expanded の経路で書き戻す（ADR 0013 の決定 5）。 */
static void verify_set_expanded(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(row_count(state) == 9, "rows before");
    require(folio_state_set_category_expanded(state, 0, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 0,
            "already expanded writes nothing");
    require(folio_state_set_category_expanded(state, 1, false, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 0,
            "already collapsed writes nothing");
    require(folio_state_set_category_expanded(state, 0, false, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 1 && !adapter.written_expanded[0],
            "collapsing writes the ledger once");
    require(row_count(state) == 6, "B's notes are hidden");
    require(folio_state_set_category_expanded(state, 1, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                adapter.writes == 2 && adapter.written_expanded[1],
            "expanding writes it too");
    require(folio_state_set_category_expanded(state, 3, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 2,
            "out of range is refused without writing");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_category_expanded(state, 0, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_STORE_FAILED &&
                row_count(state) == 9,
            "a failed write leaves the ledger alone");
    folio_state_destroy(state);
}

/* h でカーソルがカテゴリ行へ移り、l で中のノートへ戻る（ADR 0015 の決定 3）。 */
static void verify_cursor_collapse(struct folio_state *_Nonnull state,
                                   struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    adapter->last_note = nullptr;
    require(folio_state_set_category_expanded(state, 0, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "h collapses B");
    expect_cursor(state, "c0", "the cursor moves onto the collapsed category row");
    expect_selection(state, "B/one", "the selection and the right pane stay");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j");
    expect_cursor(state, "c1", "on to the next collapsed category");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "k");
    expect_cursor(state, "c0", "and back to the category we collapsed");
    expect_selection(state, "B/one", "the right pane never changed");
    require(adapter->last_note == nullptr, "and no note was read");
    require(folio_state_set_category_expanded(state, 0, true, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "l expands B");
    expect_cursor(state, "n0.0", "the selected note inside takes the cursor");
    require(adapter->last_note == nullptr, "the selected note is not read again");
}

/* 選択が中に無ければ、l は最初のノートを選んで右ペインを変える（決定 3）。 */
static void verify_cursor_expand_elsewhere(struct folio_state *_Nonnull state,
                                           struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_select_note(state, 2, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select C / two");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
            "k twice onto the collapsed A");
    expect_cursor(state, "c1", "the cursor is on A");
    adapter->last_note = nullptr;
    require(folio_state_set_category_expanded(state, 1, true, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "l expands A");
    expect_cursor(state, "n1.0", "the first note of A takes the cursor");
    expect_selection(state, "A/one", "and the right pane follows it");
    require(same_text(adapter->last_note, "one"), "that note was read");
    require(folio_state_set_category_expanded(state, 1, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "h collapses A again");
    expect_cursor(state, "c1", "the cursor is back on the category row");
    expect_selection(state, "A/one", "with the selection left inside");
    require(folio_state_set_category_expanded(state, 1, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "h on a collapsed category does nothing");
    expect_cursor(state, "c1", "and the cursor stays");
}

/* カーソルが別のカテゴリにあるときは、開閉でカーソルは動かない。 */
static void verify_cursor_elsewhere(struct folio_state *_Nonnull state)
{
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / two");
    require(folio_state_set_category_expanded(state, 2, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "collapse C");
    expect_cursor(state, "n0.1", "collapsing another category leaves the cursor alone");
    require(folio_state_set_category_expanded(state, 2, true, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "expand C");
    expect_cursor(state, "n0.1", "expanding another category leaves it alone too");
}

static void verify_cursor_expanded(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    verify_cursor_collapse(state, &adapter);
    verify_cursor_expand_elsewhere(state, &adapter);
    verify_cursor_elsewhere(state);
    folio_state_destroy(state);
}

/* 選択の行が見える位置へ来る量を要求量にする（ADR 0013 の決定 6）。50 px の窓・行 10 px で、
 * 行は B 0..10 / one 10..20 / two 20..30 / three 30..40 / A 40..50（折り畳み）/ C 50..60 /
 * one 60..70 / two 70..80 / three 80..90。上限は 40。 */
static void verify_reveal_cursor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "without a cursor nothing moves");
    require(folio_state_select_note(state, 2, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select C / one");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -20,
            "a row hidden below rises until its bottom edge is in view");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -20,
            "a row already in view does not move again");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -10,
            "a row hidden above sinks until its top edge reaches the band");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "G");
    require(scrolled_top(state) == -10, "the step alone does not scroll");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "the last note lands at the reach");
    /* 行は B 0..10 / A 10..20 / C 20..30 / one 30..40 / two 40..50 / three 50..60・上限 10。 */
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one again");
    require(folio_state_set_category_expanded(state, 0, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "h collapses B");
    expect_cursor(state, "c0", "the cursor is on the collapsed category row");
    require(scrolled_top(state) == -10, "the request is still past the smaller reach");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == 0,
            "a category row is revealed like a note row");
    folio_state_destroy(state);
}

/* ノートの切り替えは表示モードを変えない（ADR 0013 の決定 4）。 */
static void verify_mode_survives_selection(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "begin edit");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select another note");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "selecting keeps the edit mode");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "a step keeps it too");
    require(adapter.note_writes == 0, "selecting never saves by itself");
    require(folio_state_end_edit(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "only end_edit returns to view");
    folio_state_destroy(state);
}

/* CRLF のノートを 1 つ選び、編集モードに入った状態を作る。 */
static struct folio_state *_Nonnull edited_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = port_for(adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for edit");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / two");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"),
            "pane text is the body");
    require(folio_state_pane_text_length(state) == 15, "pane text length");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "begin edit");
    return state;
}

static void verify_edit_guards(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for guards");
    require(folio_state_pane_mode(state) == PANE_MODE_VIEW, "view before any intent");
    require(same_text(folio_state_pane_text(state), "") && folio_state_pane_text_length(state) == 0,
            "no body before selection");
    require(folio_state_begin_edit(state) == FOLIO_STATE_NOTHING_SELECTED, "nothing selected");
    require(folio_state_pane_mode(state) == PANE_MODE_VIEW, "the refused intent keeps view");
    require(folio_state_store_note(state, u"x", 1) == FOLIO_STATE_NOTHING_SELECTED,
            "saving NONE synchronizes and then refuses without writing");
    require(folio_state_end_edit(state, u"x", 1) == FOLIO_STATE_NOTHING_SELECTED,
            "ending edit with NONE also refuses without writing");
    require(adapter.note_writes == 0, "refused intents never write");
    folio_state_destroy(state);
}

static void verify_edit_unchanged(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_store_note(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY &&
                adapter.note_writes == 0 && folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "the same body is not written and the mode stays");
    require(folio_state_end_edit(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY &&
                adapter.note_writes == 0 && folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "leaving without a change writes nothing");
    require(adapter.archives == 0 && same_text(adapter.calls, ""),
            "the same body leaves no history either (ADR 0012)");
    folio_state_destroy(state);
}

/* :q の問い合わせは保存と同じ改行正規化で比較し、状態も永続化も変えない（ADR 0016）。 */
static void verify_note_changed(void)
{
    static const char16_t lone_surrogate[] = {u'a', 0xD83D, u'b'};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    enum folio_note_change change = FOLIO_NOTE_CHANGED;
    const char16_t same[] = u"# Hello\n\nbody";
    require(folio_state_note_changed(state, same, sizeof same / sizeof same[0] - 1, &change) ==
                    FOLIO_STATE_READY &&
                change == FOLIO_NOTE_SAME,
            "line ending normalization finds the saved body");
    const char16_t changed[] = u"# Hello\n\nchanged";
    require(folio_state_note_changed(state, changed, sizeof changed / sizeof changed[0] - 1,
                                     &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_CHANGED,
            "changed editor text is reported");
    change = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, lone_surrogate, 3, &change) ==
                    FOLIO_STATE_NOTE_MALFORMED &&
                change == FOLIO_NOTE_SAME,
            "malformed editor text does not touch the answer");
    require(adapter.archives == 0 && adapter.note_writes == 0 &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"),
            "the comparison changes neither persistence nor state");
    require(folio_state_end_edit(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY,
            "leave edit after comparison");
    require(folio_state_note_changed(state, u"", 0, &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_SAME,
            "a synchronized view has no unsaved change");
    folio_state_destroy(state);
}

/* 保存の主操作と未保存印の元（ADR0060 の決定 8）。同期せず、写せない本文は CHANGED に倒す。
 * 保存の世代は md を書いたときだけ進む。 */
static void verify_unsaved(void)
{
    static const char16_t lone_surrogate[] = {u'a', 0xD83D, u'b'};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE &&
                folio_state_unsaved(state, u"x", 1) == FOLIO_NOTE_SAME,
            "no document has nothing to save");
    folio_state_destroy(state);
    state = edited_state(&adapter);
    const char16_t same[] = u"# Hello\n\nbody";
    require(folio_state_unsaved(state, same, sizeof same / sizeof same[0] - 1) == FOLIO_NOTE_SAME,
            "entering edit alone does not light the save");
    require(folio_state_unsaved(state, u"# Hello\n\nchanged", 16) == FOLIO_NOTE_CHANGED,
            "a changed body is unsaved");
    require(folio_state_unsaved(state, lone_surrogate, 3) == FOLIO_NOTE_CHANGED,
            "a body that cannot be normalized asks to be saved");
    uint64_t before = folio_state_store_revision(state);
    require(folio_state_store_note(state, same, sizeof same / sizeof same[0] - 1) ==
                    FOLIO_STATE_READY &&
                folio_state_store_revision(state) == before,
            "a save that writes nothing does not advance the store revision");
    require(folio_state_store_note(state, u"# Hello\n\nchanged", 16) == FOLIO_STATE_READY &&
                folio_state_store_revision(state) == before + 1 &&
                folio_state_unsaved(state, u"# Hello\n\nchanged", 16) == FOLIO_NOTE_SAME,
            "a written save advances the revision and the body becomes saved");
    require(adapter.note_writes == 1 && folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "the query writes nothing itself");
    require(folio_state_end_edit(state, u"# Hello\n\nchanged", 16) == FOLIO_STATE_READY &&
                folio_state_unsaved(state, u"other", 5) == FOLIO_NOTE_SAME,
            "view mode has no unsaved change");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_unsaved(state, u"", 0) == FOLIO_NOTE_CHANGED,
            "an untitled note is unsaved from the moment it opens");
    folio_state_destroy(state);
}

/* 保存は履歴が先で、写せたときだけ md を書く（ADR 0012 の決定 2）。 */
static void verify_history_order(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_READY, "store a change");
    require(adapter.archives == 1 && adapter.note_writes == 1 &&
                same_text(adapter.calls, "archive/write"),
            "the history is written before the md");
    require(same_text(adapter.archived_category, "B") && same_text(adapter.archived_note, "two"),
            "the archive names the selected note");
    folio_state_destroy(state);
}

/* 履歴を書けなければ保存もしない。md も表示も編集モードもそのまま（ADR 0012 の決定 2）。 */
static void verify_history_failures(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    adapter.archive_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_HISTORY_FAILED,
            "an unwritable history is reported");
    require(adapter.archives == 1 && adapter.note_writes == 0 &&
                same_text(adapter.calls, "archive"),
            "the md is never written when the history fails");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody") &&
                strstr(folio_state_pane_rtf(state), "changed") == nullptr,
            "neither the mode nor the read body nor the pane changed");
    require(folio_state_end_edit(state, u"changed", 7) == FOLIO_STATE_HISTORY_FAILED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "leaving is refused while the history fails");
    adapter.archive_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_OUT_OF_MEMORY &&
                adapter.note_writes == 0,
            "the archive reports out of memory");
    adapter.archive_outcome = PERSISTENCE_ABSENT;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_READY &&
                adapter.note_writes == 1,
            "a missing md has nothing to archive, so the save goes on");
    folio_state_destroy(state);
}

static void verify_edit_saves(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    /* RichEdit は CRLF を返すが、LF が混ざっても元の形（CRLF）へ畳む。 */
    require(folio_state_store_note(state, u"# Hello\r\n\r\nbody\nmore", 20) == FOLIO_STATE_READY,
            "store a changed body");
    require(adapter.note_writes == 1 &&
                same_text(adapter.written_body, "# Hello\r\n\r\nbody\r\nmore"),
            "the written bytes keep the original line ending");
    require(same_text(adapter.written_category, "B") && same_text(adapter.written_note, "two"),
            "written to the selected note");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody\r\nmore") &&
                strstr(folio_state_pane_rtf(state), "more") != nullptr,
            "the read body and the pane follow the save");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "Ctrl+S stays in edit");
    require(folio_state_end_edit(state, u"# Hello\r\n\r\nbody\r\nmore\r\nlast", 27) ==
                FOLIO_STATE_READY,
            "end edit saves");
    require(adapter.note_writes == 2 && folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "leaving with a change writes once and returns to view");
    require(adapter.archives == 2 && same_text(adapter.calls, "archive/write/archive/write"),
            "every save archives first (ADR 0012)");
    folio_state_destroy(state);
}

static bool save_preparation_attempt(size_t nth)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    const char *body = folio_state_pane_text(state);
    const char *pane = folio_state_pane_rtf(state);
    uint64_t revision = folio_state_store_revision(state);
    allocation_probe_fail_at(nth);
    enum folio_state_outcome saved = folio_state_store_note(state, u"changed", 7);
    allocation_probe_fail_at(0);
    printf("save preparation: fail_at=%zu outcome=%d archives=%zu writes=%zu\n", nth, saved,
           adapter.archives, adapter.note_writes);
    if (saved == FOLIO_STATE_READY)
    {
        require(adapter.archives == 1 && adapter.note_writes == 1 &&
                    same_text(adapter.calls, "archive/write"),
                "the first uninjected save archives and writes exactly once");
        folio_state_destroy(state);
        return true;
    }
    require(saved == FOLIO_STATE_OUT_OF_MEMORY && adapter.archives == 0 &&
                adapter.note_writes == 0 && same_text(adapter.calls, ""),
            "save preparation failure must not consume history or write the note");
    allocation_probe_fail_at(nth);
    enum folio_state_outcome left = folio_state_end_edit(state, u"changed", 7);
    allocation_probe_fail_at(0);
    require(left == FOLIO_STATE_OUT_OF_MEMORY && adapter.archives == 0 && adapter.note_writes == 0,
            "repeating the preparation failure while leaving still preserves history");
    require(folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == pane &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_store_revision(state) == revision &&
                same_text(folio_state_pane_title(state).category, "B") &&
                same_text(folio_state_pane_title(state).note, "two"),
            "failed preparation preserves the saved body, view, mode, revision and target");
    require(folio_state_end_edit(state, u"changed", 7) == FOLIO_STATE_READY &&
                adapter.archives == 1 && adapter.note_writes == 1 &&
                same_text(adapter.calls, "archive/write") &&
                same_text(adapter.written_body, "changed") &&
                same_text(folio_state_pane_text(state), "changed") &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                folio_state_store_revision(state) == revision + 1,
            "retry after preparation failure saves once through the original archive/write path");
    folio_state_destroy(state);
    return false;
}

size_t run_save_preparation_allocation_tests(void)
{
    allocation_probe_fail_at(1);
    struct note_text *check = nullptr;
    enum note_text_outcome active = note_text_create("", 0, &check);
    allocation_probe_fail_at(0);
    note_text_destroy(check);
    if (active != NOTE_TEXT_OUT_OF_MEMORY)
        return 0;
    finder = test_regex_port();
    for (size_t nth = 1; nth < 128; ++nth)
    {
        if (save_preparation_attempt(nth))
            return nth - 1;
    }
    test_failure("save preparation allocation sweep must reach a successful save");
}

static void verify_edit_failures(void)
{
    static const char16_t lone_surrogate[] = {u'a', 0xD83D, u'b'};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_store_note(state, lone_surrogate, 3) == FOLIO_STATE_NOTE_MALFORMED &&
                adapter.note_writes == 0,
            "a lone surrogate is refused without writing");
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_NOTE_STORE_FAILED,
            "the store failure is reported");
    require(adapter.note_writes == 1 && folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"),
            "the failed store changes neither the mode nor the read body");
    require(folio_state_end_edit(state, u"changed", 7) == FOLIO_STATE_NOTE_STORE_FAILED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "leaving is refused while the store fails");
    adapter.note_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_OUT_OF_MEMORY,
            "the write reports out of memory");
    folio_state_destroy(state);
}

/* `:e!` は前提だけを判定する。未選択と閲覧中は断る（ADR 0016 の補正 4）。 */
static void verify_discard_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_discard_edits(state) == FOLIO_STATE_NOTHING_SELECTED,
            "nothing to discard without a document");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_discard_edits(state) == FOLIO_STATE_NOT_EDITING,
            "a viewed note has no edits to discard");
    folio_state_destroy(state);
}

/* 保存に失敗したあとの `:e!` は保存済みの本文へ戻し、無題は空の無題のまま（補正 4）。 */
static void verify_discard_edits(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_NOTE_STORE_FAILED,
            "the note cannot be written");
    require(folio_state_discard_edits(state) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT && adapter.note_writes == 1,
            "discarding keeps the edit mode and writes nothing");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody") &&
                folio_state_pane_text_length(state) == 15,
            "the body to reload is the saved one");
    enum folio_note_change change = FOLIO_NOTE_CHANGED;
    require(folio_state_note_changed(state, u"# Hello\r\n\r\nbody", 15, &change) ==
                    FOLIO_STATE_READY &&
                change == FOLIO_NOTE_SAME,
            "the reloaded body is no longer a change");
    folio_state_destroy(state);

    adapter.note_write_outcome = PERSISTENCE_STORED;
    state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "an untitled note");
    require(folio_state_discard_edits(state) == FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_text_length(state) == 0 &&
                same_text(folio_state_pane_text(state), ""),
            "an untitled note is emptied and stays untitled");
    folio_state_destroy(state);
}

/* 編集中でも並び替えられる。本文は UI が持つので保存を挟まない（ADR 0007 の決定 7）。 */
static void verify_move_while_editing(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_READY, "move while editing");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "the mode is kept");
    require(same_text(folio_state_pane_text(state), "# Hello\r\n\r\nbody"), "the body is kept");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "B") && same_text(title.note, "two"),
            "the selection is renumbered");
    require(adapter.note_writes == 0, "the reorder never saves the note");
    folio_state_destroy(state);
}

/* 表引きへ移す前（ADR 0027）の文言そのもの。1 字でも変わったら落ちる。
 * 列挙の全値がちょうど 1 度ずつ並ぶことは CNF-009 が字句で守る。 */
static const char *_Nonnull const expected_failure_lines[] = {
    [FOLIO_STATE_EXPORT_EXTERNAL_IMAGES] =
        "images/ 外のローカル画像もZIPへ同梱しますか？参照の変更は書き出すコピーだけに適用します。",
    [FOLIO_STATE_EXPORT_IMAGE_UNREADABLE] =
        "画像を同梱できませんでした。画像の参照先と読み取り権限を確認してください。",
    [FOLIO_STATE_WEB_LINK_REJECTED] = "このリンクは開けません。",
    [FOLIO_STATE_WEB_LINK_UNAVAILABLE] = "既定のブラウザーでリンクを開けませんでした。",
    [FOLIO_STATE_CODE_COPY_REJECTED] = "このコードブロックはコピーできません。",
    [FOLIO_STATE_SOURCE_UNMAPPED] = "この場所の原文を特定できません。",
    [FOLIO_STATE_LEDGER_CHANGED_OUTSIDE] =
        "一覧がアプリの外で変わっていたので読み直しました。操作は行っていません。",
    [FOLIO_STATE_LEDGER_REFRESHED] = "アプリの外で変わった一覧を読み直しました。",
    [FOLIO_STATE_CLIPBOARD_BUSY] =
        "ほかのアプリがクリップボードを使用中です。もう一度お試しください。",
    [FOLIO_STATE_CLIPBOARD_UNAVAILABLE] = "クリップボードへコピーできませんでした。",
    [FOLIO_STATE_EXPORT_TOO_LARGE] =
        "ZIPの上限は128MiB・65,534ファイル・画像参照4,096件です。画像1枚は16MiBまでです。",
    [FOLIO_STATE_EXPORT_EXISTS] = "その名前のファイルがあります。別の名前を指定してください。",
    [FOLIO_STATE_EXPORT_FAILED] =
        "書き出せませんでした。保存先の場所と書き込み権限を確認してください。",
    [FOLIO_STATE_TRANSFER_UNAVAILABLE] =
        "ファイル選択画面を開けないか、選んだ場所を扱えませんでした。",

    [FOLIO_STATE_READY] = "",
    [FOLIO_STATE_DATA_UNREADABLE] = "data/ を読めませんでした。",
    [FOLIO_STATE_LEDGER_MALFORMED] =
        "data/ の台帳（categories.json / index.json）が版 1 の形ではありません。",
    [FOLIO_STATE_STORE_FAILED] =
        "data/ の台帳（categories.json / index.json）に書き戻せませんでした。表示は変えて"
        "いません。",
    [FOLIO_STATE_NO_SUCH_CATEGORY] = "索引に無いカテゴリが操作されました。",
    [FOLIO_STATE_NO_SUCH_NOTE] = "索引に無いノートが操作されました。",
    [FOLIO_STATE_NOTE_UNREADABLE] = "ノートを読めませんでした。表示は変えていません。",
    [FOLIO_STATE_NOTHING_SELECTED] = "ノートを選んでから編集してください。",
    [FOLIO_STATE_NOT_EDITING] = "編集モードではありません。",
    [FOLIO_STATE_NOTE_MALFORMED] =
        "編集中の本文に壊れた文字、または扱えない文字があります。保存していません。",
    [FOLIO_STATE_NOTE_TOO_LARGE] =
        "本文が大きすぎて保存できません。上限は 16 MiB です。本文を減らしてから保存してください。",
    [FOLIO_STATE_NOTE_UNEDITABLE] = "この本文には編集で保てない文字（U+0007・U+2028・U+"
                                    "2029）があります。閲覧のままにしました。",
    [FOLIO_STATE_NOTE_STORE_FAILED] = "ノートを書き戻せませんでした。編集中の本文はそのままです。"
                                      "別名で保存するか、編集を破棄して読み直せます。",
    [FOLIO_STATE_HISTORY_FAILED] =
        "履歴を書けなかったので保存していません。編集中の本文は残っています。"
        "編集を破棄して読み直せます。",
    [FOLIO_STATE_UNSAVED_CHANGES] =
        "未保存の変更があります。保存するか、未保存変更を破棄して終了してください。",
    [FOLIO_STATE_NAME_TAKEN] =
        "同じ名前のノートがあります。別の名前を指定してください。既存ファイルは変更していま"
        "せん。",
    [FOLIO_STATE_NAME_UNCHANGED] = "同じ名前です。",
    [FOLIO_STATE_LEDGER_STALE] =
        "mdは反映しましたが、台帳（index."
        "json）を書き戻せませんでした。保存を再試行するか、次回の起動で揃います。",
    [FOLIO_STATE_LEDGER_UNSYNCED] =
        "前回の台帳（index.json）をまだ書き戻せていません。今回の操作は行っていないので、"
        "保存を再試行してください。",
    [FOLIO_STATE_RENAME_PENDING] =
        "名前の変更が途中で止まっています。「名前を変更」から再試行してください。",
    [FOLIO_STATE_RENAME_UNLOCKED] =
        "data/ に書けないため名前を変更できません。名前は変更していません。",
    [FOLIO_STATE_RENAME_UNSUPPORTED] = "この data/ では名前を変更できません（ローカルの NTFS "
                                       "以外、またはシンボリックリンク／junction）。",
    [FOLIO_STATE_RENAME_IDENTITY_FAILED] =
        "元のファイルやカテゴリを確かめられないので名前を変更できません。名前は変更していません。",
    [FOLIO_STATE_RENAME_JOURNAL_FAILED] =
        "名前変更の記録（data/.rename.json）を書けませんでした。名前は変更していません。",
    [FOLIO_STATE_RENAME_JOURNAL_BROKEN] =
        "名前変更の記録（data/.rename.json）の版か形を読めません。記録は残しています。",
    [FOLIO_STATE_RENAME_HALTED] =
        "名前変更の記録と実ファイルが一致しません。data/.rename.json と data/<カテゴリ>/ "
        "を確認してください。",
    [FOLIO_STATE_SEARCH_MALFORMED] = "検索する語に壊れた文字があります。語は前のままです。",
    [FOLIO_STATE_DISCOVERY_QUERY_LIMIT] = "検索語は256文字以内で入力してください。",
    [FOLIO_STATE_DISCOVERY_QUERY_INVALID] =
        "検索できない文字があります。検索語を入力し直してください。",
    [FOLIO_STATE_ORDERED] =
        "ノートを並べ替えるには、お気に入り優先を解除して手動順へ戻してください。",
    [FOLIO_STATE_MOVE_ROLLBACK_PENDING] =
        "ノートを移動できず、移動先の台帳も戻せませんでした。次の操作で台帳の同期を再試行します。",
    [FOLIO_STATE_FILTERED] =
        "絞り込み中はこの操作ができません。絞り込みを解いてからやり直してください。",
    [FOLIO_STATE_SETTINGS_UNREADABLE] =
        "設定（data/settings.json）を読めません。既定値で始め、直すまで上書きしません。",
    [FOLIO_STATE_APPEARANCE_UNAVAILABLE] =
        "Windowsの外観を取得できません。前の配色（初回は通常配色）を保ちます。",
    [FOLIO_STATE_SETTINGS_STORE_FAILED] =
        "設定（data/settings.json）を書けませんでした。設定は変えていません。",
    [FOLIO_STATE_PANE_UNAVAILABLE] = "表示中の本文を取り出せませんでした。探していません。",
    [FOLIO_STATE_PANE_LOAD_FAILED] = "本文読込失敗。同じモードで再試行。",
    [FOLIO_STATE_PANE_RECOLOR_FAILED] =
        "本文の配色を更新できませんでした。本文は保持しています。テーマの再選択で再試行できます。",
    [FOLIO_STATE_REPLACE_NO_PATTERN] = "置換するパターンを入れてください。",
    [FOLIO_STATE_REPLACE_BAD_PATTERN] = "正規表現の書き方が違います。",
    [FOLIO_STATE_REPLACE_BAD_TEMPLATE] =
        "置換後の文字列の書き方が違います。使えるのは & \\0〜\\9 \\r \\n \\\\ \\& \\/ です。",
    [FOLIO_STATE_REPLACE_TIMED_OUT] =
        "このパターンは時間がかかりすぎるので止めました。本文は変えていません。",
    [FOLIO_STATE_REPLACE_TOO_COMPLEX] =
        "このパターンは複雑すぎて当てられません。本文は変えていません。",
    [FOLIO_STATE_REPLACE_TOO_MANY] = "一致が多すぎます。パターンを狭めてください。",
    [FOLIO_STATE_REPLACE_TOO_LARGE] = "置き換えた本文が大きすぎます。本文は変えていません。",
    [FOLIO_STATE_REPLACE_STALE] = "下見が古くなりました。もう一度入力してください。",
    [FOLIO_STATE_REPLACE_BAD_SPAN] = "選択範囲が正しくありません。本文は変えていません。",
    [FOLIO_STATE_TAG_INVALID] = "タグ名が空か、使えない文字を含んでいます。",
    [FOLIO_STATE_TAG_LIMIT] = "タグの長さ・個数、またはメタデータの上限を超えます。",
    [FOLIO_STATE_TAGS_UNAVAILABLE] =
        "このメタデータのタグは編集できません。原文を確認してください。",
    [FOLIO_STATE_TAGS_STALE] = "本文か保存先が変わりました。タグの編集を開き直してください。",
    [FOLIO_STATE_NOTE_SOURCE_CHANGED] =
        "ファイルの内容が変わったため保存していません。編集中の本文は残っています。",
    [FOLIO_STATE_OUT_OF_MEMORY] = "記憶域が足りません。",
    [FOLIO_STATE_NAME_REQUIRED] =
        "無題のノートに名前をつけて保存してください。本文は残っています。",
    [FOLIO_STATE_INVALID_NAME] = "使えない名前です。予約名・末尾の空白やピリオド・区切りを避け、."
                                 "mdを含め255バイト以内で指定してください。",
    [FOLIO_STATE_ALREADY_NAMED] =
        "このノートには名前があります。別名保存（:saveas）または名前変更（:"
        "rename）を使ってください。",
    [FOLIO_STATE_CANCELLED] = "",
    [FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE] = "同梱の書体を読めなかったので、OS の書体で表示します。",
    [FOLIO_STATE_HISTORY_EMPTY] = "履歴はありません。",
    [FOLIO_STATE_HISTORY_UNREADABLE] = "その版は読めません。",
    [FOLIO_STATE_CATEGORY_NAME_INVALID] =
        "使えないカテゴリ名です。先頭のピリオド・予約名・末尾の空白やピリオド・区切りを避け、"
        "255バイト以内で指定してください。",
    [FOLIO_STATE_CATEGORY_NAME_TAKEN] =
        "同じ名前のカテゴリがあります。別の名前を指定してください。何も変えていません。",
    [FOLIO_STATE_CATEGORY_NOT_CREATED] =
        "カテゴリのフォルダを作れませんでした。何も変えていません。"
        "data/ に書き込めるか確かめてください。",
    [FOLIO_STATE_CATEGORY_LEDGER_STALE] =
        "カテゴリは反映しましたが、台帳（categories.json）を書き戻せませんでした。"
        "表示はフォルダに従い、次に台帳を書くときに揃います。",
    [FOLIO_STATE_TRASH_UNAVAILABLE] = "この場所ではごみ箱を使えないため、ノートを移していません。",
    [FOLIO_STATE_TRASH_BUSY] = "ほかのプログラムが使用中のため、ノートを移していません。",
    [FOLIO_STATE_TRASH_FAILED] =
        "ごみ箱への移動を確認できませんでした。ファイルとごみ箱を確認してください。"
        "履歴だけ移っている場合があります。",
    [FOLIO_STATE_CATEGORY_NOT_EMPTY] =
        "ノートが残っているカテゴリや、無題のノートの保存先は削除できません。",
    [FOLIO_STATE_CATEGORY_HAS_FILES] =
        "フォルダにほかのファイルが残っているので削除していません。フォルダを確認してください。",
    [FOLIO_STATE_CATEGORY_HISTORY_NOT_RECYCLED] =
        "履歴のごみ箱への移動を確認できないため、カテゴリのフォルダを削除していません。"
        "履歴とごみ箱を確認してください。",
    [FOLIO_STATE_CATEGORY_REMOVE_FAILED] =
        "カテゴリのフォルダの削除を確認できませんでした。フォルダとごみ箱を確認してください。"
        "履歴や索引だけ処理済みの場合があります。",
    [FOLIO_STATE_CATEGORY_NOT_SELECTED] = "カテゴリを選んでください。",
};

static void verify_failure_lines(void)
{
    const size_t count = sizeof expected_failure_lines / sizeof expected_failure_lines[0];
    for (size_t index = 0; index < count; ++index)
    {
        require(
            same_text(folio_state_failure_line((enum folio_state_outcome)index, FOLIO_LANGUAGE_JA),
                      expected_failure_lines[index]),
            "every failure line still reads exactly as before");
    }
}

static void expect_failure(struct persistence_adapter adapter, enum folio_state_outcome expected,
                           const char *_Nonnull description)
{
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == expected,
            description);
}

static void verify_failures(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable categories");
    adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_MALFORMED;
    expect_failure(adapter, FOLIO_STATE_LEDGER_MALFORMED, "malformed categories");
    adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_OUT_OF_MEMORY;
    expect_failure(adapter, FOLIO_STATE_OUT_OF_MEMORY, "out of memory categories");
    adapter = healthy_adapter();
    adapter.scan_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable scan");
    adapter = healthy_adapter();
    adapter.notes_outcome = PERSISTENCE_MALFORMED;
    expect_failure(adapter, FOLIO_STATE_LEDGER_MALFORMED, "malformed notes");
    adapter = healthy_adapter();
    adapter.notes_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable notes");
    adapter = healthy_adapter();
    adapter.notes_scan_outcome = PERSISTENCE_UNREADABLE;
    expect_failure(adapter, FOLIO_STATE_DATA_UNREADABLE, "unreadable note scan");
    adapter = healthy_adapter();
    adapter.notes_scan_outcome = PERSISTENCE_OUT_OF_MEMORY;
    expect_failure(adapter, FOLIO_STATE_OUT_OF_MEMORY, "out of memory note scan");
}

struct persistence_adapter *_Nonnull test_adapter_create(const char *_Nonnull categories_text,
                                                         const char *_Nonnull notes_text)
{
    struct persistence_adapter *adapter = malloc(sizeof *adapter);
    if (adapter == nullptr)
    {
        exit(1);
    }
    *adapter = healthy_adapter();
    adapter->categories_text = categories_text;
    adapter->notes_text = notes_text;
    return adapter;
}

void test_adapter_history(struct persistence_adapter *_Nonnull adapter, size_t version,
                          const char *_Nullable body)
{
    adapter->history[version] = body;
}

struct persistence_port test_adapter_port(struct persistence_adapter *_Nonnull adapter)
{
    return port_for(adapter);
}

void test_adapter_second_notes(struct persistence_adapter *_Nonnull adapter,
                               const char *_Nonnull category, const char *_Nonnull notes_text,
                               const char *_Nonnull const *_Nonnull scanned)
{
    adapter->alt_category = category;
    adapter->alt_notes_text = notes_text;
    adapter->alt_scanned = scanned;
}

size_t test_adapter_category_creates(const struct persistence_adapter *_Nonnull adapter)
{
    return adapter->category_creates;
}

size_t test_adapter_body_publications(const struct persistence_adapter *_Nonnull adapter)
{
    return adapter->note_writes + adapter->archives;
}

size_t test_adapter_trashes(const struct persistence_adapter *_Nonnull adapter)
{
    return adapter->trashes;
}

size_t test_adapter_category_removes(const struct persistence_adapter *_Nonnull adapter)
{
    return adapter->category_removes;
}

void test_adapter_destroy(struct persistence_adapter *_Nullable adapter)
{
    free(adapter);
}

/* 3 つのポートを 1 つの束にする（ADR 0029 の決定 3）。folio_state_create は束を借りるだけで
 * 返ったあとは参照しないので、テストの中だけで生きる 1 つの入れ物を使い回す。 */
static enum web_link_outcome test_open_link(const struct web_uri *_Nonnull uri)
{
    (void)uri;
    return WEB_LINK_UNAVAILABLE;
}

const struct folio_ports *_Nonnull test_ports(const struct persistence_port *_Nonnull persistence,
                                              const struct appearance_port *_Nonnull appearance,
                                              const struct regex_port *_Nonnull regex)
{
    static struct folio_ports ports;
    ports.persistence = persistence;
    ports.appearance = appearance;
    ports.regex = regex;
    static const struct web_link_port links = {.open = test_open_link};
    ports.web_link = &links;
    ports.clipboard = test_clipboard_port();
    ports.transfer = test_transfer_port();
    ports.images = test_image_port();
    return &ports;
}

struct appearance_port test_appearance_port(void)
{
    return looks_for(&dark_adapter);
}

static struct note_name *_Nonnull accepted_note_name(const char *_Nonnull text)
{
    struct note_name *name = nullptr;
    require(note_name_create(text, strlen(text), &name) == NOTE_NAME_ACCEPTED, "name prepared");
    return name;
}

static void verify_untitled(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE &&
                folio_state_category_count(state) == 3 && folio_state_current_category(state) == 0,
            "empty pane default category");
    require(folio_state_new_note(state, 99, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "no invented category");
    require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "new in A");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_document_category(state) == 1,
            "untitled edit state");
    require(same_text(folio_state_pane_title(state).note, "無題") &&
                same_text(folio_state_category_name(state, 1), "A"),
            "untitled title and category");
    struct note_ref selection = {.category = 0, .note = 0};
    require(!folio_state_selection(state, &selection) && folio_state_note_count(state) == 9,
            "untitled has no fictitious index row");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_NAME_REQUIRED &&
                folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NAME_REQUIRED,
            "untitled cannot be displaced");
    require(folio_state_store_note(state, u"x", 1) == FOLIO_STATE_NAME_REQUIRED &&
                folio_state_end_edit(state, u"x", 1) == FOLIO_STATE_NAME_REQUIRED,
            "unnamed saves require name");
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, u"", 0, &changed) == FOLIO_STATE_READY &&
                changed == FOLIO_NOTE_CHANGED,
            "even blank untitled requires explicit save or discard");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_move_category(state, 1, 0) == FOLIO_STATE_READY &&
                folio_state_document_category(state) == 0,
            "untitled follows category ordering");
    folio_state_destroy(state);
}

static void verify_first_save(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "new note");
    struct note_name *name = accepted_note_name("日報 名前.MD");
    struct note_destination destination = {.category = 1, .name = name};
    require(folio_state_store_new(state, &destination, u"日本語\r\n本文", 7) == FOLIO_STATE_READY,
            "first save to chosen category");
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_NAME_TAKEN,
            "another create cannot overwrite the just-created note");
    note_name_destroy(name);
    require(adapter.creates == 1 && adapter.note_writes == 0 && adapter.archives == 0 &&
                adapter.ledger_writes == 1,
            "create then ledger, with no archive or replace");
    require(same_text(adapter.created_note, "日報 名前") &&
                same_text(adapter.written_category, "A") &&
                same_text(adapter.written_body, "日本語\n本文"),
            "named utf8 LF creation");
    struct note_ref selected = {.category = 0, .note = 0};
    require(folio_state_selection(state, &selected) && selected.category == 1 &&
                selected.note == 3 && folio_state_note_count(state) == 10 &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED,
            "creation selects the appended note");
    require(same_text(folio_state_pane_title(state).note, "日報 名前"),
            "ledger owns the name after input is gone");
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_READY &&
                adapter.note_writes == 1 && adapter.archives == 1,
            "later save uses the existing history path");
    folio_state_destroy(state);
}

static void verify_first_save_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_name *name = accepted_note_name("draft");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_NOTHING_SELECTED,
            "not an untitled document");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled");
    destination.category = 99;
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "invalid destination");
    destination.category = 0;
    const char16_t broken[] = {0xD800};
    require(folio_state_store_new(state, &destination, broken, 1) == FOLIO_STATE_NOTE_MALFORMED,
            "invalid UTF16 stays unsaved");
    adapter.create_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_NOTE_STORE_FAILED,
            "create failure");
    adapter.create_outcome = PERSISTENCE_NAME_TAKEN;
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_NAME_TAKEN,
            "disk collision");
    adapter.create_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_OUT_OF_MEMORY,
            "adapter allocation failure");
    note_name_destroy(name);
    name = accepted_note_name("one");
    destination.name = name;
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_NAME_TAKEN &&
                adapter.creates == 3 && adapter.ledger_writes == 0 && adapter.archives == 0,
            "index collision is rejected without I/O");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_note_count(state) == 9,
            "failures retain the untitled document and index");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_created_stale_index(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled");
    struct note_name *name = accepted_note_name("draft");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"draft text", 10) ==
                FOLIO_STATE_LEDGER_STALE,
            "report partial completion");
    note_name_destroy(name);
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED &&
                same_text(folio_state_pane_text(state), "draft text") &&
                same_text(folio_state_pane_title(state).note, "draft"),
            "state agrees with the durable md");
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, u"draft text", 10, &changed) ==
                FOLIO_STATE_LEDGER_UNSYNCED,
            "q cannot ignore a pending index");
    require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_move_category(state, 0, 1) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_store_note(state, u"draft text", 10) == FOLIO_STATE_LEDGER_UNSYNCED,
            "pending index prevents follow-up operations");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note(state, u"draft text", 10) == FOLIO_STATE_READY &&
                adapter.creates == 1 && adapter.note_writes == 0 && adapter.archives == 0,
            "retry repairs only the index");
    require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "continue after repair");
    folio_state_destroy(state);
}

static void verify_first_save_keeps_category_cursor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_category_expanded(state, 2, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "last category is collapsed in this scenario");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled in B");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "cursor to last collapsed category");
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {.category = 0, .note = 0};
    require(folio_state_cursor(state, &kind, &cursor) && kind == FOLIO_CURSOR_CATEGORY,
            "category cursor exists");
    size_t category = cursor.category;
    struct note_name *name = accepted_note_name("draft");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"draft", 5) == FOLIO_STATE_READY,
            "save in another category");
    require(folio_state_cursor(state, &kind, &cursor) && kind == FOLIO_CURSOR_CATEGORY &&
                cursor.category == category,
            "first save does not steal the category cursor");
    require(folio_state_set_category_expanded(state, category, true, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_document_category(state) == category,
            "l still enters the intended category");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* 別名保存は元のwrite/archiveを呼ばず、編集中の本文だけを新しいmdへ保存する。 */
static void verify_save_as_edit(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Original\r\n";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit source for save as");
    struct note_name *name = accepted_note_name("別の 名前.md");
    struct note_destination destination = {.category = 1, .name = name};
    require(folio_state_store_new(state, &destination, u"# Draft\n日本語", 11) == FOLIO_STATE_READY,
            "save unsaved input under a different name");
    require(adapter.creates == 1 && adapter.archives == 0 && adapter.note_writes == 0 &&
                same_text(adapter.written_body, "# Draft\r\n日本語") &&
                same_text(adapter.note_body, "# Original\r\n"),
            "original file and history are not written; CRLF is retained");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_title(state).note, "別の 名前") &&
                folio_state_document_category(state) == 1,
            "new note is open in the same mode and chosen category");
    require(folio_state_store_note(state, u"later", 5) == FOLIO_STATE_READY &&
                adapter.archives == 1 && adapter.note_writes == 1 &&
                same_text(adapter.written_note, "別の 名前"),
            "subsequent save uses the new note history and name");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_save_as_view(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# 原文\r\n**強調** [link](url)\n";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select view");
    struct note_name *name = accepted_note_name("view-copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"Rendered text", 13) == FOLIO_STATE_READY,
            "save as while viewing");
    require(same_text(adapter.written_body, adapter.note_body) && adapter.creates == 1 &&
                adapter.archives == 0 && adapter.note_writes == 0 &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "view copies markdown syntax and original mixed line endings, ignoring UI text");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_save_as_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "saved";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit source for save as");
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.create_outcome = PERSISTENCE_NAME_TAKEN;
    require(folio_state_store_new(state, &destination, u"draft", 5) == FOLIO_STATE_NAME_TAKEN,
            "disk collision is not overwritten");
    adapter.create_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"draft", 5) ==
                FOLIO_STATE_NOTE_STORE_FAILED,
            "unwritable new name is refused");
    require(same_text(folio_state_pane_text(state), "saved") &&
                same_text(folio_state_pane_title(state).note, "one") &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_note_count(state) == 9 && adapter.archives == 0 &&
                adapter.note_writes == 0,
            "failure retains original selection and never autosaves the input");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_save_as_view_stale(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select view");
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"", 0) == FOLIO_STATE_LEDGER_STALE &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                same_text(folio_state_pane_title(state).note, "copy"),
            "a published copy is adopted even when its index is stale");
    enum folio_note_change change = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, u"", 0, &change) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_store_note(state, u"", 0) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_store_new(state, &destination, u"", 0) == FOLIO_STATE_LEDGER_UNSYNCED &&
                adapter.creates == 1,
            "view quit, save and another create report the unrepaired index without acting");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note(state, u"", 0) == FOLIO_STATE_READY && adapter.creates == 1 &&
                adapter.archives == 0 && adapter.note_writes == 0 &&
                folio_state_note_changed(state, u"", 0, &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_SAME,
            "save repairs the index without rewriting the viewing copy");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* 台帳が未同期のままの別名保存は md を作らず LEDGER_UNSYNCED（名前入力面は閉じない）。
 * :q の変更確認も他の保存系と同じ修復を試み、回復したら通常の判定へ進む（2026-09-16 の補正）。 */
static void verify_save_as_edit_unsynced(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "saved";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit source for save as");
    struct note_name *first = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = first};
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"draft", 5) == FOLIO_STATE_LEDGER_STALE &&
                adapter.creates == 1 && folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "the published copy is adopted while its index stays pending");
    note_name_destroy(first);
    struct note_name *second = accepted_note_name("copy2");
    destination.name = second;
    require(folio_state_store_new(state, &destination, u"draft again", 11) ==
                    FOLIO_STATE_LEDGER_UNSYNCED &&
                adapter.creates == 1 && adapter.archives == 0 && adapter.note_writes == 0,
            "save as under an unrepaired index creates no md");
    require(same_text(folio_state_pane_title(state).note, "copy") &&
                same_text(folio_state_pane_text(state), "draft") &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "the refused save as leaves the open document and its input untouched");
    enum folio_note_change change = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, u"draft again", 11, &change) ==
                FOLIO_STATE_LEDGER_UNSYNCED,
            "q retries the repair and refuses while it keeps failing");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_note_changed(state, u"draft again", 11, &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_CHANGED,
            "a repaired index lets q judge the body itself");
    require(folio_state_note_changed(state, u"draft", 5, &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_SAME && adapter.note_writes == 0 && adapter.archives == 0,
            "the question never saves");
    require(folio_state_store_new(state, &destination, u"draft again", 11) == FOLIO_STATE_READY &&
                adapter.creates == 2 && same_text(folio_state_pane_title(state).note, "copy2"),
            "save as succeeds once the index is repaired");
    note_name_destroy(second);
    folio_state_destroy(state);
}

/* 改名は未選択・無題・同名・大小文字だけの別名を、記録を公開する前に断る（ADR 0022 の決定 1）。 */
static void verify_rename_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_name *other = accepted_note_name("次の名前");
    require(folio_state_rename_note(state, other, u"", 0) == FOLIO_STATE_NOTHING_SELECTED,
            "nothing selected cannot be renamed");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled draft");
    require(folio_state_rename_note(state, other, u"draft", 5) == FOLIO_STATE_NAME_REQUIRED,
            "an untitled note must be saved first");
    require(adapter.renames == 0 && adapter.creates == 0, "no refusal touched data/");
    note_name_destroy(other);
    folio_state_destroy(state);

    adapter = healthy_adapter();
    state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    struct note_name *same = accepted_note_name("one.md");
    require(folio_state_rename_note(state, same, u"", 0) == FOLIO_STATE_NAME_UNCHANGED &&
                adapter.renames == 0,
            "the same name changes nothing");
    note_name_destroy(same);
    struct note_name *folded = accepted_note_name("ONE");
    require(folio_state_rename_note(state, folded, u"", 0) == FOLIO_STATE_NAME_TAKEN &&
                adapter.renames == 0,
            "a name differing only in case is a collision on Windows");
    note_name_destroy(folded);
    struct note_name *taken = accepted_note_name("Two.md");
    require(folio_state_rename_note(state, taken, u"", 0) == FOLIO_STATE_NAME_TAKEN &&
                adapter.renames == 0,
            "another note in the category is never renamed over");
    note_name_destroy(taken);
    require(same_text(folio_state_pane_title(state).note, "one") &&
                !folio_state_pane_title(state).recovering,
            "the refused rename keeps the current name");
    folio_state_destroy(state);
}

/* 編集中の未保存本文は改名より先に同じ保存経路で確定する（決定 1）。 */
static void verify_rename_saves_first(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "saved";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit the note to rename");
    struct note_name *name = accepted_note_name("日本語 新しい名前.md");
    require(folio_state_rename_note(state, name, u"draft", 5) == FOLIO_STATE_READY,
            "the rename completes after the save");
    require(same_text(adapter.calls, "archive/write/rename") && adapter.archives == 1 &&
                adapter.note_writes == 1 && adapter.renames == 1,
            "the body is archived and written under the old name before the move");
    require(same_text(adapter.renamed_from, "one") &&
                same_text(adapter.renamed_to, "日本語 新しい名前") &&
                same_text(adapter.renamed_category, "B"),
            "the intent carries the category and both names");
    struct note_ref selection = {.category = 0, .note = 0};
    require(folio_state_selection(state, &selection) && selection.category == 0 &&
                selection.note == 0,
            "the note keeps its position in the index");
    require(same_text(folio_state_pane_title(state).note, "日本語 新しい名前") &&
                !folio_state_pane_title(state).recovering &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_pane_text(state), "draft"),
            "the breadcrumb follows the new name without resetting body or mode");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* 先行する保存や台帳の修復が失敗したら、改名の意図を確保すらしない。 */
static void verify_rename_blocked_by_save(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "saved";
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit the note to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    require(folio_state_rename_note(state, name, u"draft", 5) == FOLIO_STATE_NOTE_STORE_FAILED &&
                adapter.renames == 0,
            "an unwritable body stops the rename before the intent");
    note_name_destroy(name);
    folio_state_destroy(state);

    adapter = healthy_adapter();
    state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to copy");
    struct note_name *copy = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = copy};
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"", 0) == FOLIO_STATE_LEDGER_STALE,
            "the copy is published while its index stays pending");
    struct note_name *renamed = accepted_note_name("copy2");
    require(folio_state_rename_note(state, renamed, u"", 0) == FOLIO_STATE_LEDGER_UNSYNCED &&
                adapter.renames == 0,
            "an unrepaired index refuses the rename without acting");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_rename_note(state, renamed, u"", 0) == FOLIO_STATE_READY &&
                adapter.renames == 1 && same_text(adapter.renamed_from, "copy"),
            "the repaired index lets the same rename through");
    note_name_destroy(renamed);
    note_name_destroy(copy);
    folio_state_destroy(state);
}

/* 記録の公開前に断られた改名は意図を残さず、次の操作を妨げない（決定 2）。 */
static void verify_rename_refused_before_journal(void)
{
    static const enum rename_outcome refusals[] = {RENAME_UNLOCKED, RENAME_NAME_TAKEN,
                                                   RENAME_UNSUPPORTED, RENAME_IDENTITY_FAILED,
                                                   RENAME_JOURNAL_FAILED};
    static const enum folio_state_outcome expected[] = {
        FOLIO_STATE_RENAME_UNLOCKED, FOLIO_STATE_NAME_TAKEN, FOLIO_STATE_RENAME_UNSUPPORTED,
        FOLIO_STATE_RENAME_IDENTITY_FAILED, FOLIO_STATE_RENAME_JOURNAL_FAILED};
    for (size_t index = 0; index < sizeof refusals / sizeof refusals[0]; ++index)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = ready_state(&adapter);
        require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
                "select to rename");
        struct note_name *name = accepted_note_name("新しい名前");
        adapter.rename_outcome = refusals[index];
        require(folio_state_rename_note(state, name, u"", 0) == expected[index],
                "each refusal keeps its own reason");
        require(!adapter.journal_published &&
                    same_text(folio_state_pane_title(state).note, "one") &&
                    !folio_state_pane_title(state).recovering,
                "a refusal before the journal leaves no intent");
        require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                    folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                    adapter.renames == 1,
                "the next intents run without a retry");
        note_name_destroy(name);
        folio_state_destroy(state);
    }
}

/* 未完了の意図があるあいだ、すべての意図が先に同じ改名を再試行する（決定 7）。 */
static void verify_rename_blocked_intents(struct folio_state *_Nonnull state,
                                          enum folio_state_outcome expected)
{
    enum folio_note_change change = FOLIO_NOTE_SAME;
    struct note_name *copy = accepted_note_name("別名の写し");
    struct note_destination destination = {.category = 0, .name = copy};
    struct note_ref from = {.category = 0, .note = 0};
    struct note_ref to = {.category = 0, .note = 2};
    struct rgb_color color = {.red = 1, .green = 2, .blue = 3};
    require(folio_state_store_note(state, u"", 0) == expected &&
                folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == expected &&
                folio_state_toggle_category(state, 0) == expected &&
                folio_state_recolor_category(state, 0, color) == expected &&
                folio_state_move_category(state, 0, 1) == expected &&
                folio_state_move_note(state, from, to) == expected &&
                folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == expected &&
                folio_state_store_new(state, &destination, u"", 0) == expected &&
                folio_state_note_changed(state, u"", 0, &change) == expected,
            "every intent retries the same rename and refuses to proceed");
    note_name_destroy(copy);
}

static void verify_rename_pending_blocks(struct folio_state *_Nonnull state,
                                         struct persistence_adapter *_Nonnull adapter)
{
    verify_rename_blocked_intents(state, FOLIO_STATE_RENAME_PENDING);
    require(adapter->renames == 10 && adapter->last_attempt == RENAME_RESUME &&
                adapter->note_writes == 0 && adapter->writes == 0 && adapter->creates == 0 &&
                adapter->moves == 0,
            "the retries are the only calls the refused intents make, and they all resume");
}

static void verify_rename_pending(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "saved";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING &&
                adapter.renames == 1 && adapter.last_attempt == RENAME_START &&
                adapter.journal_published,
            "the published journal becomes the single retained intent");
    struct pane_title_view title = folio_state_pane_title(state);
    struct rename_view pending = {.from = "", .to = ""};
    require(title.recovering && same_text(title.note, "one") && same_text(title.category, "B") &&
                same_text(folio_state_document_name(state), "one"),
            "the view keeps the real name and only flags the recovery");
    require(folio_state_rename_pending(state, &pending) && same_text(pending.from, "one") &&
                same_text(pending.to, "新しい名前"),
            "the held intent is readable as a closed pair of names");
    verify_rename_pending_blocks(state, &adapter);
    struct note_name *other_name = accepted_note_name("別の名前");
    require(folio_state_rename_note(state, other_name, u"", 0) == FOLIO_STATE_RENAME_PENDING &&
                same_text(adapter.renamed_to, "新しい名前"),
            "a different rename cannot start while one is unfinished");
    note_name_destroy(other_name);
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_store_note(state, u"", 0) == FOLIO_STATE_READY && adapter.renames == 12 &&
                !adapter.journal_published,
            "the next intent finishes the rename before doing its own work");
    title = folio_state_pane_title(state);
    require(!title.recovering && same_text(title.note, "新しい名前") &&
                same_text(folio_state_document_name(state), "新しい名前") &&
                !folio_state_rename_pending(state, &pending),
            "the completed intent hands over the prepared ledger and drops the pending pair");
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_NAME_UNCHANGED &&
                adapter.renames == 12,
            "asking for the finished name again changes nothing");
    note_name_destroy(name);
    folio_state_destroy(state);
}

/* 記録を公開した後の HALTED も意図を捨てない。理由は PENDING と別の 1 行になる（決定 2）。 */
static void verify_rename_halted(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_HALTED;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_HALTED &&
                adapter.renames == 1 && adapter.journal_published,
            "a halted rename keeps the published intent instead of dropping it");
    struct rename_view pending = {.from = "", .to = ""};
    require(folio_state_pane_title(state).recovering &&
                folio_state_rename_pending(state, &pending) && same_text(pending.from, "one") &&
                same_text(pending.to, "新しい名前"),
            "the halted intent is the one that is retried");
    verify_rename_blocked_intents(state, FOLIO_STATE_RENAME_HALTED);
    require(adapter.renames == 10 && adapter.last_attempt == RENAME_RESUME &&
                adapter.note_writes == 0 && adapter.creates == 0 && adapter.moves == 0,
            "a halted intent blocks the same operations and only re-evaluates");
    require(strcmp(folio_state_failure_line(FOLIO_STATE_RENAME_HALTED, FOLIO_LANGUAGE_JA),
                   folio_state_failure_line(FOLIO_STATE_RENAME_PENDING, FOLIO_LANGUAGE_JA)) != 0,
            "the two post-publication reasons read differently");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_store_note(state, u"", 0) == FOLIO_STATE_READY &&
                same_text(folio_state_document_name(state), "新しい名前"),
            "repairing data/ lets the same intent finish");
    note_name_destroy(name);
    folio_state_destroy(state);

    /* HALTED のまま閉じても記録は残る（:q! と同じ）。 */
    adapter = healthy_adapter();
    state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *again = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_HALTED;
    require(folio_state_rename_note(state, again, u"", 0) == FOLIO_STATE_RENAME_HALTED,
            "a halted rename");
    note_name_destroy(again);
    folio_state_destroy(state);
    require(adapter.journal_published, "closing never discards a halted intent either");
}

/* :q! は意図を捨てずに終わる。永続的な記録は次回の起動が続きを行う（決定 7）。 */
static void verify_rename_force_quit_keeps_intent(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "an unfinished rename");
    note_name_destroy(name);
    folio_state_destroy(state);
    require(adapter.journal_published && adapter.renames == 1,
            "closing the application never discards the published intent");
}

/* 起動はカテゴリの走査より先に復旧を終える。終わらなければ起動しない（決定 6）。 */
static void verify_rename_recovery(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(adapter.recovers == 1 && adapter.note_scans > 0, "recovery runs at startup");
    folio_state_destroy(state);

    adapter = healthy_adapter();
    adapter.recover_outcome = RENAME_COMPLETED;
    state = ready_state(&adapter);
    require(adapter.recovers == 1, "a finished recovery lets the scan proceed");
    folio_state_destroy(state);

    static const enum rename_outcome refusals[] = {RENAME_PENDING, RENAME_JOURNAL_BROKEN,
                                                   RENAME_HALTED, RENAME_UNLOCKED};
    static const enum folio_state_outcome expected[] = {
        FOLIO_STATE_RENAME_PENDING, FOLIO_STATE_RENAME_JOURNAL_BROKEN, FOLIO_STATE_RENAME_HALTED,
        FOLIO_STATE_RENAME_UNLOCKED};
    for (size_t index = 0; index < sizeof refusals / sizeof refusals[0]; ++index)
    {
        struct persistence_adapter refused = healthy_adapter();
        refused.recover_outcome = refusals[index];
        struct persistence_port port = port_for(&refused);
        struct appearance_port looks = looks_for(&dark_adapter);
        struct folio_state *blocked = nullptr;
        require(folio_state_create(test_ports(&port, &looks, &finder), &blocked) ==
                        expected[index] &&
                    blocked == nullptr && refused.note_scans == 0,
                "an unfinished recovery stops the startup before the scan");
    }
}

/* 語と方向だけを持ち、本文も選択も持たない（ADR 0023 の決定 3）。 */
static void verify_search_term(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(same_text(folio_state_search_term(state), "") &&
                folio_state_search_term_length(state) == 0 &&
                folio_state_search_direction(state) == SEARCH_DIRECTION_FORWARD,
            "a fresh state remembers no term and searches forward");
    require(folio_state_set_search_term(state, u"日報", 2) == FOLIO_STATE_READY &&
                same_text(folio_state_search_term(state), "日報") &&
                folio_state_search_term_length(state) == 6,
            "the term is kept as UTF-8");
    folio_state_set_search_direction(state, SEARCH_DIRECTION_BACKWARD);
    require(folio_state_search_direction(state) == SEARCH_DIRECTION_BACKWARD,
            "the direction is remembered");
    const char16_t lone[] = {u'a', 0xD800, u'\0'};
    require(folio_state_set_search_term(state, lone, 2) == FOLIO_STATE_SEARCH_MALFORMED &&
                same_text(folio_state_search_term(state), "日報"),
            "a broken term keeps the previous one");
    require(folio_state_set_search_term(state, u"", 0) == FOLIO_STATE_READY &&
                same_text(folio_state_search_term(state), "") &&
                folio_state_search_term_length(state) == 0,
            "an empty term is dropped");
    require(folio_state_set_search_term(state, u"ab", 2) == FOLIO_STATE_READY, "a second term");
    /* 語は 1 つだけ。#40 の絞り込み語は別の所有者で、ここには存在しない。 */
    require(folio_state_set_search_term(state, u"cd", 2) == FOLIO_STATE_READY &&
                same_text(folio_state_search_term(state), "cd"),
            "only one in-note term exists at a time");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_search_term(state), "cd") &&
                folio_state_search_direction(state) == SEARCH_DIRECTION_BACKWARD,
            "selecting a note touches neither the term nor the direction");
    folio_state_destroy(state);
}

/* 面の印が付いた行があるか（絞り込みで現在の文書の行が消えた証拠に使う）。 */
static bool any_selected_row(const struct folio_state *_Nonnull state)
{
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY, "layout");
    bool found = false;
    size_t count = drawer_layout_row_count(layout);
    for (size_t index = 0; index < count; ++index)
    {
        found = found || drawer_layout_row(layout, index).selected;
    }
    drawer_layout_destroy(layout);
    return found;
}

/* 本文は最初の空でない語で 1 回だけ読み、名前でも本文でも一致し、空語で元へ戻る
 * （FR-032 / ADR 0024 の決定 1〜3）。 */
static void verify_index_filter(void)
{
    static const char *const alt_scanned[] = {"four", "five", nullptr};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.rare_note = "four";
    adapter.rare_body = "the needle is here";
    test_adapter_second_notes(&adapter, "A", "{\"version\": 1, \"notes\": [\"four\", \"five\"]}",
                              alt_scanned);
    struct folio_state *state = ready_state(&adapter);
    require(!folio_state_filtering(state) && adapter.note_reads == 0,
            "no note is read before the first term");
    require(row_count(state) == 9, "the whole index is placed");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY,
            "a term filters the index");
    require(adapter.note_reads == 8, "every note was read exactly once");
    require(folio_state_filtering(state) && folio_state_index_filter_count(state) == 1,
            "one note matches the body");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "A/four"), "a collapsed category is opened for its match");
    require(test_index_filter_complete(state, u"NEEDLE", 6) == FOLIO_STATE_READY &&
                adapter.note_reads == 8,
            "a second term reads nothing again");
    require(folio_state_index_filter_count(state) == 1, "and ASCII case is ignored");
    require(test_index_filter_complete(state, u"three", 5) == FOLIO_STATE_READY, "a name term");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/C/three"), "the name is matched too");
    require(same_text(folio_state_index_filter_term(state), "three"), "the term is kept as UTF-8");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY &&
                !folio_state_filtering(state) && row_count(state) == 9,
            "an empty term restores the whole index");
    require(folio_state_index_filter_count(state) == 8, "and every note is visible again");
    folio_state_destroy(state);
}

/* 読めないノートは写しを持たないので一致しない。壊れた語は前の絞り込みを保つ（決定 1 / 2）。 */
static void verify_index_filter_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.missing_note = "two";
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"Hello", 5) == FOLIO_STATE_READY,
            "every readable body is searched");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/one/three/C/one/three"),
            "a note that could not be read never matches");
    const char16_t lone[] = {u'a', 0xD800, u'\0'};
    require(test_index_filter_complete(state, lone, 2) == FOLIO_STATE_SEARCH_MALFORMED,
            "a broken term is refused");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/one/three/C/one/three"),
            "and the previous filter is kept");
    require(same_text(folio_state_index_filter_term(state), "Hello"), "with the previous term");
    folio_state_destroy(state);
}

/* 絞り込み中は台帳を書く 4 つの意図を断り、色・閲覧・編集は許す（決定 4）。 */
static void verify_filtered_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY, "filter on");
    size_t writes = adapter.writes;
    size_t ledger_writes = adapter.ledger_writes;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_FILTERED, "no toggle");
    require(folio_state_set_category_expanded(state, 0, false, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_FILTERED,
            "no h/l");
    require(folio_state_move_category(state, 0, 1) == FOLIO_STATE_FILTERED, "no reorder");
    require(folio_state_move_note(state, at(0, 0), at(0, 1)) == FOLIO_STATE_FILTERED, "no move");
    require(adapter.writes == writes && adapter.ledger_writes == ledger_writes,
            "and nothing was written");
    struct rgb_color chosen = {.red = 1, .green = 2, .blue = 3};
    require(folio_state_recolor_category(state, 0, chosen) == FOLIO_STATE_READY,
            "the colour still changes while filtering");
    /* 絞り込みは台帳の expanded を触らない。色と同じ経路で書き戻される値が元のままであること
     * （絞り込み中に全カテゴリが展開して見えることは、書き戻す値ではない・決定 3 / 4）。 */
    require(adapter.writes == writes + 1 && adapter.written_expanded[0] &&
                !adapter.written_expanded[1] && adapter.written_expanded[2],
            "and the expansion written with it is the ledger's own, not the filtered view");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a note can still be read");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "and edited");
    require(same_text(folio_state_failure_line(FOLIO_STATE_FILTERED, FOLIO_LANGUAGE_JA),
                      "絞り込み中はこの操作ができません。絞り込みを解いてからやり直してください。"),
            "the refusal has one line");
    folio_state_destroy(state);
}

/* 現在の文書は一致しなくても閉じず、カーソルだけが最初に見える行へ移る（決定 5）。 */
static void verify_filter_keeps_document(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.rare_note = "three";
    adapter.rare_body = "the needle is here";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "and edit it");
    require(folio_state_scroll_drawer(state, scroll_metrics, 20) == FOLIO_STATE_READY, "scroll");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    expect_selection(state, "B/one", "the current note is not closed when it stops matching");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "and the mode stays");
    require(!any_selected_row(state), "no row carries the selection while its row is gone");
    require(scrolled_top(state) == 0, "the amount is reset on every change of the term");
    expect_cursor(state, "c0", "the cursor moves to the first visible row");
    /* 止まる行は B/three・A/three・C/three だけになる。 */
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j");
    expect_cursor(state, "n0.2", "the first stop is the match inside the first category");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j again");
    expect_cursor(state, "n1.2", "the next stop skips the notes that do not match");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j again");
    expect_cursor(state, "n2.2", "and lands in the last category");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "j at the end");
    expect_cursor(state, "n2.2", "the end does not move");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "k");
    expect_cursor(state, "n1.2", "backwards walks the same visible rows");
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "gg");
    expect_cursor(state, "n0.2", "gg is the first visible row");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "G");
    expect_cursor(state, "n2.2", "G is the last visible row");
    folio_state_destroy(state);
}

/* 保存と改名は写しと一致集合を更新し、絞り込みを保つ。語はノート内検索とは別（決定 4 / 7）。 */
static void verify_filter_follows_content(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    require(folio_state_index_filter_count(state) == 0, "nothing matches yet");
    require(folio_state_store_note(state, u"a needle appears", 16) == FOLIO_STATE_READY, "save");
    require(folio_state_index_filter_count(state) == 1, "the saved body now matches");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one"), "and it is the note that was saved");
    require(test_index_filter_complete(state, u"renamed", 7) == FOLIO_STATE_READY, "a new term");
    require(folio_state_index_filter_count(state) == 0, "no name holds it");
    struct note_name *name = accepted_note_name("renamed");
    require(folio_state_rename_note(state, name, u"a needle appears", 16) == FOLIO_STATE_READY,
            "rename the note");
    note_name_destroy(name);
    require(folio_state_index_filter_count(state) == 1, "the new name matches");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/renamed"), "and the copy travelled with the name");
    require(folio_state_set_search_term(state, u"zzz", 3) == FOLIO_STATE_READY, "an in-note term");
    require(folio_state_index_filter_count(state) == 1 &&
                same_text(folio_state_index_filter_term(state), "renamed"),
            "does not touch the filter");
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY, "a filter term");
    require(same_text(folio_state_search_term(state), "zzz"),
            "and the filter does not touch the in-note term");
    folio_state_destroy(state);
}

/* 新しく公開したノートも写しと一致集合に入る（決定 4）。 */
static void verify_filter_follows_new_note(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    require(folio_state_index_filter_count(state) == 0, "nothing matches yet");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a new untitled note");
    struct note_name *name = accepted_note_name("fresh");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"a needle here", 13) == FOLIO_STATE_READY,
            "the first save publishes it");
    note_name_destroy(name);
    require(folio_state_index_filter_count(state) == 1, "the published note matches");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/fresh"), "and it is the one that was created");
    folio_state_destroy(state);
}

/* カーソルの行が消えたとき、その行のカテゴリ行が「見えていて止まれる行」ならそこへ残り、
 * そうでなければ最初に見える行へ移る（ADR 0024 の補正 4・ADR 0015 の決定 2）。 */
static void verify_filter_cursor_lands_near(void)
{
    static const char *const alt_scanned[] = {"four", "five", nullptr};
    struct persistence_adapter adapter = healthy_adapter();
    /* C を four / five にして、C にだけ一致する語を作る。A は折り畳んだままの one / two / three。
     */
    test_adapter_second_notes(&adapter, "C", "{\"version\": 1, \"notes\": [\"four\", \"five\"]}",
                              alt_scanned);
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 1, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select A / one");
    expect_cursor(state, "n1.0", "the cursor follows the selection into the collapsed category");
    require(test_index_filter_complete(state, u"Hello", 5) == FOLIO_STATE_READY,
            "a term that every body holds");
    expect_cursor(state, "n1.0", "filtering opens A, so the cursor keeps its own row");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY, "clear the term");
    expect_cursor(state, "c1", "A closes again and the cursor stops on A's own row");
    expect_selection(state, "A/one", "the selection and the right pane never moved");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(test_index_filter_complete(state, u"four", 4) == FOLIO_STATE_READY,
            "a term only C holds");
    expect_cursor(state, "c2", "a cursor whose category is gone falls back to the first shown row");
    folio_state_destroy(state);
}

/* 改名を完了させたのが別の意図（再開）でも、写しと一致集合は新しい名前で作り直される
 * （ADR 0024 の補正 3。作り直しは adopt_rename の 1 か所）。 */
static void verify_filter_follows_resumed_rename(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    require(test_index_filter_complete(state, u"renamed", 7) == FOLIO_STATE_READY, "filter");
    require(folio_state_index_filter_count(state) == 0, "no name and no body holds the term");
    struct note_name *name = accepted_note_name("renamed");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "the rename stops after publishing its journal");
    note_name_destroy(name);
    require(folio_state_index_filter_count(state) == 0,
            "an unfinished rename changes neither the copy nor the set");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "another intent resumes and finishes the same rename");
    require(folio_state_index_filter_count(state) == 1, "the resumed rename rebuilds the set");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/renamed"), "with the copy travelling to the new name");
    folio_state_destroy(state);
}

/* 絞り込みを解いてから移した写しは、次の絞り込みで新しいカテゴリの下に現れる（決定 1）。 */
static void verify_filter_follows_move(void)
{
    static const char *const alt_scanned[] = {"four", "five", nullptr};
    struct persistence_adapter adapter = healthy_adapter();
    /* A は four / five にして、そこへ B の two を受け入れられるようにする（同名は移せない）。 */
    test_adapter_second_notes(&adapter, "A", "{\"version\": 1, \"notes\": [\"four\", \"five\"]}",
                              alt_scanned);
    adapter.rare_note = "two";
    adapter.rare_body = "the needle is here";
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    require(adapter.note_reads == 8, "every note is read once at the first term");
    char order[160] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two/C/two"), "the two bodies that hold the term");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY, "clear the term");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY,
            "B / two moves into A while nothing is filtered");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY, "filter again");
    require(adapter.note_reads == 8, "the copies are still the ones read at the first term");
    row_order(state, order, sizeof order);
    require(same_text(order, "A/two/C/two"),
            "the moved copy matches under its new category, and B has no match left");
    folio_state_destroy(state);
}

/* 設定の値の所有（ADR 0025 の決定 5）。無いファイルでは既定値で、起動時には書かない。 */
static void verify_settings_absent(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "absent settings start");
    require(adapter.settings_reads == 1, "the settings are read exactly once at startup");
    require(adapter.settings_writes == 0, "an absent settings file is not created at startup");
    require(!folio_state_number(state), "the default hides the line numbers");
    require(folio_state_settings_notice(state) == FOLIO_STATE_READY, "an absent file is no notice");
    folio_state_destroy(state);
}

static void verify_settings_loaded(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "stored settings start");
    require(folio_state_number(state), "the stored value is adopted");
    require(adapter.settings_writes == 0, "reading the settings never writes them");
    folio_state_destroy(state);
}

/* 変更は先に書いてから採用し、同じ値では書かない。 */
static void verify_set_number(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state");
    require(folio_state_set_number(state, false) == FOLIO_STATE_READY,
            "the same value is accepted");
    require(adapter.settings_writes == 0, "the same value is not written");
    require(folio_state_set_number(state, true) == FOLIO_STATE_READY, "showing is accepted");
    require(adapter.settings_writes == 1 && adapter.written_number, "the change is written once");
    require(folio_state_number(state), "the written value is adopted");
    require(folio_state_set_number(state, true) == FOLIO_STATE_READY, "showing twice");
    require(adapter.settings_writes == 1, "the same value is still not written");
    require(folio_state_set_number(state, false) == FOLIO_STATE_READY, "hiding is accepted");
    require(adapter.settings_writes == 2 && !adapter.written_number, "hiding is written");
    require(!folio_state_number(state), "hiding is adopted");
    folio_state_destroy(state);
}

static void verify_set_number_unwritable(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state");
    require(folio_state_set_number(state, true) == FOLIO_STATE_SETTINGS_STORE_FAILED,
            "an unwritable data/ refuses the change");
    require(adapter.settings_writes == 1, "the write was attempted");
    require(!folio_state_number(state), "the value did not change");
    folio_state_destroy(state);
}

/* 設定の変更は data/ のノートを動かさないので、未完了の改名を再試行しない
 * （保存・切替・並替・色・新規・別名保存だけが再試行する・ADR 0022 の決定 6）。 */
static void verify_set_number_ignores_resumed_rename(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "the rename stops after publishing its journal");
    note_name_destroy(name);
    size_t renames = adapter.renames;
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_set_number(state, true) == FOLIO_STATE_READY,
            "the settings change is accepted while a rename is unfinished");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY,
            "and so is the theme (it never goes through synchronize either)");
    require(adapter.renames == renames, "it never calls apply_rename, not even to resume");
    require(adapter.settings_writes == 2 && adapter.written_number &&
                adapter.written_theme == FOLIO_THEME_CHOICE_LIGHT,
            "and the settings are written once per change");
    require(folio_state_number(state), "the written value is adopted");
    struct rename_view pending = {.from = "", .to = ""};
    require(folio_state_rename_pending(state, &pending) && same_text(pending.to, "新しい名前"),
            "the held intent is still waiting for an intent that touches data/");
    folio_state_destroy(state);
}

/* 読めない設定では既定値で起動し、知らせを 1 つ持ち、変更を断って上書きしない（決定 6）。 */
static void verify_settings_unreadable(enum persistence_outcome read,
                                       const char *_Nonnull description)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = read;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            description);
    require(folio_state_settings_notice(state) == FOLIO_STATE_SETTINGS_UNREADABLE,
            "the notice is kept for the composition root");
    require(!folio_state_number(state), "an unreadable settings file starts from the default");
    require(folio_state_set_number(state, true) == FOLIO_STATE_SETTINGS_UNREADABLE,
            "the session refuses to change the settings");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) ==
                FOLIO_STATE_SETTINGS_UNREADABLE,
            "the theme is refused with the same value as the line numbers");
    require(folio_state_set_minimap(state, false) == FOLIO_STATE_SETTINGS_UNREADABLE &&
                folio_state_minimap(state),
            "unreadable settings refuse minimap changes");
    require(adapter.settings_writes == 0, "a file nobody could read is never overwritten");
    require(!folio_state_number(state), "the refused change left the value alone");
    require(folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM,
            "and the theme starts from the default too");
    /* ノートの閲覧・編集・保存は妨げない。 */
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "viewing still works");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing still works");
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_READY,
            "saving still works");
    folio_state_destroy(state);
}

/* 壊れた文書と未知の版は、偽のポートでも core の parse を通って MALFORMED になる。 */
static void verify_settings_shapes(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    adapter.settings_text = "{\"version\": 3, \"number\": true, \"theme\": \"dark\"}";
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "unknown version");
    require(folio_state_settings_notice(state) == FOLIO_STATE_SETTINGS_UNREADABLE,
            "an unknown version is a notice, not a startup failure");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    adapter.settings_text = "{\"version\": 1, \"theme\": \"dark\"}";
    port = port_for(&adapter);
    state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "unknown key");
    require(folio_state_settings_notice(state) == FOLIO_STATE_SETTINGS_UNREADABLE,
            "an unknown key is a notice too");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_OUT_OF_MEMORY;
    expect_failure(adapter, FOLIO_STATE_OUT_OF_MEMORY, "out of memory settings fail the startup");
}

/* ダークの本文の色。閲覧文書が新しい palette で作り直されたかを RTF の色表で見る。 */
static const char *const dark_body_color = "\\red239\\green228\\blue234;";
static const char *const light_body_color = "\\red36\\green19\\blue24;";

/* 選択は設定から来て、解決値は OS の値と合わせて決まる（ADR 0031 の決定 2 / 3）。 */
static void verify_theme_resolution(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    adapter.settings_text = "{\"version\": 2, \"number\": false, \"theme\": \"light\"}";
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "stored theme starts");
    require(folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_LIGHT, "the choice is adopted");
    require(folio_state_theme(state) == FOLIO_THEME_LIGHT,
            "an explicit choice ignores the dark operating system");
    require(strstr(folio_state_pane_rtf(state), light_body_color) != nullptr,
            "the empty pane already carries the chosen colours");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    port = port_for(&adapter);
    state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "default theme starts");
    require(folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM, "the default is system");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "system follows the operating system");
    require(strstr(folio_state_pane_rtf(state), dark_body_color) != nullptr,
            "the pane follows the resolved theme");
    folio_state_destroy(state);
}

/* 変更は先に書いてから採用し、同じ値では書かない。閲覧文書は新しい色で作り直す。 */
static void verify_set_theme(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a named document");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_SYSTEM) == FOLIO_STATE_READY,
            "the same choice is accepted");
    require(adapter.settings_writes == 0, "the same choice is not written");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY,
            "a new choice is accepted");
    require(adapter.settings_writes == 1 && adapter.written_theme == FOLIO_THEME_CHOICE_LIGHT,
            "the change is written once");
    require(folio_state_theme(state) == FOLIO_THEME_LIGHT, "the written choice is adopted");
    require(strstr(folio_state_pane_rtf(state), light_body_color) != nullptr,
            "the note is re-rendered with the new palette");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_DARK) == FOLIO_STATE_READY,
            "the choice can change while editing");
    require(strstr(folio_state_pane_rtf(state), dark_body_color) != nullptr,
            "the view document is rebuilt even in edit mode");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "the mode is unchanged");
    folio_state_destroy(state);
}

static void verify_set_theme_unwritable(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) ==
                FOLIO_STATE_SETTINGS_STORE_FAILED,
            "an unwritable data/ refuses the change");
    require(adapter.settings_writes == 1, "the write was attempted");
    require(folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM, "the choice is kept");
    require(strstr(folio_state_pane_rtf(state), dark_body_color) != nullptr,
            "and the pane keeps the old colours");
    folio_state_destroy(state);
}

/* OS の値を読み直す意図（決定 3 / 4）。設定は書かず、SYSTEM のときだけ解決値が動く。 */
static void verify_refresh_theme(void)
{
    struct appearance_adapter looks_adapter = {.theme = FOLIO_THEME_DARK};
    struct persistence_adapter adapter = healthy_adapter();
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&looks_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "it starts dark");
    looks_adapter.theme = FOLIO_THEME_LIGHT;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY, "the port is read again");
    require(folio_state_theme(state) == FOLIO_THEME_LIGHT, "system follows the new value");
    require(adapter.settings_writes == 0, "following the operating system writes nothing");
    require(strstr(folio_state_pane_rtf(state), light_body_color) != nullptr,
            "the pane follows too");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_DARK) == FOLIO_STATE_READY, "dark");
    looks_adapter.theme = FOLIO_THEME_DARK;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY, "reading again is harmless");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "an explicit choice does not move");
    looks_adapter.theme = FOLIO_THEME_LIGHT;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY, "and again");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "still the explicit choice");
    require(adapter.settings_writes == 1, "refreshing never writes the settings");
    folio_state_destroy(state);
}

/* 確保に失敗したときの不変（ADR 0031 の決定 3）。書き込みが OUT_OF_MEMORY を返す経路は
 * 偎のポートだけで踏める（閲覧文書の作り直しの失敗は allocation_tests.c が測定ビルドで踏む）。 */
static void verify_set_theme_out_of_memory(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a named document");
    const char *before = folio_state_pane_rtf(state);
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_OUT_OF_MEMORY,
            "a failing write is reported as out of memory");
    require(folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM,
            "the choice is unchanged");
    require(folio_state_theme(state) == FOLIO_THEME_DARK, "and so is the resolved theme");
    require(folio_state_pane_rtf(state) == before &&
                strstr(folio_state_pane_rtf(state), dark_body_color) != nullptr,
            "the view document is the very same one, still in the old colours");
    require(adapter.settings_writes == 1, "the write was attempted exactly once");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_SYSTEM) == FOLIO_STATE_READY,
            "the unchanged choice is still accepted without writing");
    require(adapter.settings_writes == 1, "and it did not write again");
    folio_state_destroy(state);
}

/* 言語は先に書いてから採用し、同じ値では書かない。閲覧文書は新しい face で作り直す
 * （ADR 0032 の決定 5）。色は動かない。 */
static void verify_set_language(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a named document");
    require(folio_state_language(state) == FOLIO_LANGUAGE_JA, "the default is Japanese");
    require(folio_state_set_language(state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "the same language is accepted");
    require(adapter.settings_writes == 0, "the same language is not written");
    require(folio_state_set_language(state, FOLIO_LANGUAGE_ZH_HANS) == FOLIO_STATE_READY,
            "a new language is accepted");
    require(adapter.settings_writes == 1 && adapter.written_language == FOLIO_LANGUAGE_ZH_HANS,
            "the change is written once");
    require(folio_state_language(state) == FOLIO_LANGUAGE_ZH_HANS, "and then adopted");
    require(strstr(folio_state_pane_rtf(state), ui_font_face(FOLIO_LANGUAGE_ZH_HANS)) != nullptr,
            "the view document is rebuilt with the new face");
    require(strstr(folio_state_pane_rtf(state), dark_body_color) != nullptr,
            "and keeps the colours it had");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "editing");
    require(folio_state_set_language(state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_READY,
            "the language can change while editing");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "the mode is unchanged");
    require(strstr(folio_state_pane_rtf(state), ui_font_face(FOLIO_LANGUAGE_EN)) != nullptr,
            "the view document is rebuilt even in edit mode");
    /* 文言も言語に従う（表を引くのは core だが、値の所有者は application である）。 */
    require(
        same_text(folio_state_failure_line(FOLIO_STATE_NOT_EDITING, folio_state_language(state)),
                  "Not in edit mode."),
        "the failure line follows the language the state holds");
    folio_state_destroy(state);
}

/* 設定は md・台帳と独立なので synchronize を通さない（未完了の改名を再開しない）。 */
static void verify_set_language_ignores_resumed_rename(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "the rename stops after publishing its journal");
    note_name_destroy(name);
    size_t renames = adapter.renames;
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_set_language(state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_READY,
            "the language change is accepted while a rename is unfinished");
    require(adapter.renames == renames, "it never calls apply_rename, not even to resume");
    folio_state_destroy(state);
}

static void verify_set_language_unwritable(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "a named document");
    const char *before = folio_state_pane_rtf(state);
    require(folio_state_set_language(state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_SETTINGS_STORE_FAILED,
            "an unwritable data/ refuses the change");
    require(adapter.settings_writes == 1, "the write was attempted");
    require(folio_state_language(state) == FOLIO_LANGUAGE_JA, "the language is kept");
    require(folio_state_pane_rtf(state) == before, "and so is the very view document");
    adapter.settings_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_set_language(state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_OUT_OF_MEMORY,
            "a failing write is reported as out of memory");
    require(folio_state_language(state) == FOLIO_LANGUAGE_JA, "and still changes nothing");
    folio_state_destroy(state);
}

/* 読めない設定のセッションは言語も断り、上書きもしない（ADR 0025 の決定 6）。 */
static void verify_set_language_unreadable(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_MALFORMED;
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "an unreadable settings file still starts");
    require(folio_state_set_language(state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_SETTINGS_UNREADABLE,
            "the session refuses to change the language");
    require(adapter.settings_writes == 0, "a file nobody could read is never overwritten");
    require(folio_state_language(state) == FOLIO_LANGUAGE_JA, "the default is kept");
    folio_state_destroy(state);
}

/* 起動時に読んだ言語は、最初の閲覧文書の face から効いている（決定 5）。 */
static void verify_language_at_start(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    adapter.settings_text = "{\"version\": 3, \"number\": true, \"theme\": \"dark\", "
                            "\"language\": \"zh-Hans\"}";
    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "stored settings start");
    require(folio_state_language(state) == FOLIO_LANGUAGE_ZH_HANS, "the stored language is read");
    require(strstr(folio_state_pane_rtf(state), ui_font_face(FOLIO_LANGUAGE_ZH_HANS)) != nullptr,
            "the first view document already carries the face of that language");
    require(adapter.settings_writes == 0, "reading the settings never writes them");
    folio_state_destroy(state);
}

static void verify_named_theme(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct appearance_adapter appearance = {.theme = FOLIO_THEME_DARK};
    struct persistence_port persistence = port_for(&adapter);
    struct appearance_port looks = looks_for(&appearance);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&persistence, &looks, &finder), &state) ==
                FOLIO_STATE_READY,
            "named theme fixture starts");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "named theme fixture is editing a document");
    const char *body = folio_state_pane_text(state);
    size_t writes = adapter.note_writes;
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_DRACULA) == FOLIO_STATE_READY,
            "a named palette can be applied while editing");
    require(adapter.settings_writes == 1 && adapter.written_theme == FOLIO_THEME_CHOICE_DRACULA &&
                folio_state_theme(state) == FOLIO_THEME_DRACULA,
            "the named choice is written and adopted once");
    require(strstr(folio_state_pane_rtf(state), "\\red248\\green248\\blue242;") != nullptr,
            "the view document carries Dracula's body color");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_pane_text(state) == body && adapter.note_writes == writes,
            "theme application neither saves nor replaces the application body");
    appearance.theme = FOLIO_THEME_LIGHT;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_DRACULA &&
                folio_state_system_theme(state) == FOLIO_THEME_LIGHT &&
                adapter.settings_writes == 1,
            "OS refresh updates the system preview while preserving the explicit palette");
    const char *rtf = folio_state_pane_rtf(state);
    adapter.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_NORD) ==
                    FOLIO_STATE_SETTINGS_STORE_FAILED &&
                folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_DRACULA &&
                folio_state_pane_rtf(state) == rtf && adapter.note_writes == writes,
            "a failed named-theme write leaves the adopted palette and document untouched");
    folio_state_destroy(state);
}

static void verify_named_theme_restart(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    adapter.settings_text =
        "{\"version\":4,\"number\":true,\"theme\":\"dracula\",\"language\":\"en\"}";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_DRACULA &&
                folio_state_theme(state) == FOLIO_THEME_DRACULA && folio_state_number(state) &&
                folio_state_language(state) == FOLIO_LANGUAGE_EN && adapter.settings_writes == 0,
            "startup restores a named theme and other settings without writing them");
    folio_state_destroy(state);
    adapter.settings_text =
        "{\"version\":4,\"number\":true,\"theme\":\"future-theme\",\"language\":\"en\"}";
    state = ready_state(&adapter);
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_DRACULA) ==
                    FOLIO_STATE_SETTINGS_UNREADABLE &&
                adapter.settings_writes == 0,
            "an unknown persisted theme cannot be overwritten by choosing another theme");
    folio_state_destroy(state);
}

static struct appearance_snapshot contrast_sample(bool light)
{
    return (struct appearance_snapshot){
        .theme = FOLIO_THEME_DARK,
        .contrast = FOLIO_CONTRAST_HIGH,
        .window = light ? (struct rgb_color){239, 244, 231} : (struct rgb_color){13, 24, 35},
        .window_text = light ? (struct rgb_color){17, 28, 39} : (struct rgb_color){241, 232, 223},
        .button = light ? (struct rgb_color){221, 232, 243} : (struct rgb_color){31, 42, 53},
        .button_text = light ? (struct rgb_color){23, 34, 45} : (struct rgb_color){233, 244, 255},
        .highlight = light ? (struct rgb_color){45, 56, 67} : (struct rgb_color){211, 222, 233},
        .highlight_text =
            light ? (struct rgb_color){245, 236, 227} : (struct rgb_color){15, 26, 37},
    };
}

static struct folio_state *_Nonnull appearance_state(struct persistence_adapter *_Nonnull files,
                                                     struct appearance_adapter *_Nonnull looks)
{
    struct persistence_port port = port_for(files);
    struct appearance_port appearance = looks_for(looks);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &appearance, &finder), &state) ==
                FOLIO_STATE_READY,
            "appearance fixture starts without writing data");
    return state;
}

static void verify_contrast_pairs(const struct folio_state *_Nonnull state,
                                  struct appearance_snapshot expected)
{
    const struct theme_palette *palette = folio_state_palette(state);
    require(palette->contrast == FOLIO_CONTRAST_HIGH &&
                rgb_color_equal(palette->pane, expected.window) &&
                rgb_color_equal(palette->editor_text, expected.window_text) &&
                rgb_color_equal(palette->category_text, expected.window_text),
            "body and category labels use the user window pair");
    require(rgb_color_equal(palette->chip_background, expected.button) &&
                rgb_color_equal(palette->chip_text, expected.button_text) &&
                rgb_color_equal(palette->status_background, expected.button) &&
                rgb_color_equal(palette->status_text, expected.button_text) &&
                rgb_color_equal(palette->selected_background, expected.highlight) &&
                rgb_color_equal(palette->selected_text, expected.highlight_text),
            "buttons and selected rows retain their own OS foreground/background pairs");
    require(rgb_color_equal(palette->rtf.heading, expected.window_text) &&
                rgb_color_equal(palette->rtf.link, expected.window_text) &&
                rgb_color_equal(palette->rtf.muted, expected.window_text) &&
                rgb_color_equal(palette->rtf.code_text, expected.window_text) &&
                rgb_color_equal(palette->rtf.code_background, expected.window),
            "Markdown decoration cannot introduce a second contrast palette");
    require(rgb_color_equal(palette->rtf.code_keyword, expected.window_text) &&
                rgb_color_equal(palette->rtf.code_string, expected.window_text) &&
                rgb_color_equal(palette->rtf.code_comment, expected.window_text) &&
                rgb_color_equal(palette->rtf.code_number, expected.window_text),
            "code syntax colors respect the system window text");
    require(rgb_color_equal(palette->dialog_surface, expected.window) &&
                rgb_color_equal(palette->dialog_secondary, expected.window_text) &&
                palette->dialog_backdrop_alpha == 255,
            "modal text uses the window pair and its backdrop is opaque");
}

static void verify_contrast_transition(enum folio_theme_choice choice)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK};
    struct folio_state *state = appearance_state(&files, &looks);
    require(folio_state_set_theme(state, choice) == FOLIO_STATE_READY, "stored choice");
    size_t settings_writes = files.settings_writes;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "open an editable note");
    const char *body = folio_state_pane_text(state);
    looks.snapshot = contrast_sample(false);
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY, "normal to high contrast");
    verify_contrast_pairs(state, looks.snapshot);
    require(strstr(folio_state_pane_rtf(state), "\\red241\\green232\\blue223;") != nullptr,
            "the prepared RTF uses the same arbitrary dark foreground");
    uint64_t revision = folio_state_palette_revision(state);
    looks.snapshot = contrast_sample(true);
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_palette_revision(state) != revision,
            "HC color changes are detected while the OS light/dark enum stays equal");
    verify_contrast_pairs(state, looks.snapshot);
    require(strstr(folio_state_pane_rtf(state), "\\red17\\green28\\blue39;") != nullptr,
            "light RTF and visible palette move together");
    const char *rendered = folio_state_pane_rtf(state);
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_pane_rtf(state) == rendered,
            "duplicate OS notifications do not rebuild the document");
    looks.snapshot.contrast = FOLIO_CONTRAST_STANDARD;
    looks.theme = FOLIO_THEME_LIGHT;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                theme_palette_equal(
                    folio_state_palette(state),
                    folio_theme_palette(folio_theme_choice_resolve(choice, looks.theme))),
            "OFF restores the saved selection against the current OS mode");
    require(folio_state_theme_choice(state) == choice && folio_state_pane_text(state) == body &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                files.settings_writes == settings_writes && files.note_writes == 0 &&
                files.archives == 0 && files.writes == 0,
            "OS appearance never saves settings, category colors, body or history");
    folio_state_destroy(state);
}

static void verify_contrast_failures(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK,
                                       .outcome = APPEARANCE_UNAVAILABLE};
    struct folio_state *state = appearance_state(&files, &looks);
    require(folio_state_appearance_notice(state) == FOLIO_STATE_APPEARANCE_UNAVAILABLE &&
                folio_state_system_theme(state) == FOLIO_THEME_LIGHT && files.settings_writes == 0,
            "first failure has an explicit notice and standard light fallback");
    looks.outcome = APPEARANCE_READY;
    looks.snapshot = contrast_sample(false);
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_appearance_notice(state) == FOLIO_STATE_READY,
            "recovery clears notice");
    const char *before = folio_state_pane_rtf(state);
    uint64_t revision = folio_state_palette_revision(state);
    looks.outcome = APPEARANCE_UNAVAILABLE;
    looks.snapshot = contrast_sample(true);
    require(folio_state_refresh_theme(state) == FOLIO_STATE_APPEARANCE_UNAVAILABLE &&
                folio_state_palette_revision(state) == revision &&
                folio_state_pane_rtf(state) == before,
            "failed retrieval cannot be interpreted as OFF or replace a valid document");
    verify_contrast_pairs(state, contrast_sample(false));
    looks.outcome = APPEARANCE_READY;
    allocation_probe_fail_at(1);
    enum folio_state_outcome result = folio_state_refresh_theme(state);
    allocation_probe_fail_at(0);
    require(result == FOLIO_STATE_READY || result == FOLIO_STATE_OUT_OF_MEMORY,
            "typed preparation result");
    if (result == FOLIO_STATE_OUT_OF_MEMORY)
    {
        require(folio_state_palette_revision(state) == revision &&
                    folio_state_pane_rtf(state) == before,
                "RTF preparation failure also keeps the entire valid snapshot");
        verify_contrast_pairs(state, contrast_sample(false));
    }
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY,
            "retry can adopt the new snapshot");
    verify_contrast_pairs(state, contrast_sample(true));
    folio_state_destroy(state);
}

static void verify_theme_choice_during_contrast(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK,
                                       .snapshot = contrast_sample(false)};
    struct folio_state *state = appearance_state(&files, &looks);
    uint64_t revision = folio_state_palette_revision(state);
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_NORD) == FOLIO_STATE_READY &&
                files.written_theme == FOLIO_THEME_CHOICE_NORD &&
                folio_state_palette_revision(state) == revision,
            "choosing a fixed theme stores it but cannot override OS high contrast");
    verify_contrast_pairs(state, looks.snapshot);
    files.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_DRACULA) ==
                    FOLIO_STATE_SETTINGS_STORE_FAILED &&
                folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_NORD,
            "a failed settings write still cannot adopt a new saved selection");
    looks.snapshot.contrast = FOLIO_CONTRAST_STANDARD;
    require(
        folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
            theme_palette_equal(folio_state_palette(state), folio_theme_palette(FOLIO_THEME_NORD)),
        "OFF returns to the theme chosen during HC");
    folio_state_destroy(state);
}

void run_system_contrast_tests(void)
{
    finder = test_regex_port();
    verify_contrast_transition(FOLIO_THEME_CHOICE_SYSTEM);
    verify_contrast_transition(FOLIO_THEME_CHOICE_DRACULA);
    verify_contrast_failures();
    verify_theme_choice_during_contrast();
    verify_refresh_theme();
}

static void verify_theme_preview_transaction(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct folio_state *state = ready_state(&files);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "preview with an editable document");
    const char *body = folio_state_pane_text(state);
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_LIGHT &&
                folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM &&
                strstr(folio_state_pane_rtf(state), light_body_color) != nullptr,
            "candidate changes the shared palette and RTF, not the saved theme");
    const char *pane = folio_state_pane_rtf(state);
    uint64_t revision = folio_state_palette_revision(state);
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY &&
                folio_state_pane_rtf(state) == pane &&
                folio_state_palette_revision(state) == revision && files.settings_writes == 0,
            "repeated candidate does no rendering or writing");
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY &&
                files.settings_writes == 1 && folio_state_palette_revision(state) == revision,
            "confirming the preview writes once without changing its palette");
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_NORD) == FOLIO_STATE_READY &&
                folio_state_preview_theme(state, folio_state_theme_choice(state)) ==
                    FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_LIGHT,
            "cancel returns to the latest confirmation, not the opening choice");
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_DARK) == FOLIO_STATE_READY &&
                folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_LIGHT && files.settings_writes == 1,
            "confirming the saved value restores it without writing again");
    require(folio_state_pane_text(state) == body &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT && files.note_writes == 0 &&
                files.archives == 0 && files.writes == 0,
            "preview and cancel keep document, mode and unrelated files");
    folio_state_destroy(state);
}

static void verify_theme_preview_failures(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct folio_state *state = ready_state(&files);
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY,
            "preview before a failed confirmation");
    const char *pane = folio_state_pane_rtf(state);
    files.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_theme(state, FOLIO_THEME_CHOICE_LIGHT) ==
                    FOLIO_STATE_SETTINGS_STORE_FAILED &&
                folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM &&
                folio_state_pane_rtf(state) == pane,
            "failed confirmation retains preview and saved baseline");
    uint64_t revision = folio_state_palette_revision(state);
    allocation_probe_fail_at(1);
    enum folio_state_outcome result =
        folio_state_preview_theme(state, folio_state_theme_choice(state));
    allocation_probe_fail_at(0);
    require(result == FOLIO_STATE_READY || result == FOLIO_STATE_OUT_OF_MEMORY,
            "cancel reports a typed preparation outcome");
    if (result == FOLIO_STATE_OUT_OF_MEMORY)
    {
        require(folio_state_pane_rtf(state) == pane &&
                    folio_state_palette_revision(state) == revision &&
                    folio_state_theme(state) == FOLIO_THEME_LIGHT,
                "failed cancel keeps the complete preview for retry");
    }
    require(folio_state_preview_theme(state, folio_state_theme_choice(state)) ==
                    FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_DARK && files.settings_writes == 1,
            "cancel retries without another settings write");
    folio_state_destroy(state);
}

static void verify_theme_preview_contrast(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK,
                                       .snapshot = contrast_sample(false)};
    struct folio_state *state = appearance_state(&files, &looks);
    uint64_t revision = folio_state_palette_revision(state);
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_NORD) == FOLIO_STATE_READY &&
                folio_state_palette_revision(state) == revision && files.settings_writes == 0,
            "HC keeps OS colors while the candidate moves");
    verify_contrast_pairs(state, looks.snapshot);
    looks.snapshot.contrast = FOLIO_CONTRAST_STANDARD;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_NORD &&
                folio_state_theme_choice(state) == FOLIO_THEME_CHOICE_SYSTEM,
            "HC off resolves the preview rather than the saved choice");
    require(folio_state_preview_theme(state, folio_state_theme_choice(state)) == FOLIO_STATE_READY,
            "cancel back to OS-following choice");
    looks.theme = FOLIO_THEME_LIGHT;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_LIGHT && files.settings_writes == 0,
            "cancelled system choice follows later OS changes without persistence");
    require(folio_state_preview_theme(state, FOLIO_THEME_CHOICE_LIGHT) == FOLIO_STATE_READY,
            "fixed candidate can share the current OS palette");
    looks.theme = FOLIO_THEME_DARK;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_theme(state) == FOLIO_THEME_LIGHT,
            "equal palettes still adopt semantic choice for later OS refresh");
    folio_state_destroy(state);
}

static void verify_motion_observation(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK};
    struct folio_state *state = appearance_state(&files, &looks);
    require(folio_state_motion(state) && !folio_state_motion_available(state) &&
                !folio_state_motion_enabled(state) && files.settings_writes == 0,
            "unknown OS animation keeps the saved default but is static");
    uint64_t revision = folio_state_palette_revision(state);
    const char *pane = folio_state_pane_rtf(state);
    const enum appearance_motion observations[] = {
        APPEARANCE_MOTION_ALLOWED, APPEARANCE_MOTION_DISABLED, APPEARANCE_MOTION_UNKNOWN};
    const bool available[] = {true, false, false};
    for (size_t at = 0; at < sizeof observations / sizeof observations[0]; ++at)
    {
        looks.snapshot.motion = observations[at];
        require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                    folio_state_motion_enabled(state) == available[at] &&
                    folio_state_motion_available(state) == available[at] &&
                    folio_state_palette_revision(state) == revision &&
                    folio_state_pane_rtf(state) == pane && folio_state_motion(state),
                "motion-only OS updates change neither body nor palette nor saved choice");
    }
    looks.snapshot = contrast_sample(false);
    looks.snapshot.motion = APPEARANCE_MOTION_ALLOWED;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                !folio_state_motion_available(state),
            "high contrast overrides OS animation ON");
    looks.snapshot.contrast = FOLIO_CONTRAST_STANDARD;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_motion_enabled(state),
            "leaving high contrast restores the saved ON");
    looks.outcome = APPEARANCE_UNAVAILABLE;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_APPEARANCE_UNAVAILABLE &&
                !folio_state_motion_available(state) && folio_state_motion(state),
            "failed observation cannot retain an earlier motion permission");
    looks.outcome = APPEARANCE_READY;
    require(folio_state_refresh_theme(state) == FOLIO_STATE_READY &&
                folio_state_motion_enabled(state) && files.settings_writes == 0,
            "observation recovery restores eligibility without saving");
    folio_state_destroy(state);
}

static void verify_motion_setting(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK,
                                       .snapshot.motion = APPEARANCE_MOTION_ALLOWED};
    struct folio_state *state = appearance_state(&files, &looks);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "motion setting EDIT fixture");
    const char *pane = folio_state_pane_rtf(state);
    require(folio_state_set_motion(state, true) == FOLIO_STATE_READY && files.settings_writes == 0,
            "the same motion choice is not saved");
    files.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_motion(state, false) == FOLIO_STATE_SETTINGS_STORE_FAILED &&
                folio_state_motion_enabled(state),
            "failed save preserves effective motion ON");
    files.settings_write_outcome = PERSISTENCE_STORED;
    require(folio_state_set_motion(state, false) == FOLIO_STATE_READY &&
                !folio_state_motion(state) && !files.written_motion &&
                folio_state_motion_available(state) && !folio_state_motion_enabled(state),
            "saved motion OFF is independent of OS eligibility");
    require(folio_state_set_minimap(state, false) == FOLIO_STATE_READY && !files.written_motion &&
                !folio_state_motion(state),
            "another saved option keeps motion OFF");
    require(folio_state_set_motion(state, true) == FOLIO_STATE_READY && files.written_motion &&
                folio_state_motion_enabled(state) && folio_state_pane_rtf(state) == pane &&
                files.note_writes == 0 && files.archives == 0,
            "motion changes preserve the EDIT document and history");
    folio_state_destroy(state);
}

static void verify_motion_restart(void)
{
    struct persistence_adapter files = healthy_adapter();
    files.settings_outcome = PERSISTENCE_LOADED;
    files.settings_text = "{\"version\":7,\"number\":true,\"theme\":\"dark\",\"language\":\"en\","
                          "\"minimap\":true,\"note_order\":\"manual\",\"motion\":false}";
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK,
                                       .snapshot.motion = APPEARANCE_MOTION_ALLOWED};
    struct folio_state *state = appearance_state(&files, &looks);
    require(!folio_state_motion(state) && folio_state_motion_available(state) &&
                !folio_state_motion_enabled(state) && files.settings_writes == 0,
            "restart reads motion OFF without rewriting the file");
    folio_state_destroy(state);
    files.settings_outcome = PERSISTENCE_MALFORMED;
    state = appearance_state(&files, &looks);
    require(folio_state_set_motion(state, false) == FOLIO_STATE_SETTINGS_UNREADABLE &&
                files.settings_writes == 0,
            "unreadable settings cannot be replaced by motion");
    folio_state_destroy(state);
}

static void verify_motion_allocation_state(void)
{
    struct persistence_adapter files = healthy_adapter();
    struct appearance_adapter looks = {.theme = FOLIO_THEME_DARK,
                                       .snapshot.motion = APPEARANCE_MOTION_ALLOWED};
    struct folio_state *state = appearance_state(&files, &looks);
    allocation_probe_fail_at(1);
    enum folio_state_outcome result = folio_state_set_motion(state, false);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    if (calls != 0)
        require(result == FOLIO_STATE_OUT_OF_MEMORY && folio_state_motion_enabled(state) &&
                    files.settings_writes == 0,
                "motion OOM stops before any save or adoption");
    if (calls == 0)
        require(result == FOLIO_STATE_READY && !folio_state_motion_enabled(state),
                "normal motion save succeeds without failure injection");
    require(folio_state_set_motion(state, true) == FOLIO_STATE_READY, "restore saved motion ON");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "palette OOM has a real rendered document");
    const char *pane = folio_state_pane_rtf(state);
    looks.snapshot = contrast_sample(false);
    looks.snapshot.motion = APPEARANCE_MOTION_ALLOWED;
    allocation_probe_fail_at(1);
    result = folio_state_refresh_theme(state);
    calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(!folio_state_motion_enabled(state) && folio_state_motion(state),
            "palette adoption failure or high contrast both settle motion without saving OFF");
    if (calls != 0)
        require(result == FOLIO_STATE_OUT_OF_MEMORY && folio_state_pane_rtf(state) == pane,
                "palette OOM keeps the previous document while motion becomes static");
    if (calls == 0)
        require(result == FOLIO_STATE_READY, "normal palette adoption succeeds");
    folio_state_destroy(state);
}

void run_motion_state_tests(void)
{
    verify_motion_observation();
    verify_motion_setting();
    verify_motion_restart();
    verify_motion_allocation_state();
}

static void verify_set_minimap(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_minimap(state), "default minimap is visible");
    require(folio_state_set_minimap(state, true) == FOLIO_STATE_READY &&
                adapter.settings_writes == 0,
            "unchanged minimap is not written");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "editable note");
    const char *pane = folio_state_pane_rtf(state);
    require(folio_state_set_minimap(state, false) == FOLIO_STATE_READY &&
                !folio_state_minimap(state) && !adapter.written_minimap &&
                adapter.settings_writes == 1 && folio_state_pane_rtf(state) == pane,
            "hiding commits once without rebuilding the body");
    require(folio_state_set_number(state, true) == FOLIO_STATE_READY &&
                !folio_state_minimap(state) && !adapter.written_minimap && adapter.written_number,
            "number change preserves hidden preference");
    require(folio_state_set_minimap(state, true) == FOLIO_STATE_READY &&
                folio_state_number(state) && adapter.written_minimap &&
                adapter.settings_writes == 3 && folio_state_pane_rtf(state) == pane,
            "showing preserves existing settings and body");
    folio_state_destroy(state);
}

static void verify_minimap_write_failure(enum persistence_outcome failure,
                                         enum folio_state_outcome expected)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    adapter.settings_write_outcome = failure;
    require(folio_state_set_minimap(state, false) == expected && folio_state_minimap(state) &&
                adapter.settings_writes == 1,
            "failed storage preserves visible preference");
    folio_state_destroy(state);
}

static void verify_minimap_restart(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.settings_outcome = PERSISTENCE_LOADED;
    adapter.settings_text = "{\"version\":5,\"number\":true,\"theme\":\"dracula\","
                            "\"language\":\"en\",\"minimap\":false}";
    struct folio_state *state = ready_state(&adapter);
    require(!folio_state_minimap(state) && folio_state_number(state) &&
                folio_state_language(state) == FOLIO_LANGUAGE_EN && adapter.settings_writes == 0,
            "restart adopts hidden preference without writing");
    folio_state_destroy(state);
}

void run_minimap_state_tests(void)
{
    verify_set_minimap();
    verify_minimap_restart();
    verify_minimap_write_failure(PERSISTENCE_UNWRITABLE, FOLIO_STATE_SETTINGS_STORE_FAILED);
    verify_minimap_write_failure(PERSISTENCE_OUT_OF_MEMORY, FOLIO_STATE_OUT_OF_MEMORY);
    verify_settings_unreadable(PERSISTENCE_MALFORMED, "malformed settings protect map preference");
    verify_settings_unreadable(PERSISTENCE_UNREADABLE,
                               "unreadable settings protect map preference");
    verify_set_number();
    verify_set_number_unwritable();
}

static void verify_settings(void)
{
    run_system_contrast_tests();
    run_minimap_state_tests();
    verify_theme_preview_transaction();
    verify_theme_preview_failures();
    verify_theme_preview_contrast();
    verify_named_theme();
    verify_named_theme_restart();
    verify_settings_absent();
    verify_settings_loaded();
    verify_theme_resolution();
    verify_set_theme();
    verify_set_theme_unwritable();
    verify_set_theme_out_of_memory();
    verify_refresh_theme();
    verify_set_number();
    verify_set_number_unwritable();
    verify_set_number_ignores_resumed_rename();
    verify_settings_unreadable(PERSISTENCE_MALFORMED, "malformed settings still start");
    verify_settings_unreadable(PERSISTENCE_UNREADABLE, "unreadable settings still start");
    verify_settings_shapes();
    verify_set_language();
    verify_set_language_ignores_resumed_rename();
    verify_set_language_unwritable();
    verify_set_language_unreadable();
    verify_language_at_start();
}

/* 改名・検索・絞り込み・設定の単位（run_state_tests を C-012 の 60 行に収めるための束ね）。 */
/* 履歴を持つノート（B / one）を閲覧中の状態。 */
static struct folio_state *_Nonnull history_state(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = port_for(adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    struct folio_state *state = nullptr;
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state for history");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / one");
    return state;
}

static void verify_bom_no_write(void)
{
    const char *raw = "\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF"
                      "A";
    const char16_t wide[] = {0xFEFF, 0xFEFF, 0xFEFF, u'A'};
    for (size_t signatures = 0; signatures <= 4; ++signatures)
    {
        struct persistence_adapter adapter = healthy_adapter();
        adapter.note_body = raw + (4 - signatures) * 3;
        struct folio_state *state = history_state(&adapter);
        size_t literals = signatures == 0 ? 0 : signatures - 1;
        const char16_t *edited = wide + 3 - literals;
        require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "BOM enters edit");
        enum folio_note_change change = FOLIO_NOTE_CHANGED;
        require(folio_state_note_changed(state, edited, literals + 1, &change) ==
                        FOLIO_STATE_READY &&
                    change == FOLIO_NOTE_SAME,
                "unchanged FEFF body is SAME");
        require(folio_state_store_note(state, edited, literals + 1) == FOLIO_STATE_READY &&
                    folio_state_end_edit(state, edited, literals + 1) == FOLIO_STATE_READY &&
                    adapter.note_writes == 0 && adapter.archives == 0,
                "no write or history for signatures zero through four");
        folio_state_destroy(state);
    }
}

static void verify_bom_edit_save(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF"
                        "A";
    struct folio_state *state = history_state(&adapter);
    require(test_index_filter_complete(state, u"\uFEFF\uFEFFA", 3) == FOLIO_STATE_READY &&
                any_selected_row(state),
            "initial search copy retains two literal FEFF");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "BOM edit fixture");
    enum folio_note_change change = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, u"\uFEFF\uFEFFAx", 4, &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_CHANGED,
            "actual edit remains CHANGED");
    require(folio_state_store_note(state, u"\uFEFF\uFEFFAx", 4) == FOLIO_STATE_READY &&
                adapter.note_writes == 1 && adapter.archives == 1 &&
                same_text(adapter.written_body, "\xEF\xBB\xBF\xEF\xBB\xBF"
                                                "Ax") &&
                same_text(folio_state_pane_text(state), adapter.written_body),
            "save and application body keep literal FEFF");
    require(test_index_filter_complete(state, u"\uFEFF\uFEFFAx", 4) == FOLIO_STATE_READY &&
                any_selected_row(state),
            "saved search copy retains two literal FEFF");
    folio_state_destroy(state);
}

static void verify_bom_save_as(void)
{
    for (size_t edited = 0; edited < 2; ++edited)
    {
        struct persistence_adapter adapter = healthy_adapter();
        adapter.note_body = "\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF"
                            "A";
        struct folio_state *state = history_state(&adapter);
        if (edited == 1)
        {
            require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "BOM save as edit");
        }
        struct note_name *name = accepted_note_name("bom-copy");
        struct note_destination destination = {.category = 0, .name = name};
        require(folio_state_store_new(state, &destination, u"\uFEFF\uFEFFAx", 4) ==
                        FOLIO_STATE_READY &&
                    adapter.creates == 1 && adapter.archives == 0 && adapter.note_writes == 0,
                "VIEW and EDIT save as only create new file");
        const char *expected = edited == 0 ? "\xEF\xBB\xBF\xEF\xBB\xBF"
                                             "A"
                                           : "\xEF\xBB\xBF\xEF\xBB\xBF"
                                             "Ax";
        require(same_text(adapter.written_body, expected) &&
                    same_text(folio_state_pane_text(state), expected),
                "save as copies literal source in VIEW and literal editor in EDIT");
        note_name_destroy(name);
        folio_state_destroy(state);
    }
}

static void verify_bom_history(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "current";
    adapter.history[1] = "\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF"
                         "A";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "BOM history opened");
    const struct note_text *body = folio_state_history_body(state, 0);
    require(body != nullptr && same_text(note_text_bytes(body), "\xEF\xBB\xBF\xEF\xBB\xBF"
                                                                "A"),
            "history file consumes one signature only");
    require(folio_state_restore_history(state, 0) == FOLIO_STATE_READY &&
                adapter.note_writes == 0 && adapter.archives == 0,
            "restore is initially unsaved");
    require(folio_state_store_note(state, u"\uFEFF\uFEFFA", 3) == FOLIO_STATE_READY &&
                same_text(adapter.written_body, "\xEF\xBB\xBF\xEF\xBB\xBF"
                                                "A"),
            "restored editor keeps literal FEFF on save");
    folio_state_destroy(state);
}

void run_bom_state_tests(void)
{
    finder = test_regex_port();
    verify_bom_no_write();
    verify_bom_edit_save();
    verify_bom_save_as();
    verify_bom_history();
}

static void expect_history_row(const struct folio_state *_Nonnull state, size_t index,
                               size_t version, const char *_Nonnull first_line)
{
    struct history_row_view row = {};
    require(folio_state_history_row(state, index, &row), "the row exists");
    require(row.version == version, "rows keep the version number");
    require(row.readable, "the row is readable");
    require(row.first_line_length == strlen(first_line) &&
                memcmp(row.first_line, first_line, row.first_line_length) == 0,
            "the row shows the first non-empty line");
    require(folio_state_history_body(state, index) != nullptr, "a readable row has a body");
}

/* 5 版すべて・問い合わせ・閲覧中の restore・close（ADR 0038 の検証 1）。 */
static void verify_history_all(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    const char *const bodies[] = {
        "", "\n# first\nbody", "second", "\r\n  third  \r\n", "fourth", "fifth"};
    for (size_t version = 1; version <= 5; ++version)
    {
        adapter.history[version] = bodies[version];
    }
    struct folio_state *state = history_state(&adapter);
    require(folio_state_history_count(state) == 0, "no list before opening");
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "open five versions");
    require(adapter.history_reads == 5, "every version is read once");
    require(same_text(adapter.history_category, "B") && same_text(adapter.history_note, "one"),
            "the current note's history is read");
    require(folio_state_history_count(state) == 5, "five rows");
    expect_history_row(state, 0, 1, "# first");
    expect_history_row(state, 1, 2, "second");
    expect_history_row(state, 2, 3, "third");
    expect_history_row(state, 4, 5, "fifth");
    struct history_row_view row = {};
    require(!folio_state_history_row(state, 5, &row) &&
                folio_state_history_body(state, 5) == nullptr,
            "no row past the end");
    require(same_text(note_text_bytes(folio_state_history_body(state, 1)), "second"),
            "the body is the version's text");
    require(folio_state_pane_mode(state) == PANE_MODE_VIEW, "opening keeps the mode");
    require(folio_state_restore_history(state, 1) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "restoring while viewing enters edit mode");
    require(folio_state_history_count(state) == 5 && adapter.note_writes == 0 &&
                adapter.archives == 0,
            "restoring writes nothing and keeps the list");
    require(folio_state_restore_history(state, 2) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "restoring while editing stays in edit mode");
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_history_count(state) == 5 && adapter.history_reads == 10,
            "reopening replaces the list");
    folio_state_close_history(state);
    require(folio_state_history_count(state) == 0 && folio_state_history_body(state, 0) == nullptr,
            "closing drops every version");
    require(folio_state_restore_history(state, 0) == FOLIO_STATE_HISTORY_UNREADABLE,
            "nothing to restore after closing");
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "reopen before selecting");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_history_count(state) == 0,
            "moving to another note drops the list");
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "reopen before saving");
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_READY &&
                adapter.note_writes == 1 && folio_state_history_count(state) == 0,
            "saving drops the list whose numbers moved");
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "reopen before destroying");
    folio_state_destroy(state);
}

/* 歯抜け・0 版・読めない版・確保失敗・未選択と無題（ADR 0038 の検証 1）。 */
static void verify_history_gaps(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.history[1] = "one";
    adapter.history[3] = "three";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_history_count(state) == 2,
            "only the present versions");
    expect_history_row(state, 0, 1, "one");
    expect_history_row(state, 1, 3, "three");

    adapter.history[1] = nullptr;
    adapter.history[3] = nullptr;
    require(folio_state_open_history(state) == FOLIO_STATE_HISTORY_EMPTY &&
                folio_state_history_count(state) == 0,
            "no version at all is HISTORY_EMPTY");

    adapter.history[2] = "two";
    adapter.history_outcomes[3] = PERSISTENCE_MALFORMED;
    adapter.history_outcomes[4] = PERSISTENCE_UNREADABLE;
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_history_count(state) == 3,
            "unreadable versions stay as rows");
    struct history_row_view row = {};
    require(folio_state_history_row(state, 1, &row) && row.version == 3 && !row.readable &&
                row.first_line_length == 0 && same_text(row.first_line, ""),
            "a malformed version has no body");
    require(folio_state_history_row(state, 2, &row) && row.version == 4 && !row.readable,
            "an unreadable version has no body");
    require(folio_state_history_body(state, 2) == nullptr, "no body to restore");
    require(folio_state_restore_history(state, 2) == FOLIO_STATE_HISTORY_UNREADABLE &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "an unreadable version changes nothing");
    require(folio_state_restore_history(state, 9) == FOLIO_STATE_HISTORY_UNREADABLE,
            "past the end is refused the same way");

    adapter.history_outcomes[4] = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_open_history(state) == FOLIO_STATE_OUT_OF_MEMORY &&
                folio_state_history_count(state) == 0,
            "out of memory keeps nothing");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_open_history(state) == FOLIO_STATE_NAME_REQUIRED &&
                folio_state_history_count(state) == 0,
            "an untitled note has no history");
    folio_state_destroy(state);

    struct persistence_port port = port_for(&adapter);
    struct appearance_port looks = looks_for(&dark_adapter);
    require(folio_state_create(test_ports(&port, &looks, &finder), &state) == FOLIO_STATE_READY,
            "state without a selection");
    require(folio_state_open_history(state) == FOLIO_STATE_NOTHING_SELECTED &&
                folio_state_history_count(state) == 0,
            "no document has no history");
    folio_state_destroy(state);
}

static struct category_name *_Nonnull accepted_category_name(const char *_Nonnull text)
{
    struct category_name *name = nullptr;
    require(category_name_create(text, strlen(text), &name) == CATEGORY_NAME_ACCEPTED,
            "category name prepared");
    return name;
}

/* カーソルが category 番目のカテゴリ行にあるか。 */
static bool cursor_on_category(const struct folio_state *_Nonnull state, size_t category)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref ref = {.category = 0, .note = 0};
    return folio_state_cursor(state, &kind, &ref) && kind == FOLIO_CURSOR_CATEGORY &&
           ref.category == category;
}

/* 作成の前後で選択・開いている文書・モードが変わっていないか（ADR 0039 の決定 10）。 */
static bool keeps_document(const struct folio_state *_Nonnull state, struct note_ref selected,
                           enum pane_mode mode)
{
    struct note_ref now = {.category = 0, .note = 0};
    return folio_state_selection(state, &now) && now.category == selected.category &&
           now.note == selected.note && folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED &&
           folio_state_pane_mode(state) == mode;
}

/* data/ が空でも作れる。新しい行は空なので止まる行になり、カーソルが乗る（ADR 0015）。 */
static void verify_create_first_category(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_outcome = PERSISTENCE_ABSENT;
    adapter.scan_outcome = PERSISTENCE_ABSENT;
    struct folio_state *state = ready_state(&adapter);
    struct category_name *name = accepted_category_name("仕事");
    require(folio_state_create_category(state, name) == FOLIO_STATE_READY, "first category");
    require(adapter.category_creates == 1 && same_text(adapter.created_category, "仕事") &&
                same_text(adapter.calls, "create_category"),
            "the folder is asked for by its name");
    require(adapter.writes == 1 && adapter.written_count == 1 &&
                same_text(adapter.written_names, "仕事") && adapter.written_expanded[0],
            "the ledger is written with the new category expanded");
    require(folio_state_category_count(state) == 1 && row_count(state) == 1 &&
                same_text(folio_state_category_name(state, 0), "仕事") &&
                cursor_on_category(state, 0) &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE,
            "the empty row is shown and holds the cursor");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED,
            "a note can be started in the new category at once");
    struct note_name *note = accepted_note_name("最初");
    struct note_destination destination = {.category = 0, .name = note};
    require(folio_state_store_new(state, &destination, u"x", 1) == FOLIO_STATE_READY &&
                same_text(adapter.created_note, "最初") &&
                same_text(adapter.written_category, "仕事") && folio_state_note_count(state) == 1,
            "and saved into it");
    note_name_destroy(note);
    category_name_destroy(name);
    folio_state_destroy(state);
}

/* 既存の後ろに付き、選択・文書・モードは触らず、カーソルだけが新しい行へ移る。 */
static void verify_create_category(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "a document is open for editing");
    struct category_name *name = accepted_category_name("D");
    require(folio_state_create_category(state, name) == FOLIO_STATE_READY, "fourth category");
    require(adapter.writes == 1 && adapter.written_count == 4 &&
                same_text(adapter.written_names, "B/A/C/D") && adapter.written_expanded[3] &&
                has_color(adapter.written_colors[3], adapter.written_colors[2].red,
                          adapter.written_colors[2].green, adapter.written_colors[2].blue),
            "it goes last with the default colour (C's) and expanded");
    require(folio_state_category_count(state) == 4 && row_count(state) == 10 &&
                cursor_on_category(state, 3) && keeps_document(state, at(0, 1), PANE_MODE_EDIT),
            "the cursor moves to the new row and the open document stays as it was");
    require(folio_state_toggle_category(state, 3) == FOLIO_STATE_READY &&
                !adapter.written_expanded[3],
            "the new index toggles");
    struct rgb_color chosen = {.red = 1, .green = 2, .blue = 3};
    require(folio_state_recolor_category(state, 3, chosen) == FOLIO_STATE_READY &&
                has_color(adapter.written_colors[3], 1, 2, 3),
            "the new index takes a colour");
    require(folio_state_move_category(state, 3, 0) == FOLIO_STATE_READY &&
                same_text(adapter.written_names, "D/B/A/C") &&
                keeps_document(state, at(1, 1), PANE_MODE_EDIT) && cursor_on_category(state, 0),
            "the new index moves with its (empty) note ledger");
    category_name_destroy(name);
    folio_state_destroy(state);
}

/* 大小文字だけ違う名前も、絞り込み中も、port に届く前に断る。状態は変わらない。 */
static void verify_create_category_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct category_name *folded = accepted_category_name("b");
    require(folio_state_create_category(state, folded) == FOLIO_STATE_CATEGORY_NAME_TAKEN,
            "a name that differs only in case is taken");
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY, "filter on");
    struct category_name *name = accepted_category_name("D");
    require(folio_state_create_category(state, name) == FOLIO_STATE_FILTERED,
            "filtering refuses the structural change");
    require(adapter.category_creates == 0 && adapter.writes == 0 && adapter.renames == 0 &&
                folio_state_category_count(state) == 3 && !cursor_on_category(state, 3),
            "neither refusal reaches the port or changes the state");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY, "filter off");
    adapter.category_create_outcome = PERSISTENCE_NAME_TAKEN;
    require(folio_state_create_category(state, name) == FOLIO_STATE_CATEGORY_NAME_TAKEN,
            "a folder that appeared meanwhile is taken");
    adapter.category_create_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_create_category(state, name) == FOLIO_STATE_CATEGORY_NOT_CREATED,
            "a folder that cannot be made is not created");
    require(adapter.category_creates == 2 && adapter.writes == 0 &&
                folio_state_category_count(state) == 3 && row_count(state) == 9 &&
                !cursor_on_category(state, 3),
            "after a refused folder the ledger is unwritten and the state unchanged");
    category_name_destroy(name);
    category_name_destroy(folded);
    folio_state_destroy(state);
}

/* フォルダを作った後で台帳を書けなくても、フォルダに従って採用する（決定 3・補正 1）。 */
static void verify_create_category_stale_ledger(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    struct folio_state *state = ready_state(&adapter);
    struct category_name *name = accepted_category_name("D");
    require(folio_state_create_category(state, name) == FOLIO_STATE_CATEGORY_LEDGER_STALE,
            "the ledger lags behind the folder");
    require(adapter.category_creates == 1 && adapter.writes == 1 &&
                folio_state_category_count(state) == 4 && cursor_on_category(state, 3),
            "the new category is adopted in memory anyway");
    adapter.write_outcome = PERSISTENCE_STORED;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                adapter.written_count == 4 && same_text(adapter.written_names, "B/A/C/D"),
            "the next ledger write carries the new category");
    category_name_destroy(name);
    folio_state_destroy(state);
}

/* 未完了の改名があれば同期の値を返し、フォルダを作らない（ADR 0039 の決定 5）。 */
static void verify_create_category_pending_rename(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *renamed = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, renamed, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "a rename is left unfinished");
    struct category_name *name = accepted_category_name("D");
    require(folio_state_create_category(state, name) == FOLIO_STATE_RENAME_PENDING &&
                adapter.renames == 2 && adapter.last_attempt == RENAME_RESUME,
            "the unfinished rename is retried first and its outcome returned");
    require(adapter.category_creates == 0 && adapter.writes == 0 &&
                folio_state_category_count(state) == 3,
            "and no folder is made");
    category_name_destroy(name);
    note_name_destroy(renamed);
    folio_state_destroy(state);
}

static void verify_create_categories(void)
{
    verify_create_first_category();
    verify_create_category();
    verify_create_category_refusals();
    verify_create_category_stale_ledger();
    verify_create_category_pending_rename();
}

static struct folio_state *_Nonnull empty_category_state(
    struct persistence_adapter *_Nonnull adapter, const char *_Nonnull category)
{
    static const char *const no_notes[] = {nullptr};
    test_adapter_second_notes(adapter, category, "{\"version\":1,\"notes\":[]}", no_notes);
    return ready_state(adapter);
}

static void verify_remove_category_outcomes(void)
{
    const enum category_remove_outcome results[] = {
        CATEGORY_REMOVE_REMOVED, CATEGORY_REMOVE_ABSENT, CATEGORY_REMOVE_HAS_FILES,
        CATEGORY_REMOVE_HISTORY_NOT_RECYCLED, CATEGORY_REMOVE_FAILED};
    const enum folio_state_outcome expected[] = {
        FOLIO_STATE_READY, FOLIO_STATE_READY, FOLIO_STATE_CATEGORY_HAS_FILES,
        FOLIO_STATE_CATEGORY_HISTORY_NOT_RECYCLED, FOLIO_STATE_CATEGORY_REMOVE_FAILED};
    for (size_t index = 0; index < sizeof results / sizeof results[0]; ++index)
    {
        struct persistence_adapter adapter = healthy_adapter();
        adapter.category_remove_outcome = results[index];
        struct folio_state *state = empty_category_state(&adapter, "B");
        require(folio_state_select_note(state, 2, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
                "select later note");
        size_t reads = adapter.note_reads;
        const char *body = folio_state_pane_text(state);
        require(folio_state_delete_category(state, 0) == expected[index], "typed removal result");
        bool adopted = index < 2;
        require(adapter.category_removes == 1 && same_text(adapter.removed_category, "B") &&
                    adapter.writes == (adopted ? 1 : 0) &&
                    folio_state_category_count(state) == (adopted ? 2 : 3) &&
                    keeps_document(state, at(adopted ? 1 : 2, 1), PANE_MODE_VIEW),
                "only removal acceptance and absence adopt the prepared category ledger");
        require(folio_state_pane_text(state) == body && adapter.note_reads == reads &&
                    adapter.archives == 0 && adapter.note_writes == 0 && adapter.ledger_writes == 0,
                "removal does not save or reread any document or note index");
        folio_state_destroy(state);
    }
}

static void verify_remove_category_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = empty_category_state(&adapter, "B");
    require(folio_state_delete_category(state, 1) == FOLIO_STATE_CATEGORY_NOT_EMPTY &&
                folio_state_delete_category(state, 9) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "notes and invalid category refuse before the port");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_delete_category(state, 0) == FOLIO_STATE_CATEGORY_NOT_EMPTY,
            "an untitled destination is not empty");
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_delete_category(state, 9) == FOLIO_STATE_FILTERED,
            "filter rejection precedes range and synchronization");
    require(adapter.category_removes == 0 && adapter.writes == 0 && adapter.archives == 0 &&
                adapter.note_writes == 0,
            "refusals have no destructive port or save calls");
    folio_state_destroy(state);
}

static void prepare_trash_preview(struct folio_state *_Nonnull state);
static void expect_trash_preview_usable(struct folio_state *_Nonnull state);

static void verify_remove_category_preserves_edit(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.history[1] = "old";
    struct folio_state *state = empty_category_state(&adapter, "B");
    require(folio_state_select_note(state, 2, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_open_history(state) == FOLIO_STATE_READY,
            "keep a later document with loaded history");
    prepare_trash_preview(state);
    const char *body = folio_state_pane_text(state);
    const char *pane = folio_state_pane_rtf(state);
    const struct note_text *history = folio_state_history_body(state, 0);
    size_t reads = adapter.note_reads;
    size_t history_reads = adapter.history_reads;
    require(folio_state_delete_category(state, 0) == FOLIO_STATE_READY &&
                keeps_document(state, at(1, 1), PANE_MODE_EDIT),
            "a preceding empty category only shifts the edited document index");
    require(folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == pane &&
                folio_state_replace_count(state) == 1 &&
                folio_state_history_body(state, 0) == history && adapter.note_reads == reads &&
                adapter.history_reads == history_reads && adapter.archives == 0 &&
                adapter.note_writes == 0,
            "body, rendered pane, preview and history are retained without saving or reloading");
    expect_trash_preview_usable(state);
    folio_state_destroy(state);
}

static void verify_remove_category_cursor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct category_name *name = accepted_category_name("D");
    require(folio_state_create_category(state, name) == FOLIO_STATE_READY &&
                folio_state_move_category(state, 3, 1) == FOLIO_STATE_READY,
            "create a middle empty category heading");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_delete_category(state, 1) == FOLIO_STATE_CATEGORY_LEDGER_STALE &&
                cursor_on_category(state, 1) && folio_state_category_count(state) == 3 &&
                same_text(adapter.removed_category, "D"),
            "accepted removal adopts even if ledger writing fails and keeps the next heading");
    category_name_destroy(name);
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.scanned_category_count = 0;
    adapter.categories_outcome = PERSISTENCE_ABSENT;
    state = ready_state(&adapter);
    name = accepted_category_name("only");
    require(folio_state_create_category(state, name) == FOLIO_STATE_READY &&
                folio_state_delete_category(state, 0) == FOLIO_STATE_READY &&
                folio_state_category_count(state) == 0,
            "remove the only empty category");
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = at(9, 9);
    require(!folio_state_cursor(state, &kind, &cursor), "no category leaves no cursor");
    category_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_remove_category_untitled_and_last_cursor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = empty_category_state(&adapter, "B");
    require(folio_state_new_note(state, 2, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled in a later category");
    const char *body = folio_state_pane_text(state);
    const char *pane = folio_state_pane_rtf(state);
    require(folio_state_delete_category(state, 0) == FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_current_category(state) == 1 &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == pane &&
                adapter.archives == 0 && adapter.note_writes == 0 && adapter.note_reads == 0,
            "a later untitled destination shifts without saving or recreating its body");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    state = ready_state(&adapter);
    struct category_name *name = accepted_category_name("last");
    require(folio_state_create_category(state, name) == FOLIO_STATE_READY &&
                cursor_on_category(state, 3) &&
                folio_state_delete_category(state, 3) == FOLIO_STATE_READY &&
                cursor_on_category(state, 0),
            "removing the last heading settles on the first remaining category heading");
    category_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_remove_category_pending_rename(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = empty_category_state(&adapter, "B");
    require(folio_state_select_note(state, 1, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select to rename");
    struct note_name *name = accepted_note_name("renamed");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING &&
                folio_state_delete_category(state, 0) == FOLIO_STATE_RENAME_PENDING &&
                adapter.renames == 2 && adapter.last_attempt == RENAME_RESUME &&
                adapter.category_removes == 0,
            "pending rename synchronization prevents the destructive port");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_remove_categories(void)
{
    verify_remove_category_outcomes();
    verify_remove_category_refusals();
    verify_remove_category_preserves_edit();
    verify_remove_category_cursor();
    verify_remove_category_untitled_and_last_cursor();
    verify_remove_category_pending_rename();
}

void run_rename_state_tests(void)
{
    verify_rename_refusals();
    verify_rename_saves_first();
    verify_rename_blocked_by_save();
    verify_rename_refused_before_journal();
    verify_rename_pending();
    verify_rename_halted();
    verify_rename_force_quit_keeps_intent();
    verify_rename_recovery();
}

static void verify_later_units(void)
{
    run_rename_state_tests();
    verify_search_term();
    verify_index_filter();
    verify_index_filter_refusals();
    verify_filtered_refusals();
    verify_filter_keeps_document();
    verify_filter_follows_content();
    verify_filter_follows_new_note();
    verify_filter_cursor_lands_near();
    verify_filter_follows_resumed_rename();
    verify_filter_follows_move();
    verify_settings();
    verify_create_categories();
}

static void prepare_trash_preview(struct folio_state *_Nonnull state)
{
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit for trash preview");
    struct replace_request request = {.text = u"# Hello\n\nbody",
                                      .length = 13,
                                      .pattern = u"body",
                                      .pattern_length = 4,
                                      .replacement = u"new",
                                      .replacement_length = 3};
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY &&
                folio_state_replace_count(state) == 1,
            "preview before trash");
}

static void verify_trash_current(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.history[1] = "old";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "history before trash");
    size_t reads = adapter.note_reads;
    require(folio_state_trash_note(state, at(0, 0), u"ignored", 7) == FOLIO_STATE_READY,
            "trash the viewed current note");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                folio_state_pane_text_length(state) == 0 && folio_state_history_count(state) == 0,
            "current deletion leaves an empty view without history");
    expect_cursor(state, "c0", "current deletion returns the cursor to its category");
    require(adapter.note_reads == reads && adapter.note_writes == 0 && adapter.trashes == 1,
            "view deletion neither writes nor reads a next note");
    require(same_text(adapter.trashed_category, "B") && same_text(adapter.trashed_note, "one") &&
                same_text(adapter.ledger_order, "two/three"),
            "explicit target and reduced ledger");
    require(folio_state_store_note(state, u"must not write", 14) == FOLIO_STATE_NOTHING_SELECTED &&
                adapter.note_writes == 0,
            "NONE never writes to the compacted next note");
    folio_state_destroy(state);

    adapter = healthy_adapter();
    adapter.history[1] = "old";
    state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "history before edit trash");
    prepare_trash_preview(state);
    require(folio_state_trash_note(state, at(0, 0), u"changed", 7) == FOLIO_STATE_READY,
            "trash an edited note after saving");
    require(same_text(adapter.calls, "archive/write/trash_note/B/two/three") &&
                same_text(adapter.written_body, "changed"),
            "archive and write precede trash and the reduced ledger");
    require(folio_state_replace_count(state) == 0 && folio_state_history_count(state) == 0 &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE,
            "current removal discards preview and history");
    folio_state_destroy(state);
}

static void expect_trash_preview_usable(struct folio_state *_Nonnull state)
{
    struct replace_apply apply = {.text = u"# Hello\n\nbody",
                                  .length = 13,
                                  .anchor = {.start = 0, .end = 0},
                                  .scope = REPLACE_ALL};
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_replace(state, &apply, &edit) == FOLIO_STATE_READY,
            "the kept preview still applies to the same document after index compaction");
    replace_edit_destroy(edit);
}

static void verify_trash_other(void)
{
    const struct note_ref targets[] = {at(0, 0), at(0, 2), at(1, 0)};
    for (size_t index = 0; index < 3; ++index)
    {
        struct persistence_adapter adapter = healthy_adapter();
        adapter.history[1] = "old";
        struct folio_state *state = ready_state(&adapter);
        require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                    folio_state_open_history(state) == FOLIO_STATE_READY,
                "select and history");
        prepare_trash_preview(state);
        const char *body = folio_state_pane_text(state);
        const char *pane = folio_state_pane_rtf(state);
        const struct note_text *history = folio_state_history_body(state, 0);
        size_t reads = adapter.note_reads;
        adapter.missing_note = index == 0 ? "one" : "three";
        require(folio_state_trash_note(state, targets[index], u"unsaved editor", 14) ==
                    FOLIO_STATE_READY,
                "trash a different note, including an unreadable target");
        require(keeps_document(state, at(0, index == 0 ? 0 : 1), PANE_MODE_EDIT) &&
                    folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == pane &&
                    folio_state_history_body(state, 0) == history &&
                    folio_state_replace_count(state) == 1,
                "only the selected index is remapped");
        require(adapter.note_reads == reads && adapter.note_writes == 0 && adapter.archives == 0,
                "a different target never saves or rereads the current editor");
        expect_trash_preview_usable(state);
        folio_state_destroy(state);
    }
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled editor");
    const char *body = folio_state_pane_text(state);
    require(folio_state_trash_note(state, at(0, 0), u"unsaved", 7) == FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_pane_text(state) == body && adapter.note_writes == 0,
            "a different target preserves an untitled editor");
    require(folio_state_store_note(state, u"unsaved", 7) == FOLIO_STATE_NAME_REQUIRED,
            "untitled saving keeps its existing rejection");
    folio_state_destroy(state);
}

static void verify_trash_failures(void)
{
    const enum trash_outcome failures[] = {TRASH_UNAVAILABLE, TRASH_BUSY, TRASH_FAILED};
    const enum folio_state_outcome expected[] = {FOLIO_STATE_TRASH_UNAVAILABLE,
                                                 FOLIO_STATE_TRASH_BUSY, FOLIO_STATE_TRASH_FAILED};
    for (size_t index = 0; index < 3; ++index)
    {
        struct persistence_adapter adapter = healthy_adapter();
        adapter.trash_outcome = failures[index];
        adapter.history[1] = "old";
        struct folio_state *state = history_state(&adapter);
        require(folio_state_open_history(state) == FOLIO_STATE_READY, "history before failure");
        prepare_trash_preview(state);
        const char *body = folio_state_pane_text(state);
        const char *pane = folio_state_pane_rtf(state);
        const struct note_text *history = folio_state_history_body(state, 0);
        require(folio_state_trash_note(state, at(0, 0), u"# Hello\n\nbody", 13) == expected[index],
                "trash failures map to their closed application outcomes");
        require(keeps_document(state, at(0, 0), PANE_MODE_EDIT) &&
                    folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == pane &&
                    folio_state_history_body(state, 0) == history &&
                    folio_state_replace_count(state) == 1 && adapter.ledger_writes == 0,
                "failed trash preserves the selection, display, history, preview and ledger");
        folio_state_destroy(state);
    }
    struct persistence_adapter adapter = healthy_adapter();
    adapter.trash_outcome = TRASH_ABSENT;
    struct folio_state *state = history_state(&adapter);
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE &&
                same_text(adapter.ledger_order, "two/three"),
            "ABSENT adopts exactly like TRASHED");
    folio_state_destroy(state);
}

static void verify_trash_save_failure(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit before failed save");
    adapter.archive_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_trash_note(state, at(0, 0), u"changed", 7) == FOLIO_STATE_HISTORY_FAILED &&
                adapter.trashes == 0 && adapter.note_writes == 0,
            "history failure blocks trash");
    adapter.archive_outcome = PERSISTENCE_STORED;
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_trash_note(state, at(0, 0), u"changed", 7) ==
                    FOLIO_STATE_NOTE_STORE_FAILED &&
                adapter.trashes == 0 && keeps_document(state, at(0, 0), PANE_MODE_EDIT),
            "save failure blocks trash and preserves the document");
    adapter.note_write_outcome = PERSISTENCE_STORED;
    adapter.trash_outcome = TRASH_FAILED;
    require(folio_state_trash_note(state, at(0, 0), u"changed", 7) == FOLIO_STATE_TRASH_FAILED &&
                same_text(folio_state_pane_text(state), "changed") &&
                keeps_document(state, at(0, 0), PANE_MODE_EDIT),
            "a successful pre-trash save remains adopted when trash fails");
    folio_state_destroy(state);
}

static void verify_trash_ledger_retry(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_LEDGER_STALE &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE && adapter.trashes == 1,
            "the actual deletion is adopted even when the ledger cannot be written");
    size_t writes = adapter.ledger_writes;
    require(folio_state_trash_note(state, at(9, 0), u"", 0) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                folio_state_trash_note(state, at(0, 9), u"", 0) == FOLIO_STATE_NO_SUCH_NOTE &&
                adapter.ledger_writes == writes,
            "range rejection precedes pending synchronization");
    require(folio_state_store_note(state, u"never write", 11) == FOLIO_STATE_LEDGER_UNSYNCED &&
                adapter.ledger_writes == writes + 1 && adapter.note_writes == 0,
            "NONE still retries the pending ledger before rejecting save");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note(state, u"never write", 11) == FOLIO_STATE_NOTHING_SELECTED &&
                same_text(adapter.ledger_order, "two/three") && adapter.note_writes == 0,
            "retry repairs the reduced ledger and never writes another note");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_document_name(state), "two"),
            "the next note is explicitly selected");
    folio_state_destroy(state);
}

static void verify_trash_synchronization(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    struct note_name *name = accepted_note_name("renamed");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "pending rename before trash");
    size_t renames = adapter.renames;
    require(folio_state_trash_note(state, at(0, 9), u"", 0) == FOLIO_STATE_NO_SUCH_NOTE &&
                adapter.renames == renames,
            "invalid trash does not resume the pending rename");
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_RENAME_PENDING &&
                adapter.renames == renames + 1 && adapter.trashes == 0,
            "unfinished synchronization blocks trash before preparation and side effects");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_READY &&
                same_text(adapter.trashed_note, "renamed") && adapter.trashes == 1,
            "successful synchronization supplies the canonical renamed target to trash");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_trash_filter(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    require(test_index_filter_complete(state, u"two", 3) == FOLIO_STATE_READY,
            "filter a later note");
    require(folio_state_scroll_drawer(state, scroll_metrics, 10) == FOLIO_STATE_READY,
            "scroll request");
    int top = scrolled_top(state);
    size_t reads = adapter.note_reads;
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 3 &&
                same_text(folio_state_index_filter_term(state), "two") &&
                adapter.note_reads == reads,
            "nonmatch deletion keeps the term and maps later matches without rereading");
    require(scrolled_top(state) == top, "trash retains the scroll request");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_document_name(state), "two"),
            "mapped match selects the right note");
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 2,
            "last match in this category disappears");
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 2,
            "removed cache entry cannot reappear on requery");
    folio_state_destroy(state);
}

static void verify_trash_hidden_category_cursor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    static const char *const only_note[] = {"four", nullptr};
    test_adapter_second_notes(&adapter, "A", "{\"version\": 1, \"notes\": [\"four\"]}", only_note);
    adapter.rare_note = "four";
    adapter.rare_body = "needle";
    struct folio_state *state = history_state(&adapter);
    for (size_t index = 0; index < 3; ++index)
    {
        require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
                "step through B notes into collapsed A");
    }
    require(cursor_on_category(state, 1), "cursor on collapsed A before filtering");
    struct note_ref selected = {.category = 0, .note = 0};
    require(folio_state_selection(state, &selected), "the current B document stays selected");
    const char *body = folio_state_pane_text(state);
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && cursor_on_category(state, 1),
            "only A's note matches, with a category cursor retained");
    require(folio_state_trash_note(state, at(1, 0), u"", 0) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 &&
                keeps_document(state, selected, PANE_MODE_VIEW) &&
                folio_state_pane_text(state) == body,
            "removing the category's last match preserves the current document");
    expect_cursor(state, "-", "the cursor does not remain on the now hidden category");
    folio_state_destroy(state);
}

static void verify_trash_all(void)
{
    verify_trash_current();
    verify_trash_other();
    verify_trash_failures();
    verify_trash_save_failure();
    verify_trash_ledger_retry();
    verify_trash_synchronization();
    verify_trash_filter();
    verify_trash_hidden_category_cursor();
}

/* #242: RichEdit が変える文字（U+2028）を含む保存済みノートは編集へ入らない。閲覧のままなので、
 * RichEdit が返す変換後の本文を渡しても変更なしと判定され、切り替えで保存されない。 */
static void verify_uneditable_begin_edit(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "a\xE2\x80\xA8"
                        "b";
    struct folio_state *state = history_state(&adapter);
    const char *body = folio_state_pane_text(state);
    require(folio_state_begin_edit(state) == FOLIO_STATE_NOTE_UNEDITABLE &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                folio_state_pane_text(state) == body,
            "a body with U+2028 does not enter edit mode");
    enum folio_note_change changed = FOLIO_NOTE_CHANGED;
    require(folio_state_note_changed(state, u"a\vb", 3, &changed) == FOLIO_STATE_READY &&
                changed == FOLIO_NOTE_SAME && adapter.note_writes == 0 && adapter.archives == 0,
            "switching away from the refused note saves nothing");
    folio_state_destroy(state);
}

/* #242: 履歴の版が変わる文字（U+2029）を含めば、本文・mode・一覧を変えずに断る。 */
static void verify_uneditable_restore(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "kept";
    adapter.history[1] = "old\xE2\x80\xA9"
                         "x";
    adapter.history[2] = "fine";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_history_count(state) == 2,
            "two versions are listed");
    const char *body = folio_state_pane_text(state);
    require(folio_state_restore_history(state, 0) == FOLIO_STATE_NOTE_UNEDITABLE &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                folio_state_pane_text(state) == body && same_text(body, "kept") &&
                folio_state_history_count(state) == 2 && adapter.note_writes == 0,
            "a version with U+2029 is not restored");
    require(folio_state_restore_history(state, 1) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "a clean version is still restored");
    folio_state_destroy(state);

    adapter.note_body = "\x07"
                        "bell";
    state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_restore_history(state, 1) == FOLIO_STATE_NOTE_UNEDITABLE &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "a current body with U+0007 cannot be put into the editor by a restore");
    folio_state_destroy(state);
}

/* #242: VT・U+FFFD・TAB・CRLF だけの本文は編集できる。編集のまま切り替えた先が変わる文字を
 * 含むなら、その本文は editor へ渡さず閲覧で開く。 */
static void verify_uneditable_neighbours(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "a\vb\xEF\xBF\xBD\tc\r\nd";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "VT and U+FFFD stay editable");
    folio_state_destroy(state);

    adapter.note_body = "x\xE2\x80\xA8"
                        "y";
    adapter.rare_note = "one";
    adapter.rare_body = "clean";
    state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_document_name(state), "two") &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "switching in edit mode to an uneditable body opens it in view mode");
    require(folio_state_begin_edit(state) == FOLIO_STATE_NOTE_UNEDITABLE,
            "the switched note still refuses edit mode");
    folio_state_destroy(state);
}

/* #258: 編集から閲覧へ落とした切り替えだけが NOTE_UNEDITABLE を残し、次の切り替えで READY へ
 * 戻る。閲覧のまま該当ノートを開いても何も奪っていないので READY。 */
static void verify_uneditable_switch_notice(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "x\xE2\x80\xA8"
                        "y";
    adapter.rare_note = "one";
    adapter.rare_body = "clean";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_select_notice(state) == FOLIO_STATE_READY, "no notice before any switch");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                folio_state_select_notice(state) == FOLIO_STATE_NOTE_UNEDITABLE &&
                adapter.note_writes == 0,
            "dropping from edit to view on a switch leaves NOTE_UNEDITABLE");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_select_notice(state) == FOLIO_STATE_READY,
            "the next switch clears the notice");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                folio_state_select_notice(state) == FOLIO_STATE_READY,
            "opening the note in view mode takes nothing away");
    folio_state_destroy(state);
}

void run_editor_eligibility_state_tests(void)
{
    finder = test_regex_port();
    verify_uneditable_begin_edit();
    verify_uneditable_restore();
    verify_uneditable_neighbours();
    verify_uneditable_switch_notice();
}

/* 編集用 RTF の本文部分（頭の後の段落の書式から）。 */
static const char *_Nonnull edit_rtf_tail(const char *_Nonnull rtf)
{
    const char *tail = strstr(rtf, "\\pard");
    require(tail != nullptr, "the edit rtf has a paragraph");
    return tail == nullptr ? rtf : tail;
}

/* folio_state_pane_text と同じ本文を core で直接変換した本文部分と同じか。 */
static void expect_edit_rtf_matches_pane_text(const struct folio_state *_Nonnull state)
{
    struct note_text *text = nullptr;
    require(note_text_create(folio_state_pane_text(state), folio_state_pane_text_length(state),
                             &text) == NOTE_TEXT_ACCEPTED,
            "pane text");
    struct markdown_rtf *plain = nullptr;
    require(markdown_rtf_plain(text, folio_theme_palette(FOLIO_THEME_LIGHT),
                               ui_font_face(FOLIO_LANGUAGE_JA), &plain) == MARKDOWN_RTF_CONVERTED,
            "core plain");
    require(same_text(edit_rtf_tail(folio_state_edit_rtf(state)),
                      edit_rtf_tail(markdown_rtf_text(plain))) &&
                folio_state_edit_rtf_length(state) == strlen(folio_state_edit_rtf(state)),
            "the edit rtf is the core plain rtf of the pane text");
    markdown_rtf_destroy(plain);
    note_text_destroy(text);
}

/* #250: 現在の本文と無題から編集用 RTF を作る。閲覧の pane は変えない。 */
static void verify_pane_edit_rtf(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(same_text(folio_state_edit_rtf(state), "") && folio_state_edit_rtf_length(state) == 0,
            "no edit rtf before the first request");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select B / two");
    const char *pane = folio_state_pane_rtf(state);
    require(folio_state_pane_edit_rtf(state) == FOLIO_STATE_READY, "edit rtf of the note");
    expect_edit_rtf_matches_pane_text(state);
    require(strstr(folio_state_edit_rtf(state), "# Hello\\par \\par body\\par }") != nullptr &&
                strstr(folio_state_edit_rtf(state), "\\b\\cf2") == nullptr &&
                strstr(folio_state_edit_rtf(state), ui_font_face(FOLIO_LANGUAGE_JA)) != nullptr,
            "the markdown is kept as plain text with the view face");
    require(folio_state_pane_edit_rtf(state) == FOLIO_STATE_READY, "a second request");
    expect_edit_rtf_matches_pane_text(state);
    require(folio_state_pane_rtf(state) == pane && folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "the view pane and the mode are untouched");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_store_note(state, u"latest saved body", 17) == FOLIO_STATE_READY,
            "save an edited body before a possible load retry");
    require(strstr(folio_state_edit_rtf(state), "# Hello") != nullptr,
            "the prior edit cache can be stale after saving");
    require(folio_state_pane_edit_rtf(state) == FOLIO_STATE_READY,
            "retry regenerates from the latest saved body");
    expect_edit_rtf_matches_pane_text(state);
    require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "new untitled in A");
    require(folio_state_pane_edit_rtf(state) == FOLIO_STATE_READY, "edit rtf of the untitled");
    require(same_text(edit_rtf_tail(folio_state_edit_rtf(state)),
                      "\\pard\\sb0\\sa0\\sl240\\slmult1\\cf1 \\par }"),
            "the untitled edit rtf is empty");
    folio_state_destroy(state);
}

void run_pane_load_state_tests(void)
{
    verify_pane_edit_rtf();
    verify_failure_lines();
}

static void verify_time_value(void)
{
    struct note_file_times *times = nullptr;
    const char *invalid[] = {"",
                             "2020/01-02 03:04",
                             "2020-01-02 03:0x",
                             "1600-01-01 00:00",
                             "2020-00-01 00:00",
                             "2020-13-01 00:00",
                             "2020-02-30 00:00",
                             "1900-02-29 00:00",
                             "2020-01-00 00:00",
                             "2020-01-01 24:00",
                             "2020-01-01 00:60"};
    for (size_t at = 0; at < sizeof invalid / sizeof invalid[0]; ++at)
        require(note_file_times_create(invalid[at], "2000-02-29 00:00", &times) ==
                        NOTE_FILE_TIMES_INVALID &&
                    times == nullptr,
                "invalid calendar display value leaves output alone");
    require(note_file_times_create("2000-02-29 00:00", "invalid", &times) ==
                NOTE_FILE_TIMES_INVALID,
            "updated time is also checked");
    allocation_probe_fail_at(1);
    enum note_file_times_outcome made =
        note_file_times_create("2000-02-29 00:00", "2024-02-29 23:59", &times);
    require(made == (allocation_probe_calls() == 0 ? NOTE_FILE_TIMES_ACCEPTED
                                                   : NOTE_FILE_TIMES_OUT_OF_MEMORY),
            "time snapshot allocation has an explicit failure");
    allocation_probe_fail_at(0);
    note_file_times_destroy(times);
}

static void verify_time_transitions(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.times_ready = true;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_note_times(state).state == NOTE_TIMES_NONE && adapter.times_reads == 0,
            "no file attributes before selection");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_note_times(state).created, "2020-01-02 03:04"),
            "selected file owns its creation snapshot");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                same_text(folio_state_note_times(state).created, "2021-02-03 04:05"),
            "switching files replaces the previous time");
    size_t reads = adapter.times_reads;
    (void)folio_state_note_times(state);
    require(adapter.times_reads == reads, "display queries perform no file I/O");
    adapter.times_ready = false;
    folio_state_refresh_note_times(state);
    require(folio_state_note_times(state).state == NOTE_TIMES_UNAVAILABLE &&
                same_text(folio_state_note_times(state).created, ""),
            "attribute failure clears stale values");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_note_times(state).state == NOTE_TIMES_UNSAVED,
            "untitled has no fabricated time");
    struct note_name *name = accepted_note_name("fresh");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.times_ready = true;
    require(folio_state_store_new(state, &destination, u"fresh", 5) == FOLIO_STATE_READY &&
                folio_state_note_times(state).state == NOTE_TIMES_READY,
            "first save reads the new file attributes");
    note_name_destroy(name);
    require(folio_state_trash_note(state, (struct note_ref){0, 3}, u"fresh", 5) ==
                    FOLIO_STATE_READY &&
                folio_state_note_times(state).state == NOTE_TIMES_NONE,
            "deleting the selected file clears its time");
    folio_state_destroy(state);
}

static void verify_time_save_rename(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.times_ready = true;
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    size_t reads = adapter.times_reads;
    require(folio_state_store_note(state, u"# Hello\r\n\r\nbody", 15) == FOLIO_STATE_READY &&
                adapter.times_reads == reads,
            "unchanged save does not refresh attributes");
    adapter.updated_time = "2026-11-03 13:00";
    require(folio_state_store_note(state, u"changed", 7) == FOLIO_STATE_READY &&
                same_text(folio_state_note_times(state).updated, adapter.updated_time) &&
                same_text(folio_state_note_times(state).created, "2021-02-03 04:05"),
            "successful save refreshes the timestamp snapshot");
    adapter.times_ready = false;
    require(folio_state_store_note(state, u"changed again", 13) == FOLIO_STATE_READY &&
                folio_state_note_times(state).state == NOTE_TIMES_UNAVAILABLE,
            "attribute failure does not turn saved bytes into a save failure");
    adapter.times_ready = true;
    folio_state_refresh_note_times(state);
    struct note_name *name = accepted_note_name("renamed");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"changed again", 13) ==
                    FOLIO_STATE_RENAME_PENDING &&
                folio_state_note_times(state).state == NOTE_TIMES_UNAVAILABLE,
            "pending rename cannot display a timestamp for an uncertain path");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_retry_rename(state) == FOLIO_STATE_READY &&
                folio_state_note_times(state).state == NOTE_TIMES_READY,
            "completed rename reads the adopted path");
    note_name_destroy(name);
    folio_state_destroy(state);
}

void run_status_tests(void)
{
    finder = test_regex_port();
    run_system_contrast_tests();
    verify_time_value();
    verify_time_transitions();
    verify_time_save_rename();
}

void run_save_state_tests(void)
{
    finder = test_regex_port();
    verify_edit_guards();
    verify_edit_unchanged();
    verify_note_changed();
    verify_unsaved();
    verify_history_order();
    verify_history_failures();
    verify_edit_saves();
    verify_edit_failures();
    (void)run_save_preparation_allocation_tests();
}

void run_state_tests(void)
{
    run_layout_mode_state_tests();
    verify_pane_edit_rtf();
    finder = test_regex_port();
    verify_ready_state();
    verify_absent_data();
    verify_failures();
    verify_toggle();
    verify_recolor();
    run_drawer_scroll_delta_tests();
    verify_move_category();
    verify_move_note();
    verify_category_refresh();
    verify_refresh_adds();
    verify_refresh_follows_names();
    verify_refresh_refuses();
    verify_refresh_unreadable();
    verify_refresh_keeps_open();
    verify_refresh_search_and_cursor();
    verify_refresh_allocation();
    verify_refresh_created_and_binned();
    verify_refresh_taken_shown();
    verify_refresh_transfer();
    verify_refresh_retry_places();
    verify_refresh_bins_open();
    verify_move_selection();
    verify_move_to_category();
    verify_transfer_note();
    verify_transfer_to_end();
    verify_transfer_selection();
    verify_transfer_renumbering();
    verify_transfer_refusals();
    verify_transfer_stale_ledger();
    verify_select();
    verify_select_adjacent();
    verify_set_expanded();
    verify_cursor_expanded();
    verify_reveal_cursor();
    verify_mode_survives_selection();
    run_save_state_tests();
    verify_discard_refusals();
    verify_discard_edits();
    verify_move_while_editing();
    verify_failure_lines();
    verify_untitled();
    verify_first_save();
    verify_first_save_refusals();
    verify_created_stale_index();
    verify_first_save_keeps_category_cursor();
    verify_save_as_edit();
    verify_save_as_view();
    verify_save_as_refusals();
    verify_save_as_view_stale();
    verify_save_as_edit_unsynced();
    verify_history_all();
    verify_history_gaps();
    run_editor_eligibility_state_tests();
    verify_later_units();
    verify_trash_all();
    verify_remove_categories();
}
/* ADR0041 D2: カテゴリ改名の保存範囲・単一保留・採用・読込前同期。 */
static enum folio_state_outcome rename_category_to(struct folio_state *_Nonnull state,
                                                   size_t category, const char *_Nonnull text)
{
    struct category_name *name = accepted_category_name(text);
    struct category_rename_target target = {.category = category, .name = name};
    enum folio_state_outcome outcome = folio_state_rename_category(state, &target, u"ignored", 7);
    category_name_destroy(name);
    return outcome;
}

static void verify_category_rename_refusals(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit before refusals");
    require(rename_category_to(state, 0, "B") == FOLIO_STATE_NAME_UNCHANGED &&
                rename_category_to(state, 0, "b") == FOLIO_STATE_CATEGORY_NAME_TAKEN &&
                rename_category_to(state, 0, "A") == FOLIO_STATE_CATEGORY_NAME_TAKEN &&
                rename_category_to(state, 9, "D") == FOLIO_STATE_NO_SUCH_CATEGORY,
            "same-name is no-op and ASCII-fold collisions/range refuse before saving");
    require(adapter.renames == 0 && adapter.note_writes == 0 && adapter.archives == 0,
            "no-op and early refusal have no new side effect");
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY &&
                rename_category_to(state, 9, "D") == FOLIO_STATE_FILTERED,
            "filter precedes target range");
    struct note_name *name = accepted_note_name("new");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"# Hello\n\nbody", 13) ==
                FOLIO_STATE_RENAME_PENDING,
            "existing note intent can be pending in a filter");
    size_t calls = adapter.renames;
    require(rename_category_to(state, 0, "D") == FOLIO_STATE_FILTERED && adapter.renames == calls,
            "filtered category request does not even resume prior intent");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY &&
                rename_category_to(state, 9, "D") == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.renames == calls,
            "range rejection precedes prior synchronization");
    require(rename_category_to(state, 0, "B") == FOLIO_STATE_RENAME_PENDING &&
                adapter.renames == calls + 1,
            "prior synchronization precedes same-name no-op");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void category_scope_document(struct folio_state *_Nonnull state, size_t scenario)
{
    if (scenario == 1 || scenario == 3 || scenario == 4)
    {
        require(folio_state_select_note(state, scenario == 1 ? 0 : 2, 1, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
                "select current or separate document");
    }
    if (scenario == 2 || scenario == 5)
    {
        require(folio_state_new_note(state, scenario == 2 ? 0 : 2, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
                "untitled current or separate document");
    }
    if (scenario == 4)
    {
        require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit separate document");
    }
}

static void verify_category_rename_scope(void)
{
    for (size_t scenario = 0; scenario < 6; ++scenario)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = ready_state(&adapter);
        category_scope_document(state, scenario);
        const char *body = folio_state_pane_text(state);
        const char *pane = folio_state_pane_rtf(state);
        enum folio_document_kind kind = folio_state_document_kind(state);
        enum pane_mode mode = folio_state_pane_mode(state);
        size_t reads = adapter.note_reads;
        require(rename_category_to(state, 0, "D") == FOLIO_STATE_READY,
                "non-saving category rename succeeds");
        require(same_text(folio_state_category_name(state, 0), "D") &&
                    folio_state_document_kind(state) == kind &&
                    folio_state_pane_mode(state) == mode && folio_state_pane_text(state) == body &&
                    folio_state_pane_rtf(state) == pane && adapter.note_reads == reads &&
                    adapter.archives == 0 && adapter.note_writes == 0,
                "NONE/VIEW/UNTITLED/separate edit preserve document and never save/read");
        struct rename_view pending;
        require(!folio_state_rename_pending(state, &pending),
                "complete adopts and releases intent");
        if (scenario == 2)
        {
            struct note_name *name = accepted_note_name("first");
            struct note_destination target = {.category = 0, .name = name};
            require(folio_state_store_new(state, &target, u"first body", 10) == FOLIO_STATE_READY &&
                        same_text(adapter.written_category, "D"),
                    "first save uses renamed category");
            note_name_destroy(name);
        }
        folio_state_destroy(state);
    }
}

static void verify_category_rename_save(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.history[1] = "old";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "history and edit");
    struct category_name *name = accepted_category_name("D");
    struct category_rename_target target = {.category = 0, .name = name};
    adapter.rename_outcome = RENAME_JOURNAL_FAILED;
    require(folio_state_rename_category(state, &target, u"changed", 7) ==
                FOLIO_STATE_RENAME_JOURNAL_FAILED,
            "rename may fail after successful pre-save");
    require(adapter.archives == 1 && adapter.note_writes == 1 && adapter.renames == 1 &&
                same_text(adapter.written_category, "B") &&
                same_text(folio_state_pane_text(state), "changed") &&
                folio_state_history_count(state) == 0 &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "successful pre-save is retained, including history close, after rename refusal");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_rename_category(state, &target, u"changed", 7) == FOLIO_STATE_READY &&
                adapter.note_writes == 1 && adapter.archives == 1 &&
                same_text(folio_state_pane_title(state).note, "one"),
            "same body is not saved again");
    category_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_category_rename_save_failure(void)
{
    for (size_t scenario = 0; scenario < 2; ++scenario)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = history_state(&adapter);
        require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit to test save failure");
        if (scenario == 0)
        {
            adapter.archive_outcome = PERSISTENCE_UNWRITABLE;
        }
        if (scenario == 1)
        {
            adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
        }
        struct category_name *name = accepted_category_name("D");
        struct category_rename_target target = {.category = 0, .name = name};
        enum folio_state_outcome expected =
            scenario == 0 ? FOLIO_STATE_HISTORY_FAILED : FOLIO_STATE_NOTE_STORE_FAILED;
        require(folio_state_rename_category(state, &target, u"changed", 7) == expected &&
                    adapter.renames == 0 && same_text(folio_state_category_name(state, 0), "B") &&
                    same_text(folio_state_pane_text(state), "# Hello\n\nbody"),
                "failed pre-save stops before plan/port and preserves saved document");
        category_name_destroy(name);
        folio_state_destroy(state);
    }
}

static void verify_category_rename_start_results(void)
{
    const enum rename_outcome outcomes[] = {
        RENAME_COMPLETED,      RENAME_NONE,           RENAME_PENDING,      RENAME_HALTED,
        RENAME_UNLOCKED,       RENAME_NAME_TAKEN,     RENAME_UNSUPPORTED,  RENAME_IDENTITY_FAILED,
        RENAME_JOURNAL_FAILED, RENAME_JOURNAL_BROKEN, RENAME_OUT_OF_MEMORY};
    const enum folio_state_outcome expected[] = {FOLIO_STATE_READY,
                                                 FOLIO_STATE_READY,
                                                 FOLIO_STATE_RENAME_PENDING,
                                                 FOLIO_STATE_RENAME_HALTED,
                                                 FOLIO_STATE_RENAME_UNLOCKED,
                                                 FOLIO_STATE_CATEGORY_NAME_TAKEN,
                                                 FOLIO_STATE_RENAME_UNSUPPORTED,
                                                 FOLIO_STATE_RENAME_IDENTITY_FAILED,
                                                 FOLIO_STATE_RENAME_JOURNAL_FAILED,
                                                 FOLIO_STATE_RENAME_JOURNAL_BROKEN,
                                                 FOLIO_STATE_OUT_OF_MEMORY};
    for (size_t index = 0; index < sizeof outcomes / sizeof outcomes[0]; ++index)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = ready_state(&adapter);
        adapter.rename_outcome = outcomes[index];
        require(rename_category_to(state, 0, "D") == expected[index],
                "all START results stay typed");
        struct rename_view pending;
        bool retained = outcomes[index] == RENAME_PENDING || outcomes[index] == RENAME_HALTED;
        require(folio_state_rename_pending(state, &pending) == retained && adapter.renames == 1,
                "only published pending/halted retain one intent");
        require(same_text(folio_state_category_name(state, 0),
                          outcomes[index] == RENAME_COMPLETED ? "D" : "B"),
                "only COMPLETED adopts prepared category ledger");
        if (retained)
        {
            require(pending.kind == RENAME_KIND_CATEGORY && same_text(pending.from, "B") &&
                        same_text(pending.to, "D"),
                    "pending view names actual category intent");
            adapter.rename_outcome = RENAME_COMPLETED;
            require(folio_state_retry_rename(state) == FOLIO_STATE_READY &&
                        same_text(folio_state_category_name(state, 0), "D"),
                    "RESUME adopts same intent");
        }
        folio_state_destroy(state);
    }
}

static void category_title_document(struct folio_state *_Nonnull state, size_t scenario)
{
    if (scenario == 1 || scenario == 3)
    {
        require(folio_state_select_note(state, scenario == 1 ? 0 : 2, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
                "selected title");
    }
    if (scenario == 2)
    {
        require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
                "untitled title");
    }
}

static void verify_category_rename_pending_titles(void)
{
    for (size_t scenario = 0; scenario < 4; ++scenario)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = ready_state(&adapter);
        category_title_document(state, scenario);
        adapter.rename_outcome = RENAME_PENDING;
        require(rename_category_to(state, 0, "D") == FOLIO_STATE_RENAME_PENDING,
                "pending for title");
        struct pane_title_view title = folio_state_pane_title(state);
        const enum pane_recovery recoveries[] = {PANE_RECOVERY_NONE, PANE_RECOVERY_CATEGORY,
                                                 PANE_RECOVERY_CATEGORY, PANE_RECOVERY_NONE};
        enum pane_recovery recovery = recoveries[scenario];
        require(title.any == (scenario != 0) && title.recovering == recovery,
                "NONE has no false title; only target category is recovering");
        require(rename_category_to(state, 2, "E") == FOLIO_STATE_RENAME_PENDING &&
                    same_text(adapter.renamed_from, "B") && same_text(adapter.renamed_to, "D"),
                "new request cannot overwrite pending category intent");
        adapter.rename_outcome = RENAME_COMPLETED;
        require(folio_state_retry_rename(state) == FOLIO_STATE_READY && adapter.archives == 0 &&
                    adapter.note_writes == 0 && title.any == folio_state_pane_title(state).any &&
                    folio_state_pane_title(state).recovering == PANE_RECOVERY_NONE,
                "generic retry works from NONE/UNTITLED/other document without new saving");
        folio_state_destroy(state);
    }
}

static void verify_category_rename_filter(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"body", 4) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
            "load old corpus then clear filter");
    require(folio_state_scroll_drawer(state, metrics, 40) == FOLIO_STATE_READY,
            "scroll before pending");
    size_t reads = adapter.note_reads;
    size_t old_reads = adapter.old_category_reads;
    adapter.rename_outcome = RENAME_PENDING;
    require(rename_category_to(state, 0, "D") == FOLIO_STATE_RENAME_PENDING &&
                test_index_filter_complete(state, u"body", 4) == FOLIO_STATE_RENAME_PENDING &&
                adapter.note_reads == reads && !folio_state_filtering(state) &&
                same_text(folio_state_index_filter_term(state), ""),
            "pending filter keeps display before read/allocation");
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY &&
                drawer_layout_row(layout, 0).top == -40,
            "pending filter preserves scroll");
    drawer_layout_destroy(layout);
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
            "empty filter remains usable");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(test_index_filter_complete(state, u"body", 4) == FOLIO_STATE_READY &&
                adapter.note_reads == reads + 9 && adapter.old_category_reads == old_reads &&
                folio_state_index_filter_count(state) == 9,
            "completion clears all old-name cache and reloads new paths");
    folio_state_destroy(state);
}

static void verify_category_rename_history(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.history[1] = "old";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY, "history before pending");
    const struct note_text *row = folio_state_history_body(state, 0);
    size_t reads = adapter.history_reads;
    adapter.rename_outcome = RENAME_HALTED;
    require(rename_category_to(state, 0, "D") == FOLIO_STATE_RENAME_HALTED &&
                folio_state_open_history(state) == FOLIO_STATE_RENAME_HALTED &&
                folio_state_history_body(state, 0) == row && adapter.history_reads == reads,
            "pending target history syncs before discarding rows or reading old path");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_retry_rename(state) == FOLIO_STATE_READY &&
                folio_state_history_body(state, 0) == row,
            "pure category adoption preserves loaded history rows");
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                same_text(adapter.history_category, "D"),
            "later history read follows new category path");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.history[1] = "other";
    state = ready_state(&adapter);
    require(folio_state_select_note(state, 2, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "separate history target");
    adapter.rename_outcome = RENAME_PENDING;
    require(rename_category_to(state, 0, "D") == FOLIO_STATE_RENAME_PENDING &&
                folio_state_open_history(state) == FOLIO_STATE_READY && adapter.renames == 1 &&
                same_text(adapter.history_category, "C"),
            "separate history is usable without resuming category intent");
    folio_state_destroy(state);
}

static void category_preview(struct folio_state *_Nonnull state, const char16_t *_Nonnull pattern)
{
    struct replace_request request = {.text = u"# Hello\n\nbody",
                                      .length = 13,
                                      .pattern = pattern,
                                      .pattern_length = 4,
                                      .replacement = u"new",
                                      .replacement_length = 3};
    require(folio_state_preview_replace(state, &request) == FOLIO_STATE_READY &&
                folio_state_has_replace_preview(state),
            "preview is valid even with zero matches");
}

static void category_rename_preview_case(size_t scenario)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, scenario == 2 ? 2 : 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "preview document");
    category_preview(state, u"body");
    adapter.rename_outcome = RENAME_PENDING;
    struct category_name *name = accepted_category_name("D");
    struct category_rename_target target = {.category = 0, .name = name};
    require(folio_state_rename_category(state, &target, u"# Hello\n\nbody", 13) ==
                FOLIO_STATE_RENAME_PENDING,
            "unchanged pre-save retains preview");
    category_preview(state, scenario == 1 ? u"none" : u"body");
    require(folio_state_replace_count(state) == (scenario == 1 ? 0 : 1),
            "new pending preview, including zero");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY &&
                folio_state_has_replace_preview(state) == (scenario == 2),
            "generic synchronization invalidates latest target preview only");
    if (scenario == 2)
    {
        expect_trash_preview_usable(state);
    }
    category_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_category_rename_preview(void)
{
    for (size_t scenario = 0; scenario < 3; ++scenario)
    {
        category_rename_preview_case(scenario);
    }
}

static void verify_category_rename_layout(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 2, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "keep later selection");
    char cursor[64];
    cursor_name(state, cursor, sizeof cursor);
    require(rename_category_to(state, 1, "D") == FOLIO_STATE_READY &&
                keeps_document(state, at(2, 1), PANE_MODE_VIEW),
            "all document indices survive");
    expect_cursor(state, cursor, "cursor index survives category adoption");
    struct drawer_layout *layout = nullptr;
    require(folio_state_drawer_layout(state, metrics, &layout) == FOLIO_STATE_READY &&
                drawer_layout_row_count(layout) == 9,
            "row count and collapsed state survive");
    struct drawer_row first = drawer_layout_row(layout, 0);
    struct drawer_row target = drawer_layout_row(layout, 4);
    require(same_text(first.text, "B") && first.expanded &&
                has_color(first.color, 0x11, 0x11, 0x11),
            "preceding category metadata is untouched");
    require(same_text(target.text, "D") && target.category == 1 && !target.expanded &&
                has_color(target.color, 0x22, 0x22, 0x22),
            "target keeps position/color/collapsed state");
    require(same_text(drawer_layout_row(layout, 2).text, "two") &&
                same_text(drawer_layout_row(layout, 3).text, "three") &&
                same_text(drawer_layout_row(layout, 5).text, "C"),
            "all note ledgers and order are untouched");
    drawer_layout_destroy(layout);
    folio_state_destroy(state);
}

static void verify_category_rename_store_sync(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "target edit for store retry");
    struct category_name *name = accepted_category_name("D");
    struct category_rename_target target = {.category = 0, .name = name};
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_category(state, &target, u"# Hello\n\nbody", 13) ==
                FOLIO_STATE_RENAME_PENDING,
            "unchanged target enters pending");
    require(folio_state_store_note(state, u"later", 5) == FOLIO_STATE_RENAME_PENDING &&
                adapter.note_writes == 0 && adapter.archives == 0,
            "pending blocks old-path saving");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_store_note(state, u"later", 5) == FOLIO_STATE_READY &&
                adapter.note_writes == 1 && adapter.archives == 1 &&
                same_text(adapter.written_category, "D") &&
                same_text(adapter.archived_category, "D"),
            "generic save resumes before writing the new path");
    category_name_destroy(name);
    folio_state_destroy(state);
}

void run_category_rename_state_tests(void)
{
    finder = test_regex_port();
    verify_category_rename_refusals();
    verify_category_rename_scope();
    verify_category_rename_save();
    verify_category_rename_save_failure();
    verify_category_rename_start_results();
    verify_category_rename_pending_titles();
    verify_category_rename_filter();
    verify_category_rename_history();
    verify_category_rename_preview();
    verify_category_rename_layout();
    verify_category_rename_store_sync();
}

static void category_preparation_failed(struct folio_state *_Nonnull state,
                                        const struct persistence_adapter *_Nonnull adapter)
{
    struct rename_view view;
    require(adapter->renames == 0 && !folio_state_rename_pending(state, &view) &&
                same_text(folio_state_category_name(state, 0), "B"),
            "all preparation OOM points precede START and leak no intent");
    require(folio_state_history_count(state) == (adapter->note_writes == 0 ? 1 : 0) &&
                same_text(folio_state_pane_text(state),
                          adapter->note_writes == 0 ? "# Hello\n\nbody" : "changed"),
            "pre-save adoption remains true when later preparation fails");
}

static void category_preparation_completed(struct folio_state *_Nonnull state,
                                           struct persistence_adapter *_Nonnull adapter)
{
    adapter->rename_outcome = RENAME_COMPLETED;
    size_t reads = adapter->note_reads;
    allocation_probe_fail_at(1);
    require(folio_state_retry_rename(state) == FOLIO_STATE_READY,
            "completed category adoption requires no extra allocation");
    allocation_probe_fail_at(0);
    require(adapter->note_reads == reads && same_text(folio_state_category_name(state, 0), "D"),
            "pending owns prepared corpus and ledger through allocation-free adoption");
}

static bool category_state_allocation(size_t nth, bool saved, bool cached)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.history[1] = "old";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "allocation setup without injection");
    if (cached)
    {
        require(test_index_filter_complete(state, u"body", 4) == FOLIO_STATE_READY &&
                    test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
                "pre-save cache allocation boundary");
    }
    struct category_name *name = accepted_category_name("D");
    struct category_rename_target target = {.category = 0, .name = name};
    adapter.rename_outcome = RENAME_PENDING;
    allocation_probe_fail_at(nth);
    enum folio_state_outcome outcome = folio_state_rename_category(
        state, &target, saved ? u"changed" : u"# Hello\n\nbody", saved ? 7 : 13);
    allocation_probe_fail_at(0);
    require(outcome == FOLIO_STATE_OUT_OF_MEMORY || outcome == FOLIO_STATE_RENAME_PENDING,
            "every preparation allocation failure stays typed");
    if (outcome == FOLIO_STATE_OUT_OF_MEMORY)
    {
        category_preparation_failed(state, &adapter);
    }
    if (outcome == FOLIO_STATE_RENAME_PENDING)
    {
        category_preparation_completed(state, &adapter);
    }
    category_name_destroy(name);
    folio_state_destroy(state);
    return outcome == FOLIO_STATE_RENAME_PENDING;
}

static void verify_note_retry_allocation(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    require(test_index_filter_complete(state, u"body", 4) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
            "NOTE retry has loaded corpus");
    struct note_name *name = accepted_note_name("new");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "retain original NOTE pending contract");
    adapter.rename_outcome = RENAME_COMPLETED;
    allocation_probe_fail_at(1);
    require(folio_state_retry_rename(state) == FOLIO_STATE_OUT_OF_MEMORY,
            "NOTE retry can report corpus OOM after disk completion");
    allocation_probe_fail_at(0);
    struct rename_view view;
    require(!folio_state_rename_pending(state, &view) &&
                same_text(folio_state_document_name(state), "new"),
            "adopted NOTE has no retry intent after cache OOM; borrowed view must be reacquired");
    note_name_destroy(name);
    folio_state_destroy(state);
}

void run_category_rename_allocation_tests(void)
{
    allocation_probe_fail_at(1);
    struct category_name *check = nullptr;
    enum category_name_outcome active = category_name_create("D", 1, &check);
    allocation_probe_fail_at(0);
    category_name_destroy(check);
    if (active != CATEGORY_NAME_OUT_OF_MEMORY)
    {
        return;
    }
    finder = test_regex_port();
    verify_note_retry_allocation();
    for (size_t scenario = 0; scenario < 3; ++scenario)
    {
        bool completed = false;
        size_t completed_at = 0;
        for (size_t nth = 1; nth < 512 && !completed; ++nth)
        {
            completed = category_state_allocation(nth, scenario != 0, scenario == 2);
            completed_at = nth;
        }
        require(completed, "category rename allocation scenario completes");
        printf("category state allocation scenario %zu: %zu failed points, then complete\n",
               scenario, completed_at - 1);
    }
}

/* 改名で旧mdが消えてから初めて検索する順序（#219）。完了後の新名を欠落させない。 */
static void verify_note_pending_first_filter(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select before first filter");
    struct note_name *name = accepted_note_name("newnote");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "md can move before the rename completes");
    adapter.missing_note = "one";
    enum folio_state_outcome filtered = test_index_filter_complete(state, u"newnote", 7);
    require(filtered == FOLIO_STATE_RENAME_PENDING && adapter.note_reads == 1,
            "first filter synchronizes NOTE before any old-path cache read");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_retry_rename(state) == FOLIO_STATE_READY, "finish NOTE rename");
    require(test_index_filter_complete(state, u"newnote", 7) == FOLIO_STATE_READY,
            "search the completed name");
    require(folio_state_index_filter_count(state) == 1,
            "completed NOTE remains searchable after first filter during pending");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_note_filter_refusals(void);

void run_index_cache_retry_state_tests(void)
{
    finder = test_regex_port();
    verify_note_pending_first_filter();
    verify_note_filter_refusals();
}

static void verify_saved_copy_retry(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY,
            "cache holds the old saved body");
    adapter.copy_fail_at = 1;
    require(folio_state_store_note(state, u"needle", 6) == FOLIO_STATE_OUT_OF_MEMORY,
            "copy OOM occurs after successful md publication");
    allocation_probe_fail_at(0);
    adapter.copy_fail_at = 0;
    require(same_text(adapter.written_body, "needle") &&
                same_text(folio_state_pane_text(state), "needle"),
            "published md and owned body already hold the new text");
    require(folio_state_store_note(state, u"needle", 6) == FOLIO_STATE_READY &&
                adapter.note_writes == 1 && adapter.archives == 1,
            "same-body retry does not write or archive again");
    require(folio_state_index_filter_count(state) == 1,
            "same-body retry repairs the saved copy and match set");
    folio_state_destroy(state);
}

static void verify_copy_allocation_boundaries(void);

void run_index_cache_retry_allocation_tests(void)
{
    allocation_probe_fail_at(1);
    struct note_text *check = nullptr;
    enum note_text_outcome active = note_text_create("", 0, &check);
    allocation_probe_fail_at(0);
    note_text_destroy(check);
    if (active != NOTE_TEXT_OUT_OF_MEMORY)
    {
        return;
    }
    finder = test_regex_port();
    verify_saved_copy_retry();
    verify_copy_allocation_boundaries();
}

static void note_filter_refusal_case(enum rename_outcome pending, enum folio_state_outcome expected)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"Hello", 5) == FOLIO_STATE_READY &&
                folio_state_scroll_drawer(state, scroll_metrics, 20) == FOLIO_STATE_READY,
            "keep an existing match set and scroll before NOTE pending");
    struct note_name *name = accepted_note_name("newnote");
    adapter.rename_outcome = pending;
    require(folio_state_rename_note(state, name, u"", 0) == expected, "hold NOTE intent");
    adapter.missing_note = "one";
    size_t reads = adapter.note_reads;
    int top = scrolled_top(state);
    require(test_index_filter_complete(state, u"newnote", 7) == expected &&
                adapter.note_reads == reads && scrolled_top(state) == top &&
                folio_state_index_filter_count(state) == 9 &&
                same_text(folio_state_index_filter_term(state), "Hello"),
            "pending and halted filter attempts keep rows, term and scroll without old reads");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY &&
                !folio_state_filtering(state) && adapter.note_reads == reads,
            "empty filter can clear while NOTE recovery is blocked");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(test_index_filter_complete(state, u"newnote", 7) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads,
            "filter itself resumes NOTE then uses rekeyed cached body");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_note_filter_refusals(void)
{
    note_filter_refusal_case(RENAME_PENDING, FOLIO_STATE_RENAME_PENDING);
    note_filter_refusal_case(RENAME_HALTED, FOLIO_STATE_RENAME_HALTED);
}

static struct folio_state *_Nonnull copy_test_state(struct persistence_adapter *_Nonnull adapter)
{
    struct folio_state *state = ready_state(adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY,
            "old saved text is loaded into the search corpus");
    return state;
}

static enum folio_state_outcome copy_publication(struct folio_state *_Nonnull state,
                                                 struct persistence_adapter *_Nonnull adapter,
                                                 size_t nth)
{
    adapter->copy_fail_at = nth;
    enum folio_state_outcome saved = folio_state_store_note(state, u"needle", 6);
    allocation_probe_fail_at(0);
    adapter->copy_fail_at = 0;
    return saved;
}

static void copy_retry_refused(struct folio_state *_Nonnull state,
                               const struct persistence_adapter *_Nonnull adapter)
{
    const char *body = folio_state_pane_text(state);
    size_t matches = folio_state_index_filter_count(state);
    allocation_probe_fail_at(1);
    enum folio_state_outcome saved = folio_state_store_note(state, u"needle", 6);
    allocation_probe_fail_at(0);
    require(saved == FOLIO_STATE_OUT_OF_MEMORY && folio_state_pane_text(state) == body &&
                folio_state_index_filter_count(state) == matches &&
                keeps_document(state, at(0, 0), PANE_MODE_EDIT) && adapter->archives == 1 &&
                adapter->note_writes == 1,
            "another copy OOM retains the canonical body and blocks extra archive/write");
}

static bool copy_allocation_case(size_t nth)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = copy_test_state(&adapter);
    size_t reads = adapter.note_reads;
    enum folio_state_outcome saved = copy_publication(state, &adapter, nth);
    require(saved == FOLIO_STATE_READY || saved == FOLIO_STATE_OUT_OF_MEMORY,
            "each post-publication copy allocation remains typed");
    if (saved == FOLIO_STATE_OUT_OF_MEMORY)
    {
        copy_retry_refused(state, &adapter);
    }
    require(folio_state_store_note(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads &&
                adapter.archives == 1 && adapter.note_writes == 1,
            "same-body retry repairs every put/filter OOM with no disk read or repeated write");
    folio_state_destroy(state);
    return saved == FOLIO_STATE_READY;
}

static void verify_copy_publication_allocations(void)
{
    for (size_t nth = 1; nth <= 16; ++nth)
    {
        if (copy_allocation_case(nth))
        {
            printf("saved copy allocation points: %zu\n", nth - 1);
            return;
        }
    }
    require(false, "all copy publication allocations must eventually complete");
}

static void verify_copy_filter_reentry(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = copy_test_state(&adapter);
    require(copy_publication(state, &adapter, 1) == FOLIO_STATE_OUT_OF_MEMORY,
            "copy needs repair before filter reentry");
    size_t reads = adapter.note_reads;
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
            "clearing the filter does not pretend the saved copy was repaired");
    allocation_probe_fail_at(1);
    enum folio_state_outcome filtered = test_index_filter_complete(state, u"needle", 6);
    allocation_probe_fail_at(0);
    require(filtered == FOLIO_STATE_OUT_OF_MEMORY && !folio_state_filtering(state) &&
                same_text(folio_state_index_filter_term(state), "") && adapter.note_reads == reads,
            "nonempty filter waits for copy repair and retains the cleared state on OOM");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads,
            "reentry uses the owned saved body, not another read of old fake md bytes");
    folio_state_destroy(state);
}

/* One attempt with the nth allocation of the repairing save failing. True when it fell where
 * the test means it: after the waiting index was written, in the repair of the saved copy.
 * #1069 / ADR0117 (stage 4): the index is read again before it is written, so the first
 * allocations of that save are the reading's; a failure there leaves the index waiting and
 * is not this test's case. */
static bool copy_new_document_at(bool untitled, size_t nth)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = copy_test_state(&adapter);
    if (untitled)
    {
        require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
                "first save shares the new md publication boundary");
    }
    struct note_name *name = accepted_note_name("copied");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.copy_fail_at = 1;
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"needle", 6) == FOLIO_STATE_LEDGER_STALE,
            "new md publication retains both pending index and copy after failure");
    allocation_probe_fail_at(0);
    adapter.copy_fail_at = 0;
    require(same_text(folio_state_document_name(state), "copied") &&
                same_text(folio_state_pane_text(state), "needle") && adapter.creates == 1,
            "published copy remains the current saved document");
    require(folio_state_store_note(state, u"needle", 6) == FOLIO_STATE_LEDGER_UNSYNCED,
            "index repair precedes copy repair");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    allocation_probe_fail_at(nth);
    enum folio_state_outcome saved = folio_state_store_note(state, u"needle", 6);
    allocation_probe_fail_at(0);
    if (saved != FOLIO_STATE_OUT_OF_MEMORY || adapter.ledger_writes != 3)
    {
        require(saved == FOLIO_STATE_READY || saved == FOLIO_STATE_OUT_OF_MEMORY,
                "a failure while the index is read again is typed");
        note_name_destroy(name);
        folio_state_destroy(state);
        return false;
    }
    require(folio_state_end_edit(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW && adapter.creates == 1 &&
                adapter.note_writes == 0 && adapter.archives == 0 && adapter.ledger_writes == 3,
            "copy retry neither republishes new md nor saves the original");
    note_name_destroy(name);
    folio_state_destroy(state);
    return true;
}

static void verify_copy_new_document(bool untitled)
{
    for (size_t nth = 1; nth < 64; ++nth)
        if (copy_new_document_at(untitled, nth))
            return;
    require(false, "repaired index can be followed by another copy OOM");
}

static enum folio_state_outcome copy_target_change(struct folio_state *_Nonnull state,
                                                   size_t scenario)
{
    if (scenario == 0)
    {
        return folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT);
    }
    if (scenario == 1)
    {
        return folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT);
    }
    if (scenario == 2)
    {
        return folio_state_move_note(state, at(0, 0), at(0, 2));
    }
    return folio_state_trash_note(state, at(0, 0), u"needle", 6);
}

static void copy_target_change_case(size_t scenario)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = copy_test_state(&adapter);
    require(copy_publication(state, &adapter, 1) == FOLIO_STATE_OUT_OF_MEMORY,
            "target still owns a copy repair");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY,
            "allow ledger operations without resolving the pending copy");
    size_t reads = adapter.note_reads;
    allocation_probe_fail_at(1);
    enum folio_state_outcome changed = copy_target_change(state, scenario);
    allocation_probe_fail_at(0);
    require(changed == FOLIO_STATE_OUT_OF_MEMORY &&
                keeps_document(state, at(0, 0), PANE_MODE_EDIT) &&
                same_text(folio_state_pane_text(state), "needle") && adapter.note_reads == reads &&
                adapter.ledger_writes == 0 && adapter.trashes == 0,
            "copy OOM prevents body replacement, index change and trash publication");
    require(copy_target_change(state, scenario) == FOLIO_STATE_READY,
            "target intent can proceed once its old saved copy is repaired");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == (scenario == 3 ? 0 : 1) &&
                adapter.note_reads == reads + (scenario == 0 ? 1 : 0),
            "the saved copy is repaired under its original name before target mutation");
    folio_state_destroy(state);
}

static size_t same_save_allocation_count(bool loaded)
{
    for (size_t nth = 1; nth <= 32; ++nth)
    {
        struct persistence_adapter adapter = healthy_adapter();
        adapter.note_body = "needle";
        struct folio_state *state = ready_state(&adapter);
        require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                    folio_state_begin_edit(state) == FOLIO_STATE_READY,
                "prepare same-body save");
        if (loaded)
        {
            require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY,
                    "same-body save with loaded search cache");
        }
        allocation_probe_fail_at(nth);
        enum folio_state_outcome saved = folio_state_store_note(state, u"needle", 6);
        allocation_probe_fail_at(0);
        require(adapter.archives == 0 && adapter.note_writes == 0,
                "normal unchanged saves never publish again");
        folio_state_destroy(state);
        if (saved == FOLIO_STATE_READY)
        {
            return nth - 1;
        }
        require(saved == FOLIO_STATE_OUT_OF_MEMORY, "normalization allocation fails explicitly");
    }
    require(false, "unchanged save normalization must eventually complete");
    return 0;
}

static void verify_copy_view_query(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY,
            "viewing source has a loaded search cache");
    struct note_name *name = accepted_note_name("copied");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.copy_fail_at = 1;
    require(folio_state_store_new(state, &destination, u"ignored", 7) == FOLIO_STATE_OUT_OF_MEMORY,
            "view save-as adopts raw saved text then copy OOM");
    allocation_probe_fail_at(0);
    adapter.copy_fail_at = 0;
    size_t reads = adapter.note_reads;
    enum folio_note_change change = FOLIO_NOTE_CHANGED;
    allocation_probe_fail_at(1);
    enum folio_state_outcome queried = folio_state_note_changed(state, u"", 0, &change);
    allocation_probe_fail_at(0);
    require(queried == FOLIO_STATE_OUT_OF_MEMORY && change == FOLIO_NOTE_CHANGED &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "quit query repairs before reporting SAME and leaves output on failed repair");
    require(folio_state_note_changed(state, u"", 0, &change) == FOLIO_STATE_READY &&
                change == FOLIO_NOTE_SAME &&
                test_index_filter_complete(state, u"copied", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads &&
                adapter.creates == 1 && adapter.archives == 0 && adapter.note_writes == 0,
            "view query repairs the copy without input normalization or repeat publication");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_copy_filter_failure(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = copy_test_state(&adapter);
    require(test_index_filter_complete(state, u"Hello", 5) == FOLIO_STATE_READY &&
                folio_state_scroll_drawer(state, scroll_metrics, 20) == FOLIO_STATE_READY &&
                copy_publication(state, &adapter, 1) == FOLIO_STATE_OUT_OF_MEMORY,
            "saved copy is pending while the old term and rows remain");
    size_t reads = adapter.note_reads;
    int top = scrolled_top(state);
    allocation_probe_fail_at(1);
    enum folio_state_outcome filtered = test_index_filter_complete(state, u"needle", 6);
    allocation_probe_fail_at(0);
    require(filtered == FOLIO_STATE_OUT_OF_MEMORY && scrolled_top(state) == top &&
                folio_state_index_filter_count(state) == 9 &&
                same_text(folio_state_index_filter_term(state), "Hello") &&
                adapter.note_reads == reads,
            "new search term cannot replace rows or scroll before copy repair succeeds");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && scrolled_top(state) == 0 &&
                adapter.note_reads == reads,
            "search proceeds from the repaired saved body with no whole-corpus reload");
    folio_state_destroy(state);
}

static void verify_copy_allocation_boundaries(void)
{
    verify_copy_publication_allocations();
    verify_copy_filter_reentry();
    verify_copy_new_document(false);
    verify_copy_new_document(true);
    verify_copy_view_query();
    verify_copy_filter_failure();
    for (size_t scenario = 0; scenario < 4; ++scenario)
    {
        copy_target_change_case(scenario);
    }
    size_t unloaded = same_save_allocation_count(false);
    size_t loaded = same_save_allocation_count(true);
    require(unloaded == loaded, "normal same-save adds no cache allocation");
    printf("unchanged-save allocations loaded/unloaded: %zu/%zu\n", loaded, unloaded);
}

static void verify_size_named(const char16_t *_Nonnull units, size_t count)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "M";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit size fixture");
    const char *old_body = folio_state_pane_text(state);
    require(folio_state_store_note(state, units, count) == FOLIO_STATE_NOTE_TOO_LARGE &&
                folio_state_end_edit(state, units, count) == FOLIO_STATE_NOTE_TOO_LARGE,
            "normal save and leaving EDIT reject oversized body");
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, units, count) == FOLIO_STATE_NOTE_TOO_LARGE,
            "EDIT Save As rejects before new md preparation");
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, units, count, &changed) == FOLIO_STATE_NOTE_TOO_LARGE &&
                changed == FOLIO_NOTE_SAME,
            "named EDIT :q query fails with output unchanged");
    require(folio_state_pane_text(state) == old_body && same_text(old_body, "M") &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED &&
                same_text(folio_state_document_name(state), "one") &&
                folio_state_document_category(state) == 0 && adapter.archives == 0 &&
                adapter.note_writes == 0 && adapter.creates == 0 && adapter.ledger_writes == 0,
            "size failure retains mode/body/target and no publication/archive");
    require(folio_state_store_note(state, u"M", 1) == FOLIO_STATE_READY && adapter.archives == 0 &&
                adapter.note_writes == 0,
            "same-body save remains a no-op");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_size_untitled(const char16_t *_Nonnull units, size_t count)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "M";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "new size fixture");
    const char *body = folio_state_pane_text(state);
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, units, count) ==
                    FOLIO_STATE_NOTE_TOO_LARGE &&
                folio_state_pane_text(state) == body &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT && adapter.creates == 0 &&
                adapter.archives == 0 && adapter.note_writes == 0,
            "first save keeps untitled state before render/create/archive");
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, units, count, &changed) == FOLIO_STATE_READY &&
                changed == FOLIO_NOTE_CHANGED,
            "untitled query retains the existing CHANGED early return");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_size_view(const char16_t *_Nonnull units, size_t count)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "M";
    struct folio_state *state = history_state(&adapter);
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, units, count) == FOLIO_STATE_READY &&
                adapter.creates == 1 && same_text(adapter.written_body, "M") &&
                adapter.archives == 0 && adapter.note_writes == 0 &&
                folio_state_pane_mode(state) == PANE_MODE_VIEW,
            "VIEW Save As ignores editor input and copies the owned valid Markdown");
    enum folio_note_change changed = FOLIO_NOTE_CHANGED;
    require(folio_state_note_changed(state, units, count, &changed) == FOLIO_STATE_READY &&
                changed == FOLIO_NOTE_SAME,
            "VIEW query retains existing SAME early return");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_size_after_index_repair(const char16_t *_Nonnull units, size_t count)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "M";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "pending index size fixture");
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_new(state, &destination, u"M", 1) == FOLIO_STATE_LEDGER_STALE &&
                adapter.creates == 1 && adapter.ledger_writes == 1,
            "existing published index pending precedes the oversized intent");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note(state, units, count) == FOLIO_STATE_NOTE_TOO_LARGE &&
                adapter.ledger_writes == 2 && adapter.creates == 1 && adapter.archives == 0 &&
                adapter.note_writes == 0 && same_text(folio_state_document_name(state), "copy"),
            "prior index repair remains allowed before current body size refusal");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_size_expansion(char16_t *_Nonnull units)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "M\r\n";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "CRLF size fixture");
    units[0] = u'\r';
    require(folio_state_store_note(state, units, persisted_size_limit) ==
                    FOLIO_STATE_NOTE_TOO_LARGE &&
                adapter.archives == 0 && adapter.note_writes == 0,
            "application rejects CRLF expansion despite raw length within limit");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.note_body = "M\n";
    state = history_state(&adapter);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "LF size fixture");
    units[1] = u'\n';
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, units, persisted_size_limit + 1, &changed) ==
                    FOLIO_STATE_READY &&
                changed == FOLIO_NOTE_CHANGED && adapter.archives == 0 &&
                adapter.note_writes == 0 && adapter.creates == 0,
            "application accepts CRLF shrinking to canonical limit without publishing");
    folio_state_destroy(state);
}

static void verify_size_languages(void)
{
    require(same_text(folio_state_failure_line(FOLIO_STATE_NOTE_TOO_LARGE, FOLIO_LANGUAGE_JA),
                      "本文が大きすぎて保存できません。上限は 16 MiB "
                      "です。本文を減らしてから保存してください。"),
            "size failure Japanese reason");
    require(same_text(folio_state_failure_line(FOLIO_STATE_NOTE_TOO_LARGE, FOLIO_LANGUAGE_EN),
                      "This note is too large to save. The limit is 16 MiB. Shorten the note, then "
                      "save again."),
            "size failure English reason");
    require(same_text(folio_state_failure_line(FOLIO_STATE_NOTE_TOO_LARGE, FOLIO_LANGUAGE_ZH_HANS),
                      "笔记过大，无法保存。上限为 16 MiB。请缩短内容后再保存。"),
            "size failure simplified Chinese reason");
}

void run_note_size_state_tests(void)
{
    finder = test_regex_port();
    char16_t *units = malloc((persisted_size_limit + 2) * sizeof *units);
    if (units == nullptr)
    {
        require(false, "editor size fixture allocated");
        return;
    }
    for (size_t index = 0; index < persisted_size_limit + 2; ++index)
    {
        units[index] = u'X';
    }
    verify_size_named(units, persisted_size_limit + 1);
    verify_size_untitled(units, persisted_size_limit + 1);
    verify_size_view(units, persisted_size_limit + 1);
    verify_size_after_index_repair(units, persisted_size_limit + 1);
    verify_size_expansion(units);
    verify_size_languages();
    free(units);
}

static void verify_nul_named(void)
{
    const char16_t units[] = {u'A', u'\0', u'B'};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "kept";
    adapter.history[1] = "old";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_open_history(state) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "named NUL fixture has an owned body and history");
    const char *body = folio_state_pane_text(state);
    const struct note_text *history = folio_state_history_body(state, 0);
    require(folio_state_store_note(state, units, 3) == FOLIO_STATE_NOTE_MALFORMED &&
                folio_state_end_edit(state, units, 3) == FOLIO_STATE_NOTE_MALFORMED,
            "save and leave EDIT reject embedded NUL");
    struct note_name *name = accepted_note_name("copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, units, 3) == FOLIO_STATE_NOTE_MALFORMED,
            "Save As rejects NUL before publication");
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    require(folio_state_note_changed(state, units, 3, &changed) == FOLIO_STATE_NOTE_MALFORMED &&
                changed == FOLIO_NOTE_SAME,
            "quit query refuses NUL with output unchanged");
    require(folio_state_pane_text(state) == body && same_text(body, "kept") &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                same_text(folio_state_document_name(state), "one") &&
                folio_state_document_category(state) == 0 &&
                folio_state_history_body(state, 0) == history &&
                folio_state_history_count(state) == 1 && adapter.archives == 0 &&
                adapter.note_writes == 0 && adapter.creates == 0 && adapter.ledger_writes == 0,
            "NUL refusal preserves body mode destination and history without writes");
    require(folio_state_store_note(state, u"kept", 4) == FOLIO_STATE_READY &&
                adapter.note_writes == 0 && adapter.archives == 0,
            "valid unchanged input remains saveable after refusal");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_nul_untitled(void)
{
    const char16_t units[] = {u'\0', u'B'};
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = history_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled NUL fixture");
    const char *body = folio_state_pane_text(state);
    struct note_name *name = accepted_note_name("first");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, units, 2) == FOLIO_STATE_NOTE_MALFORMED &&
                folio_state_pane_text(state) == body &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT && adapter.creates == 0 &&
                adapter.archives == 0 && adapter.note_writes == 0,
            "first save rejects NUL without leaving the untitled editor");
    note_name_destroy(name);
    folio_state_destroy(state);
}

void run_note_nul_state_tests(void)
{
    finder = test_regex_port();
    verify_nul_named();
    verify_nul_untitled();
}

/* UI本文を所有するRichEditとは独立に、applicationの表示採用は文書/portを触らない。 */
void run_layout_mode_state_tests(void)
{
    finder = test_regex_port();
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "original body";
    struct folio_state *state = history_state(&adapter);
    require(folio_state_layout_mode(state) == FOLIO_LAYOUT_BOTH, "layout starts with both regions");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "layout fixture is editing");
    const char *body = folio_state_pane_text(state);
    const char *rtf = folio_state_pane_rtf(state);
    struct persistence_adapter before;
    memcpy(&before, &adapter, sizeof before);
    static const enum folio_layout_mode modes[] = {
        FOLIO_LAYOUT_DRAWER, FOLIO_LAYOUT_NOTE,   FOLIO_LAYOUT_BOTH, FOLIO_LAYOUT_NOTE,
        FOLIO_LAYOUT_NOTE,   FOLIO_LAYOUT_DRAWER, FOLIO_LAYOUT_BOTH};
    for (size_t index = 0; index < sizeof modes / sizeof modes[0]; ++index)
    {
        folio_state_set_layout_mode(state, modes[index]);
        require(folio_state_layout_mode(state) == modes[index] &&
                    folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                    folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED,
                "display adoption leaves document kind and editing mode alone");
        require(folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == rtf &&
                    same_text(body, "original body"),
                "display adoption keeps the owned body and render value");
        require(memcmp(&before, &adapter, sizeof before) == 0,
                "display adoption does not invoke persistence or mutate adapter observations");
    }
    /* #975 / ADR0114: the headings column is a display state of this run, closed at first. */
    require(!folio_state_outline(state), "the headings column starts closed");
    for (int round = 0; round < 2; ++round)
    {
        folio_state_set_outline(state, round == 0);
        require(folio_state_outline(state) == (round == 0) &&
                    folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                    folio_state_pane_text(state) == body && folio_state_pane_rtf(state) == rtf &&
                    memcmp(&before, &adapter, sizeof before) == 0,
                "opening and closing the headings column touch neither the note nor the ports");
    }
    folio_state_destroy(state);
}

void run_theme_state_tests(void)
{
    finder = test_regex_port();
    verify_settings();
}

/* ADR0050: permission authorizes only this transition, never an intermediate empty document. */
static void verify_untitled_departure(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "prepare an untitled departure");
    const char *pane = folio_state_pane_rtf(state);
    const char *edit = folio_state_edit_rtf(state);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_NAME_REQUIRED &&
                folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_NAME_REQUIRED,
            "the default permission protects an untitled document");
    require(folio_state_select_note(state, 99, 0, FOLIO_UNTITLED_DISCARD) ==
                    FOLIO_STATE_NO_SUCH_CATEGORY &&
                folio_state_select_note(state, 0, 99, FOLIO_UNTITLED_DISCARD) ==
                    FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_new_note(state, 99, FOLIO_UNTITLED_DISCARD) ==
                    FOLIO_STATE_NO_SUCH_CATEGORY,
            "discard does not invent destinations");
    adapter.note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_DISCARD) ==
                    FOLIO_STATE_NOTE_UNREADABLE &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_document_category(state) == 1 && folio_state_pane_rtf(state) == pane &&
                folio_state_edit_rtf(state) == edit &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "unreadable replacement preserves the entire held untitled projection");
    adapter.note_outcome = PERSISTENCE_LOADED;
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_DISCARD) == FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED &&
                folio_state_edit_rtf_length(state) != 0,
            "a successful replacement already owns its editor projection");
    require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_new_note(state, 0, FOLIO_UNTITLED_DISCARD) == FOLIO_STATE_READY &&
                folio_state_document_category(state) == 0 &&
                folio_state_edit_rtf_length(state) != 0,
            "next new document prepares its empty editor before adoption");
    require(adapter.creates == 0 && adapter.note_writes == 0 && adapter.archives == 0 &&
                adapter.ledger_writes == 0 && adapter.writes == 0,
            "discarded drafts never create md, history or an index row");
    folio_state_destroy(state);
}

static void verify_untitled_expand_permission(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "prepare the index cursor");
    require(folio_state_set_category_expanded(state, 0, false, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_FIRST, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
            "a category cursor can move while holding an untitled document");
    require(folio_state_category_replaces_document(state, 0, true),
            "collapsed category with a note will replace the untitled document");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY &&
                !folio_state_category_replaces_document(state, 0, true) &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED,
            "mouse expansion followed by an already-expanded l never requests discard");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_READY,
            "restore the collapsed category for the actual replacement");
    require(folio_state_set_category_expanded(state, 0, true, FOLIO_UNTITLED_DISCARD) ==
                    FOLIO_STATE_READY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED &&
                folio_state_edit_rtf_length(state) != 0 && adapter.creates == 0 &&
                adapter.archives == 0,
            "expansion passes the local discard permission to the same prepared selection");
    folio_state_destroy(state);
}

static size_t departure_allocation_boundary(bool next_new)
{
    for (size_t nth = 1; nth < 128; ++nth)
    {
        struct persistence_adapter adapter = healthy_adapter();
        struct folio_state *state = ready_state(&adapter);
        require(folio_state_new_note(state, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
                "prepare allocation-failure departure");
        const char *pane = folio_state_pane_rtf(state);
        const char *edit = folio_state_edit_rtf(state);
        allocation_probe_fail_at(nth);
        enum folio_state_outcome outcome =
            next_new ? folio_state_new_note(state, 0, FOLIO_UNTITLED_DISCARD)
                     : folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_DISCARD);
        allocation_probe_fail_at(0);
        if (outcome == FOLIO_STATE_OUT_OF_MEMORY)
        {
            require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                        folio_state_document_category(state) == 1 &&
                        folio_state_pane_rtf(state) == pane &&
                        folio_state_edit_rtf(state) == edit &&
                        folio_state_pane_mode(state) == PANE_MODE_EDIT,
                    "every failed body/view/editor allocation keeps the prior draft");
        }
        require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_OUT_OF_MEMORY,
                "departure has explicit allocation outcomes");
        require(adapter.creates == 0 && adapter.archives == 0 && adapter.note_writes == 0 &&
                    adapter.ledger_writes == 0 && adapter.writes == 0,
                "allocation probes never persist the discarded draft");
        folio_state_destroy(state);
        if (outcome == FOLIO_STATE_READY)
        {
            return nth - 1;
        }
    }
    require(false, "replacement allocation boundary must complete");
    return 0;
}

static void verify_departure_partial_publication(void)
{
    struct note_text *probe = nullptr;
    allocation_probe_fail_at(1);
    enum note_text_outcome active = note_text_create("", 0, &probe);
    allocation_probe_fail_at(0);
    note_text_destroy(probe);
    if (active != NOTE_TEXT_OUT_OF_MEMORY)
    {
        return;
    }
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY,
            "prepare the first-save cache boundary");
    struct note_name *name = accepted_note_name("pending-draft");
    struct note_destination destination = {.category = 0, .name = name};
    adapter.copy_fail_at = 1;
    enum folio_state_outcome published = folio_state_store_new(state, &destination, u"needle", 6);
    allocation_probe_fail_at(0);
    adapter.copy_fail_at = 0;
    require(published == FOLIO_STATE_OUT_OF_MEMORY &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED && adapter.creates == 1,
            "published draft is named even when its saved-copy repair fails");
    enum folio_note_change changed = FOLIO_NOTE_CHANGED;
    allocation_probe_fail_at(1);
    enum folio_state_outcome queried = folio_state_note_changed(state, u"needle", 6, &changed);
    allocation_probe_fail_at(0);
    require(queried == FOLIO_STATE_OUT_OF_MEMORY && changed == FOLIO_NOTE_CHANGED &&
                adapter.creates == 1 && adapter.archives == 0,
            "a pending saved-copy repair continues to block departure rather than discarding");
    require(folio_state_note_changed(state, u"needle", 6, &changed) == FOLIO_STATE_READY &&
                changed == FOLIO_NOTE_SAME && adapter.creates == 1,
            "retry repairs the published draft without creating it again");
    note_name_destroy(name);
    folio_state_destroy(state);
}

void run_first_save_target_tests(void)
{
    verify_untitled();
    verify_first_save();
    verify_first_save_refusals();
}

void run_unsaved_departure_tests(void)
{
    finder = test_regex_port();
    verify_departure_partial_publication();
    verify_untitled_departure();
    verify_untitled_expand_permission();
    size_t selected = departure_allocation_boundary(false);
    size_t created = departure_allocation_boundary(true);
    printf("untitled departure allocation points: select=%zu new=%zu\n", selected, created);
}

void run_untitled_expansion_query_tests(void)
{
    verify_untitled_expand_permission();
}

static void verify_accessible_focus(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    const char *pane = folio_state_pane_rtf(state);
    const char *edit = folio_state_edit_rtf(state);
    size_t reads = adapter.note_reads;
    uint64_t revision = folio_state_index_revision(state);
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(2, 2)) == FOLIO_STATE_READY,
            "focus reaches an unselected visible note while saving is unavailable");
    expect_cursor(state, "n2.2", "the independent cursor reaches C / three");
    require(keeps_document(state, at(0, 1), PANE_MODE_EDIT) &&
                folio_state_pane_rtf(state) == pane && folio_state_edit_rtf(state) == edit,
            "focus preserves the held document and both rendered objects");
    require(adapter.note_reads == reads && adapter.archives == 0 && adapter.note_writes == 0 &&
                adapter.writes == 0 && adapter.ledger_writes == 0 &&
                folio_state_index_revision(state) == revision,
            "focus performs no read or persistence and keeps row identity");
    require(folio_state_store_note(state, u"unsaved", 7) == FOLIO_STATE_NOTE_STORE_FAILED,
            "a subsequent save can fail independently");
    expect_cursor(state, "n2.2", "save failure preserves the focused unselected row");
    require(keeps_document(state, at(0, 1), PANE_MODE_EDIT), "save failure keeps selection A");
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(1, 0)) ==
                    FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(2, 99)) ==
                    FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_focus_cursor(state, FOLIO_CURSOR_CATEGORY, at(99, 0)) ==
                    FOLIO_STATE_NO_SUCH_CATEGORY,
            "collapsed children and missing rows cannot acquire focus");
    expect_cursor(state, "n2.2", "refused focus leaves the previous cursor");
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_CATEGORY, at(0, 0)) == FOLIO_STATE_READY,
            "an expanded category is an explicit focus target");
    expect_cursor(state, "c0", "category focus does not follow its selected child");
    folio_state_destroy(state);
}

static void verify_accessible_untitled_focus(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "prepare an unsaved draft");
    const char *edit = folio_state_edit_rtf(state);
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(2, 0)) == FOLIO_STATE_READY,
            "focus a row without replacing the draft");
    require(folio_state_select_note(state, 2, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_NAME_REQUIRED,
            "selection still requires the existing draft decision");
    expect_cursor(state, "n2.0", "a refused selection preserves the prior focus");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_edit_rtf(state) == edit && adapter.note_reads == 0 &&
                adapter.creates == 0 && adapter.archives == 0,
            "focus and refused selection retain the draft without persistence");
    adapter.note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_select_note(state, 2, 0, FOLIO_UNTITLED_DISCARD) ==
                FOLIO_STATE_NOTE_UNREADABLE,
            "a failed read cannot adopt the target");
    expect_cursor(state, "n2.0", "read failure preserves the cursor");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_edit_rtf(state) == edit,
            "failed replacement keeps the draft objects");
    adapter.note_outcome = PERSISTENCE_LOADED;
    require(folio_state_select_note(state, 2, 0, FOLIO_UNTITLED_DISCARD) == FOLIO_STATE_READY &&
                keeps_document(state, at(2, 0), PANE_MODE_EDIT),
            "successful selection adopts B");
    expect_cursor(state, "n2.0", "successful selection aligns cursor and selection");
    folio_state_destroy(state);
}

static void verify_accessible_expansion(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    size_t reads = adapter.note_reads;
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_CATEGORY, at(1, 0)) == FOLIO_STATE_READY,
            "focus a collapsed category outside the held document");
    uint64_t revision = folio_state_index_revision(state);
    require(folio_state_set_expansion(state, 1, true) == FOLIO_STATE_READY &&
                folio_state_index_revision(state) != revision,
            "expansion changes exposed rows");
    expect_cursor(state, "c1", "UIA-style expansion leaves the cursor on its category");
    require(keeps_document(state, at(0, 1), PANE_MODE_EDIT) && adapter.note_reads == reads &&
                adapter.archives == 0 && adapter.note_writes == 0 && adapter.writes == 1,
            "expansion writes only the category ledger and never selects a child");
    revision = folio_state_index_revision(state);
    require(folio_state_set_expansion(state, 1, true) == FOLIO_STATE_READY &&
                folio_state_index_revision(state) == revision && adapter.writes == 1,
            "idempotent expansion keeps identity and writes nothing");
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(1, 2)) == FOLIO_STATE_READY &&
                folio_state_set_expansion(state, 1, false) == FOLIO_STATE_READY,
            "collapse a category containing only the cursor");
    expect_cursor(state, "c1", "collapse returns the hidden cursor to its visible parent");
    require(keeps_document(state, at(0, 1), PANE_MODE_EDIT), "collapse leaves selection elsewhere");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    revision = folio_state_index_revision(state);
    require(folio_state_set_expansion(state, 1, true) == FOLIO_STATE_STORE_FAILED &&
                folio_state_index_revision(state) == revision,
            "failed expansion preserves row identity");
    folio_state_destroy(state);
}

static void verify_accessible_reveal(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(0, 0)) == FOLIO_STATE_READY,
            "focus the first note while a different note is selected");
    struct drawer_cursor target = {.kind = DRAWER_ROW_NOTE, .ref = at(2, 2)};
    uint64_t revision = folio_state_index_revision(state);
    require(folio_state_reveal_row(state, scroll_metrics, target) == FOLIO_STATE_READY &&
                folio_state_drawer_scroll(state) == 40,
            "reveal the last row without focus");
    expect_cursor(state, "n0.0", "ScrollItem does not move the cursor");
    require(keeps_document(state, at(0, 1), PANE_MODE_EDIT) && adapter.archives == 0 &&
                adapter.note_writes == 0 && folio_state_index_revision(state) == revision,
            "reveal preserves selection, persistence and row identity");
    target = (struct drawer_cursor){.kind = DRAWER_ROW_CATEGORY, .ref = at(0, 0)};
    require(folio_state_reveal_row(state, scroll_metrics, target) == FOLIO_STATE_READY &&
                folio_state_drawer_scroll(state) == 0,
            "category reveal uses the same layout path");
    folio_state_destroy(state);
}

static void verify_accessible_row_identity(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "# Hello\r\n\r\nbody";
    struct folio_state *state = edited_state(&adapter);
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_NOTE, at(0, 2)) == FOLIO_STATE_READY,
            "focus B / three while B / two remains selected");
    uint64_t revision = folio_state_index_revision(state);
    require(folio_state_move_note(state, at(0, 2), at(0, 0)) == FOLIO_STATE_READY &&
                folio_state_index_revision(state) != revision,
            "row order invalidates prior references");
    expect_cursor(state, "n0.0", "cursor follows the same reordered note");
    require(keeps_document(state, at(0, 2), PANE_MODE_EDIT), "selection follows its separate note");
    revision = folio_state_index_revision(state);
    require(folio_state_move_category(state, 0, 2) == FOLIO_STATE_READY &&
                folio_state_index_revision(state) != revision,
            "category order changes row identity");
    expect_cursor(state, "n2.0", "the independent cursor follows the category");
    require(keeps_document(state, at(2, 2), PANE_MODE_EDIT),
            "the document follows the category too");
    require(test_index_filter_complete(state, u"nothing-matches", 15) == FOLIO_STATE_READY,
            "hide every row");
    expect_cursor(state, "-", "a zero-result filter has no hidden cursor");
    require(folio_state_focus_cursor(state, FOLIO_CURSOR_CATEGORY, at(2, 0)) ==
                FOLIO_STATE_NO_SUCH_NOTE,
            "a filtered-out category cannot acquire focus");
    require(keeps_document(state, at(2, 2), PANE_MODE_EDIT), "filtering keeps the held document");
    require(folio_state_set_expansion(state, 2, false) == FOLIO_STATE_FILTERED,
            "expansion cannot bypass the filter contract");
    folio_state_destroy(state);
}

void run_accessibility_state_tests(void)
{
    finder = test_regex_port();
    verify_accessible_focus();
    verify_accessible_untitled_focus();
    verify_accessible_expansion();
    verify_accessible_reveal();
    verify_accessible_row_identity();
    verify_select_adjacent();
    verify_cursor_expanded();
    verify_reveal_cursor();
    verify_move_while_editing();
    verify_first_save_keeps_category_cursor();
    verify_trash_all();
    verify_remove_categories();
}

/* #328 の対象実行。本文写しを読む既存の操作境界を同じ試験で確認する。 */
void run_filter_lookup_state_tests(void)
{
    finder = test_regex_port();
    verify_index_filter();
    verify_index_filter_refusals();
    verify_filtered_refusals();
    verify_filter_keeps_document();
    verify_filter_follows_content();
    verify_filter_follows_new_note();
    verify_filter_cursor_lands_near();
    verify_filter_follows_resumed_rename();
    verify_filter_follows_move();
    verify_trash_filter();
    verify_category_rename_filter();
}

void run_menu_availability_tests(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_ref target = {0};
    size_t reads = adapter.note_reads, scans = adapter.note_scans, writes = adapter.writes;
    require(folio_state_command_availability(state, FOLIO_COMMAND_RENAME,
                                             FOLIO_COMMAND_TARGET_CURRENT,
                                             target) == FOLIO_STATE_NOTHING_SELECTED,
            "no current note disables rename");
    require(folio_state_command_availability(state, FOLIO_COMMAND_RENAME, FOLIO_COMMAND_TARGET_NOTE,
                                             target) == FOLIO_STATE_READY,
            "explicit note can be renamed without being selected");
    require(folio_state_command_availability(state, FOLIO_COMMAND_DELETE_CATEGORY,
                                             FOLIO_COMMAND_TARGET_CATEGORY,
                                             target) == FOLIO_STATE_CATEGORY_NOT_EMPTY,
            "known notes disable category deletion");
    for (size_t at = 0; at < folio_command_count(); ++at)
    {
        enum folio_state_outcome outcome = folio_state_command_availability(
            state, folio_command_at(at), FOLIO_COMMAND_TARGET_CURRENT, target);
        require(outcome <= FOLIO_STATE_CATEGORY_NOT_SELECTED,
                "every command has a known query result");
    }
    require(adapter.note_reads == reads && adapter.note_scans == scans && adapter.writes == writes,
            "availability never reads notes, scans files or writes state");
    require(folio_state_recolor_availability(state, 999) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "stale color target is unavailable");
    folio_state_destroy(state);
}

/* #351: 完全同名は本文を保存しないが、先行同期の門は飛ばさない。 */
static void verify_name_unchanged_note(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit an existing note");
    const char *body = folio_state_pane_text(state);
    struct note_name *same = accepted_note_name("one.md");
    require(folio_state_rename_note(state, same, u"unsaved", 7) == FOLIO_STATE_NAME_UNCHANGED &&
                adapter.note_writes == 0 && adapter.archives == 0 && adapter.renames == 0 &&
                adapter.creates == 0 && folio_state_pane_text(state) == body &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "same note leaves unsaved editing and persistence untouched");
    struct note_name *next = accepted_note_name("next");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, next, u"unsaved", 7) == FOLIO_STATE_RENAME_PENDING,
            "publish an unfinished rename");
    size_t writes = adapter.note_writes, archives = adapter.archives, renames = adapter.renames;
    require(folio_state_rename_note(state, same, u"later", 5) == FOLIO_STATE_RENAME_PENDING &&
                adapter.renames == renames + 1 && adapter.last_attempt == RENAME_RESUME &&
                adapter.note_writes == writes && adapter.archives == archives,
            "prior synchronization precedes same-name rejection and never saves the later body");
    note_name_destroy(next);
    note_name_destroy(same);
    folio_state_destroy(state);
}

void run_name_unchanged_state_tests(void)
{
    finder = test_regex_port();
    verify_name_unchanged_note();
    verify_category_rename_refusals();
}

/* 入力の再設定をせず、public継続だけで完了まで進める試験用driver。 */
static enum folio_state_outcome finish_test_index_filter(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    size_t limit = folio_state_note_count(state) + 65536;
    while (outcome == FOLIO_STATE_READY && folio_state_index_filter_pending(state) && limit > 0)
    {
        outcome = folio_state_continue_index_filter(state);
        limit -= 1;
    }
    require(!folio_state_index_filter_pending(state) || outcome != FOLIO_STATE_READY,
            "bounded preparation completes without mutations");
    return outcome;
}

/* 通常のUI待機継続と同じpublic APIで完成させる。旧契約の試験へ同期実装を複製しない。 */
enum folio_state_outcome test_index_filter_complete(struct folio_state *_Nonnull state,
                                                    const char16_t *_Nonnull units, size_t count)
{
    enum folio_state_outcome outcome = folio_state_set_index_filter(state, units, count);
    return outcome == FOLIO_STATE_READY ? finish_test_index_filter(state) : outcome;
}

static struct persistence_adapter preparation_adapter(void)
{
    static const char *const categories[] = {"B", nullptr};
    static const char *const notes[] = {"one", "two", nullptr};
    struct persistence_adapter adapter = healthy_adapter();
    adapter.categories_text = "{\"version\":1,\"categories\":["
                              "{\"name\":\"B\",\"color\":\"#111111\",\"expanded\":true}]}";
    adapter.scanned_categories = categories;
    adapter.scanned_category_count = 1;
    adapter.scanned_notes = notes;
    adapter.scanned_note_count = 2;
    adapter.note_body = "plain";
    return adapter;
}

static void verify_preparation_steps(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) && folio_state_filtering(state) &&
                adapter.note_reads == 0 && folio_state_index_filter_count(state) == 2,
            "accept intent without reading or publishing partial matches");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1 && same_text(adapter.last_note, "one") &&
                folio_state_index_filter_pending(state) &&
                folio_state_index_filter_count(state) == 2,
            "first continuation reads exactly the first note and keeps ordinary rows");
    const char16_t broken[] = {0xd800};
    require(folio_state_set_index_filter(state, broken, 1) == FOLIO_STATE_SEARCH_MALFORMED &&
                same_text(folio_state_index_filter_term(state), "one"),
            "malformed replacement preserves the pending request");
    require(folio_state_set_index_filter(state, u"two", 3) == FOLIO_STATE_READY &&
                adapter.note_reads == 1 && same_text(folio_state_index_filter_term(state), "two"),
            "latest valid intent replaces only the term");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 2 && same_text(adapter.last_note, "two") &&
                folio_state_index_filter_pending(state),
            "replacement does not reread the first note");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 2 && !folio_state_index_filter_pending(state) &&
                folio_state_index_filter_count(state) == 1,
            "completion publishes only the latest request without another read");
    char order[64];
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two"), "latest requested name supplies the result");
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) && adapter.note_reads == 2 &&
                finish_test_index_filter(state) == FOLIO_STATE_READY && adapter.note_reads == 2,
            "completed corpus queues warm matching without rereading");
    folio_state_destroy(state);
}

void run_index_bounded_warm_tests(void)
{
    finder = test_regex_port();
    static const char *const notes[] = {"one", nullptr};
    char body[2 * 65536 + 18];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    adapter.notes_text = "{\"version\":1,\"notes\":[\"one\"]}";
    adapter.scanned_notes = notes;
    adapter.scanned_note_count = 1;
    adapter.note_body = body;
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"a", 1) == FOLIO_STATE_READY &&
                !folio_state_index_filter_pending(state) && adapter.note_reads == 1 &&
                folio_state_index_filter_count(state) == 1,
            "warm fixture loads one body and completes its initial matching");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY,
            "clear before the warm request without invalidating the completed corpus");
    require(folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) && adapter.note_reads == 1,
            "warm set returns pending before matching the giant single body");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) && adapter.note_reads == 1,
            "first warm matching continuation yields inside the single body without rereading");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY &&
                !folio_state_index_filter_pending(state) &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                !folio_state_filtering(state) && adapter.note_reads == 1,
            "clear cancels the partially matched warm request");
    folio_state_destroy(state);
}

static void verify_preparation_cancel(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
            "prepare one copy before cancellation");
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_FILTERED &&
                folio_state_set_expansion(state, 0, false) == FOLIO_STATE_FILTERED &&
                folio_state_move_note(state, at(0, 0), at(0, 1)) == FOLIO_STATE_FILTERED &&
                folio_state_command_availability(state, FOLIO_COMMAND_NEW_CATEGORY,
                                                 FOLIO_COMMAND_TARGET_CURRENT,
                                                 at(0, 0)) == FOLIO_STATE_FILTERED,
            "preparation uses the same mutation refusals as completed filtering");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY &&
                !folio_state_index_filter_pending(state) && !folio_state_filtering(state) &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "clear cancels future reads");
    require(folio_state_move_note(state, at(0, 0), at(0, 1)) == FOLIO_STATE_READY &&
                folio_state_set_index_filter(state, u"two", 3) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                same_text(adapter.last_note, "two") && adapter.note_reads == 2,
            "cancel and reorder restart at the new first note instead of the old position");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY && adapter.note_reads == 3 &&
                folio_state_index_filter_count(state) == 1,
            "restart reads every current note once");
    folio_state_destroy(state);
}

static void verify_preparation_stale_copy(enum persistence_outcome outcome)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.rare_note = "one";
    adapter.rare_body = "needle";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY,
            "retain a partial copy then cancel");
    adapter.note_outcome = outcome;
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 && adapter.note_reads == 3,
            "unavailable or malformed reread removes stale partial copies");
    folio_state_destroy(state);
}

static void verify_preparation_save(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "select editable first note");
    const char *edit = folio_state_edit_rtf(state);
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_edit_rtf(state) == edit &&
                keeps_document(state, at(0, 0), PANE_MODE_EDIT),
            "reading saved copies does not replace the held edit or its mode");
    require(folio_state_store_note(state, u"needle", 6) == FOLIO_STATE_READY,
            "saving remains available during preparation");
    adapter.rare_note = "one";
    adapter.rare_body = adapter.written_body;
    size_t reads = adapter.note_reads;
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                same_text(adapter.last_note, "one") && adapter.note_reads == reads + 1,
            "a changed saved body restarts the incomplete scan");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 &&
                same_text(folio_state_pane_text(state), "needle"),
            "completed result sees the newly saved text");
    folio_state_destroy(state);
}

static void verify_preparation_mutations(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_set_index_filter(state, u"renamed", 7) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
            "prepare before rename");
    struct note_name *name = accepted_note_name("renamed");
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                same_text(adapter.last_note, "renamed"),
            "rename restarts at the new identity");
    note_name_destroy(name);
    require(folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                same_text(adapter.last_note, "two"),
            "deletion restarts at the shifted remaining note");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "new draft remains allowed");
    name = accepted_note_name("fresh");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"needle", 6) == FOLIO_STATE_READY,
            "publish while preparing");
    note_name_destroy(name);
    adapter.rare_note = "fresh";
    adapter.rare_body = adapter.written_body;
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "newly published body is included");
    char order[64];
    row_order(state, order, sizeof order);
    require(same_text(order, "B/fresh"), "deleted and renamed old copies never reappear");
    folio_state_destroy(state);
}

static void verify_preparation_failure(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
            "prepare partial corpus");
    adapter.note_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_OUT_OF_MEMORY &&
                !folio_state_index_filter_pending(state) && !folio_state_filtering(state),
            "read OOM stops preparation without publishing a partial result");
    adapter.note_outcome = PERSISTENCE_LOADED;
    require(folio_state_set_index_filter(state, u"two", 3) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                same_text(adapter.last_note, "one"),
            "retry after failure restarts at the first note");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "retry completes with correct results");
    folio_state_destroy(state);
}

static void verify_preparation_pending_rename(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_set_index_filter(state, u"renamed", 7) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
            "prepare before interrupted rename");
    struct note_name *name = accepted_note_name("renamed");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "rename keeps its pending intent");
    size_t reads = adapter.note_reads;
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_RENAME_PENDING &&
                !folio_state_index_filter_pending(state) && adapter.note_reads == reads,
            "pending rename stops preparation before any old-path read");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(test_index_filter_complete(state, u"renamed", 7) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "retry synchronizes rename and searches the new identity");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_preparation_category_invalidation(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.rare_note = "one";
    adapter.rare_body = "needle";
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 &&
                folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY,
            "complete and clear before category rename");
    require(rename_category_to(state, 0, "D") == FOLIO_STATE_READY,
            "category rename invalidates the formerly complete corpus");
    size_t reads = adapter.note_reads;
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == reads + 2 && same_text(adapter.read_category, "D") &&
                folio_state_index_filter_count(state) == 1,
            "invalidated completed corpus restarts at zero and reads the renamed category");
    folio_state_destroy(state);
}

static void verify_preparation_empty(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.categories_text = "{\"version\":1,\"categories\":[]}";
    adapter.scanned_category_count = 0;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                !folio_state_index_filter_pending(state) && adapter.note_reads == 0 &&
                folio_state_index_filter_count(state) == 0,
            "empty corpus completes without a read or repeated idle work");
    folio_state_destroy(state);
}

static void verify_matching_policy_boundary(void)
{
    static const char *const notes[] = {"N", nullptr};
    char body[65534];
    for (size_t extra = 0; extra < 2; ++extra)
    {
        /* Entry 1 + name byte/NUL 2 + name comparison 1 + body bytes. */
        size_t length = 65532 + extra;
        memset(body, 'a', length);
        body[length] = '\0';
        struct persistence_adapter adapter = preparation_adapter();
        adapter.notes_text = "{\"version\":1,\"notes\":[\"N\"]}";
        adapter.scanned_notes = notes;
        adapter.scanned_note_count = 1;
        adapter.note_body = body;
        struct folio_state *state = ready_state(&adapter);
        require(test_index_filter_complete(state, u"a", 1) == FOLIO_STATE_READY &&
                    folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY &&
                    folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY,
                "prime one warm body for the policy boundary");
        require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                    folio_state_index_filter_pending(state) == (extra == 1) &&
                    adapter.note_reads == 1,
                "first matching call creates and uses exactly the 65536-work policy budget");
        require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                    folio_state_index_filter_count(state) == 0 && adapter.note_reads == 1,
                "the immediately over-budget body completes on a later call without rereading");
        folio_state_destroy(state);
    }
}

static void verify_matching_empty_category(void)
{
    static const char *const categories[] = {"B", "A", nullptr};
    static const char *const notes[] = {"N", nullptr};
    static const char *const no_notes[] = {nullptr};
    char body[65533];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    adapter.categories_text = "{\"version\":1,\"categories\":["
                              "{\"name\":\"B\",\"color\":\"#111111\",\"expanded\":true},"
                              "{\"name\":\"A\",\"color\":\"#222222\",\"expanded\":true}]}";
    adapter.scanned_categories = categories;
    adapter.scanned_category_count = 2;
    adapter.notes_text = "{\"version\":1,\"notes\":[\"N\"]}";
    adapter.scanned_notes = notes;
    adapter.scanned_note_count = 1;
    adapter.note_body = body;
    test_adapter_second_notes(&adapter, "B", "{\"version\":1,\"notes\":[]}", no_notes);
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"a", 1) == FOLIO_STATE_READY &&
                folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY,
            "prime a corpus with an empty category before its one note");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) && adapter.note_reads == 1,
            "the empty category consumes one work beyond the otherwise exact 65536 entry");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 && adapter.note_reads == 1,
            "empty-category progress resumes without starving the valid entry or rereading");
    folio_state_destroy(state);
}

static struct folio_state *_Nonnull matching_mutation_state(
    struct persistence_adapter *_Nonnull adapter, const char *_Nonnull body)
{
    *adapter = preparation_adapter();
    adapter->note_body = body;
    adapter->rare_note = "one";
    adapter->rare_body = "small";
    struct folio_state *state = ready_state(adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"a", 1) == FOLIO_STATE_READY,
            "warm mutation fixture has one short current note and one giant cached body");
    return state;
}

static void verify_matching_save_restart(void)
{
    char body[2 * 65536 + 18];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = matching_mutation_state(&adapter, body);
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "match partially before saving");
    size_t reads = adapter.note_reads;
    require(folio_state_store_note(state, u"zmall", 5) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "pending save keeps the request");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads,
            "same-length saved first entry is rematched after invalidation without corpus reread");
    folio_state_destroy(state);
}

static void verify_matching_identity_restart(void)
{
    char body[2 * 65536 + 18];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = matching_mutation_state(&adapter, body);
    size_t reads = adapter.note_reads;
    require(folio_state_set_index_filter(state, u"q", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "match partially before rename");
    struct note_name *name = accepted_note_name("q");
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "pending rename keeps the request");
    note_name_destroy(name);
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads,
            "a renamed entry before the old cursor is rematched without rereading");
    require(folio_state_set_index_filter(state, u"q", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) &&
                folio_state_trash_note(state, at(0, 0), u"", 0) == FOLIO_STATE_READY,
            "delete remains available with a partially matched request");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 && adapter.note_reads == reads,
            "delete restarts against the shifted ledger and drops the old admitted match");
    folio_state_destroy(state);
}

static void verify_matching_new_and_latest(void)
{
    char body[2 * 65536 + 18];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = matching_mutation_state(&adapter, body);
    size_t reads = adapter.note_reads;
    require(folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "partially match before replacement");
    const char16_t broken[] = {0xd800};
    require(folio_state_set_index_filter(state, broken, 1) == FOLIO_STATE_SEARCH_MALFORMED &&
                same_text(folio_state_index_filter_term(state), "z"),
            "malformed warm replacement preserves the current request");
    require(folio_state_set_index_filter(state, u"a", 1) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 2,
            "latest warm term replaces the old partial matcher");
    require(folio_state_set_index_filter(state, u"q", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) &&
                folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "new draft remains available while matching is partial");
    struct note_name *name = accepted_note_name("q");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"", 0) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "publication keeps the latest request");
    note_name_destroy(name);
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads,
            "published name/body is included by a fresh matching count without rereading");
    folio_state_destroy(state);
}

void run_index_preparation_tests(void)
{
    finder = test_regex_port();
    run_index_bounded_warm_tests();
    verify_matching_policy_boundary();
    verify_matching_empty_category();
    verify_matching_save_restart();
    verify_matching_identity_restart();
    verify_matching_new_and_latest();
    verify_preparation_steps();
    verify_preparation_cancel();
    verify_preparation_stale_copy(PERSISTENCE_ABSENT);
    verify_preparation_stale_copy(PERSISTENCE_MALFORMED);
    verify_preparation_save();
    verify_preparation_mutations();
    verify_preparation_failure();
    verify_preparation_pending_rename();
    verify_preparation_category_invalidation();
    verify_preparation_empty();
}

static bool preparation_allocation(size_t nth)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    allocation_probe_fail_at(nth);
    enum folio_state_outcome outcome = test_index_filter_complete(state, u"one", 3);
    allocation_probe_fail_at(0);
    require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_OUT_OF_MEMORY,
            "all preparation and adoption allocations have a typed outcome");
    require(!folio_state_index_filter_pending(state),
            "allocation failure cannot leave a retry loop");
    require(test_index_filter_complete(state, u"two", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "every failure point can retry safely");
    folio_state_destroy(state);
    return outcome == FOLIO_STATE_READY;
}

static void verify_warm_request_oom(void)
{
    char body[2 * 65536 + 18];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = matching_mutation_state(&adapter, body);
    size_t reads = adapter.note_reads;
    require(folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "active warm matcher before term OOM");
    allocation_probe_fail_at(1);
    enum folio_state_outcome replaced = folio_state_set_index_filter(state, u"a", 1);
    allocation_probe_fail_at(0);
    require(replaced == FOLIO_STATE_OUT_OF_MEMORY && folio_state_index_filter_pending(state) &&
                same_text(folio_state_index_filter_term(state), "z"),
            "warm term OOM preserves the active request and builder");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                !folio_state_index_filter_pending(state) &&
                folio_state_index_filter_count(state) == 0 && adapter.note_reads == reads,
            "term OOM preserves the already consumed work rather than restarting the giant entry");
    folio_state_destroy(state);
}

static void verify_warm_build_oom(size_t nth)
{
    char body[2 * 65536 + 18];
    memset(body, 'a', sizeof body - 1);
    body[sizeof body - 1] = '\0';
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = matching_mutation_state(&adapter, body);
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 &&
                folio_state_set_index_filter(state, u"a", 1) == FOLIO_STATE_READY,
            "a valid warm term is accepted before its delayed build/finish OOM");
    allocation_probe_fail_at(nth);
    enum folio_state_outcome outcome = folio_state_continue_index_filter(state);
    allocation_probe_fail_at(0);
    require(outcome == FOLIO_STATE_OUT_OF_MEMORY && !folio_state_index_filter_pending(state) &&
                !folio_state_filtering(state) && folio_state_index_filter_count(state) == 2 &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
            "delayed OOM stops once on the full index without restoring the old narrow filter");
    require(test_index_filter_complete(state, u"a", 1) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 2,
            "stopped warm construction can retry through the same request/engine path");
    folio_state_destroy(state);
}

static void verify_warm_prefix_oom(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = matching_mutation_state(&adapter, "aaaaaaaa");
    size_t reads = adapter.note_reads;
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 &&
                folio_state_set_index_filter(state, u"aa", 2) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "accept a warm general request after an existing narrow completed filter");
    allocation_probe_fail_at(4);
    enum folio_state_outcome outcome = folio_state_continue_index_filter(state);
    size_t calls = allocation_probe_calls(), bytes = allocation_probe_malloc_size();
    allocation_probe_fail_at(0);
    require(outcome == FOLIO_STATE_OUT_OF_MEMORY && calls == 4 && bytes == 2 * sizeof(size_t) &&
                !folio_state_index_filter_pending(state) && !folio_state_filtering(state) &&
                folio_state_index_filter_count(state) == 2 && adapter.note_reads == reads,
            "the new prefix OOM stops the accepted warm request on the full cached index");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                !folio_state_filtering(state) && folio_state_index_filter_count(state) == 2,
            "the stopped request returns no second failure and does not restore the old filter");
    require(test_index_filter_complete(state, u"aa", 2) == FOLIO_STATE_READY &&
                folio_state_filtering(state) && folio_state_index_filter_count(state) == 1 &&
                same_text(folio_state_index_filter_term(state), "aa") &&
                adapter.note_reads == reads,
            "the same general request recovers through the one engine without corpus reread");
    printf("warm prefix OOM: calls=%zu bytes=%zu, one failure then READY; retry count=1\n", calls,
           bytes);
    folio_state_destroy(state);
}

void run_index_preparation_allocation_tests(void)
{
    allocation_probe_fail_at(1);
    struct note_text *check = nullptr;
    enum note_text_outcome active = note_text_create("", 0, &check);
    allocation_probe_fail_at(0);
    note_text_destroy(check);
    if (active != NOTE_TEXT_OUT_OF_MEMORY)
        return;
    finder = test_regex_port();
    verify_warm_request_oom();
    verify_warm_prefix_oom();
    for (size_t nth = 1; nth <= 4; ++nth)
        verify_warm_build_oom(nth);
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
            "partial prior request");
    allocation_probe_fail_at(1);
    enum folio_state_outcome replaced = folio_state_set_index_filter(state, u"two", 3);
    allocation_probe_fail_at(0);
    require(replaced == FOLIO_STATE_OUT_OF_MEMORY && folio_state_index_filter_pending(state) &&
                same_text(folio_state_index_filter_term(state), "one"),
            "term OOM preserves the prior intent");
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                same_text(adapter.last_note, "two") && adapter.note_reads == 2,
            "term OOM also preserves progress");
    folio_state_destroy(state);
    bool completed = false;
    size_t nth = 1;
    for (; nth < 128 && !completed; ++nth)
        completed = preparation_allocation(nth);
    require(completed, "preparation allocation sweep completes");
    printf("index preparation allocations: %zu failed points, then complete\n", nth - 2);
}

void run_markdown_source_state_tests(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "A | B\r\n--- | ---\n**cell** | 日本語😀\r\n\n----\n";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select native table source");
    require(strstr(folio_state_pane_rtf(state), "\\trowd") != nullptr,
            "view prepares the canonical native table RTF");
    const char16_t native[] = u"\xFFF9\rA\x0007";
    require(folio_state_store_note(state, native, sizeof native / sizeof native[0] - 1) ==
                    FOLIO_STATE_READY &&
                adapter.note_writes == 0 &&
                same_text(folio_state_pane_text(state), adapter.note_body),
            "VIEW save ignores native structural text and retains source");
    struct note_name *name = accepted_note_name("table-copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, native,
                                  sizeof native / sizeof native[0] - 1) == FOLIO_STATE_READY &&
                same_text(adapter.written_body, adapter.note_body),
            "VIEW SaveAs copies exact mixed-line-ending source, not native table markers");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_pane_edit_rtf(state) == FOLIO_STATE_READY &&
                strstr(folio_state_edit_rtf(state), "A | B\\par --- | ---\\par **cell**") !=
                    nullptr &&
                strstr(folio_state_edit_rtf(state), "\\trowd") == nullptr,
            "EDIT is constructed from original Markdown and has no native table structure");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_favorite_state(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_favorite_count(state) == 0, "new ledger has no favorites");
    require(folio_state_toggle_favorite(state, at(0, 2)) == FOLIO_STATE_READY,
            "star can be set without opening any note");
    require(adapter.note_reads == 0 && adapter.note_writes == 0 && adapter.archives == 0 &&
                adapter.written_favorite_count == 1,
            "star saves only index metadata");
    char order[160] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/one/two/A/C/one/two/three"),
            "star is first within its category");
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref next = {0};
    require(folio_state_step_kind(state, FOLIO_STEP_FIRST, &kind, &next) &&
                kind == FOLIO_CURSOR_NOTE && next.category == 0 && next.note == 2,
            "keyboard follows the same displayed first note");
    require(folio_state_select_note(state, 0, 2, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "open starred note in edit");
    require(folio_state_step_kind(state, FOLIO_STEP_NEXT, &kind, &next) && next.note == 0,
            "keyboard leaves first star for first manual-order ordinary note");
    const char *body = folio_state_pane_text(state);
    size_t reads = adapter.note_reads;
    folio_state_set_note_scope(state, NOTE_SCOPE_FAVORITES);
    require(folio_state_index_filter_count(state) == 1 && adapter.note_reads == reads,
            "star filtering never prepares corpus");
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 &&
                folio_state_favorite_count(state) == 1,
            "star and term filters intersect");
    require(test_index_filter_complete(state, u"three", 5) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "term finds the starred note");
    require(folio_state_toggle_favorite(state, at(0, 2)) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 &&
                folio_state_favorite_count(state) == 0 && folio_state_pane_text(state) == body &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT && adapter.note_writes == 0 &&
                adapter.archives == 0,
            "removing the filtered row retains open body, mode and saved history");
    folio_state_destroy(state);
}

static void verify_favorite_failure_and_order(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_STORE_FAILED &&
                !folio_state_note_favorite(state, at(0, 1)),
            "failed star write never changes display");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_READY,
            "star retry succeeds");
    require(folio_state_move_note(state, at(0, 1), at(0, 0)) == FOLIO_STATE_ORDERED,
            "manual reorder is rejected while priority changes the order");
    adapter.settings_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_note_order(state, NOTE_ORDER_MANUAL) ==
                    FOLIO_STATE_SETTINGS_STORE_FAILED &&
                folio_state_note_order(state) == NOTE_ORDER_FAVORITES_FIRST,
            "failed preference save preserves order");
    adapter.settings_write_outcome = PERSISTENCE_STORED;
    require(folio_state_set_note_order(state, NOTE_ORDER_MANUAL) == FOLIO_STATE_READY,
            "manual preference saved");
    char order[160] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/two/three/A/C/one/two/three"),
            "manual order is restored exactly");
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_READY &&
                folio_state_note_favorite(state, at(1, 1)) && adapter.move_favorite_count == 1,
            "destination star is saved before md moves and adopted at destination");
    folio_state_destroy(state);
}

static void verify_move_rollback_pending(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    adapter.move_outcome = PERSISTENCE_UNWRITABLE;
    adapter.ledger_fail_at = 2;
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_MOVE_ROLLBACK_PENDING &&
                adapter.moves == 1,
            "failed md plus failed rollback is distinct from successful move");
    adapter.ledger_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_LEDGER_UNSYNCED &&
                adapter.moves == 1,
            "unrestored index blocks later metadata mutation");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY,
            "same synchronization restores old destination then accepts star");
    folio_state_destroy(state);
}

void run_favorite_state_tests(void)
{
    finder = test_regex_port();
    verify_favorite_state();
    verify_favorite_failure_and_order();
    verify_move_rollback_pending();
    verify_transfer_note();
    verify_transfer_to_end();
    verify_transfer_selection();
    verify_transfer_renumbering();
    verify_transfer_refusals();
    verify_transfer_stale_ledger();
}

static bool favorite_state_allocation(size_t nth)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = transfer_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "prepare editor before metadata allocation failure");
    const char *body = folio_state_pane_text(state);
    allocation_probe_fail_at(nth);
    enum folio_state_outcome outcome = folio_state_toggle_favorite(state, at(0, 1));
    allocation_probe_fail_at(0);
    bool accepted = outcome == FOLIO_STATE_READY;
    require(accepted || outcome == FOLIO_STATE_OUT_OF_MEMORY,
            "metadata allocation failure keeps its typed outcome");
    require(folio_state_note_favorite(state, at(0, 1)) == accepted &&
                adapter.ledger_writes == (accepted ? 1u : 0u) &&
                folio_state_pane_text(state) == body && adapter.note_writes == 0 &&
                adapter.archives == 0,
            "failed construction publishes no star and performs no write");
    if (!accepted)
        require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_READY &&
                    folio_state_note_favorite(state, at(0, 1)),
                "same state retries metadata after allocation failure");
    folio_state_destroy(state);
    return accepted;
}

void run_favorite_state_allocation_tests(void)
{
    finder = test_regex_port();
    for (size_t nth = 1; nth < 128; ++nth)
    {
        if (favorite_state_allocation(nth))
        {
            require(nth > 1, "favorite state sweep actually injected failures");
            printf("favorite state: %zu allocation failures and successful retries\n", nth - 1);
            return;
        }
    }
    test_failure("favorite state allocation sweep did not finish");
}

static struct note_tags *_Nonnull selected_tags(size_t count)
{
    struct note_tag *red = nullptr;
    struct note_tag *blue = nullptr;
    require(note_tag_create("red", 3, &red) == NOTE_TAG_READY &&
                note_tag_create("blue", 4, &blue) == NOTE_TAG_READY,
            "selected tag fixture");
    const struct note_tag *names[] = {red, blue};
    struct note_tags *tags = nullptr;
    require(note_tags_create(names, count, &tags) == NOTE_TAGS_READY, "selected immutable set");
    note_tag_destroy(red);
    note_tag_destroy(blue);
    return tags;
}

static struct persistence_adapter tagged_adapter(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.note_body = "---\ntags: [red, blue]\n---\nneedle";
    adapter.rare_note = "two";
    adapter.rare_body = "---\ntags: [blue]\n---\nother";
    return adapter;
}

static void verify_empty_tag_mode(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *empty = nullptr;
    require(note_tags_empty(&empty) == NOTE_TAGS_READY, "empty condition set");
    require(folio_state_set_tag_filter(state, empty, TAG_MATCH_ANY) == FOLIO_STATE_READY &&
                folio_state_tag_match_mode(state) == TAG_MATCH_ANY && !folio_state_filtering(state),
            "OR can be selected before selecting any tag");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY &&
                folio_state_tag_match_mode(state) == TAG_MATCH_ANY,
            "clearing an empty text query retains the explicit tag mode");
    require(folio_state_set_tag_filter(state, empty, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                folio_state_tag_match_mode(state) == TAG_MATCH_ALL,
            "explicit clear-all mode restores AND");
    note_tags_destroy(empty);
    folio_state_destroy(state);
}

static void verify_tag_filter_queries(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "keep an open editor");
    const char *body = folio_state_pane_text(state);
    struct note_tags *selected = selected_tags(2);
    size_t reads = adapter.note_reads;
    require(folio_state_set_tag_filter(state, selected, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state) && adapter.note_reads == reads &&
                folio_state_tag_filter(state) != selected,
            "tag intent copies query without reads");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "tag-only ALL selects one saved note");
    reads = adapter.note_reads;
    require(folio_state_set_tag_filter(state, selected, TAG_MATCH_ANY) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 2 && adapter.note_reads == reads,
            "warm ANY uses the same corpus without rereads");
    require(test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1,
            "text intersects tag conditions");
    require(test_index_filter_complete(state, u"", 0) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 2 &&
                folio_state_tag_match_mode(state) == TAG_MATCH_ANY,
            "clearing text preserves tags and selected mode");
    require(folio_state_toggle_favorite(state, at(0, 1)) == FOLIO_STATE_READY,
            "favorite another matching note");
    folio_state_set_note_scope(state, NOTE_SCOPE_FAVORITES);
    require(folio_state_index_filter_count(state) == 1 &&
                test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0,
            "favorites AND tags AND text compose");
    require(folio_state_pane_text(state) == body &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT && adapter.note_writes == 0 &&
                adapter.archives == 0,
            "filtering retains the open editor and never saves its body");
    note_tags_destroy(selected);
    folio_state_destroy(state);
}

static void verify_tag_filter_clearing(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    struct note_tags *empty = selected_tags(0);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "partially loaded tag request");
    require(folio_state_set_tag_filter(state, empty, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                !folio_state_filtering(state) && !folio_state_index_filter_pending(state),
            "clearing all conditions stops an unfinished request");
    require(test_index_filter_complete(state, u"other", 5) == FOLIO_STATE_READY &&
                folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0,
            "tag replacement retains the text term");
    require(folio_state_set_tag_filter(state, empty, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 1 &&
                same_text(folio_state_index_filter_term(state), "other"),
            "clearing tags preserves text matching");
    note_tags_destroy(red);
    note_tags_destroy(empty);
    folio_state_destroy(state);
}

static void verify_tag_filter_save(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY,
            "saved query established before changing tags");
    const char16_t replacement[] = u"---\rtags: [blue]\r---\rneedle";
    size_t length = sizeof replacement / sizeof replacement[0] - 1;
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note(state, replacement, length) == FOLIO_STATE_NOTE_STORE_FAILED,
            "failed note publication reports its existing typed result");
    require(folio_state_index_filter_count(state) == 1, "failed save retains saved tag membership");
    adapter.note_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note(state, replacement, length) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 &&
                same_text(folio_state_pane_text(state), "---\ntags: [blue]\n---\nneedle"),
            "successful save repairs tag-only filter through the sole matcher");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "disappearing filtered row retains its open editor");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static bool tag_filter_allocation(size_t nth, const struct note_tags *_Nonnull red)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"two", 3) == FOLIO_STATE_READY,
            "prior text query before tag allocation failure");
    allocation_probe_fail_at(nth);
    enum folio_state_outcome accepted = folio_state_set_tag_filter(state, red, TAG_MATCH_ALL);
    enum folio_state_outcome outcome = accepted;
    if (accepted == FOLIO_STATE_READY)
        outcome = finish_test_index_filter(state);
    allocation_probe_fail_at(0);
    require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_OUT_OF_MEMORY,
            "tag query allocations have typed failures");
    require(!folio_state_index_filter_pending(state), "failure stops without an idle retry loop");
    if (accepted == FOLIO_STATE_OUT_OF_MEMORY)
        require(same_text(folio_state_index_filter_term(state), "two") &&
                    folio_state_index_filter_count(state) == 1,
                "pre-acceptance failure retains the complete old query");
    if (accepted == FOLIO_STATE_READY && outcome == FOLIO_STATE_OUT_OF_MEMORY)
        require(folio_state_tag_filter(state) == nullptr && !folio_state_filtering(state) &&
                    same_text(folio_state_index_filter_term(state), ""),
                "asynchronous failure clears the whole latest query, including tag conditions");
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"two", 3) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0,
            "same state retries after every query failure");
    folio_state_destroy(state);
    return outcome == FOLIO_STATE_READY;
}

static enum folio_state_outcome finish_tag_catalog(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome result = FOLIO_STATE_READY;
    size_t remaining = folio_state_note_count(state) + 65536;
    while (result == FOLIO_STATE_READY && folio_state_index_work_pending(state) && remaining > 0)
    {
        result = folio_state_continue_index_filter(state);
        --remaining;
    }
    require(!folio_state_index_work_pending(state) || result != FOLIO_STATE_READY,
            "catalog and matching finish through the single idle driver");
    return result;
}

static void verify_tag_catalog_lifecycle(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    folio_state_request_tag_catalog(state);
    require(folio_state_tag_catalog_status(state) == TAG_CATALOG_PREPARING &&
                !folio_state_filtering(state) && adapter.note_reads == 0,
            "request is pending without reads or false filtering");
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "catalog and filter share one read per idle call");
    require(finish_tag_catalog(state) == FOLIO_STATE_READY && adapter.note_reads == 2 &&
                folio_state_tag_catalog_status(state) == TAG_CATALOG_AVAILABLE,
            "catalog is published only after all saved notes are known");
    const struct tag_catalog *catalog = folio_state_tag_catalog(state);
    require(tag_catalog_count(catalog) == 2 && tag_catalog_unavailable(catalog) == 0 &&
                same_text(note_tag_bytes(tag_catalog_tag(catalog, 0)), "blue") &&
                tag_catalog_usage(catalog, 0) == 2 && tag_catalog_usage(catalog, 1) == 1,
            "usage counts the saved corpus independently of active red filter");
    folio_state_close_tag_catalog(state);
    require(folio_state_tag_catalog_status(state) == TAG_CATALOG_UNREQUESTED &&
                folio_state_filtering(state) && folio_state_index_filter_count(state) == 1,
            "closing catalog retains the independent condition set and matches");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "edit after closing tag surface");
    const char16_t body[] = u"---\rtags: [blue]\r---\rneedle";
    require(folio_state_store_note(state, body, sizeof body / sizeof body[0] - 1) ==
                FOLIO_STATE_READY,
            "publish changed tags through common save");
    folio_state_request_tag_catalog(state);
    require(finish_tag_catalog(state) == FOLIO_STATE_READY &&
                tag_catalog_count(folio_state_tag_catalog(state)) == 1 &&
                tag_catalog_usage(folio_state_tag_catalog(state), 0) == 2,
            "next catalog reflects latest saved tags without a second file reader");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void verify_tag_catalog_unavailable(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    adapter.rare_body = "---\ntags: *alias\n---\nother";
    struct folio_state *state = ready_state(&adapter);
    folio_state_request_tag_catalog(state);
    require(folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "partial catalog before cancellation");
    folio_state_close_tag_catalog(state);
    require(!folio_state_index_work_pending(state) &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "closing alone stops unused catalog preparation");
    folio_state_request_tag_catalog(state);
    require(finish_tag_catalog(state) == FOLIO_STATE_READY && adapter.note_reads == 2 &&
                tag_catalog_unavailable(folio_state_tag_catalog(state)) == 1,
            "resume reads remaining note; unsupported metadata is unavailable, not untagged");
    folio_state_destroy(state);
}

static bool tag_catalog_allocation(size_t nth)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(test_index_filter_complete(state, u"one", 3) == FOLIO_STATE_READY,
            "warm corpus fixture");
    folio_state_request_tag_catalog(state);
    allocation_probe_fail_at(nth);
    enum folio_state_outcome result = finish_tag_catalog(state);
    allocation_probe_fail_at(0);
    require(result == FOLIO_STATE_READY || result == FOLIO_STATE_OUT_OF_MEMORY,
            "catalog state allocation has typed result");
    if (result == FOLIO_STATE_OUT_OF_MEMORY)
        require(folio_state_tag_catalog_status(state) == TAG_CATALOG_FAILED &&
                    !folio_state_index_work_pending(state) &&
                    folio_state_continue_index_filter(state) == FOLIO_STATE_READY,
                "failed catalog does not retry without another request");
    require(folio_state_index_filter_count(state) == 1 &&
                same_text(folio_state_index_filter_term(state), "one"),
            "catalog allocation failure leaves independent completed filter intact");
    folio_state_request_tag_catalog(state);
    require(finish_tag_catalog(state) == FOLIO_STATE_READY &&
                tag_catalog_count(folio_state_tag_catalog(state)) == 2,
            "same state retries catalog safely");
    folio_state_destroy(state);
    return result == FOLIO_STATE_READY;
}

static void verify_tag_edit_source(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_begin_tag_edit(state, u"", 0) == FOLIO_STATE_NOTHING_SELECTED,
            "tag edit needs a document");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"draft", 5) == FOLIO_STATE_READY &&
                note_tags_count(folio_state_edit_tags(state)) == 0,
            "session copies unsaved editor source, not the saved red/blue body");
    require(folio_state_enter_edit_tag(state, u" work ", 6) == FOLIO_STATE_READY &&
                note_tags_count(folio_state_edit_tags(state)) == 1,
            "new name uses the same trimming and identity as source tags");
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_tag_edit(state, u"draft", 5, &edit) == FOLIO_STATE_READY &&
                edit != nullptr && replace_edit_span(edit).start == 0 &&
                replace_edit_span(edit).end == 0,
            "unsaved plain text receives one prefix insertion");
    const char16_t expected[] = u"---\rtags:\r  - \"work\"\r---\r";
    require(replace_edit_length(edit) == sizeof expected / sizeof expected[0] - 1 &&
                memcmp(replace_edit_units(edit), expected, sizeof expected) == 0 &&
                adapter.note_writes == 0 && adapter.archives == 0,
            "fixed native replacement without early save or history");
    replace_edit_destroy(edit);
    require(folio_state_enter_edit_tag(state, u"work", 4) == FOLIO_STATE_READY &&
                note_tags_count(folio_state_edit_tags(state)) == 1,
            "entering an existing name does not unexpectedly remove it");
    require(folio_state_toggle_edit_tag(state, note_tags_at(folio_state_edit_tags(state), 0)) ==
                    FOLIO_STATE_READY &&
                folio_state_apply_tag_edit(state, u"draft", 5, &edit) == FOLIO_STATE_READY &&
                edit == nullptr,
            "toggling back returns no edit and no dirty change");
    folio_state_close_tag_edit(state);
    require(folio_state_enter_edit_tag(state, u"work", 4) == FOLIO_STATE_TAGS_STALE &&
                folio_state_apply_tag_edit(state, u"draft", 5, &edit) == FOLIO_STATE_TAGS_STALE,
            "cancelled session cannot apply or mutate");
    folio_state_destroy(state);
}

static void verify_tag_edit_stale(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    adapter.rare_body = adapter.note_body;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"draft", 5) == FOLIO_STATE_READY,
            "staged edit source");
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_tag_edit(state, u"changed", 7, &edit) == FOLIO_STATE_TAGS_STALE &&
                edit == nullptr && folio_state_edit_tags(state) != nullptr,
            "changed body refuses stale apply without clearing staged choices");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_apply_tag_edit(state, u"draft", 5, &edit) == FOLIO_STATE_TAGS_STALE &&
                folio_state_edit_tags(state) == nullptr,
            "different named note with identical content closes the session");
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"", 0) == FOLIO_STATE_READY &&
                folio_state_new_note(state, 0, FOLIO_UNTITLED_DISCARD) == FOLIO_STATE_READY &&
                folio_state_apply_tag_edit(state, u"", 0, &edit) == FOLIO_STATE_TAGS_STALE,
            "another empty untitled note cannot reuse identical prior snapshot");
    folio_state_destroy(state);
}

static void verify_tag_edit_view(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    adapter.note_body = "---\ntags: [red]\r\n---\nbody";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"ignored viewer text", 19) == FOLIO_STATE_READY,
            "view session uses normalized original, never rendered text");
    struct replace_edit *edit = nullptr;
    require(folio_state_apply_tag_edit(state, u"", 0, &edit) == FOLIO_STATE_READY &&
                edit == nullptr,
            "unchanged view assignment avoids edit mode and a native operation");
    require(folio_state_enter_edit_tag(state, u"blue", 4) == FOLIO_STATE_READY &&
                folio_state_apply_tag_edit(state, u"", 0, &edit) == FOLIO_STATE_READY &&
                edit != nullptr && folio_state_pane_mode(state) == PANE_MODE_VIEW &&
                adapter.note_writes == 0,
            "view preparation produces a replacement for the existing UI save path");
    replace_edit_destroy(edit);
    struct note_name *name = accepted_note_name("renamed");
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_READY &&
                folio_state_apply_tag_edit(state, u"", 0, &edit) == FOLIO_STATE_TAGS_STALE,
            "same text with a renamed destination rejects the old snapshot");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_tag_carry(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "---\r\ntitle: 'Keep' # comment\r\ntags: [red, '日本語']\r\n---\r\nBody";
    struct folio_state *state = ready_state(&adapter);
    size_t count = 999;
    require(folio_state_note_tag_count(state, u"", 0, &count) == FOLIO_STATE_NOTHING_SELECTED &&
                count == 999,
            "no-document tag summary preserves output");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "select portable tagged source");
    struct note_name *name = accepted_note_name("tag-renamed");
    require(folio_state_rename_note(state, name, u"rendered", 8) == FOLIO_STATE_READY &&
                same_text(folio_state_pane_text(state), adapter.note_body) &&
                folio_state_note_tag_count(state, u"rendered", 8, &count) == FOLIO_STATE_READY &&
                count == 2,
            "rename keeps exact source and derives tags from source rather than rendered text");
    note_name_destroy(name);
    require(folio_state_move_note(state, at(0, 0), at(1, 1)) == FOLIO_STATE_READY &&
                same_text(folio_state_pane_text(state), adapter.note_body),
            "category move keeps the same tagged source");
    name = accepted_note_name("tag-copy");
    struct note_destination destination = {.category = 0, .name = name};
    require(folio_state_store_new(state, &destination, u"rendered", 8) == FOLIO_STATE_READY &&
                same_text(adapter.written_body, adapter.note_body) &&
                folio_state_begin_tag_edit(state, u"", 0) == FOLIO_STATE_READY &&
                note_tags_count(folio_state_edit_tags(state)) == 2,
            "VIEW SaveAs copies full front matter and its tags");
    note_name_destroy(name);
    folio_state_destroy(state);
}

static void verify_tag_summary(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled source for current summary");
    size_t count = 999;
    const char16_t source[] = u"---\rtags: [new]\r---\rbody";
    require(folio_state_note_tag_count(state, source, sizeof source / sizeof source[0] - 1,
                                       &count) == FOLIO_STATE_READY &&
                count == 1,
            "current summary uses unsaved front matter");
    const char16_t unsupported[] = u"---\rtags: *alias\r---\rbody";
    require(folio_state_note_tag_count(state, unsupported,
                                       sizeof unsupported / sizeof unsupported[0] - 1,
                                       &count) == FOLIO_STATE_TAGS_UNAVAILABLE &&
                count == 1,
            "unsupported summary preserves output and is not known untagged");
    const char16_t invalid[] = {0xd800};
    require(folio_state_note_tag_count(state, invalid, 1, &count) == FOLIO_STATE_NOTE_MALFORMED &&
                count == 1,
            "invalid native source preserves summary output");
    folio_state_destroy(state);
}

static void verify_tag_edit_rejections(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"draft", 5) == FOLIO_STATE_READY,
            "valid session before invalid replacement");
    const struct note_tags *prior = folio_state_edit_tags(state);
    const char16_t unsupported[] = u"---\rtags: *alias\r---\rbody";
    require(folio_state_begin_tag_edit(state, unsupported,
                                       sizeof unsupported / sizeof unsupported[0] - 1) ==
                    FOLIO_STATE_TAGS_UNAVAILABLE &&
                folio_state_edit_tags(state) == prior,
            "unsupported header preserves existing session instead of prepending another header");
    require(folio_state_enter_edit_tag(state, u" ", 1) == FOLIO_STATE_TAG_INVALID &&
                folio_state_enter_edit_tag(state, u"bad\nname", 8) == FOLIO_STATE_TAG_INVALID,
            "empty and control character names rejected before staging");
    char16_t long_name[65];
    for (size_t index = 0; index < 65; ++index)
        long_name[index] = u'a';
    require(folio_state_enter_edit_tag(state, long_name, 65) == FOLIO_STATE_TAG_LIMIT &&
                folio_state_edit_tags(state) == prior,
            "overlong tag refuses without mutation");
    const char16_t invalid[] = {0xd800};
    require(folio_state_enter_edit_tag(state, invalid, 1) == FOLIO_STATE_TAG_INVALID &&
                folio_state_begin_tag_edit(state, invalid, 1) == FOLIO_STATE_NOTE_MALFORMED,
            "invalid UTF16 is typed at the source boundary");
    folio_state_destroy(state);
}

static bool tag_session_allocation(size_t nth)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"old", 3) == FOLIO_STATE_READY,
            "session before replacement allocation");
    const struct note_tags *prior = folio_state_edit_tags(state);
    allocation_probe_fail_at(nth);
    enum folio_state_outcome result = folio_state_begin_tag_edit(state, u"new", 3);
    allocation_probe_fail_at(0);
    require(result == FOLIO_STATE_READY || result == FOLIO_STATE_OUT_OF_MEMORY,
            "snapshot creation has a typed failure");
    if (result == FOLIO_STATE_OUT_OF_MEMORY)
        require(folio_state_edit_tags(state) == prior, "failed replacement keeps original session");
    require(folio_state_begin_tag_edit(state, u"new", 3) == FOLIO_STATE_READY,
            "same state retries after every snapshot allocation failure");
    folio_state_destroy(state);
    return result == FOLIO_STATE_READY;
}

static bool tag_apply_allocation(size_t nth)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"draft", 5) == FOLIO_STATE_READY &&
                folio_state_enter_edit_tag(state, u"work", 4) == FOLIO_STATE_READY,
            "changed staged selection before apply failure");
    struct replace_edit *edit = nullptr;
    allocation_probe_fail_at(nth);
    enum folio_state_outcome result = folio_state_apply_tag_edit(state, u"draft", 5, &edit);
    allocation_probe_fail_at(0);
    require(result == FOLIO_STATE_READY || result == FOLIO_STATE_OUT_OF_MEMORY,
            "apply has typed allocation failure");
    if (result == FOLIO_STATE_OUT_OF_MEMORY)
        require(edit == nullptr && note_tags_count(folio_state_edit_tags(state)) == 1,
                "failed apply preserves out and staged choice");
    replace_edit_destroy(edit);
    edit = nullptr;
    require(folio_state_apply_tag_edit(state, u"draft", 5, &edit) == FOLIO_STATE_READY &&
                edit != nullptr && adapter.note_writes == 0,
            "apply retries on same state without saving early");
    replace_edit_destroy(edit);
    folio_state_destroy(state);
    return result == FOLIO_STATE_READY;
}

static void verify_catalog_copy_failure(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY,
            "completed tag-only query before a saved-copy failure");
    const char16_t changed[] = u"---\rtags: [blue]\r---\rneedle";
    size_t length = sizeof changed / sizeof changed[0] - 1;
    adapter.copy_fail_at = 1;
    enum folio_state_outcome saved = folio_state_store_note(state, changed, length);
    allocation_probe_fail_at(0);
    adapter.copy_fail_at = 0;
    require(saved == FOLIO_STATE_OUT_OF_MEMORY && adapter.note_writes == 1,
            "publication succeeded but corpus repair remains pending");
    folio_state_request_tag_catalog(state);
    allocation_probe_fail_at(1);
    enum folio_state_outcome prepared = folio_state_continue_index_filter(state);
    allocation_probe_fail_at(0);
    require(prepared == FOLIO_STATE_OUT_OF_MEMORY &&
                folio_state_tag_catalog_status(state) == TAG_CATALOG_FAILED &&
                !folio_state_index_work_pending(state) &&
                note_tags_equal(folio_state_tag_filter(state), red) &&
                folio_state_index_filter_count(state) == 1,
            "catalog-only synchronization failure preserves the entire completed query");
    folio_state_request_tag_catalog(state);
    require(finish_tag_catalog(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 &&
                tag_catalog_count(folio_state_tag_catalog(state)) == 1 &&
                adapter.note_writes == 1 && adapter.archives == 1,
            "retry repairs saved copy and tag query without republishing or archiving");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void tag_edit_allocation_sweep(void)
{
    size_t nth = 1;
    while (nth < 128 && !tag_session_allocation(nth))
        ++nth;
    require(nth > 1 && nth < 128, "session sweep finishes after real injection");
    printf("tag session: %zu allocation failures and retries\n", nth - 1);
    nth = 1;
    while (nth < 128 && !tag_apply_allocation(nth))
        ++nth;
    require(nth > 1 && nth < 128, "apply sweep finishes after real injection");
    printf("tag apply: %zu allocation failures and retries\n", nth - 1);
}

static void tag_catalog_allocation_sweep(void)
{
    for (size_t nth = 1; nth < 128; ++nth)
        if (tag_catalog_allocation(nth))
        {
            require(nth > 1, "catalog state sweep requires actual injection");
            printf("tag catalog state: %zu allocation failures and retries\n", nth - 1);
            return;
        }
    test_failure("tag catalog state allocation sweep did not finish");
}

void run_tag_filter_allocation_tests(void)
{
    finder = test_regex_port();
    tag_catalog_allocation_sweep();
    verify_catalog_copy_failure();
    tag_edit_allocation_sweep();
    struct note_tags *red = selected_tags(1);
    for (size_t nth = 1; nth < 128; ++nth)
        if (tag_filter_allocation(nth, red))
        {
            require(nth > 1, "tag filter sweep requires actual allocation failure injection");
            printf("tag filter: %zu allocation failures and successful retries\n", nth - 1);
            note_tags_destroy(red);
            return;
        }
    note_tags_destroy(red);
    test_failure("tag filter allocation sweep did not finish");
}

static void verify_note_tag_summary(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    size_t count = 999;
    require(folio_state_note_tag_count(state, u"", 0, &count) == FOLIO_STATE_NOTHING_SELECTED &&
                count == 999,
            "summary requires a document and failure preserves out");
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_note_tag_count(state, u"ignored", 7, &count) == FOLIO_STATE_READY &&
                count == 2,
            "view summary uses saved original metadata");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_begin_tag_edit(state, u"draft", 5) == FOLIO_STATE_READY,
            "summary must not replace an active tag session");
    const struct note_tags *session = folio_state_edit_tags(state);
    const char16_t one[] = u"---\rtags: [new]\r---\rbody";
    require(folio_state_note_tag_count(state, one, sizeof one / sizeof one[0] - 1, &count) ==
                    FOLIO_STATE_READY &&
                count == 1 && folio_state_edit_tags(state) == session,
            "editor summary sees unsaved tags without changing staged assignment");
    const char16_t unsupported[] = u"---\rtags: *alias\r---\rbody";
    require(folio_state_note_tag_count(state, unsupported,
                                       sizeof unsupported / sizeof unsupported[0] - 1,
                                       &count) == FOLIO_STATE_TAGS_UNAVAILABLE &&
                count == 1,
            "unavailable metadata is not shown as no tags");
    folio_state_destroy(state);
}

void run_tag_filter_state_tests(void)
{
    finder = test_regex_port();
    verify_tag_filter_queries();
    verify_empty_tag_mode();
    verify_tag_filter_clearing();
    verify_tag_filter_save();
    verify_tag_catalog_lifecycle();
    verify_tag_catalog_unavailable();
    verify_tag_edit_source();
    verify_tag_edit_stale();
    verify_tag_edit_view();
    verify_tag_carry();
    verify_tag_summary();
    verify_tag_edit_rejections();
    verify_note_tag_summary();
}

static void checked_save_refusals(struct folio_state *_Nonnull state,
                                  struct persistence_adapter *_Nonnull adapter)
{
    const char *pane = folio_state_pane_text(state);
    require(folio_state_store_note_checked(state, u"saved", 5) == FOLIO_STATE_READY &&
                adapter->archives == 0 && adapter->note_writes == 0,
            "unchanged checked entry verifies without history or write");
    adapter->note_body = "externally changed";
    require(
        folio_state_store_note_checked(state, u"edited", 6) == FOLIO_STATE_NOTE_SOURCE_CHANGED &&
            folio_state_store_note_checked(state, u"saved", 5) == FOLIO_STATE_NOTE_SOURCE_CHANGED,
        "changed and unchanged editor snapshots both reject an external source change");
    adapter->note_outcome = PERSISTENCE_ABSENT;
    require(folio_state_store_note_checked(state, u"edited", 6) == FOLIO_STATE_NOTE_SOURCE_CHANGED,
            "checked save does not recreate a source deleted after opening");
    adapter->note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_store_note_checked(state, u"edited", 6) == FOLIO_STATE_NOTE_UNREADABLE,
            "unreadable expected source is refused before history");
    require(adapter->archives == 0 && adapter->note_writes == 0 &&
                folio_state_pane_text(state) == pane &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "every precondition refusal preserves the current document and mode");
    adapter->note_outcome = PERSISTENCE_LOADED;
    adapter->note_body = "saved";
}

static void checked_save_failures(struct folio_state *_Nonnull state,
                                  struct persistence_adapter *_Nonnull adapter)
{
    adapter->archive_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note_checked(state, u"edited", 6) == FOLIO_STATE_HISTORY_FAILED &&
                adapter->archives == 1 && adapter->note_writes == 0,
            "checked save refuses publication after history failure");
    adapter->archive_outcome = PERSISTENCE_STORED;
    adapter->note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_store_note_checked(state, u"edited", 6) == FOLIO_STATE_NOTE_STORE_FAILED &&
                adapter->archives == 2 && adapter->note_writes == 1 &&
                folio_state_store_revision(state) == 0,
            "publication failure can rotate history but does not claim a saved body");
    adapter->note_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note_checked(state, u"edited", 6) == FOLIO_STATE_READY &&
                adapter->archives == 3 && adapter->note_writes == 2 &&
                folio_state_store_revision(state) == 1 &&
                same_text(adapter->written_body, "edited") &&
                folio_state_unsaved(state, u"edited", 6) == FOLIO_NOTE_SAME,
            "same state retries the common checked save and adopts the actual publication");
}

static struct persistence_adapter management_adapter(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.note_body = "---\ntags: [red, blue]\n---\nneedle";
    adapter.remember_note_writes = true;
    return adapter;
}

static void prepare_management(struct folio_state *_Nonnull state,
                               struct persistence_adapter *_Nonnull adapter,
                               const char *_Nonnull from, const char *_Nullable to)
{
    struct note_tag *source = nullptr;
    struct note_tag *target = nullptr;
    require(note_tag_create(from, strlen(from), &source) == NOTE_TAG_READY, "source tag fixture");
    if (to != nullptr)
        require(note_tag_create(to, strlen(to), &target) == NOTE_TAG_READY,
                "replacement tag fixture");
    size_t writes = adapter->note_writes, archives = adapter->archives;
    require(folio_state_prepare_tag_management(state, source, target) == FOLIO_STATE_READY,
            "request a fresh all-note preview");
    note_tag_destroy(source);
    note_tag_destroy(target);
    for (size_t step = 0; step < 32; ++step)
    {
        size_t reads = adapter->note_reads;
        enum tag_management_step outcome = folio_state_continue_tag_management(state);
        require(adapter->note_reads <= reads + 1 && adapter->note_writes == writes &&
                    adapter->archives == archives,
                "preview reads at most one body and never saves");
        if (outcome == TAG_MANAGEMENT_FINISHED)
            break;
    }
    require(folio_state_tag_management_summary(state).phase == TAG_MANAGEMENT_PREVIEW,
            "only a complete fresh scan publishes the preview");
}

static void verify_management_success(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "selected note stays in edit mode");
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_set_tag_filter(state, red, TAG_MATCH_ANY) == FOLIO_STATE_READY &&
                finish_test_index_filter(state) == FOLIO_STATE_READY,
            "existing combined query");
    folio_state_set_note_scope(state, NOTE_SCOPE_FAVORITES);
    const char *pane = folio_state_pane_text(state);
    prepare_management(state, &adapter, "red", "blue");
    require(folio_state_tag_management_summary(state).pending == 2 &&
                folio_state_pane_text(state) == pane &&
                same_text(tag_update_note(folio_state_tag_management_at(state, 0)), "one"),
            "all saved notes are previewed despite a filter hiding every row");
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_READY,
            "complete preview can start publication");
    require(folio_state_continue_tag_management(state) == TAG_MANAGEMENT_CURRENT_CHANGED,
            "first publication reports immediate current adoption");
    require(folio_state_store_revision(state) == 1 && adapter.note_writes == 1,
            "current publication is counted exactly once");
    require(same_text(folio_state_pane_text(state), "---\ntags: [blue]\n---\nneedle"),
            "adopted current body equals the merge golden");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "adopting editable source preserves edit mode");
    require(folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_FINISHED &&
                folio_state_tag_management_summary(state).changed == 2 &&
                folio_state_store_revision(state) == 2 && adapter.archives == 2,
            "each publication counts once independently");
    require(same_text(folio_state_index_filter_term(state), "needle") &&
                note_tags_equal(folio_state_tag_filter(state), red) &&
                folio_state_tag_match_mode(state) == TAG_MATCH_ANY &&
                folio_state_note_scope(state) == NOTE_SCOPE_FAVORITES &&
                finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0,
            "query meanings survive the batch");
    prepare_management(state, &adapter, "red", "blue");
    require(folio_state_tag_management_summary(state).pending == 0 &&
                folio_state_apply_tag_management(state) == FOLIO_STATE_NAME_UNCHANGED &&
                adapter.note_writes == 2,
            "fresh retry never repeats already published changes");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void verify_management_stop(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tag *red = nullptr;
    require(note_tag_create("red", 3, &red) == NOTE_TAG_READY &&
                folio_state_prepare_tag_management(state, red, nullptr) == FOLIO_STATE_READY &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED,
            "partially prepare one note");
    folio_state_stop_tag_management(state);
    require(folio_state_tag_management_summary(state).phase == TAG_MANAGEMENT_RESULT &&
                folio_state_tag_management_summary(state).notice == FOLIO_STATE_CANCELLED &&
                folio_state_apply_tag_management(state) == FOLIO_STATE_TAGS_STALE &&
                adapter.note_writes == 0,
            "cancelled partial preview cannot be applied");
    prepare_management(state, &adapter, "red", nullptr);
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_READY &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED,
            "remove the tag from only the first note");
    folio_state_stop_tag_management(state);
    require(folio_state_tag_management_summary(state).changed == 1 &&
                folio_state_tag_management_summary(state).pending == 1 &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_IDLE &&
                adapter.note_writes == 1,
            "stop preserves success and leaves the rest unprocessed");
    prepare_management(state, &adapter, "red", nullptr);
    require(folio_state_tag_management_summary(state).pending == 1 &&
                folio_state_tag_management_status(state, 0) == TAG_APPLY_NOT_TARGET &&
                folio_state_apply_tag_management(state) == FOLIO_STATE_READY &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_FINISHED &&
                adapter.note_writes == 2 && adapter.archives == 2,
            "retry rescans and applies only the remaining source tag");
    folio_state_close_tag_management(state);
    require(folio_state_tag_management_summary(state).phase == TAG_MANAGEMENT_CLOSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_IDLE,
            "closed management has no continuation side effect");
    note_tag_destroy(red);
    folio_state_destroy(state);
}

static void verify_management_partial_failure(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    prepare_management(state, &adapter, "red", "green");
    adapter.rare_note = "one";
    adapter.rare_body = "external change";
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_READY &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_tag_management_status(state, 0) == TAG_APPLY_CONFLICT &&
                adapter.archives == 0 && adapter.note_writes == 0,
            "preview source conflict is skipped before history and publication");
    adapter.note_write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_FINISHED &&
                folio_state_tag_management_status(state, 1) == TAG_APPLY_WRITE_FAILED &&
                folio_state_tag_management_summary(state).skipped == 1 &&
                folio_state_tag_management_summary(state).failed == 1 &&
                folio_state_tag_management_summary(state).changed == 0 &&
                folio_state_store_revision(state) == 0 && adapter.archives == 1,
            "history rotation is not falsely counted as a published body");
    adapter.note_write_outcome = PERSISTENCE_STORED;
    prepare_management(state, &adapter, "red", "green");
    require(folio_state_tag_management_summary(state).pending == 1 &&
                folio_state_apply_tag_management(state) == FOLIO_STATE_READY &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_FINISHED &&
                folio_state_tag_management_summary(state).changed == 1 &&
                folio_state_store_revision(state) == 1 && adapter.saved_count == 1,
            "retry respects the external document and repairs only the failed note");
    folio_state_destroy(state);
}

static void verify_management_unknown(void)
{
    struct persistence_adapter adapter = management_adapter();
    adapter.missing_note = "one";
    adapter.rare_note = "two";
    adapter.rare_body = "---\ntags: *alias\n---\nbody";
    struct folio_state *state = ready_state(&adapter);
    prepare_management(state, &adapter, "red", nullptr);
    require(folio_state_tag_management_summary(state).skipped == 2 &&
                folio_state_tag_management_summary(state).pending == 0 &&
                folio_state_tag_management_status(state, 0) == TAG_APPLY_UNREADABLE &&
                folio_state_tag_management_status(state, 1) == TAG_APPLY_UNAVAILABLE &&
                folio_state_apply_tag_management(state) == FOLIO_STATE_NAME_UNCHANGED,
            "unknown membership is visible and is never considered successful completion");
    adapter.missing_note = nullptr;
    adapter.rare_note = nullptr;
    prepare_management(state, &adapter, "red", "red");
    require(folio_state_tag_management_summary(state).pending == 0 &&
                folio_state_tag_management_summary(state).skipped == 0,
            "same-name request changes no source representation");
    prepare_management(state, &adapter, "red", nullptr);
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_READY, "apply fresh plan");
    adapter.archive_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_tag_management_status(state, 0) == TAG_APPLY_HISTORY_FAILED &&
                adapter.note_writes == 0,
            "archive failure blocks only that publication");
    adapter.archive_outcome = PERSISTENCE_STORED;
    adapter.missing_note = "two";
    require(folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_tag_management_status(state, 1) == TAG_APPLY_CONFLICT &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_FINISHED,
            "source deleted after preview is not silently recreated");
    folio_state_destroy(state);
}

static void verify_management_fresh_merge(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    folio_state_request_tag_catalog(state);
    require(finish_test_index_filter(state) == FOLIO_STATE_READY,
            "initial catalog precedes an external change");
    adapter.rare_note = "two";
    adapter.rare_body = "---\ntags: [green]\n---\nexternal";
    prepare_management(state, &adapter, "red", "green");
    require(folio_state_tag_management_summary(state).merging &&
                folio_state_tag_management_summary(state).pending == 1,
            "fresh scan notices an existing target even in a non-target note");
    adapter.rare_note = nullptr;
    prepare_management(state, &adapter, "red", "green");
    require(!folio_state_tag_management_summary(state).merging,
            "new scan clears an obsolete merge warning");
    prepare_management(state, &adapter, "red", "red");
    require(!folio_state_tag_management_summary(state).merging,
            "same-name no-op does not claim a merge");
    folio_state_destroy(state);
}

static void verify_management_untitled_and_identity(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "untitled document remains outside the saved-note plan");
    const char *pane = folio_state_pane_text(state);
    prepare_management(state, &adapter, "red", nullptr);
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_READY &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_PROGRESSED &&
                folio_state_continue_tag_management(state) == TAG_MANAGEMENT_FINISHED &&
                folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_text(state) == pane && adapter.creates == 0,
            "batch publication never adopts or saves the untitled native document");
    prepare_management(state, &adapter, "blue", "green");
    require(folio_state_move_note(state, (struct note_ref){0, 0}, (struct note_ref){0, 1}) ==
                FOLIO_STATE_READY,
            "change row identity after preparation");
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_TAGS_STALE &&
                folio_state_tag_management_summary(state).phase == TAG_MANAGEMENT_RESULT &&
                adapter.note_writes == 2,
            "stale row mapping cannot start publication");
    folio_state_destroy(state);
}

static bool management_prepare_allocation(size_t nth)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tag *red = nullptr;
    require(note_tag_create("red", 3, &red) == NOTE_TAG_READY, "allocation source tag");
    allocation_probe_fail_at(nth);
    enum folio_state_outcome result = folio_state_prepare_tag_management(state, red, nullptr);
    for (size_t step = 0;
         result == FOLIO_STATE_READY && step < 4 &&
         folio_state_tag_management_summary(state).phase == TAG_MANAGEMENT_PREPARING;
         ++step)
        (void)folio_state_continue_tag_management(state);
    struct tag_management_summary summary = folio_state_tag_management_summary(state);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(adapter.note_writes == 0 && adapter.archives == 0,
            "even a failed preparation never changes stored notes");
    if (result != FOLIO_STATE_READY || summary.phase != TAG_MANAGEMENT_PREVIEW)
    {
        require(result == FOLIO_STATE_OUT_OF_MEMORY || summary.notice == FOLIO_STATE_OUT_OF_MEMORY,
                "preparation allocation failure remains typed");
        require(folio_state_apply_tag_management(state) == FOLIO_STATE_TAGS_STALE,
                "failed preparation cannot apply a partial list");
        prepare_management(state, &adapter, "red", nullptr);
        require(folio_state_tag_management_summary(state).pending == 2,
                "same state can rescan after every preparation allocation failure");
    }
    note_tag_destroy(red);
    folio_state_destroy(state);
    return nth > calls;
}

static bool management_apply_allocation(size_t nth)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "selected edit allocation fixture");
    prepare_management(state, &adapter, "red", nullptr);
    const char *before = folio_state_pane_text(state);
    require(folio_state_apply_tag_management(state) == FOLIO_STATE_READY, "start allocation apply");
    allocation_probe_fail_at(nth);
    enum tag_management_step step = folio_state_continue_tag_management(state);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    struct tag_management_summary summary = folio_state_tag_management_summary(state);
    require(summary.changed == adapter.saved_count &&
                folio_state_store_revision(state) == adapter.saved_count,
            "publications always determine changed count even under allocation failure");
    if (adapter.saved_count == 0)
        require(step == TAG_MANAGEMENT_PROGRESSED && summary.failed == 1 &&
                    folio_state_tag_management_status(state, 0) == TAG_APPLY_OUT_OF_MEMORY &&
                    folio_state_pane_text(state) == before && adapter.archives == 0,
                "current adoption resources and expected read are prepared before history");
    if (adapter.saved_count == 1)
        require(step == TAG_MANAGEMENT_CURRENT_CHANGED && summary.failed == 0 &&
                    same_text(folio_state_pane_text(state), "---\ntags: [blue]\n---\nneedle"),
                "successful publication has an immediately adopted current body");
    folio_state_stop_tag_management(state);
    prepare_management(state, &adapter, "red", nullptr);
    require(folio_state_tag_management_summary(state).pending == 2 - adapter.saved_count,
            "allocation retry sees exactly the remaining persisted sources");
    folio_state_destroy(state);
    return nth > calls;
}

static void management_allocation_sweeps(void)
{
    size_t nth = 1;
    while (nth < 1024 && !management_prepare_allocation(nth))
        ++nth;
    require(nth < 1024, "preparation allocation sweep reaches uninjected success");
    printf("tag preparation: %zu allocation boundaries\n", nth - 1);
    nth = 1;
    while (nth < 1024 && !management_apply_allocation(nth))
        ++nth;
    require(nth < 1024, "application allocation sweep reaches uninjected success");
    printf("tag application: %zu allocation boundaries\n", nth - 1);
}

void run_tag_management_state_tests(void)
{
    finder = test_regex_port();
    struct persistence_adapter adapter = healthy_adapter();
    adapter.note_body = "saved";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "checked management entry starts from a named editor");
    checked_save_refusals(state, &adapter);
    checked_save_failures(state, &adapter);
    folio_state_destroy(state);
    verify_management_success();
    verify_management_stop();
    verify_management_partial_failure();
    verify_management_unknown();
    verify_management_fresh_merge();
    verify_management_untitled_and_identity();
    management_allocation_sweeps();
}

static enum folio_state_outcome finish_discovery_test(struct folio_state *_Nonnull state,
                                                      struct persistence_adapter *_Nonnull adapter)
{
    enum folio_state_outcome result = FOLIO_STATE_READY;
    for (size_t step = 0; step < 128 && folio_state_discovery_work_pending(state); ++step)
    {
        size_t reads = adapter->note_reads;
        result = folio_state_continue_discovery(state);
        require(adapter->note_reads <= reads + 1, "discovery reads at most one source per idle");
        if (result != FOLIO_STATE_READY)
            return result;
    }
    require(!folio_state_discovery_work_pending(state), "discovery finishes finite fixture");
    return result;
}

static void verify_discovery_lazy_preview(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    adapter.missing_note = "one";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_new_note(state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_open_discovery(state) == FOLIO_STATE_READY && adapter.note_reads == 0,
            "untitled discovery neither saves nor preloads bodies");
    require(folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).phase == DISCOVERY_READY &&
                folio_state_discovery_summary(state).shown == 2 && adapter.note_reads == 0,
            "name results include unread documents without probing any body");
    uint64_t results_revision = folio_state_discovery_summary(state).results_revision;
    require(folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).preview == DISCOVERY_PREVIEW_UNREADABLE &&
                folio_state_discovery_summary(state).unreadable == 1 && adapter.note_reads == 1,
            "selected unreadable body is distinct from no match");
    require(
        folio_state_select_discovery(state, 1) == FOLIO_STATE_READY &&
            folio_state_discovery_excerpt(state) == nullptr &&
            finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
            same_text(note_excerpt_bytes(folio_state_discovery_excerpt(state)), adapter.rare_body),
        "selection clears the old preview and exposes exact saved source");
    require(folio_state_discovery_summary(state).results_revision == results_revision,
            "preview failure and selection preserve candidate identities");
    require(folio_state_query_discovery(state, u"to", 2) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 0 &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 1 && adapter.note_reads == 2,
            "warm fuzzy query reuses observed source without another read");
    require(folio_state_discovery_summary(state).results_revision > results_revision,
            "query completion invalidates previous result identities");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                same_text(folio_state_pane_text(state), "") && adapter.creates == 0 &&
                adapter.note_writes == 0 && adapter.archives == 0,
            "candidate navigation preserves untitled document and never publishes");
    folio_state_close_discovery(state);
    require(folio_state_discovery_summary(state).phase == DISCOVERY_CLOSED &&
                folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 2,
            "closed session has no pending work");
    folio_state_destroy(state);
}

static void verify_discovery_conditions(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ANY) == FOLIO_STATE_READY &&
                test_index_filter_complete(state, u"needle", 6) == FOLIO_STATE_READY &&
                folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                folio_state_set_note_order(state, NOTE_ORDER_FAVORITES_FIRST) == FOLIO_STATE_READY,
            "establish independent list term, tag, favorite and ordering");
    folio_state_set_note_scope(state, NOTE_SCOPE_FAVORITES);
    size_t reads = adapter.note_reads;
    require(folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == reads + 2 &&
                folio_state_discovery_summary(state).shown == 1 &&
                folio_state_discovery_tag_mode(state) == TAG_MATCH_ANY,
            "tag discovery freshly reads all notes once and previews the shared source");
    require(folio_state_query_discovery(state, u"B", 1) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == reads + 2,
            "same-session query reuses the completed fresh tag pass");
    require(folio_state_remove_discovery_tag(state, 0) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 1,
            "removing this session tag preserves its star restriction");
    folio_state_clear_discovery_conditions(state);
    require(finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 2 &&
                folio_state_discovery_order(state) == NOTE_ORDER_FAVORITES_FIRST &&
                folio_state_discovery_row(state, 0).favorite &&
                same_text(folio_state_index_filter_term(state), "needle") &&
                note_tags_equal(folio_state_tag_filter(state), red) &&
                folio_state_note_scope(state) == NOTE_SCOPE_FAVORITES,
            "clearing session conditions preserves priority and every list condition");
    require(folio_state_remove_discovery_tag(state, 0) == FOLIO_STATE_TAG_INVALID &&
                folio_state_select_discovery(state, 9) == FOLIO_STATE_NO_SUCH_NOTE,
            "absent tag and stale row reject without changing valid results");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void verify_discovery_partial_tags(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    adapter.rare_body = "---\ntags: *alias\n---\nbody";
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "fresh tag preparation yields after one document");
    require(folio_state_query_discovery(state, u"two", 3) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 2 && folio_state_discovery_summary(state).shown == 0 &&
                folio_state_discovery_summary(state).unsupported == 1,
            "query change retains corpus progress and unsupported tags differ from complete zero");
    folio_state_clear_discovery_conditions(state);
    require(
        finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
            folio_state_discovery_summary(state).shown == 1 && adapter.note_reads == 2 &&
            same_text(note_excerpt_bytes(folio_state_discovery_excerpt(state)), adapter.rare_body),
        "name-only search can preview unsupported metadata as untouched source");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void verify_discovery_limits(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    struct folio_state *state = ready_state(&adapter);
    char16_t emojis[514];
    for (size_t at = 0; at < 514; at += 2)
    {
        emojis[at] = 0xd83d;
        emojis[at + 1] = 0xde00;
    }
    require(folio_state_query_discovery(state, u"", 0) == FOLIO_STATE_CANCELLED &&
                folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                folio_state_query_discovery(state, emojis, 512) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY,
            "exact 256-scalar and 1024-byte query is accepted");
    require(folio_state_query_discovery(state, emojis, 514) == FOLIO_STATE_DISCOVERY_QUERY_LIMIT &&
                folio_state_discovery_summary(state).phase == DISCOVERY_FAILED &&
                folio_state_discovery_summary(state).shown == 0,
            "over-limit query cannot retain old results");
    folio_state_clear_discovery_conditions(state);
    require(!folio_state_discovery_work_pending(state),
            "condition change cannot revive rejected query");
    const char16_t nul_queries[][3] = {{0, u'a', u'b'}, {u'a', 0, u'b'}, {u'a', u'b', 0}};
    for (size_t index = 0; index < 3; ++index)
        require(folio_state_query_discovery(state, nul_queries[index], 3) ==
                        FOLIO_STATE_DISCOVERY_QUERY_INVALID &&
                    folio_state_discovery_summary(state).phase == DISCOVERY_FAILED &&
                    folio_state_discovery_summary(state).shown == 0,
                "embedded NUL at each input position is rejected without stale results");
    const char16_t malformed[] = {0xd800};
    require(folio_state_query_discovery(state, malformed, 1) ==
                    FOLIO_STATE_DISCOVERY_QUERY_INVALID &&
                folio_state_query_discovery(state, u"one", 3) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 1,
            "malformed query is explicit and corrected query recovers");
    folio_state_destroy(state);
}

static void verify_discovery_identity(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    static const char *const names[] = {"one.md", "two", nullptr};
    adapter.scanned_notes = names;
    adapter.notes_text = "{\"version\":1,\"notes\":[\"one.md\",\"two\"]}";
    struct folio_state *state = ready_state(&adapter);
    struct note_identity *identity = nullptr;
    require(folio_state_capture_note(state, at(0, 0), &identity) == FOLIO_STATE_READY &&
                same_text(note_identity_note(identity), "one.md"),
            "identity preserves a literal .md stem without stripping twice");
    require(folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_move_note(state, at(0, 0), at(0, 1)) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 0,
            "ledger change immediately discards completed discovery rows");
    struct note_ref resolved = {0};
    require(folio_state_resolve_note(state, identity, &resolved) == FOLIO_STATE_READY &&
                resolved.category == 0 && resolved.note == 1 &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).selected == 1,
            "owned identity re-resolves moved row and restores candidate by name");
    require(folio_state_capture_note(state, at(8, 0), &identity) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                folio_state_capture_note(state, at(0, 8), &identity) == FOLIO_STATE_NO_SUCH_NOTE,
            "invalid row capture preserves owned identity");
    note_identity_destroy(identity);
    folio_state_destroy(state);
}

static void verify_discovery_corpus_replacement(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    char *body = malloc(200001);
    if (body == nullptr)
        test_failure("long corpus replacement fixture");
    memset(body, 'a', 200000);
    body[200000] = '\0';
    adapter.note_body = body;
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"z", 1) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_continue_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_pending(state),
            "list matcher yields with positions into a long cached body");
    adapter.note_body = "tiny";
    require(folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                same_text(note_excerpt_bytes(folio_state_discovery_excerpt(state)), "tiny") &&
                folio_state_index_filter_pending(state) &&
                same_text(folio_state_index_filter_term(state), "z"),
            "fresh preview replaces shared corpus while preserving the pending list intent");
    require(finish_test_index_filter(state) == FOLIO_STATE_READY &&
                folio_state_index_filter_count(state) == 0 &&
                folio_state_discovery_summary(state).shown == 2,
            "restarted list never reuses old body positions and name results stay valid");
    folio_state_destroy(state);
    free(body);
}

static void verify_discovery_saved_revision(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY &&
                folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY,
            "completed preview before saving changed editor source");
    require(folio_state_store_note(state, u"new body", 8) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 0 &&
                folio_state_discovery_excerpt(state) == nullptr &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                same_text(note_excerpt_bytes(folio_state_discovery_excerpt(state)), "new body"),
            "saved revision invalidates every result and refreshes selected preview source");
    require(adapter.note_writes == 1 && adapter.archives == 1 &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT,
            "preview refresh adds neither save nor history nor mode transition");
    folio_state_destroy(state);
}

static void verify_discovery_failed_read_retry(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY &&
                folio_state_open_discovery(state) == FOLIO_STATE_READY,
            "tag session before failed corpus reads");
    adapter.note_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_continue_discovery(state) == FOLIO_STATE_OUT_OF_MEMORY &&
                folio_state_query_discovery(state, u"o", 1) == FOLIO_STATE_READY &&
                folio_state_continue_discovery(state) == FOLIO_STATE_OUT_OF_MEMORY,
            "two read failures remain local to discovery across query replacement");
    require(folio_state_tag_filter(state) != nullptr &&
                note_tags_equal(folio_state_tag_filter(state), red),
            "discovery read failure preserves independent list conditions");
    adapter.note_outcome = PERSISTENCE_LOADED;
    require(folio_state_query_discovery(state, u"one", 3) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 1 &&
                folio_state_discovery_summary(state).unreadable == 0,
            "retry cannot skip an unobserved matching source after read failure");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static bool discovery_allocation(size_t nth)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY,
            "allocation fixture tag conditions");
    allocation_probe_fail_at(nth);
    enum folio_state_outcome outcome = folio_state_open_discovery(state);
    if (outcome == FOLIO_STATE_READY)
        outcome = finish_discovery_test(state, &adapter);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_OUT_OF_MEMORY,
            "discovery allocations return only success or explicit allocation failure");
    require(adapter.note_writes == 0 && adapter.creates == 0 && adapter.archives == 0,
            "allocation failure never publishes document changes");
    enum folio_state_outcome retry = folio_state_open_discovery(state);
    if (retry == FOLIO_STATE_READY)
        retry = finish_discovery_test(state, &adapter);
    if (retry != FOLIO_STATE_READY || folio_state_discovery_summary(state).shown != 1)
        printf("discovery retry: allocation=%zu initial=%u retry=%u shown=%zu\n", nth,
               (unsigned)outcome, (unsigned)retry, folio_state_discovery_summary(state).shown);
    require(retry == FOLIO_STATE_READY && folio_state_discovery_summary(state).shown == 1,
            "same owner retries after each discovery allocation failure");
    note_tags_destroy(red);
    folio_state_destroy(state);
    return calls < nth;
}

void run_discovery_state_tests(void)
{
    finder = test_regex_port();
    verify_discovery_lazy_preview();
    verify_discovery_conditions();
    verify_discovery_partial_tags();
    verify_discovery_limits();
    verify_discovery_identity();
    verify_discovery_corpus_replacement();
    verify_discovery_saved_revision();
    verify_discovery_failed_read_retry();
    size_t nth = 1;
    while (nth < 512 && !discovery_allocation(nth))
        ++nth;
    require(nth < 512, "discovery allocation sweep reaches success");
    printf("discovery session: %zu allocation boundaries\n", nth - 1);
}

static void start_body_discovery(struct folio_state *_Nonnull state,
                                 struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_query_discovery(state, u"needle", 6) == FOLIO_STATE_READY &&
                finish_discovery_test(state, adapter) == FOLIO_STATE_READY,
            "complete saved-body discovery through the shared session");
}

static void verify_body_discovery_modes(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_CANCELLED &&
                folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).phase == DISCOVERY_INPUT &&
                !folio_state_discovery_work_pending(state) &&
                folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 0,
            "empty body query prompts for input without starting any corpus pass");
    require(folio_state_set_discovery_mode(state, DISCOVERY_MODE_NAMES) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 1,
            "name mode first previews only the selected source");
    require(folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 0 &&
                folio_state_query_discovery(state, u"needle", 6) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 3 && folio_state_discovery_summary(state).shown == 1,
            "first nonempty body request starts a fresh pass despite an earlier prefix preview");
    struct index_ranked_note row = folio_state_discovery_row(state, 0);
    char snippet[note_excerpt_snippet_capacity];
    struct note_snippet_clip clip = {0};
    require(row.kind == INDEX_MATCH_BODY_FIRST && row.match.line == 4 &&
                folio_state_discovery_snippet(state, 0, snippet, &clip) == FOLIO_STATE_READY &&
                strcmp(snippet, "needle") == 0 && !clip.leading && !clip.trailing,
            "body row exposes the first matching source line and bounded display text");
    require(folio_state_query_discovery(state, u"two", 3) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 0 &&
                folio_state_set_discovery_mode(state, DISCOVERY_MODE_NAMES) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 1 && adapter.note_reads == 3,
            "query is preserved across mode changes and completed corpus is reused");
    require(folio_state_discovery_snippet(state, 0, snippet, &clip) == FOLIO_STATE_CANCELLED &&
                folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_query_discovery(state, u"other", 5) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 3,
            "name rows have no body snippet; reentering body mode performs no read");
    start_body_discovery(state, &adapter);
    require(adapter.note_reads == 5, "a reopened session requests its own new full pass");
    folio_state_destroy(state);
}

static void verify_body_discovery_partial_pass(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                folio_state_query_discovery(state, u"two", 3) == FOLIO_STATE_READY &&
                folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1 &&
                folio_state_discovery_summary(state).phase == DISCOVERY_PREPARING,
            "body preparation yields after the first saved source");
    require(
        folio_state_set_discovery_mode(state, DISCOVERY_MODE_NAMES) == FOLIO_STATE_READY &&
            finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
            adapter.note_reads == 2 && folio_state_discovery_summary(state).shown == 1 &&
            same_text(note_excerpt_bytes(folio_state_discovery_excerpt(state)), adapter.rare_body),
        "name matching does not wait for the body pass and can preview an unread source");
    require(folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_query_discovery(state, u"other", 5) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 2 &&
                folio_state_discovery_summary(state).source_checked == 2 &&
                folio_state_discovery_summary(state).shown == 1,
            "resuming the same pass reuses the preview observation instead of rereading it");
    adapter.rare_body = "---\ntags: [red]\n---\nneedle";
    prepare_management(state, &adapter, "red", nullptr);
    require(adapter.note_reads == 4 && folio_state_tag_management_summary(state).pending == 2 &&
                folio_state_discovery_summary(state).shown == 0,
            "a new explicit management pass invalidates body results and reads both sources anew");
    require(folio_state_query_discovery(state, u"needle", 6) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 4 && folio_state_discovery_summary(state).shown == 2,
            "discovery uses the newly refreshed shared sources after management preparation");
    folio_state_destroy(state);
}

static void verify_body_discovery_tags(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ANY) == FOLIO_STATE_READY &&
                folio_state_toggle_favorite(state, at(0, 0)) == FOLIO_STATE_READY &&
                folio_state_set_note_order(state, NOTE_ORDER_FAVORITES_FIRST) == FOLIO_STATE_READY,
            "body discovery condition fixture");
    folio_state_set_note_scope(state, NOTE_SCOPE_FAVORITES);
    require(folio_state_open_discovery(state) == FOLIO_STATE_READY &&
                folio_state_continue_discovery(state) == FOLIO_STATE_READY &&
                adapter.note_reads == 1 &&
                folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).phase == DISCOVERY_INPUT &&
                folio_state_query_discovery(state, u"needle", 6) == FOLIO_STATE_READY &&
                finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 2 && folio_state_discovery_summary(state).shown == 1 &&
                note_tags_equal(folio_state_discovery_tags(state), red) &&
                folio_state_discovery_tag_mode(state) == TAG_MATCH_ANY &&
                folio_state_discovery_scope(state) == NOTE_SCOPE_FAVORITES &&
                folio_state_discovery_order(state) == NOTE_ORDER_FAVORITES_FIRST,
            "body mode shares an already-started tag pass and retains every session condition");
    folio_state_clear_discovery_conditions(state);
    require(finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
                adapter.note_reads == 2 && note_tags_equal(folio_state_tag_filter(state), red) &&
                folio_state_note_scope(state) == NOTE_SCOPE_FAVORITES,
            "clearing body-search conditions neither rereads nor changes the drawer conditions");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void verify_body_discovery_unknown_and_input(void)
{
    struct persistence_adapter adapter = tagged_adapter();
    adapter.note_body = "---\ntags: *alias\n---\nneedle";
    adapter.missing_note = "two";
    struct folio_state *state = ready_state(&adapter);
    start_body_discovery(state, &adapter);
    require(folio_state_discovery_summary(state).shown == 1 &&
                folio_state_discovery_summary(state).unsupported == 0 &&
                folio_state_discovery_summary(state).unreadable == 1,
            "without tag conditions unsupported metadata remains searchable and read failure is "
            "separate");
    struct note_tags *red = selected_tags(1);
    require(folio_state_set_tag_filter(state, red, TAG_MATCH_ALL) == FOLIO_STATE_READY,
            "set next session's tag restriction");
    start_body_discovery(state, &adapter);
    require(folio_state_discovery_summary(state).shown == 0 &&
                folio_state_discovery_summary(state).unsupported == 1 &&
                folio_state_discovery_summary(state).unreadable == 1,
            "tag conditions exclude unknown metadata without conflating it with unreadable source");
    const char16_t *invalid[] = {u"needle\r", u"\nneedle", u"nee\r\ndle"};
    const size_t lengths[] = {7, 7, 8};
    for (size_t at = 0; at < 3; ++at)
        require(folio_state_query_discovery(state, invalid[at], lengths[at]) ==
                        FOLIO_STATE_DISCOVERY_QUERY_INVALID &&
                    folio_state_discovery_summary(state).shown == 0 &&
                    !folio_state_discovery_work_pending(state),
                "all newline forms reject a query without exposing old results");
    require(folio_state_query_discovery(state, u"", 0) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).phase == DISCOVERY_INPUT &&
                folio_state_discovery_excerpt(state) == nullptr,
            "empty body input recovers from rejection without a zero-results claim");
    note_tags_destroy(red);
    folio_state_destroy(state);
}

static void verify_body_discovery_target(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.note_body = "a\r\n\xf0\x9f\x98\x80\nneedle\rending";
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "target begins as the same named viewing document");
    struct discovery_target *target = nullptr;
    require(folio_state_prepare_discovery_target(state, &target) == FOLIO_STATE_CANCELLED,
            "closed session cannot capture a target");
    start_body_discovery(state, &adapter);
    require(folio_state_prepare_discovery_target(state, &target) == FOLIO_STATE_READY,
            "completed body result copies an immutable target before any departure");
    struct note_search_span span = {.start = 99, .end = 100};
    require(
        folio_state_check_discovery_target(state, target, &span) == DISCOVERY_TARGET_UNAVAILABLE &&
            span.start == 99 && folio_state_begin_edit(state) == FOLIO_STATE_READY &&
            folio_state_check_discovery_target(state, target, &span) == DISCOVERY_TARGET_CURRENT &&
            span.start == 5 && span.end == 11,
        "only editing the exact adopted snapshot exposes RichEdit positions for mixed newlines");
    adapter.note_body = "externally replaced needle";
    folio_state_close_discovery(state);
    require(
        folio_state_check_discovery_target(state, target, &span) == DISCOVERY_TARGET_CURRENT,
        "target outlives the finder and same-document validation never rereads an external file");
    require(folio_state_select_note(state, 0, 1, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_check_discovery_target(state, target, &span) ==
                    DISCOVERY_TARGET_UNAVAILABLE &&
                folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_check_discovery_target(state, target, &span) ==
                    DISCOVERY_TARGET_CHANGED &&
                span.start == 5 && span.end == 11,
            "different identity or changed adopted source never yields stale coordinates");
    discovery_target_destroy(target);
    folio_state_destroy(state);
}

static void verify_body_discovery_saved_revision(void)
{
    struct persistence_adapter adapter = management_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY &&
                folio_state_begin_edit(state) == FOLIO_STATE_READY,
            "saved-revision target starts in a named editor");
    start_body_discovery(state, &adapter);
    struct discovery_target *target = nullptr;
    require(folio_state_prepare_discovery_target(state, &target) == FOLIO_STATE_READY &&
                folio_state_store_note(state, u"replacement", 11) == FOLIO_STATE_READY &&
                folio_state_discovery_summary(state).shown == 0 &&
                folio_state_discovery_excerpt(state) == nullptr,
            "saving invalidates results immediately while the captured target keeps its snapshot");
    size_t reads = adapter.note_reads;
    struct note_search_span span = {0};
    require(
        folio_state_check_discovery_target(state, target, &span) == DISCOVERY_TARGET_CHANGED &&
            finish_discovery_test(state, &adapter) == FOLIO_STATE_READY &&
            adapter.note_reads == reads + 2 && folio_state_discovery_summary(state).shown == 1 &&
            folio_state_discovery_row(state, 0).ref.note == 1 && adapter.note_writes == 1 &&
            adapter.archives == 1,
        "fresh saved revision matches remaining sources without extra writes or stale selection");
    discovery_target_destroy(target);
    folio_state_destroy(state);
}

static void verify_body_discovery_uneditable(void)
{
    struct persistence_adapter adapter = preparation_adapter();
    adapter.note_body = "needle\x07";
    struct folio_state *state = ready_state(&adapter);
    start_body_discovery(state, &adapter);
    struct discovery_target *target = nullptr;
    require(
        folio_state_discovery_summary(state).shown == 2 &&
            folio_state_prepare_discovery_target(state, &target) == FOLIO_STATE_NOTE_UNEDITABLE &&
            target == nullptr && folio_state_discovery_summary(state).phase == DISCOVERY_READY &&
            folio_state_document_kind(state) == FOLIO_DOCUMENT_NONE,
        "uneditable source remains searchable but target refusal changes neither document nor "
        "finder");
    folio_state_destroy(state);
}

static bool body_discovery_allocation(size_t nth)
{
    struct persistence_adapter adapter = tagged_adapter();
    struct folio_state *state = ready_state(&adapter);
    struct discovery_target *target = nullptr;
    allocation_probe_fail_at(nth);
    enum folio_state_outcome result = folio_state_open_discovery(state);
    if (result == FOLIO_STATE_READY)
        result = folio_state_set_discovery_mode(state, DISCOVERY_MODE_BODY);
    if (result == FOLIO_STATE_READY)
        result = folio_state_query_discovery(state, u"needle", 6);
    if (result == FOLIO_STATE_READY)
        result = finish_discovery_test(state, &adapter);
    if (result == FOLIO_STATE_READY)
        result = folio_state_prepare_discovery_target(state, &target);
    size_t calls = allocation_probe_calls();
    allocation_probe_fail_at(0);
    require(result == FOLIO_STATE_READY || result == FOLIO_STATE_OUT_OF_MEMORY,
            "body discovery allocation failures propagate explicitly");
    require(adapter.note_writes == 0 && adapter.archives == 0 && adapter.creates == 0,
            "failed preparation never changes persistence");
    discovery_target_destroy(target);
    start_body_discovery(state, &adapter);
    require(folio_state_discovery_summary(state).shown == 1,
            "the same owner recovers after every body-session allocation failure");
    folio_state_destroy(state);
    return calls < nth;
}

void run_body_discovery_state_tests(void)
{
    finder = test_regex_port();
    verify_body_discovery_modes();
    verify_body_discovery_partial_pass();
    verify_body_discovery_tags();
    verify_body_discovery_unknown_and_input();
    verify_body_discovery_target();
    verify_body_discovery_saved_revision();
    verify_body_discovery_uneditable();
    size_t nth = 1;
    while (nth < 512 && !body_discovery_allocation(nth))
        ++nth;
    require(nth < 512, "body session allocation sweep reaches uninjected success");
    printf("body discovery session: %zu allocation boundaries\n", nth - 1);
}
