#include "dialog_theme.h"

#include "button_pointer_state.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_shortcut.h"
#include "folio_state.h"
#include "icon_paint.h"
#include "ui_part.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include <commctrl.h>
#include <stddef.h>
#include <windowsx.h>

#include <dwmapi.h>
#include <stdlib.h>

struct dialog_theme
{
    struct folio_palette palette;
    HBRUSH _Nonnull window; /* 面の地と STATIC */
    HBRUSH _Nonnull chip;   /* 札の地（釦・枠） */
    HBRUSH _Nonnull field;  /* EDIT の中とコンボの閉じた面 */
    HBRUSH _Nonnull list;   /* ドロップダウンのリスト */
};

/* コンボの項目の字を左の縁から離す量（96 DPI）。 */
constexpr int base_item_indent = 4;
static const wchar_t pointer_property[] = L"NeNeFolio.DialogButtonPointer";
constexpr UINT tooltip_info_size = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM);

static LRESULT CALLBACK button_pointer(HWND, UINT, WPARAM, LPARAM);

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, 96);
}

static void delete_brush(HBRUSH _Nullable brush)
{
    if (brush != nullptr)
    {
        DeleteObject(brush);
    }
}

enum dialog_theme_outcome dialog_theme_create(const struct folio_palette *_Nonnull palette,
                                              struct dialog_theme *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct dialog_theme *_Nullable self = calloc(1, sizeof *self);
    if (self == nullptr)
    {
        return DIALOG_THEME_NO_MEMORY;
    }
    HBRUSH window = CreateSolidBrush(palette->dialog_surface);
    HBRUSH chip = CreateSolidBrush(palette->chip_background);
    HBRUSH field = CreateSolidBrush(palette->pane);
    HBRUSH list = CreateSolidBrush(palette->pane);
    if (window == nullptr || chip == nullptr || field == nullptr || list == nullptr)
    {
        delete_brush(window);
        delete_brush(chip);
        delete_brush(field);
        delete_brush(list);
        free(self);
        return DIALOG_THEME_NO_BRUSH;
    }
    self->palette = *palette;
    self->window = window;
    self->chip = chip;
    self->field = field;
    self->list = list;
    *out = self;
    return DIALOG_THEME_READY;
}

void dialog_theme_destroy(struct dialog_theme *_Nullable theme)
{
    if (theme == nullptr)
    {
        return;
    }
    DeleteObject(theme->window);
    DeleteObject(theme->chip);
    DeleteObject(theme->field);
    DeleteObject(theme->list);
    free(theme);
}

bool dialog_theme_follow(HWND _Nonnull dialog, const struct folio_state *_Nonnull state,
                         struct dialog_theme *_Nullable *_Nonnull theme)
{
    HWND owner = GetWindow(dialog, GW_OWNER);
    if (owner != nullptr)
    {
        SendMessageW(owner, folio_message_appearance_changed, 0, 0);
    }
    struct folio_palette palette = folio_palette_from(folio_state_palette(state));
    struct dialog_theme *_Nullable changed = nullptr;
    switch (dialog_theme_create(&palette, &changed))
    {
    case DIALOG_THEME_READY:
        break;
    case DIALOG_THEME_NO_MEMORY:
    case DIALOG_THEME_NO_BRUSH:
        return false;
    }
    dialog_theme_destroy(*theme);
    *theme = changed;
    dialog_theme_decorate(changed, dialog);
    RedrawWindow(dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
    return true;
}

HBRUSH dialog_theme_color(const struct dialog_theme *_Nonnull theme,
                          enum dialog_theme_surface surface, HDC _Nonnull device)
{
    COLORREF text = theme->palette.current_text;
    COLORREF background = theme->palette.dialog_surface;
    HBRUSH brush = theme->window;
    switch (surface)
    {
    case DIALOG_THEME_DIALOG:
    case DIALOG_THEME_LABEL:
        break;
    case DIALOG_THEME_SECONDARY:
        text = theme->palette.dialog_secondary;
        break;
    case DIALOG_THEME_FIELD:
        text = theme->palette.editor_text;
        background = theme->palette.pane;
        brush = theme->field;
        break;
    case DIALOG_THEME_LIST:
        text = theme->palette.editor_text;
        background = theme->palette.pane;
        brush = theme->list;
        break;
    }
    SetTextColor(device, text);
    SetBkColor(device, background);
    return brush;
}

/* 字は釦の窓から取り、面が選んだ書体で描く（ui_text の ID を二重に持たない・決定 2）。 */
static void button_caption(const DRAWITEMSTRUCT *_Nonnull item, RECT bounds, COLORREF color)
{
    wchar_t units[ui_text_unit_limit];
    int count = GetWindowTextW(item->hwndItem, units, (int)ui_text_unit_limit);
    HFONT font = (HFONT)SendMessageW(item->hwndItem, WM_GETFONT, 0, 0);
    HGDIOBJ old_font = font != nullptr ? SelectObject(item->hDC, font) : nullptr;
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, color);
    DrawTextW(item->hDC, units, count, &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER |
                  ((item->itemState & ODS_NOACCEL) != 0 ? DT_HIDEPREFIX : 0));
    if (old_font != nullptr)
    {
        SelectObject(item->hDC, old_font);
    }
}

