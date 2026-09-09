/* 右ペイン（FR-005 の閲覧側と FR-006 の編集側）。Msftedit.dll の RICHEDIT50W を所有し、
 * application が作った RTF を EM_STREAMIN で写し、編集中の本文を EM_STREAMOUT で渡すだけで、
 * 判断を持たない（ARC-011 / ADR 0002 / ADR 0006）。 */
#ifndef NENEFOLIO_NOTE_PANE_H
#define NENEFOLIO_NOTE_PANE_H

#include "note_pane_outcome.h"
#include "note_pane_text_outcome.h"

#include <stddef.h>
#include <uchar.h>
#include <windows.h>

struct note_pane;

/* background は地の色、text は編集モードの本文色（テーマの正本 folio_palette から）。 */
[[nodiscard]] enum note_pane_outcome note_pane_create(HWND _Nonnull parent, COLORREF background,
                                                      COLORREF text,
                                                      struct note_pane *_Nullable *_Nonnull out);
/* 親が配置に使う。ウィンドウが既に破棄されていれば nullptr。 */
[[nodiscard]] HWND _Nullable note_pane_handle(const struct note_pane *_Nonnull pane);
/* RTF を流し込んで表示を置き換え、読み取り専用に戻す（閲覧）。 */
void note_pane_render(struct note_pane *_Nonnull pane, const char *_Nonnull rtf, size_t length);
/* 本文を平文で流し込み、入力を受け付ける（編集）。 */
void note_pane_edit(struct note_pane *_Nonnull pane, const char16_t *_Nonnull units, size_t count);
/* 編集中の本文を UTF-16 で取り出す。pane が所有し、次の note_pane の呼び出しまで有効。 */
[[nodiscard]] enum note_pane_text_outcome note_pane_text(struct note_pane *_Nonnull pane,
                                                         const char16_t *_Nonnull *_Nonnull units,
                                                         size_t *_Nonnull count);
void note_pane_destroy(struct note_pane *_Nullable pane);

#endif
