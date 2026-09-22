#include "appearance_port.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "folio_settings.h"
#include "folio_state.h"
#include "name_list.h"
#include "note_ledger.h"
#include "note_name.h"
#include "note_rename.h"
#include "note_text.h"
#include "persistence_port.h"
#include "regex_port.h"
#include "unit_tests.h"

#include <stdlib.h>
#include <string.h>

/* テスト用のポート実装。application が不完全型として知る persistence_adapter をここで定義する。 */
struct persistence_adapter
{
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
    size_t note_writes; /* write_note が呼ばれた回数 */
    enum persistence_outcome create_outcome;
    size_t creates;
    char created_note[256];
    char written_body[256];             /* 最後に書かれた本文（終端付き） */
    const char *_Nullable written_note; /* 最後に書かれたノート名 */
    const char *_Nullable written_category;
    char written_names[64]; /* 最後に書かれたカテゴリ台帳の名前（'/' 区切り） */
    enum persistence_outcome ledger_write_outcome;
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
    enum rename_outcome rename_outcome;  /* rename_note が返す結果 */
    enum rename_outcome recover_outcome; /* 起動時の recover_rename が返す結果 */
    size_t renames;                      /* rename_note が呼ばれた回数 */
    size_t recovers;                     /* recover_rename が呼ばれた回数 */
    /* 最後の意図の名前。意図は呼び出しの間だけ借りるので複製して持つ。 */
    char renamed_category[64];
    char renamed_from[64];
    char renamed_to[64];
    bool journal_published;           /* 記録を公開したまま終わっているか（:q! で残る意図の証拠） */
    enum rename_attempt last_attempt; /* 最後の rename_note が START か RESUME か */
    size_t ledger_fail_at; /* この番号（1 始まり）の索引の書き戻しだけ失敗させる。0 なら使わない */
    /* data/settings.json（ADR 0025）。既定は「ファイルが無い」 */
    enum persistence_outcome settings_outcome;
    const char *_Nonnull settings_text;
    size_t settings_reads;
    enum persistence_outcome settings_write_outcome;
    size_t settings_writes;             /* write_settings が呼ばれた回数 */
    bool written_number;                /* 最後に書かれた設定の number */
    const char *_Nullable alt_category; /* この名前のカテゴリだけ別の索引と md を返す */
    const char *_Nonnull alt_notes_text;
    const char *_Nonnull const *_Nullable alt_scanned; /* nullptr で終わる名前の並び */
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
    return list_names(adapter->scanned_categories, adapter->scanned_category_count, out);
}