static enum ui_part_style button_style(enum dialog_theme_button kind)
{
    switch (kind)
    {
    case DIALOG_THEME_BUTTON_PRIMARY:
        return UI_PART_STYLE_PRIMARY;
    case DIALOG_THEME_BUTTON_SECONDARY:
        return UI_PART_STYLE_COMMAND;
    case DIALOG_THEME_BUTTON_CLOSE:
        return UI_PART_STYLE_ICON;
    }
    return UI_PART_STYLE_COMMAND;
}

static bool button_hovered(HWND window)
{
    const struct button_pointer_state *pointer = GetPropW(window, pointer_property);
    return pointer != nullptr && pointer->hovered;
}

void dialog_theme_draw_button(const struct dialog_theme *_Nonnull theme,
                              const DRAWITEMSTRUCT *_Nonnull item, enum dialog_theme_button kind)
{
    FillRect(item->hDC, &item->rcItem, theme->window);
    struct ui_part_paint part = {.bounds = item->rcItem,
                                 .style = button_style(kind),
                                 .dpi = GetDpiForWindow(item->hwndItem),
                                 .focus_inside = true,
                                 .labeled = kind != DIALOG_THEME_BUTTON_CLOSE,
                                 .has_icon = kind == DIALOG_THEME_BUTTON_CLOSE,
                                 .icon = ICON_PAINT_CLOSE,
                                 .hovered = button_hovered(item->hwndItem),
                                 .pressed = (item->itemState & ODS_SELECTED) != 0,
                                 .focused = (item->itemState & ODS_FOCUS) != 0 &&
                                            (item->itemState & ODS_NOFOCUSRECT) == 0,
                                 .disabled = (item->itemState & ODS_DISABLED) != 0};
    struct ui_part_ink ink = ui_part_draw(item->hDC, &theme->palette, &part);
    if (part.labeled)
    {
        RECT label = item->rcItem;
        InflateRect(&label, -scale(ui_part_padding, part.dpi), 0);
        button_caption(item, label, ink.text);
    }
}

