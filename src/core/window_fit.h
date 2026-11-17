#ifndef NENEFOLIO_WINDOW_FIT_H
#define NENEFOLIO_WINDOW_FIT_H
#include "window_fit_outcome.h"
#include "window_fit_request.h"
/* #891 / ADR0108: the one place that decides whether a recorded place may be used. RESTORE
 * writes the rectangle to open at (its size adjusted to the screen's DPI); DEFAULT when the
 * user chose the initial place, nothing was recorded, the header band cannot be grabbed on any
 * screen, or the size is below the minimum or larger than that screen's work area. */
[[nodiscard]] enum window_fit_outcome window_fit(const struct window_fit_request *_Nonnull request,
                                                 struct window_area *_Nonnull out);
#endif
