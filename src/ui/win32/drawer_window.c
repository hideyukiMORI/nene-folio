#include "drawer_window.h"

#include "drawer_layout.h"
#include "folio_state.h"
#include "utf16_text.h"

#include <stdlib.h>

struct drawer_window
{
    HWND _Nullable handle;
    const struct folio_state *_Nonnull state;
    HFONT _Nullable category_font;
    HFONT _Nullable note_font;
};

static const wchar_t class_name[] = L"NeNeFolioDrawer";
static const wchar_t font_face[] = L"Yu Gothic UI";

/* 配色。利用者が変えるのはカテゴリの色だけで、それは台帳から来る（FR-007）。 */
constexpr COLORREF background_color = RGB(0xF6, 0xF5, 0xF2);
constexpr COLORREF category_text_color = RGB(0x24, 0x24, 0x24);
constexpr COLORREF note_text_color = RGB(0x4A, 0x4A, 0x4A);

/* 96 DPI での寸法。描くときに DPI で拡大する（FR-013）。 */
constexpr int base_dpi = 96;
constexpr int base_top_padding = 8;
constexpr int base_row_height = 30;
constexpr int base_category_indent = 16;
constexpr int base_note_indent = 36;
constexpr int base_color_bar_width = 4;
constexpr int base_font_height = 14;

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

static HFONT _Nullable create_font(UINT dpi, int weight)
{
    return CreateFontW(-scale(base_font_height, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE,
                       DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, font_face);
}

static void release_fonts(struct drawer_window *_Nonnull self)
{
    if (self->category_font != nullptr)
    {
        DeleteObject(self->category_font);
        self->category_font = nullptr;
    }
    if (self->note_font != nullptr)
    {
        DeleteObject(self->note_font);
        self->note_font = nullptr;
    }
}

static void refresh_fonts(struct drawer_window *_Nonnull self, UINT dpi)
{
    release_fonts(self);
    self->category_font = create_font(dpi, FW_SEMIBOLD);
    self->note_font = create_font(dpi, FW_NORMAL);
}

static struct drawer_metrics metrics_for(UINT dpi)
{
    struct drawer_metrics metrics = {
        .top_padding = scale(base_top_padding, dpi),
        .row_height = scale(base_row_height, dpi),
        .category_indent = scale(base_category_indent, dpi),
        .note_indent = scale(base_note_indent, dpi),
    };
    return metrics;
}

/* 1 行を描く。カテゴリ行は左端に色の帯を付ける。 */
static void draw_row(const struct drawer_window *_Nonnull self, HDC device, struct drawer_row row,
                     int width)
{
    UINT dpi = GetDpiForWindow(self->handle);
    if (row.kind == DRAWER_ROW_CATEGORY)
    {
        RECT bar = {0, row.top, scale(base_color_bar_width, dpi), row.top + row.height};
        HBRUSH brush = CreateSolidBrush(RGB(row.color.red, row.color.green, row.color.blue));
        FillRect(device, &bar, brush);
        DeleteObject(brush);
    }
    struct utf16_text *_Nullable text = nullptr;
    if (utf16_text_create(row.text, strlen(row.text), &text) != UTF16_TEXT_CONVERTED)
    {
        return;
    }
    bool category = row.kind == DRAWER_ROW_CATEGORY;
    SelectObject(device, category ? self->category_font : self->note_font);
    SetTextColor(device, category ? category_text_color : note_text_color);
    RECT bounds = {row.indent, row.top, width - scale(base_category_indent, dpi),
                   row.top + row.height};
    DrawTextW(device, utf16_text_units(text), (int)utf16_text_length(text), &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    utf16_text_destroy(text);
}

static void draw_rows(const struct drawer_window *_Nonnull self, HDC device, RECT client)
{
    struct drawer_layout *_Nullable layout = nullptr;
    UINT dpi = GetDpiForWindow(self->handle);
    if (folio_state_drawer_layout(self->state, metrics_for(dpi), &layout) != FOLIO_STATE_READY)
    {
        return;
    }
    SetBkMode(device, TRANSPARENT);
    size_t count = drawer_layout_row_count(layout);
    for (size_t index = 0; index < count; ++index)
    {
        draw_row(self, device, drawer_layout_row(layout, index), client.right);
    }
    drawer_layout_destroy(layout);
}

/* メモリ DC で完成させてから転送する（C-017）。 */
static void paint(struct drawer_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC target = BeginPaint(self->handle, &painting);
    RECT client;
    GetClientRect(self->handle, &client);
    HDC memory = CreateCompatibleDC(target);
    HBITMAP surface = CreateCompatibleBitmap(target, client.right, client.bottom);
    if (memory != nullptr && surface != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, surface);
        HBRUSH brush = CreateSolidBrush(background_color);
        FillRect(memory, &client, brush);
        DeleteObject(brush);
        draw_rows(self, memory, client);
        BitBlt(target, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, previous);
    }
    if (surface != nullptr)
    {
        DeleteObject(surface);
    }
    if (memory != nullptr)
    {
        DeleteDC(memory);
    }
    EndPaint(self->handle, &painting);
}

static struct drawer_window *_Nullable self_of(HWND window)
{
    return (struct drawer_window *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static LRESULT CALLBACK drawer_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct drawer_window *_Nullable self = self_of(window);
    if (message == WM_CREATE)
    {
        /* Win32 のコールバック引数を境界で受ける唯一の void *（C-006）。 */
        const CREATESTRUCTW *_Nonnull creation = (const CREATESTRUCTW *)lparam;
        self = creation->lpCreateParams;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)self);
        self->handle = window;
        refresh_fonts(self, GetDpiForWindow(window));
        return 0;
    }
    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    switch (message)
    {
    case WM_PAINT:
        paint(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_DPICHANGED_AFTERPARENT:
        refresh_fonts(self, GetDpiForWindow(window));
        InvalidateRect(window, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        release_fonts(self);
        self->handle = nullptr;
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(window, message, wparam, lparam);
    }
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
        .lpfnWndProc = drawer_procedure,
        .hInstance = instance,
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .lpszClassName = class_name,
    };
    return RegisterClassExW(&description) != 0;
}

enum drawer_window_outcome drawer_window_create(HWND _Nonnull parent,
                                                const struct folio_state *_Nonnull state,
                                                struct drawer_window *_Nullable *_Nonnull out)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!ensure_class(instance))
    {
        return DRAWER_WINDOW_NOT_CREATED;
    }
    struct drawer_window *_Nullable self = calloc(1, sizeof *self);
    if (self == nullptr)
    {
        return DRAWER_WINDOW_OUT_OF_MEMORY;
    }
    self->state = state;
    HWND handle = CreateWindowExW(0, class_name, L"", WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, parent,
                                  nullptr, instance, self);
    if (handle == nullptr)
    {
        drawer_window_destroy(self);
        return DRAWER_WINDOW_NOT_CREATED;
    }
    *out = self;
    return DRAWER_WINDOW_CREATED;
}

HWND _Nullable drawer_window_handle(const struct drawer_window *_Nonnull drawer)
{
    return drawer->handle;
}

void drawer_window_destroy(struct drawer_window *_Nullable drawer)
{
    if (drawer == nullptr)
    {
        return;
    }
    if (drawer->handle != nullptr)
    {
        DestroyWindow(drawer->handle);
    }
    release_fonts(drawer);
    free(drawer);
}
