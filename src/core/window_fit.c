#include "window_fit.h"
#include <stdint.h>

static int scaled(int64_t length, unsigned dpi, unsigned base)
{
    return (int)(length * dpi / base);
}

/* The header band is inside the area from top to bottom, and enough of its width shows. */
static bool grabbable(const struct window_fit_request *_Nonnull request, struct window_area area,
                      int width)
{
    const struct window_placement *saved = request->saved;
    int64_t band = scaled(request->header_height, area.dpi, 96);
    int64_t top = saved->y;
    if (top < area.y || top + band > (int64_t)area.y + area.height)
        return false;
    int64_t left = saved->x > area.x ? saved->x : area.x;
    int64_t window_right = (int64_t)saved->x + width;
    int64_t area_right = (int64_t)area.x + area.width;
    int64_t right = window_right < area_right ? window_right : area_right;
    return right - left >= scaled(request->grip_width, area.dpi, 96);
}

static bool sized(const struct window_fit_request *_Nonnull request, struct window_area area,
                  struct window_area window)
{
    return window.width >= scaled(request->minimum_width, area.dpi, 96) &&
           window.height >= scaled(request->minimum_height, area.dpi, 96) &&
           window.width <= area.width && window.height <= area.height;
}

static bool usable(struct window_area area)
{
    return area.dpi != 0 && area.width > 0 && area.height > 0;
}

enum window_fit_outcome window_fit(const struct window_fit_request *_Nonnull request,
                                   struct window_area *_Nonnull out)
{
    const struct window_placement *saved = request->saved;
    if (!saved->restore || !saved->placed || saved->width <= 0 || saved->height <= 0 ||
        saved->dpi == 0)
        return WINDOW_FIT_DEFAULT;
    for (size_t index = 0; index < request->count; ++index)
    {
        struct window_area area = request->areas[index];
        if (!usable(area))
            continue;
        struct window_area window = {.x = saved->x,
                                     .y = saved->y,
                                     .width = scaled(saved->width, area.dpi, saved->dpi),
                                     .height = scaled(saved->height, area.dpi, saved->dpi),
                                     .dpi = area.dpi};
        if (!grabbable(request, area, window.width))
            continue;
        /* The first screen that holds the header decides: a size that does not fit it is not
         * tried against another screen. */
        if (!sized(request, area, window))
            return WINDOW_FIT_DEFAULT;
        *out = window;
        return WINDOW_FIT_RESTORE;
    }
    return WINDOW_FIT_DEFAULT;
}
