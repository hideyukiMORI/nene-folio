#include "note_pane_geometry.h"
#include <limits.h>
#include <richedit.h>
#include <string.h>

int note_pane_geometry_limit(struct note_pane_geometry geometry)
{
    return geometry.height > geometry.visible ? geometry.height - geometry.visible : 0;
}

bool note_pane_geometry_bounds(HWND window, ITextRange *_Nonnull range, RECT *_Nonnull out)
{
    RECT bounds = {0};
    LONG flags = tomStart | tomClientCoord | tomAllowOffClient;
    if (range->lpVtbl->GetPoint(range, flags | TA_TOP | TA_LEFT, &bounds.left, &bounds.top) !=
            S_OK ||
        range->lpVtbl->GetPoint(range, flags | TA_BOTTOM | TA_RIGHT, &bounds.right,
                                &bounds.bottom) != S_OK)
        return false;
    POINT scroll = {0};
    SendMessageW(window, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    LONGLONG top = (LONGLONG)bounds.top + scroll.y;
    LONGLONG bottom = (LONGLONG)bounds.bottom + scroll.y;
    if (top < 0 || bottom < top || bottom > INT_MAX)
        return false;
    bounds.top = (LONG)top;
    bounds.bottom = (LONG)bottom;
    *out = bounds;
    return true;
}

static int font_line(HWND window, const LOGFONTW *_Nonnull description)
{
    HFONT font = CreateFontIndirectW(description);
    HDC device = GetDC(window);
    TEXTMETRICW metrics = {0};
    if (font != nullptr && device != nullptr)
    {
        HGDIOBJ previous = SelectObject(device, font);
        if (previous != nullptr && previous != HGDI_ERROR)
        {
            GetTextMetricsW(device, &metrics);
            SelectObject(device, previous);
        }
    }
    if (device != nullptr)
        ReleaseDC(window, device);
    if (font != nullptr)
        DeleteObject(font);
    return metrics.tmHeight + metrics.tmExternalLeading;
}

int note_pane_geometry_line(HWND window)
{
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(window, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&format);
    if (format.yHeight <= 0 || format.yHeight > 32760)
        return 0;
    int numerator = 0, denominator = 0;
    SendMessageW(window, EM_GETZOOM, (WPARAM)&numerator, (LPARAM)&denominator);
    int height = MulDiv(format.yHeight, (int)GetDpiForWindow(window), 1440);
    if (numerator > 0 && denominator > 0)
        height = MulDiv(height, numerator, denominator);
    LOGFONTW description = {.lfHeight = -height, .lfCharSet = format.bCharSet};
    memcpy(description.lfFaceName, format.szFaceName, sizeof description.lfFaceName);
    return font_line(window, &description);
}

bool note_pane_geometry_read(HWND window, ITextRange *_Nonnull range,
                             struct note_pane_geometry *_Nonnull out)
{
    LONG length = 0;
    if (FAILED(range->lpVtbl->GetStoryLength(range, &length)) || length <= 0 ||
        FAILED(range->lpVtbl->SetRange(range, length - 1, length - 1)))
        return false;
    RECT last = {0}, client = {0};
    if (!note_pane_geometry_bounds(window, range, &last) || !GetClientRect(window, &client) ||
        IsRectEmpty(&client))
        return false;
    POINT scroll = {0};
    SendMessageW(window, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    struct note_pane_geometry geometry = {.height = last.bottom,
                                          .width = client.right - client.left,
                                          .first = scroll.y < 0 ? 0 : scroll.y,
                                          .visible = client.bottom - client.top,
                                          .step = note_pane_geometry_line(window)};
    int limit = note_pane_geometry_limit(geometry);
    geometry.first = geometry.first > limit ? limit : geometry.first;
    if (geometry.height <= 0 || geometry.step <= 0)
        return false;
    *out = geometry;
    return true;
}
