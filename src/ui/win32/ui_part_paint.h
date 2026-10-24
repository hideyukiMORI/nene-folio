/* 操作部品 1 つを描くための表示値。矩形は描画・当たり判定・focus・UIA と同じ関数から取る。
 * 状態は同時に立ち得る（focus 中の選択項目の hover など）ので、閉じた enum
 * ではなく独立の真偽で持つ。 */
#ifndef NENEFOLIO_UI_PART_PAINT_H
#define NENEFOLIO_UI_PART_PAINT_H

#include "icon_paint_kind.h"
#include "ui_part_style.h"

#include <windows.h>

struct ui_part_paint
{
    RECT bounds;
    enum ui_part_style style;
    UINT dpi;
    bool grouped;      /* 群の中の項目。枠は群が持ち、focus は内側に描く */
    bool focus_inside; /* native子窓ではHDC内側にfocusを描く。既存単独部品は偽 */
    bool leading_rule; /* 群の 2 つ目以降の項目。左端に 1px の区切りを描く */
    bool labeled;      /* 字を描く。幅が足りずに絵へ縮めたときは偽 */
    bool has_icon;
    enum icon_paint_kind icon;
    bool hovered;
    bool pressed;
    bool selected;
    bool focused;
    bool disabled;
};

#endif
