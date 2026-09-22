#include "folio_theme.h"

/* 選択と OS の値を 1 つのテーマへ解決する唯一の場所（ARC-001 / ADR 0031 の決定 2）。
 * 閉じた選択に default を書かない（C-002）。選択肢が増えたらここが落ちる。 */
enum folio_theme folio_theme_resolve(enum folio_theme_choice choice, enum folio_theme os_theme)
{
    switch (choice)
    {
    case FOLIO_THEME_CHOICE_SYSTEM:
        return os_theme;
    case FOLIO_THEME_CHOICE_LIGHT:
        return FOLIO_THEME_LIGHT;
    case FOLIO_THEME_CHOICE_DARK:
        return FOLIO_THEME_DARK;
    }
    return os_theme;
}
