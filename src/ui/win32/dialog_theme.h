/* 名前入力面と失敗の箱を palette で塗る唯一の経路（ADR 0035 の決定 2・ARC-001）。
 * comctl32 v5 のまま、WM_CTLCOLOR* のブラシ・owner-draw の釦と
 * コンボの項目・DWM の題の帯だけで塗る。palette は作る瞬間に写し、
 * 開いている面は追随しない（決定 5）。ブラシの所有者はこの型 1 つで、
 * DialogBoxIndirectParamW が返った直後に destroy で捨てる（C-016 / C-017）。 */
#ifndef NENEFOLIO_DIALOG_THEME_H
#define NENEFOLIO_DIALOG_THEME_H

#include "dialog_theme_button.h"
#include "dialog_theme_outcome.h"
#include "dialog_theme_surface.h"

#include <windows.h>

struct dialog_theme;
struct folio_palette;

/* READY 以外（確保・ブラシの作成の失敗）なら *out は nullptr のまま。 */
[[nodiscard]] enum dialog_theme_outcome
dialog_theme_create(const struct folio_palette *_Nonnull palette,
                    struct dialog_theme *_Nullable *_Nonnull out);
void dialog_theme_destroy(struct dialog_theme *_Nullable theme);
/* 役割の字と地の色を device に当て、面が WM_CTLCOLOR* に返すブラシを答える。 */
[[nodiscard]] HBRUSH _Nonnull dialog_theme_color(const struct dialog_theme *_Nonnull theme,
                                                 enum dialog_theme_surface surface,
                                                 HDC _Nonnull device);
/* BS_OWNERDRAW の押し釦を右ペインの頭の札と同じ形で描く。字は釦の窓の文字列から取る。 */
void dialog_theme_draw_button(const struct dialog_theme *_Nonnull theme,
                              const DRAWITEMSTRUCT *_Nonnull item, enum dialog_theme_button kind);
/* CBS_OWNERDRAWFIXED のコンボの項目（閉じた面とリストの行）を描く。 */
void dialog_theme_draw_item(const struct dialog_theme *_Nonnull theme,
                            const DRAWITEMSTRUCT *_Nonnull item, const wchar_t *_Nonnull text);
/* 名前欄などの外側に札の地の色の 1px の枠を描く（面が WS_BORDER の代わりに描く枠）。 */
void dialog_theme_frame(const struct dialog_theme *_Nonnull theme, HDC _Nonnull dc,
                        const RECT *_Nonnull rect);
/* 題の帯を DWM で palette の地と字の色にする。 */
void dialog_theme_decorate(const struct dialog_theme *_Nonnull theme, HWND _Nonnull dialog);

#endif
