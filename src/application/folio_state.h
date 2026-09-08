/* 文書の一覧とカテゴリ設定の唯一の所有者（ARC-004）。起動時にポートから台帳と走査結果を受け、
 * core の照合で 1 つの索引に固定する。外から見える状態は不変で、UI は派生値を受け取り、
 * 操作は意図（folio_state_toggle_category 等）として渡すだけ（ARC-011）。 */
#ifndef NENEFOLIO_FOLIO_STATE_H
#define NENEFOLIO_FOLIO_STATE_H

#include "drawer_metrics.h"
#include "folio_state_outcome.h"
#include "folio_theme.h"
#include "pane_title_view.h"

#include <stddef.h>

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
/* いまの索引と寸法からドロワーの配置を作り、選択中のノートに印を付ける。呼び出し側が破棄する。 */
[[nodiscard]] enum folio_state_outcome
folio_state_drawer_layout(const struct folio_state *_Nonnull state, struct drawer_metrics metrics,
                          struct drawer_layout *_Nullable *_Nonnull out);
/* 索引にあるノートの総数。 */
[[nodiscard]] size_t folio_state_note_count(const struct folio_state *_Nonnull state);
/* カテゴリの展開状態を反転し、台帳を書き戻す（FR-004 / FR-007）。書き戻せなければ状態は変えない。
 */
[[nodiscard]] enum folio_state_outcome
folio_state_toggle_category(struct folio_state *_Nonnull state, size_t index);
/* ノートを選び、本文を読んで右ペインの表示値を作る（FR-005）。読めなければ表示は変えない。 */
[[nodiscard]] enum folio_state_outcome folio_state_select_note(struct folio_state *_Nonnull state,
                                                               size_t category, size_t note);
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
