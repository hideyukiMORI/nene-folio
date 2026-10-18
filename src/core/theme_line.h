/* 状態・境界・focusの線の役割（ADR0060の決定3）。線は文字系トークンで引き、
 * 役割ごとに接する地が決まる。値を足したら theme_contrast.c の閉じた switch に枝を足す（C-002）。
 */
#ifndef NENEFOLIO_THEME_LINE_H
#define NENEFOLIO_THEME_LINE_H

enum theme_line : unsigned char
{
    THEME_LINE_HOVER,      /* 部品の内側1pxのhover枠。地は window / pane */
    THEME_LINE_FOCUS,      /* 単独部品の1px外側の2px focus。地は window / pane / dialog_surface */
    THEME_LINE_FOCUS_ITEM, /* 群の非選択項目の内側の2px focus。地は window / pane */
    THEME_LINE_FOCUS_SELECTED, /* 群の選択中・押下中の項目の内側のfocus。地は selected_background */
    THEME_LINE_FOCUS_PRIMARY,  /* B主操作の内側のfocus。地は chip_background */
    THEME_LINE_FIELD, /* 入力欄の静止枠。地は window / pane / dialog_surface / dialog_control */
    THEME_LINE_FIELD_FOCUS /* 入力欄のfocus枠。地はFIELDと同じ */
};

#endif
