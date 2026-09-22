#include "folio_state.h"

#include "appearance_port.h"
#include "category_ledger.h"
#include "drawer_layout.h"
#include "folio_settings.h"
#include "index_filter.h"
#include "markdown_rtf.h"
#include "name_list.h"
#include "note_corpus.h"
#include "note_ledger.h"
#include "note_name.h"
#include "note_rename.h"
#include "note_replace.h"
#include "note_text.h"
#include "persistence_port.h"
#include "regex_matches.h"
#include "regex_port.h"
#include "regex_request.h"
#include "replace_edit.h"
#include "replace_preview.h"
#include "rtf_palette.h"
#include "utf8_text.h"

#include <stdlib.h>
#include <string.h>

/* 走査の入れ物の最初の大きさ。足りなければ総数まで広げて 1 度だけ走査し直す（ADR 0028 の決定 2）。
 */
constexpr size_t initial_matches = 64;

struct folio_state
{
    struct persistence_port port;
    /* 正規表現ポート（ADR 0028 の決定 1 / 2）。ICU を知るのは adapter だけである */
    struct regex_port regex;
    struct regex_match *_Nonnull found; /* 走査の入れ物。下見へ写したら使い回す */
    size_t found_capacity;
    struct replace_preview *_Nullable preview; /* 直前の下見。1 つだけ持つ（決定 6） */
    size_t replace_offset; /* 直前の下見が BAD_PATTERN だったときの位置（1 起算） */
    enum folio_theme theme;
    /* data/settings.json の値の所有者（ADR 0025 の決定 5）。読めなければ既定値のまま */
    struct folio_settings *_Nullable settings;
    /* READY か SETTINGS_UNREADABLE。後者のあいだ設定の変更を断り、上書きもしない（決定 6） */
    enum folio_state_outcome settings_notice;
    struct rtf_palette palette;
    struct category_ledger *_Nullable categories;
    struct note_ledger *_Nonnull *_Nullable notes; /* categories と同じ数・同じ順 */
    size_t notes_count;
    struct markdown_rtf *_Nullable pane; /* 選択中のノートの表示値。無ければ空の文書 */
    struct note_text *_Nullable body;    /* 最後に読んだ本文。何も選んでいなければ空 */
    enum pane_mode mode;
    int scroll; /* ドロワーのスクロール量（要求量・画素・0 以上）。上限は core が決める */
    enum folio_document_kind document;
    size_t selected_category;
    size_t selected_note;
    bool index_pending; /* 初回作成後に同期できなかった台帳。次の保存・変更前に再試行 */
    size_t pending_category;
    /* 記録を公開したまま完了していない改名の意図。1 つだけ持つ（ADR 0022 の決定 2 / 7） */
    struct note_rename *_Nullable rename;
    size_t rename_category;
    /* ノート内検索の語（UTF-8）と直前の方向。絞り込みの語とは別（ADR 0023 の決定 3） */
    struct utf8_text *_Nullable search_term;
    enum search_direction search_direction;
    /* 全ノートの絞り込み（ADR 0024）。語と一致集合の所有者で、どちらも永続化しない */
    struct utf8_text *_Nullable index_term;
    struct index_filter *_Nullable filter;
    struct note_corpus *_Nullable corpus; /* 本文の写し。最初の絞り込みで 1 回だけ読む */
    bool corpus_loaded;
    bool cursor_any; /* 索引のカーソルがあるか（ADR 0015 の決定 1） */
    enum folio_cursor_kind cursor_kind;
    size_t cursor_category; /* FOLIO_CURSOR_CATEGORY のときのカテゴリ番号 */
};

/* 絞り込みの作り直しとカーソルの着地は、写しを付け替える経路（改名）より後に書いてあるので
 * ここで名前だけ先に出す。定義は 1 つずつで、経路は増やさない（ARC-001）。 */
static enum folio_state_outcome refresh_filter(struct folio_state *_Nonnull state);
static bool holds_category(const struct folio_state *_Nonnull state, size_t category);
static struct drawer_cursor first_stop_in(const struct folio_state *_Nonnull state,
                                          size_t category);

static enum folio_state_outcome translate(enum persistence_outcome outcome)
{
    switch (outcome)
    {
    case PERSISTENCE_LOADED:
    case PERSISTENCE_ABSENT:
        return FOLIO_STATE_READY;
    case PERSISTENCE_UNREADABLE:
        return FOLIO_STATE_DATA_UNREADABLE;
    case PERSISTENCE_MALFORMED:
        return FOLIO_STATE_LEDGER_MALFORMED;
    case PERSISTENCE_STORED:
        return FOLIO_STATE_READY;
    case PERSISTENCE_UNWRITABLE:
        return FOLIO_STATE_STORE_FAILED;
    case PERSISTENCE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case PERSISTENCE_NAME_TAKEN:
        return FOLIO_STATE_NAME_TAKEN;
    }
    return FOLIO_STATE_DATA_UNREADABLE;
}

/* 前回書けなかった index.json を、次の意図より先に修復する。
 * 失敗は LEDGER_UNSYNCED。呼び出し側はまだ何も実行していないので、意図ごと拒む。 */
static enum folio_state_outcome synchronize_index(struct folio_state *_Nonnull state)
{
    if (!state->index_pending)
    {
        return FOLIO_STATE_READY;
    }
    enum persistence_outcome stored = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, state->pending_category),
        state->notes[state->pending_category]);
    if (stored != PERSISTENCE_STORED)
    {
        return FOLIO_STATE_LEDGER_UNSYNCED;
    }
    state->index_pending = false;
    return FOLIO_STATE_READY;
}

/* md を公開した直後の同期の結果。ここの失敗は「何もしていない」修復失敗ではなく LEDGER_STALE。 */
static enum folio_state_outcome published_index(enum folio_state_outcome synced)
{
    return synced == FOLIO_STATE_LEDGER_UNSYNCED ? FOLIO_STATE_LEDGER_STALE : synced;
}

