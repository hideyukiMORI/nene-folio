/* #975 / ADR0114: 閲覧表示の見出しの索引。描画が段落へ付けた見出しのスタイル（RichEdit の組み込みの
 * Heading 1〜6）を段落ごとに読み、段落の先頭の native
 * 位置と階層を文書順に持つ。文書に文字を足さない。 文書を読み込み直したら作り直す（native
 * 位置は読み込みごとに変わる）。 */
#ifndef NENEFOLIO_NOTE_PANE_HEADINGS_H
#define NENEFOLIO_NOTE_PANE_HEADINGS_H
#include "note_pane_heading.h"
#include "note_pane_link_outcome.h"
#include <windows.h>

#include <richedit.h>
#include <tom.h>
struct note_pane_headings;
[[nodiscard]] enum note_pane_link_outcome
note_pane_headings_create(ITextDocument *_Nonnull document,
                          struct note_pane_headings *_Nullable *_Nonnull out);
void note_pane_headings_destroy(struct note_pane_headings *_Nullable headings);
[[nodiscard]] size_t note_pane_headings_count(const struct note_pane_headings *_Nullable headings);
/* index は count 未満。 */
[[nodiscard]] struct note_pane_heading
note_pane_headings_at(const struct note_pane_headings *_Nonnull headings, size_t index);
#endif