void dialog_theme_draw_item(const struct dialog_theme *_Nonnull theme,
                            const DRAWITEMSTRUCT *_Nonnull item, const wchar_t *_Nonnull text)
{
    bool closed = (item->itemState & ODS_COMBOBOXEDIT) != 0;
    bool selected = !closed && (item->itemState & ODS_SELECTED) != 0;
    COLORREF background = selected ? theme->palette.selected_background : theme->palette.pane;
    COLORREF ink = selected ? theme->palette.selected_text : theme->palette.editor_text;
    if ((item->itemState & ODS_DISABLED) != 0 && theme->palette.contrast != FOLIO_CONTRAST_HIGH)
    {
        ink = theme->palette.border;
    }
    RECT bounds = item->rcItem;
    SetDCBrushColor(item->hDC, background);
    FillRect(item->hDC, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    if (closed && (item->itemState & ODS_FOCUS) != 0 && (item->itemState & ODS_NOFOCUSRECT) == 0)
    {
        FrameRect(item->hDC, &bounds, theme->chip);
    }
    UINT dpi = GetDpiForWindow(item->hwndItem);
    bounds.left += scale(base_item_indent, dpi);
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, ink);
    DrawTextW(item->hDC, text, -1, &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

void dialog_theme_field(const struct dialog_theme *_Nonnull theme, HDC _Nonnull dc,
                        const RECT *_Nonnull rect, int focus)
{
    ui_part_fill_field(dc, *rect, theme->palette.pane);
    ui_part_draw_field(dc, &theme->palette, *rect, focus);
}

void dialog_theme_decorate(const struct dialog_theme *_Nonnull theme, HWND _Nonnull dialog)
{
    /* 地に対して白い字が立つなら暗い面なので、題の帯も暗くする（ADR 0017 と同じ判定）。 */
    BOOL dark = folio_palette_ink(theme->palette.window) == RGB(0xFF, 0xFF, 0xFF) ? TRUE : FALSE;
    DwmSetWindowAttribute(dialog, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof dark);
    DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(dialog, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof corners);
    COLORREF border = DWMWA_COLOR_NONE;
    DwmSetWindowAttribute(dialog, DWMWA_BORDER_COLOR, &border, sizeof border);
    COLORREF caption = theme->palette.dialog_surface;
    DwmSetWindowAttribute(dialog, DWMWA_CAPTION_COLOR, &caption, sizeof caption);
    COLORREF text = theme->palette.current_text;
    DwmSetWindowAttribute(dialog, DWMWA_TEXT_COLOR, &text, sizeof text);
}

static void pointer_destroy(struct button_pointer_state *_Nullable pointer)
{
    free(pointer);
}

static void pointer_move(HWND window, struct button_pointer_state *_Nonnull pointer)
{
    if (pointer->hovered)
        return;
    TRACKMOUSEEVENT tracking = {
        .cbSize = sizeof tracking, .dwFlags = TME_LEAVE, .hwndTrack = window};
    if (TrackMouseEvent(&tracking))
    {
        pointer->hovered = true;
        InvalidateRect(window, nullptr, FALSE);
    }
}

static LRESULT CALLBACK button_pointer(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct button_pointer_state *pointer = GetPropW(window, pointer_property);
    if (pointer == nullptr)
        return DefWindowProcW(window, message, wparam, lparam);
    if (message == WM_MOUSEMOVE)
        pointer_move(window, pointer);
    if (message == WM_MOUSELEAVE)
    {
        pointer->hovered = false;
        InvalidateRect(window, nullptr, FALSE);
    }
    LRESULT result = CallWindowProcW(pointer->original, window, message, wparam, lparam);
    if (message == WM_NCDESTROY)
    {
        RemovePropW(window, pointer_property);
        pointer_destroy(pointer);
    }
    return result;
}

enum dialog_theme_control_outcome dialog_theme_track_button(HWND _Nonnull button)
{
    struct button_pointer_state *pointer = calloc(1, sizeof *pointer);
    if (pointer == nullptr)
        return DIALOG_THEME_CONTROL_UNAVAILABLE;
    pointer->original = (WNDPROC)GetWindowLongPtrW(button, GWLP_WNDPROC);
    if (!SetPropW(button, pointer_property, pointer))
    {
        pointer_destroy(pointer);
        return DIALOG_THEME_CONTROL_UNAVAILABLE;
    }
    if (SetWindowLongPtrW(button, GWLP_WNDPROC, (LONG_PTR)button_pointer) == 0)
    {
        RemovePropW(button, pointer_property);
        pointer_destroy(pointer);
        return DIALOG_THEME_CONTROL_UNAVAILABLE;
    }
    return DIALOG_THEME_CONTROL_READY;
}

static HWND _Nullable close_tooltip(HWND dialog, HWND button, enum folio_language language)
{
    INITCOMMONCONTROLSEX controls = {.dwSize = sizeof controls, .dwICC = ICC_WIN95_CLASSES};
    if (!InitCommonControlsEx(&controls))
    {
        return nullptr;
    }
    HWND tip = CreateWindowExW(WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
                               WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, dialog, nullptr,
                               GetModuleHandleW(nullptr), nullptr);
    if (tip == nullptr)
    {
        return nullptr;
    }
    TOOLINFOW tool = {.cbSize = tooltip_info_size,
                      .uFlags = TTF_IDISHWND | TTF_SUBCLASS,
                      .hwnd = dialog,
                      .uId = (UINT_PTR)button,
                      .lpszText = LPSTR_TEXTCALLBACKW,
                      .lParam = (LPARAM)language};
    bool placed =
        SetWindowPos(tip, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE) != 0;
    if (!placed || !SendMessageW(tip, TTM_ADDTOOLW, 0, (LPARAM)&tool))
    {
        DestroyWindow(tip);
        return nullptr;
    }
    SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, scale(360, GetDpiForWindow(dialog)));
    return tip;
}

enum dialog_theme_control_outcome dialog_theme_create_close(HWND _Nonnull dialog,
                                                            HFONT _Nonnull font,
                                                            enum folio_language language,
                                                            HWND _Nullable *_Nonnull out)
{
    *out = nullptr;
    char16_t name[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(ui_text_line(UI_TEXT_TOOLTIP_CLOSE, language), name, ui_text_unit_limit,
                        &written) != UTF16_TEXT_FILL_READY)
    {
        return DIALOG_THEME_CONTROL_UNAVAILABLE;
    }
    HWND button =
        CreateWindowExW(0, L"BUTTON", name, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 0, 0,
                        dialog, (HMENU)(INT_PTR)IDCLOSE, GetModuleHandleW(nullptr), nullptr);
    if (button == nullptr)
    {
        return DIALOG_THEME_CONTROL_UNAVAILABLE;
    }
    if (dialog_theme_track_button(button) != DIALOG_THEME_CONTROL_READY ||
        close_tooltip(dialog, button, language) == nullptr)
    {
        DestroyWindow(button);
        return DIALOG_THEME_CONTROL_UNAVAILABLE;
    }
    SendMessageW(button, WM_SETFONT, (WPARAM)font, TRUE);
    *out = button;
    return DIALOG_THEME_CONTROL_READY;
}

static bool close_tip_text(HWND dialog, LPARAM lparam)
{
    NMHDR *header = (NMHDR *)lparam;
    if (header->code != TTN_GETDISPINFOW ||
        header->idFrom != (UINT_PTR)GetDlgItem(dialog, IDCLOSE) ||
        GetWindow(header->hwndFrom, GW_OWNER) != dialog)
    {
        return false;
    }
    NMTTDISPINFOW *info = (NMTTDISPINFOW *)lparam;
    if (info->lParam < FOLIO_LANGUAGE_JA || info->lParam > FOLIO_LANGUAGE_ZH_HANS)
    {
        return false;
    }
    enum folio_language language = (enum folio_language)info->lParam;
    struct ui_text_request request = {
        .id = UI_TEXT_TOOLTIP_NAMED_KEY,
        .language = language,
        .name = ui_text_line(UI_TEXT_TOOLTIP_CLOSE, language),
        .from = folio_shortcut_command_hint(FOLIO_COMMAND_QUIT, language)};
    char text[ui_text_unit_limit * 4];
    size_t written = 0;
    info->szText[0] = L'\0';
    if (ui_text_format(&request, text, sizeof text) == UI_TEXT_FORMAT_READY)
    {
        (void)utf16_text_fill(text, info->szText, sizeof info->szText / sizeof info->szText[0],
                              &written);
    }
    info->lpszText = info->szText;
    return true;
}

static void native_system_menu(HWND dialog)
{
    HMENU menu = GetSystemMenu(dialog, FALSE);
    if (menu == nullptr)
    {
        return;
    }
    RECT bounds;
    GetWindowRect(dialog, &bounds);
    UINT dpi = GetDpiForWindow(dialog);
    UINT selected = (UINT)TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN, bounds.left + scale(ui_part_edge, dpi),
        bounds.top + scale(ui_part_surface_title_height, dpi), 0, dialog, nullptr);
    if (selected != 0)
    {
        PostMessageW(dialog, WM_SYSCOMMAND, selected, 0);
    }
}

