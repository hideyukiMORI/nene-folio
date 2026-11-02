#ifndef NENEFOLIO_NOTE_MINIMAP_PAINT_H
#define NENEFOLIO_NOTE_MINIMAP_PAINT_H
#include "note_pane_geometry.h"
#include <windows.h>
struct note_minimap_paint
{
    RECT bounds;
    UINT dpi;
    struct note_pane_geometry view;
    bool focused;
    bool active;
};
#endif
