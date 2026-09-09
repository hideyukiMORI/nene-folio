#include "folio_window.h"

#include "drawer_window.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "note_pane.h"
#include "utf16_text.h"

#include <dwmapi.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

struct folio_window
{
    HWND _Nullable handle;
    struct folio_state *_Nonnull state;
    struct folio_palette palette;
    struct drawer_window *_Nullable drawer;
    struct note_pane *_Nullable pane;
    HFONT _Nullable mono_font;
};

static const wchar_t class_name[] = L"NeNeFolioWindow";
static const wchar_t mono_face[] = L"Consolas";
static const wchar_t view_label[] = L"閲覧";

/* 96 DPI での寸法（デザイン「案2 堅」）。 */
constexpr int base_dpi = 96;
constexpr int base_width = 960;
constexpr int base_height = 640;
constexpr int base_drawer_width = 240;
constexpr int base_caption_height = 44; /* 右ペインの頭。窓を掴んで動かせる帯 */
constexpr int base_caption_indent = 36;
constexpr int base_caption_gap = 14;
constexpr int base_pane_left = 36;
constexpr int base_pane_right = 56;
constexpr int base_pane_top = 16;
constexpr int base_pane_bottom = 32;
constexpr int base_chip_padding = 12;
constexpr int base_chip_height = 24;
constexpr int base_chip_inset = 16;
constexpr int base_chip_radius = 3;
constexpr int base_mono_font = 11;
constexpr int base_tracking = 1;

/* DWM の窓の角と縁（Windows 11）。dwmapi.h の版によっては未定義なので数値で持つ。 */
constexpr DWORD attribute_corner_preference = 33;
constexpr DWORD attribute_border_color = 34;
constexpr DWORD corner_round_small = 3;

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