/* 別の索引を持たせたカテゴリか（別カテゴリへの移動を試すために要る）。 */
static bool is_alt(const struct persistence_adapter *_Nonnull adapter,
                   const char *_Nonnull category)
{
    return adapter->alt_category != nullptr && strcmp(adapter->alt_category, category) == 0;
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
    enum category_ledger_outcome parsed =
        category_ledger_parse(adapter->categories_text, strlen(adapter->categories_text), out);
    if (parsed == CATEGORY_LEDGER_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return parsed == CATEGORY_LEDGER_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY
                                                   : PERSISTENCE_MALFORMED;
}

static enum persistence_outcome fake_read_note_ledger(struct persistence_adapter *_Nonnull adapter,
                                                      const char *_Nonnull category,
                                                      struct note_ledger *_Nullable *_Nonnull out)
{
    if (adapter->notes_outcome != PERSISTENCE_LOADED)
    {
        return adapter->notes_outcome;
    }
    const char *_Nonnull text =
        is_alt(adapter, category) ? adapter->alt_notes_text : adapter->notes_text;
    enum note_ledger_outcome parsed = note_ledger_parse(text, strlen(text), out);
    if (parsed == NOTE_LEDGER_ACCEPTED)
    {
        return PERSISTENCE_LOADED;
    }
    return parsed == NOTE_LEDGER_OUT_OF_MEMORY ? PERSISTENCE_OUT_OF_MEMORY : PERSISTENCE_MALFORMED;
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

static enum persistence_outcome fake_read_note(struct persistence_adapter *_Nonnull adapter,
                                               const char *_Nonnull category,
                                               const char *_Nonnull note,
                                               struct note_text *_Nullable *_Nonnull out)
{
    (void)category;
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
    bool rare = adapter->rare_note != nullptr && adapter->rare_body != nullptr &&
                strcmp(adapter->rare_note, note) == 0;
    const char *_Nonnull body = rare ? adapter->rare_body : adapter->note_body;
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
    return PERSISTENCE_STORED;
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
    require(note_text_length(body) < sizeof adapter->written_body, "created body fits");
    memcpy(adapter->written_body, note_text_bytes(body), note_text_length(body) + 1);
    return PERSISTENCE_STORED;
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
    size_t position = 0;
    adapter->ledger_order[0] = '\0';
    size_t count = note_ledger_count(ledger);
    for (size_t index = 0; index < count; ++index)
    {
        append_name(adapter->ledger_order, sizeof adapter->ledger_order, &position,
                    note_ledger_name(ledger, index));
        record_call(adapter, note_ledger_name(ledger, index));
    }
    return PERSISTENCE_STORED;
}

/* md の移動。指定された結果を返し、呼び出しの順を記録する（rename が先であることの証拠）。 */
static enum persistence_outcome fake_move_note(struct persistence_adapter *_Nonnull adapter,
                                               const char *_Nonnull from_category,
                                               const char *_Nonnull note,
                                               const char *_Nonnull to_category)
{
    adapter->moves += 1;
    adapter->move_from = from_category;
    size_t length = strlen(note);
    require(length + 1 < sizeof adapter->moved_note, "the moved name fits the fake");
    memcpy(adapter->moved_note, note, length + 1);
    adapter->move_to = to_category;
    record_call(adapter, "move");
    return adapter->move_outcome;
}

static void copy_name(char *_Nonnull out, size_t capacity, const char *_Nonnull name)
{
    size_t length = strlen(name);
    require(length + 1 < capacity, "the renamed name fits the fake");
    memcpy(out, name, length + 1);
}

/* 偽の改名。どの段階で止めるかは rename_outcome が決める（ADR 0022）。
 * 記録の公開後に止めた PENDING だけが journal_published を真にし、完了で消える。 */
static enum rename_outcome fake_rename_note(struct persistence_adapter *_Nonnull adapter,
                                            const struct note_rename *_Nonnull plan,
                                            enum rename_attempt attempt)
{
    adapter->renames += 1;
    adapter->last_attempt = attempt;
    record_call(adapter, "rename");
    copy_name(adapter->renamed_category, sizeof adapter->renamed_category,
              note_rename_category(plan));
    copy_name(adapter->renamed_from, sizeof adapter->renamed_from, note_rename_from(plan));
    copy_name(adapter->renamed_to, sizeof adapter->renamed_to, note_rename_to(plan));
    /* 公開後の 2 値だけが記録を残し、完了で消える。公開前の拒否は data/ を変えない。 */
    if (adapter->rename_outcome == RENAME_PENDING || adapter->rename_outcome == RENAME_HALTED)
    {
        adapter->journal_published = true;
    }
    if (adapter->rename_outcome == RENAME_COMPLETED)
    {
        adapter->journal_published = false;
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
};

static enum folio_theme fake_read_theme(struct appearance_adapter *_Nonnull adapter)
{
    return adapter->theme;
}

static struct appearance_adapter dark_adapter = {.theme = FOLIO_THEME_DARK};

static struct appearance_port looks_for(struct appearance_adapter *_Nonnull adapter)
{
    struct appearance_port port = {.adapter = adapter, .read_theme = fake_read_theme};
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
    adapter->written_number = folio_settings_number(settings);
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
        .note_scans = 0,
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

static struct persistence_port port_for(struct persistence_adapter *_Nonnull adapter)
{
    struct persistence_port port = {
        .adapter = adapter,
        .scan_categories = fake_scan_categories,
        .scan_notes = fake_scan_notes,
        .read_category_ledger = fake_read_category_ledger,
        .write_category_ledger = fake_write_category_ledger,
        .read_note = fake_read_note,
        .archive_note = fake_archive_note,
        .write_note = fake_write_note,
        .create_note = fake_create_note,
        .move_note = fake_move_note,
        .rename_note = fake_rename_note,
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select in B");
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
    verify_scroll_recount(state);
    require(folio_state_scroll_drawer(state, scroll_metrics, 5) == FOLIO_STATE_READY,
            "scroll a little");
    require(folio_state_select_note(state, 1, 0) == FOLIO_STATE_READY && scrolled_top(state) == -5,
            "selecting a note leaves the amount alone");
    require(folio_state_move_note(state, at(1, 0), at(1, 2)) == FOLIO_STATE_READY &&
                scrolled_top(state) == -5,
            "reordering leaves the amount alone");
    folio_state_destroy(state);
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
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
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
    require(same_text(adapter.calls, "move/A/four/two/five/B/one/three"),
            "the rename comes first, then the destination ledger, then the source");
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
    require(same_text(adapter.calls, "move/A/four/five/one/B/two/three"),
            "the destination ledger gained the note at its end");
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
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
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
    require(folio_state_select_note(state, 0, 2) == FOLIO_STATE_READY, "select B / three");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY, "two leaves B");
    require(same_text(folio_state_pane_title(state).note, "three"),
            "the note behind the moved one closes up");
    require(folio_state_select_note(state, 1, 2) == FOLIO_STATE_READY, "select A / five");
    require(folio_state_move_note(state, at(0, 0), at(1, 0)) == FOLIO_STATE_READY, "one joins A");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.ordinal == 2 && same_text(title.category, "A") && same_text(title.note, "five"),
            "the notes at and after the insertion move down");
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / three again");
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
    require(adapter.moves == 1 && adapter.ledger_writes == 0 && same_text(adapter.calls, "move"),
            "no ledger is written when the rename fails");
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
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_LEDGER_STALE,
            "the destination ledger could not be written");
    require(adapter.moves == 1 && adapter.ledger_writes == 2 &&
                same_text(adapter.calls, "move/A/B/one/three"),
            "both ledgers were attempted, the destination first");
    require(folio_state_toggle_category(state, 1) == FOLIO_STATE_READY, "expand A");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/four/two/five/C/one/two/three"),
            "the index follows the file even when a ledger is stale");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.ledger_fail_at = 2;
    state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_LEDGER_STALE,
            "the source ledger could not be written either");
    require(same_text(adapter.calls, "move/A/four/two/five/B"),
            "the destination was still written");
    folio_state_destroy(state);
    adapter = healthy_adapter();
    adapter.ledger_write_outcome = PERSISTENCE_OUT_OF_MEMORY;
    state = transfer_state(&adapter);
    require(folio_state_move_note(state, at(0, 1), at(1, 1)) == FOLIO_STATE_OUT_OF_MEMORY,
            "out of memory is not folded into stale");
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
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
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
    require(folio_state_select_note(state, 5, 0) == FOLIO_STATE_NO_SUCH_CATEGORY, "bad category");
    require(folio_state_select_note(state, 0, 3) == FOLIO_STATE_NO_SUCH_NOTE, "bad note");
    adapter.note_outcome = PERSISTENCE_ABSENT;
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_NOTE_UNREADABLE &&
                folio_state_pane_rtf(state) == rtf && folio_state_pane_title(state).note[0] == 't',
            "absent note keeps the pane and the selection");
    adapter.note_outcome = PERSISTENCE_UNREADABLE;
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_NOTE_UNREADABLE, "unreadable note");
    adapter.note_outcome = PERSISTENCE_LOADED;
    adapter.note_body = "\xC3";
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_NOTE_UNREADABLE, "invalid utf8");
    adapter.note_outcome = PERSISTENCE_OUT_OF_MEMORY;
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_OUT_OF_MEMORY, "note oom");
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
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "the first step stops at the first stop row");
    expect_selection(state, "B/one", "nothing selected means the first note");
    expect_cursor(state, "n0.0", "the cursor is the selected note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "two more steps");
    expect_selection(state, "B/three", "the last note of the first category");
    adapter->last_note = nullptr;
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "the next stop is the collapsed category row");
    expect_cursor(state, "c1", "the cursor stops on the collapsed category");
    expect_selection(state, "B/three", "a category row leaves the selection alone");
    require(adapter->last_note == nullptr, "a category row never reads a note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "past the collapsed category");
    expect_selection(state, "C/one", "the next note is in the following category");
    expect_cursor(state, "n2.0", "and the cursor follows it");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY,
            "backwards onto the category row");
    expect_cursor(state, "c1", "the collapsed category is a stop backwards too");
    expect_selection(state, "C/one", "the right pane is still the note it was");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY, "back");
    expect_selection(state, "B/three", "backwards past the category row selects again");
    expect_cursor(state, "n0.2", "and the cursor is that note");
}