static enum folio_state_outcome from_rename(enum rename_outcome outcome)
{
    switch (outcome)
    {
    case RENAME_COMPLETED:
    case RENAME_NONE:
        return FOLIO_STATE_READY;
    case RENAME_PENDING:
        return FOLIO_STATE_RENAME_PENDING;
    case RENAME_HALTED:
        return FOLIO_STATE_RENAME_HALTED;
    case RENAME_UNLOCKED:
        return FOLIO_STATE_RENAME_UNLOCKED;
    case RENAME_NAME_TAKEN:
        return FOLIO_STATE_NAME_TAKEN;
    case RENAME_UNSUPPORTED:
        return FOLIO_STATE_RENAME_UNSUPPORTED;
    case RENAME_IDENTITY_FAILED:
        return FOLIO_STATE_RENAME_IDENTITY_FAILED;
    case RENAME_JOURNAL_FAILED:
        return FOLIO_STATE_RENAME_JOURNAL_FAILED;
    case RENAME_JOURNAL_BROKEN:
        return FOLIO_STATE_RENAME_JOURNAL_BROKEN;
    case RENAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_RENAME_PENDING;
}

/* 完了した意図から準備済みの台帳を受け取り、索引を新しい名前へ揃える（ADR 0022 の決定 6）。
 * 位置は変わらないので、選択中の番号もカーソルもそのままでよい。 */
/* 台帳を差し替える前に、本文の写しを新しい名前へ付け替える（ADR 0024 の決定 1）。
 * 写しが無ければ何も起きない。確保に失敗したら写しを次の絞り込みで読み直させ、記憶不足として
 * 報せる（md も履歴も台帳も新しい名前で揃っている）。 */
static enum folio_state_outcome recopy_rename(struct folio_state *_Nonnull state)
{
    struct note_rename *_Nonnull intent = state->rename;
    if (!state->corpus_loaded)
    {
        return FOLIO_STATE_READY;
    }
    if (note_corpus_rename(
            state->corpus, category_ledger_name(state->categories, state->rename_category),
            note_rename_from(intent), note_rename_to(intent)) != NOTE_CORPUS_ACCEPTED)
    {
        state->corpus_loaded = false;
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_READY;
}

/* 名前も判定の対象なので、写しを付け替えたら一致集合も作り直す。どの意図が改名を完了させても
 * （`rename_note` でも `resume_rename` でも）同じ結果になるよう、ここ 1 か所で行う
 * （ADR 0024 の補正 3）。絞り込んでいなければ `refresh_filter` は何もしない。 */
static enum folio_state_outcome adopt_rename(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome copied = recopy_rename(state);
    struct note_rename *_Nonnull intent = state->rename;
    struct note_ledger *_Nonnull renamed = note_rename_take_ledger(intent);
    note_ledger_destroy(state->notes[state->rename_category]);
    state->notes[state->rename_category] = renamed;
    state->rename = nullptr;
    note_rename_destroy(intent);
    return copied != FOLIO_STATE_READY ? copied : refresh_filter(state);
}

/* 未完了の改名を、次の意図より先に同じ意図で再開する（ADR 0022 の決定 7）。 */
static enum folio_state_outcome resume_rename(struct folio_state *_Nonnull state)
{
    if (state->rename == nullptr)
    {
        return FOLIO_STATE_READY;
    }
    /* 保持している意図の続きなので RESUME。記録が無ければ adapter が推測せずに止める（決定 5）。 */
    enum rename_outcome moved =
        state->port.rename_note(state->port.adapter, state->rename, RENAME_RESUME);
    if (moved != RENAME_COMPLETED)
    {
        /* どの理由でも意図は捨てない。捨てられるのは完了したときだけ。 */
        return from_rename(moved);
    }
    return adopt_rename(state);
}

/* 保存・切替・並替・色・新規・別名保存・終了確認が共有する唯一の同期の入口。
 * 前回書けなかった index.json と未完了の改名を、この順に片付けてから意図へ進む。 */
static enum folio_state_outcome synchronize(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome synced = synchronize_index(state);
    return synced == FOLIO_STATE_READY ? resume_rename(state) : synced;
}

static enum folio_state_outcome from_category_ledger(enum category_ledger_outcome outcome)
{
    switch (outcome)
    {
    case CATEGORY_LEDGER_ACCEPTED:
        return FOLIO_STATE_READY;
    case CATEGORY_LEDGER_MALFORMED:
    case CATEGORY_LEDGER_UNSUPPORTED_VERSION:
        return FOLIO_STATE_LEDGER_MALFORMED;
    case CATEGORY_LEDGER_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_LEDGER_MALFORMED;
}

static enum folio_state_outcome from_note_ledger(enum note_ledger_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_LEDGER_ACCEPTED:
        return FOLIO_STATE_READY;
    case NOTE_LEDGER_MALFORMED:
    case NOTE_LEDGER_UNSUPPORTED_VERSION:
        return FOLIO_STATE_LEDGER_MALFORMED;
    case NOTE_LEDGER_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_LEDGER_MALFORMED;
}

/* 台帳の書き戻しの結果を意図の結果に写す。記憶不足だけは書けなかったことと区別する。 */
static enum folio_state_outcome from_store(enum persistence_outcome stored)
{
    return stored == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                               : FOLIO_STATE_STORE_FAILED;
}

/* 台帳（無ければ空）と走査結果（無ければ空）を照合して categories を確定する。 */
static enum folio_state_outcome load_categories(struct folio_state *_Nonnull state,
                                                const struct persistence_port *_Nonnull port)
{
    struct category_ledger *_Nullable stored = nullptr;
    enum persistence_outcome read = port->read_category_ledger(port->adapter, &stored);
    if (read == PERSISTENCE_ABSENT)
    {
        enum folio_state_outcome outcome = from_category_ledger(category_ledger_empty(&stored));
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
    }
    else if (read != PERSISTENCE_LOADED)
    {
        return translate(read);
    }
    struct name_list *_Nullable scanned = nullptr;
    enum persistence_outcome scan = port->scan_categories(port->adapter, &scanned);
    enum folio_state_outcome outcome = translate(scan);
    if (scan == PERSISTENCE_ABSENT && name_list_create(&scanned) != NAME_LIST_ACCEPTED)
    {
        outcome = FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome =
            from_category_ledger(category_ledger_reconcile(stored, scanned, &state->categories));
    }
    name_list_destroy(scanned);
    category_ledger_destroy(stored);
    return outcome;
}

/* 1 カテゴリの index.json（無ければ空）と md の走査結果を照合する。 */
static enum folio_state_outcome load_notes(struct note_ledger *_Nullable *_Nonnull slot,
                                           const struct persistence_port *_Nonnull port,
                                           const char *_Nonnull category)
{
    struct note_ledger *_Nullable stored = nullptr;
    enum persistence_outcome read = port->read_note_ledger(port->adapter, category, &stored);
    if (read == PERSISTENCE_ABSENT)
    {
        enum folio_state_outcome outcome = from_note_ledger(note_ledger_empty(&stored));
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
    }
    else if (read != PERSISTENCE_LOADED)
    {
        return translate(read);
    }
    struct name_list *_Nullable scanned = nullptr;
    enum persistence_outcome scan = port->scan_notes(port->adapter, category, &scanned);
    enum folio_state_outcome outcome = translate(scan);
    if (scan == PERSISTENCE_ABSENT && name_list_create(&scanned) != NAME_LIST_ACCEPTED)
    {
        outcome = FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = from_note_ledger(note_ledger_reconcile(stored, scanned, slot));
    }
    name_list_destroy(scanned);
    note_ledger_destroy(stored);
    return outcome;
}

static enum folio_state_outcome load_all_notes(struct folio_state *_Nonnull state,
                                               const struct persistence_port *_Nonnull port)
{
    size_t count = category_ledger_count(state->categories);
    state->notes = calloc(count + 1, sizeof *state->notes);
    if (state->notes == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index < count; ++index)
    {
        struct note_ledger *_Nullable notes = nullptr;
        enum folio_state_outcome outcome =
            load_notes(&notes, port, category_ledger_name(state->categories, index));
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
        state->notes[index] = notes;
        state->notes_count += 1;
    }
    return FOLIO_STATE_READY;
}

/* 無ければ既定値のまま、壊れていれば既定値＋知らせ、記憶不足だけが起動の失敗（決定 4 / 6）。
 * 読みでは返らない書き込み側の値も「読めない」に畳んで、閉じた列挙のまま扱う。 */
static enum folio_state_outcome adopt_settings(struct folio_state *_Nonnull state,
                                               enum persistence_outcome read,
                                               struct folio_settings *_Nullable loaded)
{
    switch (read)
    {
    case PERSISTENCE_LOADED:
        /* LOADED なのに出力を触らなかったポートは契約違反である。既定値を捨てて null を
         * 採用するより、確保失敗と同じ起動の失敗に畳む（ADR 0026 の補正 10）。 */
        if (loaded == nullptr)
        {
            return FOLIO_STATE_OUT_OF_MEMORY;
        }
        folio_settings_destroy(state->settings);
        state->settings = loaded;
        return FOLIO_STATE_READY;
    case PERSISTENCE_ABSENT:
        return FOLIO_STATE_READY;
    case PERSISTENCE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case PERSISTENCE_UNREADABLE:
    case PERSISTENCE_MALFORMED:
    case PERSISTENCE_STORED:
    case PERSISTENCE_UNWRITABLE:
    case PERSISTENCE_NAME_TAKEN:
        state->settings_notice = FOLIO_STATE_SETTINGS_UNREADABLE;
        return FOLIO_STATE_READY;
    }
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome load_settings(struct folio_state *_Nonnull state,
                                              const struct persistence_port *_Nonnull port)
{
    struct folio_settings *_Nullable loaded = nullptr;
    /* 読みを先に済ませてから渡す（引数の評価順は決まっていない）。 */
    enum persistence_outcome read = port->read_settings(port->adapter, &loaded);
    return adopt_settings(state, read, loaded);
}

enum folio_state_outcome folio_state_create(const struct persistence_port *_Nonnull persistence,
                                            const struct appearance_port *_Nonnull appearance,
                                            const struct regex_port *_Nonnull regex,
                                            struct folio_state *_Nullable *_Nonnull out)
{
    struct folio_state *_Nullable state = calloc(1, sizeof *state);
    if (state == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    state->port = *persistence;
    state->regex = *regex;
    state->theme = appearance->read_theme(appearance->adapter);
    state->palette = rtf_palette_for(state->theme);
    state->found = calloc(initial_matches, sizeof *state->found);
    state->found_capacity = initial_matches;
    if (state->found == nullptr ||
        markdown_rtf_empty(state->palette, &state->pane) != MARKDOWN_RTF_CONVERTED ||
        note_text_create("", 0, &state->body) != NOTE_TEXT_ACCEPTED ||
        note_corpus_create(&state->corpus) != NOTE_CORPUS_ACCEPTED ||
        folio_settings_default(&state->settings) != FOLIO_SETTINGS_READY)
    {
        folio_state_destroy(state);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    /* カテゴリ・ノートの走査より先に、前回の改名を終わらせる（ADR 0022 の決定 6）。 */
    enum folio_state_outcome outcome =
        from_rename(persistence->recover_rename(persistence->adapter));
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = load_categories(state, persistence);
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = load_all_notes(state, persistence);
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = load_settings(state, persistence);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        folio_state_destroy(state);
        return outcome;
    }
    *out = state;
    return FOLIO_STATE_READY;
}

static struct note_ref located(size_t category, size_t note)
{
    struct note_ref ref = {.category = category, .note = note};
    return ref;
}

static struct drawer_cursor on_note(size_t category, size_t note)
{
    struct drawer_cursor cursor = {.kind = DRAWER_ROW_NOTE, .ref = located(category, note)};
    return cursor;
}

static struct drawer_cursor on_category(size_t category)
{
    struct drawer_cursor cursor = {.kind = DRAWER_ROW_CATEGORY, .ref = located(category, 0)};
    return cursor;
}

/* いまのカーソル。無ければ false。ノートのカーソルは選択そのもの（ADR 0015 の決定 1）。 */
static bool current_cursor(const struct folio_state *_Nonnull state,
                           struct drawer_cursor *_Nonnull out)
{
    if (!state->cursor_any)
    {
        return false;
    }
    switch (state->cursor_kind)
    {
    case FOLIO_CURSOR_NOTE:
        *out = on_note(state->selected_category, state->selected_note);
        return true;
    case FOLIO_CURSOR_CATEGORY:
        *out = on_category(state->cursor_category);
        return true;
    }
    return false;
}

/* カーソルを選択中のノートへ置く（選択が動いたとき）。 */
static void cursor_to_selection(struct folio_state *_Nonnull state)
{
    state->cursor_any = true;
    state->cursor_kind = FOLIO_CURSOR_NOTE;
}

/* カーソルをカテゴリ行へ置く。選択と右ペインは触らない（決定 2 / 3）。 */
static void cursor_to_category(struct folio_state *_Nonnull state, size_t category)
{
    state->cursor_any = true;
    state->cursor_kind = FOLIO_CURSOR_CATEGORY;
    state->cursor_category = category;
}

static enum folio_cursor_kind kind_of(struct drawer_cursor cursor)
{
    switch (cursor.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return FOLIO_CURSOR_CATEGORY;
    case DRAWER_ROW_NOTE:
        return FOLIO_CURSOR_NOTE;
    }
    return FOLIO_CURSOR_NOTE;
}

/* 絞り込んでいるあいだは、一致するノートを持つカテゴリだけが行になる（ADR 0024 の決定 3）。 */
static bool shown_category(const struct folio_state *_Nonnull state, size_t category)
{
    return state->filter == nullptr || index_filter_category(state->filter, category);
}

/* 絞り込んでいるあいだは、一致したノートだけが行になる。 */
static bool shown_note(const struct folio_state *_Nonnull state, size_t category, size_t note)
{
    return state->filter == nullptr || index_filter_note(state->filter, located(category, note));
}

/* 絞り込んでいるあいだは台帳の expanded に関わらず展開して見える。 */
static bool shown_expanded(const struct folio_state *_Nonnull state, size_t category)
{
    return state->filter != nullptr || category_ledger_expanded(state->categories, category);
}

/* 台帳の順で最初の見えるカテゴリ。1 つも無ければ false。 */
static bool first_shown_category(const struct folio_state *_Nonnull state, size_t *_Nonnull out)
{
    size_t count = category_ledger_count(state->categories);
    for (size_t category = 0; category < count; ++category)
    {
        if (shown_category(state, category))
        {
            *out = category;
            return true;
        }
    }
    return false;
}

static size_t total_notes(const struct folio_state *_Nonnull state)
{
    size_t total = 0;
    for (size_t index = 0; index < state->notes_count; ++index)
    {
        total += note_ledger_count(state->notes[index]);
    }
    return total;
}

static enum folio_state_outcome from_index_filter(enum index_filter_outcome outcome)
{
    switch (outcome)
    {
    case INDEX_FILTER_ACCEPTED:
        return FOLIO_STATE_READY;
    case INDEX_FILTER_NO_TERM:
    case INDEX_FILTER_MALFORMED:
        return FOLIO_STATE_SEARCH_MALFORMED;
    case INDEX_FILTER_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_SEARCH_MALFORMED;
}

/* 索引の全ノートを台帳の順に写す。写しが無いノート（読めなかった・まだ読んでいない）は
 * 本文を nullptr で渡し、core が一致しない扱いにする（ADR 0024 の決定 1）。 */
static size_t collect_entries(const struct folio_state *_Nonnull state,
                              struct index_filter_entry *_Nonnull entries)
{
    size_t filled = 0;
    for (size_t category = 0; category < state->notes_count; ++category)
    {
        const char *_Nonnull folder = category_ledger_name(state->categories, category);
        size_t notes = note_ledger_count(state->notes[category]);
        for (size_t note = 0; note < notes; ++note)
        {
            const char *_Nonnull name = note_ledger_name(state->notes[category], note);
            const struct note_text *_Nullable body = note_corpus_body(state->corpus, folder, name);
            struct index_filter_entry entry = {
                .ref = located(category, note),
                .name = name,
                .body = body == nullptr ? nullptr : note_text_bytes(body),
                .length = body == nullptr ? 0 : note_text_length(body)};
            entries[filled] = entry;
            filled += 1;
        }
    }
    return filled;
}

/* 語と索引の全ノートから一致集合を作る。列は呼び出しの間だけ持つ（集合は番号しか持たない）。 */
static enum folio_state_outcome build_filter(const struct folio_state *_Nonnull state,
                                             const struct utf8_text *_Nonnull term,
                                             struct index_filter *_Nullable *_Nonnull out)
{
    struct index_filter_entry *_Nullable entries =
        malloc((total_notes(state) + 1) * sizeof *entries);
    if (entries == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct index_filter_query query = {.term = utf8_text_bytes(term),
                                       .term_length = utf8_text_length(term),
                                       .entries = entries,
                                       .count = collect_entries(state, entries)};
    enum index_filter_outcome built = index_filter_create(&query, out);
    free(entries);
    return from_index_filter(built);
}

/* 空でない語が初めて来たときだけ、1 カテゴリぶんの本文を読んで写しにする（決定 1）。
 * 読めないノートは写しを持たず、絞り込みで一致しない（理由は出さない）。 */
static enum folio_state_outcome load_category_corpus(struct folio_state *_Nonnull state,
                                                     size_t category)
{
    const char *_Nonnull folder = category_ledger_name(state->categories, category);
    size_t notes = note_ledger_count(state->notes[category]);
    for (size_t note = 0; note < notes; ++note)
    {
        const char *_Nonnull name = note_ledger_name(state->notes[category], note);
        struct note_text *_Nullable body = nullptr;
        enum persistence_outcome read =
            state->port.read_note(state->port.adapter, folder, name, &body);
        if (read == PERSISTENCE_OUT_OF_MEMORY)
        {
            /* 記憶不足は「読めないノート」ではない。写しを欠いたまま絞り込まない。 */
            return FOLIO_STATE_OUT_OF_MEMORY;
        }
        if (read != PERSISTENCE_LOADED)
        {
            continue;
        }
        enum note_corpus_outcome stored = note_corpus_put(state->corpus, folder, name, body);
        note_text_destroy(body);
        if (stored != NOTE_CORPUS_ACCEPTED)
        {
            return FOLIO_STATE_OUT_OF_MEMORY;
        }
    }
    return FOLIO_STATE_READY;
}

/* 本文の写しを 1 回だけ載せる。起動時には読まない（決定 1）。 */
static enum folio_state_outcome load_corpus(struct folio_state *_Nonnull state)
{
    if (state->corpus_loaded)
    {
        return FOLIO_STATE_READY;
    }
    for (size_t category = 0; category < state->notes_count; ++category)
    {
        enum folio_state_outcome read = load_category_corpus(state, category);
        if (read != FOLIO_STATE_READY)
        {
            return read;
        }
    }
    state->corpus_loaded = true;
    return FOLIO_STATE_READY;
}

/* カーソルの行がいまの配置にあるか。 */
static bool cursor_row_present(const struct folio_state *_Nonnull state,
                               struct drawer_cursor cursor)
{
    size_t category = cursor.ref.category;
    if (category >= state->notes_count || !shown_category(state, category))
    {
        return false;
    }
    switch (cursor.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return true;
    case DRAWER_ROW_NOTE:
        return shown_expanded(state, category) &&
               cursor.ref.note < note_ledger_count(state->notes[category]) &&
               shown_note(state, category, cursor.ref.note);
    }
    return false;
}

/* カーソルの行が消えたときの移し先（決定 5・ADR 0024 の補正 4）。
 * その行のカテゴリ行が「見えていてカーソルが止まれる行」（＝見えるノート行を持たない・
 * ADR 0015 の決定 2）ならそこへ、そうでなければ最初に見える行（＝最初に見えるカテゴリ行）へ移す。
 * 選択・右ペイン・モードは変えない。見える行が 1 つも無ければカーソルを持たない。 */
static void settle_cursor(struct folio_state *_Nonnull state)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor) || cursor_row_present(state, cursor))
    {
        return;
    }
    if (holds_category(state, cursor.ref.category) &&
        first_stop_in(state, cursor.ref.category).kind == DRAWER_ROW_CATEGORY)
    {
        cursor_to_category(state, cursor.ref.category);
        return;
    }
    size_t category = 0;
    if (first_shown_category(state, &category))
    {
        cursor_to_category(state, category);
        return;
    }
    state->cursor_any = false;
}

/* 写しが変わったので一致集合を作り直す（決定 4 の「改名と新規は絞り込みを保つ」）。
 * 絞り込んでいなければ何もしない。スクロール量は触らない（変えるのは語を変えたときだけ）。 */
static enum folio_state_outcome refresh_filter(struct folio_state *_Nonnull state)
{
    if (state->filter == nullptr || state->index_term == nullptr)
    {
        return FOLIO_STATE_READY;
    }
    struct index_filter *_Nullable rebuilt = nullptr;
    enum folio_state_outcome built = build_filter(state, state->index_term, &rebuilt);
    if (built != FOLIO_STATE_READY)
    {
        return built;
    }
    index_filter_destroy(state->filter);
    state->filter = rebuilt;
    settle_cursor(state);
    return FOLIO_STATE_READY;
}

/* 配置の入力。絞り込みの有無で経路を分けない（ADR 0024 の決定 3）。 */
static struct drawer_source source_of(const struct folio_state *_Nonnull state)
{
    struct drawer_source source = {
        .categories = state->categories,
        .notes = (const struct note_ledger *_Nonnull const *_Nonnull)state->notes,
        .filter = state->filter};
    return source;
}

enum folio_state_outcome folio_state_drawer_layout(const struct folio_state *_Nonnull state,
                                                   struct drawer_metrics metrics,
                                                   struct drawer_layout *_Nullable *_Nonnull out)
{
    struct drawer_source source = source_of(state);
    if (drawer_layout_create(&source, metrics, out) != DRAWER_LAYOUT_CREATED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct note_ref selection = located(state->selected_category, state->selected_note);
    struct drawer_cursor cursor = on_note(0, 0);
    bool any = current_cursor(state, &cursor);
    drawer_layout_mark(*out, state->document == FOLIO_DOCUMENT_NAMED ? &selection : nullptr,
                       any ? &cursor : nullptr);
    drawer_layout_scroll(*out, state->scroll);
    return FOLIO_STATE_READY;
}

/* 0 と上限の間へ丸める（core の drawer_layout_scroll と同じ規則）。 */
static int clamped(int value, int limit)
{
    if (value < 0)
    {
        return 0;
    }
    return value > limit ? limit : value;
}

enum folio_state_outcome folio_state_scroll_drawer(struct folio_state *_Nonnull state,
                                                   struct drawer_metrics metrics, int delta)
{
    struct drawer_layout *_Nullable layout = nullptr;
    enum folio_state_outcome outcome = folio_state_drawer_layout(state, metrics, &layout);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    int limit = drawer_layout_scroll_limit(layout);
    drawer_layout_destroy(layout);
    state->scroll = clamped(clamped(state->scroll, limit) + delta, limit);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_reveal_cursor(struct folio_state *_Nonnull state,
                                                   struct drawer_metrics metrics)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor))
    {
        return FOLIO_STATE_READY;
    }
    struct drawer_layout *_Nullable layout = nullptr;
    enum folio_state_outcome outcome = folio_state_drawer_layout(state, metrics, &layout);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    state->scroll = drawer_layout_reveal(layout, cursor);
    drawer_layout_destroy(layout);
    return FOLIO_STATE_READY;
}

enum folio_theme folio_state_theme(const struct folio_state *_Nonnull state)
{
    return state->theme;
}

enum folio_state_outcome folio_state_settings_notice(const struct folio_state *_Nonnull state)
{
    return state->settings_notice;
}

bool folio_state_number(const struct folio_state *_Nonnull state)
{
    return folio_settings_number(state->settings);
}

/* 書けたときの結果。書けない理由は場所の違う 1 行に写す（台帳の STORE_FAILED とは別）。 */
static enum folio_state_outcome from_settings_store(enum persistence_outcome stored)
{
    if (stored == PERSISTENCE_STORED)
    {
        return FOLIO_STATE_READY;
    }
    return stored == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                               : FOLIO_STATE_SETTINGS_STORE_FAILED;
}

enum folio_state_outcome folio_state_set_number(struct folio_state *_Nonnull state, bool number)
{
    if (state->settings_notice != FOLIO_STATE_READY)
    {
        return state->settings_notice;
    }
    if (folio_settings_number(state->settings) == number)
    {
        return FOLIO_STATE_READY;
    }
    struct folio_settings *_Nullable changed = nullptr;
    if (folio_settings_with_number(state->settings, number, &changed) != FOLIO_SETTINGS_READY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    /* 先に書いてから採用する。書けなければ値を変えない（ADR 0025 の決定 5）。 */
    enum folio_state_outcome stored =
        from_settings_store(state->port.write_settings(state->port.adapter, changed));
    if (stored != FOLIO_STATE_READY)
    {
        folio_settings_destroy(changed);
        return stored;
    }
    folio_settings_destroy(state->settings);
    state->settings = changed;
    return FOLIO_STATE_READY;
}

size_t folio_state_note_count(const struct folio_state *_Nonnull state)
{
    return total_notes(state);
}

enum folio_state_outcome folio_state_toggle_category(struct folio_state *_Nonnull state,
                                                     size_t index)
{
    if (state->filter != nullptr)
    {
        /* 絞り込み中は台帳を書く操作を断る（ADR 0024 の決定 4）。 */
        return FOLIO_STATE_FILTERED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (index >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    struct category_ledger *_Nullable toggled = nullptr;
    enum folio_state_outcome outcome =
        from_category_ledger(category_ledger_toggled(state->categories, index, &toggled));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored =
        state->port.write_category_ledger(state->port.adapter, toggled);
    if (stored != PERSISTENCE_STORED)
    {
        category_ledger_destroy(toggled);
        return from_store(stored);
    }
    category_ledger_destroy(state->categories);
    state->categories = toggled;
    return FOLIO_STATE_READY;
}

/* 展開したあとのカーソル（ADR 0015 の決定 3）。選択がその中にあればそのノート、
 * 無ければ最初のノート（右ペインが変わる）、ノートが 0 本なら行に留まる。 */
static enum folio_state_outcome cursor_into(struct folio_state *_Nonnull state, size_t index)
{
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == index)
    {
        cursor_to_selection(state);
        return FOLIO_STATE_READY;
    }
    if (note_ledger_count(state->notes[index]) == 0)
    {
        return FOLIO_STATE_READY;
    }
    return folio_state_select_note(state, index, 0);
}

/* 開閉のあとにカーソルを移す。カーソルがそのカテゴリに無ければ動かさない（決定 3）。 */
static enum folio_state_outcome cursor_after_expanded(struct folio_state *_Nonnull state,
                                                      size_t index, bool expanded)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor) || cursor.ref.category != index)
    {
        return FOLIO_STATE_READY;
    }
    if (!expanded)
    {
        cursor_to_category(state, index);
        return FOLIO_STATE_READY;
    }
    return cursor_into(state, index);
}

enum folio_state_outcome folio_state_set_category_expanded(struct folio_state *_Nonnull state,
                                                           size_t index, bool expanded)
{
    if (state->filter != nullptr)
    {
        return FOLIO_STATE_FILTERED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (index >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (category_ledger_expanded(state->categories, index) == expanded)
    {
        /* 変わらないなら書く理由が無い（ADR 0013 の決定 5）。カーソルもそのまま。 */
        return FOLIO_STATE_READY;
    }
    enum folio_state_outcome outcome = folio_state_toggle_category(state, index);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    return cursor_after_expanded(state, index, expanded);
}

static bool same_color(struct rgb_color left, struct rgb_color right)
{
    return left.red == right.red && left.green == right.green && left.blue == right.blue;
}

enum folio_state_outcome folio_state_recolor_category(struct folio_state *_Nonnull state,
                                                      size_t index, struct rgb_color color)
{
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (index >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (same_color(category_ledger_color(state->categories, index), color))
    {
        /* 変わらないなら書く理由が無い（ADR 0010 の決定 2）。 */
        return FOLIO_STATE_READY;
    }
    struct category_ledger *_Nullable recolored = nullptr;
    enum folio_state_outcome outcome = from_category_ledger(
        category_ledger_recolored(state->categories, index, color, &recolored));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored =
        state->port.write_category_ledger(state->port.adapter, recolored);
    if (stored != PERSISTENCE_STORED)
    {
        category_ledger_destroy(recolored);
        return from_store(stored);
    }
    category_ledger_destroy(state->categories);
    state->categories = recolored;
    return FOLIO_STATE_READY;
}

/* 並び替えたあとに、元の index 番目が来る位置。 */
static size_t moved_index(size_t from, size_t to, size_t index)
{
    if (index == from)
    {
        return to;
    }
    if (from < to)
    {
        return index > from && index <= to ? index - 1 : index;
    }
    return index >= to && index < from ? index + 1 : index;
}

/* 索引台帳の配列を台帳と同じ順に並べ替える。確保はしない。 */
static void move_notes(struct note_ledger *_Nonnull *_Nonnull items, size_t from, size_t to)
{
    struct note_ledger *_Nonnull moved = items[from];
    while (from < to)
    {
        items[from] = items[from + 1];
        from += 1;
    }
    while (from > to)
    {
        items[from] = items[from - 1];
        from -= 1;
    }
    items[to] = moved;
}

enum folio_state_outcome folio_state_move_category(struct folio_state *_Nonnull state, size_t from,
                                                   size_t to)
{
    if (state->filter != nullptr)
    {
        return FOLIO_STATE_FILTERED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    size_t count = category_ledger_count(state->categories);
    if (from >= count || to >= count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (from == to)
    {
        return FOLIO_STATE_READY;
    }
    struct category_ledger *_Nullable moved = nullptr;
    enum folio_state_outcome outcome =
        from_category_ledger(category_ledger_moved(state->categories, from, to, &moved));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored = state->port.write_category_ledger(state->port.adapter, moved);
    if (stored != PERSISTENCE_STORED)
    {
        category_ledger_destroy(moved);
        return from_store(stored);
    }
    category_ledger_destroy(state->categories);
    state->categories = moved;
    move_notes(state->notes, from, to);
    if (state->document != FOLIO_DOCUMENT_NONE)
    {
        state->selected_category = moved_index(from, to, state->selected_category);
    }
    if (state->cursor_any && state->cursor_kind == FOLIO_CURSOR_CATEGORY)
    {
        /* カテゴリ行のカーソルも同じカテゴリを指したまま移る。 */
        state->cursor_category = moved_index(from, to, state->cursor_category);
    }
    return FOLIO_STATE_READY;
}

/* 並び替えた索引台帳を書き戻し、書けたときだけ差し替える。 */
static enum folio_state_outcome store_notes(struct folio_state *_Nonnull state, size_t category,
                                            size_t from, size_t to)
{
    struct note_ledger *_Nullable moved = nullptr;
    enum folio_state_outcome outcome =
        from_note_ledger(note_ledger_moved(state->notes[category], from, to, &moved));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome stored = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, category), moved);
    if (stored != PERSISTENCE_STORED)
    {
        note_ledger_destroy(moved);
        return from_store(stored);
    }
    note_ledger_destroy(state->notes[category]);
    state->notes[category] = moved;
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == category)
    {
        state->selected_note = moved_index(from, to, state->selected_note);
    }
    return FOLIO_STATE_READY;
}

/* 同じカテゴリ内の並び替え（ADR 0007 の決定 3）。 */
static enum folio_state_outcome reorder_note(struct folio_state *_Nonnull state, size_t category,
                                             size_t from, size_t to)
{
    size_t count = note_ledger_count(state->notes[category]);
    if (from >= count || to >= count)
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    if (from == to)
    {
        return FOLIO_STATE_READY;
    }
    return store_notes(state, category, from, to);
}

/* 台帳に同じ名前があるか。判定はバイト単位（name_list と同じ・ADR 0008 の文脈）。 */
static bool holds_name(const struct note_ledger *_Nonnull ledger, const char *_Nonnull name)
{
    size_t count = note_ledger_count(ledger);
    for (size_t index = 0; index < count; ++index)
    {
        if (strcmp(note_ledger_name(ledger, index), name) == 0)
        {
            return true;
        }
    }
    return false;
}

/* (a) 範囲と移動先の同名を確かめる。to.note は移動先のノート数と等しければ末尾。 */
static enum folio_state_outcome transfer_allowed(const struct folio_state *_Nonnull state,
                                                 struct note_ref from, struct note_ref to)
{
    if (from.note >= note_ledger_count(state->notes[from.category]) ||
        to.note > note_ledger_count(state->notes[to.category]))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    const char *_Nonnull name = note_ledger_name(state->notes[from.category], from.note);
    return holds_name(state->notes[to.category], name) ? FOLIO_STATE_NAME_TAKEN : FOLIO_STATE_READY;
}

/* (d) 選択中の番号を移動後の索引へ付け替える。別のカテゴリなので 2 つの補正は同時に起きない。 */
static void renumber_selection(struct folio_state *_Nonnull state, struct note_ref from,
                               struct note_ref to)
{
    if (state->document != FOLIO_DOCUMENT_NAMED)
    {
        return;
    }
    if (state->selected_category == from.category && state->selected_note == from.note)
    {
        state->selected_category = to.category;
        state->selected_note = to.note;
        return;
    }
    if (state->selected_category == from.category && state->selected_note > from.note)
    {
        state->selected_note -= 1;
    }
    if (state->selected_category == to.category && state->selected_note >= to.note)
    {
        state->selected_note += 1;
    }
}

/* (e) 移動先 → 移動元 の順に書き戻す。どちらかが書けなければ索引は移動後のまま LEDGER_STALE。 */
static enum folio_state_outcome store_both(struct folio_state *_Nonnull state, size_t to,
                                           size_t from)
{
    enum persistence_outcome grown = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, to), state->notes[to]);
    enum persistence_outcome shrunk = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, from), state->notes[from]);
    if (grown == PERSISTENCE_OUT_OF_MEMORY || shrunk == PERSISTENCE_OUT_OF_MEMORY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (grown != PERSISTENCE_STORED || shrunk != PERSISTENCE_STORED)
    {
        return FOLIO_STATE_LEDGER_STALE;
    }
    return FOLIO_STATE_READY;
}

/* (b) 移動後の 2 つの台帳を作る。どちらかが作れなければ両方捨てる。 */
static enum folio_state_outcome transfer_ledgers(const struct folio_state *_Nonnull state,
                                                 struct note_ref from, struct note_ref to,
                                                 struct note_ledger *_Nullable *_Nonnull pair)
{
    enum folio_state_outcome outcome =
        from_note_ledger(note_ledger_removed(state->notes[from.category], from.note, &pair[0]));
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = from_note_ledger(note_ledger_inserted(
            state->notes[to.category], to.note,
            note_ledger_name(state->notes[from.category], from.note), &pair[1]));
    }
    if (outcome != FOLIO_STATE_READY)
    {
        note_ledger_destroy(pair[0]);
    }
    return outcome;
}