static LRESULT native_hit(HWND dialog, LPARAM lparam)
{
    RECT client;
    GetClientRect(dialog, &client);
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(dialog, &point);
    UINT dpi = GetDpiForWindow(dialog);
    RECT title = ui_part_surface_rect(client, dpi, UI_PART_SURFACE_TITLE);
    RECT close = ui_part_surface_rect(client, dpi, UI_PART_SURFACE_CLOSE);
    return PtInRect(&title, point) && !PtInRect(&close, point) ? HTCAPTION : HTCLIENT;
}

bool dialog_theme_native_message(HWND _Nonnull dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCALCSIZE || message == WM_NCHITTEST)
    {
        SetWindowLongPtrW(dialog, DWLP_MSGRESULT,
                          message == WM_NCHITTEST ? native_hit(dialog, lparam) : 0);
        return true;
    }
    bool menu = message == WM_SYSCOMMAND && (wparam & 0xFFF0) == SC_KEYMENU && lparam == VK_SPACE;
    bool root_menu =
        message == WM_SYSKEYDOWN && wparam == VK_SPACE && (lparam & ((LPARAM)1 << 29)) != 0;
    if (menu || root_menu)
    {
        native_system_menu(dialog);
        return true;
    }
    return message == WM_NOTIFY && lparam != 0 && close_tip_text(dialog, lparam);
}

