#include "folio_window.h"

#include "drawer_window.h"
#include "folio_message.h"
#include "folio_state.h"
#include "note_pane.h"

#include <stdlib.h>
#include <windows.h>
#include <windowsx.h>

struct folio_window
{
    HWND _Nullable handle;
    struct folio_state *_Nonnull state;
    struct drawer_window *_Nullable drawer;
    struct note_pane *_Nullable pane;
};

static const wchar_t class_name[] = L"NeNeFolioWindow";

constexpr COLORREF pane_color = RGB(0xFF, 0xFF, 0xFF);

/* 96 DPI での寸法（SPECIFICATION 第 2 節の初期値。実機で見て変えてよい）。 */
constexpr int base_dpi = 96;
constexpr int base_width = 960;
constexpr int base_height = 640;
constexpr int base_drawer_width = 240;
constexpr int base_caption_height = 36; /* 右ペイン上部の、窓を掴んで動かせる帯 */
constexpr int base_pane_margin = 24;    /* 右ペインの本文の余白 */

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

static struct folio_window *_Nullable self_of(HWND window)
{
    return (struct folio_window *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static void arrange(const struct folio_window *_Nonnull self)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int drawer_width = scale(base_drawer_width, dpi);
    int caption = scale(base_caption_height, dpi);
    int margin = scale(base_pane_margin, dpi);
    if (drawer != nullptr)
    {
        MoveWindow(drawer, 0, 0, drawer_width, client.bottom, TRUE);
    }
    if (pane != nullptr)
    {
        MoveWindow(pane, drawer_width + margin, caption, client.right - drawer_width - 2 * margin,
                   client.bottom - caption - margin, TRUE);
    }
}

static void render_pane(const struct folio_window *_Nonnull self)
{
    if (self->pane != nullptr)
    {
        note_pane_render(self->pane, folio_state_pane_rtf(self->state),
                         folio_state_pane_rtf_length(self->state));
    }
}

/* value が low 未満なら 0、high 以上なら 2、間なら 1。 */
static int band(LONG value, LONG low, LONG high)
{
    if (value < low)
    {
        return 0;
    }
    return value >= high ? 2 : 1;
}

/* 縁の判定。枠を消したので DefWindowProcW は全域を HTCLIENT と答える。 */
static LRESULT edge_hit(POINT point, RECT window, int border)
{
    static const LRESULT hits[3][3] = {{HTTOPLEFT, HTTOP, HTTOPRIGHT},
                                       {HTLEFT, HTNOWHERE, HTRIGHT},
                                       {HTBOTTOMLEFT, HTBOTTOM, HTBOTTOMRIGHT}};
    int column = band(point.x, window.left + border, window.right - border);
    int row = band(point.y, window.top + border, window.bottom - border);
    return hits[row][column];
}

static LRESULT hit_test(const struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    RECT window;
    GetWindowRect(self->handle, &window);
    UINT dpi = GetDpiForWindow(self->handle);
    int border = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                 GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    LRESULT edge = edge_hit(point, window, border);
    if (edge != HTNOWHERE)
    {
        return edge;
    }
    bool in_caption = point.x >= window.left + scale(base_drawer_width, dpi) &&
                      point.y < window.top + scale(base_caption_height, dpi);
    return in_caption ? HTCAPTION : HTCLIENT;
}

static void paint_pane(const struct folio_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC device = BeginPaint(self->handle, &painting);
    HBRUSH brush = CreateSolidBrush(pane_color);
    FillRect(device, &painting.rcPaint, brush);
    DeleteObject(brush);
    EndPaint(self->handle, &painting);
}

static LRESULT on_create(HWND window, LPARAM lparam)
{
    /* Win32 のコールバック引数を境界で受ける唯一の void *（C-006）。 */
    const CREATESTRUCTW *_Nonnull creation = (const CREATESTRUCTW *)lparam;
    struct folio_window *_Nonnull self = creation->lpCreateParams;
    SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)self);
    self->handle = window;
    if (drawer_window_create(window, self->state, &self->drawer) != DRAWER_WINDOW_CREATED ||
        note_pane_create(window, &self->pane) != NOTE_PANE_CREATED)
    {
        return -1;
    }
    render_pane(self);
    return 0;
}

static LRESULT on_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                          LPARAM lparam)
{
    switch (message)
    {
    case WM_NCCALCSIZE:
        return wparam ? 0 : DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_NCHITTEST:
        return hit_test(self, lparam);
    case WM_SIZE:
        arrange(self);
        return 0;
    case WM_DPICHANGED:
    {
        const RECT *_Nonnull suggested = (const RECT *)lparam;
        SetWindowPos(self->handle, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_PAINT:
        paint_pane(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case folio_message_selection_changed:
        render_pane(self);
        return 0;
    case WM_KEYDOWN:
        if (wparam == VK_ESCAPE)
        {
            DestroyWindow(self->handle);
        }
        return 0;
    case WM_DESTROY:
        self->handle = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(self->handle, message, wparam, lparam);
    }
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_CREATE)
    {
        return on_create(window, lparam);
    }
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return on_message(self, message, wparam, lparam);
}

static bool ensure_class(HINSTANCE instance)
{
    WNDCLASSEXW existing = {.cbSize = sizeof existing};
    if (GetClassInfoExW(instance, class_name, &existing))
    {
        return true;
    }
    WNDCLASSEXW description = {
        .cbSize = sizeof description,
        .lpfnWndProc = window_procedure,
        .hInstance = instance,
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .lpszClassName = class_name,
    };
    return RegisterClassExW(&description) != 0;
}

enum folio_window_outcome folio_window_create(struct folio_state *_Nonnull state,
                                              struct folio_window *_Nullable *_Nonnull out)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!ensure_class(instance))
    {
        return FOLIO_WINDOW_NOT_CREATED;
    }
    struct folio_window *_Nullable self = calloc(1, sizeof *self);
    if (self == nullptr)
    {
        return FOLIO_WINDOW_OUT_OF_MEMORY;
    }
    self->state = state;
    UINT dpi = GetDpiForSystem();
    DWORD style = WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    HWND handle = CreateWindowExW(0, class_name, L"NeNe Folio", style, CW_USEDEFAULT, CW_USEDEFAULT,
                                  scale(base_width, dpi), scale(base_height, dpi), nullptr, nullptr,
                                  instance, self);
    if (handle == nullptr)
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
    ShowWindow(handle, SW_SHOW);
    *out = self;
    return FOLIO_WINDOW_CREATED;
}

void folio_window_destroy(struct folio_window *_Nullable window)
{
    if (window == nullptr)
    {
        return;
    }
    if (window->handle != nullptr)
    {
        DestroyWindow(window->handle);
    }
    note_pane_destroy(window->pane);
    drawer_window_destroy(window->drawer);
    free(window);
}