/* 端では動かず、READY のまま読み直しもしない。 */
static void verify_step_edges(struct folio_state *_Nonnull state,
                              struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST) == FOLIO_STATE_READY, "gg");
    expect_selection(state, "B/one", "the first stop row");
    adapter->last_note = nullptr;
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY,
            "at the top it stops");
    expect_cursor(state, "n0.0", "the cursor did not move");
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST) == FOLIO_STATE_READY &&
                adapter->last_note == nullptr,
            "the same note is never read again");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY, "G");
    expect_selection(state, "C/three", "the last stop row");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "at the end it stops");
    expect_cursor(state, "n2.2", "the cursor did not move");
}

/* 折り畳んだカテゴリの中に選択があるときは、その位置から次／前の止まる行へ
 * （自分のカテゴリ行へは戻らない）。 */
static void verify_step_from_hidden(struct folio_state *_Nonnull state)
{
    require(folio_state_select_note(state, 1, 1) == FOLIO_STATE_READY, "select A / two");
    expect_cursor(state, "n1.1", "the cursor is the hidden note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "step out of the collapsed category");
    expect_selection(state, "C/one", "forward from a hidden note");
    require(folio_state_select_note(state, 1, 1) == FOLIO_STATE_READY, "select A / two again");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY,
            "and backwards");
    expect_selection(state, "B/three", "backwards from a hidden note");
}

