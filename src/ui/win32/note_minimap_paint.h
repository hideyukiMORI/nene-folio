#ifndef NENEFOLIO_NOTE_MINIMAP_PAINT_H
#define NENEFOLIO_NOTE_MINIMAP_PAINT_H
#include "note_minimap_view.h"
#include <windows.h>
struct note_minimap_paint
{
    RECT bounds;
    UINT dpi;
    struct note_minimap_view view;
    bool focused;
    bool active;
};
#endif
