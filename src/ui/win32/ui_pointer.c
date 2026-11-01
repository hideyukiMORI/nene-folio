#include "ui_pointer.h"

#include <windowsx.h>

static LPCWSTR cursor_name(enum ui_pointer_kind kind)
{
    switch (kind)
    {
    case UI_POINTER_ARROW:
        return IDC_ARROW;
    case UI_POINTER_ACTION:
        return IDC_HAND;
    case UI_POINTER_TEXT:
        return IDC_IBEAM;
    case UI_POINTER_RESIZE:
        return IDC_SIZEWE;
    }
    return IDC_ARROW;
}

void ui_pointer_show(enum ui_pointer_kind kind)
{
    SetCursor(LoadCursorW(nullptr, cursor_name(kind)));
}

bool ui_pointer_client(HWND _Nonnull window, WPARAM target, LPARAM hit)
{
    return (HWND)target == window && LOWORD(hit) == HTCLIENT;
}

POINT ui_pointer_message_point(HWND _Nonnull window)
{
    DWORD position = GetMessagePos();
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    ScreenToClient(window, &point);
    return point;
}

static bool refresh_message(UINT message)
{
    switch (message)
    {
    case WM_PAINT:
    case WM_SIZE:
    case WM_ENABLE:
    case WM_SHOWWINDOW:
    case WM_DPICHANGED:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_HSCROLL:
    case WM_VSCROLL:
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
    case WM_COMMAND:
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
        return true;
    default:
        return false; /* 開いたWin32 message。cursor/hit/notifyの再入は再評価しない。 */
    }
}

void ui_pointer_after(HWND _Nonnull window, UINT message)
{
    if (!refresh_message(message) || !IsWindow(window) || GetCapture() != nullptr)
        return;
    DWORD position = GetMessagePos();
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    HWND target = WindowFromPoint(point);
    HWND root = GetAncestor(window, GA_ROOT);
    if (target == nullptr || GetAncestor(target, GA_ROOT) != root)
        return;
    LRESULT hit = SendMessageW(target, WM_NCHITTEST, 0, (LPARAM)position);
    while (hit == HTTRANSPARENT && target != root)
    {
        target = GetParent(target);
        if (target == nullptr)
            return;
        hit = SendMessageW(target, WM_NCHITTEST, 0, (LPARAM)position);
    }
    SendMessageW(target, WM_SETCURSOR, (WPARAM)target, MAKELPARAM(hit, WM_MOUSEMOVE));
}
