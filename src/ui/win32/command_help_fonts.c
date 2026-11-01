#include "command_help_fonts.h"
#include "ui_face.h"
#include <wchar.h>

static HFONT _Nullable create_font(const wchar_t *_Nonnull face, int size, int weight, UINT dpi)
{
    LOGFONTW font = {.lfHeight = -MulDiv(size, (int)dpi, 96),
                     .lfWeight = weight,
                     .lfCharSet = DEFAULT_CHARSET,
                     .lfQuality = CLEARTYPE_QUALITY};
    (void)wcscpy_s(font.lfFaceName, LF_FACESIZE, face);
    return CreateFontIndirectW(&font);
}

struct command_help_fonts command_help_fonts_create(enum folio_language language, UINT dpi)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(language, face);
    return (struct command_help_fonts){.body = create_font(face, 14, FW_NORMAL, dpi),
                                       .key = create_font(L"Consolas", 13, FW_NORMAL, dpi),
                                       .heading = create_font(face, 14, FW_SEMIBOLD, dpi)};
}

void command_help_fonts_destroy(struct command_help_fonts *_Nonnull fonts)
{
    if (fonts->body != nullptr)
        DeleteObject(fonts->body);
    if (fonts->key != nullptr)
        DeleteObject(fonts->key);
    if (fonts->heading != nullptr)
        DeleteObject(fonts->heading);
    *fonts = (struct command_help_fonts){0};
}
