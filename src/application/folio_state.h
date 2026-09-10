/* 文書の一覧とカテゴリ設定の唯一の所有者（ARC-004）。起動時にポートから台帳と走査結果を受け、
 * core の照合で 1 つの索引に固定する。外から見える状態は不変で、UI は派生値を受け取り、
 * 操作は意図（folio_state_toggle_category 等）として渡すだけ（ARC-011）。 */
#ifndef NENEFOLIO_FOLIO_STATE_H
#define NENEFOLIO_FOLIO_STATE_H

#include "drawer_metrics.h"
#include "folio_state_outcome.h"
#include "folio_theme.h"
#include "note_ref.h"
#include "pane_mode.h"
#include "pane_title_view.h"
#include "rgb_color.h"

#include <stddef.h>
#include <uchar.h>

struct appearance_port;
struct drawer_layout;
struct folio_state;
struct persistence_port;

/* ポートは複製して持つ。両ポートの adapter は state より長く生きていなければならない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_create(const struct persistence_port *_Nonnull persistence,
                   const struct appearance_port *_Nonnull appearance,
                   struct folio_state *_Nullable *_Nonnull out);
/* 起動時に読んだテーマ。 */
[[nodiscard]] enum folio_theme folio_state_theme(const struct folio_state *_Nonnull state);
/* いまの索引と寸法からドロワーの配置を作り、選択中のノートといまのスクロール量に印を付ける。
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
/* カテゴリの展開状態を反転し、台帳を書き戻す（FR-004 / FR-007）。書き戻せなければ状態は変えない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_category(struct folio_state *_Nonnull state, size_t index);
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
/* ノートを from から to へ動かす（FR-008 / FR-009）。同じカテゴリなら表示順を変えて index.json を
 * 書き戻し（from == to は書かずに READY・書き戻せなければ状態は変えない）、別のカテゴリなら md を
 * 移してから移動先・移動元の index.json を書き戻す（ADR 0008 の決定 3）。to.note は移動先の
 * ノート数と等しければ末尾。移動先に同じ名前があれば NAME_TAKEN で何もしない。 */
[[nodiscard]] enum folio_state_outcome
folio_state_move_note(struct folio_state *_Nonnull state, struct note_ref from, struct note_ref to);
/* ノートを選び、本文を読んで右ペインの表示値を作る（FR-005）。読めなければ表示は変えない。 */
[[nodiscard]] enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                               size_t category, size_t note);
/* 編集モードへ入る（FR-006）。ノートを選んでいなければ NOTHING_SELECTED。 */
[[nodiscard]] enum folio_state_outcome folio_state_begin_edit(struct folio_state *_Nonnull state);
/* 編集中の本文（UTF-16 の単位列）を保存し、編集モードのまま残る（Ctrl+S）。
 * 読んだ本文と同じなら書かない。書き戻せなければ状態は変えない（ADR 0006）。 */
[[nodiscard]] enum folio_state_outcome folio_state_store_note(struct folio_state *_Nonnull state,
                                                              const char16_t *_Nonnull units,
                                                              size_t count);
/* 編集中の本文を保存して閲覧へ戻る。保存できなければ編集モードのまま。 */
[[nodiscard]] enum folio_state_outcome folio_state_end_edit(struct folio_state *_Nonnull state,
                                                            const char16_t *_Nonnull units,
                                                            size_t count);
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
/* READY 以外の結果を利用者に見せる 1 行（UTF-8・終端付き・静的）。READY は空文字列。 */
[[nodiscard]] const char *_Nonnull folio_state_failure_line(enum folio_state_outcome outcome);
void folio_state_destroy(struct folio_state *_Nullable state);

#endif
