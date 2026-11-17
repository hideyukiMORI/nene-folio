#include "window_restore.h"
#include "window_fit.h"

constexpr size_t screen_limit = 16;

/* The work areas found so far. A screen's own DPI needs a library the UI does not link, so each
 * area carries the DPI the place was recorded with: the size is checked as recorded, and
 * window_restore_settle corrects it once the window reports its real DPI. */
struct window_restore
{
    struct window_area areas[screen_limit];
    size_t count;
    unsigned dpi;
};

static BOOL CALLBACK add_screen(HMONITOR monitor, HDC device, LPRECT bounds, LPARAM parameter)
{
    (void)device;
    (void)bounds;
    struct window_restore *screens = (struct window_restore *)parameter;
    MONITORINFO info = {.cbSize = sizeof info};
    if (screens->count == screen_limit || !GetMonitorInfoW(monitor, &info))
        return TRUE;
    screens->areas[screens->count++] =
        (struct window_area){.x = info.rcWork.left,
                             .y = info.rcWork.top,
                             .width = info.rcWork.right - info.rcWork.left,
                             .height = info.rcWork.bottom - info.rcWork.top,
                             .dpi = screens->dpi};
    return TRUE;
}

bool window_restore_place(struct window_fit_request request, struct window_area *_Nonnull out)
{
    struct window_restore screens = {.dpi = request.saved->dpi};
    EnumDisplayMonitors(nullptr, nullptr, add_screen, (LPARAM)&screens);
    request.areas = screens.areas;
    request.count = screens.count;
    return window_fit_resolve(&request, out) == WINDOW_FIT_RESTORE;
}

void window_restore_settle(HWND _Nonnull window, const struct window_placement *_Nonnull saved)
{
    UINT dpi = GetDpiForWindow(window);
    MONITORINFO info = {.cbSize = sizeof info};
    if (dpi == 0 || dpi == saved->dpi ||
        !GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info))
        return;
    int width = MulDiv(saved->width, (int)dpi, (int)saved->dpi);
    int height = MulDiv(saved->height, (int)dpi, (int)saved->dpi);
    int room_width = info.rcWork.right - info.rcWork.left;
    int room_height = info.rcWork.bottom - info.rcWork.top;
    SetWindowPos(window, nullptr, 0, 0, width < room_width ? width : room_width,
                 height < room_height ? height : room_height,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
}

struct window_placement window_restore_capture(HWND _Nonnull window)
{
    struct window_placement captured = window_placement_default();
    WINDOWPLACEMENT native = {.length = sizeof native};
    UINT dpi = GetDpiForWindow(window);
    if (!GetWindowPlacement(window, &native) || dpi == 0)
        return captured;
    /* rcNormalPosition is in workspace coordinates: offset by the work area's origin on the
     * window's screen to get screen coordinates (they differ when a taskbar is at the top or
     * the left). */
    MONITORINFO info = {.cbSize = sizeof info};
    POINT offset = {0, 0};
    if (GetMonitorInfoW(MonitorFromRect(&native.rcNormalPosition, MONITOR_DEFAULTTONEAREST), &info))
        offset =
            (POINT){info.rcWork.left - info.rcMonitor.left, info.rcWork.top - info.rcMonitor.top};
    captured.placed = true;
    captured.x = native.rcNormalPosition.left + offset.x;
    captured.y = native.rcNormalPosition.top + offset.y;
    captured.width = native.rcNormalPosition.right - native.rcNormalPosition.left;
    captured.height = native.rcNormalPosition.bottom - native.rcNormalPosition.top;
    captured.dpi = dpi;
    captured.maximized =
        native.showCmd == SW_SHOWMAXIMIZED ||
        (native.showCmd == SW_SHOWMINIMIZED && (native.flags & WPF_RESTORETOMAXIMIZED) != 0);
    return captured;
}