/* 台帳を差し替える前に、本文の写しを移動先のカテゴリへ付け替える（ADR 0024 の決定 1）。
 * 確保に失敗したら写しを次の絞り込みで読み直させ、記憶不足として報せる。 */
static enum folio_state_outcome recopy_move(struct folio_state *_Nonnull state,
                                            struct note_ref from, size_t to)
{
    if (!state->corpus_loaded)
    {
        return FOLIO_STATE_READY;
    }
    if (note_corpus_relocate(state->corpus, category_ledger_name(state->categories, from.category),
                             note_ledger_name(state->notes[from.category], from.note),
                             category_ledger_name(state->categories, to)) != NOTE_CORPUS_ACCEPTED)
    {
        state->corpus_loaded = false;
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_READY;
}

/* 別のカテゴリへ移す（ADR 0008 の決定 3 の (a)〜(e)）。md の rename が先で、台帳が追随する。 */
static enum folio_state_outcome transfer_note(struct folio_state *_Nonnull state,
                                              struct note_ref from, struct note_ref to)
{
    enum folio_state_outcome outcome = transfer_allowed(state, from, to);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    struct note_ledger *_Nullable pair[2] = {nullptr, nullptr};
    outcome = transfer_ledgers(state, from, to, pair);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    enum persistence_outcome moved = state->port.move_note(
        state->port.adapter, category_ledger_name(state->categories, from.category),
        note_ledger_name(state->notes[from.category], from.note),
        category_ledger_name(state->categories, to.category));
    if (moved != PERSISTENCE_STORED)
    {
        note_ledger_destroy(pair[1]);
        note_ledger_destroy(pair[0]);
        return from_store(moved);
    }
    enum folio_state_outcome copied = recopy_move(state, from, to.category);
    note_ledger_destroy(state->notes[from.category]);
    state->notes[from.category] = pair[0];
    note_ledger_destroy(state->notes[to.category]);
    state->notes[to.category] = pair[1];
    renumber_selection(state, from, to);
    enum folio_state_outcome stored = store_both(state, to.category, from.category);
    return stored != FOLIO_STATE_READY ? stored : copied;
}

enum folio_state_outcome folio_state_move_note(struct folio_state *_Nonnull state,
                                               struct note_ref from, struct note_ref to)
{
    if (state->filter != nullptr)
    {
        return FOLIO_STATE_FILTERED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    size_t count = category_ledger_count(state->categories);
    if (from.category >= count || to.category >= count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (from.category == to.category)
    {
        return reorder_note(state, from.category, from.note, to.note);
    }
    return transfer_note(state, from, to);
}

/* 本文を読んで RTF にする。読めない理由は 1 つに畳む（無い・読めない・UTF-8 でない）。 */
static enum folio_state_outcome render_note(struct folio_state *_Nonnull state,
                                            const char *_Nonnull category,
                                            const char *_Nonnull note)
{
    struct note_text *_Nullable body = nullptr;
    enum persistence_outcome read =
        state->port.read_note(state->port.adapter, category, note, &body);
    if (read == PERSISTENCE_OUT_OF_MEMORY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (read != PERSISTENCE_LOADED)
    {
        return FOLIO_STATE_NOTE_UNREADABLE;
    }
    struct markdown_rtf *_Nullable rendered = nullptr;
    enum markdown_rtf_outcome converted = markdown_rtf_create(body, state->palette, &rendered);
    if (converted != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(body);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    markdown_rtf_destroy(state->pane);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = body;
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                 size_t category, size_t note)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }

    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    if (category >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (note >= note_ledger_count(state->notes[category]))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    enum folio_state_outcome outcome =
        render_note(state, category_ledger_name(state->categories, category),
                    note_ledger_name(state->notes[category], note));
    if (outcome == FOLIO_STATE_READY)
    {
        state->document = FOLIO_DOCUMENT_NAMED;
        state->selected_category = category;
        state->selected_note = note;
        cursor_to_selection(state);
    }
    return outcome;
}

/* category の中で at 以上の最初の見えるノート行。無ければ false。 */
static bool shown_note_from(const struct folio_state *_Nonnull state, size_t category, size_t at,
                            size_t *_Nonnull out)
{
    if (!shown_expanded(state, category))
    {
        return false;
    }
    size_t count = note_ledger_count(state->notes[category]);
    for (size_t note = at; note < count; ++note)
    {
        if (shown_note(state, category, note))
        {
            *out = note;
            return true;
        }
    }
    return false;
}

/* category の中で at 未満の最後の見えるノート行。無ければ false。 */
static bool shown_note_before(const struct folio_state *_Nonnull state, size_t category, size_t at,
                              size_t *_Nonnull out)
{
    if (!shown_expanded(state, category))
    {
        return false;
    }
    size_t count = note_ledger_count(state->notes[category]);
    size_t reach = at < count ? at : count;
    while (reach > 0)
    {
        reach -= 1;
        if (shown_note(state, category, reach))
        {
            *out = reach;
            return true;
        }
    }
    return false;
}

/* カテゴリの最初の止まる行。見えるノートがあればその 1 本目、無ければカテゴリ行（決定 2）。 */
static struct drawer_cursor first_stop_in(const struct folio_state *_Nonnull state, size_t category)
{
    size_t note = 0;
    return shown_note_from(state, category, 0, &note) ? on_note(category, note)
                                                      : on_category(category);
}

/* カテゴリの最後の止まる行。 */
static struct drawer_cursor last_stop_in(const struct folio_state *_Nonnull state, size_t category)
{
    size_t note = 0;
    size_t count = note_ledger_count(state->notes[category]);
    return shown_note_before(state, category, count, &note) ? on_note(category, note)
                                                            : on_category(category);
}

/* 台帳の順で最初の止まる行。止まる行が 1 つも無ければ false。 */
static bool first_stop(const struct folio_state *_Nonnull state, struct drawer_cursor *_Nonnull out)
{
    size_t category = 0;
    if (!first_shown_category(state, &category))
    {
        return false;
    }
    *out = first_stop_in(state, category);
    return true;
}

/* 台帳の順で最後の止まる行。止まる行が 1 つも無ければ false。 */
static bool last_stop(const struct folio_state *_Nonnull state, struct drawer_cursor *_Nonnull out)
{
    size_t category = category_ledger_count(state->categories);
    while (category > 0)
    {
        category -= 1;
        if (shown_category(state, category))
        {
            *out = last_stop_in(state, category);
            return true;
        }
    }
    return false;
}

/* from と同じカテゴリで、from の次に来るノート行の番号（見えるノート数以上なら次は無い）。
 * カテゴリ行のカーソルの次は、そのカテゴリの中の 1 本目になる。 */
static size_t after_in(struct drawer_cursor from)
{
    switch (from.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return 0;
    case DRAWER_ROW_NOTE:
        return from.ref.note + 1;
    }
    return 0;
}

/* from と同じカテゴリで、from より前を探し始める位置。カテゴリ行のカーソルは中へ戻らない。 */
static size_t before_in(struct drawer_cursor from)
{
    switch (from.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return 0;
    case DRAWER_ROW_NOTE:
        return from.ref.note;
    }
    return 0;
}

/* カーソルのカテゴリが索引の中にあるか（絞り込みで消えた行から数え直すときの番人）。 */
static bool holds_category(const struct folio_state *_Nonnull state, size_t category)
{
    return category < category_ledger_count(state->categories) && shown_category(state, category);
}

/* from より後ろにある最初の止まる行。端なら false。from が折り畳んだカテゴリの中のノートでも、
 * その位置から数え直す（自分のカテゴリ行へは戻らない）。 */
static bool next_stop(const struct folio_state *_Nonnull state, struct drawer_cursor from,
                      struct drawer_cursor *_Nonnull out)
{
    size_t note = 0;
    if (holds_category(state, from.ref.category) &&
        shown_note_from(state, from.ref.category, after_in(from), &note))
    {
        *out = on_note(from.ref.category, note);
        return true;
    }
    size_t count = category_ledger_count(state->categories);
    for (size_t category = from.ref.category + 1; category < count; ++category)
    {
        if (shown_category(state, category))
        {
            *out = first_stop_in(state, category);
            return true;
        }
    }
    return false;
}

/* from より前にある最後の止まる行。端なら false。 */
static bool previous_stop(const struct folio_state *_Nonnull state, struct drawer_cursor from,
                          struct drawer_cursor *_Nonnull out)
{
    size_t note = 0;
    if (holds_category(state, from.ref.category) &&
        shown_note_before(state, from.ref.category, before_in(from), &note))
    {
        *out = on_note(from.ref.category, note);
        return true;
    }
    size_t count = category_ledger_count(state->categories);
    size_t category = from.ref.category < count ? from.ref.category : count;
    while (category > 0)
    {
        category -= 1;
        if (shown_category(state, category))
        {
            *out = last_stop_in(state, category);
            return true;
        }
    }
    return false;
}

/* 歩みの行き先。端なら false（ADR 0015 の決定 2）。 */
static bool stepped_to(const struct folio_state *_Nonnull state, enum folio_step step,
                       struct drawer_cursor *_Nonnull out)
{
    struct drawer_cursor from = on_note(0, 0);
    bool any = current_cursor(state, &from);
    switch (step)
    {
    case FOLIO_STEP_NEXT:
        return any ? next_stop(state, from, out) : first_stop(state, out);
    case FOLIO_STEP_PREVIOUS:
        return any ? previous_stop(state, from, out) : last_stop(state, out);
    case FOLIO_STEP_FIRST:
        return first_stop(state, out);
    case FOLIO_STEP_LAST:
        return last_stop(state, out);
    }
    return false;
}

/* 行き先のノートへ移る。同じノートなら読み直さず、カーソルだけをそのノートへ戻す。 */
static enum folio_state_outcome stepped_to_note(struct folio_state *_Nonnull state,
                                                struct note_ref target)
{
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == target.category &&
        state->selected_note == target.note)
    {
        cursor_to_selection(state);
        return FOLIO_STATE_READY;
    }
    return folio_state_select_note(state, target.category, target.note);
}

enum folio_state_outcome folio_state_select_adjacent(struct folio_state *_Nonnull state,
                                                     enum folio_step step)
{
    struct drawer_cursor target = on_note(0, 0);
    if (!first_stop(state, &target))
    {
        /* 止まる行が 1 つも無い（カテゴリが無い）。 */
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    if (!stepped_to(state, step, &target))
    {
        /* 端では動かない（ADR 0015 の決定 2）。 */
        return FOLIO_STATE_READY;
    }
    switch (target.kind)
    {
    case DRAWER_ROW_CATEGORY:
        /* カテゴリ行ではカーソルだけが動く。選択も右ペインも変えない（決定 2）。 */
        cursor_to_category(state, target.ref.category);
        return FOLIO_STATE_READY;
    case DRAWER_ROW_NOTE:
        return stepped_to_note(state, target.ref);
    }
    return FOLIO_STATE_READY;
}

bool folio_state_step_kind(const struct folio_state *_Nonnull state, enum folio_step step,
                           enum folio_cursor_kind *_Nonnull kind, struct note_ref *_Nonnull out)
{
    struct drawer_cursor target = on_note(0, 0);
    if (!stepped_to(state, step, &target))
    {
        return false;
    }
    *kind = kind_of(target);
    *out = target.ref;
    return true;
}

bool folio_state_cursor(const struct folio_state *_Nonnull state,
                        enum folio_cursor_kind *_Nonnull kind, struct note_ref *_Nonnull out)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!current_cursor(state, &cursor))
    {
        return false;
    }
    *kind = kind_of(cursor);
    *out = cursor.ref;
    return true;
}

bool folio_state_selection(const struct folio_state *_Nonnull state, struct note_ref *_Nonnull out)
{
    if (state->document != FOLIO_DOCUMENT_NAMED)
    {
        return false;
    }
    *out = located(state->selected_category, state->selected_note);
    return true;
}

enum folio_state_outcome folio_state_begin_edit(struct folio_state *_Nonnull state)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    state->mode = PANE_MODE_EDIT;
    return FOLIO_STATE_READY;
}

enum folio_document_kind folio_state_document_kind(const struct folio_state *_Nonnull state)
{
    return state->document;
}

size_t folio_state_category_count(const struct folio_state *_Nonnull state)
{
    return category_ledger_count(state->categories);
}

size_t folio_state_document_category(const struct folio_state *_Nonnull state)
{
    return state->selected_category;
}

const char *_Nonnull folio_state_category_name(const struct folio_state *_Nonnull state,
                                               size_t category)
{
    return category_ledger_name(state->categories, category);
}

size_t folio_state_current_category(const struct folio_state *_Nonnull state)
{
    struct drawer_cursor cursor = {0};
    if (current_cursor(state, &cursor))
    {
        return cursor.ref.category;
    }
    return state->document == FOLIO_DOCUMENT_NONE ? 0 : state->selected_category;
}

enum folio_state_outcome folio_state_new_note(struct folio_state *_Nonnull state, size_t category)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    if (category >= folio_state_category_count(state))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    struct note_text *_Nullable body = nullptr;
    struct markdown_rtf *_Nullable pane = nullptr;
    if (note_text_create("", 0, &body) != NOTE_TEXT_ACCEPTED ||
        markdown_rtf_empty(state->palette, &pane) != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(body);
        markdown_rtf_destroy(pane);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    note_text_destroy(state->body);
    markdown_rtf_destroy(state->pane);
    state->body = body;
    state->pane = pane;
    state->document = FOLIO_DOCUMENT_UNTITLED;
    state->selected_category = category;
    state->mode = PANE_MODE_EDIT;
    state->cursor_any = false;
    return FOLIO_STATE_READY;
}

/* 書き戻す直前に、いまファイルにある本文を履歴へ写させる（ADR 0012 の決定 2）。
 * 元の md が無ければ写すものが無いので、そのまま書き戻しへ進む。 */
static enum folio_state_outcome archive_before_store(struct folio_state *_Nonnull state)
{
    enum persistence_outcome archived = state->port.archive_note(
        state->port.adapter, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note));
    if (archived == PERSISTENCE_STORED || archived == PERSISTENCE_ABSENT)
    {
        return FOLIO_STATE_READY;
    }
    return archived == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                 : FOLIO_STATE_HISTORY_FAILED;
}

