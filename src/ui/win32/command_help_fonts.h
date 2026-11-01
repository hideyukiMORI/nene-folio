#ifndef NENEFOLIO_COMMAND_HELP_FONTS_H
#define NENEFOLIO_COMMAND_HELP_FONTS_H

#include "folio_language.h"
#include <windows.h>

struct command_help_fonts
{
    HFONT _Nullable body;
    HFONT _Nullable key;
    HFONT _Nullable heading;
};

[[nodiscard]] struct command_help_fonts command_help_fonts_create(enum folio_language language,
                                                                  UINT dpi);
void command_help_fonts_destroy(struct command_help_fonts *_Nonnull fonts);

#endif