static struct folio_window *_Nullable self_of(HWND window)
{
    return (struct folio_window *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static void refresh_font(struct folio_window *_Nonnull self, UINT dpi)
{
    if (self->mono_font != nullptr)
    {
        DeleteObject(self->mono_font);
    }
    self->mono_font = CreateFontW(-scale(base_mono_font, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                  FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, mono_face);
}

static void arrange(const struct folio_window *_Nonnull self)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int drawer_width = scale(base_drawer_width, dpi);
    int top = scale(base_caption_height, dpi) + scale(base_pane_top, dpi);
    int left = drawer_width + scale(base_pane_left, dpi);
    if (drawer != nullptr)
    {
        MoveWindow(drawer, 0, 0, drawer_width, client.bottom, TRUE);
    }
    if (pane != nullptr)
    {
        MoveWindow(pane, left, top, client.right - left - scale(base_pane_right, dpi),
                   client.bottom - top - scale(base_pane_bottom, dpi), TRUE);
    }
}

static void render_pane(const struct folio_window *_Nonnull self)
{
    if (self->pane != nullptr)
    {
        note_pane_render(self->pane, folio_state_pane_rtf(self->state),
                         folio_state_pane_rtf_length(self->state));
    }
    InvalidateRect(self->handle, nullptr, FALSE);
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

/* UTF-8 を 1 行で描き、描いた幅を返す。 */
static int draw_utf8(HDC device, const char *_Nonnull text, RECT bounds)
{
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(text, strlen(text), &wide) != UTF16_TEXT_CONVERTED)
    {
        return 0;
    }
    RECT measured = bounds;
    DrawTextW(device, utf16_text_units(wide), (int)utf16_text_length(wide), &measured,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
    DrawTextW(device, utf16_text_units(wide), (int)utf16_text_length(wide), &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    utf16_text_destroy(wide);
    return measured.right - measured.left;
}

/* 頭のパンくず: 番号（カテゴリ色）・カテゴリ・/・ノート。選択が無ければ何も描かない。 */
static void draw_breadcrumb(const struct folio_window *_Nonnull self, HDC device, RECT caption)
{
    struct pane_title_view title = folio_state_pane_title(self->state);
    if (!title.any)
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    int gap = scale(base_caption_gap, dpi);
    RECT cursor = caption;
    wchar_t ordinal[3] = {(wchar_t)(L'0' + (title.ordinal / 10) % 10),
                          (wchar_t)(L'0' + title.ordinal % 10), L'\0'};
    SetTextColor(device, RGB(title.color.red, title.color.green, title.color.blue));
    RECT measured = cursor;
    DrawTextW(device, ordinal, 2, &measured,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
    DrawTextW(device, ordinal, 2, &cursor, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    cursor.left += measured.right - measured.left + gap;
    SetTextColor(device, self->palette.header_text);
    cursor.left += draw_utf8(device, title.category, cursor) + gap;
    SetTextColor(device, self->palette.border);
    cursor.left += draw_utf8(device, "/", cursor) + gap;
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, title.note, cursor);
}

/* 右端の「閲覧」の札。編集は機能が入るまで描かない。 */
static void draw_chip(const struct folio_window *_Nonnull self, HDC device, RECT caption)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT measured = caption;
    DrawTextW(device, view_label, -1, &measured, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    int width = measured.right - measured.left + scale(base_chip_padding, dpi) * 2;
    int height = scale(base_chip_height, dpi);
    int middle = (caption.top + caption.bottom) / 2;
    RECT chip = {caption.right - scale(base_chip_inset, dpi) - width, middle - height / 2,
                 caption.right - scale(base_chip_inset, dpi), middle - height / 2 + height};
    HBRUSH brush = CreateSolidBrush(self->palette.chip_background);
    HPEN pen = CreatePen(PS_NULL, 0, 0);
    HGDIOBJ old_brush = SelectObject(device, brush);
    HGDIOBJ old_pen = SelectObject(device, pen);
    int radius = scale(base_chip_radius, dpi) * 2;
    RoundRect(device, chip.left, chip.top, chip.right, chip.bottom, radius, radius);
    SelectObject(device, old_pen);
    SelectObject(device, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
    SetTextColor(device, self->palette.chip_text);
    DrawTextW(device, view_label, -1, &chip, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
}

/* 右ペインの地と頭を描く。本文は note_pane が持つ。 */
static void paint_pane(struct folio_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC device = BeginPaint(self->handle, &painting);
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT pane = {scale(base_drawer_width, dpi), 0, client.right, client.bottom};
    HBRUSH brush = CreateSolidBrush(self->palette.pane);
    FillRect(device, &pane, brush);
    DeleteObject(brush);
    RECT caption = {pane.left + scale(base_caption_indent, dpi), 0, client.right,
                    scale(base_caption_height, dpi)};
    SetBkMode(device, TRANSPARENT);
    SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    draw_breadcrumb(self, device, caption);
    draw_chip(self, device, caption);
    SetTextCharacterExtra(device, 0);
    EndPaint(self->handle, &painting);
}

static LRESULT on_create(HWND window, LPARAM lparam)
{
    /* Win32 のコールバック引数を境界で受ける唯一の void *（C-006）。 */
    const CREATESTRUCTW *_Nonnull creation = (const CREATESTRUCTW *)lparam;
    struct folio_window *_Nonnull self = creation->lpCreateParams;
    SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)self);
    self->handle = window;
    refresh_font(self, GetDpiForWindow(window));
    if (drawer_window_create(window, self->state, &self->drawer) != DRAWER_WINDOW_CREATED ||
        note_pane_create(window, self->palette.pane, &self->pane) != NOTE_PANE_CREATED)
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
        refresh_font(self, GetDpiForWindow(self->handle));
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
        if (self->mono_font != nullptr)
        {
            DeleteObject(self->mono_font);
            self->mono_font = nullptr;
        }
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

/* Windows 11 の小さな角丸と、テーマに合わせた縁の色。古い OS では無視される。 */
static void decorate(HWND handle, struct folio_palette palette)
{
    DWORD corner = corner_round_small;
    DwmSetWindowAttribute(handle, attribute_corner_preference, &corner, sizeof corner);
    COLORREF border = palette.border;
    DwmSetWindowAttribute(handle, attribute_border_color, &border, sizeof border);
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
    self->palette = folio_palette_for(folio_state_theme(state));
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
    decorate(handle, self->palette);
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
    if (window->mono_font != nullptr)
    {
        DeleteObject(window->mono_font);
    }
    free(window);
}