/* 全部畳んでもカテゴリ行が止まる行として残る。右ペインは動かない（ADR 0015 の決定 2）。 */
static void verify_step_all_collapsed(struct folio_state *_Nonnull state,
                                      struct persistence_adapter *_Nonnull adapter)
{
    for (size_t index = 0; index < 3; ++index)
    {
        require(folio_state_set_category_expanded(state, index, false) == FOLIO_STATE_READY,
                "collapse everything");
    }
    adapter->last_note = nullptr;
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST) == FOLIO_STATE_READY, "gg");
    expect_cursor(state, "c0", "the first stop is the first category row");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "two steps down the category rows");
    expect_cursor(state, "c2", "every category row is a stop");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "at the end it stops");
    expect_cursor(state, "c2", "the cursor did not move");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY,
            "backwards");
    expect_cursor(state, "c1", "and back up the category rows");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY, "G");
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
    require(folio_state_select_note(state, 0, 2) == FOLIO_STATE_READY, "select B / three");
    require(folio_state_step_kind(state, FOLIO_STEP_NEXT, &kind, &target) &&
                kind == FOLIO_CURSOR_CATEGORY,
            "the next stop is the collapsed category row");
    expect_cursor(state, "n0.2", "and the cursor is still the note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY, "G");
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
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_NO_SUCH_NOTE &&
                folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_NO_SUCH_NOTE,
            "no stop row at all");
    expect_cursor(state, "-", "the refused step leaves no cursor");
    struct note_ref target = {.category = 0, .note = 0};
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    require(!folio_state_step_kind(state, FOLIO_STEP_FIRST, &kind, &target),
            "and no target to ask about");
    require(folio_state_new_note(state, 0) == FOLIO_STATE_NO_SUCH_CATEGORY &&
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
    require(folio_state_select_note(state, 0, 2) == FOLIO_STATE_READY, "select B / three");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j onto A");
    expect_cursor(state, "c1", "the collapsed empty category is a stop");
    require(folio_state_set_category_expanded(state, 1, true) == FOLIO_STATE_READY &&
                adapter.writes == 1,
            "expanding an empty category writes the ledger");
    expect_cursor(state, "c1", "with no note to move to, the cursor stays on the row");
    expect_selection(state, "B/three", "and the right pane is untouched");
    require(row_count(state) == 9, "an expanded empty category adds no row");
    require(folio_state_set_category_expanded(state, 1, true) == FOLIO_STATE_READY &&
                adapter.writes == 1,
            "expanding it again writes nothing");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j past it");
    expect_selection(state, "C/one", "the next stop is the following category's first note");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY, "k back");
    expect_cursor(state, "c1", "an expanded category without notes is a stop backwards too");
    require(folio_state_set_category_expanded(state, 1, false) == FOLIO_STATE_READY,
            "h collapses it again");
    expect_cursor(state, "c1", "and the cursor stays on the row");
    folio_state_destroy(state);
}

/* 末尾が折り畳んだカテゴリなら G はそこに止まる（決定 2）。 */
static void verify_step_to_collapsed_end(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_set_category_expanded(state, 2, false) == FOLIO_STATE_READY, "collapse C");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY, "G");
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
    require(folio_state_set_category_expanded(state, 0, true) == FOLIO_STATE_READY &&
                adapter.writes == 0,
            "already expanded writes nothing");
    require(folio_state_set_category_expanded(state, 1, false) == FOLIO_STATE_READY &&
                adapter.writes == 0,
            "already collapsed writes nothing");
    require(folio_state_set_category_expanded(state, 0, false) == FOLIO_STATE_READY &&
                adapter.writes == 1 && !adapter.written_expanded[0],
            "collapsing writes the ledger once");
    require(row_count(state) == 6, "B's notes are hidden");
    require(folio_state_set_category_expanded(state, 1, true) == FOLIO_STATE_READY &&
                adapter.writes == 2 && adapter.written_expanded[1],
            "expanding writes it too");
    require(folio_state_set_category_expanded(state, 3, true) == FOLIO_STATE_NO_SUCH_CATEGORY &&
                adapter.writes == 2,
            "out of range is refused without writing");
    adapter.write_outcome = PERSISTENCE_UNWRITABLE;
    require(folio_state_set_category_expanded(state, 0, true) == FOLIO_STATE_STORE_FAILED &&
                row_count(state) == 9,
            "a failed write leaves the ledger alone");
    folio_state_destroy(state);
}

/* h でカーソルがカテゴリ行へ移り、l で中のノートへ戻る（ADR 0015 の決定 3）。 */
static void verify_cursor_collapse(struct folio_state *_Nonnull state,
                                   struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    adapter->last_note = nullptr;
    require(folio_state_set_category_expanded(state, 0, false) == FOLIO_STATE_READY,
            "h collapses B");
    expect_cursor(state, "c0", "the cursor moves onto the collapsed category row");
    expect_selection(state, "B/one", "the selection and the right pane stay");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j");
    expect_cursor(state, "c1", "on to the next collapsed category");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY, "k");
    expect_cursor(state, "c0", "and back to the category we collapsed");
    expect_selection(state, "B/one", "the right pane never changed");
    require(adapter->last_note == nullptr, "and no note was read");
    require(folio_state_set_category_expanded(state, 0, true) == FOLIO_STATE_READY, "l expands B");
    expect_cursor(state, "n0.0", "the selected note inside takes the cursor");
    require(adapter->last_note == nullptr, "the selected note is not read again");
}

