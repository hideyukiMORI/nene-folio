#include "ui_part.h"

#include "icon_paint.h"

static int scaled(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, 96);
}

static void fill(HDC device, RECT bounds, COLORREF color)
{
    SetDCBrushColor(device, color);
    FillRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
}

/* 内側へ width の太さの直角の枠。 */
static void frame(HDC device, RECT bounds, int width, COLORREF color)
{
    fill(device, (RECT){bounds.left, bounds.top, bounds.right, bounds.top + width}, color);
    fill(device, (RECT){bounds.left, bounds.bottom - width, bounds.right, bounds.bottom}, color);
    fill(device, (RECT){bounds.left, bounds.top, bounds.left + width, bounds.bottom}, color);
    fill(device, (RECT){bounds.right - width, bounds.top, bounds.right, bounds.bottom}, color);
}

/* 押下の塗りか、選択群の選択中の塗りか。どちらも selected_background＋selected_text。 */
static bool part_lit(const struct ui_part_paint *_Nonnull part)
{
    bool selected = part->style == UI_PART_STYLE_CHOICE && part->selected;
    return !part->disabled && (part->pressed || selected);
}

/* 静止時の字と絵の色（全値の switch・C-002）。 */
static COLORREF resting_ink(const struct folio_palette *_Nonnull palette, enum ui_part_style style)
{
    switch (style)
    {
    case UI_PART_STYLE_COMMAND:
    case UI_PART_STYLE_CHOICE:
        return palette->current_text;
    case UI_PART_STYLE_PRIMARY:
        return palette->unsaved_ink;
    case UI_PART_STYLE_ICON:
        return palette->header_text;
    }
    return palette->current_text;
}

static COLORREF part_ink(const struct folio_palette *_Nonnull palette,
                         const struct ui_part_paint *_Nonnull part)
{
    if (part->disabled)
    {
        return palette->border; /* dialog_theme の既存規則 */
    }
    return part_lit(part) ? palette->selected_text : resting_ink(palette, part->style);
}

/* 区切りの 1px を除いた、部品の中身の矩形。 */
static RECT part_body(const struct ui_part_paint *_Nonnull part)
{
    RECT body = part->bounds;
    body.left += part->leading_rule ? 1 : 0;
    return body;
}

static void draw_selection_bar(HDC device, const struct folio_palette *_Nonnull palette,
                               const struct ui_part_paint *_Nonnull part)
{
    RECT bar = part_body(part);
    bar.top = bar.bottom - ui_part_focus_width(part->dpi);
    fill(device, bar, part->disabled ? palette->border : palette->selected_text);
}

/* B 主操作の地。塗りが地に 3:1 届くテーマでは塗り、届かないテーマでは同じ色の 2px の枠
 * （theme_contrast_unsaved・#349）。 */
static void draw_primary(HDC device, const struct folio_palette *_Nonnull palette,
                         const struct ui_part_paint *_Nonnull part)
{
    if (palette->unsaved_filled)
    {
        fill(device, part_body(part), palette->unsaved_fill);
        return;
    }
    frame(device, part_body(part), ui_part_focus_width(part->dpi), palette->unsaved_fill);
}

static void draw_surface(HDC device, const struct folio_palette *_Nonnull palette,
                         const struct ui_part_paint *_Nonnull part)
{
    if (part_lit(part))
    {
        fill(device, part_body(part), palette->selected_background);
    }
    else if (part->style == UI_PART_STYLE_PRIMARY)
    {
        draw_primary(device, palette, part);
    }
    if (part->style == UI_PART_STYLE_CHOICE && part->selected)
    {
        draw_selection_bar(device, palette, part);
    }
    if (part->leading_rule)
    {
        fill(
            device,
            (RECT){part->bounds.left, part->bounds.top, part->bounds.left + 1, part->bounds.bottom},
            palette->border);
    }
    if (part->style == UI_PART_STYLE_COMMAND && !part->grouped)
    {
        frame(device, part->bounds, 1,
              palette->contrast == FOLIO_CONTRAST_HIGH ? palette->category_text
                                                       : palette->dialog_border);
    }
}

