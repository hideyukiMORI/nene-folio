/* 対話面の押し釦の描き分け（ADR 0035 の決定 2）。owner-draw の釦には ODS_DEFAULT が
 * 立たないので、既定かどうかは面が渡す。 */
#ifndef NENEFOLIO_DIALOG_THEME_BUTTON_H
#define NENEFOLIO_DIALOG_THEME_BUTTON_H

enum dialog_theme_button : unsigned char
{
    DIALOG_THEME_BUTTON_PRIMARY,  /* 既定の釦: 札の地に札の字 */
    DIALOG_THEME_BUTTON_SECONDARY /* 他の釦: 札の色の 1px の枠に字 */
};

#endif
