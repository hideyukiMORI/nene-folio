/* 表示のテーマ。閉じた集合（C-002）。OS の「アプリのモード」に従う（FR-016）。 */
#ifndef NENEFOLIO_FOLIO_THEME_H
#define NENEFOLIO_FOLIO_THEME_H

#include "folio_theme_choice.h"

enum folio_theme : unsigned char
{
    FOLIO_THEME_LIGHT,
    FOLIO_THEME_DARK
};

/* 選択と OS の値から描画に使うテーマを決める純関数（ADR 0031 の決定 2）。
 * SYSTEM だけが os_theme を通し、LIGHT / DARK は os_theme を見ない。
 * 実装は folio_theme.c（型定義 0・関数 1）にある（CNF-002 の接頭辞）。 */
[[nodiscard]] enum folio_theme folio_theme_resolve(enum folio_theme_choice choice,
                                                   enum folio_theme os_theme);

#endif
