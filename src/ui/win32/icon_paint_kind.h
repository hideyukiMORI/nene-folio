/* ui が自前で描く絵の種別（ADR 0033 の決定 3）。24 の viewBox の面のパスとして持ち、
 * 値を足したら塗り方の閉じた switch に枝を足させる（C-002）。 */
#ifndef NENEFOLIO_ICON_PAINT_KIND_H
#define NENEFOLIO_ICON_PAINT_KIND_H

enum icon_paint_kind : unsigned char
{
    ICON_PAINT_CLOSE,         /* 頭の右端の閉じる × */
    ICON_PAINT_SETTINGS,      /* その左隣の歯車 */
    ICON_PAINT_FOLD_COLLAPSE, /* 展開中のカテゴリ行の − */
    ICON_PAINT_FOLD_EXPAND,   /* 折り畳み中のカテゴリ行の + */
    ICON_PAINT_SELECTION      /* 設定画面のいまの値の行に付く印 */
};

#endif
