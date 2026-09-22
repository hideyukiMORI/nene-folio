/* dialog_theme の生成結果（C-005）。 */
#ifndef NENEFOLIO_DIALOG_THEME_OUTCOME_H
#define NENEFOLIO_DIALOG_THEME_OUTCOME_H

enum dialog_theme_outcome : unsigned char
{
    DIALOG_THEME_READY,
    DIALOG_THEME_NO_MEMORY, /* 型の確保が失敗した */
    DIALOG_THEME_NO_BRUSH   /* CreateSolidBrush が失敗した */
};

#endif
