/* 操作部品の地・枠・hover・押下・選択・focus・絵を描く唯一の経路（ADR0060 の決定 1〜3）。
 * 共通ヘッダー・本文操作帯・ドロワーの帯がこの 1 本を通り、状態の表現を窓ごとに持たない。 */
#ifndef NENEFOLIO_UI_PART_H
#define NENEFOLIO_UI_PART_H

#include "folio_palette.h"
#include "ui_part_ink.h"
#include "ui_part_paint.h"

#include <windows.h>

/* 寸法の体系（96 DPI の論理 px・ADR0060 の決定 2）。角は直角。 */
constexpr int ui_part_height = 28;        /* 操作部品の高さ */
constexpr int ui_part_padding = 12;       /* 字の釦の左右の余白 */
constexpr int ui_part_icon = 16;          /* A 様式の絵 */
constexpr int ui_part_icon_gap = 6;       /* 絵と字の間 */
constexpr int ui_part_glyph = 20;         /* C 様式の絵 */
constexpr int ui_part_group_gap = 16;     /* 群と群の間 */
constexpr int ui_part_edge = 12;          /* 部品と面の端 */
constexpr int ui_part_choice_padding = 4; /* 選択群の枠と項目の間 */

/* 部品を描き、字の色と矩形を返す。字そのものは呼び出し側の文字の経路が描く。 */
struct ui_part_ink ui_part_draw(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                                const struct ui_part_paint *_Nonnull part);

/* 群の 1px の枠（装飾の面。状態は表さない）。 */
void ui_part_frame_group(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                         RECT bounds);

/* 入力欄の外側の枠。focus に太さを渡せば focus 線、0 なら 1px の静止枠。 */
void ui_part_frame_field(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                         RECT field, int focus);

/* focus 線の太さ（2px を DPI で拡大）。 */
[[nodiscard]] int ui_part_focus_width(UINT dpi);

/* 単独部品の外側の focus が矩形からはみ出す幅（間の 1px ＋太さ）。再描画の範囲に使う。 */
[[nodiscard]] int ui_part_focus_reach(UINT dpi);

/* A 様式の字の釦の幅。字の幅は呼び出し側が自分の書体で測る。 */
[[nodiscard]] int ui_part_command_width(int text, bool icon, UINT dpi);

#endif
