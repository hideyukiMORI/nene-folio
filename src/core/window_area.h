#ifndef NENEFOLIO_WINDOW_AREA_H
#define NENEFOLIO_WINDOW_AREA_H
/* #891: a rectangle on the desktop in physical pixels with the DPI that applies there: one
 * screen's work area (without the taskbar), or the place a window is to be restored to. */
struct window_area
{
    int x;
    int y;
    int width;
    int height;
    unsigned dpi;
};
#endif
