#ifndef NENEFOLIO_NOTE_PANE_LINKS_H
#define NENEFOLIO_NOTE_PANE_LINKS_H
#include "note_pane_link_outcome.h"
#include "note_search_span.h"
#include <uchar.h>
#include <windows.h>

#include <richedit.h>
#include <tom.h>
struct note_pane_links;
struct utf8_text;
[[nodiscard]] enum note_pane_link_outcome
note_pane_links_create(ITextDocument *_Nonnull document,
                       struct note_pane_links *_Nullable *_Nonnull out);
void note_pane_links_destroy(struct note_pane_links *_Nullable links);
[[nodiscard]] size_t note_pane_links_visible(const struct note_pane_links *_Nullable links,
                                             size_t native);
[[nodiscard]] CHARRANGE note_pane_links_native(const struct note_pane_links *_Nullable links,
                                               struct note_search_span span);
[[nodiscard]] size_t note_pane_links_compact(const struct note_pane_links *_Nonnull links,
                                             char16_t *_Nonnull units, size_t length);
/* #975: 同じ詰め方を、native 位置 base から読んだ length 単位の断片に行う（units は length+1
 * 単位以上）。非表示の区間を除いて終端を置き、残った長さを返す。 */
[[nodiscard]] size_t note_pane_links_compact_from(const struct note_pane_links *_Nonnull links,
                                                  size_t base, char16_t *_Nonnull units,
                                                  size_t length);
[[nodiscard]] enum note_pane_link_outcome
note_pane_links_target(ITextDocument *_Nonnull document, CHARRANGE selection,
                       struct utf8_text *_Nullable *_Nonnull out);
void note_pane_links_input(struct note_pane_links *_Nullable links, UINT message, LPARAM position);
void note_pane_links_finish_input(struct note_pane_links *_Nullable links, UINT message);
[[nodiscard]] bool note_pane_links_clicked(struct note_pane_links *_Nonnull links,
                                           HWND _Nonnull window, const ENLINK *_Nonnull notice);
/* コードブロックのコピー操作の見える区間（文書順・ADR0098）。描画時の装飾が位置を引くための
 * 派生値で、nativeの文字位置だけを持つ。index は action_count 未満。 */
[[nodiscard]] size_t note_pane_links_action_count(const struct note_pane_links *_Nullable links);
[[nodiscard]] CHARRANGE note_pane_links_action(const struct note_pane_links *_Nonnull links,
                                               size_t index);
[[nodiscard]] size_t note_pane_links_action_slot(const struct note_pane_links *_Nonnull links,
                                                 size_t index);
/* #945: 脚注の番号の見える区間（native位置）。back が真なら脚注の側の番号、偽なら本文の側で
 * 文書順に最初のもの。その番号の区間が無ければ false。 */
[[nodiscard]] bool note_pane_links_footnote(const struct note_pane_links *_Nullable links,
                                            bool back, size_t number, CHARRANGE *_Nonnull out);
#endif
