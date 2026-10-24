/* 操作部品の地・枠・hover・押下・選択・focus・絵を描く唯一の経路（ADR0060 の決定 1〜3）。
 * 共通ヘッダー・本文操作帯・ドロワーの帯がこの 1 本を通り、状態の表現を窓ごとに持たない。 */
#ifndef NENEFOLIO_UI_PART_H
#define NENEFOLIO_UI_PART_H

#include "folio_palette.h"
#include "ui_part_ink.h"
#include "ui_part_menu_paint.h"
#include "ui_part_paint.h"
#include "ui_part_surface_region.h"

#include <windows.h>

/* 寸法の体系（96 DPI の論理 px・ADR0060 の決定 2）。角は直角。 */
constexpr int ui_part_height = 28;       /* 操作部品の高さ */
constexpr int ui_part_padding = 12;      /* 字の釦の左右の余白 */
constexpr int ui_part_icon = 16;         /* A 様式の絵 */
constexpr int ui_part_icon_gap = 6;      /* 絵と字の間 */
constexpr int ui_part_glyph = 20;        /* C 様式の絵 */
constexpr int ui_part_group_gap = 16;    /* 群と群の間 */
constexpr int ui_part_edge = 16;         /* 部品と窓・面の縁（余白の原則: 縁 16・枠の内側 8） */
constexpr int ui_part_field_padding = 8; /* 入力欄の枠の内側の字の左右の余白（#349） */
constexpr int ui_part_surface_title_height = 44; /* 面の題の段（ADR0060 決定22） */

/* boundsは呼出元座標のphysical px。題44・内容の縁16・閉じるの縁8/当たり28はdpiで拡大する。 */
[[nodiscard]] RECT ui_part_surface_rect(RECT bounds, UINT dpi, enum ui_part_surface_region region);
/* dialog_surfaceの地とdialog_borderの1 physical pxの境界。内容や字は各ownerが描く。 */
void ui_part_draw_surface(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                          RECT bounds);

/* 部品を描き、字の色と矩形を返す。字そのものは呼び出し側の文字の経路が描く。 */
struct ui_part_ink ui_part_draw(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                                const struct ui_part_paint *_Nonnull part);

/* 群の 1px の枠（装飾の面。状態は表さない）。 */
void ui_part_frame_group(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                         RECT bounds);

/* 入力欄の箱（高さ 28）に単一行 EDIT を置く矩形。枠は箱の内側に静止 1px / focus 2px で描くので、
 * EDIT は左右と上下を focus の太さだけ空け、縦は字の高さで中央に寄せる（単一行 EDIT は字を上端に
 * 描く）。字の左右の余白は ui_part_field_margins と合わせて箱の縁から 8 になり、focus で動かない。
 */
[[nodiscard]] RECT ui_part_field_edit(RECT box, int text_height, UINT dpi);

/* EDIT の字の左右の余白。箱の縁からの 8（DPI で拡大）から ui_part_field_edit が空けた focus の
 * 太さを引いた値。右に部品（×）を置く欄は right_room の方が広ければそれ。
 * placeholder は EM_GETRECT から描くので同じ位置に並ぶ。 */
void ui_part_field_margins(HWND _Nonnull edit, UINT dpi, int right_room);

/* 単一行 EDIT の字の高さ（設定済みの書体の tmHeight）。測れなければ 0。 */
[[nodiscard]] int ui_part_field_text_height(HWND _Nonnull edit);

/* 入力欄の箱の内側に枠（静止 1px・focus 2px）を描く。箱の地は呼び出し側が EDIT の地で塗る
 * （ui_part_fill_field）。 */
void ui_part_draw_field(HDC _Nonnull device, const struct folio_palette *_Nonnull palette, RECT box,
                        int focus);

/* 入力欄の箱を EDIT の地で塗る。EDIT の上下左右に空いた所が地と同じ色になる。 */
void ui_part_fill_field(HDC _Nonnull device, RECT box, COLORREF background);

/* focus 線の太さ（2px を DPI で拡大）。 */
[[nodiscard]] int ui_part_focus_width(UINT dpi);

/* 単独部品の外側の focus が矩形からはみ出す幅（間の 1px ＋太さ）。再描画の範囲に使う。 */
[[nodiscard]] int ui_part_focus_reach(UINT dpi);

/* A 様式の字の釦の幅（論理 px の 4 の倍数へ切り上げる）。字の幅は呼び出し側が自分の書体で測る。 */
[[nodiscard]] int ui_part_command_width(int text, bool icon, UINT dpi);

/* MENU一覧variant。字のinset16、選択の面inset4と左2pxの棒。 */
struct ui_part_ink ui_part_draw_menu(HDC _Nonnull device,
                                     const struct folio_palette *_Nonnull palette,
                                     const struct ui_part_menu_paint *_Nonnull part);
#endif