/* 選択が中に無ければ、l は最初のノートを選んで右ペインを変える（決定 3）。 */
static void verify_cursor_expand_elsewhere(struct folio_state *_Nonnull state,
                                           struct persistence_adapter *_Nonnull adapter)
{
    require(folio_state_select_note(state, 2, 1) == FOLIO_STATE_READY, "select C / two");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY &&
                folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY,
            "k twice onto the collapsed A");
    expect_cursor(state, "c1", "the cursor is on A");
    adapter->last_note = nullptr;
    require(folio_state_set_category_expanded(state, 1, true) == FOLIO_STATE_READY, "l expands A");
    expect_cursor(state, "n1.0", "the first note of A takes the cursor");
    expect_selection(state, "A/one", "and the right pane follows it");
    require(same_text(adapter->last_note, "one"), "that note was read");
    require(folio_state_set_category_expanded(state, 1, false) == FOLIO_STATE_READY,
            "h collapses A again");
    expect_cursor(state, "c1", "the cursor is back on the category row");
    expect_selection(state, "A/one", "with the selection left inside");
    require(folio_state_set_category_expanded(state, 1, false) == FOLIO_STATE_READY,
            "h on a collapsed category does nothing");
    expect_cursor(state, "c1", "and the cursor stays");
}

/* カーソルが別のカテゴリにあるときは、開閉でカーソルは動かない。 */
static void verify_cursor_elsewhere(struct folio_state *_Nonnull state)
{
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
    require(folio_state_set_category_expanded(state, 2, false) == FOLIO_STATE_READY, "collapse C");
    expect_cursor(state, "n0.1", "collapsing another category leaves the cursor alone");
    require(folio_state_set_category_expanded(state, 2, true) == FOLIO_STATE_READY, "expand C");
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
    require(folio_state_select_note(state, 2, 0) == FOLIO_STATE_READY, "select C / one");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -20,
            "a row hidden below rises until its bottom edge is in view");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -20,
            "a row already in view does not move again");
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -10,
            "a row hidden above sinks until its top edge reaches the band");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY, "G");
    require(scrolled_top(state) == -10, "the step alone does not scroll");
    require(folio_state_reveal_cursor(state, scroll_metrics) == FOLIO_STATE_READY &&
                scrolled_top(state) == -40,
            "the last note lands at the reach");
    /* 行は B 0..10 / A 10..20 / C 20..30 / one 30..40 / two 40..50 / three 50..60・上限 10。 */
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one again");
    require(folio_state_set_category_expanded(state, 0, false) == FOLIO_STATE_READY,
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "begin edit");
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select another note");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "selecting keeps the edit mode");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j");
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
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY, "select B / two");
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
    require(folio_state_store_note(state, u"x", 1) == FOLIO_STATE_READY,
            "view save only synchronizes");
    require(folio_state_end_edit(state, u"x", 1) == FOLIO_STATE_READY,
            "already viewing is idempotent");
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
    [FOLIO_STATE_NOTE_MALFORMED] = "編集中の本文に壊れた文字があります。保存していません。",
    [FOLIO_STATE_NOTE_STORE_FAILED] = "ノートを書き戻せませんでした。編集中の本文はそのままです。",
    [FOLIO_STATE_HISTORY_FAILED] =
        "履歴を書けなかったので保存していません。編集中の本文は残っています。",
    [FOLIO_STATE_UNSAVED_CHANGES] =
        "未保存の変更があります。保存するか、未保存変更を破棄して終了してください。",
    [FOLIO_STATE_NAME_TAKEN] =
        "同じ名前のノートがあります。別の名前を指定してください。既存ファイルは変更していま"
        "せん。",
    [FOLIO_STATE_LEDGER_STALE] =
        "mdは反映しましたが、台帳（index."
        "json）を書き戻せませんでした。保存を再試行するか、次回の起動で揃います。",
    [FOLIO_STATE_LEDGER_UNSYNCED] =
        "前回の台帳（index.json）をまだ書き戻せていません。今回の操作は行っていないので、"
        "保存を再試行してください。",
    [FOLIO_STATE_RENAME_PENDING] =
        "名前の変更が途中で止まっています。同じ名前変更をやり直してください。",
    [FOLIO_STATE_RENAME_UNLOCKED] =
        "data/ に書けないため名前を変更できません。何も変えていません。",
    [FOLIO_STATE_RENAME_UNSUPPORTED] =
        "この data/ ではノート名を変更できません（ローカルの NTFS 以外、またはシンボリック"
        "リンク／junction）。",
    [FOLIO_STATE_RENAME_IDENTITY_FAILED] =
        "元のファイルを確かめられないので名前を変更できません。何も変えていません。",
    [FOLIO_STATE_RENAME_JOURNAL_FAILED] =
        "名前変更の記録（data/.rename.json）を書けませんでした。何も変えていません。",
    [FOLIO_STATE_RENAME_JOURNAL_BROKEN] =
        "名前変更の記録（data/.rename.json）が版 1 の形ではありません。消していません。",
    [FOLIO_STATE_RENAME_HALTED] =
        "名前変更の記録と実ファイルが一致しません。data/.rename.json と data/<カテゴリ>/ "
        "を確認してください。",
    [FOLIO_STATE_SEARCH_MALFORMED] = "検索する語に壊れた文字があります。語は前のままです。",
    [FOLIO_STATE_FILTERED] = "絞り込み中は並び替えと開閉ができません。",
    [FOLIO_STATE_SETTINGS_UNREADABLE] =
        "設定（data/settings.json）を読めません。既定値で始め、直すまで上書きしません。",
    [FOLIO_STATE_SETTINGS_STORE_FAILED] =
        "設定（data/settings.json）を書けませんでした。設定は変えていません。",
    [FOLIO_STATE_PANE_UNAVAILABLE] = "表示中の本文を取り出せませんでした。探していません。",
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
    [FOLIO_STATE_REPLACE_STALE] =
        "本文か入力が変わったので、この置換は当てられません。もう一度入力してください。",
    [FOLIO_STATE_REPLACE_BAD_SPAN] = "選択範囲が正しくありません。本文は変えていません。",
    [FOLIO_STATE_OUT_OF_MEMORY] = "記憶域が足りません。",
    [FOLIO_STATE_NAME_REQUIRED] =
        "無題のノートに名前をつけて保存してください。本文は残っています。",
    [FOLIO_STATE_INVALID_NAME] = "使えない名前です。予約名・末尾の空白やピリオド・区切りを避け、."
                                 "mdを含め255バイト以内で指定してください。",
    [FOLIO_STATE_ALREADY_NAMED] =
        "このノートには名前があります。別名保存（:saveas）または名前変更（:"
        "rename）を使ってください。",
    [FOLIO_STATE_CANCELLED] = "",
};

