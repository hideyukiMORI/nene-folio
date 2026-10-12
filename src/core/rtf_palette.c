#include "rtf_palette.h"

#include "theme_palette.h"

struct rtf_palette rtf_palette_for(enum folio_theme theme)
{
    return folio_theme_palette(theme)->rtf;
}
