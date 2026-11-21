/* 文書の一覧とカテゴリ設定の唯一の所有者（ARC-004）。起動時にポートから台帳と走査結果を受け、
 * core の照合で 1 つの索引に固定する。外から見える状態は不変で、UI は派生値を受け取り、
 * 操作は意図（folio_state_toggle_category 等）として渡すだけ（ARC-011）。 */
#ifndef NENEFOLIO_FOLIO_STATE_H
#define NENEFOLIO_FOLIO_STATE_H
#include "tag_apply_status.h"
#include "tag_management_step.h"
#include "tag_management_summary.h"
struct tag_update;
struct note_tag;
struct open_request;
struct folio_state;
/* A fresh read-only preview through the shared corpus loader. Null replacement removes from.
 * All saved ledger notes are examined, independently of the active display filters. */
[[nodiscard]] enum folio_state_outcome
folio_state_prepare_tag_management(struct folio_state *_Nonnull state,
                                   const struct note_tag *_Nonnull from,
                                   const struct note_tag *_Nullable to);
[[nodiscard]] enum folio_state_outcome
folio_state_apply_tag_management(struct folio_state *_Nonnull state);
[[nodiscard]] enum tag_management_step
folio_state_continue_tag_management(struct folio_state *_Nonnull state);
void folio_state_stop_tag_management(struct folio_state *_Nonnull state);
void folio_state_close_tag_management(struct folio_state *_Nonnull state);
[[nodiscard]] struct tag_management_summary
folio_state_tag_management_summary(const struct folio_state *_Nonnull state);
/* index < summary.prepared; values stay valid until close or a new prepare request. */
[[nodiscard]] const struct tag_update *_Nonnull folio_state_tag_management_at(
    const struct folio_state *_Nonnull state, size_t index);
[[nodiscard]] enum tag_apply_status
folio_state_tag_management_status(const struct folio_state *_Nonnull state, size_t index);
[[nodiscard]] enum tag_management_phase
folio_state_tag_management_phase(const struct folio_state *_Nonnull state);
/* Requires a non-CLOSED request. Values live until close or the next accepted preparation. */
[[nodiscard]] const struct note_tag *_Nonnull folio_state_tag_management_source(
    const struct folio_state *_Nonnull state);
[[nodiscard]] const struct note_tag *_Nullable folio_state_tag_management_target(
    const struct folio_state *_Nonnull state);

#include "category_rename_target.h"
#include "discovery_summary.h"
#include "discovery_target_check.h"
#include "drawer_cursor.h"
#include "drawer_metrics.h"
#include "folio_command.h"
#include "folio_command_target.h"
#include "folio_cursor_kind.h"
#include "folio_document_kind.h"
#include "folio_language.h"
#include "folio_layout_mode.h"
#include "folio_note_change.h"
#include "folio_ports.h"
#include "folio_state_outcome.h"
#include "folio_step.h"
#include "folio_theme.h"
#include "folio_theme_choice.h"
#include "folio_untitled_policy.h"
#include "history_row_view.h"
#include "image_port.h"
#include "index_ranked_note.h"
#include "markdown_source_selection.h"
#include "note_byte_span.h"
#include "note_destination.h"
#include "note_export_request.h"
#include "note_import_request.h"
#include "note_import_result.h"
#include "note_order.h"
#include "note_ref.h"
#include "note_scope.h"
#include "note_snippet_clip.h"
#include "note_times_view.h"
#include "open_place_kind.h"
#include "pane_mode.h"

#include "pane_title_view.h"
#include "product_link.h"
#include "rename_view.h"
#include "replace_apply.h"
#include "replace_request.h"
#include "rgb_color.h"
#include "search_direction.h"
#include "tag_catalog_status.h"
#include "tag_match_mode.h"
#include "theme_palette.h"

#include <stddef.h>
#include <stdint.h>
#include <uchar.h>

struct category_name;
struct drawer_layout;
struct folio_state;
struct note_name;
struct note_text;
struct note_identity;
struct note_excerpt;
struct discovery_target;
/* Identity copy is independent of row numbers and survives a save prompt. */
[[nodiscard]] enum folio_state_outcome
folio_state_capture_note(const struct folio_state *_Nonnull state, struct note_ref ref,
                         struct note_identity *_Nullable *_Nonnull out);
[[nodiscard]] enum folio_state_outcome
folio_state_resolve_note(const struct folio_state *_Nonnull state,
                         const struct note_identity *_Nonnull identity,
                         struct note_ref *_Nonnull out);