static void verify_failure_lines(void)
{
    const size_t count = sizeof expected_failure_lines / sizeof expected_failure_lines[0];
    for (size_t index = 0; index < count; ++index)
    {
        require(same_text(folio_state_failure_line((enum folio_state_outcome)index),
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

void test_adapter_destroy(struct persistence_adapter *_Nullable adapter)
{
    free(adapter);
}

/* 3 つのポートを 1 つの束にする（ADR 0029 の決定 3）。folio_state_create は束を借りるだけで
 * 返ったあとは参照しないので、テストの中だけで生きる 1 つの入れ物を使い回す。 */
const struct folio_ports *_Nonnull test_ports(const struct persistence_port *_Nonnull persistence,
                                              const struct appearance_port *_Nonnull appearance,
                                              const struct regex_port *_Nonnull regex)
{
    static struct folio_ports ports;
    ports.persistence = persistence;
    ports.appearance = appearance;
    ports.regex = regex;
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
    require(folio_state_new_note(state, 99) == FOLIO_STATE_NO_SUCH_CATEGORY,
            "no invented category");
    require(folio_state_new_note(state, 1) == FOLIO_STATE_READY, "new in A");
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                folio_state_document_category(state) == 1,
            "untitled edit state");
    require(same_text(folio_state_pane_title(state).note, "無題（未保存）") &&
                same_text(folio_state_category_name(state, 1), "A"),
            "untitled title and category");
    struct note_ref selection = {.category = 0, .note = 0};
    require(!folio_state_selection(state, &selection) && folio_state_note_count(state) == 9,
            "untitled has no fictitious index row");
    require(folio_state_new_note(state, 0) == FOLIO_STATE_NAME_REQUIRED &&
                folio_state_select_note(state, 0, 0) == FOLIO_STATE_NAME_REQUIRED,
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
    require(folio_state_new_note(state, 0) == FOLIO_STATE_READY, "new note");
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
    require(folio_state_new_note(state, 0) == FOLIO_STATE_READY, "untitled");
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
    require(folio_state_new_note(state, 0) == FOLIO_STATE_READY, "untitled");
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
    require(folio_state_new_note(state, 1) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_select_note(state, 0, 0) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_move_category(state, 0, 1) == FOLIO_STATE_LEDGER_UNSYNCED &&
                folio_state_store_note(state, u"draft text", 10) == FOLIO_STATE_LEDGER_UNSYNCED,
            "pending index prevents follow-up operations");
    adapter.ledger_write_outcome = PERSISTENCE_STORED;
    require(folio_state_store_note(state, u"draft text", 10) == FOLIO_STATE_READY &&
                adapter.creates == 1 && adapter.note_writes == 0 && adapter.archives == 0,
            "retry repairs only the index");
    require(folio_state_new_note(state, 1) == FOLIO_STATE_READY, "continue after repair");
    folio_state_destroy(state);
}

static void verify_first_save_keeps_category_cursor(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_category_expanded(state, 2, false) == FOLIO_STATE_READY,
            "last category is collapsed in this scenario");
    require(folio_state_new_note(state, 0) == FOLIO_STATE_READY, "untitled in B");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY,
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
    require(folio_state_set_category_expanded(state, category, true) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select view");
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select view");
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY &&
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
    require(folio_state_new_note(state, 0) == FOLIO_STATE_READY, "untitled draft");
    require(folio_state_rename_note(state, other, u"draft", 5) == FOLIO_STATE_NAME_REQUIRED,
            "an untitled note must be saved first");
    require(adapter.renames == 0 && adapter.creates == 0, "no refusal touched data/");
    note_name_destroy(other);
    folio_state_destroy(state);

    adapter = healthy_adapter();
    state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    struct note_name *same = accepted_note_name("one.md");
    require(folio_state_rename_note(state, same, u"", 0) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to copy");
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
        require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to rename");
        struct note_name *name = accepted_note_name("新しい名前");
        adapter.rename_outcome = refusals[index];
        require(folio_state_rename_note(state, name, u"", 0) == expected[index],
                "each refusal keeps its own reason");
        require(!adapter.journal_published &&
                    same_text(folio_state_pane_title(state).note, "one") &&
                    !folio_state_pane_title(state).recovering,
                "a refusal before the journal leaves no intent");
        require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY &&
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
                folio_state_select_note(state, 0, 1) == expected &&
                folio_state_toggle_category(state, 0) == expected &&
                folio_state_recolor_category(state, 0, color) == expected &&
                folio_state_move_category(state, 0, 1) == expected &&
                folio_state_move_note(state, from, to) == expected &&
                folio_state_new_note(state, 0) == expected &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to rename");
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
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_READY &&
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to rename");
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
    require(strcmp(folio_state_failure_line(FOLIO_STATE_RENAME_HALTED),
                   folio_state_failure_line(FOLIO_STATE_RENAME_PENDING)) != 0,
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to rename");
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to rename");
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY &&
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
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY,
            "a term filters the index");
    require(adapter.note_reads == 8, "every note was read exactly once");
    require(folio_state_filtering(state) && folio_state_index_filter_count(state) == 1,
            "one note matches the body");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "A/four"), "a collapsed category is opened for its match");
    require(folio_state_set_index_filter(state, u"NEEDLE", 6) == FOLIO_STATE_READY &&
                adapter.note_reads == 8,
            "a second term reads nothing again");
    require(folio_state_index_filter_count(state) == 1, "and ASCII case is ignored");
    require(folio_state_set_index_filter(state, u"three", 5) == FOLIO_STATE_READY, "a name term");
    row_order(state, order, sizeof order);
    require(same_text(order, "B/three/C/three"), "the name is matched too");
    require(same_text(folio_state_index_filter_term(state), "three"), "the term is kept as UTF-8");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY &&
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
    require(folio_state_set_index_filter(state, u"Hello", 5) == FOLIO_STATE_READY,
            "every readable body is searched");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one/three/A/one/three/C/one/three"),
            "a note that could not be read never matches");
    const char16_t lone[] = {u'a', 0xD800, u'\0'};
    require(folio_state_set_index_filter(state, lone, 2) == FOLIO_STATE_SEARCH_MALFORMED,
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
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY, "filter on");
    size_t writes = adapter.writes;
    size_t ledger_writes = adapter.ledger_writes;
    require(folio_state_toggle_category(state, 0) == FOLIO_STATE_FILTERED, "no toggle");
    require(folio_state_set_category_expanded(state, 0, false) == FOLIO_STATE_FILTERED, "no h/l");
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "a note can still be read");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "and edited");
    require(same_text(folio_state_failure_line(FOLIO_STATE_FILTERED),
                      "絞り込み中は並び替えと開閉ができません。"),
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "and edit it");
    require(folio_state_scroll_drawer(state, scroll_metrics, 20) == FOLIO_STATE_READY, "scroll");
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    expect_selection(state, "B/one", "the current note is not closed when it stops matching");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT, "and the mode stays");
    require(!any_selected_row(state), "no row carries the selection while its row is gone");
    require(scrolled_top(state) == 0, "the amount is reset on every change of the term");
    expect_cursor(state, "c0", "the cursor moves to the first visible row");
    /* 止まる行は B/three・A/three・C/three だけになる。 */
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j");
    expect_cursor(state, "n0.2", "the first stop is the match inside the first category");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j again");
    expect_cursor(state, "n1.2", "the next stop skips the notes that do not match");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY, "j again");
    expect_cursor(state, "n2.2", "and lands in the last category");
    require(folio_state_select_adjacent(state, FOLIO_STEP_NEXT) == FOLIO_STATE_READY,
            "j at the end");
    expect_cursor(state, "n2.2", "the end does not move");
    require(folio_state_select_adjacent(state, FOLIO_STEP_PREVIOUS) == FOLIO_STATE_READY, "k");
    expect_cursor(state, "n1.2", "backwards walks the same visible rows");
    require(folio_state_select_adjacent(state, FOLIO_STEP_FIRST) == FOLIO_STATE_READY, "gg");
    expect_cursor(state, "n0.2", "gg is the first visible row");
    require(folio_state_select_adjacent(state, FOLIO_STEP_LAST) == FOLIO_STATE_READY, "G");
    expect_cursor(state, "n2.2", "G is the last visible row");
    folio_state_destroy(state);
}