/* hover は地を塗らず内側 1px の枠（ADR0060 の決定 3）。押下中・選択中・無効では描かない。
 * B 主操作は塗りの上なので、枠の代わりに下辺へ focus と同じ太さの chip_text の棒を描く。 */
static void draw_hover(HDC device, const struct folio_palette *_Nonnull palette,
                       const struct ui_part_paint *_Nonnull part)
{
    if (!part->hovered || part->disabled || part_lit(part))
    {
        return;
    }
    RECT body = part_body(part);
    if (part->style == UI_PART_STYLE_PRIMARY)
    {
        body.top = body.bottom - ui_part_focus_width(part->dpi);
        fill(device, body, palette->unsaved_ink);
        return;
    }
    if (part->style == UI_PART_STYLE_COMMAND && !part->grouped)
    {
        InflateRect(&body, -1, -1);
    }
    frame(device, body, 1, palette->hover_line);
}

static COLORREF item_focus_color(const struct folio_palette *_Nonnull palette,
                                 const struct ui_part_paint *_Nonnull part)
{
    if (part_lit(part))
    {
        return palette->focus_selected_line;
    }
    return part->style == UI_PART_STYLE_PRIMARY ? palette->focus_primary_line
                                                : palette->focus_item_line;
}

/* focus は 2px の実線。単独部品は 1px 外側、群の項目は内側に、その地の上の字の色で描く。 */
static void draw_focus(HDC device, const struct folio_palette *_Nonnull palette,
                       const struct ui_part_paint *_Nonnull part)
{
    if (!part->focused)
    {
        return;
    }
    int width = ui_part_focus_width(part->dpi);
    if (part->grouped || part->focus_inside)
    {
        frame(device, part_body(part), width, item_focus_color(palette, part));
        return;
    }
    RECT ring = part->bounds;
    int reach = ui_part_focus_reach(part->dpi);
    InflateRect(&ring, reach, reach);
    frame(device, ring, width, palette->focus_line);
}

static RECT icon_box(const struct ui_part_paint *_Nonnull part, RECT body)
{
    int size = scaled(part->style == UI_PART_STYLE_ICON ? ui_part_glyph : ui_part_icon, part->dpi);
    int top = (body.top + body.bottom - size) / 2;
    int left = part->labeled ? body.left + scaled(ui_part_padding, part->dpi)
                             : (body.left + body.right - size) / 2;
    return (RECT){left, top, left + size, top + size};
}

static RECT label_rect(const struct ui_part_paint *_Nonnull part, RECT body)
{
    if (part->style == UI_PART_STYLE_CHOICE)
    {
        return body;
    }
    /* 幅は字の実測から決めるので、右の余白は字の測り誤差の逃げに使い、省略記号の境界にしない。 */
    RECT label = body;
    label.left += scaled(ui_part_padding, part->dpi);
    label.left += part->has_icon ? scaled(ui_part_icon + ui_part_icon_gap, part->dpi) : 0;
    return label;
}

struct ui_part_ink ui_part_draw(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                                const struct ui_part_paint *_Nonnull part)
{
    draw_surface(device, palette, part);
    draw_hover(device, palette, part);
    COLORREF ink = part_ink(palette, part);
    RECT body = part_body(part);
    if (part->has_icon)
    {
        icon_paint_fill(device, icon_box(part, body), part->icon, ink);
    }
    draw_focus(device, palette, part);
    return (struct ui_part_ink){.text = ink, .label = label_rect(part, body)};
}

void ui_part_frame_group(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                         RECT bounds)
{
    frame(device, bounds, 1, palette->border);
}

RECT ui_part_field_edit(RECT box, int text_height, UINT dpi)
{
    int ring = ui_part_focus_width(dpi);
    InflateRect(&box, -ring, -ring);
    int room = box.bottom - box.top;
    if (text_height <= 0 || text_height >= room)
    {
        return box;
    }
    int top = box.top + (room - text_height) / 2;
    return (RECT){box.left, top, box.right, top + text_height};
}

