#include "folio_state.h"

#include "ui_text.h"

#include "appearance_port.h"
#include "category_ledger.h"
#include "category_name.h"
#include "clipboard_port.h"
#include "discovery_source.h"
#include "discovery_target.h"
#include "drawer_layout.h"
#include "folio_settings.h"
#include "image_export.h"
#include "index_filter.h"
#include "index_filter_build.h"
#include "index_filter_progress.h"
#include "markdown_images.h"
#include "markdown_media.h"
#include "markdown_rtf.h"
#include "markdown_source_lookup.h"
#include "name_list.h"
#include "note_archive.h"
#include "note_corpus.h"
#include "note_excerpt.h"
#include "note_file_times.h"
#include "note_history.h"
#include "note_identity.h"
#include "note_ledger.h"
#include "note_metadata.h"
#include "note_name.h"
#include "note_replace.h"
#include "note_save_mode.h"
#include "note_source.h"
#include "note_store_request.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "persistence_port.h"
#include "regex_matches.h"
#include "regex_port.h"
#include "regex_request.h"
#include "rename_plan.h"
#include "replace_edit.h"
#include "replace_preview.h"
#include "rtf_palette.h"
#include "storage_path.h"
#include "tag_catalog.h"
#include "tag_edit_snapshot.h"
#include "tag_update.h"
#include "tag_update_request.h"
#include "text_buffer.h"
#include "transfer_port.h"
#include "ui_font.h"
#include "utf8_text.h"
#include "web_link_port.h"
#include "web_uri.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t index_matching_work_budget = 65536;

/* 走査の入れ物の最初の大きさ。足りなければ総数まで広げて 1 度だけ走査し直す（ADR 0028 の決定 2）。
 */
constexpr size_t initial_matches = 64;

struct folio_state
{
    struct note_file_times *_Nullable file_times;
    enum note_times_state times_state;
    struct persistence_port port;
    struct web_link_port web_link;
    struct clipboard_port clipboard;
    struct transfer_port transfer;
    struct image_port images;
    uint64_t image_origin_revision;
    /* 正規表現ポート（ADR 0028 の決定 1 / 2）。ICU を知るのは adapter だけである */
    struct regex_port regex;
    /* 走査の入れ物。下見が引き取ったら空（null・0）に戻り、次の走査で確保し直す（補正 25）。 */
    struct regex_match *_Nullable found;
    size_t found_capacity;
    struct replace_preview *_Nullable preview; /* 直前の下見。1 つだけ持つ（決定 6） */
    size_t replace_offset; /* 直前の下見が BAD_PATTERN だったときの位置（1 起算） */
    /* 外観ポート（ADR 0031 の決定 3）。OS の変更で読み直すので値で写して持つ */
    struct appearance_port appearance;
    struct appearance_snapshot appearance_snapshot; /* 最後に採用できたOS外観（ADR0057）。 */
    enum folio_state_outcome appearance_notice;
    uint64_t palette_revision;                    /* 実効配色変更時だけ進むUI cacheの無効化番号。 */
    enum folio_theme_choice display_theme_choice; /* 保存前の候補も含む表示選択（ADR0061）。 */
    /* data/settings.json の値の所有者（ADR 0025 の決定 5）。読めなければ既定値のまま */
    struct folio_settings *_Nullable settings;
    /* READY か SETTINGS_UNREADABLE。後者のあいだ設定の変更を断り、上書きもしない（決定 6） */
    enum folio_state_outcome settings_notice;
    /* READY か NOTE_UNEDITABLE。直前の select_note が編集から閲覧へ落としたかの事実（#258） */
    enum folio_state_outcome select_notice;
    bool ledger_notice; /* #1069: a list was read again and differed; taken by the window */
    uint64_t adoptions; /* how many times a list read again took the place of the one in memory */
    struct theme_palette palette;
    struct category_ledger *_Nullable categories;
    struct note_ledger *_Nonnull *_Nullable notes; /* categories と同じ数・同じ順 */
    size_t notes_count;
    struct markdown_rtf *_Nullable pane; /* 選択中のノートの表示値。無ければ空の文書 */
    struct markdown_rtf *_Nullable edit; /* 編集用の平文 RTF（#250）。作るまでは null */
    struct note_text *_Nullable body;    /* 最後に読んだ本文。何も選んでいなければ空 */
    enum pane_mode mode;
    enum folio_layout_mode layout_mode;
    bool outline; /* #975 / ADR0114: the headings column is open, while the app runs. */
    int scroll;   /* ドロワーのスクロール量（要求量・画素・0 以上）。上限は core が決める */
    enum folio_document_kind document;
    size_t selected_category;
    size_t selected_note;
    bool index_pending; /* 初回作成後に同期できなかった台帳。次の保存・変更前に再試行 */
    size_t pending_category;
    /* 記録を公開したまま完了していない改名の意図。1 つだけ持つ（ADR 0022 の決定 2 / 7） */
    struct rename_plan *_Nullable rename;
    size_t rename_category;
    /* CATEGORY意図に結び付いた空corpus。公開前に確保し、完了時に交換する（ADR0041）。 */
    struct note_corpus *_Nullable rename_corpus;
    /* ノート内検索の語（UTF-8）と直前の方向。絞り込みの語とは別（ADR 0023 の決定 3） */
    struct utf8_text *_Nullable search_term;
    enum search_direction search_direction;
    /* 全ノートの絞り込み（ADR 0024）。語と一致集合の所有者で、どちらも永続化しない */
    struct utf8_text *_Nullable index_term;
    struct note_tags
        *_Nullable index_tags; /* Omitted means no tag restriction, independent of catalog. */
    enum tag_match_mode tag_mode;
    struct tag_catalog *_Nullable catalog;
    enum tag_catalog_status catalog_status;
    struct tag_edit_snapshot *_Nullable tag_edit;
    struct note_tags *_Nullable edit_tags;
    struct note_tag *_Nullable tag_from;
    struct note_tag *_Nullable tag_to;
    struct tag_update *_Nullable *_Nullable tag_updates;
    enum tag_apply_status *_Nullable tag_results;
    size_t tag_total;
    size_t tag_prepared;
    size_t tag_next;
    uint64_t tag_rows;
    enum tag_management_phase tag_phase;
    enum folio_state_outcome tag_notice;
    bool tag_merging;
    struct note_text *_Nullable tag_current_body;
    struct markdown_rtf *_Nullable tag_current_view;
    struct markdown_rtf *_Nullable tag_current_edit;
    struct index_filter *_Nullable filter;
    enum note_scope note_scope;
    /* 準備中の最新要求と次の読込位置（ADR0068）。部分corpusを結果へ公開しない。 */
    struct utf8_text *_Nullable index_request;
    struct index_filter_build *_Nullable index_build;
    struct note_ref index_cursor;
    struct utf8_text *_Nullable discovery_query;
    struct note_tags *_Nullable discovery_tags;
    enum tag_match_mode discovery_tag_mode;
    enum note_scope discovery_scope;
    enum note_order discovery_order;
    enum discovery_phase discovery_phase;
    enum discovery_mode discovery_mode;
    enum discovery_preview discovery_preview;
    enum folio_state_outcome discovery_notice;
    struct index_filter_build *_Nullable discovery_build;
    struct index_filter *_Nullable discovery_results;
    struct note_ref discovery_cursor;
    struct note_identity *_Nullable discovery_identity;
    struct note_excerpt *_Nullable discovery_excerpt;
    enum discovery_source *_Nullable discovery_sources;
    size_t discovery_source_count;
    size_t discovery_source_checked;
    size_t discovery_unreadable;
    size_t discovery_unsupported;
    size_t discovery_selected;
    uint64_t discovery_revision;
    uint64_t discovery_results_revision;
    bool discovery_refresh;
    bool discovery_corpus_started; /* This session has requested the current fresh shared pass. */
    size_t corpus_category;
    size_t corpus_note;
    struct note_corpus *_Nullable corpus; /* 本文の写し。最初の絞り込みで 1 回だけ読む */
    bool corpus_loaded;
    /* trueならNAMEDかつcorpus_loaded。対象は現在文書の保存先で、state.bodyは保存済み正本。
     * 文書/保存先を変える前の共通同期で修復するため、別の本文・名前・添字を所有しない（#219）。 */
    bool copy_pending;
    bool cursor_any; /* 選択とは独立した索引の行参照（ADR0055/0056）。 */
    enum folio_cursor_kind cursor_kind;
    size_t cursor_category;
    size_t cursor_note;       /* FOLIO_CURSOR_NOTEのときだけ使う。 */
    uint64_t index_revision;  /* 行配置/名前が変わるたびに更新する。 */
    uint64_t row_revision;    /* 並べ替え等で行参照が別の意味になる改訂。開閉だけでは変えない。 */
    uint64_t ledger_revision; /* Saved-note identities/order, excluding filter projections. */
    uint64_t store_revision;  /* md を書き戻す・作るたびに進む（保存の成功の印・#349）。 */
    /* 開いている履歴の一覧（ADR 0038 の決定 7）。版番号の昇順に history_count 行。
     * 本文が null の行は読めない版。閉じる・別の文書へ移る・保存するときに捨てる（決定 10） */
    struct note_text *_Nullable history[note_history_depth];
    size_t history_versions[note_history_depth];
    size_t history_count;
};

static void document_changed(struct folio_state *_Nonnull state, enum folio_document_kind kind)
{
    folio_state_close_tag_edit(state);
    state->document = kind;
}

/* 絞り込みの作り直しとカーソルの着地は、写しを付け替える経路（改名）より後に書いてあるので
 * ここで名前だけ先に出す。定義は 1 つずつで、経路は増やさない（ARC-001）。 */
static struct drawer_source source_of(const struct folio_state *_Nonnull state);
static enum folio_state_outcome refresh_filter(struct folio_state *_Nonnull state);
static enum folio_state_outcome refresh_copy(struct folio_state *_Nonnull state);
static enum folio_state_outcome prepare_tag_management_note(struct folio_state *_Nonnull state);
static void invalidate_discovery(struct folio_state *_Nonnull state);
static void refresh_discovery(struct folio_state *_Nonnull state);
static void observe_discovery_source(struct folio_state *_Nonnull state, struct note_ref ref);
static bool discovery_uses_body(const struct folio_state *_Nonnull state);
static void discovery_fresh_pass(struct folio_state *_Nonnull state);
static bool discovery_source_reused(const struct folio_state *_Nonnull state, struct note_ref ref);
static void invalidate_corpus_dependents(struct folio_state *_Nonnull state);
static void fresh_tag_corpus(struct folio_state *_Nonnull state);
static enum folio_state_outcome refresh_notes(struct folio_state *_Nonnull state, size_t category,
                                              size_t *_Nullable first, size_t *_Nullable second);
static bool holds_category(const struct folio_state *_Nonnull state, size_t category);
static struct drawer_cursor first_stop_in(const struct folio_state *_Nonnull state,
                                          size_t category);

void folio_state_refresh_note_times(struct folio_state *_Nonnull state)
{
    note_file_times_destroy(state->file_times);
    state->file_times = nullptr;
    switch (state->document)
    {
    case FOLIO_DOCUMENT_NONE:
        state->times_state = NOTE_TIMES_NONE;
        return;
    case FOLIO_DOCUMENT_UNTITLED:
        state->times_state = NOTE_TIMES_UNSAVED;
        return;
    case FOLIO_DOCUMENT_NAMED:
        break;
    }
    state->times_state = NOTE_TIMES_UNAVAILABLE;
    if (state->rename != nullptr)
        return;
    struct note_file_times *_Nullable times = nullptr;
    enum persistence_outcome read = state->port.read_note_times(
        state->port.adapter, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note), &times);
    if (read == PERSISTENCE_LOADED)
    {
        state->file_times = times;
        state->times_state = NOTE_TIMES_READY;
    }
}

struct note_times_view folio_state_note_times(const struct folio_state *_Nonnull state)
{
    enum note_times_state kind = state->rename != nullptr && state->document == FOLIO_DOCUMENT_NAMED
                                     ? NOTE_TIMES_UNAVAILABLE
                                     : state->times_state;
    return (struct note_times_view){
        .state = kind,
        .created = kind == NOTE_TIMES_READY ? note_file_times_created(state->file_times) : "",
        .updated = kind == NOTE_TIMES_READY ? note_file_times_updated(state->file_times) : ""};
}

static void discard_view(struct folio_state *_Nonnull state)
{
    state->images.cancel(state->images.adapter);
    markdown_rtf_destroy(state->pane);
    state->pane = nullptr;
}
static void image_origin_changed(struct folio_state *_Nonnull state)
{
    state->images.cancel(state->images.adapter);
    ++state->image_origin_revision;
}
static void layout_changed(struct folio_state *_Nonnull state)
{
    if (state->index_revision != UINT64_MAX)
    {
        state->index_revision += 1;
    }
}

