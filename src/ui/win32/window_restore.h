/* #891 / ADR0108: the Win32 side of the remembered window place. The decision whether a
 * recorded place may be used is core's window_fit; this gathers the screens' work areas, applies
 * the result to the window and reads the place back at close. */
#ifndef NENEFOLIO_WINDOW_RESTORE_H
#define NENEFOLIO_WINDOW_RESTORE_H
#include "window_fit_request.h"
#include <windows.h>
/* Fills the request's screens and asks window_fit. True with the rectangle to create the window
 * at; false for the initial place. The request's lengths and `saved` come from the caller. */
[[nodiscard]] bool window_restore_place(struct window_fit_request request,
                                        struct window_area *_Nonnull out);
/* After the window exists at `place`: if the screen it landed on has another DPI than the place
 * was recorded with, the size follows the DPI (never larger than that screen's work area). */
void window_restore_settle(HWND _Nonnull window, const struct window_placement *_Nonnull saved);
/* The window's normal (not maximized, not minimized) rectangle, its DPI and whether it is
 * maximized now. */
[[nodiscard]] struct window_placement window_restore_capture(HWND _Nonnull window);
#endif