/* 保存と改名は写しと一致集合を更新し、絞り込みを保つ。語はノート内検索とは別（決定 4 / 7）。 */
static void verify_filter_follows_content(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_begin_edit(state) == FOLIO_STATE_READY, "edit");
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    require(folio_state_index_filter_count(state) == 0, "nothing matches yet");
    require(folio_state_store_note(state, u"a needle appears", 16) == FOLIO_STATE_READY, "save");
    require(folio_state_index_filter_count(state) == 1, "the saved body now matches");
    char order[128] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/one"), "and it is the note that was saved");
    require(folio_state_set_index_filter(state, u"renamed", 7) == FOLIO_STATE_READY, "a new term");
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
    require(folio_state_set_index_filter(state, u"one", 3) == FOLIO_STATE_READY, "a filter term");
    require(same_text(folio_state_search_term(state), "zzz"),
            "and the filter does not touch the in-note term");
    folio_state_destroy(state);
}

/* 新しく公開したノートも写しと一致集合に入る（決定 4）。 */
static void verify_filter_follows_new_note(void)
{
    struct persistence_adapter adapter = healthy_adapter();
    struct folio_state *state = ready_state(&adapter);
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    require(folio_state_index_filter_count(state) == 0, "nothing matches yet");
    require(folio_state_new_note(state, 0) == FOLIO_STATE_READY, "a new untitled note");
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
    require(folio_state_select_note(state, 1, 0) == FOLIO_STATE_READY, "select A / one");
    expect_cursor(state, "n1.0", "the cursor follows the selection into the collapsed category");
    require(folio_state_set_index_filter(state, u"Hello", 5) == FOLIO_STATE_READY,
            "a term that every body holds");
    expect_cursor(state, "n1.0", "filtering opens A, so the cursor keeps its own row");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY, "clear the term");
    expect_cursor(state, "c1", "A closes again and the cursor stops on A's own row");
    expect_selection(state, "A/one", "the selection and the right pane never moved");
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_set_index_filter(state, u"four", 4) == FOLIO_STATE_READY,
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select B / one");
    require(folio_state_set_index_filter(state, u"renamed", 7) == FOLIO_STATE_READY, "filter");
    require(folio_state_index_filter_count(state) == 0, "no name and no body holds the term");
    struct note_name *name = accepted_note_name("renamed");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "the rename stops after publishing its journal");
    note_name_destroy(name);
    require(folio_state_index_filter_count(state) == 0,
            "an unfinished rename changes neither the copy nor the set");
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_select_note(state, 0, 1) == FOLIO_STATE_READY,
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
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY, "filter");
    require(adapter.note_reads == 8, "every note is read once at the first term");
    char order[160] = {0};
    row_order(state, order, sizeof order);
    require(same_text(order, "B/two/C/two"), "the two bodies that hold the term");
    require(folio_state_set_index_filter(state, u"", 0) == FOLIO_STATE_READY, "clear the term");
    require(folio_state_move_note(state, at(0, 1), at(1, 0)) == FOLIO_STATE_READY,
            "B / two moves into A while nothing is filtered");
    require(folio_state_set_index_filter(state, u"needle", 6) == FOLIO_STATE_READY, "filter again");
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
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "select to rename");
    struct note_name *name = accepted_note_name("新しい名前");
    adapter.rename_outcome = RENAME_PENDING;
    require(folio_state_rename_note(state, name, u"", 0) == FOLIO_STATE_RENAME_PENDING,
            "the rename stops after publishing its journal");
    note_name_destroy(name);
    size_t renames = adapter.renames;
    adapter.rename_outcome = RENAME_COMPLETED;
    require(folio_state_set_number(state, true) == FOLIO_STATE_READY,
            "the settings change is accepted while a rename is unfinished");
    require(adapter.renames == renames, "it never calls rename_note, not even to resume");
    require(adapter.settings_writes == 1 && adapter.written_number,
            "and the settings are written exactly once");
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
    require(adapter.settings_writes == 0, "a file nobody could read is never overwritten");
    require(!folio_state_number(state), "the refused change left the value alone");
    /* ノートの閲覧・編集・保存は妨げない。 */
    require(folio_state_select_note(state, 0, 0) == FOLIO_STATE_READY, "viewing still works");
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
    adapter.settings_text = "{\"version\": 2, \"number\": true}";
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

