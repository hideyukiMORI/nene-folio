/* 利用者が設定で選ぶテーマ（ADR 0031 の決定 2）。閉じた集合（C-002）。
 * 描画に渡すのは解決した enum folio_theme で、SYSTEM はここにしか現れない。
 * 解決は core の純関数 folio_theme_resolve（folio_theme.h）1 か所だけが行う。 */
#ifndef NENEFOLIO_FOLIO_THEME_CHOICE_H
#define NENEFOLIO_FOLIO_THEME_CHOICE_H

enum folio_theme_choice : unsigned char
{
    FOLIO_THEME_CHOICE_SYSTEM, /* OS の「アプリのモード」に従う（既定） */
    FOLIO_THEME_CHOICE_LIGHT,
    FOLIO_THEME_CHOICE_DARK
};

#endif