/* UI performs the existing named-EDIT save before opening. No document mutation here. */
[[nodiscard]] enum folio_state_outcome
folio_state_open_discovery(struct folio_state *_Nonnull state);
void folio_state_close_discovery(struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome
folio_state_query_discovery(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                            size_t count);
[[nodiscard]] enum folio_state_outcome
folio_state_set_discovery_mode(struct folio_state *_Nonnull state, enum discovery_mode mode);
[[nodiscard]] enum folio_state_outcome
folio_state_continue_discovery(struct folio_state *_Nonnull state);
[[nodiscard]] bool folio_state_discovery_work_pending(const struct folio_state *_Nonnull state);
[[nodiscard]] struct discovery_summary
folio_state_discovery_summary(const struct folio_state *_Nonnull state);
/* READY and index < summary.shown required. Row names remain ledger-owned. */
[[nodiscard]] struct index_ranked_note
folio_state_discovery_row(const struct folio_state *_Nonnull state, size_t index);
[[nodiscard]] enum folio_state_outcome
folio_state_select_discovery(struct folio_state *_Nonnull state, size_t index);
/* BODY/READY only. out has note_excerpt_snippet_capacity bytes. No IO or mutation. */
[[nodiscard]] enum folio_state_outcome
folio_state_discovery_snippet(const struct folio_state *_Nonnull state, size_t index,
                              char *_Nonnull out, struct note_snippet_clip *_Nonnull clip);
/* Own the current result's identity, source and position before any departure prompt. */
[[nodiscard]] enum folio_state_outcome
folio_state_prepare_discovery_target(const struct folio_state *_Nonnull state,
                                     struct discovery_target *_Nullable *_Nonnull out);
/* Only CURRENT writes the EDIT paragraph position. UI must also require a ready native pane. */
[[nodiscard]] enum discovery_target_check
folio_state_check_discovery_target(const struct folio_state *_Nonnull state,
                                   const struct discovery_target *_Nonnull target,
                                   struct note_search_span *_Nonnull selection);
/* Optional excerpt is available only in PREVIEW_READY, until the next session mutation. */
[[nodiscard]] const struct note_excerpt *_Nullable folio_state_discovery_excerpt(
    const struct folio_state *_Nonnull state);
[[nodiscard]] const struct note_tags *_Nullable folio_state_discovery_tags(
    const struct folio_state *_Nonnull state);
[[nodiscard]] enum note_scope folio_state_discovery_scope(const struct folio_state *_Nonnull state);
[[nodiscard]] enum note_order folio_state_discovery_order(const struct folio_state *_Nonnull state);
[[nodiscard]] enum tag_match_mode
folio_state_discovery_tag_mode(const struct folio_state *_Nonnull state);
/* Clear only this session's restrictions; preserve its initial priority preference. */
void folio_state_clear_discovery_conditions(struct folio_state *_Nonnull state);
void folio_state_clear_discovery_scope(struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome
folio_state_remove_discovery_tag(struct folio_state *_Nonnull state, size_t index);
struct note_tags;
struct tag_catalog;
struct note_tag;
struct replace_edit;
struct markdown_images;
struct markdown_media;
[[nodiscard]] const struct markdown_media *_Nullable folio_state_media(
    const struct folio_state *_Nonnull state);
/* Explicit observation intent; the view query itself never performs I/O. */
void folio_state_refresh_note_times(struct folio_state *_Nonnull state);
[[nodiscard]] struct note_times_view
folio_state_note_times(const struct folio_state *_Nonnull state);
[[nodiscard]] const struct markdown_images *_Nullable folio_state_images(
    const struct folio_state *_Nonnull state);
[[nodiscard]] enum image_outcome folio_state_request_image(struct folio_state *_Nonnull state,
                                                           size_t index, uint64_t *_Nonnull ticket);
[[nodiscard]] enum image_outcome
folio_state_poll_image(struct folio_state *_Nonnull state, uint64_t ticket,
                       struct image_bitmap *_Nullable *_Nonnull out);
[[nodiscard]] enum image_phase folio_state_image_phase(const struct folio_state *_Nonnull state);
[[nodiscard]] uint64_t folio_state_image_origin_revision(const struct folio_state *_Nonnull state);
void folio_state_cancel_images(struct folio_state *_Nonnull state);

[[nodiscard]] struct note_import_result
folio_state_import_notes(struct folio_state *_Nonnull state,
                         const struct note_import_request *_Nonnull request);
/* Current exports the editor snapshot without saving; ZIP scopes read saved originals. */
[[nodiscard]] enum folio_state_outcome
folio_state_export_notes(struct folio_state *_Nonnull state,
                         const struct note_export_request *_Nonnull request);

/* View-mode explicit activation; no document or editing rejects without an external effect. */
[[nodiscard]] enum folio_state_outcome folio_state_open_web_link(struct folio_state *_Nonnull state,
                                                                 const char *_Nonnull bytes,
                                                                 size_t length);
/* #900: 閲覧中のリンク先を、開くときと同じ検証（web_uri）を通した綴りでクリップボードの
 * ポートへ 1 回渡す。閲覧以外・文書なし・検証を通らないリンク先は WEB_LINK_REJECTED で、
 * ポートを呼ばない。文書・選択・Undo・未保存の状態は変えない。 */
[[nodiscard]] enum folio_state_outcome folio_state_copy_web_link(struct folio_state *_Nonnull state,
                                                                 const char *_Nonnull bytes,
                                                                 size_t length);
/* 閲覧中の現在の描画が持つコードブロック slot の原文を、クリップボードのポートへ 1 回渡す
 * （ADR0098）。現在の描画に無い slot・閲覧以外は REJECTED で、ポートを呼ばない。
 * 文書・選択・Undo・未保存の状態は変えない。 */
[[nodiscard]] enum folio_state_outcome folio_state_copy_code(struct folio_state *_Nonnull state,
                                                             size_t slot);
/* #1012 / ADR0116: whether the reading view's paragraphs (`view_total` marks, CR and U+0007, in
 * the pane's text) are the ones the current render's source map describes. */
[[nodiscard]] bool folio_state_source_mapped(const struct folio_state *_Nonnull state,
                                             size_t view_total);
/* The editing view's caret (UTF-16 units, each newline one) for the reading view's paragraph
 * `ordinal`: its content, or a table cell's text. SOURCE_UNMAPPED when the view and the map
 * disagree, the place has no source, or the note cannot be edited. Changes nothing. */
[[nodiscard]] enum folio_state_outcome folio_state_source_caret(struct folio_state *_Nonnull state,
                                                                size_t ordinal, size_t view_total,
                                                                size_t *_Nonnull native);
/* The Markdown of the blocks a reading-view selection touches, to the clipboard port once.
 * Document, selection and Undo do not change. */
[[nodiscard]] enum folio_state_outcome
folio_state_copy_markdown(struct folio_state *_Nonnull state,
                          struct markdown_source_selection selection, size_t view_total);

/* 製品の公式リンク（ADR0097）。文書・モードに依存せず、同じ検証とportへ一度だけ渡す。 */
[[nodiscard]] enum folio_state_outcome
folio_state_open_product_link(struct folio_state *_Nonnull state, enum product_link link);

/* 実行中だけの表示状態。設定/本文/IOに触れず、値だけを採用する（ADR0047）。 */
[[nodiscard]] enum folio_layout_mode
folio_state_layout_mode(const struct folio_state *_Nonnull state);
void folio_state_set_layout_mode(struct folio_state *_Nonnull state, enum folio_layout_mode mode);
/* #975 / ADR0114: 閲覧表示の横の「見出し」の欄を開いているか。実行中だけの表示状態で、初めは閉じて
 * いる。設定/本文/IOに触れない。 */
[[nodiscard]] bool folio_state_outline(const struct folio_state *_Nonnull state);
void folio_state_set_outline(struct folio_state *_Nonnull state, bool open);

/* ポートは複製して持つ。各ポートの adapter は state より長く生きていなければならない。
 * 束（struct folio_ports）は呼び出しの間だけ借り、返ったあとは参照しない（ADR 0029 の決定 3）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_create(const struct folio_ports *_Nonnull ports,
                   struct folio_state *_Nullable *_Nonnull out);
/* 通常表示での固定テーマID。HCの実効色はfolio_state_paletteを使う（ADR0057）。 */
[[nodiscard]] enum folio_theme folio_state_theme(const struct folio_state *_Nonnull state);
/* 実効配色。表示側はテーマIDから引き直さず、この値を共用する（ADR0057）。 */
[[nodiscard]] const struct theme_palette *_Nonnull folio_state_palette(
    const struct folio_state *_Nonnull state);
/* 実効値が変わったときだけ進む。UIは前後の等否で再着色を判断する。 */
[[nodiscard]] uint64_t folio_state_palette_revision(const struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome
folio_state_appearance_notice(const struct folio_state *_Nonnull state);
/* Cached OS appearance, for the system candidate preview; does not perform IO. */
[[nodiscard]] enum folio_theme folio_state_system_theme(const struct folio_state *_Nonnull state);
/* 保存済みのテーマ。設定画面の採用印とプレビュー取消の復元先（ADR0061）。 */
[[nodiscard]] enum folio_theme_choice
folio_state_theme_choice(const struct folio_state *_Nonnull state);
/* テーマの選択を変える意図（ADR 0031 の決定 3）。**新しい設定と新しい閲覧文書を先に作り、
 * data/settings.json へ書けてから採用する**ので、表示と保存が食い違わない。
 * 同じ保存値なら書かずに表示を戻す。作れなければ OUT_OF_MEMORY、書けなければ
 * SETTINGS_STORE_FAILED で、どちらもファイルも状態も変えない。設定が読めていなければ
 * SETTINGS_UNREADABLE（folio_state_set_number と同じ値で断り、上書きもしない）。
 * 設定は md・台帳・.rename.json と独立なので、未完了の改名の再開は通さない。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_theme(struct folio_state *_Nonnull state,
                                                             enum folio_theme_choice choice);
/* 一時的な表示choice。設定/IOは変えず、paletteとRTFを準備できたときだけ採用する。
 * OUT_OF_MEMORYなら表示も保持する。取消は保存済みtheme_choiceを渡す（ADR0061）。 */
[[nodiscard]] enum folio_state_outcome folio_state_preview_theme(struct folio_state *_Nonnull state,
                                                                 enum folio_theme_choice choice);
/* OS外観を再取得し、実効色が変わったときだけRTFを準備して同時に採用する（ADR0057）。
 * 取得/準備失敗は既存配色とsnapshotを保持し、appearance_noticeに理由を残す。設定は書かない。
 * UIはpalette_revisionの前後を比べて再着色する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_refresh_theme(struct folio_state *_Nonnull state);
/* 利用者が選んでいる表示言語（ADR 0032 の決定 5）。UI は ui_text_line / ui_text_format と
 * ui_font_face へこの値を渡す。値の所有者はここ 1 つで、UI は第 2 の言語を持たない（ARC-004）。 */
[[nodiscard]] enum folio_language folio_state_language(const struct folio_state *_Nonnull state);
/* 表示言語を変える意図（ADR 0032 の決定 5）。folio_state_set_theme と同じ順で、
 * **新しい設定と新しい閲覧文書（新しい face の fonttbl）を先に作り、書けてから採用する**。
 * いまと同じ言語なら書かずに READY。作れなければ OUT_OF_MEMORY、書けなければ
 * SETTINGS_STORE_FAILED で、どちらもファイルも状態も変えない。設定が読めていなければ
 * SETTINGS_UNREADABLE。未完了の改名の再開は通さない。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_language(struct folio_state *_Nonnull state,
                                                                enum folio_language language);
/* 起動時に読んだ設定の知らせ（ADR 0025 の決定 6）。読めていれば READY、
 * 壊れている・未知の版・読めないなら SETTINGS_UNREADABLE。合成ルートが窓を作る前に 1 回だけ
 * 尋ね、READY でなければ既存の 1 行で見せて起動を続ける（「見せたか」はどこにも持たない）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_settings_notice(const struct folio_state *_Nonnull state);
/* 直前の folio_state_select_note が編集から閲覧へ落としたとき（編集で保てない本文・#242）
 * NOTE_UNEDITABLE、それ以外 READY（#258）。select_note は入口で READY へ戻す。 */
[[nodiscard]] enum folio_state_outcome
folio_state_select_notice(const struct folio_state *_Nonnull state);
/* 編集中の本文の左に原文の行番号を出すか（FR-021 / ADR 0026 の決定 5）。
 * 値の所有者はここ 1 つで、UI は第 2 の真偽を持たない（ARC-004）。 */
/* ミニマップ表示設定。書込成功後だけ採用する（ADR0084）。 */
[[nodiscard]] bool folio_state_minimap(const struct folio_state *_Nonnull state);
/* 保存選択、OS観測の適用可否、その積。装飾以外の操作可否に使わない（ADR0096）。 */
[[nodiscard]] bool folio_state_motion(const struct folio_state *_Nonnull state);
[[nodiscard]] bool folio_state_motion_available(const struct folio_state *_Nonnull state);
[[nodiscard]] bool folio_state_motion_enabled(const struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome folio_state_set_motion(struct folio_state *_Nonnull state,
                                                              bool motion);
[[nodiscard]] enum folio_state_outcome folio_state_set_minimap(struct folio_state *_Nonnull state,
                                                               bool minimap);
[[nodiscard]] bool folio_state_number(const struct folio_state *_Nonnull state);
/* 行番号の表示を変える意図（ADR 0025 の決定 5）。**先に data/settings.json へ書いてから採用する**
 * ので、表示と保存が食い違わない。いまと同じ値なら書かずに READY。
 * 書けなければ値を変えず SETTINGS_STORE_FAILED、設定が読めていなければ SETTINGS_UNREADABLE
 * （上書きしない）。設定は md・台帳・.rename.json と独立なので、未完了の改名の再開は通さない。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_number(struct folio_state *_Nonnull state,
                                                              bool number);
/* いまの索引と寸法からドロワーの配置を作り、選択中のノート・カーソル・いまのスクロール量に印を付ける。
 * 呼び出し側が破棄する。問い合わせなので状態は変えない（要求量の丸めは配置の中だけ・ADR 0009）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_drawer_layout(const struct folio_state *_Nonnull state, struct drawer_metrics metrics,
                          struct drawer_layout *_Nullable *_Nonnull out);
/* ドロワーを delta 画素ぶんスクロールする意図（FR-012 / ADR 0009 の決定 3）。
 * いまの寸法での有効量（要求量を上限で丸めた量）から数え直し、0〜上限に丸めて要求量にする。
 * 折り畳みで上限が縮んでいても 1 回で必ず動く。配置が作れなければ OUT_OF_MEMORY で状態は変えない。
 * 選択・トグル・並び替え・移動・編集はスクロール量を触らない。 */
[[nodiscard]] enum folio_state_outcome folio_state_scroll_drawer(struct folio_state *_Nonnull state,
                                                                 struct drawer_metrics metrics,
                                                                 int delta);
/* 索引にあるノートの総数。 */
[[nodiscard]] size_t folio_state_note_count(const struct folio_state *_Nonnull state);
/* ドロワーのスクロール量を、カーソルの行が見える位置に来る最小の量にする
 * （FR-018 / ADR 0013 の決定 6・ADR 0015 の決定 6）。カテゴリ行のカーソルにも効く。
 * カーソルが無ければ READY で何も変えない。
 * 配置が作れなければ OUT_OF_MEMORY で状態は変えない。鍵でカーソルを動かしたときだけ使う。 */
[[nodiscard]] enum folio_state_outcome folio_state_reveal_cursor(struct folio_state *_Nonnull state,
                                                                 struct drawer_metrics metrics);
/* 行だけを見える位置へ寄せる。cursor・選択・本文は変えない。 */
[[nodiscard]] enum folio_state_outcome folio_state_reveal_row(struct folio_state *_Nonnull state,
                                                              struct drawer_metrics metrics,
                                                              struct drawer_cursor cursor);
[[nodiscard]] int folio_state_drawer_scroll(const struct folio_state *_Nonnull state);
/* カテゴリの展開状態を反転し、台帳を書き戻す（FR-004 / FR-007）。書き戻せなければ状態は変えない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_category(struct folio_state *_Nonnull state, size_t index);
/* 展開状態だけを採用する。別ノートを選択せず、消えたcursorだけ可視の行へ戻す。 */
[[nodiscard]] enum folio_state_outcome folio_state_set_expansion(struct folio_state *_Nonnull state,
                                                                 size_t index, bool expanded);
/* カテゴリの展開状態を expanded にする（FR-018 の h / l・ADR 0015 の決定 3）。
 * index が範囲外なら NO_SUCH_CATEGORY。既にそうなら書かずに READY（カーソルも動かさない）。
 * 変わるときは folio_state_toggle_category と同じ経路で書き戻す。
 * カーソルがそのカテゴリにあるときだけ、書き戻したあとにカーソルが移る:
 * 畳めばそのカテゴリ行へ、展開すれば選択中のノートがその中にあればそのノートへ、
 * 無ければ最初のノートへ（右ペインが変わる。読めなければその結果を返しカーソルは行に残る）。
 * ノートが 0 本ならカテゴリ行に留まる。 */
[[nodiscard]] enum folio_state_outcome
folio_state_set_category_expanded(struct folio_state *_Nonnull state, size_t index, bool expanded,
                                  enum folio_untitled_policy untitled);
/* カテゴリの色を変え、categories.json を書き戻す（FR-010 / ADR 0010 の決定 2）。
 * index が範囲外なら NO_SUCH_CATEGORY。いまと同じ色なら書かずに READY。
 * 書き戻せなければ状態は変えない。選択・編集モード・スクロール量は触らない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_recolor_category(struct folio_state *_Nonnull state, size_t index,
                             struct rgb_color color);
/* カテゴリの表示順を変え、categories.json を書き戻す（FR-009）。from == to は書かずに READY。
 * 書き戻せなければ状態は変えない。選択中のノートは移動後も同じノートを指す。 */
[[nodiscard]] enum folio_state_outcome folio_state_move_category(struct folio_state *_Nonnull state,
                                                                 size_t from, size_t to);
/* 空のカテゴリを末尾に作り、categories.json を書き戻す（FR-034 / ADR 0039 の決定 10）。
 * 絞り込み中は同期より先に FILTERED。大小文字だけ違う名前も含めて既にあれば CATEGORY_NAME_TAKEN。
 * フォルダを作れなければ CATEGORY_NOT_CREATED で、ここまでの失敗は状態を変えない。
 * フォルダを作った後で台帳を書けなければ採用して CATEGORY_LEDGER_STALE（決定 3・補正 1）。
 * 作ったらカーソルを新しいカテゴリ行へ置く。選択・開いている文書・モード・スクロール量は触らない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_create_category(struct folio_state *_Nonnull state,
                            const struct category_name *_Nonnull name);
/* 空のカテゴリだけ削除する（ADR 0039 補正 5）。確保は port より前に完了する。
 * REMOVED / ABSENT は採用し、台帳を書けなくても CATEGORY_LEDGER_STALE。既存の文書は保存しない。
 * port の失敗は採用せず、履歴や index が処理済みの場合がある。即時の物理不在は保証しない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_delete_category(struct folio_state *_Nonnull state, size_t category);
/* ノートを from から to へ動かす（FR-008 / FR-009）。同じカテゴリなら表示順を変えて index.json を
 * 書き戻し（from == to は書かずに READY・書き戻せなければ状態は変えない）、別のカテゴリなら md を
 * 移してから移動先・移動元の index.json を書き戻す（ADR 0008 の決定 3）。to.note は移動先の
 * ノート数と等しければ末尾。移動先に同じ名前があれば NAME_TAKEN で何もしない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_move_note(struct folio_state *_Nonnull state, struct note_ref from, struct note_ref to);
/* ノートを別のカテゴリの末尾へ動かす（#941）。移動先の位置を UI に数えさせないための入口で、
 * 絞り込み中・同名の拒否と失敗は folio_state_move_note と同じ。末尾は表示順に依らないので、
 * お気に入り優先の表示中でも断らない（ADR0111）。いまと同じカテゴリなら何もせず READY。 */
[[nodiscard]] enum folio_state_outcome
folio_state_move_note_to_category(struct folio_state *_Nonnull state, struct note_ref from,
                                  size_t category);
/* 指定ノートと履歴をごみ箱へ移す（ADR 0040）。現在の編集中の文書だけ先に保存する。
 * 成功なら現在文書は NONE / VIEW、別ノートなら本文とモードを保つ。 */
[[nodiscard]] enum folio_state_outcome folio_state_trash_note(struct folio_state *_Nonnull state,
                                                              struct note_ref target,
                                                              const char16_t *_Nonnull units,
                                                              size_t count);
/* ノートを選び、本文を読んで右ペインの表示値を作る（FR-005）。読めなければ表示は変えない。
 * 表示モードは変えない（ADR 0013 の決定 4）。 */
[[nodiscard]] enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                               size_t category, size_t note,
                                                               enum folio_untitled_policy untitled);
/* カーソルを歩みのぶんだけ動かす（FR-018 / ADR 0015 の決定 2）。
 * 止まる行は「カテゴリごとに、見えるノート行があればそのノート、無ければそのカテゴリ行 1 つ」で、
 * 台帳の順に辿る。カーソルが無ければ NEXT / FIRST は最初の止まる行、PREVIOUS / LAST は最後。
 * 端では動かず READY。止まる行が 1 つも無ければ（カテゴリが無い）NO_SUCH_NOTE。
 * ノートに止まるときは folio_state_select_note と同じ読み込み経路で、表示モードは変わらない。
 * カテゴリ行に止まるときはカーソルだけが動き、選択も右ペインも変わらない（保存も要らない）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_select_adjacent(struct folio_state *_Nonnull state, enum folio_step step,
                            enum folio_untitled_policy untitled);
/* 歩みの行き先の種類（ADR 0015 の決定 2）。行き先が無ければ（端・止まる行が無い）false で
 * kindとoutは触らない。UIは保存前に行き先を写し、保存でカーソルが動いても同じ対象へ進む。 */
[[nodiscard]] bool folio_state_step_kind(const struct folio_state *_Nonnull state,
                                         enum folio_step step,
                                         enum folio_cursor_kind *_Nonnull kind,
                                         struct note_ref *_Nonnull out);
/* 索引のカーソル。無ければ false で kind も out も触らない。
 * FOLIO_CURSOR_CATEGORY のとき out->note は使わない（カテゴリ行に止まっている）。 */
[[nodiscard]] bool folio_state_cursor(const struct folio_state *_Nonnull state,
                                      enum folio_cursor_kind *_Nonnull kind,
                                      struct note_ref *_Nonnull out);
/* 可視の行へcursorだけを置く。保存・読込・未完了処理の再試行は行わず、本文/選択を保つ。
 * 行が見えない場合はNO_SUCH_NOTE、カテゴリ自体が無ければNO_SUCH_CATEGORY。 */
[[nodiscard]] enum folio_state_outcome folio_state_focus_cursor(struct folio_state *_Nonnull state,
                                                                enum folio_cursor_kind kind,
                                                                struct note_ref target);
/* 行集合/名前/順序/展開の改訂。派生レイアウトの無効化に使う。 */
[[nodiscard]] uint64_t folio_state_index_revision(const struct folio_state *_Nonnull state);
/* 行の意味を変える改訂。開閉は保持し、消えた行は可視性で拒む。上限で行公開を停止する。 */
[[nodiscard]] uint64_t folio_state_row_revision(const struct folio_state *_Nonnull state);
/* 選択中のノートの居場所。何も選んでいなければ false で out は触らない。 */
[[nodiscard]] bool folio_state_selection(const struct folio_state *_Nonnull state,
                                         struct note_ref *_Nonnull out);
/* 編集モードへ入る（FR-006）。ノートを選んでいなければ NOTHING_SELECTED。 */
[[nodiscard]] enum folio_state_outcome folio_state_begin_edit(struct folio_state *_Nonnull state);
/* 現在文書の種類。無題は索引に存在しない（ADR0020）。 */
[[nodiscard]] enum folio_document_kind
folio_state_document_kind(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_category_count(const struct folio_state *_Nonnull state);
/* Whether this category expansion will actually replace the held document (ADR0050). */
[[nodiscard]] bool folio_state_category_replaces_document(const struct folio_state *_Nonnull state,
                                                          size_t category, bool expanded);
[[nodiscard]] const char *_Nonnull folio_state_category_name(
    const struct folio_state *_Nonnull state, size_t category);
/* その名前のカテゴリの、いまの一覧での番号。無ければ false で out は変えない（#1069 / ADR0117）。
 * 意図は書く前に一覧を読み直すので、番号は意図をまたいで持たない。面が開いているあいだ相手を
 * 覚えるときは名前で覚え、意図を呼ぶたびにここで番号へ引き直す。 */
[[nodiscard]] bool folio_state_find_category(const struct folio_state *_Nonnull state,
                                             const char *_Nonnull name, size_t *_Nonnull out);
/* #1070 / ADR0119: data の変更に気づいたときに、すべての一覧（カテゴリの台帳と、各カテゴリの
 * ノートの台帳）をディスクから読み直す。ADR0117
 * の読み直しの部品をそのまま使い、同じなら何もしない。
 * **読むだけで、何も書かない。** 利用者の意図ではなく背景のきっかけ（フォルダの変化・窓が前面に
 * 戻った）で呼ばれるので、意図の前の同期（保留中の台帳の書き込み・止まっている改名の再開）は
 * 行わない: それらは次の利用者の意図が行う。ここで書くと、書いたことがまた変化の知らせを呼ぶ。
 * - 改名が保留のあいだは何も読まない（ディスクの名前がもう新しいことがあり、古い名前で読むと
 *   一覧が空になる）。READY で *changed は false。
 * - 一覧が入れ替わったら *changed を true にする（呼ぶ側は描き直す）。
 * - あるカテゴリが読めなくても、ほかのカテゴリは読み直す。結果は最初の失敗（無ければ READY）。
 * 呼ぶ側の約束:
 * - 番号を持っている面・操作の途中では呼ばない（#1069 の教訓）。一覧は意図の外で入れ替わるので、
 *   呼んだ後は窓が持つ番号をすべて引き直す。
 * - 失敗の結果を、きっかけのたびに箱で出さない（壊れた台帳があると、変化のたびに同じ結果が返る）。
 */
[[nodiscard]] enum folio_state_outcome folio_state_refresh_lists(struct folio_state *_Nonnull state,
                                                                 bool *_Nonnull changed);
/* 開いている名前のあるノートのファイルが、読み込んだときの本文と違うか。違えば *stale を true。
 * 無題・未選択・改名の保留中・ファイルが無い／読めないときは
 * false（新しい本文があるとは言えない）。 何も変えない:
 * 読み直すか、知らせるだけにするかは、表示の側が表示モードで決める。 */
[[nodiscard]] enum folio_state_outcome
folio_state_document_stale(struct folio_state *_Nonnull state, bool *_Nonnull stale);
/* #1070 / ADR0119 の決定 3: 開いている名前のあるノートを、ファイルから読み直す。選択も表示モードも
 * 変えない（編集で保てない本文になったときだけ、ノートの切り替えと同じく閲覧へ落とす）。
 * 読めなければ何も変えない（NOTE_UNREADABLE）。無題・未選択は NOTHING_SELECTED。
 * 編集中に呼ぶと、編集の元になる本文が入れ替わる: 表示の側が本文を流し込み直す。 */
[[nodiscard]] enum folio_state_outcome
folio_state_reload_document(struct folio_state *_Nonnull state);
/* #1071 / ADR0118: 外から来た「開く」依頼のノートが、いまの一覧のどこにあるか。
 * 引く前に、カテゴリの台帳と、そのカテゴリのノートの台帳を読み直す（依頼を書いた側が、直前に
 * ノートを外で作っていることがある。読み直しても書かない＝ADR0117 の決定 6）。名前は ASCII の
 * 英字だけ大小を問わずに引く（ASCII 以外の大小や正規化の違いは別の名前として扱い、見つからない
 * 側へ倒す）。
 * READY なら out は、すぐ次に呼ぶ「ノートを選ぶ」意図へ渡せる番号。カテゴリが無ければ
 * NO_SUCH_CATEGORY、ノートが無ければ NO_SUCH_NOTE で、out は変えない。読み直せなければその結果。
 * 一覧に無いものは作らない。
 * 呼ぶ側の約束（#1069 の教訓。番号は意図をまたいで持たない）:
 * - どの結果でも一覧は読み直した新しいものになりうる。番号を持っている面（入力面・メニュー・
 *   ドラッグ・取り込み・削除の途中）が開いているあいだは呼ばない。呼んだ後は結果によらず描き直す。
 * - 編集中の保存や、無題の下書きに名前を付ける手順は、これを呼ぶ**前**に済ませる（それらは一覧に
 *   ノートを足して番号をずらす）。依頼は名前で持ち、番号はここで引いてすぐ使う。 */
[[nodiscard]] enum folio_state_outcome
folio_state_locate_open_request(struct folio_state *_Nonnull state,
                                const struct open_request *_Nonnull request,
                                struct note_ref *_Nonnull out);
/* 開いている文書の保存済みの原文の上で、依頼の箇所を決める（open_place_find）。文書が無ければ
 * OPEN_PLACE_NONE。範囲は原文のバイト位置。 */
[[nodiscard]] enum open_place_kind
folio_state_open_place(const struct folio_state *_Nonnull state,
                       const struct open_request *_Nonnull request,
                       struct note_byte_span *_Nonnull out);
/* 原文の範囲が、いまの閲覧表示のどの段落か（first から last の手前まで。ADR0116 の対応表を逆に
 * 引く）。view_total は閲覧欄がいま持っている段落の記号の数で、対応表と合わなければ引かない。
 * 引けなければ SOURCE_UNMAPPED で out は変えない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_open_paragraphs(const struct folio_state *_Nonnull state, struct note_byte_span source,
                            size_t view_total, struct markdown_source_selection *_Nonnull out);
/* カーソル→現在文書→最初のカテゴリ。カテゴリ0件なら0で、new_noteが拒否する。 */
[[nodiscard]] size_t folio_state_current_category(const struct folio_state *_Nonnull state);
/* 現在文書の保存先。NONEのときは呼ばない。 */
[[nodiscard]] size_t folio_state_document_category(const struct folio_state *_Nonnull state);
/* PROTECTは無題をNAME_REQUIREDで保護する。DISCARDはこの一回の置換だけを許す。
 * 空本文・閲覧RTF・編集RTFを採用前に準備し、失敗では現在文書を保つ（ADR0050）。 */
[[nodiscard]] enum folio_state_outcome folio_state_new_note(struct folio_state *_Nonnull state,
                                                            size_t category,
                                                            enum folio_untitled_policy untitled);
/* 初回/別名保存の共通作成。VIEWではunits/countを使わず所有する原文を複製する。
 * EDITでは入力を正規化。元ノートのwrite/archiveは呼ばず、成功時は新ノートを選択する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_store_new(struct folio_state *_Nonnull state,
                      const struct note_destination *_Nonnull destination,
                      const char16_t *_Nonnull units, size_t count);
/* 現在の文書の実名（台帳にある名前・終端付き UTF-8）。無題と未選択では空文字列。
 * 右ペインの頭の表示値とは別で、名前入力面の初期値はこちらを使う（ADR 0022 の決定 1）。
 * 未完了の改名があるあいだも旧名を返すが、その名前の実体の現存は保証しない。次の意図まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_document_name(
    const struct folio_state *_Nonnull state);
/* 未完了の改名の旧名と新名。保持していなければ false で out は触らない（ADR 0022 の決定 2）。
 * 文字列は次の意図まで有効で、改名が完了すると無効になる。 */
[[nodiscard]] bool folio_state_rename_pending(const struct folio_state *_Nonnull state,
                                              struct rename_view *_Nonnull out);
/* 現在のノートの md と履歴を同じカテゴリの新しい名前へ移す（FR-031 / ADR 0022）。
 * units / count は編集中の本文で、未保存なら同じ保存経路で先に確定してから改名する
 * （VIEW では使わない）。未選択は NOTHING_SELECTED、無題は NAME_REQUIRED（初回保存が先）、
 * 完全同名は NAME_UNCHANGED（先行同期のみ）。大小文字だけ違う名前を含む同名は NAME_TAKEN。
 *
 * 記録を公開する前の拒否では意図が残らない。公開後に止まったら RENAME_PENDING（再試行で進み得る）
 * または RENAME_HALTED（data/ を直すまで進まない）を返し、どちらでも意図を 1 つだけ保持して、
 * 次の意図より先に同じ改名を再試行する（決定 7）。
 * 成功しても本文・モード・選択位置は変えない。name は呼び出しの間だけ借りる。 */
[[nodiscard]] enum folio_state_outcome
folio_state_rename_note(struct folio_state *_Nonnull state, const struct note_name *_Nonnull name,
                        const char16_t *_Nonnull units, size_t count);
/* 対象カテゴリの名前だけを変える（ADR0041）。FILTERED・範囲拒否→共通同期→同名no-op→
 * ASCII-fold衝突→必要な保存→事前準備→port。units/count は対象NAMED+EDITだけ使う。
 * UNTITLED・VIEW・NONE・別カテゴリの本文は保存せず、台帳採用は添字・本文・modeを保つ。
 * target/name は呼び出しの間だけ借り、未完了planと空corpusだけをstateが所有する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_rename_category(struct folio_state *_Nonnull state,
                            const struct category_rename_target *_Nonnull target,
                            const char16_t *_Nonnull units, size_t count);
/* 保持中の意図を共通同期で再開するだけ。新保存・新改名・文書選択は行わない。
 * READYは元の要求を実行した意味ではない。借用rename_viewは呼び出し後に取り直す。 */
[[nodiscard]] enum folio_state_outcome folio_state_retry_rename(struct folio_state *_Nonnull state);
/* 編集中の本文（UTF-16 の単位列）を保存し、編集モードのまま残る（Ctrl+S）。
 * VIEWでは台帳だけ同期し本文は書かない。読んだ本文と同じなら書かない。書き戻せなければ状態は変えない（ADR
 * 0006）。 */
[[nodiscard]] enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                              const char16_t *_Nonnull units,
                                                              size_t count);
/* Same save implementation, additionally requiring the file to match the last saved body.
 * Used before global tag management. A same-body edit still checks, without writing. */
[[nodiscard]] enum folio_state_outcome
folio_state_store_note_checked(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                               size_t count);
/* 編集中の本文を保存時と同じ形へ正規化し、保存済み本文との差を答える。
 * VIEWでは台帳・保存済み本文の写し・改名が同期済みならSAME。未同期なら保存系と同じ修復を先に試み、
 * index修復の失敗はLEDGER_UNSYNCED、写しの記憶不足はOUT_OF_MEMORY。この修復のほかは
 * mode・本文・履歴・md を変えない問い合わせである（ADR 0016・ADR 0021 の決定 3）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_note_changed(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                         size_t count, enum folio_note_change *_Nonnull out);
/* 保存の主操作・未保存印・UIA の状態の 1 つの元（ADR0060 の決定 8）。未選択と閲覧は SAME、
 * 無題は開いた時点から CHANGED、編集中は note_changed と同じ正規化で保存済み本文と比べる。
 * 台帳の同期も修復もしない純粋な問い合わせで、本文を写せない（記憶不足・壊れた文字）ときは
 * 保存を促す側の CHANGED と答える。 */
[[nodiscard]] enum folio_note_change folio_state_unsaved(const struct folio_state *_Nonnull state,
                                                         const char16_t *_Nonnull units,
                                                         size_t count);
/* md を書き戻すか新しい md を作るたびに 1 進む世代。保存の成功を UI が 1 か所で知るために使う
 * （同じ本文で書かなかった保存では進まない）。 */
[[nodiscard]] uint64_t folio_state_store_revision(const struct folio_state *_Nonnull state);
/* 編集中の本文を保存して閲覧へ戻る。保存できなければ編集モードのまま。
 * 使うのは「閲覧」の札。終了操作は編集モードを変えず store_note で保存する（ADR 0016）。
 * ノートの切り替えではモードを保つ（ADR 0013 の決定 4）。 */
[[nodiscard]] enum folio_state_outcome folio_state_end_edit(struct folio_state *_Nonnull state,
                                                            const char16_t *_Nonnull units,
                                                            size_t count);
/* 編集を破棄して保存済みの本文に戻す（`:e!`・ADR 0016 の補正 4）。前提の判定だけで状態は変えない:
 * 未選択は NOTHING_SELECTED、閲覧中は NOT_EDITING、編集中は READY。READY なら UI が
 * folio_state_pane_text（最後に読んだか保存に成功した本文・無題なら空）を本文へ流し込み直す。
 * ディスクは読み直さない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_discard_edits(const struct folio_state *_Nonnull state);
/* 索引の絞り込み要求（FR-032 / ADR0068/0073）。UTF-16入力はC-014の例外7。
 * 空語は取消。不正語/語確保OOMは前の要求/結果を保つ。先行copy/改名修復は同期で行う。
 * 有効語の受理後はwarmでも最新要求だけを持ち、全索引を準備中表示する。本文read/全一致計算をしない。
 * 語の差替えは未完成本文の読込位置を保ち、一致計算進捗だけを破棄する。
 * 更新はmatchingを破棄し、未完成本文走査だけを先頭へ戻す。builder/finish OOMは要求を停止する。
 * 現在文書/本文/modeは変えず、語の受理でscrollを戻す。cursorは結果の採用/取消で整える。 */
[[nodiscard]] enum folio_state_outcome
folio_state_set_index_filter(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                             size_t count);
/* Copy immutable tag conditions, retaining the latest text term. No note is edited or saved.
 * UI must use the same named-editor save barrier as text filtering before entering the surface.
 * Pre-acceptance failure preserves query/results. An asynchronous build failure stops and clears
 * the entire latest query, reports its error and never leaves tag conditions falsely effective.
 * Empty tags remove only the tag restriction; empty text removes only the text restriction. */
[[nodiscard]] enum folio_state_outcome
folio_state_set_tag_filter(struct folio_state *_Nonnull state,
                           const struct note_tags *_Nonnull tags, enum tag_match_mode mode);
/* A fresh saved snapshot for one open tag surface. These intents never scan files.
 * The same continue_index_filter driver prepares at most one corpus note per idle call.
 * Closing discards the snapshot/request; it leaves filtering conditions and corpus intact. */
void folio_state_request_tag_catalog(struct folio_state *_Nonnull state);
void folio_state_close_tag_catalog(struct folio_state *_Nonnull state);
/* Copy the native snapshot immediately in EDIT; VIEW uses the saved original.
 * Failure preserves an existing session. No save, dirty mark or native edit occurs here. */
[[nodiscard]] enum folio_state_outcome
folio_state_begin_tag_edit(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                           size_t count);
void folio_state_close_tag_edit(struct folio_state *_Nonnull state);
/* Current source summary without starting an edit session. VIEW uses its owned metadata;
 * EDIT uses the canonical native conversion/parser. Failure preserves out. */
[[nodiscard]] enum folio_state_outcome
folio_state_note_tag_count(const struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                           size_t count, size_t *_Nonnull out);
/* Omitted only when no edit session is active. Borrow until the next tag intention. */
[[nodiscard]] const struct note_tags *_Nullable folio_state_edit_tags(
    const struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_edit_tag(struct folio_state *_Nonnull state,
                            const struct note_tag *_Nonnull tag);
/* Add an entered name; existing membership is unchanged. Row toggles perform removal. */
[[nodiscard]] enum folio_state_outcome
folio_state_enter_edit_tag(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                           size_t count);
/* Verify current destination and source before producing one native replace_edit.
 * READY with omitted edit means unchanged. Failure leaves out/session unchanged.
 * UI applies once through note_pane_replace, then closes the session. */
[[nodiscard]] enum folio_state_outcome
folio_state_apply_tag_edit(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                           size_t count, struct replace_edit *_Nullable *_Nonnull out);
[[nodiscard]] enum tag_catalog_status
folio_state_tag_catalog_status(const struct folio_state *_Nonnull state);
/* AVAILABLE is required; snapshot is valid until request/close/destroy. */
[[nodiscard]] const struct tag_catalog *_Nonnull folio_state_tag_catalog(
    const struct folio_state *_Nonnull state);
[[nodiscard]] bool folio_state_index_work_pending(const struct folio_state *_Nonnull state);
/* Borrowed until the next intent; nullptr means the optional tag restriction is absent. */
[[nodiscard]] const struct note_tags *_Nullable folio_state_tag_filter(
    const struct folio_state *_Nonnull state);
[[nodiscard]] enum tag_match_mode
folio_state_tag_match_mode(const struct folio_state *_Nonnull state);
/* 一回最大一ノートread後、本文が完成済みなら65536比較workまでmatchingを継続する。
 * 最初のmatching継続はcount/build作成とworkを同じcallで行い、count0は即finish。
 * 未読/不正本文は旧copyも除く。OOM/先行同期失敗は要求を止め、完成時だけ最新語の結果を採用。
 * 比較workは実時間上限ではない。部分結果は非公開、要求無しなら何もしない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_continue_index_filter(struct folio_state *_Nonnull state);
[[nodiscard]] bool folio_state_index_filter_pending(const struct folio_state *_Nonnull state);
/* 準備要求または確定した絞り込みがあるか。UIはこれを見てドラッグを始めない（ADR0068）。 */
[[nodiscard]] bool folio_state_filtering(const struct folio_state *_Nonnull state);
/* 最新の準備要求または確定語（UTF-8・終端付き）。無ければ空文字列。次のset/continueまで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_index_filter_term(
    const struct folio_state *_Nonnull state);
/* いま索引に見えているノートの数。絞り込んでいなければ索引の総数。 */
[[nodiscard]] size_t folio_state_index_filter_count(const struct folio_state *_Nonnull state);
/* 置換の下見を取り直す（FR-023 / ADR 0028 の決定 6）。UTF-16 の入口の**8 本目**で、
 * 位置が EM_EXSETSEL と 1 対 1 でなければならないので本文は UTF-8 へ写さない（C-014 の例外）。
 * 編集中でなければ NOT_EDITING、パターンが空なら REPLACE_NO_PATTERN。
 * 走査と置換文字列の失敗は REPLACE_BAD_PATTERN / REPLACE_BAD_TEMPLATE / REPLACE_TIMED_OUT /
 * REPLACE_TOO_COMPLEX / REPLACE_TOO_MANY へ写し、そのときは前の下見をそのまま保つ。
 * 成功すると古い下見を捨てて新しい下見を持つ。件数はゼロ幅の一致も 1 件。
 * 本文も選択も RichEdit が持ち、ここは触らない（ADR 0023 の決定 3 と同じ境界）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_preview_replace(struct folio_state *_Nonnull state,
                            const struct replace_request *_Nonnull request);
/* 有効な0件の下見と破棄済みを区別する純粋な問い合わせ（ADR0041）。 */
[[nodiscard]] bool folio_state_has_replace_preview(const struct folio_state *_Nonnull state);
/* 直前の下見の一致の件数。下見が無ければ 0。 */
[[nodiscard]] size_t folio_state_replace_count(const struct folio_state *_Nonnull state);
/* 直前の下見が REPLACE_BAD_PATTERN だったときの位置（1 起算のコード単位）。
 * それ以外は 0 で、UI は位置を添えない（決定 9）。 */
[[nodiscard]] size_t folio_state_replace_error_offset(const struct folio_state *_Nonnull state);
/* 下見を本文へ当てる（決定 6）。UTF-16 の入口の**9 本目**。
 * 渡された本文が下見の写しと違う、宛先が今の文書と違う、下見が無いなら REPLACE_STALE で
 * 何も変えない。反転した anchor は REPLACE_BAD_SPAN。
 * 成功しても md は書かず、RichEdit へ当てるのは UI である。適用する一致が無ければ READY で
 * out は nullptr になる。out は失敗のときも nullptr で、成功時だけ呼び出し側が
 * replace_edit_destroy する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_apply_replace(struct folio_state *_Nonnull state,
                          const struct replace_apply *_Nonnull apply,
                          struct replace_edit *_Nullable *_Nonnull out);
/* ノート内検索の語を覚える（FR-011 / ADR 0023 の決定 3）。UTF-16 の単位列を受ける
 * C-014 の例外で、store_new / rename_note / store_note / note_changed / end_edit に次ぐ 6 本目。
 * count が 0 なら語を捨てる。語が UTF-16 として壊れていれば SEARCH_MALFORMED で前の語を保つ。
 * #40 の絞り込み語とは別の語で、同期しない。本文も選択もフォーカスも持たず、永続化もしない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_set_search_term(struct folio_state *_Nonnull state, const char16_t *_Nonnull units,
                            size_t count);
/* 覚えている語（UTF-8・終端付き）。無ければ空文字列。次の set まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_search_term(
    const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_search_term_length(const struct folio_state *_Nonnull state);
/* 直前の方向（`/` なら前方、`?` なら後方）。初期値は前方。前後ボタンは変えない。
 * n / N の N は逆向きに 1 回進むだけで、覚えている方向は変えない。 */
[[nodiscard]] enum search_direction
folio_state_search_direction(const struct folio_state *_Nonnull state);
void folio_state_set_search_direction(struct folio_state *_Nonnull state,
                                      enum search_direction direction);
/* いまの表示モード。 */
[[nodiscard]] enum pane_mode folio_state_pane_mode(const struct folio_state *_Nonnull state);
/* 選択中のノートの本文（UTF-8・終端付き）。何も選んでいなければ空文字列。次の意図まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_pane_text(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_pane_text_length(const struct folio_state *_Nonnull state);
/* 右ペインの頭の表示値。 */
[[nodiscard]] struct pane_title_view
folio_state_pane_title(const struct folio_state *_Nonnull state);
/* 右ペインに写す RTF（終端付き）。何も選んでいなければ空の文書。次の意図まで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_pane_rtf(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_pane_rtf_length(const struct folio_state *_Nonnull state);
/* 編集用の平文 RTF（#250・markdown_rtf_plain）を folio_state_pane_text と同じ本文（最後に読んだか
 * 保存に成功した本文・無題なら空）から作り、state が持つ 1 つの slot に置く。palette と face は
 * 閲覧と同じ。READY か OUT_OF_MEMORY で、確保に失敗すれば前の slot を保つ。ほかの状態は変えない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_pane_edit_rtf(struct folio_state *_Nonnull state);
/* slot の RTF（終端付き）。まだ作っていなければ空文字列と 0。次に slot を置き換えるまで有効。 */
[[nodiscard]] const char *_Nonnull folio_state_edit_rtf(const struct folio_state *_Nonnull state);
[[nodiscard]] size_t folio_state_edit_rtf_length(const struct folio_state *_Nonnull state);
/* 現在のノートの履歴の一覧を開く（FR-033 / ADR 0038 の決定 7）。前の一覧は先に捨てる。
 * port の read_history で 1〜note_history_depth を順に読み、無い版は飛ばし、読めない版
 * （UTF-8 でない・読めない）は本文の無い行として版番号順に持つ。md も履歴も書かない。
 * 未選択は NOTHING_SELECTED、無題は NAME_REQUIRED、1 版も無ければ HISTORY_EMPTY で、
 * どれも一覧を持たない。確保に失敗すれば OUT_OF_MEMORY で、途中まで読んだ版も捨てる。 */
[[nodiscard]] enum folio_state_outcome folio_state_open_history(struct folio_state *_Nonnull state);
/* 開いている一覧の行数。開いていなければ 0。 */
[[nodiscard]] size_t folio_state_history_count(const struct folio_state *_Nonnull state);
/* 一覧の index 行の表示値（決定 8）。範囲の外なら false で out は触らない。
 * 見出しは本文の最初の空でない論理行（note_text_first_line）で、時刻は持たない。 */
[[nodiscard]] bool folio_state_history_row(const struct folio_state *_Nonnull state, size_t index,
                                           struct history_row_view *_Nonnull out);
/* 一覧の index 行の本文。読めない版と範囲の外は nullptr。次の意図まで有効。 */
[[nodiscard]] const struct note_text *_Nullable folio_state_history_body(
    const struct folio_state *_Nonnull state, size_t index);
/* index 行の版へ戻す意図（決定 1 / 9）。読めない行と範囲の外は HISTORY_UNREADABLE で何も変えない。
 * 閲覧中なら folio_state_begin_edit と同じ遷移で編集モードへ入り、READY を返す。
 * 本文は folio_state_history_body で取り、UI が note_pane_replace で流し込む（未保存の変更）。
 * ファイルは書かず、一覧も捨てない（閉じるのは UI の close）。 */
[[nodiscard]] enum folio_state_outcome
folio_state_restore_history(struct folio_state *_Nonnull state, size_t index);
/* 一覧を捨てる（決定 10）。面を閉じるときに UI が呼ぶ。別の文書へ移る・保存する・破棄の経路でも
 * application が自分で呼ぶので、古い版を持ち越さない。 */
void folio_state_close_history(struct folio_state *_Nonnull state);
/* READY 以外の結果を利用者に見せる 1 行（UTF-8・終端付き・静的）。READY は空文字列。 */
[[nodiscard]] const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome,
                                                            enum folio_language language);
void folio_state_destroy(struct folio_state *_Nullable state);

/* メニューの既知の可否だけを読む。確保・同期・I/O・本文変更を行わない。
 * CURRENTではtargetを使わない。実行時は既存意図が改めて検証する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_command_availability(const struct folio_state *_Nonnull state,
                                 enum folio_command command, enum folio_command_target target_kind,
                                 struct note_ref target);
[[nodiscard]] enum folio_state_outcome
folio_state_recolor_availability(const struct folio_state *_Nonnull state, size_t category);
/* 有効な索引上の対象の名前。同期呼出しの間だけ借りる。 */
[[nodiscard]] const char *_Nonnull folio_state_note_name(const struct folio_state *_Nonnull state,
                                                         struct note_ref target);
[[nodiscard]] bool folio_state_note_favorite(const struct folio_state *_Nonnull state,
                                             struct note_ref ref);
[[nodiscard]] size_t folio_state_favorite_count(const struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_favorite(struct folio_state *_Nonnull state, struct note_ref ref);
/* #1069 / ADR0117: 台帳を書く意図の結果を、利用者へ伝える結果に直す唯一の場所。意図が、書く前に
 * 読み直した一覧をメモリへ採用していて、結果が READY なら LEDGER_REFRESHED。ほかの結果はそのまま。
 * 読むと知らせは消える（1回だけ伝える）。台帳を書く意図を呼んだ直後に、必ず1回通す。 */
[[nodiscard]] enum folio_state_outcome
folio_state_ledger_outcome(struct folio_state *_Nonnull state, enum folio_state_outcome outcome);
[[nodiscard]] enum note_scope folio_state_note_scope(const struct folio_state *_Nonnull state);
void folio_state_set_note_scope(struct folio_state *_Nonnull state, enum note_scope scope);
[[nodiscard]] enum note_order folio_state_note_order(const struct folio_state *_Nonnull state);
[[nodiscard]] enum folio_state_outcome
folio_state_set_note_order(struct folio_state *_Nonnull state, enum note_order order);
#endif
