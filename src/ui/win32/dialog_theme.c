#include "dialog_theme.h"
#include "folio_palette.h"
#include "ui_text.h"

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

/* 右ペインの頭の札と同じ角（folio_window.c の base_chip_radius・ADR 0035 の決定 2）。 */
constexpr int base_chip_radius = 3;
/* コンボの項目の字を左の縁から離す量（96 DPI）。 */
constexpr int base_item_indent = 4;
/* 釦のフォーカスの印を札の縁から離す量（96 DPI）。 */
constexpr int base_focus_inset = 3;

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
    HBRUSH window = CreateSolidBrush(palette->window);
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

HBRUSH dialog_theme_color(const struct dialog_theme *_Nonnull theme,
                          enum dialog_theme_surface surface, HDC _Nonnull device)
{
    COLORREF text = theme->palette.current_text;
    COLORREF background = theme->palette.window;
    HBRUSH brush = theme->window;
    switch (surface)
    {
    case DIALOG_THEME_DIALOG:
    case DIALOG_THEME_LABEL:
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

/* 1px の枠を DC のブラシで描く（一時のブラシを作らない）。 */
static void frame(HDC device, RECT bounds, COLORREF color)
{
    SetDCBrushColor(device, color);
    FrameRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
}

/* 釦の矩形いっぱいの札の角丸。面の色 face と縁の色 edge を DC のブラシとペンで当てる。 */
static void round_face(const DRAWITEMSTRUCT *_Nonnull item, COLORREF face, COLORREF edge)
{
    HDC device = item->hDC;
    RECT bounds = item->rcItem;
    UINT dpi = GetDpiForWindow(item->hwndItem);
    SetDCBrushColor(device, face);
    SetDCPenColor(device, edge);
    HGDIOBJ old_brush = SelectObject(device, GetStockObject(DC_BRUSH));
    HGDIOBJ old_pen = SelectObject(device, GetStockObject(DC_PEN));
    int radius = scale(base_chip_radius, dpi) * 2;
    RoundRect(device, bounds.left, bounds.top, bounds.right, bounds.bottom, radius, radius);
    SelectObject(device, old_pen);
    SelectObject(device, old_brush);
}

/* 釦の面と縁の色。既定は札の地、他は面の地に札の色の縁。押している間は選択の面。 */
static COLORREF button_face(const struct dialog_theme *_Nonnull theme, UINT state,
                            enum dialog_theme_button kind)
{
    if ((state & ODS_SELECTED) != 0)
    {
        return theme->palette.selected_background;
    }
    switch (kind)
    {
    case DIALOG_THEME_BUTTON_PRIMARY:
        return theme->palette.chip_background;
    case DIALOG_THEME_BUTTON_SECONDARY:
        return theme->palette.window;
    }
    return theme->palette.window;
}

static COLORREF button_text(const struct dialog_theme *_Nonnull theme, UINT state,
                            enum dialog_theme_button kind)
{
    if ((state & ODS_DISABLED) != 0)
    {
        return theme->palette.border;
    }
    if ((state & ODS_SELECTED) != 0)
    {
        return theme->palette.selected_text;
    }
    switch (kind)
    {
    case DIALOG_THEME_BUTTON_PRIMARY:
        return theme->palette.chip_text;
    case DIALOG_THEME_BUTTON_SECONDARY:
        return theme->palette.current_text;
    }
    return theme->palette.current_text;
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
              DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
    if (old_font != nullptr)
    {
        SelectObject(item->hDC, old_font);
    }
}

void dialog_theme_draw_button(const struct dialog_theme *_Nonnull theme,
                              const DRAWITEMSTRUCT *_Nonnull item, enum dialog_theme_button kind)
{
    UINT dpi = GetDpiForWindow(item->hwndItem);
    RECT bounds = item->rcItem;
    FillRect(item->hDC, &bounds, theme->window);
    COLORREF face = button_face(theme, item->itemState, kind);
    COLORREF edge = kind == DIALOG_THEME_BUTTON_PRIMARY && (item->itemState & ODS_SELECTED) == 0
                        ? face
                        : theme->palette.chip_background;
    round_face(item, face, edge);
    COLORREF text = button_text(theme, item->itemState, kind);
    if ((item->itemState & ODS_FOCUS) != 0 && (item->itemState & ODS_NOFOCUSRECT) == 0)
    {
        RECT focus = bounds;
        InflateRect(&focus, -scale(base_focus_inset, dpi), -scale(base_focus_inset, dpi));
        frame(item->hDC, focus, text);
    }
    button_caption(item, bounds, text);
}

void dialog_theme_draw_item(const struct dialog_theme *_Nonnull theme,
                            const DRAWITEMSTRUCT *_Nonnull item, const wchar_t *_Nonnull text)
{
    bool closed = (item->itemState & ODS_COMBOBOXEDIT) != 0;
    bool selected = !closed && (item->itemState & ODS_SELECTED) != 0;
    COLORREF background = selected ? theme->palette.selected_background : theme->palette.pane;
    COLORREF ink = selected ? theme->palette.selected_text : theme->palette.editor_text;
    if ((item->itemState & ODS_DISABLED) != 0)
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

void dialog_theme_frame(const struct dialog_theme *_Nonnull theme, HDC _Nonnull dc,
                        const RECT *_Nonnull rect)
{
    FrameRect(dc, rect, theme->chip);
}

void dialog_theme_decorate(const struct dialog_theme *_Nonnull theme, HWND _Nonnull dialog)
{
    /* 地に対して白い字が立つなら暗い面なので、題の帯も暗くする（ADR 0017 と同じ判定）。 */
    BOOL dark = folio_palette_ink(theme->palette.window) == RGB(0xFF, 0xFF, 0xFF) ? TRUE : FALSE;
    DwmSetWindowAttribute(dialog, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof dark);
    COLORREF caption = theme->palette.window;
    DwmSetWindowAttribute(dialog, DWMWA_CAPTION_COLOR, &caption, sizeof caption);
    COLORREF text = theme->palette.current_text;
    DwmSetWindowAttribute(dialog, DWMWA_TEXT_COLOR, &text, sizeof text);
}
