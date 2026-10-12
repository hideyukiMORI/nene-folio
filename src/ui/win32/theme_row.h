/* One transient row in the filtered theme picker (ADR0049). */
#ifndef NENEFOLIO_THEME_ROW_H
#define NENEFOLIO_THEME_ROW_H

#include "folio_theme_choice.h"

#include <windows.h>

struct theme_row
{
    RECT bounds;
    enum folio_theme_choice choice;
    bool focused;
};

#endif
