/* #891 / ADR0108: the main window's remembered place, kept per machine in
 * `nenefolio-window.json` (version 1, fixed key order):
 *   {"version":1,"restore":<bool>,"placed":<bool>,"x":<n>,"y":<n>,"width":<n>,"height":<n>,
 *    "dpi":<n>,"maximized":<bool>}
 * `x` / `y` are screen coordinates plus window_placement_bias, because screens left of or above
 * the primary one have negative coordinates and the JSON codec carries non-negative integers. */
#ifndef NENEFOLIO_WINDOW_PLACEMENT_H
#define NENEFOLIO_WINDOW_PLACEMENT_H
#include "window_placement_outcome.h"
#include <stddef.h>
struct json_writer;
constexpr int window_placement_bias = 1000000;
/* A plain value: the user's choice (`restore`), whether a place was recorded (`placed`), the
 * normal (not maximized) rectangle in physical pixels, the window's DPI when it was recorded,
 * and whether it was maximized. */
struct window_placement
{
    bool restore;
    bool placed;
    int x;
    int y;
    int width;
    int height;
    unsigned dpi;
    bool maximized;
};
/* What a missing file means: carry the place over, nothing recorded yet. */
[[nodiscard]] struct window_placement window_placement_default(void);
[[nodiscard]] enum window_placement_outcome
window_placement_parse(const char *_Nonnull text, size_t length,
                       struct window_placement *_Nonnull out);
void window_placement_write(const struct window_placement *_Nonnull placement,
                            struct json_writer *_Nonnull writer);
#endif
