#ifndef NENEFOLIO_NOTE_PANE_GEOMETRY_H
#define NENEFOLIO_NOTE_PANE_GEOMETRY_H
#include <tom.h>
#include <windows.h>

/* One native observation in document pixels; never an owner of scroll state. */
struct note_pane_geometry
{
    int height;
    int width;
    int first;
    int visible;
    int step;
};

[[nodiscard]] bool note_pane_geometry_read(HWND _Nonnull window, ITextRange *_Nonnull range,
                                           struct note_pane_geometry *_Nonnull out);
[[nodiscard]] bool note_pane_geometry_bounds(HWND _Nonnull window, ITextRange *_Nonnull range,
                                             RECT *_Nonnull out);
[[nodiscard]] int note_pane_geometry_line(HWND _Nonnull window);
[[nodiscard]] int note_pane_geometry_limit(struct note_pane_geometry geometry);
#endif