/* 検証とportへの受け渡しはここだけ。ノート内リンクと製品リンクが共有する（ADR0079/0097）。 */
static enum folio_state_outcome launch_web_link(struct folio_state *_Nonnull state,
                                                const char *_Nonnull bytes, size_t length)
{
    struct web_uri *_Nullable uri = nullptr;
    switch (web_uri_create(bytes, length, &uri))
    {
    case WEB_URI_ACCEPTED:
        break;
    case WEB_URI_REJECTED:
        return FOLIO_STATE_WEB_LINK_REJECTED;
    case WEB_URI_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum web_link_outcome outcome = state->web_link.open(uri);
    web_uri_destroy(uri);
    switch (outcome)
    {
    case WEB_LINK_OPENED:
        return FOLIO_STATE_READY;
    case WEB_LINK_UNAVAILABLE:
        return FOLIO_STATE_WEB_LINK_UNAVAILABLE;
    case WEB_LINK_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_WEB_LINK_UNAVAILABLE;
}

enum folio_state_outcome folio_state_open_web_link(struct folio_state *_Nonnull state,
                                                   const char *_Nonnull bytes, size_t length)
{
    if (state->mode != PANE_MODE_VIEW || state->document == FOLIO_DOCUMENT_NONE)
        return FOLIO_STATE_WEB_LINK_REJECTED;
    return launch_web_link(state, bytes, length);
}

enum folio_state_outcome folio_state_open_product_link(struct folio_state *_Nonnull state,
                                                       enum product_link link)
{
    return launch_web_link(state, product_link_uri(link), product_link_uri_length(link));
}

/* The clipboard port's answer as a state outcome; the one place both copies share. */
static enum folio_state_outcome copied(enum clipboard_outcome outcome)
{
    switch (outcome)
    {
    case CLIPBOARD_WRITTEN:
        return FOLIO_STATE_READY;
    case CLIPBOARD_BUSY:
        return FOLIO_STATE_CLIPBOARD_BUSY;
    case CLIPBOARD_UNAVAILABLE:
        return FOLIO_STATE_CLIPBOARD_UNAVAILABLE;
    case CLIPBOARD_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_CLIPBOARD_UNAVAILABLE;
}

enum folio_state_outcome folio_state_copy_web_link(struct folio_state *_Nonnull state,
                                                   const char *_Nonnull bytes, size_t length)
{
    if (state->mode != PANE_MODE_VIEW || state->document == FOLIO_DOCUMENT_NONE)
        return FOLIO_STATE_WEB_LINK_REJECTED;
    struct web_uri *_Nullable uri = nullptr;
    switch (web_uri_create(bytes, length, &uri))
    {
    case WEB_URI_ACCEPTED:
        break;
    case WEB_URI_REJECTED:
        return FOLIO_STATE_WEB_LINK_REJECTED;
    case WEB_URI_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome outcome = copied(
        state->clipboard.write(state->clipboard.adapter, web_uri_bytes(uri), web_uri_length(uri)));
    web_uri_destroy(uri);
    return outcome;
}

enum folio_state_outcome folio_state_copy_code(struct folio_state *_Nonnull state, size_t slot)
{
    const struct markdown_media *media = folio_state_media(state);
    if (state->mode != PANE_MODE_VIEW || media == nullptr ||
        slot >= markdown_media_code_count(media))
        return FOLIO_STATE_CODE_COPY_REJECTED;
    return copied(state->clipboard.write(state->clipboard.adapter,
                                         markdown_media_code_bytes(media, slot),
                                         markdown_media_code_length(media, slot)));
}

bool folio_state_source_mapped(const struct folio_state *_Nonnull state, size_t view_total)
{
    const struct markdown_source_map *map =
        state->pane == nullptr ? nullptr : markdown_rtf_source_map(state->pane);
    return state->mode == PANE_MODE_VIEW && state->document != FOLIO_DOCUMENT_NONE &&
           map != nullptr && markdown_source_map_count(map) > 0 &&
           markdown_source_map_total(map) == view_total;
}

enum folio_state_outcome folio_state_source_caret(struct folio_state *_Nonnull state,
                                                  size_t ordinal, size_t view_total,
                                                  size_t *_Nonnull native)
{
    if (!folio_state_source_mapped(state, view_total) ||
        note_text_editability(state->body) == NOTE_TEXT_EDITOR_ALTERS)
        return FOLIO_STATE_SOURCE_UNMAPPED;
    size_t caret =
        markdown_source_lookup_caret(markdown_rtf_source_map(state->pane), state->body, ordinal);
    if (caret == markdown_source_none)
        return FOLIO_STATE_SOURCE_UNMAPPED;
    *native = note_source_units(note_text_bytes(state->body), caret, NOTE_SOURCE_PARAGRAPH);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_copy_markdown(struct folio_state *_Nonnull state,
                                                   struct markdown_source_selection selection,
                                                   size_t view_total)
{
    if (!folio_state_source_mapped(state, view_total))
        return FOLIO_STATE_SOURCE_UNMAPPED;
    struct text_buffer *_Nullable buffer = nullptr;
    if (text_buffer_create(&buffer) != TEXT_BUFFER_ACCEPTED)
        return FOLIO_STATE_OUT_OF_MEMORY;
    enum folio_state_outcome outcome = FOLIO_STATE_SOURCE_UNMAPPED;
    if (markdown_source_lookup_copy(markdown_rtf_source_map(state->pane), state->body, selection,
                                    buffer))
        outcome =
            text_buffer_finish(buffer) != TEXT_BUFFER_ACCEPTED
                ? FOLIO_STATE_OUT_OF_MEMORY
                : copied(state->clipboard.write(state->clipboard.adapter, text_buffer_bytes(buffer),
                                                text_buffer_length(buffer)));
    text_buffer_destroy(buffer);
    return outcome;
}

static void stored_note(struct folio_state *_Nonnull state)
{
    refresh_discovery(state);
    if (state->store_revision != UINT64_MAX)
    {
        state->store_revision += 1;
    }
}

static void discard_index_build(struct folio_state *_Nonnull state)
{
    index_filter_build_destroy(state->index_build);
    state->index_build = nullptr;
    state->index_cursor = (struct note_ref){0};
}

static void restart_corpus_load(struct folio_state *_Nonnull state)
{
    discard_index_build(state);
    if (!state->corpus_loaded)
    {
        state->corpus_category = 0;
        state->corpus_note = 0;
    }
}

static void row_mapping_changed(struct folio_state *_Nonnull state)
{
    restart_corpus_load(state);
    layout_changed(state);
    if (state->row_revision != UINT64_MAX)
    {
        state->row_revision += 1;
    }
}

static void index_changed(struct folio_state *_Nonnull state)
{
    refresh_discovery(state);
    row_mapping_changed(state);
    if (state->ledger_revision != UINT64_MAX)
        state->ledger_revision += 1;
}

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

/* 保留の index.json を書く。失敗は LEDGER_UNSYNCED で、保留のまま残る。 */
static enum folio_state_outcome write_pending(struct folio_state *_Nonnull state)
{
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

/* このアプリがフォルダを変えた直後（ノートを作った・ごみ箱へ送った・移した）に、その index.json
 * を書く。 #1069 / ADR0117 (stage 4): what this app did is already in the folder, so reading the
 * index and the folder again gives the ledger to write: the outside order and stars, the outside
 * notes, and this app's own change. A ledger that cannot be read again is not overwritten with the
 * one in memory: LEDGER_UNSYNCED, and the write waits. */
static enum folio_state_outcome synchronize_index(struct folio_state *_Nonnull state)
{
    if (!state->index_pending)
    {
        return FOLIO_STATE_READY;
    }
    if (refresh_notes(state, state->pending_category, nullptr, nullptr) != FOLIO_STATE_READY)
    {
        return FOLIO_STATE_LEDGER_UNSYNCED;
    }
    return write_pending(state);
}

/* 前回書けなかった index.json を、次の意図より先に修復する。書けなければ LEDGER_UNSYNCED で、
 * 呼び出し側はまだ何も実行していないので、意図ごと拒む（従来どおり）。
 * 読み直せないあいだは書かずに待ち、意図は止めない（review of #1069, stage 4）: 待っている台帳に
 * 載っていないのは、このアプリが最後に作った・消したノートだけで、それは次に読むときにフォルダから
 * わかる。壊れた index.json ひとつで、開いているノートの保存まで止めない。 */
static enum folio_state_outcome retry_index(struct folio_state *_Nonnull state)
{
    if (!state->index_pending)
    {
        return FOLIO_STATE_READY;
    }
    if (refresh_notes(state, state->pending_category, nullptr, nullptr) != FOLIO_STATE_READY)
    {
        return FOLIO_STATE_READY;
    }
    return write_pending(state);
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
    struct rename_plan *_Nonnull intent = state->rename;
    if (!state->corpus_loaded)
    {
        return FOLIO_STATE_READY;
    }
    if (note_corpus_rename(
            state->corpus, category_ledger_name(state->categories, state->rename_category),
            rename_plan_from(intent), rename_plan_to(intent)) != NOTE_CORPUS_ACCEPTED)
    {
        state->corpus_loaded = false;
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_READY;
}

/* 名前も判定の対象なので、写しを付け替えたら一致集合も作り直す。どの意図が改名を完了させても
 * （`apply_rename` でも `resume_rename` でも）同じ結果になるよう、ここ 1 か所で行う
 * （ADR 0024 の補正 3）。絞り込んでいなければ `refresh_filter` は何もしない。 */
static enum folio_state_outcome adopt_note_rename(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome copied = recopy_rename(state);
    struct rename_plan *_Nonnull intent = state->rename;
    struct note_ledger *_Nonnull renamed = rename_plan_take_note_ledger(intent);
    note_ledger_destroy(state->notes[state->rename_category]);
    state->notes[state->rename_category] = renamed;
    index_changed(state);
    state->rename = nullptr;
    rename_plan_destroy(intent);
    folio_state_refresh_note_times(state);
    return copied != FOLIO_STATE_READY ? copied : refresh_filter(state);
}

static enum folio_state_outcome adopt_category_rename(struct folio_state *_Nonnull state)
{
    image_origin_changed(state);
    struct rename_plan *_Nonnull intent = state->rename;
    category_ledger_destroy(state->categories);
    state->categories = rename_plan_take_category_ledger(intent);
    note_corpus_destroy(state->corpus);
    state->corpus = state->rename_corpus;
    state->rename_corpus = nullptr;
    state->corpus_loaded = false;
    index_changed(state);
    if (state->document != FOLIO_DOCUMENT_NONE &&
        state->selected_category == state->rename_category)
    {
        replace_preview_destroy(state->preview);
        state->preview = nullptr;
    }
    state->rename = nullptr;
    rename_plan_destroy(intent);
    folio_state_refresh_note_times(state);
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome adopt_rename(struct folio_state *_Nonnull state)
{
    switch (rename_plan_kind(state->rename))
    {
    case RENAME_KIND_NOTE:
        return adopt_note_rename(state);
    case RENAME_KIND_CATEGORY:
        return adopt_category_rename(state);
    }
    return FOLIO_STATE_RENAME_HALTED;
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
        state->port.apply_rename(state->port.adapter, state->rename, RENAME_RESUME);
    if (moved != RENAME_COMPLETED)
    {
        /* どの理由でも意図は捨てない。捨てられるのは完了したときだけ。 */
        return from_rename(moved);
    }
    return adopt_rename(state);
}

/* 保存・切替・並替・色・新規・別名保存・終了確認が共有する唯一の同期の入口。
 * index.json→保存済み本文の写し→未完了の改名の順に片付けてから意図へ進む（#219）。 */
static enum folio_state_outcome synchronize(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome synced = retry_index(state);
    if (synced == FOLIO_STATE_READY && state->copy_pending)
    {
        synced = refresh_copy(state);
    }
    return synced == FOLIO_STATE_READY ? resume_rename(state) : synced;
}

enum folio_state_outcome folio_state_retry_rename(struct folio_state *_Nonnull state)
{
    return synchronize(state);
}

/* For an intent that is given a note by its place in the list. If the repairs above took a
 * list read again, the places the caller counted are of the old list: the intent is refused
 * with LEDGER_CHANGED_OUTSIDE, the list is the fresh one and the user points again (review of
 * #1069, stage 4). A rename finished here keeps every place and does not count. */
static enum folio_state_outcome synchronize_places(struct folio_state *_Nonnull state)
{
    uint64_t before = state->adoptions;
    enum folio_state_outcome synced = synchronize(state);
    if (synced == FOLIO_STATE_READY && state->adoptions != before)
    {
        return FOLIO_STATE_LEDGER_CHANGED_OUTSIDE;
    }
    return synced;
}

static bool category_rename_pending(const struct folio_state *_Nonnull state)
{
    return state->rename != nullptr && rename_plan_kind(state->rename) == RENAME_KIND_CATEGORY;
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

/* #1069 / ADR0117: the open note stays in the list though its file is gone outside (saving
 * makes it again; opening it says it cannot be read). */
static enum folio_state_outcome keep_name(struct name_list *_Nonnull scanned,
                                          const char *_Nullable kept)
{
    if (kept == nullptr || name_list_contains(scanned, kept))
        return FOLIO_STATE_READY;
    return name_list_append(scanned, kept, strlen(kept)) == NAME_LIST_ACCEPTED
               ? FOLIO_STATE_READY
               : FOLIO_STATE_OUT_OF_MEMORY;
}

/* 1 カテゴリの index.json（無ければ空）と md の走査結果を照合する。kept は走査に無くても残す名前。
 */
static enum folio_state_outcome load_notes(struct note_ledger *_Nullable *_Nonnull slot,
                                           const struct persistence_port *_Nonnull port,
                                           const char *_Nonnull category,
                                           const char *_Nullable kept)
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
        outcome = keep_name(scanned, kept);
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
            load_notes(&notes, port, category_ledger_name(state->categories, index), nullptr);
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

/* 閲覧の RTF へ渡す、言語から決まる値（本文の face とコピー操作の文言の言語・ADR0098）。 */
static struct markdown_rtf_locale view_locale(enum folio_language language)
{
    return (struct markdown_rtf_locale){.face = ui_font_face(language), .language = language};
}

/* いまの文書と同じ作り方で閲覧の RTF を 1 つ作る（差し替えはしない・ADR 0031 の決定 3）。
 * 名前のある文書だけが本文を持ち、無題と何も選んでいない状態は空の文書である。
 * **言語は引数で受ける**（ADR 0032 の決定 5）。採用前の新しい言語でも作れるようにするため、
 * state からは引かない。 */
static bool render_pane_document(const struct folio_state *_Nonnull state,
                                 const struct theme_palette *_Nonnull palette,
                                 enum folio_language language,
                                 struct markdown_rtf *_Nullable *_Nonnull out)
{
    const char *_Nonnull face = ui_font_face(language);
    if (state->document == FOLIO_DOCUMENT_NAMED)
    {
        return markdown_rtf_create(state->body, palette, view_locale(language), out) ==
               MARKDOWN_RTF_CONVERTED;
    }
    return markdown_rtf_empty(palette, face, out) == MARKDOWN_RTF_CONVERTED;
}

/* 設定を読んだ後に選択を採り、palette と閲覧文書を解決したテーマの色で作り直す（決定 3）。
 * 先に新しい文書を作ってから差し替えるので、確保に失敗しても状態は変わらない。 */
static enum folio_state_outcome adopt_theme_choice(struct folio_state *_Nonnull state)
{
    enum folio_theme_choice choice = folio_settings_theme(state->settings);
    struct theme_palette palette = appearance_snapshot_palette(&state->appearance_snapshot, choice);
    struct markdown_rtf *_Nullable pane = nullptr;
    if (!render_pane_document(state, &palette, folio_state_language(state), &pane))
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    discard_view(state);
    state->pane = pane;
    state->palette = palette;
    state->display_theme_choice = choice;
    return FOLIO_STATE_READY;
}

/* ポートは値で写して持つ（ADR 0029・ADR 0031 の決定 3）。adapter の寿命は合成ルートが保証する。 */
static void adopt_ports(struct folio_state *_Nonnull state,
                        const struct folio_ports *_Nonnull ports)
{
    state->port = *ports->persistence;
    state->regex = *ports->regex;
    state->web_link = *ports->web_link;
    state->clipboard = *ports->clipboard;
    state->transfer = *ports->transfer;
    state->images = *ports->images;
    state->appearance = *ports->appearance;
}

enum folio_state_outcome folio_state_create(const struct folio_ports *_Nonnull ports,
                                            struct folio_state *_Nullable *_Nonnull out)
{
    const struct persistence_port *_Nonnull persistence = ports->persistence;
    const struct appearance_port *_Nonnull appearance = ports->appearance;
    struct folio_state *_Nullable state = calloc(1, sizeof *state);
    if (state == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    adopt_ports(state, ports);
    state->appearance_snapshot.theme = FOLIO_THEME_LIGHT;
    state->appearance_notice =
        appearance->read(appearance->adapter, &state->appearance_snapshot) == APPEARANCE_READY
            ? FOLIO_STATE_READY
            : FOLIO_STATE_APPEARANCE_UNAVAILABLE;
    state->display_theme_choice = FOLIO_THEME_CHOICE_SYSTEM;
    state->layout_mode = FOLIO_LAYOUT_BOTH;
    state->select_notice = FOLIO_STATE_READY;
    state->palette =
        appearance_snapshot_palette(&state->appearance_snapshot, state->display_theme_choice);
    if (markdown_rtf_empty(&state->palette, ui_font_face(FOLIO_LANGUAGE_JA), &state->pane) !=
            MARKDOWN_RTF_CONVERTED ||
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
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = adopt_theme_choice(state);
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

/* いまのカーソル。本文の選択を参照せず、独立した行参照を返す（ADR0055）。 */
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
        *out = on_note(state->cursor_category, state->cursor_note);
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
    state->cursor_category = state->selected_category;
    state->cursor_note = state->selected_note;
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
    struct drawer_source source = source_of(state);
    return drawer_source_shown_category(&source, category);
}

/* 絞り込んでいるあいだは、一致したノートだけが行になる。 */
static bool shown_note(const struct folio_state *_Nonnull state, size_t category, size_t note)
{
    struct drawer_source source = source_of(state);
    return drawer_source_shown_note(&source, located(category, note));
}

/* 絞り込んでいるあいだは台帳の expanded に関わらず展開して見える。 */
static bool shown_expanded(const struct folio_state *_Nonnull state, size_t category)
{
    struct drawer_source source = source_of(state);
    return drawer_source_expanded(&source, category);
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

static bool index_note_count(const struct folio_state *_Nonnull state, size_t *_Nonnull out)
{
    size_t count = 0;
    for (size_t category = 0; category < state->notes_count; ++category)
    {
        size_t notes = note_ledger_count(state->notes[category]);
        if (notes > SIZE_MAX - count)
            return false;
        count += notes;
    }
    *out = count;
    return true;
}

/* Initial O(categories) count, checked before constructing the only matching owner. */
static enum folio_state_outcome
create_index_build(const struct folio_state *_Nonnull state,
                   const struct index_filter_query *_Nonnull query,
                   struct index_filter_build *_Nullable *_Nonnull out)
{
    size_t count = 0;
    if (!index_note_count(state, &count))
        return FOLIO_STATE_OUT_OF_MEMORY;
    return from_index_filter(index_filter_build_create(query, count, out));
}

static struct index_filter_query list_query(const struct folio_state *_Nonnull state,
                                            const struct utf8_text *_Nonnull term)
{
    return (struct index_filter_query){.term = utf8_text_bytes(term),
                                       .length = utf8_text_length(term),
                                       .tags = state->index_tags,
                                       .mode = state->tag_mode};
}

/* Only this call borrows name/body. No entry array or pointer survives a step. */
static const struct note_tags *_Nullable available_tags(const struct note_text *_Nullable body)
{
    if (body == nullptr)
        return nullptr;
    const struct note_metadata *_Nonnull metadata = note_text_metadata(body);
    return note_metadata_available(metadata) ? note_metadata_tags(metadata) : nullptr;
}

static struct index_filter_entry filter_entry(const struct folio_state *_Nonnull state,
                                              struct note_ref ref, bool uses_body)
{
    const char *_Nonnull folder = category_ledger_name(state->categories, ref.category);
    const char *_Nonnull name = note_ledger_name(state->notes[ref.category], ref.note);
    const struct note_text *_Nullable body =
        uses_body ? note_corpus_body(state->corpus, folder, name) : nullptr;
    return (struct index_filter_entry){
        .ref = ref,
        .name = name,
        .body = body == nullptr ? nullptr : note_text_bytes(body),
        .length = body == nullptr ? 0 : note_text_length(body),
        .tags = available_tags(body),
        .category_name = folder,
        .favorite = note_ledger_favorite(state->notes[ref.category], ref.note)};
}

/* Shared by pending idle and call-local synchronous repair; no second iterator/matcher. */
static void step_filter(const struct folio_state *_Nonnull state,
                        struct index_filter_build *_Nonnull build,
                        struct index_filter_progress *_Nonnull progress)
{
    struct note_ref *_Nonnull cursor = &progress->cursor;
    size_t *_Nonnull work = &progress->work;
    while (*work != 0 && index_filter_build_readiness(build) == INDEX_FILTER_NOT_READY)
    {
        size_t count = note_ledger_count(state->notes[cursor->category]);
        if (cursor->note == count)
        {
            cursor->category += 1;
            cursor->note = 0;
            if (count == 0)
                *work -= 1;
            continue;
        }
        struct index_filter_entry entry = filter_entry(state, *cursor, progress->uses_body);
        switch (index_filter_build_step(build, &entry, work))
        {
        case INDEX_FILTER_STEP_MORE:
            return;
        case INDEX_FILTER_STEP_ENTRY_DONE:
            cursor->note += 1;
            break;
        case INDEX_FILTER_STEP_ALL_DONE:
            cursor->note += 1;
            return;
        }
    }
}

static enum folio_state_outcome build_filter(const struct folio_state *_Nonnull state,
                                             const struct utf8_text *_Nonnull term,
                                             struct index_filter *_Nullable *_Nonnull out)
{
    struct index_filter_build *_Nullable build = nullptr;
    struct index_filter_query query = list_query(state, term);
    enum folio_state_outcome created = create_index_build(state, &query, &build);
    if (created != FOLIO_STATE_READY)
        return created;
    struct index_filter_progress progress = {.uses_body = true};
    for (;;)
    {
        progress.work = index_matching_work_budget;
        step_filter(state, build, &progress);
        switch (index_filter_finish(&build, out))
        {
        case INDEX_FILTER_FINISH_NOT_READY:
            break;
        case INDEX_FILTER_FINISH_OUT_OF_MEMORY:
            index_filter_build_destroy(build);
            return FOLIO_STATE_OUT_OF_MEMORY;
        case INDEX_FILTER_FINISH_ACCEPTED:
            return FOLIO_STATE_READY;
        }
    }
}

/* 一回につき一本文だけを既存の写しへ入れる。再試行時も既存copyでreadを省略しない。 */
static enum folio_state_outcome load_corpus_note(struct folio_state *_Nonnull state,
                                                 struct note_ref ref)
{
    const char *_Nonnull folder = category_ledger_name(state->categories, ref.category);
    const char *_Nonnull name = note_ledger_name(state->notes[ref.category], ref.note);
    struct note_text *_Nullable body = nullptr;
    enum persistence_outcome read = state->port.read_note(state->port.adapter, folder, name, &body);
    if (read == PERSISTENCE_OUT_OF_MEMORY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (read != PERSISTENCE_LOADED)
    {
        note_corpus_remove(state->corpus, folder, name);
        observe_discovery_source(state, ref);
        return FOLIO_STATE_READY;
    }
    enum note_corpus_outcome stored = note_corpus_put(state->corpus, folder, name, body);
    note_text_destroy(body);
    if (stored == NOTE_CORPUS_ACCEPTED)
        observe_discovery_source(state, ref);
    return stored == NOTE_CORPUS_ACCEPTED ? FOLIO_STATE_READY : FOLIO_STATE_OUT_OF_MEMORY;
}

/* 空カテゴリを越えて次の一件へ進む。全件終了時だけ完成済みとする（ADR0068）。 */
static bool corpus_next_present(struct folio_state *_Nonnull state)
{
    while (state->corpus_category < state->notes_count)
    {
        if (state->corpus_note < note_ledger_count(state->notes[state->corpus_category]))
        {
            return true;
        }
        state->corpus_category += 1;
        state->corpus_note = 0;
    }
    state->corpus_loaded = true;
    return false;
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
    index_changed(state);
    settle_cursor(state);
    return FOLIO_STATE_READY;
}

/* 配置の入力。絞り込みの有無で経路を分けない（ADR 0024 の決定 3）。 */
static struct drawer_source source_of(const struct folio_state *_Nonnull state)
{
    struct drawer_source source = {
        .categories = state->categories,
        .notes = (const struct note_ledger *_Nonnull const *_Nonnull)state->notes,
        .filter = state->filter,
        .order = folio_settings_note_order(state->settings),
        .scope = state->note_scope};
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
    int effective = clamped(state->scroll, limit);
    if (delta < -effective)
    {
        delta = -effective;
    }
    if (delta > limit - effective)
    {
        delta = limit - effective;
    }
    state->scroll = effective + delta;
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
    return folio_state_reveal_row(state, metrics, cursor);
}

int folio_state_drawer_scroll(const struct folio_state *_Nonnull state)
{
    return state->scroll;
}

enum folio_state_outcome folio_state_reveal_row(struct folio_state *_Nonnull state,
                                                struct drawer_metrics metrics,
                                                struct drawer_cursor cursor)
{
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

enum folio_theme folio_state_system_theme(const struct folio_state *_Nonnull state)
{
    return state->appearance_snapshot.theme;
}

enum folio_theme folio_state_theme(const struct folio_state *_Nonnull state)
{
    return folio_theme_choice_resolve(state->display_theme_choice,
                                      state->appearance_snapshot.theme);
}

const struct theme_palette *_Nonnull folio_state_palette(const struct folio_state *_Nonnull state)
{
    return &state->palette;
}

uint64_t folio_state_palette_revision(const struct folio_state *_Nonnull state)
{
    return state->palette_revision;
}

enum folio_state_outcome folio_state_appearance_notice(const struct folio_state *_Nonnull state)
{
    return state->appearance_notice;
}

static void adopt_palette(struct folio_state *_Nonnull state, struct theme_palette palette)
{
    if (!theme_palette_equal(&state->palette, &palette))
    {
        state->palette_revision += 1;
        state->palette = palette;
    }
}

enum folio_theme_choice folio_state_theme_choice(const struct folio_state *_Nonnull state)
{
    return folio_settings_theme(state->settings);
}

enum folio_state_outcome folio_state_settings_notice(const struct folio_state *_Nonnull state)
{
    return state->settings_notice;
}

enum folio_state_outcome folio_state_select_notice(const struct folio_state *_Nonnull state)
{
    return state->select_notice;
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

/* 表示だけの設定変更は、同じ保存成功後の採用へ合流する。 */
static enum folio_state_outcome store_settings_change(struct folio_state *_Nonnull state,
                                                      struct folio_settings *_Nonnull changed)
{
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

enum note_order folio_state_note_order(const struct folio_state *_Nonnull state)
{
    return folio_settings_note_order(state->settings);
}

enum folio_state_outcome folio_state_set_note_order(struct folio_state *_Nonnull state,
                                                    enum note_order order)
{
    if (state->settings_notice != FOLIO_STATE_READY)
        return state->settings_notice;
    if (folio_state_note_order(state) == order)
        return FOLIO_STATE_READY;
    struct folio_settings *_Nullable changed = nullptr;
    if (folio_settings_with_note_order(state->settings, order, &changed) != FOLIO_SETTINGS_READY)
        return FOLIO_STATE_OUT_OF_MEMORY;
    enum folio_state_outcome outcome = store_settings_change(state, changed);
    if (outcome == FOLIO_STATE_READY)
        layout_changed(state);
    return outcome;
}

enum note_scope folio_state_note_scope(const struct folio_state *_Nonnull state)
{
    return state->note_scope;
}

void folio_state_set_note_scope(struct folio_state *_Nonnull state, enum note_scope scope)
{
    if (state->note_scope == scope)
        return;
    state->note_scope = scope;
    state->scroll = 0;
    layout_changed(state);
    settle_cursor(state);
}

size_t folio_state_favorite_count(const struct folio_state *_Nonnull state)
{
    size_t count = 0;
    for (size_t category = 0; category < state->notes_count; ++category)
        count += note_ledger_favorite_count(state->notes[category]);
    return count;
}

bool folio_state_note_favorite(const struct folio_state *_Nonnull state, struct note_ref ref)
{
    return ref.category < state->notes_count &&
           ref.note < note_ledger_count(state->notes[ref.category]) &&
           note_ledger_favorite(state->notes[ref.category], ref.note);
}

/* #1069 / ADR0117: where the note at *at of the old ledger is in the fresh one. An operand that
 * is not given is always found. A place past the old ledger's end names no note: while a note
 * is being sent to the bin, the open note's place can still be the one just removed. */
static bool follow_note(const struct note_ledger *_Nonnull old,
                        const struct note_ledger *_Nonnull fresh, size_t *_Nullable at)
{
    if (at == nullptr)
        return true;
    return *at < note_ledger_count(old) && note_ledger_find(fresh, note_ledger_name(old, *at), at);
}

/* The fresh ledger takes the old one's place. What pointed at a note by its place (the open
 * note, the cursor) follows the note by its name; a cursor whose note is gone settles on its
 * category. */
static void adopt_notes(struct folio_state *_Nonnull state, size_t category,
                        struct note_ledger *_Nonnull fresh)
{
    struct note_ledger *old = state->notes[category];
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == category)
        (void)follow_note(old, fresh, &state->selected_note); /* kept by load_notes: found */
    bool on_note = state->cursor_any && state->cursor_kind == FOLIO_CURSOR_NOTE &&
                   state->cursor_category == category;
    if (on_note && !follow_note(old, fresh, &state->cursor_note))
        state->cursor_note = note_ledger_count(fresh);
    state->notes[category] = fresh;
    note_ledger_destroy(old);
    state->ledger_notice = true;
    state->adoptions += 1;
    index_changed(state);
    /* The bodies read for searching belong to the old places, and a note added outside has
     * none yet: read them all again, and let the filter and the tag list be made again from
     * that pass (review of #1069). Nothing is allocated here, so adopting cannot fail. */
    fresh_tag_corpus(state);
    settle_cursor(state);
}

/* Before a note ledger is written: read the category's index.json and folder again, as at
 * startup, and take what they say if it differs from memory. `first` and `second` are the
 * places of the notes the operation is about; they are moved to the same notes' new places.
 * LEDGER_CHANGED_OUTSIDE when one of them is gone: the list is the fresh one and the caller
 * writes nothing. Nothing is written here either (ADR0117 decision 6). */
static enum folio_state_outcome refresh_notes(struct folio_state *_Nonnull state, size_t category,
                                              size_t *_Nullable first, size_t *_Nullable second)
{
    const struct note_ledger *old = state->notes[category];
    bool open = state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == category &&
                state->selected_note < note_ledger_count(old);
    struct note_ledger *_Nullable fresh = nullptr;
    enum folio_state_outcome outcome =
        load_notes(&fresh, &state->port, category_ledger_name(state->categories, category),
                   open ? note_ledger_name(old, state->selected_note) : nullptr);
    if (outcome != FOLIO_STATE_READY || fresh == nullptr)
        return outcome;
    if (note_ledger_equals(old, fresh))
    {
        note_ledger_destroy(fresh);
        return FOLIO_STATE_READY;
    }
    bool found = follow_note(old, fresh, first);
    found = follow_note(old, fresh, second) && found;
    adopt_notes(state, category, fresh);
    return found ? FOLIO_STATE_READY : FOLIO_STATE_LEDGER_CHANGED_OUTSIDE;
}

/* The one place that turns "the list was read again" into what the user is told: an intent
 * that went through becomes LEDGER_REFRESHED, any other outcome stays. Reading takes the
 * notice, so it is told once. */
enum folio_state_outcome folio_state_ledger_outcome(struct folio_state *_Nonnull state,
                                                    enum folio_state_outcome outcome)
{
    bool noticed = state->ledger_notice;
    state->ledger_notice = false;
    return noticed && outcome == FOLIO_STATE_READY ? FOLIO_STATE_LEDGER_REFRESHED : outcome;
}

enum folio_state_outcome folio_state_toggle_favorite(struct folio_state *_Nonnull state,
                                                     struct note_ref ref)
{
    state->ledger_notice = false; /* a notice is this intent's own, never an earlier one's */
    enum folio_state_outcome outcome = synchronize_places(state);
    if (outcome != FOLIO_STATE_READY)
        return outcome;
    if (ref.category >= state->notes_count)
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    if (ref.note >= note_ledger_count(state->notes[ref.category]))
        return FOLIO_STATE_NO_SUCH_NOTE;
    outcome = refresh_notes(state, ref.category, &ref.note, nullptr);
    if (outcome != FOLIO_STATE_READY)
        return outcome;
    struct note_ledger *_Nullable changed = nullptr;
    outcome = from_note_ledger(note_ledger_toggled(state->notes[ref.category], ref.note, &changed));
    if (outcome != FOLIO_STATE_READY)
        return outcome;
    enum persistence_outcome stored = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, ref.category), changed);
    if (stored != PERSISTENCE_STORED)
    {
        note_ledger_destroy(changed);
        return from_store(stored);
    }
    note_ledger_destroy(state->notes[ref.category]);
    state->notes[ref.category] = changed;
    invalidate_discovery(state);
    layout_changed(state);
    settle_cursor(state);
    return FOLIO_STATE_READY;
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
    return store_settings_change(state, changed);
}

bool folio_state_minimap(const struct folio_state *_Nonnull state)
{
    return folio_settings_minimap(state->settings);
}

bool folio_state_motion(const struct folio_state *_Nonnull state)
{
    return folio_settings_motion(state->settings);
}

bool folio_state_motion_available(const struct folio_state *_Nonnull state)
{
    return state->appearance_notice == FOLIO_STATE_READY &&
           state->appearance_snapshot.contrast == FOLIO_CONTRAST_STANDARD &&
           state->appearance_snapshot.motion == APPEARANCE_MOTION_ALLOWED;
}

bool folio_state_motion_enabled(const struct folio_state *_Nonnull state)
{
    return folio_state_motion(state) && folio_state_motion_available(state);
}

enum folio_state_outcome folio_state_set_motion(struct folio_state *_Nonnull state, bool motion)
{
    if (state->settings_notice != FOLIO_STATE_READY)
        return state->settings_notice;
    if (folio_settings_motion(state->settings) == motion)
        return FOLIO_STATE_READY;
    struct folio_settings *_Nullable changed = nullptr;
    if (folio_settings_with_motion(state->settings, motion, &changed) != FOLIO_SETTINGS_READY)
        return FOLIO_STATE_OUT_OF_MEMORY;
    return store_settings_change(state, changed);
}

enum folio_state_outcome folio_state_set_minimap(struct folio_state *_Nonnull state, bool minimap)
{
    if (state->settings_notice != FOLIO_STATE_READY)
        return state->settings_notice;
    if (folio_settings_minimap(state->settings) == minimap)
        return FOLIO_STATE_READY;
    struct folio_settings *_Nullable changed = nullptr;
    if (folio_settings_with_minimap(state->settings, minimap, &changed) != FOLIO_SETTINGS_READY)
        return FOLIO_STATE_OUT_OF_MEMORY;
    return store_settings_change(state, changed);
}

/* 設定と閲覧文書を両方先に作ってから書き、書けたときだけ差し替える（ADR 0031 の決定 3）。
 * テーマと言語の 2 つの意図が共有する唯一の経路で、どちらかが作れなければファイルも状態も
 * 変えない（ADR 0032 の決定 5）。changed はこの関数が引き取る。 */
static enum folio_state_outcome adopt_settings_change(struct folio_state *_Nonnull state,
                                                      struct folio_settings *_Nonnull changed,
                                                      struct theme_palette palette,
                                                      enum folio_language language)
{
    struct markdown_rtf *_Nullable pane = nullptr;
    if (!render_pane_document(state, &palette, language, &pane))
    {
        folio_settings_destroy(changed);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome stored =
        from_settings_store(state->port.write_settings(state->port.adapter, changed));
    if (stored != FOLIO_STATE_READY)
    {
        folio_settings_destroy(changed);
        markdown_rtf_destroy(pane);
        return stored;
    }
    folio_settings_destroy(state->settings);
    state->settings = changed;
    discard_view(state);
    state->pane = pane;
    adopt_palette(state, palette);
    return FOLIO_STATE_READY;
}

/* Preview and OS refresh share one non-persisting palette/RTF transaction (ADR0061). */
static enum folio_state_outcome adopt_display_palette(struct folio_state *_Nonnull state,
                                                      struct theme_palette palette)
{
    if (theme_palette_equal(&state->palette, &palette))
    {
        return FOLIO_STATE_READY;
    }
    struct markdown_rtf *_Nullable pane = nullptr;
    if (!render_pane_document(state, &palette, folio_state_language(state), &pane))
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    discard_view(state);
    state->pane = pane;
    adopt_palette(state, palette);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_preview_theme(struct folio_state *_Nonnull state,
                                                   enum folio_theme_choice choice)
{
    struct theme_palette palette = appearance_snapshot_palette(&state->appearance_snapshot, choice);
    enum folio_state_outcome outcome = adopt_display_palette(state, palette);
    if (outcome == FOLIO_STATE_READY)
    {
        state->display_theme_choice = choice;
    }
    return outcome;
}

enum folio_state_outcome folio_state_set_theme(struct folio_state *_Nonnull state,
                                               enum folio_theme_choice choice)
{
    if (state->settings_notice != FOLIO_STATE_READY)
    {
        return state->settings_notice;
    }
    if (folio_settings_theme(state->settings) == choice)
    {
        return folio_state_preview_theme(state, choice);
    }
    struct folio_settings *_Nullable changed = nullptr;
    if (folio_settings_with_theme(state->settings, choice, &changed) != FOLIO_SETTINGS_READY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct theme_palette palette = appearance_snapshot_palette(&state->appearance_snapshot, choice);
    enum folio_state_outcome adopted =
        adopt_settings_change(state, changed, palette, folio_state_language(state));
    if (adopted == FOLIO_STATE_READY)
    {
        state->display_theme_choice = choice;
    }
    return adopted;
}

enum folio_state_outcome folio_state_set_language(struct folio_state *_Nonnull state,
                                                  enum folio_language language)
{
    if (state->settings_notice != FOLIO_STATE_READY)
    {
        return state->settings_notice;
    }
    if (folio_settings_language(state->settings) == language)
    {
        return FOLIO_STATE_READY;
    }
    struct folio_settings *_Nullable changed = nullptr;
    if (folio_settings_with_language(state->settings, language, &changed) != FOLIO_SETTINGS_READY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    /* 色は変わらないので palette は今のまま。変わるのは fonttbl の face だけである。 */
    return adopt_settings_change(state, changed, state->palette, language);
}

enum folio_state_outcome folio_state_refresh_theme(struct folio_state *_Nonnull state)
{
    struct appearance_snapshot snapshot = state->appearance_snapshot;
    if (state->appearance.read(state->appearance.adapter, &snapshot) != APPEARANCE_READY)
    {
        state->appearance_notice = FOLIO_STATE_APPEARANCE_UNAVAILABLE;
        return state->appearance_notice;
    }
    struct theme_palette palette =
        appearance_snapshot_palette(&snapshot, state->display_theme_choice);
    enum folio_state_outcome outcome = adopt_display_palette(state, palette);
    if (outcome != FOLIO_STATE_READY)
    {
        state->appearance_notice = outcome;
        return outcome;
    }
    state->appearance_snapshot = snapshot;
    state->appearance_notice = FOLIO_STATE_READY;
    return FOLIO_STATE_READY;
}

size_t folio_state_note_count(const struct folio_state *_Nonnull state)
{
    return total_notes(state);
}

enum folio_state_outcome folio_state_toggle_category(struct folio_state *_Nonnull state,
                                                     size_t index)
{
    if (folio_state_filtering(state))
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
    layout_changed(state);
    settle_cursor(state);
    return FOLIO_STATE_READY;
}

/* 展開したあとのカーソル（ADR 0015 の決定 3）。選択がその中にあればそのノート、
 * 無ければ最初のノート（右ペインが変わる）、ノートが 0 本なら行に留まる。 */
static enum folio_state_outcome cursor_into(struct folio_state *_Nonnull state, size_t index,
                                            enum folio_untitled_policy untitled)
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
    return folio_state_select_note(state, index, 0, untitled);
}

/* 開閉のあとにカーソルを移す。カーソルがそのカテゴリに無ければ動かさない（決定 3）。 */
static enum folio_state_outcome cursor_after_expanded(struct folio_state *_Nonnull state,
                                                      size_t index, bool expanded,
                                                      enum folio_untitled_policy untitled)
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
    return cursor_into(state, index, untitled);
}

static enum folio_state_outcome adopt_expansion(struct folio_state *_Nonnull state, size_t index,
                                                bool expanded, bool *_Nonnull changed)
{
    *changed = false;
    if (folio_state_filtering(state))
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
    *changed = outcome == FOLIO_STATE_READY;
    return outcome;
}

enum folio_state_outcome folio_state_set_expansion(struct folio_state *_Nonnull state, size_t index,
                                                   bool expanded)
{
    bool changed = false;
    return adopt_expansion(state, index, expanded, &changed);
}

enum folio_state_outcome folio_state_set_category_expanded(struct folio_state *_Nonnull state,
                                                           size_t index, bool expanded,
                                                           enum folio_untitled_policy untitled)
{
    bool changed = false;
    enum folio_state_outcome outcome = adopt_expansion(state, index, expanded, &changed);
    if (outcome != FOLIO_STATE_READY || !changed)
    {
        return outcome;
    }
    return cursor_after_expanded(state, index, expanded, untitled);
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
    state->ledger_notice = false;
    if (folio_state_filtering(state))
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
    index_changed(state);
    move_notes(state->notes, from, to);
    if (state->document != FOLIO_DOCUMENT_NONE)
    {
        state->selected_category = moved_index(from, to, state->selected_category);
    }
    if (state->cursor_any)
    {
        /* ノート/カテゴリどちらのcursorも同じ行を指したまま移る。 */
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
    index_changed(state);
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == category)
    {
        state->selected_note = moved_index(from, to, state->selected_note);
    }
    if (state->cursor_any && state->cursor_kind == FOLIO_CURSOR_NOTE &&
        state->cursor_category == category)
    {
        state->cursor_note = moved_index(from, to, state->cursor_note);
    }
    return FOLIO_STATE_READY;
}

/* お気に入りを先に並べる設定で、このカテゴリに星があるか。そのときは手動の並べ替えを受けない（ADR0090）。
 */
static bool favorites_lead(const struct folio_state *_Nonnull state, size_t category)
{
    return folio_state_note_order(state) == NOTE_ORDER_FAVORITES_FIRST &&
           note_ledger_favorite_count(state->notes[category]) != 0;
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
        return FOLIO_STATE_READY; /* nothing to write, so nothing is read again */
    }
    /* #1069: both the moved note and the note it is dropped on are found again by name. Stars
     * that came from outside forbid the move as stars in memory do (the caller asked those). */
    enum folio_state_outcome refreshed = refresh_notes(state, category, &from, &to);
    if (refreshed != FOLIO_STATE_READY)
    {
        return refreshed;
    }
    if (favorites_lead(state, category))
    {
        return FOLIO_STATE_ORDERED;
    }
    return from == to ? FOLIO_STATE_READY : store_notes(state, category, from, to);
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

/* 別カテゴリへの移動に伴う行参照の写像。選択とcursorが同じ規則を使う。 */
static struct note_ref transferred_ref(struct note_ref ref, struct note_ref from,
                                       struct note_ref to)
{
    if (ref.category == from.category && ref.note == from.note)
    {
        return to;
    }
    if (ref.category == from.category && ref.note > from.note)
    {
        ref.note -= 1;
    }
    if (ref.category == to.category && ref.note >= to.note)
    {
        ref.note += 1;
    }
    return ref;
}

static void renumber_selection(struct folio_state *_Nonnull state, struct note_ref from,
                               struct note_ref to)
{
    if (state->document == FOLIO_DOCUMENT_NAMED)
    {
        struct note_ref ref =
            transferred_ref(located(state->selected_category, state->selected_note), from, to);
        state->selected_category = ref.category;
        state->selected_note = ref.note;
    }
    if (state->cursor_any && state->cursor_kind == FOLIO_CURSOR_NOTE)
    {
        struct note_ref ref =
            transferred_ref(located(state->cursor_category, state->cursor_note), from, to);
        state->cursor_category = ref.category;
        state->cursor_note = ref.note;
    }
    settle_cursor(state);
}

/* mdの移動前に、移動先の星を永続化する（ADR0090）。失敗時は同じ同期で戻す。 */
static enum folio_state_outcome move_prepared(struct folio_state *_Nonnull state,
                                              struct note_ref from, struct note_ref to,
                                              const struct note_ledger *_Nonnull destination)
{
    enum persistence_outcome stored = state->port.write_note_ledger(
        state->port.adapter, category_ledger_name(state->categories, to.category), destination);
    if (stored != PERSISTENCE_STORED)
        return from_store(stored);
    enum persistence_outcome moved = state->port.move_note(
        state->port.adapter, category_ledger_name(state->categories, from.category),
        note_ledger_name(state->notes[from.category], from.note),
        category_ledger_name(state->categories, to.category));
    if (moved == PERSISTENCE_STORED)
        return FOLIO_STATE_READY;
    state->index_pending = true;
    state->pending_category = to.category;
    if (synchronize_index(state) != FOLIO_STATE_READY)
        return FOLIO_STATE_MOVE_ROLLBACK_PENDING;
    return from_store(moved);
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
            note_ledger_entry(state->notes[from.category], from.note), &pair[1]));
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

/* #1069 / ADR0117 (stage 4): both categories are read again before the destination ledger is
 * written. The moved note, and the note it is dropped before, are found again by name; a drop
 * at the end stays at the end. A name that came to the destination from outside is NAME_TAKEN
 * here, before anything is written. */
static enum folio_state_outcome refresh_transfer(struct folio_state *_Nonnull state,
                                                 struct note_ref *_Nonnull from,
                                                 struct note_ref *_Nonnull to)
{
    bool end = to->note >= note_ledger_count(state->notes[to->category]);
    enum folio_state_outcome outcome = refresh_notes(state, from->category, &from->note, nullptr);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = refresh_notes(state, to->category, end ? nullptr : &to->note, nullptr);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    if (end)
    {
        to->note = note_ledger_count(state->notes[to->category]);
    }
    /* A drop before a note is a place of the manual order; stars that came from outside forbid
     * it as stars in memory do. A move to the end is the same place in both orders (ADR0111). */
    if (!end && (favorites_lead(state, from->category) || favorites_lead(state, to->category)))
    {
        return FOLIO_STATE_ORDERED;
    }
    return transfer_allowed(state, *from, *to);
}

/* 別カテゴリへ移す。移動先台帳→md→移動元台帳の順で星を守る（ADR0090）。 */
static enum folio_state_outcome transfer_note(struct folio_state *_Nonnull state,
                                              struct note_ref from, struct note_ref to)
{
    /* Only the places are asked of the lists in memory. Whether the name is free in the
     * destination is asked after both lists were read again: a note of that name deleted
     * outside must not keep refusing the move (review of #1069, stage 4). */
    if (from.note >= note_ledger_count(state->notes[from.category]) ||
        to.note > note_ledger_count(state->notes[to.category]))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    enum folio_state_outcome outcome = refresh_transfer(state, &from, &to);
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
    outcome = move_prepared(state, from, to, pair[1]);
    if (outcome != FOLIO_STATE_READY)
    {
        note_ledger_destroy(pair[1]);
        note_ledger_destroy(pair[0]);
        return outcome;
    }
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == from.category &&
        state->selected_note == from.note)
        image_origin_changed(state);
    enum folio_state_outcome copied = recopy_move(state, from, to.category);
    note_ledger_destroy(state->notes[from.category]);
    state->notes[from.category] = pair[0];
    note_ledger_destroy(state->notes[to.category]);
    state->notes[to.category] = pair[1];
    index_changed(state);
    renumber_selection(state, from, to);
    folio_state_refresh_note_times(state);
    state->index_pending = true;
    state->pending_category = from.category;
    enum folio_state_outcome stored = published_index(synchronize_index(state));
    return stored != FOLIO_STATE_READY ? stored : copied;
}

enum folio_state_outcome folio_state_move_note(struct folio_state *_Nonnull state,
                                               struct note_ref from, struct note_ref to)
{
    state->ledger_notice = false;
    if (folio_state_filtering(state))
    {
        return FOLIO_STATE_FILTERED;
    }
    enum folio_state_outcome synced = synchronize_places(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }

    size_t count = category_ledger_count(state->categories);
    if (from.category >= count || to.category >= count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    /* Asked of the ledgers in memory, as before, and before anything is read: under stars the
     * drop's place is a place of the shown order, not of the ledger. */
    if (favorites_lead(state, from.category) || favorites_lead(state, to.category))
        return FOLIO_STATE_ORDERED;
    if (from.category == to.category)
    {
        return reorder_note(state, from.category, from.note, to.note);
    }
    return transfer_note(state, from, to);
}

enum folio_state_outcome folio_state_move_note_to_category(struct folio_state *_Nonnull state,
                                                           struct note_ref from, size_t category)
{
    state->ledger_notice = false;
    if (folio_state_filtering(state))
    {
        return FOLIO_STATE_FILTERED;
    }
    /* The end is counted after the lists are current, as folio_state_move_note sees them. */
    enum folio_state_outcome synced = synchronize_places(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    size_t count = category_ledger_count(state->categories);
    if (from.category >= count || category >= count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (from.category == category)
    {
        return FOLIO_STATE_READY;
    }
    /* The drag is refused while favorites come first, because a drop position in that view is
     * not a position in the manual order. The end of another category is the same place in
     * both, so this move is not (ADR0111). The stars follow as in any transfer. */
    struct note_ref end = {.category = category, .note = note_ledger_count(state->notes[category])};
    return transfer_note(state, from, end);
}

/* Prepare both projections before adopting a replacement (ADR0050). */
static enum folio_state_outcome prepare_note_edit(const struct folio_state *_Nonnull state,
                                                  const struct note_text *_Nonnull body,
                                                  struct markdown_rtf *_Nullable *_Nonnull edit)
{
    if (folio_state_pane_mode(state) != PANE_MODE_EDIT ||
        note_text_editability(body) == NOTE_TEXT_EDITOR_ALTERS)
    {
        return FOLIO_STATE_READY;
    }
    return markdown_rtf_plain(body, &state->palette, ui_font_face(folio_state_language(state)),
                              edit) == MARKDOWN_RTF_CONVERTED
               ? FOLIO_STATE_READY
               : FOLIO_STATE_OUT_OF_MEMORY;
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
    enum markdown_rtf_outcome converted = markdown_rtf_create(
        body, &state->palette, view_locale(folio_state_language(state)), &rendered);
    if (converted != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(body);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct markdown_rtf *_Nullable edit = nullptr;
    enum folio_state_outcome ready = prepare_note_edit(state, body, &edit);
    if (ready != FOLIO_STATE_READY)
    {
        note_text_destroy(body);
        markdown_rtf_destroy(rendered);
        return ready;
    }
    discard_view(state);
    markdown_rtf_destroy(state->edit);
    note_text_destroy(state->body);
    state->pane = rendered;
    state->edit = edit;
    state->body = body;
    return FOLIO_STATE_READY;
}

/* 編集のまま切り替えても、編集で保てない本文は editor へ渡さず閲覧で開く（#242）。
 * 編集から閲覧へ落としたときだけ、その事実を select_notice に残す（#258）。 */
static void open_uneditable_in_view(struct folio_state *_Nonnull state)
{
    if (note_text_editability(state->body) != NOTE_TEXT_EDITOR_ALTERS)
    {
        return;
    }
    if (state->mode == PANE_MODE_EDIT)
    {
        state->select_notice = FOLIO_STATE_NOTE_UNEDITABLE;
    }
    state->mode = PANE_MODE_VIEW;
}

static bool permits_untitled_replacement(enum folio_untitled_policy policy)
{
    switch (policy)
    {
    case FOLIO_UNTITLED_PROTECT:
        return false;
    case FOLIO_UNTITLED_DISCARD:
        return true;
    }
    return false;
}

enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                 size_t category, size_t note,
                                                 enum folio_untitled_policy untitled)
{
    state->select_notice = FOLIO_STATE_READY;
    if (state->document == FOLIO_DOCUMENT_UNTITLED && !permits_untitled_replacement(untitled))
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }

    enum folio_state_outcome synced = synchronize_places(state);
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
        document_changed(state, FOLIO_DOCUMENT_NAMED);
        state->selected_category = category;
        state->selected_note = note;
        folio_state_refresh_note_times(state);
        cursor_to_selection(state);
        folio_state_close_history(state);
        open_uneditable_in_view(state);
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
    for (size_t position = at; position < count; ++position)
    {
        size_t note =
            note_ledger_ordered(state->notes[category], folio_state_note_order(state), position);
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
        size_t note =
            note_ledger_ordered(state->notes[category], folio_state_note_order(state), reach);
        if (shown_note(state, category, note))
        {
            *out = note;
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
static size_t after_in(const struct folio_state *_Nonnull state, struct drawer_cursor from)
{
    switch (from.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return 0;
    case DRAWER_ROW_NOTE:
        return note_ledger_position(state->notes[from.ref.category], folio_state_note_order(state),
                                    from.ref.note) +
               1;
    }
    return 0;
}

/* from と同じカテゴリで、from より前を探し始める位置。カテゴリ行のカーソルは中へ戻らない。 */
static size_t before_in(const struct folio_state *_Nonnull state, struct drawer_cursor from)
{
    switch (from.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return 0;
    case DRAWER_ROW_NOTE:
        return note_ledger_position(state->notes[from.ref.category], folio_state_note_order(state),
                                    from.ref.note);
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
        shown_note_from(state, from.ref.category, after_in(state, from), &note))
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
        shown_note_before(state, from.ref.category, before_in(state, from), &note))
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
                                                struct note_ref target,
                                                enum folio_untitled_policy untitled)
{
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == target.category &&
        state->selected_note == target.note)
    {
        cursor_to_selection(state);
        return FOLIO_STATE_READY;
    }
    return folio_state_select_note(state, target.category, target.note, untitled);
}

enum folio_state_outcome folio_state_select_adjacent(struct folio_state *_Nonnull state,
                                                     enum folio_step step,
                                                     enum folio_untitled_policy untitled)
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
        return stepped_to_note(state, target.ref, untitled);
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

enum folio_state_outcome folio_state_focus_cursor(struct folio_state *_Nonnull state,
                                                  enum folio_cursor_kind kind,
                                                  struct note_ref target)
{
    if (target.category >= category_ledger_count(state->categories))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    struct drawer_cursor cursor = on_category(target.category);
    switch (kind)
    {
    case FOLIO_CURSOR_CATEGORY:
        break;
    case FOLIO_CURSOR_NOTE:
        cursor = on_note(target.category, target.note);
        break;
    }
    if (!cursor_row_present(state, cursor))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    state->cursor_any = true;
    state->cursor_kind = kind;
    state->cursor_category = target.category;
    state->cursor_note = target.note;
    return FOLIO_STATE_READY;
}

uint64_t folio_state_index_revision(const struct folio_state *_Nonnull state)
{
    return state->index_revision;
}

uint64_t folio_state_row_revision(const struct folio_state *_Nonnull state)
{
    return state->row_revision;
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
    /* RichEdit が流し込みで変える文字があると、何も打たずに切り替えるだけで md が変わる。
     * 編集へ入る前に断り、閲覧のままにする（Issue #242）。無題の本文は空から始まる。 */
    if (state->document == FOLIO_DOCUMENT_NAMED &&
        note_text_editability(state->body) == NOTE_TEXT_EDITOR_ALTERS)
    {
        return FOLIO_STATE_NOTE_UNEDITABLE;
    }
    state->images.cancel(state->images.adapter);
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

bool folio_state_category_replaces_document(const struct folio_state *_Nonnull state,
                                            size_t category, bool expanded)
{
    struct drawer_cursor cursor = on_note(0, 0);
    if (!expanded || folio_state_filtering(state) ||
        category >= category_ledger_count(state->categories) ||
        category_ledger_expanded(state->categories, category) ||
        note_ledger_count(state->notes[category]) == 0)
    {
        return false;
    }
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == category)
    {
        return false;
    }
    return current_cursor(state, &cursor) && cursor.ref.category == category;
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

enum folio_state_outcome folio_state_new_note(struct folio_state *_Nonnull state, size_t category,
                                              enum folio_untitled_policy untitled)
{
    if (state->document == FOLIO_DOCUMENT_UNTITLED && !permits_untitled_replacement(untitled))
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
    struct markdown_rtf *_Nullable edit = nullptr;
    if (note_text_create("", 0, &body) != NOTE_TEXT_ACCEPTED ||
        markdown_rtf_empty(&state->palette, ui_font_face(folio_state_language(state)), &pane) !=
            MARKDOWN_RTF_CONVERTED ||
        markdown_rtf_plain(body, &state->palette, ui_font_face(folio_state_language(state)),
                           &edit) != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(body);
        markdown_rtf_destroy(pane);
        markdown_rtf_destroy(edit);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    note_text_destroy(state->body);
    discard_view(state);
    markdown_rtf_destroy(state->edit);
    state->body = body;
    state->pane = pane;
    state->edit = edit;
    document_changed(state, FOLIO_DOCUMENT_UNTITLED);
    state->selected_category = category;
    folio_state_refresh_note_times(state);
    state->images.cancel(state->images.adapter);
    state->mode = PANE_MODE_EDIT;
    state->cursor_any = false;
    folio_state_close_history(state);
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome note_store_result(enum note_store_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_STORE_STORED:
    case NOTE_STORE_UNCHANGED:
        return FOLIO_STATE_READY;
    case NOTE_STORE_SOURCE_CHANGED:
        return FOLIO_STATE_NOTE_SOURCE_CHANGED;
    case NOTE_STORE_SOURCE_UNREADABLE:
        return FOLIO_STATE_NOTE_UNREADABLE;
    case NOTE_STORE_HISTORY_FAILED:
        return FOLIO_STATE_HISTORY_FAILED;
    case NOTE_STORE_WRITE_FAILED:
        return FOLIO_STATE_NOTE_STORE_FAILED;
    case NOTE_STORE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_NOTE_STORE_FAILED;
}

/* 保存できた本文で写しを差し替え、絞り込み中なら一致集合も作り直す（ADR 0024 の決定 1 / 4）。
 * 未完成なら走査を先頭へ戻し、次の継続で保存済み本文を読む（ADR0068）。 */
static enum folio_state_outcome refresh_copy(struct folio_state *_Nonnull state)
{
    restart_corpus_load(state);
    if (!state->corpus_loaded || state->document != FOLIO_DOCUMENT_NAMED)
    {
        return FOLIO_STATE_READY;
    }
    state->copy_pending = true;
    enum note_corpus_outcome stored = note_corpus_put(
        state->corpus, category_ledger_name(state->categories, state->selected_category),
        note_ledger_name(state->notes[state->selected_category], state->selected_note),
        state->body);
    if (stored != NOTE_CORPUS_ACCEPTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome refreshed = refresh_filter(state);
    if (refreshed == FOLIO_STATE_READY)
    {
        state->copy_pending = false;
    }
    return refreshed;
}

/* 正規化済みの本文を書き戻し、表示値も作り直す。書けなければ何も変えない（ADR 0006 の決定 6）。
 * 履歴を残せなければ書き戻しにも進まない（ADR 0012 の決定 2）。 */
static enum folio_state_outcome store_edited(struct folio_state *_Nonnull state,
                                             struct note_text *_Nonnull edited,
                                             enum note_save_mode mode)
{
    bool same = note_text_equals(state->body, edited);
    if (same && mode == NOTE_SAVE_UNCONDITIONAL)
    {
        note_text_destroy(edited);
        return FOLIO_STATE_READY;
    }
    struct markdown_rtf *_Nullable rendered = nullptr;
    if (!same &&
        markdown_rtf_create(edited, &state->palette, view_locale(folio_state_language(state)),
                            &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        note_text_destroy(edited);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    const struct note_store_request request = {
        .category = category_ledger_name(state->categories, state->selected_category),
        .note = note_ledger_name(state->notes[state->selected_category], state->selected_note),
        .body = edited,
        .expected = mode == NOTE_SAVE_CHECK_CURRENT ? state->body : nullptr};
    enum note_store_outcome stored = state->port.store_note(state->port.adapter, &request);
    if (stored != NOTE_STORE_STORED)
    {
        markdown_rtf_destroy(rendered);
        note_text_destroy(edited);
        return note_store_result(stored);
    }
    stored_note(state);
    discard_view(state);
    folio_state_refresh_note_times(state);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = edited;
    /* 保存で 1.md が積まれ、持っている版の番号がずれた（ADR 0038 の決定 10）。 */
    folio_state_close_history(state);
    return refresh_copy(state);
}

/* 本文の型付き結果を、保存と複製で同じstate結果へ写す。 */
static enum folio_state_outcome text_outcome(enum note_text_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_TEXT_ACCEPTED:
        return FOLIO_STATE_READY;
    case NOTE_TEXT_INVALID_UTF8:
    case NOTE_TEXT_EMBEDDED_NUL:
        return FOLIO_STATE_NOTE_MALFORMED;
    case NOTE_TEXT_TOO_LARGE:
        return FOLIO_STATE_NOTE_TOO_LARGE;
    case NOTE_TEXT_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_NOTE_MALFORMED;
}

/* UI が持つ編集中の本文（UTF-16）を core で検証・変換し、読んだ本文の改行の形へ揃える。
 * 保存と読み取り専用の変更問い合わせがこの 1 本を共有する（C-014・ADR 0016 の決定 10）。 */
static enum folio_state_outcome edited_text(const struct folio_state *_Nonnull state,
                                            const char16_t *_Nonnull units, size_t count,
                                            struct note_text *_Nullable *_Nonnull out)
{
    if (folio_state_pane_mode(state) != PANE_MODE_EDIT)
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
    return text_outcome(accepted);
}

static enum folio_state_outcome save_note(struct folio_state *_Nonnull state,
                                          const char16_t *_Nonnull units, size_t count,
                                          enum note_save_mode mode)
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
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
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
    return store_edited(state, edited, mode);
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

/* First save and import share publication and ledger ownership. Caller prepared the ledger. */
static enum persistence_outcome publish_created(struct folio_state *_Nonnull state,
                                                const struct note_destination *_Nonnull destination,
                                                struct note_ledger *_Nonnull ledger,
                                                const struct note_text *_Nonnull body)
{
    size_t category = destination->category;
    enum persistence_outcome stored = state->port.create_note(
        state->port.adapter, category_ledger_name(state->categories, category),
        note_name_stem(destination->name), body);
    if (stored != PERSISTENCE_STORED)
        return stored;
    note_ledger_destroy(state->notes[category]);
    state->notes[category] = ledger;
    index_changed(state);
    stored_note(state);
    state->index_pending = true;
    state->pending_category = category;
    return PERSISTENCE_STORED;
}

/* 副作用より先に表示と台帳を確保。md公開後の表示採用は確保せず、
 *
 * 検索写しの失敗は保留して再試行する（#219）。 */
static enum folio_state_outcome create_edited(struct folio_state *_Nonnull state,
                                              const struct note_destination *_Nonnull destination,
                                              struct note_text *_Nonnull edited)
{
    bool follow = !state->cursor_any || (state->cursor_kind == FOLIO_CURSOR_NOTE &&
                                         state->cursor_category == state->selected_category &&
                                         state->cursor_note == state->selected_note);
    size_t category = destination->category;
    const char *_Nonnull name = note_name_stem(destination->name);
    struct note_ledger *_Nullable ledger = nullptr;
    struct markdown_rtf *_Nullable rendered = nullptr;
    size_t index = note_ledger_count(state->notes[category]);
    enum folio_state_outcome prepared = from_note_ledger(
        note_ledger_inserted(state->notes[category], index,
                             (struct note_entry){.name = name, .favorite = false}, &ledger));
    if (prepared == FOLIO_STATE_READY &&
        markdown_rtf_create(edited, &state->palette, view_locale(folio_state_language(state)),
                            &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        prepared = FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum persistence_outcome stored = PERSISTENCE_UNWRITABLE;
    if (prepared == FOLIO_STATE_READY)
    {
        stored = publish_created(state, destination, ledger, edited);
    }
    if (stored != PERSISTENCE_STORED)
    {
        note_ledger_destroy(ledger);
        markdown_rtf_destroy(rendered);
        note_text_destroy(edited);
        return unpublished(prepared, stored);
    }
    discard_view(state);
    state->pane = rendered;
    note_text_destroy(state->body);
    state->body = edited;
    document_changed(state, FOLIO_DOCUMENT_NAMED);
    state->selected_category = category;
    state->selected_note = index;
    folio_state_refresh_note_times(state);
    folio_state_close_history(state);
    if (follow)
    {
        cursor_to_selection(state);
    }
    /* 新しく公開したノートも写しと一致集合に入れる（ADR 0024 の決定 4）。
     * 台帳の同期より先に行う（#1069, stage 4）: 同期は台帳とフォルダを読み直すので確保がある。
     * 写しの確保の失敗を、同期の中の失敗と取り違えない。同期が一覧を入れ替えたら、写しは
     * そこで最初から読み直される。 */
    enum folio_state_outcome copied = refresh_copy(state);
    enum folio_state_outcome synced = published_index(synchronize_index(state));
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
    enum note_text_outcome copied = note_text_copy(state->body, out);
    return text_outcome(copied);
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

static enum folio_state_outcome import_name(const struct storage_path *_Nonnull path,
                                            struct note_name *_Nullable *_Nonnull out)
{
    const char *text = storage_path_text(path);
    const char *leaf = text;
    for (size_t index = 0; text[index] != '\0'; ++index)
    {
        if (text[index] == '\\')
            leaf = text + index + 1;
    }
    size_t length = strlen(leaf);
    if (length <= 3 || !same_folded(leaf + length - 3, ".md"))
        return FOLIO_STATE_INVALID_NAME;
    switch (note_name_create(leaf, length, out))
    {
    case NOTE_NAME_ACCEPTED:
        return FOLIO_STATE_READY;
    case NOTE_NAME_INVALID:
        return FOLIO_STATE_INVALID_NAME;
    case NOTE_NAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_INVALID_NAME;
}

static enum folio_state_outcome transfer_read_outcome(enum persistence_outcome read)
{
    switch (read)
    {
    case PERSISTENCE_LOADED:
        return FOLIO_STATE_READY;
    case PERSISTENCE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case PERSISTENCE_MALFORMED:
        return FOLIO_STATE_NOTE_MALFORMED;
    case PERSISTENCE_ABSENT:
    case PERSISTENCE_UNREADABLE:
    case PERSISTENCE_STORED:
    case PERSISTENCE_UNWRITABLE:
    case PERSISTENCE_NAME_TAKEN:
        return FOLIO_STATE_NOTE_UNREADABLE;
    }
    return FOLIO_STATE_NOTE_UNREADABLE;
}

static enum folio_state_outcome import_body(struct folio_state *_Nonnull state,
                                            const struct note_destination *_Nonnull destination,
                                            const struct note_text *_Nonnull body)
{
    size_t category = destination->category;
    const char *name = note_name_stem(destination->name);
    const char *folder = category_ledger_name(state->categories, category);
    struct note_ledger *_Nullable ledger = nullptr;
    enum folio_state_outcome prepared = from_note_ledger(
        note_ledger_inserted(state->notes[category], note_ledger_count(state->notes[category]),
                             (struct note_entry){.name = name, .favorite = false}, &ledger));
    if (prepared != FOLIO_STATE_READY)
        return prepared;
    if (state->corpus_loaded &&
        note_corpus_put(state->corpus, folder, name, body) != NOTE_CORPUS_ACCEPTED)
    {
        note_ledger_destroy(ledger);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum persistence_outcome published = publish_created(state, destination, ledger, body);
    if (published != PERSISTENCE_STORED)
    {
        note_corpus_remove(state->corpus, folder, name);
        note_ledger_destroy(ledger);
        return unpublished(FOLIO_STATE_READY, published);
    }
    return published_index(synchronize_index(state));
}

static enum folio_state_outcome import_file(struct folio_state *_Nonnull state, size_t category,
                                            const struct storage_path *_Nonnull path)
{
    struct note_name *_Nullable name = nullptr;
    enum folio_state_outcome result = import_name(path, &name);
    if (result != FOLIO_STATE_READY)
        return result;
    if (holds_folded_name(state->notes[category], note_name_stem(name)))
    {
        note_name_destroy(name);
        return FOLIO_STATE_NAME_TAKEN;
    }
    struct note_text *_Nullable body = nullptr;
    result = transfer_read_outcome(state->transfer.read(path, &body));
    if (result == FOLIO_STATE_READY)
    {
        struct note_destination destination = {.category = category, .name = name};
        result = import_body(state, &destination, body);
    }
    note_text_destroy(body);
    note_name_destroy(name);
    return result;
}

static void record_import(struct note_import_result *_Nonnull result,
                          enum folio_state_outcome imported)
{
    if (imported == FOLIO_STATE_NAME_TAKEN)
    {
        result->skipped += 1;
        result->remaining -= 1;
        return;
    }
    if (imported == FOLIO_STATE_READY || imported == FOLIO_STATE_LEDGER_STALE)
    {
        result->added += 1;
        result->remaining -= 1;
    }
    result->outcome = imported;
}

struct note_import_result
folio_state_import_notes(struct folio_state *_Nonnull state,
                         const struct note_import_request *_Nonnull request)
{
    struct note_import_result result = {.remaining = request->count, .outcome = synchronize(state)};
    if (result.outcome != FOLIO_STATE_READY)
        return result;
    if (folio_state_filtering(state))
        result.outcome = FOLIO_STATE_FILTERED;
    else if (!holds_category(state, request->category))
        result.outcome = FOLIO_STATE_NO_SUCH_CATEGORY;
    if (result.outcome != FOLIO_STATE_READY)
        return result;
    for (size_t index = 0; index < request->count; ++index)
    {
        enum folio_state_outcome imported =
            import_file(state, request->category, request->paths[index]);
        record_import(&result, imported);
        if (result.outcome != FOLIO_STATE_READY)
            break;
    }
    return result;
}

static bool export_category(const struct note_export_request *_Nonnull request, size_t category)
{
    switch (request->scope)
    {
    case NOTE_EXPORT_CURRENT:
        return false;
    case NOTE_EXPORT_CATEGORY:
        return request->category == category;
    case NOTE_EXPORT_ALL:
        return true;
    }
    return false;
}

static size_t export_count(const struct folio_state *_Nonnull state,
                           const struct note_export_request *_Nonnull request)
{
    size_t count = 0;
    for (size_t category = 0; category < state->notes_count; ++category)
    {
        if (export_category(request, category))
            count += note_ledger_count(state->notes[category]);
    }
    return count;
}

static enum folio_state_outcome read_export_entry(struct folio_state *_Nonnull state,
                                                  struct note_ref ref,
                                                  struct archive_note *_Nonnull entry,
                                                  struct note_text *_Nullable *_Nonnull body)
{
    const char *category = category_ledger_name(state->categories, ref.category);
    const char *name = note_ledger_name(state->notes[ref.category], ref.note);
    enum folio_state_outcome result =
        transfer_read_outcome(state->port.read_note(state->port.adapter, category, name, body));
    if (result == FOLIO_STATE_READY)
        *entry = (struct archive_note){.category = category, .name = name, .body = *body};
    return result;
}

static enum folio_state_outcome collect_archive(struct folio_state *_Nonnull state,
                                                const struct note_export_request *_Nonnull request,
                                                struct archive_note *_Nonnull entries,
                                                struct note_text *_Nullable *_Nonnull bodies)
{
    size_t used = 0;
    size_t bytes = 22;
    size_t first = 0;
    size_t limit = state->notes_count;
    if (request->scope == NOTE_EXPORT_CATEGORY)
    {
        first = request->category;
        limit = first + 1;
    }
    for (size_t category = first; category < limit; ++category)
    {
        for (size_t note = 0; note < note_ledger_count(state->notes[category]); ++note)
        {
            struct note_ref ref = {.category = category, .note = note};
            enum folio_state_outcome read =
                read_export_entry(state, ref, &entries[used], &bodies[used]);
            if (read != FOLIO_STATE_READY)
                return read;
            size_t size = note_text_file_length(bodies[used]);
            if (size > note_archive_byte_limit - bytes)
                return FOLIO_STATE_EXPORT_TOO_LARGE;
            bytes += size;
            used += 1;
        }
    }
    return FOLIO_STATE_READY;
}

static void destroy_export_entries(struct archive_note *_Nullable entries,
                                   struct note_text *_Nullable *_Nullable bodies, size_t count)
{
    if (bodies != nullptr)
    {
        for (size_t index = 0; index < count; ++index)
            note_text_destroy(bodies[index]);
    }
    free(bodies);
    free(entries);
}

static enum folio_state_outcome archive_outcome(enum note_archive_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_ARCHIVE_READY:
        return FOLIO_STATE_READY;
    case NOTE_ARCHIVE_INVALID:
        return FOLIO_STATE_INVALID_NAME;
    case NOTE_ARCHIVE_TOO_LARGE:
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    case NOTE_ARCHIVE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_INVALID_NAME;
}

static enum folio_state_outcome exported(enum persistence_outcome outcome)
{
    switch (outcome)
    {
    case PERSISTENCE_STORED:
        return FOLIO_STATE_READY;
    case PERSISTENCE_NAME_TAKEN:
        return FOLIO_STATE_EXPORT_EXISTS;
    case PERSISTENCE_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case PERSISTENCE_ABSENT:
    case PERSISTENCE_LOADED:
    case PERSISTENCE_UNREADABLE:
    case PERSISTENCE_MALFORMED:
    case PERSISTENCE_UNWRITABLE:
        return FOLIO_STATE_EXPORT_FAILED;
    }
    return FOLIO_STATE_EXPORT_FAILED;
}

static enum folio_state_outcome prepare_archive_images(struct image_export *_Nonnull images,
                                                       struct archive_note *_Nonnull entries,
                                                       size_t count,
                                                       enum image_export_policy policy)
{
    for (size_t index = 0; index < count; ++index)
    {
        enum folio_state_outcome result = image_export_append(images, &entries[index]);
        if (result != FOLIO_STATE_READY)
            return result;
    }
    if (policy == IMAGE_EXPORT_UNDECIDED && image_export_external(images))
        return FOLIO_STATE_EXPORT_EXTERNAL_IMAGES;
    return FOLIO_STATE_READY;
}
static enum folio_state_outcome export_archive(struct folio_state *_Nonnull state,
                                               const struct note_export_request *_Nonnull request)
{
    size_t count = export_count(state, request);
    if (count > note_archive_count_limit)
        return FOLIO_STATE_EXPORT_TOO_LARGE;
    struct archive_note *_Nullable entries = calloc(count == 0 ? 1 : count, sizeof *entries);
    struct note_text *_Nullable *_Nullable bodies = calloc(count == 0 ? 1 : count, sizeof *bodies);
    if (entries == nullptr || bodies == nullptr)
    {
        destroy_export_entries(entries, bodies, count);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome result = collect_archive(state, request, entries, bodies);
    struct image_export *_Nullable images = nullptr;
    if (result == FOLIO_STATE_READY)
        result = image_export_create(&state->images, request->images, count, &images);
    if (result == FOLIO_STATE_READY)
        result = prepare_archive_images(images, entries, count, request->images);
    struct note_archive *_Nullable archive = nullptr;
    if (result == FOLIO_STATE_READY)
        result = archive_outcome(
            note_archive_create(entries, count, image_export_assets(images), &archive));
    if (result == FOLIO_STATE_READY)
        result = exported(state->transfer.write_archive(request->destination, archive));
    note_archive_destroy(archive);
    image_export_destroy(images);
    destroy_export_entries(entries, bodies, count);
    return result;
}

enum folio_state_outcome
folio_state_export_notes(struct folio_state *_Nonnull state,
                         const struct note_export_request *_Nonnull request)
{
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
        return synced;
    if (request->scope == NOTE_EXPORT_CATEGORY && request->category >= state->notes_count)
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    if (request->scope != NOTE_EXPORT_CURRENT)
        return export_archive(state, request);
    if (state->document == FOLIO_DOCUMENT_NONE)
        return FOLIO_STATE_NOTHING_SELECTED;
    struct note_text *_Nullable body = nullptr;
    enum folio_state_outcome result = copied_text(state, request->units, request->count, &body);
    if (result == FOLIO_STATE_READY)
        result = exported(state->transfer.write_note(request->destination, body));
    note_text_destroy(body);
    return result;
}

static enum folio_state_outcome from_note_rename(enum rename_plan_outcome outcome)
{
    switch (outcome)
    {
    case RENAME_PLAN_ACCEPTED:
        return FOLIO_STATE_READY;
    case RENAME_PLAN_INVALID:
        return FOLIO_STATE_INVALID_NAME;
    case RENAME_PLAN_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_INVALID_NAME;
}

/* 準備済み意図を唯一のSTART経路へ渡す。公開前拒否では準備を残さない。 */
static enum folio_state_outcome apply_prepared_rename(struct folio_state *_Nonnull state,
                                                      struct rename_plan *_Nonnull intent,
                                                      size_t category)
{
    enum rename_outcome moved = state->port.apply_rename(state->port.adapter, intent, RENAME_START);
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
        if (rename_plan_kind(intent) == RENAME_KIND_CATEGORY)
            image_origin_changed(state);
        return from_rename(moved);
    }
    enum rename_kind kind = rename_plan_kind(intent);
    rename_plan_destroy(intent);
    note_corpus_destroy(state->rename_corpus);
    state->rename_corpus = nullptr;
    if (kind == RENAME_KIND_CATEGORY && moved == RENAME_NAME_TAKEN)
    {
        return FOLIO_STATE_CATEGORY_NAME_TAKEN;
    }
    return from_rename(moved);
}

/* 副作用の前に意図を確保し、ポートへ渡す。公開前の拒否では意図を残さない（決定 2）。 */
static enum folio_state_outcome rename_selected(struct folio_state *_Nonnull state,
                                                const struct note_name *_Nonnull name)
{
    size_t category = state->selected_category;
    struct note_rename_target target = {.index = state->selected_note, .name = name};
    struct rename_plan *_Nullable intent = nullptr;
    enum folio_state_outcome prepared =
        from_note_rename(rename_plan_create_note(category_ledger_name(state->categories, category),
                                                 state->notes[category], &target, &intent));
    return prepared == FOLIO_STATE_READY ? apply_prepared_rename(state, intent, category)
                                         : prepared;
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
        return FOLIO_STATE_NAME_UNCHANGED;
    }
    if (holds_folded_name(state->notes[category], stem))
    {
        return FOLIO_STATE_NAME_TAKEN;
    }
    /* 改名の前に既存の保存を成功させる。閲覧中は save_note が何もせずに戻る。 */
    enum folio_state_outcome saved = save_note(state, units, count, NOTE_SAVE_UNCONDITIONAL);
    if (saved != FOLIO_STATE_READY)
    {
        return saved;
    }
    /* 一致集合の作り直しは完了した改名を受け取る `adopt_rename` が行う（補正 3）。 */
    return rename_selected(state, name);
}

static bool trashing_current(const struct folio_state *_Nonnull state, struct note_ref target)
{
    return state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == target.category &&
           state->selected_note == target.note;
}

static enum folio_state_outcome from_trash(enum trash_outcome outcome)
{
    switch (outcome)
    {
    case TRASH_TRASHED:
    case TRASH_ABSENT:
        return FOLIO_STATE_READY;
    case TRASH_UNAVAILABLE:
        return FOLIO_STATE_TRASH_UNAVAILABLE;
    case TRASH_BUSY:
        return FOLIO_STATE_TRASH_BUSY;
    case TRASH_FAILED:
        return FOLIO_STATE_TRASH_FAILED;
    }
    return FOLIO_STATE_TRASH_FAILED;
}

/* 名前の借用は旧台帳を破棄するより先に使い切る。採用後の同期は確保を要しない。 */
static enum folio_state_outcome publish_trash(struct folio_state *_Nonnull state,
                                              struct note_ref target,
                                              struct note_ledger *_Nonnull ledger,
                                              struct index_filter *_Nullable filter)
{
    const char *_Nonnull category = category_ledger_name(state->categories, target.category);
    const char *_Nonnull note = note_ledger_name(state->notes[target.category], target.note);
    bool current = trashing_current(state, target);
    enum folio_state_outcome moved =
        from_trash(state->port.trash_note(state->port.adapter, category, note));
    if (moved != FOLIO_STATE_READY)
    {
        note_ledger_destroy(ledger);
        index_filter_destroy(filter);
        return moved;
    }
    note_corpus_remove(state->corpus, category, note);
    note_ledger_destroy(state->notes[target.category]);
    state->notes[target.category] = ledger;
    index_changed(state);
    index_filter_destroy(state->filter);
    state->filter = filter;
    if (state->document == FOLIO_DOCUMENT_NAMED && state->selected_category == target.category &&
        state->selected_note > target.note)
    {
        state->selected_note -= 1;
    }
    if (state->cursor_any && state->cursor_kind == FOLIO_CURSOR_NOTE &&
        state->cursor_category == target.category)
    {
        if (state->cursor_note == target.note)
        {
            cursor_to_category(state, target.category);
        }
        if (state->cursor_note > target.note)
        {
            state->cursor_note -= 1;
        }
    }
    /* The open note is the one just binned: its place names no note of the new ledger (the
     * caller empties the document next). Reading the index again must neither keep nor follow
     * the note that now stands at that place (review of #1069, stage 4). */
    if (current)
    {
        state->selected_note = note_ledger_count(ledger);
    }
    state->index_pending = true;
    state->pending_category = target.category;
    return published_index(synchronize_index(state));
}

/* 現在文書の空の表示も port より先に用意する。失敗なら準備物を捨てるだけである。 */
static enum folio_state_outcome finish_trash(struct folio_state *_Nonnull state,
                                             struct note_ref target,
                                             struct note_ledger *_Nonnull ledger,
                                             struct index_filter *_Nullable filter)
{
    bool current = trashing_current(state, target);
    struct note_text *_Nullable body = nullptr;
    struct markdown_rtf *_Nullable pane = nullptr;
    if (current && (note_text_create("", 0, &body) != NOTE_TEXT_ACCEPTED ||
                    markdown_rtf_empty(&state->palette, ui_font_face(folio_state_language(state)),
                                       &pane) != MARKDOWN_RTF_CONVERTED))
    {
        note_text_destroy(body);
        markdown_rtf_destroy(pane);
        note_ledger_destroy(ledger);
        index_filter_destroy(filter);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome outcome = publish_trash(state, target, ledger, filter);
    if (current && (outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_LEDGER_STALE))
    {
        note_text_destroy(state->body);
        discard_view(state);
        state->body = body;
        state->pane = pane;
        document_changed(state, FOLIO_DOCUMENT_NONE);
        folio_state_refresh_note_times(state);
        state->mode = PANE_MODE_VIEW;
        settle_cursor(state);
        replace_preview_destroy(state->preview);
        state->preview = nullptr;
        state->replace_offset = 0;
        folio_state_close_history(state);
        return outcome;
    }
    if (outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_LEDGER_STALE)
    {
        settle_cursor(state);
    }
    note_text_destroy(body);
    markdown_rtf_destroy(pane);
    return outcome;
}

/* The note to bin is there, and the repairs before the intent left the list as the caller saw
 * it (synchronize_places). */
static enum folio_state_outcome trash_target(struct folio_state *_Nonnull state,
                                             struct note_ref target)
{
    if (target.category >= state->notes_count)
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (target.note >= note_ledger_count(state->notes[target.category]))
    {
        return FOLIO_STATE_NO_SUCH_NOTE;
    }
    return synchronize_places(state);
}

enum folio_state_outcome folio_state_trash_note(struct folio_state *_Nonnull state,
                                                struct note_ref target,
                                                const char16_t *_Nonnull units, size_t count)
{
    enum folio_state_outcome synced = trash_target(state, target);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    if (trashing_current(state, target) && state->mode == PANE_MODE_EDIT)
    {
        enum folio_state_outcome saved = save_note(state, units, count, NOTE_SAVE_UNCONDITIONAL);
        if (saved != FOLIO_STATE_READY)
        {
            return saved;
        }
    }
    struct note_ledger *_Nullable ledger = nullptr;
    enum folio_state_outcome reserved =
        from_note_ledger(note_ledger_removed(state->notes[target.category], target.note, &ledger));
    if (reserved != FOLIO_STATE_READY)
    {
        return reserved;
    }
    struct index_filter *_Nullable filter = nullptr;
    if (state->filter != nullptr &&
        index_filter_removed(state->filter, target, &filter) != INDEX_FILTER_ACCEPTED)
    {
        note_ledger_destroy(ledger);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return finish_trash(state, target, ledger, filter);
}

/* カテゴリ名もディレクトリ名なので、大小文字だけ違う名前を衝突として断る（ADR 0039 の決定 10）。 */
static bool holds_folded_category(const struct folio_state *_Nonnull state,
                                  const char *_Nonnull name)
{
    size_t count = category_ledger_count(state->categories);
    for (size_t index = 0; index < count; ++index)
    {
        if (same_folded(category_ledger_name(state->categories, index), name))
        {
            return true;
        }
    }
    return false;
}

static enum folio_state_outcome from_category_rename(enum rename_plan_outcome outcome)
{
    switch (outcome)
    {
    case RENAME_PLAN_ACCEPTED:
        return FOLIO_STATE_READY;
    case RENAME_PLAN_INVALID:
        return FOLIO_STATE_CATEGORY_NAME_INVALID;
    case RENAME_PLAN_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_CATEGORY_NAME_INVALID;
}

static enum folio_state_outcome
prepare_category_rename(struct folio_state *_Nonnull state,
                        const struct category_rename_target *_Nonnull target)
{
    struct rename_plan *_Nullable intent = nullptr;
    enum folio_state_outcome prepared = from_category_rename(
        rename_plan_create_category(state->categories, target->category, target->name, &intent));
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
    if (note_corpus_create(&state->rename_corpus) != NOTE_CORPUS_ACCEPTED)
    {
        rename_plan_destroy(intent);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return apply_prepared_rename(state, intent, target->category);
}

enum folio_state_outcome
folio_state_rename_category(struct folio_state *_Nonnull state,
                            const struct category_rename_target *_Nonnull target,
                            const char16_t *_Nonnull units, size_t count)
{
    if (folio_state_filtering(state))
    {
        return FOLIO_STATE_FILTERED;
    }
    if (target->category >= folio_state_category_count(state))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    const char *_Nonnull name = category_name_text(target->name);
    if (strcmp(category_ledger_name(state->categories, target->category), name) == 0)
    {
        return FOLIO_STATE_NAME_UNCHANGED;
    }
    if (holds_folded_category(state, name))
    {
        return FOLIO_STATE_CATEGORY_NAME_TAKEN;
    }
    if (state->document == FOLIO_DOCUMENT_NAMED && state->mode == PANE_MODE_EDIT &&
        state->selected_category == target->category)
    {
        enum folio_state_outcome saved = save_note(state, units, count, NOTE_SAVE_UNCONDITIONAL);
        if (saved != FOLIO_STATE_READY)
        {
            return saved;
        }
    }
    return prepare_category_rename(state, target);
}

/* 挿した台帳の結果。MALFORMED は完全一致の重複なので名前の衝突として返す。 */
static enum folio_state_outcome from_inserted(enum category_ledger_outcome outcome)
{
    switch (outcome)
    {
    case CATEGORY_LEDGER_ACCEPTED:
        return FOLIO_STATE_READY;
    case CATEGORY_LEDGER_MALFORMED:
    case CATEGORY_LEDGER_UNSUPPORTED_VERSION:
        return FOLIO_STATE_CATEGORY_NAME_TAKEN;
    case CATEGORY_LEDGER_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_CATEGORY_NAME_TAKEN;
}

/* 確保した台帳と配列を捨てる。配列の count 番目は新しいカテゴリの空の索引台帳で、
 * それより前は state が持っている借り物なので壊さない。 */
static void discard_category(struct category_ledger *_Nullable ledger,
                             struct note_ledger *_Nonnull *_Nullable notes, size_t count)
{
    category_ledger_destroy(ledger);
    if (notes != nullptr)
    {
        note_ledger_destroy(notes[count]);
    }
    free(notes);
}

/* 新しい台帳（末尾・既定の色・展開）と、1 つ伸ばした索引台帳の配列を、フォルダを作るより先に
 * 確保する（ADR 0039 の決定 3）。失敗したら何も残さない。 */
static enum folio_state_outcome
reserve_category(const struct folio_state *_Nonnull state,
                 const struct category_name *_Nonnull name,
                 struct category_ledger *_Nullable *_Nonnull ledger,
                 struct note_ledger *_Nonnull *_Nullable *_Nonnull notes)
{
    size_t count = category_ledger_count(state->categories);
    enum folio_state_outcome outcome =
        from_inserted(category_ledger_inserted(state->categories, count, name, ledger));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    /* 末尾の 1 つは load_all_notes と同じ null の番兵。 */
    struct note_ledger *_Nonnull *_Nullable grown = calloc(count + 2, sizeof *grown);
    outcome = grown == nullptr ? FOLIO_STATE_OUT_OF_MEMORY
                               : from_note_ledger(note_ledger_empty(&grown[count]));
    if (outcome != FOLIO_STATE_READY)
    {
        discard_category(*ledger, grown, count);
        *ledger = nullptr;
        return outcome;
    }
    for (size_t index = 0; index < count; ++index)
    {
        grown[index] = state->notes[index];
    }
    *notes = grown;
    return FOLIO_STATE_READY;
}

/* フォルダを作った後で台帳と配列を差し替え、カーソルを新しいカテゴリ行へ置く（決定 10）。
 * 空のカテゴリ行は止まる行（ADR 0015）。選択・文書・モード・写し・スクロール量は触らない。 */
static void adopt_category(struct folio_state *_Nonnull state,
                           struct category_ledger *_Nonnull ledger,
                           struct note_ledger *_Nonnull *_Nonnull notes)
{
    size_t category = category_ledger_count(state->categories);
    category_ledger_destroy(state->categories);
    state->categories = ledger;
    index_changed(state);
    free(state->notes);
    state->notes = notes;
    state->notes_count = category + 1;
    cursor_to_category(state, category);
}

enum folio_state_outcome folio_state_create_category(struct folio_state *_Nonnull state,
                                                     const struct category_name *_Nonnull name)
{
    if (folio_state_filtering(state))
    {
        return FOLIO_STATE_FILTERED;
    }
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    const char *_Nonnull text = category_name_text(name);
    if (holds_folded_category(state, text))
    {
        return FOLIO_STATE_CATEGORY_NAME_TAKEN;
    }
    struct category_ledger *_Nullable ledger = nullptr;
    struct note_ledger *_Nonnull *_Nullable notes = nullptr;
    enum folio_state_outcome reserved = reserve_category(state, name, &ledger, &notes);
    if (reserved != FOLIO_STATE_READY)
    {
        return reserved;
    }
    enum persistence_outcome created = state->port.create_category(state->port.adapter, text);
    if (created != PERSISTENCE_STORED)
    {
        discard_category(ledger, notes, category_ledger_count(state->categories));
        return created == PERSISTENCE_NAME_TAKEN ? FOLIO_STATE_CATEGORY_NAME_TAKEN
                                                 : FOLIO_STATE_CATEGORY_NOT_CREATED;
    }
    /* フォルダが真実なので、台帳を書けなくても採用し、次に台帳を書く操作で揃える（決定 3）。 */
    enum persistence_outcome stored =
        state->port.write_category_ledger(state->port.adapter, ledger);
    adopt_category(state, ledger, notes);
    return stored == PERSISTENCE_STORED ? FOLIO_STATE_READY : FOLIO_STATE_CATEGORY_LEDGER_STALE;
}

static enum folio_state_outcome from_category_removed(enum category_remove_outcome outcome)
{
    switch (outcome)
    {
    case CATEGORY_REMOVE_REMOVED:
    case CATEGORY_REMOVE_ABSENT:
        return FOLIO_STATE_READY;
    case CATEGORY_REMOVE_HAS_FILES:
        return FOLIO_STATE_CATEGORY_HAS_FILES;
    case CATEGORY_REMOVE_HISTORY_NOT_RECYCLED:
        return FOLIO_STATE_CATEGORY_HISTORY_NOT_RECYCLED;
    case CATEGORY_REMOVE_FAILED:
        return FOLIO_STATE_CATEGORY_REMOVE_FAILED;
    }
    return FOLIO_STATE_CATEGORY_REMOVE_FAILED;
}

/* 新しい配列は既存の note_ledger を借り、末尾の null 番兵まで先に確保する。 */
static enum folio_state_outcome
reserve_category_removal(const struct folio_state *_Nonnull state, size_t target,
                         struct category_ledger *_Nullable *_Nonnull ledger,
                         struct note_ledger *_Nonnull *_Nullable *_Nonnull notes)
{
    enum folio_state_outcome outcome =
        from_category_ledger(category_ledger_removed(state->categories, target, ledger));
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    size_t count = category_ledger_count(state->categories);
    struct note_ledger *_Nonnull *_Nullable shortened = calloc(count, sizeof *shortened);
    if (shortened == nullptr)
    {
        category_ledger_destroy(*ledger);
        *ledger = nullptr;
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    for (size_t index = 0; index + 1 < count; ++index)
    {
        shortened[index] = state->notes[index < target ? index : index + 1];
    }
    *notes = shortened;
    return FOLIO_STATE_READY;
}

/* 文書・本文・写しを触らず、削除したカテゴリより後の添字だけ詰める（ADR 0039 補正 5）。 */
static void adopt_category_removal(struct folio_state *_Nonnull state, size_t target,
                                   struct category_ledger *_Nonnull ledger,
                                   struct note_ledger *_Nonnull *_Nonnull notes)
{
    note_ledger_destroy(state->notes[target]);
    category_ledger_destroy(state->categories);
    state->categories = ledger;
    index_changed(state);
    free(state->notes);
    state->notes = notes;
    state->notes_count -= 1;
    if (state->document != FOLIO_DOCUMENT_NONE && state->selected_category > target)
    {
        state->selected_category -= 1;
    }
    if (state->cursor_any && state->cursor_category > target)
    {
        state->cursor_category -= 1;
    }
    settle_cursor(state);
}

enum folio_state_outcome folio_state_delete_category(struct folio_state *_Nonnull state,
                                                     size_t category)
{
    if (folio_state_filtering(state))
    {
        return FOLIO_STATE_FILTERED;
    }
    if (!holds_category(state, category))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    enum folio_state_outcome outcome = synchronize(state);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    if (note_ledger_count(state->notes[category]) != 0 ||
        (state->document == FOLIO_DOCUMENT_UNTITLED && state->selected_category == category))
    {
        return FOLIO_STATE_CATEGORY_NOT_EMPTY;
    }
    struct category_ledger *_Nullable ledger = nullptr;
    struct note_ledger *_Nonnull *_Nullable notes = nullptr;
    outcome = reserve_category_removal(state, category, &ledger, &notes);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    outcome = from_category_removed(state->port.remove_category(
        state->port.adapter, category_ledger_name(state->categories, category)));
    if (outcome != FOLIO_STATE_READY)
    {
        category_ledger_destroy(ledger);
        free(notes);
        return outcome;
    }
    enum persistence_outcome stored =
        state->port.write_category_ledger(state->port.adapter, ledger);
    adopt_category_removal(state, category, ledger, notes);
    return stored == PERSISTENCE_STORED ? FOLIO_STATE_READY : FOLIO_STATE_CATEGORY_LEDGER_STALE;
}

enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                const char16_t *_Nonnull units, size_t count)
{
    return save_note(state, units, count, NOTE_SAVE_UNCONDITIONAL);
}

enum folio_state_outcome folio_state_store_note_checked(struct folio_state *_Nonnull state,
                                                        const char16_t *_Nonnull units,
                                                        size_t count)
{
    return save_note(state, units, count, NOTE_SAVE_CHECK_CURRENT);
}

/* 保存済み本文との差。note_changed と unsaved が共有する比較の 1 本（ARC-001）。 */
static enum folio_state_outcome note_difference(const struct folio_state *_Nonnull state,
                                                const char16_t *_Nonnull units, size_t count,
                                                enum folio_note_change *_Nonnull out)
{
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
    return note_difference(state, units, count, out);
}

enum folio_note_change folio_state_unsaved(const struct folio_state *_Nonnull state,
                                           const char16_t *_Nonnull units, size_t count)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_NOTE_SAME;
    }
    enum folio_note_change change = FOLIO_NOTE_CHANGED;
    return note_difference(state, units, count, &change) == FOLIO_STATE_READY ? change
                                                                              : FOLIO_NOTE_CHANGED;
}

uint64_t folio_state_store_revision(const struct folio_state *_Nonnull state)
{
    return state->store_revision;
}

enum folio_state_outcome folio_state_end_edit(struct folio_state *_Nonnull state,
                                              const char16_t *_Nonnull units, size_t count)
{
    enum folio_state_outcome outcome = save_note(state, units, count, NOTE_SAVE_UNCONDITIONAL);
    if (outcome == FOLIO_STATE_READY)
    {
        state->mode = PANE_MODE_VIEW;
    }
    return outcome;
}

enum folio_state_outcome folio_state_discard_edits(const struct folio_state *_Nonnull state)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    if (state->mode == PANE_MODE_VIEW)
    {
        return FOLIO_STATE_NOT_EDITING;
    }
    return FOLIO_STATE_READY;
}

enum pane_mode folio_state_pane_mode(const struct folio_state *_Nonnull state)
{
    return state->mode;
}

enum folio_layout_mode folio_state_layout_mode(const struct folio_state *_Nonnull state)
{
    return state->layout_mode;
}

void folio_state_set_layout_mode(struct folio_state *_Nonnull state, enum folio_layout_mode mode)
{
    state->layout_mode = mode;
}

bool folio_state_outline(const struct folio_state *_Nonnull state)
{
    return state->outline;
}

void folio_state_set_outline(struct folio_state *_Nonnull state, bool open)
{
    state->outline = open;
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

/* 一致の入れ物を capacity 件まで広げる。縮めない。空（下見が引き取った後）なら新しく確保する。 */
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

/* 総数が入れ物を超えたら広げて、もう 1 度だけ走査する（決定 2）。入れ物は呼び出し側が
 * reserve_matches で先に用意しておく（空のまま adapter へ渡さない）。 */
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

/* 新しい下見を作って古いものと入れ替える。作れなければ古い下見をそのまま保つ（決定 6）。
 * 下見は一致の配列を引き取るので、作れたら入れ物は空に戻す（補正 25）。作れなければ
 * 配列は入れ物のまま残り、次の走査で使い回す。 */
static enum folio_state_outcome adopt_preview(struct folio_state *_Nonnull state,
                                              const struct replace_request *_Nonnull request,
                                              struct regex_matches *_Nonnull matches)
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
    state->found = nullptr;
    state->found_capacity = 0;
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
    if (!reserve_matches(state, initial_matches))
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
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

bool folio_state_has_replace_preview(const struct folio_state *_Nonnull state)
{
    return state->preview != nullptr;
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
    if (state->filter != nullptr)
    {
        row_mapping_changed(state);
    }
    utf8_text_destroy(state->index_term);
    index_filter_destroy(state->filter);
    state->index_term = nullptr;
    state->filter = nullptr;
}

static void cancel_index_request(struct folio_state *_Nonnull state)
{
    restart_corpus_load(state);
    utf8_text_destroy(state->index_request);
    state->index_request = nullptr;
    note_tags_destroy(state->index_tags);
    state->index_tags = nullptr;
    state->tag_mode = TAG_MATCH_ALL;
}

/* 新しい語と一致集合を受け取り、前のものと入れ替える。 */
static void adopt_index_filter(struct folio_state *_Nonnull state, struct utf8_text *_Nonnull term,
                               struct index_filter *_Nonnull filter)
{
    clear_index_filter(state);
    state->index_term = term;
    state->filter = filter;
    row_mapping_changed(state);
}

static enum folio_state_outcome synchronize_filter(struct folio_state *_Nonnull state)
{
    return state->rename != nullptr || state->copy_pending ? synchronize(state) : FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_set_index_filter(struct folio_state *_Nonnull state,
                                                      const char16_t *_Nonnull units, size_t count)
{
    if (count == 0 && (state->index_tags == nullptr || note_tags_count(state->index_tags) == 0))
    {
        enum tag_match_mode mode = state->tag_mode;
        cancel_index_request(state);
        state->tag_mode = mode;
        clear_index_filter(state);
        state->scroll = 0;
        settle_cursor(state);
        return FOLIO_STATE_READY;
    }
    enum folio_state_outcome synced = synchronize_filter(state);
    if (synced != FOLIO_STATE_READY)
    {
        return synced;
    }
    struct utf8_text *_Nullable term = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(units, count, &term);
    if (converted != UTF8_TEXT_CONVERTED)
    {
        return converted == UTF8_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                    : FOLIO_STATE_SEARCH_MALFORMED;
    }
    discard_index_build(state);
    clear_index_filter(state);
    utf8_text_destroy(state->index_request);
    state->index_request = term;
    state->scroll = 0;
    return FOLIO_STATE_READY;
}

static void adopt_tag_query(struct folio_state *_Nonnull state, struct note_tags *_Nonnull tags,
                            struct utf8_text *_Nonnull term, enum tag_match_mode mode)
{
    discard_index_build(state);
    if (state->index_request == term)
        state->index_request = nullptr;
    if (state->index_term == term)
        state->index_term = nullptr;
    clear_index_filter(state);
    utf8_text_destroy(state->index_request);
    note_tags_destroy(state->index_tags);
    state->index_tags = tags;
    state->tag_mode = mode;
    state->index_request = term;
    state->scroll = 0;
    if (note_tags_count(tags) == 0 && utf8_text_length(term) == 0)
    {
        cancel_index_request(state);
        state->tag_mode = mode;
        settle_cursor(state);
    }
}

enum folio_state_outcome folio_state_set_tag_filter(struct folio_state *_Nonnull state,
                                                    const struct note_tags *_Nonnull tags,
                                                    enum tag_match_mode mode)
{
    enum folio_state_outcome synced = synchronize_filter(state);
    if (synced != FOLIO_STATE_READY)
        return synced;
    struct note_tags *_Nullable owned = nullptr;
    if (note_tags_copy(tags, &owned) != NOTE_TAGS_READY)
        return FOLIO_STATE_OUT_OF_MEMORY;
    struct utf8_text *_Nullable term =
        state->index_request == nullptr ? state->index_term : state->index_request;
    if (term == nullptr && utf8_text_create(u"", 0, &term) != UTF8_TEXT_CONVERTED)
    {
        note_tags_destroy(owned);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    adopt_tag_query(state, owned, term, mode);
    return FOLIO_STATE_READY;
}

const struct note_tags *_Nullable folio_state_tag_filter(const struct folio_state *_Nonnull state)
{
    return state->index_tags;
}

enum tag_match_mode folio_state_tag_match_mode(const struct folio_state *_Nonnull state)
{
    return state->tag_mode;
}

/* 完成するまで採用せず、最新の要求だけを既存build/adoptへ渡す。 */
static enum folio_state_outcome finish_index_request(struct folio_state *_Nonnull state)
{
    struct index_filter *_Nullable filter = nullptr;
    switch (index_filter_finish(&state->index_build, &filter))
    {
    case INDEX_FILTER_FINISH_NOT_READY:
        return FOLIO_STATE_READY;
    case INDEX_FILTER_FINISH_OUT_OF_MEMORY:
        cancel_index_request(state);
        return FOLIO_STATE_OUT_OF_MEMORY;
    case INDEX_FILTER_FINISH_ACCEPTED:
        break;
    }
    struct utf8_text *_Nonnull term = state->index_request;
    state->index_request = nullptr;
    adopt_index_filter(state, term, filter);
    state->scroll = 0;
    settle_cursor(state);
    return FOLIO_STATE_READY;
}

void folio_state_close_tag_catalog(struct folio_state *_Nonnull state)
{
    tag_catalog_destroy(state->catalog);
    state->catalog = nullptr;
    state->catalog_status = TAG_CATALOG_UNREQUESTED;
}

void folio_state_request_tag_catalog(struct folio_state *_Nonnull state)
{
    folio_state_close_tag_catalog(state);
    state->catalog_status = TAG_CATALOG_PREPARING;
}

enum tag_catalog_status folio_state_tag_catalog_status(const struct folio_state *_Nonnull state)
{
    return state->catalog_status;
}

const struct tag_catalog *_Nonnull folio_state_tag_catalog(const struct folio_state *_Nonnull state)
{
    return state->catalog;
}

bool folio_state_index_work_pending(const struct folio_state *_Nonnull state)
{
    return state->index_request != nullptr || state->catalog_status == TAG_CATALOG_PREPARING ||
           state->tag_phase == TAG_MANAGEMENT_PREPARING;
}

static void catalog_notes_destroy(const struct note_tags *_Nullable *_Nullable notes)
{
    free(notes);
}

static enum folio_state_outcome build_tag_catalog(struct folio_state *_Nonnull state)
{
    /* Every failed allocation, including the input array, ends this explicit request. */
    state->catalog_status = TAG_CATALOG_FAILED;
    size_t count = 0;
    if (!index_note_count(state, &count) || count >= SIZE_MAX / sizeof(const struct note_tags *))
        return FOLIO_STATE_OUT_OF_MEMORY;
    const struct note_tags *_Nullable *_Nullable notes = malloc((count + 1) * sizeof *notes);
    if (notes == nullptr)
        return FOLIO_STATE_OUT_OF_MEMORY;
    size_t at = 0;
    for (size_t category = 0; category < state->notes_count; ++category)
        for (size_t note = 0; note < note_ledger_count(state->notes[category]); ++note)
            notes[at++] = filter_entry(state, located(category, note), true).tags;
    enum tag_catalog_outcome built = tag_catalog_create(notes, count, &state->catalog);
    catalog_notes_destroy(notes);
    state->catalog_status = built == TAG_CATALOG_READY ? TAG_CATALOG_AVAILABLE : TAG_CATALOG_FAILED;
    return built == TAG_CATALOG_READY ? FOLIO_STATE_READY : FOLIO_STATE_OUT_OF_MEMORY;
}

static void stop_index_preparation(struct folio_state *_Nonnull state,
                                   enum folio_state_outcome reason)
{
    if (state->index_request != nullptr)
        cancel_index_request(state);
    if (state->catalog_status == TAG_CATALOG_PREPARING)
        state->catalog_status = TAG_CATALOG_FAILED;
    if (state->tag_phase == TAG_MANAGEMENT_PREPARING)
    {
        state->tag_notice = reason;
        state->tag_phase = TAG_MANAGEMENT_RESULT;
    }
}

static enum folio_state_outcome prepare_corpus_entry(struct folio_state *_Nonnull state)
{
    struct note_ref ref = located(state->corpus_category, state->corpus_note);
    enum folio_state_outcome loaded =
        discovery_source_reused(state, ref) ? FOLIO_STATE_READY : load_corpus_note(state, ref);
    if (loaded == FOLIO_STATE_READY && state->tag_phase == TAG_MANAGEMENT_PREPARING)
        loaded = prepare_tag_management_note(state);
    if (loaded == FOLIO_STATE_READY)
        state->corpus_note += 1;
    return loaded;
}

static enum folio_state_outcome prepare_corpus_next(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome loaded = synchronize_filter(state);
    if (loaded == FOLIO_STATE_READY && corpus_next_present(state))
        loaded = prepare_corpus_entry(state);
    return loaded;
}

static enum folio_state_outcome prepare_index_corpus(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome loaded = prepare_corpus_next(state);
    if (loaded != FOLIO_STATE_READY)
    {
        stop_index_preparation(state, loaded);
        return loaded;
    }
    if (state->corpus_loaded && state->tag_phase == TAG_MANAGEMENT_PREPARING)
        state->tag_phase = TAG_MANAGEMENT_PREVIEW;
    if (state->corpus_loaded && state->catalog_status == TAG_CATALOG_PREPARING)
        return build_tag_catalog(state);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_continue_index_filter(struct folio_state *_Nonnull state)
{
    if (!folio_state_index_work_pending(state))
        return FOLIO_STATE_READY;
    enum folio_state_outcome loaded = prepare_index_corpus(state);
    if (loaded != FOLIO_STATE_READY || state->index_request == nullptr)
        return loaded;
    if (!state->corpus_loaded)
        return FOLIO_STATE_READY;
    if (state->index_build == nullptr)
    {
        struct index_filter_query query = list_query(state, state->index_request);
        enum folio_state_outcome created = create_index_build(state, &query, &state->index_build);
        if (created != FOLIO_STATE_READY)
        {
            cancel_index_request(state);
            return created;
        }
    }
    struct index_filter_progress progress = {
        .cursor = state->index_cursor, .work = index_matching_work_budget, .uses_body = true};
    step_filter(state, state->index_build, &progress);
    state->index_cursor = progress.cursor;
    return finish_index_request(state);
}

bool folio_state_index_filter_pending(const struct folio_state *_Nonnull state)
{
    return state->index_request != nullptr;
}

bool folio_state_filtering(const struct folio_state *_Nonnull state)
{
    return state->index_request != nullptr || state->filter != nullptr ||
           state->note_scope == NOTE_SCOPE_FAVORITES;
}

const char *_Nonnull folio_state_index_filter_term(const struct folio_state *_Nonnull state)
{
    if (state->index_request != nullptr)
    {
        return utf8_text_bytes(state->index_request);
    }
    return state->index_term == nullptr ? "" : utf8_text_bytes(state->index_term);
}

size_t folio_state_index_filter_count(const struct folio_state *_Nonnull state)
{
    if (state->note_scope == NOTE_SCOPE_ALL)
        return state->filter == nullptr ? total_notes(state) : index_filter_count(state->filter);
    size_t count = 0;
    for (size_t category = 0; category < state->notes_count; ++category)
        for (size_t note = 0; note < note_ledger_count(state->notes[category]); ++note)
            if (shown_note(state, category, note))
                count += 1;
    return count;
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
    out->from = rename_plan_from(state->rename);
    out->to = rename_plan_to(state->rename);
    out->kind = rename_plan_kind(state->rename);
    return true;
}

struct pane_title_view folio_state_pane_title(const struct folio_state *_Nonnull state)
{
    struct pane_title_view title = {.any = false,
                                    .recovering = PANE_RECOVERY_NONE,
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
    if (state->rename != nullptr && state->selected_category == state->rename_category)
    {
        switch (rename_plan_kind(state->rename))
        {
        case RENAME_KIND_NOTE:
            title.recovering = PANE_RECOVERY_NOTE;
            break;
        case RENAME_KIND_CATEGORY:
            title.recovering = PANE_RECOVERY_CATEGORY;
            break;
        }
    }
    title.ordinal = state->selected_category + 1;
    title.category = category_ledger_name(state->categories, state->selected_category);
    title.note =
        state->document == FOLIO_DOCUMENT_UNTITLED
            ? ui_text_line(UI_TEXT_TITLE_UNTITLED, folio_state_language(state))
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

/* 本文から編集用の RTF を作って slot に置く。確保に失敗すれば前の slot を保つ（C-008）。 */
static enum folio_state_outcome place_edit_rtf(struct folio_state *_Nonnull state,
                                               const struct note_text *_Nonnull body)
{
    struct markdown_rtf *_Nullable rendered = nullptr;
    if (markdown_rtf_plain(body, &state->palette, ui_font_face(folio_state_language(state)),
                           &rendered) != MARKDOWN_RTF_CONVERTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    markdown_rtf_destroy(state->edit);
    state->edit = rendered;
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_pane_edit_rtf(struct folio_state *_Nonnull state)
{
    return place_edit_rtf(state, state->body);
}

const char *_Nonnull folio_state_edit_rtf(const struct folio_state *_Nonnull state)
{
    return state->edit == nullptr ? "" : markdown_rtf_text(state->edit);
}

size_t folio_state_edit_rtf_length(const struct folio_state *_Nonnull state)
{
    return state->edit == nullptr ? 0 : markdown_rtf_length(state->edit);
}

void folio_state_close_history(struct folio_state *_Nonnull state)
{
    for (size_t index = 0; index < state->history_count; ++index)
    {
        note_text_destroy(state->history[index]);
        state->history[index] = nullptr;
    }
    state->history_count = 0;
}

/* 1 版を読んで一覧の末尾に足す。無い版は飛ばし、読めない版は本文の無い行として持つ（ARC-009）。
 * 確保に失敗したら false で、その版は足さない。 */
static bool read_version(struct folio_state *_Nonnull state,
                         const struct history_version *_Nonnull which)
{
    struct note_text *_Nullable body = nullptr;
    enum persistence_outcome read = state->port.read_history(state->port.adapter, which, &body);
    switch (read)
    {
    case PERSISTENCE_ABSENT:
        return true;
    case PERSISTENCE_OUT_OF_MEMORY:
        return false;
    case PERSISTENCE_LOADED:
    case PERSISTENCE_UNREADABLE:
    case PERSISTENCE_MALFORMED:
    case PERSISTENCE_STORED:
    case PERSISTENCE_UNWRITABLE:
    case PERSISTENCE_NAME_TAKEN:
        break;
    }
    /* LOADED 以外で本文が返ることは無いが、返っても読めない行として捨てる。 */
    if (read != PERSISTENCE_LOADED)
    {
        note_text_destroy(body);
        body = nullptr;
    }
    state->history[state->history_count] = body;
    state->history_versions[state->history_count] = which->version;
    state->history_count += 1;
    return true;
}

enum folio_state_outcome folio_state_open_history(struct folio_state *_Nonnull state)
{
    if (category_rename_pending(state) && state->document != FOLIO_DOCUMENT_NONE &&
        state->selected_category == state->rename_category)
    {
        enum folio_state_outcome synced = synchronize(state);
        if (synced != FOLIO_STATE_READY)
        {
            return synced;
        }
    }
    folio_state_close_history(state);
    if (state->document == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    if (state->document == FOLIO_DOCUMENT_UNTITLED)
    {
        return FOLIO_STATE_NAME_REQUIRED;
    }
    struct history_version which = {
        .category = category_ledger_name(state->categories, state->selected_category),
        .note = note_ledger_name(state->notes[state->selected_category], state->selected_note),
        .version = 0,
    };
    for (size_t version = 1; version <= note_history_depth; ++version)
    {
        which.version = version;
        if (!read_version(state, &which))
        {
            folio_state_close_history(state);
            return FOLIO_STATE_OUT_OF_MEMORY;
        }
    }
    return state->history_count == 0 ? FOLIO_STATE_HISTORY_EMPTY : FOLIO_STATE_READY;
}

size_t folio_state_history_count(const struct folio_state *_Nonnull state)
{
    return state->history_count;
}

bool folio_state_history_row(const struct folio_state *_Nonnull state, size_t index,
                             struct history_row_view *_Nonnull out)
{
    if (index >= state->history_count)
    {
        return false;
    }
    const struct note_text *_Nullable body = state->history[index];
    out->version = state->history_versions[index];
    out->readable = body != nullptr;
    out->first_line = "";
    out->first_line_length = 0;
    if (body != nullptr)
    {
        size_t start = 0;
        note_text_first_line(body, &start, &out->first_line_length);
        out->first_line = note_text_bytes(body) + start;
    }
    return true;
}

const struct note_text *_Nullable folio_state_history_body(const struct folio_state *_Nonnull state,
                                                           size_t index)
{
    return index < state->history_count ? state->history[index] : nullptr;
}

enum folio_state_outcome folio_state_restore_history(struct folio_state *_Nonnull state,
                                                     size_t index)
{
    const struct note_text *_Nullable body = folio_state_history_body(state, index);
    if (body == nullptr)
    {
        return FOLIO_STATE_HISTORY_UNREADABLE;
    }
    /* 選んだ版も editor へ渡すと別の文字に変わるなら、本文も mode も一覧も変えない（#242）。 */
    if (note_text_editability(body) == NOTE_TEXT_EDITOR_ALTERS)
    {
        return FOLIO_STATE_NOTE_UNEDITABLE;
    }
    /* 一覧は名前のある文書でしか開けない。今の本文が編集で保てないときは begin_edit が断る。 */
    return folio_state_begin_edit(state);
}

/* 値ごとの失敗の 1 行（ADR 0027 の決定 1）。列挙の全値がちょうど 1 度ずつ並ぶことは CNF-009 が
 * 字句で守るので、添字の範囲検査は書かない（決定 3）。 */
/* 結果の値ごとの文言の ID（ADR 0030 の決定 2）。文言そのものは core の ui_text が持ち、
 * ここは outcome と ID の対応だけを持つ。網羅は CNF-009 が守る（ADR 0027 の経路のまま）。
 * READY と CANCELLED は見せる 1 行が無いので UI_TEXT_EMPTY を共有する。 */
static const enum ui_text failure_lines[] = {
    [FOLIO_STATE_EXPORT_TOO_LARGE] = UI_TEXT_FAILURE_EXPORT_TOO_LARGE,
    [FOLIO_STATE_EXPORT_EXISTS] = UI_TEXT_FAILURE_EXPORT_EXISTS,
    [FOLIO_STATE_EXPORT_FAILED] = UI_TEXT_FAILURE_EXPORT_FAILED,
    [FOLIO_STATE_EXPORT_EXTERNAL_IMAGES] = UI_TEXT_IMAGES_EXPORT_EXTERNAL,
    [FOLIO_STATE_EXPORT_IMAGE_UNREADABLE] = UI_TEXT_FAILURE_EXPORT_IMAGES,
    [FOLIO_STATE_TRANSFER_UNAVAILABLE] = UI_TEXT_FAILURE_TRANSFER_UNAVAILABLE,

    [FOLIO_STATE_WEB_LINK_REJECTED] = UI_TEXT_FAILURE_WEB_LINK_REJECTED,
    [FOLIO_STATE_WEB_LINK_UNAVAILABLE] = UI_TEXT_FAILURE_WEB_LINK_UNAVAILABLE,
    [FOLIO_STATE_CODE_COPY_REJECTED] = UI_TEXT_FAILURE_CODE_COPY_REJECTED,
    [FOLIO_STATE_SOURCE_UNMAPPED] = UI_TEXT_FAILURE_SOURCE_UNMAPPED,
    [FOLIO_STATE_LEDGER_CHANGED_OUTSIDE] = UI_TEXT_FAILURE_LEDGER_CHANGED_OUTSIDE,
    [FOLIO_STATE_LEDGER_REFRESHED] = UI_TEXT_FAILURE_LEDGER_REFRESHED,
    [FOLIO_STATE_CLIPBOARD_BUSY] = UI_TEXT_FAILURE_CLIPBOARD_BUSY,
    [FOLIO_STATE_CLIPBOARD_UNAVAILABLE] = UI_TEXT_FAILURE_CLIPBOARD_UNAVAILABLE,
    [FOLIO_STATE_READY] = UI_TEXT_EMPTY,
    [FOLIO_STATE_DATA_UNREADABLE] = UI_TEXT_FAILURE_DATA_UNREADABLE,
    [FOLIO_STATE_LEDGER_MALFORMED] = UI_TEXT_FAILURE_LEDGER_MALFORMED,
    [FOLIO_STATE_STORE_FAILED] = UI_TEXT_FAILURE_STORE_FAILED,
    [FOLIO_STATE_NO_SUCH_CATEGORY] = UI_TEXT_FAILURE_NO_SUCH_CATEGORY,
    [FOLIO_STATE_NO_SUCH_NOTE] = UI_TEXT_FAILURE_NO_SUCH_NOTE,
    [FOLIO_STATE_NOTE_UNREADABLE] = UI_TEXT_FAILURE_NOTE_UNREADABLE,
    [FOLIO_STATE_NOTHING_SELECTED] = UI_TEXT_FAILURE_NOTHING_SELECTED,
    [FOLIO_STATE_NOT_EDITING] = UI_TEXT_FAILURE_NOT_EDITING,
    [FOLIO_STATE_NOTE_MALFORMED] = UI_TEXT_FAILURE_NOTE_MALFORMED,
    [FOLIO_STATE_NOTE_TOO_LARGE] = UI_TEXT_FAILURE_NOTE_TOO_LARGE,
    [FOLIO_STATE_NOTE_UNEDITABLE] = UI_TEXT_FAILURE_NOTE_UNEDITABLE,
    [FOLIO_STATE_NOTE_STORE_FAILED] = UI_TEXT_FAILURE_NOTE_STORE_FAILED,
    [FOLIO_STATE_HISTORY_FAILED] = UI_TEXT_FAILURE_HISTORY_FAILED,
    [FOLIO_STATE_UNSAVED_CHANGES] = UI_TEXT_FAILURE_UNSAVED_CHANGES,
    [FOLIO_STATE_NAME_TAKEN] = UI_TEXT_FAILURE_NAME_TAKEN,
    [FOLIO_STATE_NAME_UNCHANGED] = UI_TEXT_FAILURE_NAME_UNCHANGED,
    [FOLIO_STATE_LEDGER_STALE] = UI_TEXT_FAILURE_LEDGER_STALE,
    [FOLIO_STATE_LEDGER_UNSYNCED] = UI_TEXT_FAILURE_LEDGER_UNSYNCED,
    [FOLIO_STATE_RENAME_PENDING] = UI_TEXT_FAILURE_RENAME_PENDING,
    [FOLIO_STATE_RENAME_UNLOCKED] = UI_TEXT_FAILURE_RENAME_UNLOCKED,
    [FOLIO_STATE_RENAME_UNSUPPORTED] = UI_TEXT_FAILURE_RENAME_UNSUPPORTED,
    [FOLIO_STATE_RENAME_IDENTITY_FAILED] = UI_TEXT_FAILURE_RENAME_IDENTITY_FAILED,
    [FOLIO_STATE_RENAME_JOURNAL_FAILED] = UI_TEXT_FAILURE_RENAME_JOURNAL_FAILED,
    [FOLIO_STATE_RENAME_JOURNAL_BROKEN] = UI_TEXT_FAILURE_RENAME_JOURNAL_BROKEN,
    [FOLIO_STATE_RENAME_HALTED] = UI_TEXT_FAILURE_RENAME_HALTED,
    [FOLIO_STATE_SEARCH_MALFORMED] = UI_TEXT_FAILURE_SEARCH_MALFORMED,
    [FOLIO_STATE_DISCOVERY_QUERY_LIMIT] = UI_TEXT_FAILURE_DISCOVERY_QUERY_LIMIT,
    [FOLIO_STATE_DISCOVERY_QUERY_INVALID] = UI_TEXT_FAILURE_DISCOVERY_QUERY_INVALID,
    [FOLIO_STATE_FILTERED] = UI_TEXT_FAILURE_FILTERED,
    [FOLIO_STATE_ORDERED] = UI_TEXT_FAILURE_ORDERED,
    [FOLIO_STATE_MOVE_ROLLBACK_PENDING] = UI_TEXT_FAILURE_MOVE_ROLLBACK_PENDING,
    [FOLIO_STATE_SETTINGS_UNREADABLE] = UI_TEXT_FAILURE_SETTINGS_UNREADABLE,
    [FOLIO_STATE_SETTINGS_STORE_FAILED] = UI_TEXT_FAILURE_SETTINGS_STORE_FAILED,
    [FOLIO_STATE_APPEARANCE_UNAVAILABLE] = UI_TEXT_FAILURE_APPEARANCE_UNAVAILABLE,
    [FOLIO_STATE_PANE_UNAVAILABLE] = UI_TEXT_FAILURE_PANE_UNAVAILABLE,
    [FOLIO_STATE_PANE_LOAD_FAILED] = UI_TEXT_FAILURE_PANE_LOAD_FAILED,
    [FOLIO_STATE_PANE_RECOLOR_FAILED] = UI_TEXT_FAILURE_PANE_RECOLOR_FAILED,
    [FOLIO_STATE_REPLACE_NO_PATTERN] = UI_TEXT_FAILURE_REPLACE_NO_PATTERN,
    [FOLIO_STATE_REPLACE_BAD_PATTERN] = UI_TEXT_FAILURE_REPLACE_BAD_PATTERN,
    [FOLIO_STATE_REPLACE_BAD_TEMPLATE] = UI_TEXT_FAILURE_REPLACE_BAD_TEMPLATE,
    [FOLIO_STATE_REPLACE_TIMED_OUT] = UI_TEXT_FAILURE_REPLACE_TIMED_OUT,
    [FOLIO_STATE_REPLACE_TOO_COMPLEX] = UI_TEXT_FAILURE_REPLACE_TOO_COMPLEX,
    [FOLIO_STATE_REPLACE_TOO_MANY] = UI_TEXT_FAILURE_REPLACE_TOO_MANY,
    [FOLIO_STATE_REPLACE_TOO_LARGE] = UI_TEXT_FAILURE_REPLACE_TOO_LARGE,
    [FOLIO_STATE_REPLACE_STALE] = UI_TEXT_FAILURE_REPLACE_STALE,
    [FOLIO_STATE_REPLACE_BAD_SPAN] = UI_TEXT_FAILURE_REPLACE_BAD_SPAN,
    [FOLIO_STATE_TAG_INVALID] = UI_TEXT_FAILURE_TAG_INVALID,
    [FOLIO_STATE_TAG_LIMIT] = UI_TEXT_FAILURE_TAG_LIMIT,
    [FOLIO_STATE_TAGS_UNAVAILABLE] = UI_TEXT_FAILURE_TAGS_UNAVAILABLE,
    [FOLIO_STATE_TAGS_STALE] = UI_TEXT_FAILURE_TAGS_STALE,
    [FOLIO_STATE_NOTE_SOURCE_CHANGED] = UI_TEXT_FAILURE_NOTE_SOURCE_CHANGED,
    [FOLIO_STATE_OUT_OF_MEMORY] = UI_TEXT_FAILURE_OUT_OF_MEMORY,
    [FOLIO_STATE_NAME_REQUIRED] = UI_TEXT_FAILURE_NAME_REQUIRED,
    [FOLIO_STATE_INVALID_NAME] = UI_TEXT_FAILURE_INVALID_NAME,
    [FOLIO_STATE_ALREADY_NAMED] = UI_TEXT_FAILURE_ALREADY_NAMED,
    [FOLIO_STATE_CANCELLED] = UI_TEXT_EMPTY,
    [FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE] = UI_TEXT_FAILURE_FONT_BUNDLE_UNAVAILABLE,
    [FOLIO_STATE_HISTORY_EMPTY] = UI_TEXT_FAILURE_HISTORY_EMPTY,
    [FOLIO_STATE_HISTORY_UNREADABLE] = UI_TEXT_FAILURE_HISTORY_UNREADABLE,
    [FOLIO_STATE_CATEGORY_NAME_INVALID] = UI_TEXT_FAILURE_CATEGORY_NAME_INVALID,
    [FOLIO_STATE_CATEGORY_NAME_TAKEN] = UI_TEXT_FAILURE_CATEGORY_NAME_TAKEN,
    [FOLIO_STATE_CATEGORY_NOT_CREATED] = UI_TEXT_FAILURE_CATEGORY_NOT_CREATED,
    [FOLIO_STATE_CATEGORY_LEDGER_STALE] = UI_TEXT_FAILURE_CATEGORY_LEDGER_STALE,
    [FOLIO_STATE_TRASH_UNAVAILABLE] = UI_TEXT_FAILURE_TRASH_UNAVAILABLE,
    [FOLIO_STATE_TRASH_BUSY] = UI_TEXT_FAILURE_TRASH_BUSY,
    [FOLIO_STATE_TRASH_FAILED] = UI_TEXT_FAILURE_TRASH_FAILED,
    [FOLIO_STATE_CATEGORY_NOT_EMPTY] = UI_TEXT_FAILURE_CATEGORY_NOT_EMPTY,
    [FOLIO_STATE_CATEGORY_HAS_FILES] = UI_TEXT_FAILURE_CATEGORY_HAS_FILES,
    [FOLIO_STATE_CATEGORY_HISTORY_NOT_RECYCLED] = UI_TEXT_FAILURE_CATEGORY_HISTORY_NOT_RECYCLED,
    [FOLIO_STATE_CATEGORY_REMOVE_FAILED] = UI_TEXT_FAILURE_CATEGORY_REMOVE_FAILED,
    [FOLIO_STATE_CATEGORY_NOT_SELECTED] = UI_TEXT_FAILURE_CATEGORY_NOT_SELECTED,
};

const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome,
                                              enum folio_language language)
{
    return ui_text_line(failure_lines[outcome], language);
}

enum folio_language folio_state_language(const struct folio_state *_Nonnull state)
{
    return folio_settings_language(state->settings);
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
    folio_state_close_discovery(state);
    note_file_times_destroy(state->file_times);
    /* 未完了の意図は永続的な記録の側に残る。ここで捨てるのはメモリだけ（ADR 0022 の決定 7）。 */
    rename_plan_destroy(state->rename);
    note_corpus_destroy(state->rename_corpus);
    category_ledger_destroy(state->categories);
    discard_view(state);
    markdown_rtf_destroy(state->edit);
    note_text_destroy(state->body);
    utf8_text_destroy(state->search_term);
    utf8_text_destroy(state->index_term);
    index_filter_build_destroy(state->index_build);
    utf8_text_destroy(state->index_request);
    note_tags_destroy(state->index_tags);
    folio_state_close_tag_catalog(state);
    folio_state_close_tag_management(state);
    index_filter_destroy(state->filter);
    folio_state_close_tag_edit(state);
    note_corpus_destroy(state->corpus);
    folio_settings_destroy(state->settings);
    /* 下見は次の下見か破棄まで残る。捨てる契機は数え上げない（ADR 0028 の決定 6）。 */
    replace_preview_destroy(state->preview);
    free(state->found);
    folio_state_close_history(state);
    free(state);
}

static bool query_category_target(const struct folio_state *_Nonnull state,
                                  size_t *_Nonnull category)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref cursor = {0};
    if (folio_state_cursor(state, &kind, &cursor))
    {
        *category = cursor.category;
        return true;
    }
    if (state->document != FOLIO_DOCUMENT_NONE)
    {
        *category = state->selected_category;
        return true;
    }
    return false;
}

const struct markdown_images *_Nullable folio_state_images(const struct folio_state *_Nonnull state)
{
    return state->pane == nullptr ? nullptr : markdown_rtf_images(state->pane);
}
const struct markdown_media *_Nullable folio_state_media(const struct folio_state *_Nonnull state)
{
    return state->pane == nullptr ? nullptr : markdown_rtf_media(state->pane);
}
uint64_t folio_state_image_origin_revision(const struct folio_state *_Nonnull state)
{
    return state->image_origin_revision;
}
enum image_outcome folio_state_request_image(struct folio_state *_Nonnull state, size_t index,
                                             uint64_t *_Nonnull ticket)
{
    const struct markdown_media *media = folio_state_media(state);
    if (state->mode != PANE_MODE_VIEW || state->document != FOLIO_DOCUMENT_NAMED ||
        media == nullptr || index >= markdown_media_count(media) || category_rename_pending(state))
        return IMAGE_CANCELLED;
    enum image_outcome status = markdown_media_status(media, index);
    if (status != IMAGE_READY)
        return status;
    switch (markdown_media_kind(media, index))
    {
    case MARKDOWN_MEDIA_IMAGE:
    {
        struct image_request request = {
            .category = category_ledger_name(state->categories, state->selected_category),
            .source = markdown_images_at(markdown_media_images(media),
                                         markdown_media_image_index(media, index))};
        return state->images.request(state->images.adapter, &request, ticket);
    }
    case MARKDOWN_MEDIA_DIAGRAM:
    {
        struct diagram_request request = {.source = markdown_media_diagram(media, index),
                                          .palette = diagram_palette_for(&state->palette),
                                          .face = ui_font_face(folio_state_language(state))};
        return state->images.diagram(state->images.adapter, &request, ticket);
    }
    }
    return IMAGE_CANCELLED;
}
enum image_outcome folio_state_poll_image(struct folio_state *_Nonnull state, uint64_t ticket,
                                          struct image_bitmap *_Nullable *_Nonnull out)
{
    return state->images.poll(state->images.adapter, ticket, out);
}
enum image_phase folio_state_image_phase(const struct folio_state *_Nonnull state)
{
    return state->images.phase(state->images.adapter);
}
void folio_state_cancel_images(struct folio_state *_Nonnull state)
{
    state->images.cancel(state->images.adapter);
}

static enum folio_state_outcome query_category_command(const struct folio_state *_Nonnull state,
                                                       enum folio_command command,
                                                       enum folio_command_target target_kind,
                                                       struct note_ref target)
{
    if (folio_state_filtering(state) && command != FOLIO_COMMAND_EXPORT_CATEGORY)
        return FOLIO_STATE_FILTERED;
    if (command == FOLIO_COMMAND_NEW_CATEGORY)
        return FOLIO_STATE_READY;
    if (target_kind == FOLIO_COMMAND_TARGET_CURRENT &&
        !query_category_target(state, &target.category))
        return FOLIO_STATE_CATEGORY_NOT_SELECTED;
    if (target.category >= state->notes_count)
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    if (command == FOLIO_COMMAND_DELETE_CATEGORY &&
        (note_ledger_count(state->notes[target.category]) != 0 ||
         (state->document == FOLIO_DOCUMENT_UNTITLED &&
          state->selected_category == target.category)))
        return FOLIO_STATE_CATEGORY_NOT_EMPTY;
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome query_held_note(const struct folio_state *_Nonnull state,
                                                enum folio_command command)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
        return FOLIO_STATE_NOTHING_SELECTED;
    bool named = command == FOLIO_COMMAND_RENAME || command == FOLIO_COMMAND_TRASH_NOTE ||
                 command == FOLIO_COMMAND_HISTORY || command == FOLIO_COMMAND_TOGGLE_FAVORITE ||
                 command == FOLIO_COMMAND_MOVE_NOTE;
    bool editing = command == FOLIO_COMMAND_REPLACE || command == FOLIO_COMMAND_SUBSTITUTE ||
                   command == FOLIO_COMMAND_DISCARD_EDITS;
    if (named && state->document == FOLIO_DOCUMENT_UNTITLED)
        return FOLIO_STATE_NAME_REQUIRED;
    if (editing && folio_state_pane_mode(state) != PANE_MODE_EDIT)
        return FOLIO_STATE_NOT_EDITING;
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome query_note_target(const struct folio_state *_Nonnull state,
                                                  struct note_ref target)
{
    if (!holds_category(state, target.category))
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    return target.note < note_ledger_count(state->notes[target.category])
               ? FOLIO_STATE_READY
               : FOLIO_STATE_NO_SUCH_NOTE;
}

static enum folio_state_outcome query_note_command(const struct folio_state *_Nonnull state,
                                                   enum folio_command command,
                                                   enum folio_command_target target_kind,
                                                   struct note_ref target)
{
    if (command == FOLIO_COMMAND_NEW)
    {
        size_t category = target_kind == FOLIO_COMMAND_TARGET_CURRENT
                              ? folio_state_current_category(state)
                              : target.category;
        return holds_category(state, category) ? FOLIO_STATE_READY : FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    if (target_kind == FOLIO_COMMAND_TARGET_NOTE)
        return query_note_target(state, target);
    return query_held_note(state, command);
}

enum folio_state_outcome folio_state_command_availability(const struct folio_state *_Nonnull state,
                                                          enum folio_command command,
                                                          enum folio_command_target target_kind,
                                                          struct note_ref target)
{
    switch (folio_command_scope(command))
    {
    case FOLIO_COMMAND_SCOPE_NOTE:
        return query_note_command(state, command, target_kind, target);
    case FOLIO_COMMAND_SCOPE_CATEGORY:
        return query_category_command(state, command, target_kind, target);
    case FOLIO_COMMAND_SCOPE_APP:
    case FOLIO_COMMAND_SCOPE_LAYOUT:
    case FOLIO_COMMAND_SCOPE_QUIT:
        return FOLIO_STATE_READY;
    }
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_recolor_availability(const struct folio_state *_Nonnull state,
                                                          size_t category)
{
    return holds_category(state, category) ? FOLIO_STATE_READY : FOLIO_STATE_NO_SUCH_CATEGORY;
}

const char *_Nonnull folio_state_note_name(const struct folio_state *_Nonnull state,
                                           struct note_ref target)
{
    return note_ledger_name(state->notes[target.category], target.note);
}

void folio_state_close_tag_edit(struct folio_state *_Nonnull state)
{
    tag_edit_snapshot_destroy(state->tag_edit);
    note_tags_destroy(state->edit_tags);
    state->tag_edit = nullptr;
    state->edit_tags = nullptr;
}

static struct tag_edit_identity tag_destination(const struct folio_state *_Nonnull state)
{
    return (struct tag_edit_identity){
        .category = category_ledger_name(state->categories, state->selected_category),
        .note = folio_state_document_name(state)};
}

static enum folio_state_outcome tag_source(const struct folio_state *_Nonnull state,
                                           const char16_t *_Nonnull units, size_t count,
                                           struct note_text *_Nullable *_Nonnull out)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
        return FOLIO_STATE_NOTHING_SELECTED;
    if (state->mode == PANE_MODE_EDIT)
        return edited_text(state, units, count, out);
    if (note_text_editability(state->body) == NOTE_TEXT_EDITOR_ALTERS)
        return FOLIO_STATE_NOTE_UNEDITABLE;
    return text_outcome(note_text_from_editor(note_text_bytes(state->body),
                                              note_text_length(state->body),
                                              note_text_line_ending(state->body), out));
}

enum folio_state_outcome folio_state_note_tag_count(const struct folio_state *_Nonnull state,
                                                    const char16_t *_Nonnull units, size_t count,
                                                    size_t *_Nonnull out)
{
    if (state->document == FOLIO_DOCUMENT_NONE)
        return FOLIO_STATE_NOTHING_SELECTED;
    struct note_text *_Nullable edited = nullptr;
    if (state->mode == PANE_MODE_EDIT)
    {
        enum folio_state_outcome outcome = edited_text(state, units, count, &edited);
        if (outcome != FOLIO_STATE_READY)
            return outcome;
    }
    const struct note_metadata *_Nonnull metadata =
        note_text_metadata(edited == nullptr ? state->body : edited);
    enum folio_state_outcome outcome = FOLIO_STATE_TAGS_UNAVAILABLE;
    if (note_metadata_available(metadata))
    {
        *out = note_tags_count(note_metadata_tags(metadata));
        outcome = FOLIO_STATE_READY;
    }
    note_text_destroy(edited);
    return outcome;
}

static enum folio_state_outcome tag_set_outcome(enum note_tags_outcome made)
{
    switch (made)
    {
    case NOTE_TAGS_READY:
        return FOLIO_STATE_READY;
    case NOTE_TAGS_TOO_MANY:
        return FOLIO_STATE_TAG_LIMIT;
    case NOTE_TAGS_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_OUT_OF_MEMORY;
}

/* Take the prepared body only when the full replacement session exists. */
static enum folio_state_outcome adopt_tag_session(struct folio_state *_Nonnull state,
                                                  struct note_text *_Nonnull body)
{
    const struct note_metadata *_Nonnull metadata = note_text_metadata(body);
    if (!note_metadata_available(metadata))
        return FOLIO_STATE_TAGS_UNAVAILABLE;
    struct note_tags *_Nullable tags = nullptr;
    enum folio_state_outcome copied =
        tag_set_outcome(note_tags_copy(note_metadata_tags(metadata), &tags));
    if (copied != FOLIO_STATE_READY)
        return copied;
    struct tag_edit_snapshot *_Nullable snapshot = nullptr;
    struct tag_edit_identity identity = tag_destination(state);
    if (tag_edit_snapshot_create(body, &identity, &snapshot) != TAG_EDIT_SNAPSHOT_READY)
    {
        note_tags_destroy(tags);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    folio_state_close_tag_edit(state);
    state->tag_edit = snapshot;
    state->edit_tags = tags;
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_begin_tag_edit(struct folio_state *_Nonnull state,
                                                    const char16_t *_Nonnull units, size_t count)
{
    struct note_text *_Nullable body = nullptr;
    enum folio_state_outcome result = tag_source(state, units, count, &body);
    if (result == FOLIO_STATE_READY)
        result = adopt_tag_session(state, body);
    if (result != FOLIO_STATE_READY)
        note_text_destroy(body);
    return result;
}

const struct note_tags *_Nullable folio_state_edit_tags(const struct folio_state *_Nonnull state)
{
    return state->edit_tags;
}

enum folio_state_outcome folio_state_toggle_edit_tag(struct folio_state *_Nonnull state,
                                                     const struct note_tag *_Nonnull tag)
{
    if (state->edit_tags == nullptr)
        return FOLIO_STATE_TAGS_STALE;
    struct note_tags *_Nullable tags = nullptr;
    enum note_tag_presence presence =
        note_tags_contains(state->edit_tags, tag) ? NOTE_TAG_ABSENT : NOTE_TAG_PRESENT;
    enum folio_state_outcome result =
        tag_set_outcome(note_tags_with(state->edit_tags, tag, presence, &tags));
    if (result == FOLIO_STATE_READY)
    {
        note_tags_destroy(state->edit_tags);
        state->edit_tags = tags;
    }
    return result;
}

static enum folio_state_outcome tag_name_outcome(enum note_tag_outcome made)
{
    switch (made)
    {
    case NOTE_TAG_READY:
        return FOLIO_STATE_READY;
    case NOTE_TAG_EMPTY:
    case NOTE_TAG_INVALID:
        return FOLIO_STATE_TAG_INVALID;
    case NOTE_TAG_TOO_LONG:
        return FOLIO_STATE_TAG_LIMIT;
    case NOTE_TAG_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_TAG_INVALID;
}

enum folio_state_outcome folio_state_enter_edit_tag(struct folio_state *_Nonnull state,
                                                    const char16_t *_Nonnull units, size_t count)
{
    if (state->edit_tags == nullptr)
        return FOLIO_STATE_TAGS_STALE;
    struct utf8_text *_Nullable text = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(units, count, &text);
    if (converted != UTF8_TEXT_CONVERTED)
        return converted == UTF8_TEXT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                    : FOLIO_STATE_TAG_INVALID;
    struct note_tag *_Nullable tag = nullptr;
    enum folio_state_outcome made =
        tag_name_outcome(note_tag_create(utf8_text_bytes(text), utf8_text_length(text), &tag));
    utf8_text_destroy(text);
    if (made == FOLIO_STATE_READY && !note_tags_contains(state->edit_tags, tag))
        made = folio_state_toggle_edit_tag(state, tag);
    note_tag_destroy(tag);
    return made;
}

static enum folio_state_outcome tag_edit_outcome(enum note_tag_edit_outcome made)
{
    switch (made)
    {
    case NOTE_TAG_EDIT_READY:
    case NOTE_TAG_EDIT_UNCHANGED:
        return FOLIO_STATE_READY;
    case NOTE_TAG_EDIT_UNAVAILABLE:
        return FOLIO_STATE_TAGS_UNAVAILABLE;
    case NOTE_TAG_EDIT_LIMIT:
        return FOLIO_STATE_TAG_LIMIT;
    case NOTE_TAG_EDIT_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_TAGS_UNAVAILABLE;
}

enum folio_state_outcome folio_state_apply_tag_edit(struct folio_state *_Nonnull state,
                                                    const char16_t *_Nonnull units, size_t count,
                                                    struct replace_edit *_Nullable *_Nonnull out)
{
    if (state->tag_edit == nullptr)
        return FOLIO_STATE_TAGS_STALE;
    struct note_text *_Nullable current = nullptr;
    enum folio_state_outcome result = tag_source(state, units, count, &current);
    if (result != FOLIO_STATE_READY)
        return result;
    struct tag_edit_identity identity = tag_destination(state);
    bool holds = tag_edit_snapshot_holds(state->tag_edit, current, &identity);
    note_text_destroy(current);
    if (!holds)
        return FOLIO_STATE_TAGS_STALE;
    const struct note_text *_Nonnull source = tag_edit_snapshot_text(state->tag_edit);
    struct replace_edit *_Nullable edit = nullptr;
    result =
        tag_edit_outcome(note_metadata_edit(note_text_metadata(source), state->edit_tags, &edit));
    if (result == FOLIO_STATE_READY)
        *out = edit;
    return result;
}

static void tag_current_destroy(struct folio_state *_Nonnull state)
{
    note_text_destroy(state->tag_current_body);
    markdown_rtf_destroy(state->tag_current_view);
    markdown_rtf_destroy(state->tag_current_edit);
    state->tag_current_body = nullptr;
    state->tag_current_view = nullptr;
    state->tag_current_edit = nullptr;
}

static void tag_request_destroy(struct note_tag *_Nullable from, struct note_tag *_Nullable to,
                                struct tag_update *_Nullable *_Nullable updates,
                                enum tag_apply_status *_Nullable results)
{
    note_tag_destroy(from);
    note_tag_destroy(to);
    free(updates);
    free(results);
}

void folio_state_close_tag_management(struct folio_state *_Nonnull state)
{
    for (size_t at = 0; at < state->tag_prepared; ++at)
        tag_update_destroy(state->tag_updates[at]);
    tag_request_destroy(state->tag_from, state->tag_to, state->tag_updates, state->tag_results);
    tag_current_destroy(state);
    state->tag_from = nullptr;
    state->tag_to = nullptr;
    state->tag_updates = nullptr;
    state->tag_results = nullptr;
    state->tag_total = 0;
    state->tag_prepared = 0;
    state->tag_next = 0;
    state->tag_phase = TAG_MANAGEMENT_CLOSED;
    state->tag_notice = FOLIO_STATE_READY;
    state->tag_merging = false;
}

/* Reuse the sole corpus reader; cached bodies are unavailable to matching until this pass finishes.
 */
static void invalidate_corpus_dependents(struct folio_state *_Nonnull state)
{
    discard_index_build(state);
    if (state->index_request == nullptr && state->index_term != nullptr)
    {
        state->index_request = state->index_term;
        state->index_term = nullptr;
    }
    index_filter_destroy(state->filter);
    state->filter = nullptr;
    folio_state_close_tag_catalog(state);
    if (discovery_uses_body(state))
        invalidate_discovery(state);
    layout_changed(state);
}

static void fresh_tag_corpus(struct folio_state *_Nonnull state)
{
    state->corpus_loaded = false;
    state->corpus_category = 0;
    state->corpus_note = 0;
    state->copy_pending = false;
    discovery_fresh_pass(state);
    invalidate_corpus_dependents(state);
}

static enum folio_state_outcome copy_management_tag(const struct note_tag *_Nullable source,
                                                    struct note_tag *_Nullable *_Nonnull out)
{
    if (source == nullptr)
    {
        *out = nullptr;
        return FOLIO_STATE_READY;
    }
    return note_tag_create(note_tag_bytes(source), note_tag_length(source), out) == NOTE_TAG_READY
               ? FOLIO_STATE_READY
               : FOLIO_STATE_OUT_OF_MEMORY;
}

enum folio_state_outcome folio_state_prepare_tag_management(struct folio_state *_Nonnull state,
                                                            const struct note_tag *_Nonnull from,
                                                            const struct note_tag *_Nullable to)
{
    enum folio_state_outcome synced = synchronize(state);
    if (synced != FOLIO_STATE_READY)
        return synced;
    size_t count = 0;
    if (!index_note_count(state, &count) || count >= SIZE_MAX / sizeof(struct tag_update *) ||
        count >= SIZE_MAX / sizeof(enum tag_apply_status))
        return FOLIO_STATE_OUT_OF_MEMORY;
    struct note_tag *owned_from = nullptr;
    struct note_tag *owned_to = nullptr;
    enum folio_state_outcome copied = copy_management_tag(from, &owned_from);
    if (copied == FOLIO_STATE_READY)
        copied = copy_management_tag(to, &owned_to);
    struct tag_update *_Nullable *_Nullable updates = calloc(count + 1, sizeof *updates);
    enum tag_apply_status *_Nullable results = calloc(count + 1, sizeof *results);
    if (copied != FOLIO_STATE_READY || updates == nullptr || results == nullptr)
    {
        tag_request_destroy(owned_from, owned_to, updates, results);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    folio_state_close_tag_management(state);
    state->tag_from = owned_from;
    state->tag_to = owned_to;
    state->tag_updates = updates;
    state->tag_results = results;
    state->tag_total = count;
    state->tag_rows = state->ledger_revision;
    state->tag_phase = TAG_MANAGEMENT_PREPARING;
    fresh_tag_corpus(state);
    return FOLIO_STATE_READY;
}

static enum tag_apply_status prepared_tag_status(enum tag_update_kind kind)
{
    switch (kind)
    {
    case TAG_UPDATE_CHANGE:
        return TAG_APPLY_PENDING;
    case TAG_UPDATE_NO_MATCH:
        return TAG_APPLY_NOT_TARGET;
    case TAG_UPDATE_UNREADABLE:
        return TAG_APPLY_UNREADABLE;
    case TAG_UPDATE_UNAVAILABLE:
        return TAG_APPLY_UNAVAILABLE;
    case TAG_UPDATE_LIMIT:
        return TAG_APPLY_LIMIT;
    }
    return TAG_APPLY_UNAVAILABLE;
}

static bool management_target_exists(const struct tag_update_request *_Nonnull request)
{
    if (request->source == nullptr || request->to == nullptr ||
        note_tag_equal(request->from, request->to))
        return false;
    const struct note_metadata *metadata = note_text_metadata(request->source);
    return note_metadata_available(metadata) &&
           note_tags_contains(note_metadata_tags(metadata), request->to);
}

static enum folio_state_outcome prepare_tag_management_note(struct folio_state *_Nonnull state)
{
    if (state->tag_rows != state->ledger_revision || state->tag_prepared >= state->tag_total)
        return FOLIO_STATE_TAGS_STALE;
    const char *category = category_ledger_name(state->categories, state->corpus_category);
    const char *note = note_ledger_name(state->notes[state->corpus_category], state->corpus_note);
    const struct tag_update_request request = {category, note,
                                               note_corpus_body(state->corpus, category, note),
                                               state->tag_from, state->tag_to};
    struct tag_update *update = nullptr;
    if (tag_update_create(&request, &update) != TAG_UPDATE_READY)
        return FOLIO_STATE_OUT_OF_MEMORY;
    size_t at = state->tag_prepared;
    state->tag_updates[at] = update;
    state->tag_results[at] = prepared_tag_status(tag_update_kind(update));
    state->tag_merging = state->tag_merging || management_target_exists(&request);
    state->tag_prepared += 1;
    return FOLIO_STATE_READY;
}

const struct tag_update *_Nonnull folio_state_tag_management_at(
    const struct folio_state *_Nonnull state, size_t index)
{
    return state->tag_updates[index];
}

enum tag_apply_status folio_state_tag_management_status(const struct folio_state *_Nonnull state,
                                                        size_t index)
{
    return state->tag_results[index];
}

static void count_tag_status(struct tag_management_summary *_Nonnull summary,
                             enum tag_apply_status status)
{
    switch (status)
    {
    case TAG_APPLY_NOT_TARGET:
        break;
    case TAG_APPLY_PENDING:
        summary->pending += 1;
        break;
    case TAG_APPLY_CHANGED:
        summary->changed += 1;
        break;
    case TAG_APPLY_UNCHANGED:
    case TAG_APPLY_UNREADABLE:
    case TAG_APPLY_UNAVAILABLE:
    case TAG_APPLY_LIMIT:
    case TAG_APPLY_CONFLICT:
        summary->skipped += 1;
        break;
    case TAG_APPLY_HISTORY_FAILED:
    case TAG_APPLY_WRITE_FAILED:
    case TAG_APPLY_OUT_OF_MEMORY:
        summary->failed += 1;
        break;
    }
}

const struct note_tag *_Nonnull folio_state_tag_management_source(
    const struct folio_state *_Nonnull state)
{
    return state->tag_from;
}

enum tag_management_phase folio_state_tag_management_phase(const struct folio_state *_Nonnull state)
{
    return state->tag_phase;
}

const struct note_tag *_Nullable folio_state_tag_management_target(
    const struct folio_state *_Nonnull state)
{
    return state->tag_to;
}

struct tag_management_summary
folio_state_tag_management_summary(const struct folio_state *_Nonnull state)
{
    struct tag_management_summary summary = {.phase = state->tag_phase,
                                             .notice = state->tag_notice,
                                             .total = state->tag_total,
                                             .prepared = state->tag_prepared,
                                             .merging = state->tag_merging};
    for (size_t at = 0; at < state->tag_prepared; ++at)
        count_tag_status(&summary, state->tag_results[at]);
    return summary;
}

static bool current_tag_target(const struct folio_state *_Nonnull state,
                               const struct tag_update *_Nonnull update)
{
    return state->document == FOLIO_DOCUMENT_NAMED &&
           strcmp(tag_update_category(update),
                  category_ledger_name(state->categories, state->selected_category)) == 0 &&
           strcmp(tag_update_note(update), folio_state_document_name(state)) == 0;
}

/* Every resource needed to adopt the selected body exists before the file can change. */
static enum folio_state_outcome stage_tag_current(struct folio_state *_Nonnull state,
                                                  const struct note_text *_Nonnull body)
{
    tag_current_destroy(state);
    if (note_text_copy(body, &state->tag_current_body) != NOTE_TEXT_ACCEPTED ||
        markdown_rtf_create(body, &state->palette, view_locale(folio_state_language(state)),
                            &state->tag_current_view) != MARKDOWN_RTF_CONVERTED)
    {
        tag_current_destroy(state);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome prepared = prepare_note_edit(state, body, &state->tag_current_edit);
    if (prepared != FOLIO_STATE_READY)
        tag_current_destroy(state);
    return prepared;
}

static void adopt_tag_current(struct folio_state *_Nonnull state)
{
    discard_view(state);
    note_text_destroy(state->body);
    markdown_rtf_destroy(state->edit);
    state->body = state->tag_current_body;
    state->pane = state->tag_current_view;
    state->edit = state->tag_current_edit;
    state->tag_current_body = nullptr;
    state->tag_current_view = nullptr;
    state->tag_current_edit = nullptr;
    folio_state_close_history(state);
    folio_state_close_tag_edit(state);
    folio_state_refresh_note_times(state);
    state->images.cancel(state->images.adapter);
    open_uneditable_in_view(state);
}

static enum tag_apply_status applied_tag_status(enum note_store_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_STORE_STORED:
        return TAG_APPLY_CHANGED;
    case NOTE_STORE_UNCHANGED:
        return TAG_APPLY_UNCHANGED;
    case NOTE_STORE_SOURCE_CHANGED:
        return TAG_APPLY_CONFLICT;
    case NOTE_STORE_SOURCE_UNREADABLE:
        return TAG_APPLY_UNREADABLE;
    case NOTE_STORE_HISTORY_FAILED:
        return TAG_APPLY_HISTORY_FAILED;
    case NOTE_STORE_WRITE_FAILED:
        return TAG_APPLY_WRITE_FAILED;
    case NOTE_STORE_OUT_OF_MEMORY:
        return TAG_APPLY_OUT_OF_MEMORY;
    }
    return TAG_APPLY_WRITE_FAILED;
}

static enum tag_management_step apply_tag_management_note(struct folio_state *_Nonnull state,
                                                          size_t index)
{
    const struct tag_update *update = state->tag_updates[index];
    const struct note_text *_Nullable before = tag_update_source(update);
    const struct note_text *_Nullable after = tag_update_body(update);
    if (before == nullptr || after == nullptr)
    {
        state->tag_results[index] = TAG_APPLY_UNAVAILABLE;
        return TAG_MANAGEMENT_PROGRESSED;
    }
    bool current = current_tag_target(state, update);
    if (current && stage_tag_current(state, after) != FOLIO_STATE_READY)
    {
        state->tag_results[index] = TAG_APPLY_OUT_OF_MEMORY;
        return TAG_MANAGEMENT_PROGRESSED;
    }
    const struct note_store_request request = {tag_update_category(update), tag_update_note(update),
                                               after, before};
    enum note_store_outcome saved = state->port.store_note(state->port.adapter, &request);
    state->tag_results[index] = applied_tag_status(saved);
    if (saved != NOTE_STORE_STORED)
    {
        tag_current_destroy(state);
        return TAG_MANAGEMENT_PROGRESSED;
    }
    stored_note(state);
    if (current)
        adopt_tag_current(state);
    fresh_tag_corpus(state);
    return current ? TAG_MANAGEMENT_CURRENT_CHANGED : TAG_MANAGEMENT_PROGRESSED;
}

static bool tag_management_identity_holds(struct folio_state *_Nonnull state)
{
    if (state->tag_rows == state->ledger_revision)
        return true;
    state->tag_notice = FOLIO_STATE_TAGS_STALE;
    state->tag_phase = TAG_MANAGEMENT_RESULT;
    return false;
}

enum folio_state_outcome folio_state_apply_tag_management(struct folio_state *_Nonnull state)
{
    if (state->tag_phase != TAG_MANAGEMENT_PREVIEW || !tag_management_identity_holds(state))
        return FOLIO_STATE_TAGS_STALE;
    if (folio_state_tag_management_summary(state).pending == 0)
        return FOLIO_STATE_NAME_UNCHANGED;
    state->tag_next = 0;
    state->tag_phase = TAG_MANAGEMENT_APPLYING;
    return FOLIO_STATE_READY;
}

void folio_state_stop_tag_management(struct folio_state *_Nonnull state)
{
    if (state->tag_phase == TAG_MANAGEMENT_PREPARING || state->tag_phase == TAG_MANAGEMENT_APPLYING)
    {
        state->tag_notice = FOLIO_STATE_CANCELLED;
        state->tag_phase = TAG_MANAGEMENT_RESULT;
    }
}

static enum tag_management_step continue_tag_preparation(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome outcome = folio_state_continue_index_filter(state);
    if (outcome != FOLIO_STATE_READY)
    {
        state->tag_notice = outcome;
        state->tag_phase = TAG_MANAGEMENT_RESULT;
    }
    return state->tag_phase == TAG_MANAGEMENT_PREPARING ? TAG_MANAGEMENT_PROGRESSED
                                                        : TAG_MANAGEMENT_FINISHED;
}

static enum tag_management_step continue_tag_application(struct folio_state *_Nonnull state)
{
    while (state->tag_next < state->tag_prepared)
    {
        size_t index = state->tag_next++;
        if (state->tag_results[index] == TAG_APPLY_PENDING)
            return apply_tag_management_note(state, index);
    }
    state->tag_phase = TAG_MANAGEMENT_RESULT;
    return TAG_MANAGEMENT_FINISHED;
}

enum tag_management_step folio_state_continue_tag_management(struct folio_state *_Nonnull state)
{
    switch (state->tag_phase)
    {
    case TAG_MANAGEMENT_CLOSED:
    case TAG_MANAGEMENT_PREVIEW:
    case TAG_MANAGEMENT_RESULT:
        return TAG_MANAGEMENT_IDLE;
    case TAG_MANAGEMENT_PREPARING:
        return tag_management_identity_holds(state) ? continue_tag_preparation(state)
                                                    : TAG_MANAGEMENT_FINISHED;
    case TAG_MANAGEMENT_APPLYING:
        return tag_management_identity_holds(state) ? continue_tag_application(state)
                                                    : TAG_MANAGEMENT_FINISHED;
    }
    return TAG_MANAGEMENT_IDLE;
}

enum folio_state_outcome folio_state_capture_note(const struct folio_state *_Nonnull state,
                                                  struct note_ref ref,
                                                  struct note_identity *_Nullable *_Nonnull out)
{
    if (ref.category >= state->notes_count)
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    if (ref.note >= note_ledger_count(state->notes[ref.category]))
        return FOLIO_STATE_NO_SUCH_NOTE;
    return note_identity_create(category_ledger_name(state->categories, ref.category),
                                note_ledger_name(state->notes[ref.category], ref.note),
                                out) == NOTE_IDENTITY_READY
               ? FOLIO_STATE_READY
               : FOLIO_STATE_OUT_OF_MEMORY;
}

static enum folio_state_outcome resolve_named_note(const struct folio_state *_Nonnull state,
                                                   size_t category, const char *_Nonnull name,
                                                   struct note_ref *_Nonnull out)
{
    for (size_t note = 0; note < note_ledger_count(state->notes[category]); ++note)
    {
        if (strcmp(name, note_ledger_name(state->notes[category], note)) == 0)
        {
            *out = located(category, note);
            return FOLIO_STATE_READY;
        }
    }
    return FOLIO_STATE_NO_SUCH_NOTE;
}

enum folio_state_outcome folio_state_resolve_note(const struct folio_state *_Nonnull state,
                                                  const struct note_identity *_Nonnull identity,
                                                  struct note_ref *_Nonnull out)
{
    for (size_t category = 0; category < state->notes_count; ++category)
        if (strcmp(note_identity_category(identity),
                   category_ledger_name(state->categories, category)) == 0)
            return resolve_named_note(state, category, note_identity_note(identity), out);
    return FOLIO_STATE_NO_SUCH_CATEGORY;
}

static bool discovery_uses_tags(const struct folio_state *_Nonnull state)
{
    return state->discovery_tags != nullptr && note_tags_count(state->discovery_tags) != 0;
}

static bool discovery_uses_body(const struct folio_state *_Nonnull state)
{
    if (discovery_uses_tags(state))
        return true;
    switch (state->discovery_mode)
    {
    case DISCOVERY_MODE_NAMES:
        return false;
    case DISCOVERY_MODE_BODY:
        return true;
    }
}

static void discovery_changed(struct folio_state *_Nonnull state)
{
    if (state->discovery_revision != UINT64_MAX)
        state->discovery_revision += 1;
}

static void discovery_results_changed(struct folio_state *_Nonnull state)
{
    if (state->discovery_results_revision != UINT64_MAX)
        state->discovery_results_revision += 1;
    discovery_changed(state);
}

static void discard_discovery_results(struct folio_state *_Nonnull state)
{
    index_filter_build_destroy(state->discovery_build);
    index_filter_destroy(state->discovery_results);
    note_excerpt_destroy(state->discovery_excerpt);
    state->discovery_build = nullptr;
    state->discovery_results = nullptr;
    state->discovery_excerpt = nullptr;
    state->discovery_cursor = (struct note_ref){0};
    state->discovery_preview = DISCOVERY_PREVIEW_NONE;
    state->discovery_selected = 0;
    discovery_results_changed(state);
}

static void invalidate_discovery(struct folio_state *_Nonnull state)
{
    if (state->discovery_phase == DISCOVERY_CLOSED)
        return;
    discard_discovery_results(state);
    if (state->discovery_query == nullptr)
    {
        state->discovery_phase = DISCOVERY_FAILED;
        return;
    }
    state->discovery_notice = FOLIO_STATE_READY;
    if (state->discovery_mode == DISCOVERY_MODE_BODY &&
        utf8_text_length(state->discovery_query) == 0)
    {
        state->discovery_phase = DISCOVERY_INPUT;
        return;
    }
    state->discovery_phase =
        discovery_uses_body(state) && (!state->discovery_corpus_started || !state->corpus_loaded)
            ? DISCOVERY_PREPARING
            : DISCOVERY_MATCHING;
}

static void discard_discovery_sources(struct folio_state *_Nonnull state)
{
    free(state->discovery_sources);
    state->discovery_sources = nullptr;
    state->discovery_source_count = 0;
    state->discovery_source_checked = 0;
    state->discovery_unreadable = 0;
    state->discovery_unsupported = 0;
    state->discovery_corpus_started = false;
}

static void refresh_discovery(struct folio_state *_Nonnull state)
{
    if (state->discovery_phase == DISCOVERY_CLOSED)
        return;
    discard_discovery_sources(state);
    invalidate_discovery(state);
    state->discovery_refresh = true;
}

void folio_state_close_discovery(struct folio_state *_Nonnull state)
{
    discard_discovery_results(state);
    utf8_text_destroy(state->discovery_query);
    note_tags_destroy(state->discovery_tags);
    note_identity_destroy(state->discovery_identity);
    discard_discovery_sources(state);
    state->discovery_query = nullptr;
    state->discovery_tags = nullptr;
    state->discovery_identity = nullptr;
    state->discovery_phase = DISCOVERY_CLOSED;
    state->discovery_mode = DISCOVERY_MODE_NAMES;
    state->discovery_notice = FOLIO_STATE_READY;
    state->discovery_refresh = false;
}

enum folio_state_outcome folio_state_open_discovery(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome synced = synchronize_filter(state);
    if (synced != FOLIO_STATE_READY)
        return synced;
    folio_state_close_discovery(state);
    if (utf8_text_create(u"", 0, &state->discovery_query) != UTF8_TEXT_CONVERTED)
        return FOLIO_STATE_OUT_OF_MEMORY;
    if (state->index_tags != nullptr &&
        note_tags_copy(state->index_tags, &state->discovery_tags) != NOTE_TAGS_READY)
    {
        folio_state_close_discovery(state);
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    state->discovery_tag_mode = state->tag_mode;
    state->discovery_scope = state->note_scope;
    state->discovery_order = folio_state_note_order(state);
    state->discovery_phase = DISCOVERY_MATCHING;
    refresh_discovery(state);
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome fail_discovery(struct folio_state *_Nonnull state,
                                               enum folio_state_outcome reason)
{
    discard_discovery_results(state);
    state->discovery_phase = DISCOVERY_FAILED;
    state->discovery_notice = reason;
    return reason;
}

static enum folio_state_outcome discovery_query_status(const struct utf8_text *_Nonnull query)
{
    const char *_Nonnull bytes = utf8_text_bytes(query);
    size_t length = utf8_text_length(query);
    size_t scalars = 0;
    for (size_t at = 0; at < length; ++scalars)
    {
        uint32_t point = 0;
        at += utf8_text_decode(bytes + at, length - at, &point);
        if (point == 0 || point == '\r' || point == '\n')
            return FOLIO_STATE_DISCOVERY_QUERY_INVALID;
    }
    return scalars <= 256 && length <= 1024 ? FOLIO_STATE_READY : FOLIO_STATE_DISCOVERY_QUERY_LIMIT;
}

enum folio_state_outcome folio_state_query_discovery(struct folio_state *_Nonnull state,
                                                     const char16_t *_Nonnull units, size_t count)
{
    if (state->discovery_phase == DISCOVERY_CLOSED)
        return FOLIO_STATE_CANCELLED;
    discard_discovery_results(state);
    utf8_text_destroy(state->discovery_query);
    state->discovery_query = nullptr;
    if (count > 512)
        return fail_discovery(state, FOLIO_STATE_DISCOVERY_QUERY_LIMIT);
    struct utf8_text *_Nullable query = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(units, count, &query);
    if (converted != UTF8_TEXT_CONVERTED)
        return fail_discovery(state, converted == UTF8_TEXT_OUT_OF_MEMORY
                                         ? FOLIO_STATE_OUT_OF_MEMORY
                                         : FOLIO_STATE_DISCOVERY_QUERY_INVALID);
    enum folio_state_outcome valid = discovery_query_status(query);
    if (valid != FOLIO_STATE_READY)
    {
        utf8_text_destroy(query);
        return fail_discovery(state, valid);
    }
    state->discovery_query = query;
    invalidate_discovery(state);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome folio_state_set_discovery_mode(struct folio_state *_Nonnull state,
                                                        enum discovery_mode mode)
{
    if (state->discovery_phase == DISCOVERY_CLOSED)
        return FOLIO_STATE_CANCELLED;
    if (state->discovery_mode != mode)
    {
        state->discovery_mode = mode;
        invalidate_discovery(state);
    }
    return FOLIO_STATE_READY;
}

static size_t discovery_source_index(const struct folio_state *_Nonnull state, struct note_ref ref)
{
    size_t at = ref.note;
    for (size_t category = 0; category < ref.category; ++category)
        at += note_ledger_count(state->notes[category]);
    return at;
}

static bool discovery_source_reused(const struct folio_state *_Nonnull state, struct note_ref ref)
{
    return state->discovery_corpus_started && state->discovery_sources != nullptr &&
           state->discovery_sources[discovery_source_index(state, ref)] != DISCOVERY_SOURCE_UNREAD;
}

static void discovery_fresh_pass(struct folio_state *_Nonnull state)
{
    if (state->discovery_sources == nullptr)
        return;
    for (size_t at = 0; at < state->discovery_source_count; ++at)
        state->discovery_sources[at] = DISCOVERY_SOURCE_UNREAD;
    state->discovery_source_checked = state->discovery_unreadable = state->discovery_unsupported =
        0;
    state->discovery_corpus_started = true;
}

static void discovery_source_changed(struct folio_state *_Nonnull state, size_t at,
                                     enum discovery_source source)
{
    enum discovery_source previous = state->discovery_sources[at];
    state->discovery_source_checked += previous == DISCOVERY_SOURCE_UNREAD ? 1 : 0;
    state->discovery_unreadable -= previous == DISCOVERY_SOURCE_UNREADABLE ? 1 : 0;
    state->discovery_unsupported -= previous == DISCOVERY_SOURCE_UNSUPPORTED ? 1 : 0;
    state->discovery_sources[at] = source;
    state->discovery_unreadable += source == DISCOVERY_SOURCE_UNREADABLE ? 1 : 0;
    state->discovery_unsupported += source == DISCOVERY_SOURCE_UNSUPPORTED ? 1 : 0;
}

static void observe_discovery_source(struct folio_state *_Nonnull state, struct note_ref ref)
{
    if (state->discovery_sources == nullptr)
        return;
    struct index_filter_entry entry = filter_entry(state, ref, true);
    enum discovery_source source = DISCOVERY_SOURCE_READY;
    if (entry.body == nullptr)
        source = DISCOVERY_SOURCE_UNREADABLE;
    if (entry.body != nullptr && entry.tags == nullptr)
        source = DISCOVERY_SOURCE_UNSUPPORTED;
    discovery_source_changed(state, discovery_source_index(state, ref), source);
}

static enum folio_state_outcome prepare_discovery_sources(struct folio_state *_Nonnull state)
{
    if (!state->discovery_refresh)
        return FOLIO_STATE_READY;
    size_t count = 0;
    if (!index_note_count(state, &count) || count >= SIZE_MAX / sizeof *state->discovery_sources)
        return FOLIO_STATE_OUT_OF_MEMORY;
    state->discovery_sources = calloc(count + 1, sizeof *state->discovery_sources);
    if (state->discovery_sources == nullptr)
        return FOLIO_STATE_OUT_OF_MEMORY;
    state->discovery_source_count = count;
    state->discovery_refresh = false;
    return FOLIO_STATE_READY;
}

static enum index_match_scope discovery_match_scope(enum discovery_mode mode)
{
    switch (mode)
    {
    case DISCOVERY_MODE_NAMES:
        return INDEX_MATCH_NAMES;
    case DISCOVERY_MODE_BODY:
        return INDEX_MATCH_BODY;
    }
}

static struct index_filter_query discovery_query(const struct folio_state *_Nonnull state)
{
    return (struct index_filter_query){
        .term = utf8_text_bytes(state->discovery_query),
        .length = utf8_text_length(state->discovery_query),
        .tags = state->discovery_tags,
        .mode = state->discovery_tag_mode,
        .scope = discovery_match_scope(state->discovery_mode),
        .favorites_only = state->discovery_scope == NOTE_SCOPE_FAVORITES,
        .favorites_first = state->discovery_order == NOTE_ORDER_FAVORITES_FIRST};
}

static size_t discovery_restored_selection(const struct folio_state *_Nonnull state)
{
    struct note_ref previous = {0};
    if (state->discovery_identity == nullptr ||
        folio_state_resolve_note(state, state->discovery_identity, &previous) != FOLIO_STATE_READY)
        return 0;
    size_t count = index_filter_ranked_count(state->discovery_results);
    for (size_t index = 0; index < count; ++index)
    {
        struct note_ref ref = index_filter_ranked(state->discovery_results, index).ref;
        if (ref.category == previous.category && ref.note == previous.note)
            return index;
    }
    return 0;
}

enum folio_state_outcome folio_state_select_discovery(struct folio_state *_Nonnull state,
                                                      size_t index)
{
    if (state->discovery_phase != DISCOVERY_READY ||
        index >= index_filter_ranked_count(state->discovery_results))
        return FOLIO_STATE_NO_SUCH_NOTE;
    struct note_identity *_Nullable identity = nullptr;
    enum folio_state_outcome captured = folio_state_capture_note(
        state, index_filter_ranked(state->discovery_results, index).ref, &identity);
    note_excerpt_destroy(state->discovery_excerpt);
    note_identity_destroy(state->discovery_identity);
    state->discovery_excerpt = nullptr;
    state->discovery_identity = identity;
    state->discovery_selected = index;
    state->discovery_notice = captured;
    state->discovery_preview =
        captured == FOLIO_STATE_READY ? DISCOVERY_PREVIEW_WAITING : DISCOVERY_PREVIEW_FAILED;
    discovery_changed(state);
    return captured;
}

static enum folio_state_outcome finish_discovery(struct folio_state *_Nonnull state)
{
    switch (index_filter_finish(&state->discovery_build, &state->discovery_results))
    {
    case INDEX_FILTER_FINISH_NOT_READY:
        return FOLIO_STATE_READY;
    case INDEX_FILTER_FINISH_OUT_OF_MEMORY:
        return fail_discovery(state, FOLIO_STATE_OUT_OF_MEMORY);
    case INDEX_FILTER_FINISH_ACCEPTED:
        break;
    }
    state->discovery_phase = DISCOVERY_READY;
    discovery_results_changed(state);
    if (index_filter_ranked_count(state->discovery_results) != 0)
        return folio_state_select_discovery(state, discovery_restored_selection(state));
    note_identity_destroy(state->discovery_identity);
    state->discovery_identity = nullptr;
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome match_discovery(struct folio_state *_Nonnull state)
{
    if (state->discovery_build == nullptr)
    {
        struct index_filter_query query = discovery_query(state);
        enum folio_state_outcome created =
            create_index_build(state, &query, &state->discovery_build);
        if (created != FOLIO_STATE_READY)
            return fail_discovery(state, created);
    }
    struct index_filter_progress progress = {.cursor = state->discovery_cursor,
                                             .work = index_matching_work_budget,
                                             .uses_body = discovery_uses_body(state)};
    step_filter(state, state->discovery_build, &progress);
    state->discovery_cursor = progress.cursor;
    return finish_discovery(state);
}

static const struct note_text *_Nullable discovery_source_text(
    const struct folio_state *_Nonnull state, struct note_ref ref)
{
    return note_corpus_body(state->corpus, category_ledger_name(state->categories, ref.category),
                            note_ledger_name(state->notes[ref.category], ref.note));
}

static enum folio_state_outcome preview_discovery(struct folio_state *_Nonnull state)
{
    struct index_ranked_note row =
        index_filter_ranked(state->discovery_results, state->discovery_selected);
    struct note_ref ref = row.ref;
    size_t at = discovery_source_index(state, ref);
    enum folio_state_outcome read = FOLIO_STATE_READY;
    if (state->discovery_sources[at] == DISCOVERY_SOURCE_UNREAD)
    {
        invalidate_corpus_dependents(state);
        read = load_corpus_note(state, ref);
    }
    state->discovery_preview = DISCOVERY_PREVIEW_FAILED;
    state->discovery_notice = read;
    discovery_changed(state);
    if (read != FOLIO_STATE_READY)
        return read;
    const struct note_text *_Nullable body = discovery_source_text(state, ref);
    if (body == nullptr)
    {
        state->discovery_preview = DISCOVERY_PREVIEW_UNREADABLE;
        return FOLIO_STATE_READY;
    }
    enum note_excerpt_outcome excerpt = NOTE_EXCERPT_INVALID_RANGE;
    switch (state->discovery_mode)
    {
    case DISCOVERY_MODE_NAMES:
        excerpt = note_excerpt_create(body, &state->discovery_excerpt);
        break;
    case DISCOVERY_MODE_BODY:
        excerpt = note_excerpt_create_match(body, &row.match, &state->discovery_excerpt);
        break;
    }
    if (excerpt != NOTE_EXCERPT_READY)
    {
        state->discovery_notice = excerpt == NOTE_EXCERPT_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                                        : FOLIO_STATE_NO_SUCH_NOTE;
        return state->discovery_notice;
    }
    state->discovery_preview = DISCOVERY_PREVIEW_READY;
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome prepare_discovery_pass(struct folio_state *_Nonnull state)
{
    enum folio_state_outcome prepared = prepare_discovery_sources(state);
    if (prepared == FOLIO_STATE_READY && discovery_uses_body(state) &&
        !state->discovery_corpus_started)
        fresh_tag_corpus(state);
    return prepared;
}

enum folio_state_outcome folio_state_continue_discovery(struct folio_state *_Nonnull state)
{
    if (!folio_state_discovery_work_pending(state))
        return FOLIO_STATE_READY;
    enum folio_state_outcome prepared = prepare_discovery_pass(state);
    if (prepared != FOLIO_STATE_READY)
        return fail_discovery(state, prepared);
    if (state->discovery_preview == DISCOVERY_PREVIEW_WAITING)
        return preview_discovery(state);
    if (discovery_uses_body(state) && !state->corpus_loaded)
    {
        prepared = prepare_corpus_next(state);
        if (prepared != FOLIO_STATE_READY)
            return fail_discovery(state, prepared);
        if (!state->corpus_loaded)
            return FOLIO_STATE_READY;
    }
    state->discovery_phase = DISCOVERY_MATCHING;
    return match_discovery(state);
}

bool folio_state_discovery_work_pending(const struct folio_state *_Nonnull state)
{
    return state->discovery_phase == DISCOVERY_PREPARING ||
           state->discovery_phase == DISCOVERY_MATCHING ||
           state->discovery_preview == DISCOVERY_PREVIEW_WAITING;
}

struct discovery_summary folio_state_discovery_summary(const struct folio_state *_Nonnull state)
{
    struct discovery_summary summary = {
        .phase = state->discovery_phase,
        .mode = state->discovery_mode,
        .preview = state->discovery_preview,
        .notice = state->discovery_notice,
        .selected = state->discovery_selected,
        .revision = state->discovery_revision,
        .results_revision = state->discovery_results_revision,
        .unreadable = state->discovery_unreadable,
        .unsupported = discovery_uses_tags(state) ? state->discovery_unsupported : 0,
        .source_total = state->discovery_source_count,
        .source_checked = state->discovery_source_checked};
    if (state->discovery_results != nullptr)
    {
        summary.total = index_filter_count(state->discovery_results);
        summary.shown = index_filter_ranked_count(state->discovery_results);
    }
    return summary;
}

struct index_ranked_note folio_state_discovery_row(const struct folio_state *_Nonnull state,
                                                   size_t index)
{
    return index_filter_ranked(state->discovery_results, index);
}

static bool discovery_body_row_ready(const struct folio_state *_Nonnull state, size_t index)
{
    return state->discovery_phase == DISCOVERY_READY &&
           state->discovery_mode == DISCOVERY_MODE_BODY && state->discovery_results != nullptr &&
           index < index_filter_ranked_count(state->discovery_results);
}

enum folio_state_outcome folio_state_discovery_snippet(const struct folio_state *_Nonnull state,
                                                       size_t index, char *_Nonnull out,
                                                       struct note_snippet_clip *_Nonnull clip)
{
    if (!discovery_body_row_ready(state, index))
        return FOLIO_STATE_CANCELLED;
    struct index_ranked_note row = index_filter_ranked(state->discovery_results, index);
    const struct note_text *_Nullable body = discovery_source_text(state, row.ref);
    if (body == nullptr)
        return FOLIO_STATE_NO_SUCH_NOTE;
    *clip = note_excerpt_snippet(body, &row.match, out);
    return FOLIO_STATE_READY;
}

enum folio_state_outcome
folio_state_prepare_discovery_target(const struct folio_state *_Nonnull state,
                                     struct discovery_target *_Nullable *_Nonnull out)
{
    if (!discovery_body_row_ready(state, state->discovery_selected) ||
        state->discovery_identity == nullptr)
        return FOLIO_STATE_CANCELLED;
    struct index_ranked_note row =
        index_filter_ranked(state->discovery_results, state->discovery_selected);
    const struct note_text *_Nullable body = discovery_source_text(state, row.ref);
    if (body == nullptr)
        return FOLIO_STATE_NO_SUCH_NOTE;
    switch (discovery_target_create(state->discovery_identity, body, &row.match, out))
    {
    case DISCOVERY_TARGET_READY:
        return FOLIO_STATE_READY;
    case DISCOVERY_TARGET_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    case DISCOVERY_TARGET_UNEDITABLE:
        return FOLIO_STATE_NOTE_UNEDITABLE;
    case DISCOVERY_TARGET_INVALID_RANGE:
        return FOLIO_STATE_CANCELLED;
    }
}

enum discovery_target_check
folio_state_check_discovery_target(const struct folio_state *_Nonnull state,
                                   const struct discovery_target *_Nonnull target,
                                   struct note_search_span *_Nonnull selection)
{
    if (state->document != FOLIO_DOCUMENT_NAMED || state->mode != PANE_MODE_EDIT)
        return DISCOVERY_TARGET_UNAVAILABLE;
    struct note_ref ref = {0};
    if (folio_state_resolve_note(state, discovery_target_identity(target), &ref) !=
            FOLIO_STATE_READY ||
        ref.category != state->selected_category || ref.note != state->selected_note)
        return DISCOVERY_TARGET_UNAVAILABLE;
    if (!note_text_equals(state->body, discovery_target_body(target)))
        return DISCOVERY_TARGET_CHANGED;
    *selection = discovery_target_selection(target);
    return DISCOVERY_TARGET_CURRENT;
}

const struct note_excerpt *_Nullable folio_state_discovery_excerpt(
    const struct folio_state *_Nonnull state)
{
    return state->discovery_excerpt;
}

const struct note_tags *_Nullable folio_state_discovery_tags(
    const struct folio_state *_Nonnull state)
{
    return state->discovery_tags;
}

enum note_scope folio_state_discovery_scope(const struct folio_state *_Nonnull state)
{
    return state->discovery_scope;
}

enum note_order folio_state_discovery_order(const struct folio_state *_Nonnull state)
{
    return state->discovery_order;
}

enum tag_match_mode folio_state_discovery_tag_mode(const struct folio_state *_Nonnull state)
{
    return state->discovery_tag_mode;
}

void folio_state_clear_discovery_conditions(struct folio_state *_Nonnull state)
{
    discard_discovery_results(state);
    note_tags_destroy(state->discovery_tags);
    state->discovery_tags = nullptr;
    state->discovery_scope = NOTE_SCOPE_ALL;
    invalidate_discovery(state);
}

void folio_state_clear_discovery_scope(struct folio_state *_Nonnull state)
{
    state->discovery_scope = NOTE_SCOPE_ALL;
    invalidate_discovery(state);
}

enum folio_state_outcome folio_state_remove_discovery_tag(struct folio_state *_Nonnull state,
                                                          size_t index)
{
    if (state->discovery_tags == nullptr || index >= note_tags_count(state->discovery_tags))
        return FOLIO_STATE_TAG_INVALID;
    struct note_tags *_Nullable changed = nullptr;
    if (note_tags_with(state->discovery_tags, note_tags_at(state->discovery_tags, index),
                       NOTE_TAG_ABSENT, &changed) != NOTE_TAGS_READY)
        return fail_discovery(state, FOLIO_STATE_OUT_OF_MEMORY);
    discard_discovery_results(state);
    note_tags_destroy(state->discovery_tags);
    state->discovery_tags = changed;
    invalidate_discovery(state);
    return FOLIO_STATE_READY;
}
