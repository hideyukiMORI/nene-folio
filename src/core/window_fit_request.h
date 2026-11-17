#ifndef NENEFOLIO_WINDOW_FIT_REQUEST_H
#define NENEFOLIO_WINDOW_FIT_REQUEST_H
#include "window_area.h"
#include "window_placement.h"
#include <stddef.h>
/* #891 / ADR0108: what the check needs. The lengths are at 96 DPI and are scaled to each
 * screen: the window's minimum size, the height of the header band a user drags the window by,
 * and how much of that band's width must be on a screen for it to be grabbed. */
struct window_fit_request
{
    const struct window_placement *_Nonnull saved;
    const struct window_area *_Nonnull areas; /* every screen's work area */
    size_t count;
    int minimum_width;
    int minimum_height;
    int header_height;
    int grip_width;
};
#endif
