/* 設定画面の 1 行の種別（ADR 0032 の決定 7）。行を足すたびに採用の閉じた switch が
 * 「その行で Enter を押したら何が起きるか」を決めさせる（C-002）。
 * HEADING はカーソルが止まらない段の見出しで、採用も印も無い。 */
#ifndef NENEFOLIO_SETTINGS_ROW_KIND_H
#define NENEFOLIO_SETTINGS_ROW_KIND_H

enum settings_row_kind : unsigned char
{
    SETTINGS_ROW_HEADING,
    SETTINGS_ROW_THEME,
    SETTINGS_ROW_LANGUAGE
};

#endif
