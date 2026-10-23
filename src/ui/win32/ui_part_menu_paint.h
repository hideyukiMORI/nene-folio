#ifndef NENEFOLIO_UI_PART_MENU_PAINT_H
#define NENEFOLIO_UI_PART_MENU_PAINT_H
#include <windows.h>
struct ui_part_menu_paint
{
    RECT bounds;
    UINT dpi;
    bool selected;
    bool enabled;
};
#endif
