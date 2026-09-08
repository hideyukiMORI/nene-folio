/* 右ペイン（FR-005 の閲覧側）。Msftedit.dll の RICHEDIT50W を所有し、application が作った RTF を
 * EM_STREAMIN で写すだけで、判断を持たない（ARC-011 / ADR 0002）。 */
#ifndef NENEFOLIO_NOTE_PANE_H
#define NENEFOLIO_NOTE_PANE_H

#include "note_pane_outcome.h"

#include <stddef.h>
#include <windows.h>

struct note_pane;

/* background は地の色（テーマの正本 folio_palette から）。 */
[[nodiscard]] enum note_pane_outcome note_pane_create(HWND _Nonnull parent, COLORREF background,
                                                      struct note_pane *_Nullable *_Nonnull out);
/* 親が配置に使う。ウィンドウが既に破棄されていれば nullptr。 */
[[nodiscard]] HWND _Nullable note_pane_handle(const struct note_pane *_Nonnull pane);
/* RTF を流し込んで表示を置き換える。 */
void note_pane_render(struct note_pane *_Nonnull pane, const char *_Nonnull rtf, size_t length);
void note_pane_destroy(struct note_pane *_Nullable pane);

#endif