/* 保存できた本文で写しを差し替え、絞り込み中なら一致集合も作り直す（ADR 0024 の決定 1 / 4）。
 * まだ本文を読み込んでいなければ何もしない（最初の絞り込みで全部読む）。 */
static enum folio_state_outcome refresh_copy(struct folio_state *_Nonnull state)
{
    if (!state->corpus_loaded || state->document != FOLIO_DOCUMENT_NAMED)
    {
        return FOLIO_STATE_READY;
    }
    enum note_corpus_outcome stored = note_corpus_put(
        state->corpus, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note),
        state->body);
    if (stored != NOTE_CORPUS_ACCEPTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return refresh_filter(state);
}

/* 正規化済みの本文を書き戻し、表示値も作り直す。書けなければ何も変えない（ADR 0006 の決定 6）。
 * 履歴を残せなければ書き戻しにも進まない（ADR 0012 の決定 2）。 */
static enum folio_state_outcome store_edited(struct folio_state *_Nonnull state,
                                             struct note_text *_Nonnull edited)
{
    if (note_text_equals(state->body, edited))
    {
        note_text_destroy(edited);
        return FOLIO_STATE_READY;
    }
    enum folio_state_outcome archived = archive_before_store(state);
    if (archived != FOLIO_STATE_READY)
    {
        note_text_destroy(edited);
        return archived;
    }
    struct markdown_rtf *_Nullable rendered = nullptr;
    if (markdown_rtf_create(edited, state->palette, &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(edited);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum persistence_outcome stored = state->port.write_note(
        state->port.adapter, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note), edited);
    if (stored != PERSISTENCE_STORED)
    {
        markdown_rtf_destroy(rendered);
        note_text_destroy(edited);
        return stored == PERSISTENCE_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_NOTE_STORE_FAILED;
    }
    markdown_rtf_destroy(state->pane);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = edited;
    return refresh_copy(state);
}

/* UI が持つ編集中の本文（UTF-16）を core で検証・変換し、読んだ本文の改行の形へ揃える。
 * 保存と読み取り専用の変更問い合わせがこの 1 本を共有する（C-014・ADR 0016 の決定 10）。 */
static enum folio_state_outcome edited_text(const struct folio_state *_Nonnull state,
                                            const char16_t *_Nonnull units, size_t count,
                                            struct note_text *_Nullable *_Nonnull out)
{
    if (state->mode != PANE_MODE_EDIT)
    {
        return FOLIO_STATE_NOT_EDITING;
    }
    struct utf8_text *_Nullable narrow = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(units, count, &narrow);
    if (converted != UTF8_TEXT_CONVERTED)
    {
        return converted == UTF8_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                    : FOLIO_STATE_NOTE_MALFORMED;
    }
    enum note_text_outcome accepted = note_text_from_editor(
        utf8_text_bytes(narrow), utf8_text_length(narrow), note_text_line_ending(state->body), out);
    utf8_text_destroy(narrow);
    if (accepted != NOTE_TEXT_ACCEPTED)
    {
        return accepted == NOTE_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_NOTE_MALFORMED;
    }
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome save_note(struct folio_state *_Nonnull state,
                                          const char16_t *_Nonnull units, size_t count)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    if (state->mode == PANE_MODE_VIEW)
    {
        return FOLIO_STATE_READY;
    }
    struct note_text *_Nullable edited = nullptr;
    enum folio_state_outcome outcome = edited_text(state, units, count, &edited);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    return store_edited(state, edited);
}

/* 公開できなかった理由を意図の結果へ写す。準備の失敗が先で、次に書けなかった理由。 */
static enum folio_state_outcome unpublished(enum folio_state_outcome prepared,
                                            enum persistence_outcome stored)
{
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
    return stored == PERSISTENCE_UNWRITABLE ? FOLIO_STATE_NOTE_STORE_FAILED : translate(stored);
}

/* 副作用より先にすべてを確保。mdが公開された後は確保せず、表示をファイルへ揃える。 */
static enum folio_state_outcome create_edited(struct folio_state *_Nonnull state,
                                              const struct note_destination *_Nonnull destination,
                                              struct note_text *_Nonnull edited)
{
    size_t category = destination->category;
    const char *_Nonnull name = note_name_stem(destination->name);
    struct note_ledger *_Nullable ledger = nullptr;
    struct markdown_rtf *_Nullable rendered = nullptr;
    size_t index = note_ledger_count(state->notes[category]);
    enum folio_state_outcome prepared =
        from_note_ledger(note_ledger_inserted(state->notes[category], index, name, &ledger));
    if (prepared == FOLIO_STATE_READY &&
        markdown_rtf_create(edited, state->palette, &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        prepared = FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum persistence_outcome stored = PERSISTENCE_UNWRITABLE;
    if (prepared == FOLIO_STATE_READY)
    {
        stored = state->port.create_note(
            state->port.adapter, category_ledger_name(state->categories, category), name, edited);
    }
    if (stored != PERSISTENCE_STORED)
    {
        note_ledger_destroy(ledger);
        markdown_rtf_destroy(rendered);
        note_text_destroy(edited);
        return unpublished(prepared, stored);
    }
    note_ledger_destroy(state->notes[category]);
    state->notes[category] = ledger;
    markdown_rtf_destroy(state->pane);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = edited;
    state->document = FOLIO_DOCUMENT_NAMED;
    state->selected_category = category;
    state->selected_note = index;
    if (!state->cursor_any)
    {
        cursor_to_selection(state);
    }
    state->index_pending = true;
    state->pending_category = category;
    enum folio_state_outcome synced = published_index(synchronize_index(state));
    /* 新しく公開したノートも写しと一致集合に入れる（ADR 0024 の決定 4）。 */
    enum folio_state_outcome copied = refresh_copy(state);
    return synced != FOLIO_STATE_READY ? synced : copied;
}

/* 閲覧はMarkdown原文、編集は未保存の入力を使う。RTF表示の文字は保存しない。 */
static enum folio_state_outcome copied_text(const struct folio_state *_Nonnull state,
                                            const char16_t *_Nonnull units, size_t count,
                                            struct note_text *_Nullable *_Nonnull out)
{
    if (state->mode == PANE_MODE_EDIT)
    {
        return edited_text(state, units, count, out);
    }
    enum note_text_outcome copied =
        note_text_create(note_text_bytes(state->body), note_text_length(state->body), out);
    if (copied != NOTE_TEXT_ACCEPTED)
    {
        return copied == NOTE_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                 : FOLIO_STATE_NOTE_MALFORMED;
    }
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_store_new(struct folio_state *_Nonnull state,
                                               const struct note_destination *_Nonnull destination,
                                               const char16_t *_Nonnull units, size_t count)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    if (destination->category >= folio_state_category_count(state))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (holds_name(state->notes[destination->category], note_name_stem(destination->name)))
    {
        return FOLIO_STATE_NAME_TAKEN;
    }
    struct note_text *_Nullable edited = nullptr;
    enum folio_state_outcome converted = copied_text(state, units, count, &edited);
    if (converted != FOLIO_STATE_READY)
    {
        return converted;
    }
    return create_edited(state, destination, edited);
}

/* 大小文字だけ違う名前も Windows では同じ md になるので、衝突として断る（ADR 0022 の決定 1）。 */
static char folded(char value)
{
    return value >= 'A' && value <= 'Z' ? (char)(value + 32) : value;
}

static bool same_folded(const char *_Nonnull left, const char *_Nonnull right)
{
    size_t index = 0;
    while (folded(left[index]) == folded(right[index]))
    {
        if (left[index] == '\0')
        {
            return true;
        }
        index += 1;
    }
    return false;
}

static bool holds_folded_name(const struct note_ledger *_Nonnull ledger, const char *_Nonnull name)
{
    size_t count = note_ledger_count(ledger);
    for (size_t index = 0; index < count; ++index)
    {
        if (same_folded(note_ledger_name(ledger, index), name))
        {
            return true;
        }
    }
    return false;
}

static enum folio_state_outcome from_note_rename(enum note_rename_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_RENAME_ACCEPTED:
        return FOLIO_STATE_READY;
    case NOTE_RENAME_INVALID:
        return FOLIO_STATE_INVALID_NAME;
    case NOTE_RENAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_INVALID_NAME;
}

/* 副作用の前に意図を確保し、ポートへ渡す。公開前の拒否では意図を残さない（決定 2）。 */
static enum folio_state_outcome rename_selected(struct folio_state *_Nonnull state,
                                                const struct note_name *_Nonnull name)
{
    size_t category = state->selected_category;
    struct note_rename_target target = {.index = state->selected_note, .name = name};
    struct note_rename *_Nullable intent = nullptr;
    enum folio_state_outcome prepared =
        from_note_rename(note_rename_create(category_ledger_name(state->categories, category),
                                            state->notes[category], &target, &intent));
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
    enum rename_outcome moved = state->port.rename_note(state->port.adapter, intent, RENAME_START);
    if (moved == RENAME_COMPLETED)
    {
        state->rename = intent;
        state->rename_category = category;
        return adopt_rename(state);
    }
    if (moved == RENAME_PENDING || moved == RENAME_HALTED)
    {
        /* 記録を公開した後の失敗はどちらも意図を保持する（決定 2 / 決定 7）。 */
        state->rename = intent;
        state->rename_category = category;
        return from_rename(moved);
    }
    /* 記録を公開する前の拒否。data/ は何も変わっていないので意図は残さない。 */
    note_rename_destroy(intent);
    return from_rename(moved);
}

enum folio_state_outcome folio_state_rename_note(struct folio_state *_Nonnull state,
                                                 const struct note_name *_Nonnull name,
                                                 const char16_t *_Nonnull units, size_t count)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    const char *_Nonnull stem = note_name_stem(name);
    size_t category = state->selected_category;
    if (strcmp(note_ledger_name(state->notes[category], state->selected_note), stem) == 0)
    {
        /* 同じ名前は変更なし。md も履歴も台帳も触らない（決定 1）。 */
        return FOLIO_STATE_READY;
    }
    if (holds_folded_name(state->notes[category], stem))
    {
        return FOLIO_STATE_NAME_TAKEN;
    }
    /* 改名の前に既存の保存を成功させる。閲覧中は save_note が何もせずに戻る。 */
    enum folio_state_outcome saved = save_note(state, units, count);
    if (saved != FOLIO_STATE_READY)
    {
        return saved;
    }
    /* 一致集合の作り直しは完了した改名を受け取る `adopt_rename` が行う（補正 3）。 */
    return rename_selected(state, name);
}

enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                const char16_t *_Nonnull units, size_t count)
{
    return save_note(state, units, count);
}

enum folio_state_outcome folio_state_note_changed(struct folio_state *_Nonnull state,
                                                  const char16_t *_Nonnull units, size_t count,
                                                  enum folio_note_change *_Nonnull out)
{
    /* 他の保存系と同じく未同期の台帳を先に修復する。副作用はこの修復だけ（ADR 0021 の決定 3）。 */
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        *out = FOLIO_NOTE_CHANGED;
        return FOLIO_STATE_READY;
    }
    if (state->mode == PANE_MODE_VIEW)
    {
        *out = FOLIO_NOTE_SAME;
        return FOLIO_STATE_READY;
    }
    struct note_text *_Nullable edited = nullptr;
    enum folio_state_outcome outcome = edited_text(state, units, count, &edited);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    *out = note_text_equals(state->body, edited) ? FOLIO_NOTE_SAME : FOLIO_NOTE_CHANGED;
    note_text_destroy(edited);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_end_edit(struct folio_state *_Nonnull state,
                                              const char16_t *_Nonnull units, size_t count)
{
    enum folio_state_outcome outcome = save_note(state, units, count);
    if (outcome == FOLIO_STATE_READY)
    {
        state->mode = PANE_MODE_VIEW;
    }
    return outcome;
}

enum pane_mode folio_state_pane_mode(const struct folio_state *_Nonnull state)
{
    return state->mode;
}

/* 走査の結果を閉じた値へ写す（ADR 0028 の決定 6。写像はここ 1 か所）。 */
static enum folio_state_outcome from_scan(enum regex_scan_outcome scanned)
{
    switch (scanned)
    {
    case REGEX_SCAN_READY:
        return FOLIO_STATE_READY;
    case REGEX_SCAN_BAD_PATTERN:
        return FOLIO_STATE_REPLACE_BAD_PATTERN;
    case REGEX_SCAN_TIMED_OUT:
        return FOLIO_STATE_REPLACE_TIMED_OUT;
    case REGEX_SCAN_TOO_COMPLEX:
        return FOLIO_STATE_REPLACE_TOO_COMPLEX;
    case REGEX_SCAN_TOO_MANY:
        return FOLIO_STATE_REPLACE_TOO_MANY;
    case REGEX_SCAN_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_REPLACE_BAD_PATTERN;
}

static enum folio_state_outcome from_preview(enum replace_preview_outcome made)
{
    switch (made)
    {
    case REPLACE_PREVIEW_READY:
        return FOLIO_STATE_READY;
    case REPLACE_PREVIEW_BAD_TEMPLATE:
        return FOLIO_STATE_REPLACE_BAD_TEMPLATE;
    case REPLACE_PREVIEW_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_REPLACE_BAD_TEMPLATE;
}

/* 一致が 1 つも無いのは失敗ではない。呼び出し側へ渡す編集が無いだけ（決定 6）。 */
static enum folio_state_outcome from_replace(enum note_replace_outcome built)
{
    switch (built)
    {
    case NOTE_REPLACE_READY:
    case NOTE_REPLACE_NOT_FOUND:
        return FOLIO_STATE_READY;
    case NOTE_REPLACE_BAD_SPAN:
        return FOLIO_STATE_REPLACE_BAD_SPAN;
    /* 入れ物に収まらなかった一致の列は「多すぎる」と同じ意味になる（レビュー D2）。 */
    case NOTE_REPLACE_PARTIAL_MATCHES:
        return FOLIO_STATE_REPLACE_TOO_MANY;
    case NOTE_REPLACE_TOO_LARGE:
        return FOLIO_STATE_REPLACE_TOO_LARGE;
    case NOTE_REPLACE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_REPLACE_TOO_LARGE;
}

/* 一致の入れ物を capacity 件まで広げる。縮めない。 */
static bool reserve_matches(struct folio_state *_Nonnull state, size_t capacity)
{
    if (state->found_capacity >= capacity)
    {
        return true;
    }
    struct regex_match *_Nullable grown = realloc(state->found, capacity * sizeof *grown);
    if (grown == nullptr)
    {
        return false;
    }
    state->found = grown;
    state->found_capacity = capacity;
    return true;
}

static void aim_matches(const struct folio_state *_Nonnull state,
                        struct regex_matches *_Nonnull matches)
{
    matches->items = state->found;
    matches->capacity = state->found_capacity;
    matches->count = 0;
}

/* 総数が入れ物を超えたら広げて、もう 1 度だけ走査する（決定 2）。 */
static enum regex_scan_outcome scan_matches(struct folio_state *_Nonnull state,
                                            const struct regex_request *_Nonnull request,
                                            struct regex_matches *_Nonnull matches,
                                            struct regex_pattern_error *_Nonnull error)
{
    aim_matches(state, matches);
    enum regex_scan_outcome scanned =
        state->regex.scan(state->regex.adapter, request, matches, error);
    if (scanned != REGEX_SCAN_READY || matches->count <= matches->capacity)
    {
        return scanned;
    }
    if (!reserve_matches(state, matches->count))
    {
        return REGEX_SCAN_OUT_OF_MEMORY;
    }
    aim_matches(state, matches);
    return state->regex.scan(state->regex.adapter, request, matches, error);
}

/* いまの文書の宛先（決定 6）。名前で持つので並び替えでは動かない。無題はノート名が空。 */
static struct replace_source source_for(const struct folio_state *_Nonnull state,
                                        const char16_t *_Nonnull text, size_t length)
{
    struct replace_source source = {
        .text = text,
        .length = length,
        .replacement = u"",
        .replacement_length = 0,
        /* 呼び出し元は下見も適用も editing_document() を先に通るので、文書は必ずある。 */
        .category = category_ledger_name(state->categories, state->selected_category),
        .note = folio_state_document_name(state)};
    return source;
}

/* 編集中の文書があるか（下見も適用もここを通る）。 */
static bool editing_document(const struct folio_state *_Nonnull state)
{
    return state->mode == PANE_MODE_EDIT && state->document != FOLIO_DOCUMENT_NONE;
}

/* 新しい下見を作って古いものと入れ替える。作れなければ古い下見をそのまま保つ（決定 6）。 */
static enum folio_state_outcome adopt_preview(struct folio_state *_Nonnull state,
                                              const struct replace_request *_Nonnull request,
                                              const struct regex_matches *_Nonnull matches)
{
    struct replace_source source = source_for(state, request->text, request->length);
    source.replacement = request->replacement;
    source.replacement_length = request->replacement_length;
    struct replace_preview *_Nullable preview = nullptr;
    enum folio_state_outcome made =
        from_preview(replace_preview_create(&source, matches, &preview));
    if (made != FOLIO_STATE_READY)
    {
        return made;
    }
    replace_preview_destroy(state->preview);
    state->preview = preview;
    return FOLIO_STATE_READY;
}

/* 下見を取り直す本体。入れ替えは adopt_preview が行い、ここは失敗の値を決めるだけ。 */
static enum folio_state_outcome preview_matches(struct folio_state *_Nonnull state,
                                                const struct replace_request *_Nonnull request)
{
    if (!editing_document(state))
    {
        return FOLIO_STATE_NOT_EDITING;
    }
    if (request->pattern_length == 0)
    {
        return FOLIO_STATE_REPLACE_NO_PATTERN;
    }
    struct regex_request scan = {.text = request->text,
                                 .length = request->length,
                                 .pattern = request->pattern,
                                 .pattern_length = request->pattern_length};
    struct regex_matches matches = {.items = state->found, .capacity = 0, .count = 0};
    struct regex_pattern_error error = {.offset = 0};
    enum folio_state_outcome scanned = from_scan(scan_matches(state, &scan, &matches, &error));
    state->replace_offset = scanned == FOLIO_STATE_REPLACE_BAD_PATTERN ? error.offset : 0;
    if (scanned != FOLIO_STATE_READY)
    {
        return scanned;
    }
    return adopt_preview(state, request, &matches);
}

enum folio_state_outcome folio_state_preview_replace(struct folio_state *_Nonnull state,
                                                     const struct replace_request *_Nonnull request)
{
    enum folio_state_outcome previewed = preview_matches(state, request);
    if (previewed != FOLIO_STATE_READY)
    {
        /* 下見は常に「最後の入力の結果」か「無い」。失敗した入力のまま当てさせない
         * （決定 6 の補足・2026-09-22 の独立レビュー B1）。下見が無ければ適用は STALE。 */
        replace_preview_destroy(state->preview);
        state->preview = nullptr;
    }
    return previewed;
}

size_t folio_state_replace_count(const struct folio_state *_Nonnull state)
{
    return state->preview == nullptr ? 0 : replace_preview_count(state->preview);
}

size_t folio_state_replace_error_offset(const struct folio_state *_Nonnull state)
{
    return state->replace_offset;
}

enum folio_state_outcome folio_state_apply_replace(struct folio_state *_Nonnull state,
                                                   const struct replace_apply *_Nonnull apply,
                                                   struct replace_edit *_Nullable *_Nonnull out)
{
    *out = nullptr;
    if (!editing_document(state))
    {
        return FOLIO_STATE_NOT_EDITING;
    }
    struct replace_source source = source_for(state, apply->text, apply->length);
    if (state->preview == nullptr || !replace_preview_holds(state->preview, &source))
    {
        return FOLIO_STATE_REPLACE_STALE;
    }
    struct note_replace_plan plan =
        replace_preview_plan(state->preview, apply->scope, apply->anchor);
    struct replace_edit *_Nullable edit = nullptr;
    enum note_replace_outcome built = note_replace_build(&plan, &edit);
    *out = built == NOTE_REPLACE_READY ? edit : nullptr;
    return from_replace(built);
}

/* 語は UI から UTF-16 で届くので、ここで内部の UTF-8 へ写して所有する（C-014 の例外・ADR 0023）。
 * 本文も選択も持たない。探すのは core で、選択を動かすのは RichEdit である。 */
enum folio_state_outcome folio_state_set_search_term(struct folio_state *_Nonnull state,
                                                     const char16_t *_Nonnull units, size_t count)
{
    struct utf8_text *_Nullable term = nullptr;
    if (count > 0)
    {
        enum utf8_text_outcome converted = utf8_text_create(units, count, &term);
        if (converted != UTF8_TEXT_CONVERTED)
        {
            return converted == UTF8_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                        : FOLIO_STATE_SEARCH_MALFORMED;
        }
    }
    utf8_text_destroy(state->search_term);
    state->search_term = term;
    return FOLIO_STATE_READY;
}

/* 絞り込みを解く。語も一致集合も捨て、索引は台帳のとおりに戻る。 */
static void clear_index_filter(struct folio_state *_Nonnull state)
{
    utf8_text_destroy(state->index_term);
    index_filter_destroy(state->filter);
    state->index_term = nullptr;
    state->filter = nullptr;
}

/* 新しい語と一致集合を受け取り、前のものと入れ替える。 */
static void adopt_index_filter(struct folio_state *_Nonnull state, struct utf8_text *_Nonnull term,
                               struct index_filter *_Nonnull filter)
{
    clear_index_filter(state);
    state->index_term = term;
    state->filter = filter;
}

enum folio_state_outcome folio_state_set_index_filter(struct folio_state *_Nonnull state,
                                                      const char16_t *_Nonnull units, size_t count)
{
    if (count == 0)
    {
        clear_index_filter(state);
        state->scroll = 0;
        settle_cursor(state);
        return FOLIO_STATE_READY;
    }
    struct utf8_text *_Nullable term = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(units, count, &term);
    if (converted != UTF8_TEXT_CONVERTED)
    {
        return converted == UTF8_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                    : FOLIO_STATE_SEARCH_MALFORMED;
    }
    struct index_filter *_Nullable filter = nullptr;
    enum folio_state_outcome built = load_corpus(state);
    if (built == FOLIO_STATE_READY)
    {
        built = build_filter(state, term, &filter);
    }
    if (built != FOLIO_STATE_READY)
    {
        utf8_text_destroy(term);
        return built;
    }
    adopt_index_filter(state, term, filter);
    /* 語を変えるたびに頭から見せ、消えた行のカーソルを最初に見える行へ移す（決定 5）。 */
    state->scroll = 0;
    settle_cursor(state);
    return FOLIO_STATE_READY;
}

bool folio_state_filtering(const struct folio_state *_Nonnull state)
{
    return state->filter != nullptr;
}

const char *_Nonnull folio_state_index_filter_term(const struct folio_state *_Nonnull state)
{
    return state->index_term == nullptr ? "" : utf8_text_bytes(state->index_term);
}

size_t folio_state_index_filter_count(const struct folio_state *_Nonnull state)
{
    return state->filter == nullptr ? total_notes(state) : index_filter_count(state->filter);
}

const char *_Nonnull folio_state_search_term(const struct folio_state *_Nonnull state)
{
    return state->search_term == nullptr ? "" : utf8_text_bytes(state->search_term);
}

size_t folio_state_search_term_length(const struct folio_state *_Nonnull state)
{
    return state->search_term == nullptr ? 0 : utf8_text_length(state->search_term);
}

enum search_direction folio_state_search_direction(const struct folio_state *_Nonnull state)
{
    return state->search_direction;
}

void folio_state_set_search_direction(struct folio_state *_Nonnull state,
                                      enum search_direction direction)
{
    state->search_direction = direction;
}

const char *_Nonnull folio_state_pane_text(const struct folio_state *_Nonnull state)
{
    return note_text_bytes(state->body);
}

size_t folio_state_pane_text_length(const struct folio_state *_Nonnull state)
{
    return note_text_length(state->body);
}

const char *_Nonnull folio_state_document_name(const struct folio_state *_Nonnull state)
{
    if (state->document != FOLIO_DOCUMENT_NAMED)
    {
        return "";
    }
    return note_ledger_name(state->notes[state->selected_category], state->selected_note);
}

bool folio_state_rename_pending(const struct folio_state *_Nonnull state,
                                struct rename_view *_Nonnull out)
{
    if (state->rename == nullptr)
    {
        return false;
    }
    out->from = note_rename_from(state->rename);
    out->to = note_rename_to(state->rename);
    return true;
}

struct pane_title_view folio_state_pane_title(const struct folio_state *_Nonnull state)
{
    struct pane_title_view title = {.any = false,
                                    .recovering = false,
                                    .ordinal = 0,
                                    .category = "",
                                    .note = "",
                                    .color = {0, 0, 0}};
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return title;
    }
    title.any = true;
    /* 未完了の改名があることだけを伝える。言い換えの文言は UI が持つ（ADR 0022 の決定 2）。 */
    title.recovering = state->rename != nullptr;
    title.ordinal = state->selected_category + 1;
    title.category = category_ledger_name(state->categories, state->selected_category);
    title.note =
        state->document == FOLIO_DOCUMENT_UNTITLED
            ? "無題（未保存）"
            : note_ledger_name(state->notes[state->selected_category], state->selected_note);
    title.color = category_ledger_color(state->categories, state->selected_category);
    return title;
}

const char *_Nonnull folio_state_pane_rtf(const struct folio_state *_Nonnull state)
{
    return markdown_rtf_text(state->pane);
}

size_t folio_state_pane_rtf_length(const struct folio_state *_Nonnull state)
{
    return markdown_rtf_length(state->pane);
}

/* 値ごとの失敗の 1 行（ADR 0027 の決定 1）。列挙の全値がちょうど 1 度ずつ並ぶことは CNF-009 が
 * 字句で守るので、添字の範囲検査は書かない（決定 3）。 */
static const char *_Nonnull const failure_lines[] = {
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

const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome)
{
    return failure_lines[outcome];
}
void folio_state_destroy(struct folio_state *_Nullable state)
{
    if (state == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < state->notes_count; ++index)
    {
        note_ledger_destroy(state->notes[index]);
    }
    free(state->notes);
    /* 未完了の意図は永続的な記録の側に残る。ここで捨てるのはメモリだけ（ADR 0022 の決定 7）。 */
    note_rename_destroy(state->rename);
    category_ledger_destroy(state->categories);
    markdown_rtf_destroy(state->pane);
    note_text_destroy(state->body);
    utf8_text_destroy(state->search_term);
    utf8_text_destroy(state->index_term);
    index_filter_destroy(state->filter);
    note_corpus_destroy(state->corpus);
    folio_settings_destroy(state->settings);
    /* 下見は次の下見か破棄まで残る。捨てる契機は数え上げない（ADR 0028 の決定 6）。 */
    replace_preview_destroy(state->preview);
    free(state->found);
    free(state);
}