RECT dialog_theme_popup_bounds(HWND _Nullable owner, SIZE wanted_client)
{
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    RECT area = {0, 0, wanted_client.cx, wanted_client.cy};
    if (GetMonitorInfoW(MonitorFromWindow(owner, MONITOR_DEFAULTTOPRIMARY), &monitor))
    {
        area = monitor.rcWork;
    }
    RECT center = area;
    if (owner != nullptr)
    {
        GetWindowRect(owner, &center);
    }
    int width =
        wanted_client.cx < area.right - area.left ? wanted_client.cx : area.right - area.left;
    int height =
        wanted_client.cy < area.bottom - area.top ? wanted_client.cy : area.bottom - area.top;
    int left = (center.left + center.right - width) / 2;
    int top = (center.top + center.bottom - height) / 2;
    left = left < area.left ? area.left : left;
    top = top < area.top ? area.top : top;
    left = left + width > area.right ? area.right - width : left;
    top = top + height > area.bottom ? area.bottom - height : top;
    return (RECT){left, top, left + width, top + height};
}

void dialog_theme_draw_title(const struct dialog_theme *_Nonnull theme, HWND _Nonnull dialog,
                             HDC _Nonnull device, HFONT _Nonnull font)
{
    RECT client;
    GetClientRect(dialog, &client);
    ui_part_draw_surface(device, &theme->palette, client);
    RECT title = ui_part_surface_rect(client, GetDpiForWindow(dialog), UI_PART_SURFACE_TITLE_TEXT);
    wchar_t text[ui_text_unit_limit];
    GetWindowTextW(dialog, text, (int)ui_text_unit_limit);
    HGDIOBJ previous = SelectObject(device, font);
    SetBkMode(device, TRANSPARENT);
    SetTextColor(device, theme->palette.current_text);
    DrawTextW(device, text, -1, &title, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(device, previous);
}

void dialog_theme_draw_warning(const struct dialog_theme *_Nonnull theme, HDC _Nonnull device,
                               RECT bounds)
{
    icon_paint_fill(device, bounds, ICON_PAINT_WARNING, theme->palette.current_text);
}