static void verify_settings(void)
{
    verify_settings_absent();
    verify_settings_loaded();
    verify_set_number();
    verify_set_number_unwritable();
    verify_set_number_ignores_resumed_rename();
    verify_settings_unreadable(PERSISTENCE_MALFORMED, "malformed settings still start");
    verify_settings_unreadable(PERSISTENCE_UNREADABLE, "unreadable settings still start");
    verify_settings_shapes();
}

/* 改名・検索・絞り込み・設定の単位（run_state_tests を C-012 の 60 行に収めるための束ね）。 */
static void verify_later_units(void)
{
    verify_rename_refusals();
    verify_rename_saves_first();
    verify_rename_blocked_by_save();
    verify_rename_refused_before_journal();
    verify_rename_pending();
    verify_rename_halted();
    verify_rename_force_quit_keeps_intent();
    verify_rename_recovery();
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
}

void run_state_tests(void)
{
    finder = test_regex_port();
    verify_ready_state();
    verify_absent_data();
    verify_failures();
    verify_toggle();
    verify_recolor();
    verify_scroll();
    verify_move_category();
    verify_move_note();
    verify_move_selection();
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
    verify_edit_guards();
    verify_edit_unchanged();
    verify_note_changed();
    verify_history_order();
    verify_history_failures();
    verify_edit_saves();
    verify_edit_failures();
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
    verify_later_units();
}