void ui_part_field_margins(HWND _Nonnull edit, UINT dpi, int right_room)
{
    int padding = scaled(ui_part_field_padding, dpi) - ui_part_focus_width(dpi);
    int right = right_room > padding ? right_room : padding;
    SendMessageW(edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(padding, right));
}

int ui_part_field_text_height(HWND _Nonnull edit)
{
    HDC device = GetDC(edit);
    if (device == nullptr)
    {
        return 0;
    }
    HGDIOBJ previous = SelectObject(device, (HGDIOBJ)SendMessageW(edit, WM_GETFONT, 0, 0));
    TEXTMETRICW metrics = {0};
    bool measured = GetTextMetricsW(device, &metrics) != 0;
    SelectObject(device, previous);
    ReleaseDC(edit, device);
    return measured ? (int)metrics.tmHeight : 0;
}

void ui_part_fill_field(HDC _Nonnull device, RECT box, COLORREF background)
{
    fill(device, box, background);
}

void ui_part_draw_field(HDC _Nonnull device, const struct folio_palette *_Nonnull palette, RECT box,
                        int focus)
{
    frame(device, box, focus > 0 ? focus : 1,
          focus > 0 ? palette->field_focus_line : palette->field_line);
}

int ui_part_focus_width(UINT dpi)
{
    return scaled(2, dpi);
}

int ui_part_focus_reach(UINT dpi)
{
    return scaled(1, dpi) + ui_part_focus_width(dpi);
}

/* 字の実測を足した幅を論理 px の 4 の倍数へ切り上げる（ADR0060 の決定 2）。 */
int ui_part_command_width(int text, bool icon, UINT dpi)
{
    int mark = icon ? scaled(ui_part_icon + ui_part_icon_gap, dpi) : 0;
    int width = scaled(ui_part_padding * 2, dpi) + mark + text;
    int logical = (width * 96 + (int)dpi - 1) / (int)dpi;
    return scaled((logical + 3) / 4 * 4, dpi);
}

RECT ui_part_surface_rect(RECT bounds, UINT dpi, enum ui_part_surface_region region)
{
    int title = scaled(ui_part_surface_title_height, dpi);
    int edge = scaled(ui_part_edge, dpi);
    int close_edge = scaled(8, dpi);
    int close = scaled(ui_part_height, dpi);
    switch (region)
    {
    case UI_PART_SURFACE_TITLE:
        return (RECT){bounds.left, bounds.top, bounds.right, bounds.top + title};
    case UI_PART_SURFACE_TITLE_TEXT:
        return (RECT){bounds.left + edge, bounds.top, bounds.right - close - close_edge * 2,
                      bounds.top + title};
    case UI_PART_SURFACE_CLOSE:
        return (RECT){bounds.right - close_edge - close, bounds.top + close_edge,
                      bounds.right - close_edge, bounds.top + close_edge + close};
    case UI_PART_SURFACE_CONTENT:
        return (RECT){bounds.left + edge, bounds.top + title + edge, bounds.right - edge,
                      bounds.bottom - edge};
    }
    return bounds;
}

void ui_part_draw_surface(HDC _Nonnull device, const struct folio_palette *_Nonnull palette,
                          RECT bounds)
{
    fill(device, bounds, palette->dialog_surface);
    frame(device, bounds, 1, palette->dialog_border);
}

struct ui_part_ink ui_part_draw_menu(HDC _Nonnull device,
                                     const struct folio_palette *_Nonnull palette,
                                     const struct ui_part_menu_paint *_Nonnull part)
{
    RECT body = part->bounds;
    body.left += scaled(4, part->dpi);
    body.right -= scaled(4, part->dpi);
    bool selected = part->enabled && part->selected;
    if (selected)
    {
        fill(device, body, palette->selected_background);
        body.right = body.left + ui_part_focus_width(part->dpi);
        fill(device, body, palette->focus_selected_line);
    }
    RECT label = part->bounds;
    label.left += scaled(16, part->dpi);
    label.right -= scaled(16, part->dpi);
    COLORREF text = part->enabled ? palette->current_text : palette->header_text;
    return (struct ui_part_ink){.text = selected ? palette->selected_text : text, .label = label};
}
