/* 対話面の子が塗られる役割（ADR 0035 の決定 2）。面が WM_CTLCOLOR* の相手の HWND を
 * 自分の子と比べて決め、dialog_theme_color に渡す。値を足したら塗りの閉じた switch に
 * 枝を足させる（C-002）。 */
#ifndef NENEFOLIO_DIALOG_THEME_SURFACE_H
#define NENEFOLIO_DIALOG_THEME_SURFACE_H

enum dialog_theme_surface : unsigned char
{
    DIALOG_THEME_DIALOG, /* 面の地 */
    DIALOG_THEME_LABEL,  /* STATIC の字 */
    DIALOG_THEME_FIELD,  /* EDIT の中とコンボの閉じた面（無効なコンボの STATIC も） */
    DIALOG_THEME_LIST    /* コンボのドロップダウンのリスト */
};

#endif
