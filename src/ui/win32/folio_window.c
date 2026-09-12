#include "folio_window.h"

#include "breadcrumb_layout.h"
#include "command_row.h"
#include "command_surface_mode.h"
#include "drawer_window.h"
#include "failure_box.h"
#include "folio_command.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "folio_step.h"
#include "note_pane.h"
#include "note_ref.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <dwmapi.h>
#include <richedit.h>
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
    HWND _Nullable command_layer;
    HWND _Nullable command_input;
    WNDPROC _Nullable command_input_original;
    HWND _Nullable command_return_focus;
    HBRUSH _Nullable command_brush;
    enum command_surface_mode command_surface;
    size_t command_selection;
    bool command_composing;
    bool command_unknown;
    enum folio_state_outcome command_failure;
    bool pending_g; /* 直前の文字の鍵が g だった（gg の 2 打・ADR 0013 の決定 2） */
};

static const wchar_t class_name[] = L"NeNeFolioWindow";
static const wchar_t command_layer_class[] = L"NeNeFolioCommandLayer";
static const wchar_t mono_face[] = L"Consolas";
static const wchar_t view_label[] = L"閲覧";
static const wchar_t edit_label[] = L"編集";
static const wchar_t edit_class[] = L"EDIT";
static const char *_Nonnull const command_shortcuts[] = {
    "一覧  ↑↓ 選択 / Enter 実行 / Esc 戻る",
    "INDEX・本文  Ctrl+P 一覧 / Ctrl+S 保存",
    "INDEX  ? ヘルプ / : コマンド入力",
    "INDEX  j/k 次/前 / gg/G 先頭/末尾",
    "INDEX  h/l 折畳/展開 / Enter 本文",
    "INDEX  ↑↓ / PgUp/PgDn スクロール",
    "本文  Esc INDEXへ（編集中は保存・モード維持）",
};

/* Ctrl+S が WM_CHAR で届く制御文字（GetKeyState を読まない・ARC-007）。 */
constexpr WPARAM store_character = 0x13;
constexpr WPARAM palette_character = 0x10;
constexpr int command_control_id = 1;
constexpr size_t command_input_capacity = 256;

/* 96 DPI での寸法（デザイン「案2 堅」）。 */
constexpr int base_dpi = 96;
constexpr int base_width = 960;
constexpr int base_height = 640;
/* 最小の大きさ。ドロワー・番号・パンくず・札 2 つが収まる（ADR 0011 の決定 3）。 */
constexpr int base_min_width = 560;
constexpr int base_min_height = 360;
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
constexpr int base_chip_gap = 8;
constexpr int base_chip_radius = 3;
constexpr int base_mono_font = 11;
constexpr int base_tracking = 1;
/* 省略しても残すノート名の幅（ADR 0011 の決定 4）。 */
constexpr int base_breadcrumb_note = 48;
constexpr int base_breadcrumb_height = 24;
constexpr int base_breadcrumb_padding = 8;
constexpr int base_breadcrumb_tip = 10;
constexpr int base_breadcrumb_compact_width = 320;
constexpr int base_breadcrumb_compact_padding = 4;
constexpr int base_breadcrumb_compact_tip = 6;
constexpr int base_close_size = 24;
constexpr int base_close_margin = 16;
constexpr int base_close_glyph_inset = 4;
constexpr int base_command_gap = 8;
constexpr int base_command_input_height = 28;
constexpr int base_command_palette_width = 460;
constexpr int base_command_palette_margin = 32;
constexpr int base_command_palette_top = 76;
constexpr int base_command_palette_compact_top = 40;
constexpr int base_command_palette_padding = 16;
constexpr int base_command_row_height = 32;
constexpr int base_command_status_height = 24;
constexpr int base_command_help_row_height = 14;
constexpr int base_command_palette_edge = 4;
constexpr int base_ex_height = 32;
constexpr int base_command_row_padding = 12;
constexpr int base_command_alias_room = 96;
constexpr int base_command_alias_gap = 12;
constexpr int base_command_input_vertical_inset = 2;

/* DWM の窓の角と縁（Windows 11）。dwmapi.h の版によっては未定義なので数値で持つ。 */
constexpr DWORD attribute_corner_preference = 33;
constexpr DWORD attribute_border_color = 34;
constexpr DWORD corner_round_small = 3;

static void execute_command(struct folio_window *_Nonnull self, enum folio_command command);
static void close_command_surface(struct folio_window *_Nonnull self);
static void show_command_palette(struct folio_window *_Nonnull self);
static LRESULT CALLBACK command_input_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam);
static LRESULT CALLBACK command_layer_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam);

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
    if (self->command_input != nullptr)
    {
        SendMessageW(self->command_input, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    }
}

static RECT command_palette_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int margin = scale(base_command_palette_margin, dpi);
    int available = client.right - margin * 2;
    int width = scale(base_command_palette_width, dpi);
    if (width > available)
    {
        width = available;
    }
    int rows = (int)folio_command_count();
    int height =
        scale(base_command_palette_padding * 2 + base_command_input_height + base_command_gap +
                  base_command_row_height * rows + base_command_status_height,
              dpi);
    height += scale(base_command_help_row_height, dpi) *
              (int)(sizeof command_shortcuts / sizeof command_shortcuts[0]);
    int top = scale(client.bottom < scale(480, dpi) ? base_command_palette_compact_top
                                                    : base_command_palette_top,
                    dpi);
    int latest_top = client.bottom - height - scale(base_command_palette_edge, dpi);
    if (top > latest_top)
    {
        top = latest_top;
    }
    RECT bounds = {(client.right - width) / 2, top, (client.right + width) / 2, top + height};
    return bounds;
}

static RECT command_ex_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int status = self->command_failure != FOLIO_STATE_READY || self->command_unknown
                     ? scale(base_command_status_height, dpi)
                     : 0;
    RECT bounds = {0, client.bottom - scale(base_ex_height, dpi) - status, client.right,
                   client.bottom};
    return bounds;
}

static RECT command_input_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    if (self->command_surface == COMMAND_SURFACE_EX)
    {
        int inset = scale(base_command_gap, dpi);
        int vertical = scale(base_command_input_vertical_inset, dpi);
        RECT input = {bounds.left + inset, bounds.bottom - scale(base_ex_height, dpi) + vertical,
                      bounds.right - inset, bounds.bottom - vertical};
        return input;
    }
    int inset = scale(base_command_palette_padding, dpi);
    RECT input = {inset, inset, bounds.right - inset,
                  inset + scale(base_command_input_height, dpi)};
    return input;
}

static void arrange_command_input(const struct folio_window *_Nonnull self)
{
    if (self->command_layer == nullptr || self->command_input == nullptr)
    {
        return;
    }
    bool visible = self->command_surface == COMMAND_SURFACE_EX ||
                   self->command_surface == COMMAND_SURFACE_PALETTE;
    if (!visible)
    {
        ShowWindow(self->command_layer, SW_HIDE);
        return;
    }
    RECT layer = self->command_surface == COMMAND_SURFACE_EX ? command_ex_rect(self)
                                                             : command_palette_rect(self);
    MoveWindow(self->command_layer, layer.left, layer.top, layer.right - layer.left,
               layer.bottom - layer.top, TRUE);
    RECT bounds = command_input_rect(self);
    MoveWindow(self->command_input, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
    ShowWindow(self->command_input, SW_SHOW);
    ShowWindow(self->command_layer, SW_SHOW);
    BringWindowToTop(self->command_layer);
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
    arrange_command_input(self);
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

/* UTF-8 の 1 行を測る。測れなければ 0。 */
static int measure_utf8(HDC device, const char *_Nonnull text)
{
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(text, strlen(text), &wide) != UTF16_TEXT_CONVERTED)
    {
        return 0;
    }
    RECT measured = {0, 0, 0, 0};
    DrawTextW(device, utf16_text_units(wide), (int)utf16_text_length(wide), &measured,
              DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    utf16_text_destroy(wide);
    return measured.right - measured.left;
}

/* UTF-8 を 1 行で描く。bounds に収まらなければ末尾を省略記号にする（ADR 0011 の決定 4）。 */
static void draw_utf8(HDC device, const char *_Nonnull text, RECT bounds)
{
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(text, strlen(text), &wide) != UTF16_TEXT_CONVERTED)
    {
        return;
    }
    DrawTextW(device, utf16_text_units(wide), (int)utf16_text_length(wide), &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    utf16_text_destroy(wide);
}

/* 右ペインの頭の帯（窓を掴んで動かせる帯）。 */
static RECT caption_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT caption = {scale(base_drawer_width, dpi) + scale(base_caption_indent, dpi), 0,
                    client.right, scale(base_caption_height, dpi)};
    return caption;
}

static RECT close_rect(const struct folio_window *_Nonnull self)
{
    RECT caption = caption_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int size = scale(base_close_size, dpi);
    int margin = scale(base_close_margin, dpi);
    int middle = (caption.top + caption.bottom) / 2;
    RECT bounds = {caption.right - margin - size, middle - size / 2, caption.right - margin,
                   middle - size / 2 + size};
    return bounds;
}

/* 2桁の番号。幅測定と描画で同じ文字列を使う。 */
static void ordinal_label(size_t ordinal, char *_Nonnull label)
{
    label[0] = (char)('0' + (ordinal / 10) % 10);
    label[1] = (char)('0' + ordinal % 10);
    label[2] = '\0';
}

/* カテゴリ名とノート名の幅を budget に収める。ノート名から先に削り、それでも足りなければ
 * カテゴリ名も削る（ADR 0011 の決定 4）。入る値は自然な幅、出る値は割り当てた幅。 */
static void breadcrumb_room(int budget, int minimum, int *_Nonnull category, int *_Nonnull note)
{
    if (*category + *note <= budget)
    {
        return;
    }
    if (budget - *category >= minimum)
    {
        *note = budget - *category;
        return;
    }
    *note = minimum < budget ? minimum : budget;
    *category = budget - *note;
}

static const wchar_t *_Nonnull chip_label(enum pane_mode chip)
{
    switch (chip)
    {
    case PANE_MODE_VIEW:
        return view_label;
    case PANE_MODE_EDIT:
        return edit_label;
    }
    return view_label;
}

/* 札 1 つの幅（左右の余白込み）。等幅フォントを選んだ device で測る。 */
static int chip_width(const struct folio_window *_Nonnull self, HDC device, enum pane_mode chip)
{
    RECT measured = caption_rect(self);
    DrawTextW(device, chip_label(chip), -1, &measured, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    return measured.right - measured.left +
           scale(base_chip_padding, GetDpiForWindow(self->handle)) * 2;
}

/* 札の矩形。右端が「編集」、その左が「閲覧」。描画と当たり判定はこの 1 か所を共有する。 */
static RECT chip_rect(const struct folio_window *_Nonnull self, HDC device, enum pane_mode chip)
{
    RECT caption = caption_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int height = scale(base_chip_height, dpi);
    int middle = (caption.top + caption.bottom) / 2;
    int right = close_rect(self).left - scale(base_chip_gap, dpi);
    switch (chip)
    {
    case PANE_MODE_VIEW:
        right -= chip_width(self, device, PANE_MODE_EDIT) + scale(base_chip_gap, dpi);
        break;
    case PANE_MODE_EDIT:
        break;
    }
    RECT bounds = {right - chip_width(self, device, chip), middle - height / 2, right,
                   middle - height / 2 + height};
    return bounds;
}

static struct breadcrumb_layout breadcrumb_cells(const struct folio_window *_Nonnull self,
                                                 HDC device, struct pane_title_view title,
                                                 RECT caption)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int right = chip_rect(self, device, PANE_MODE_VIEW).left - scale(base_caption_gap, dpi);
    int available = right - caption.left;
    bool compact = available < scale(base_breadcrumb_compact_width, dpi);
    int padding = scale(compact ? base_breadcrumb_compact_padding : base_breadcrumb_padding, dpi);
    int tip = scale(compact ? base_breadcrumb_compact_tip : base_breadcrumb_tip, dpi);
    char digits[3];
    ordinal_label(title.ordinal, digits);
    int ordinal = measure_utf8(device, digits) + padding * 2;
    int category = measure_utf8(device, title.category);
    int note = measure_utf8(device, title.note);
    int budget = available - ordinal - padding * 4 - tip * 2;
    breadcrumb_room(budget < 0 ? 0 : budget, scale(base_breadcrumb_note, dpi), &category, &note);
    int category_width = category > 0 ? category + padding * 2 + tip : 0;
    if (category_width == 0)
    {
        int remaining = available - ordinal - padding * 2 - tip;
        note = remaining > 0 ? remaining : 0;
    }
    int height = scale(base_breadcrumb_height, dpi);
    int top = (caption.top + caption.bottom - height) / 2;
    RECT number = {caption.left, top, caption.left + ordinal, top + height};
    RECT group = {number.right, top, number.right + category_width, top + height};
    RECT name = {group.right, top, group.right + note + padding * 2 + tip, top + height};
    return (struct breadcrumb_layout){number, group, name, padding, tip};
}

/* 右へ矢じりを延ばす同じ多角形。左の区画を後から重ね、右の区画へつなぐ。 */
static void draw_breadcrumb_segment(HDC device, RECT bounds, int tip, COLORREF background)
{
    POINT points[] = {{bounds.left, bounds.top},
                      {bounds.right, bounds.top},
                      {bounds.right + tip, (bounds.top + bounds.bottom) / 2},
                      {bounds.right, bounds.bottom},
                      {bounds.left, bounds.bottom}};
    HGDIOBJ brush = SelectObject(device, GetStockObject(DC_BRUSH));
    HGDIOBJ pen = SelectObject(device, GetStockObject(NULL_PEN));
    SetDCBrushColor(device, background);
    Polygon(device, points, (int)(sizeof points / sizeof points[0]));
    SelectObject(device, pen);
    SelectObject(device, brush);
}

static void draw_breadcrumb_label(HDC device, const char *_Nonnull text, RECT bounds, int inset)
{
    bounds.left += inset;
    draw_utf8(device, text, bounds);
}

/* 色面と文字は既存の幅割当を共有し、右の操作領域へ出さない（ADR 0017）。 */
static void draw_breadcrumb(const struct folio_window *_Nonnull self, HDC device, RECT caption)
{
    struct pane_title_view title = folio_state_pane_title(self->state);
    if (!title.any)
    {
        return;
    }
    int saved = SaveDC(device);
    if (saved == 0)
    {
        return;
    }
    struct breadcrumb_layout cells = breadcrumb_cells(self, device, title, caption);
    int right = chip_rect(self, device, PANE_MODE_VIEW).left -
                scale(base_caption_gap, GetDpiForWindow(self->handle));
    IntersectClipRect(device, caption.left, caption.top, right, caption.bottom);
    if (cells.category.right > cells.category.left)
    {
        draw_breadcrumb_segment(device, cells.category, cells.tip,
                                self->palette.breadcrumb_background);
        SetTextColor(device, self->palette.breadcrumb_text);
        draw_breadcrumb_label(device, title.category, cells.category, cells.padding + cells.tip);
    }
    COLORREF color = RGB(title.color.red, title.color.green, title.color.blue);
    draw_breadcrumb_segment(device, cells.ordinal, cells.tip, color);
    SetTextColor(device, folio_palette_ink(color));
    char digits[3];
    ordinal_label(title.ordinal, digits);
    draw_breadcrumb_label(device, digits, cells.ordinal, cells.padding);
    SetTextColor(device, self->palette.current_text);
    draw_breadcrumb_label(device, title.note, cells.note, cells.padding + cells.tip);
    RestoreDC(device, saved);
}

/* 有効な側だけ面を塗り、無効な側は頭の文字色で描く（ADR 0006 の決定 7）。 */
static void draw_chip(const struct folio_window *_Nonnull self, HDC device, enum pane_mode chip)
{
    RECT bounds = chip_rect(self, device, chip);
    UINT dpi = GetDpiForWindow(self->handle);
    if (chip == folio_state_pane_mode(self->state))
    {
        HBRUSH brush = CreateSolidBrush(self->palette.chip_background);
        HPEN pen = CreatePen(PS_NULL, 0, 0);
        HGDIOBJ old_brush = SelectObject(device, brush);
        HGDIOBJ old_pen = SelectObject(device, pen);
        int radius = scale(base_chip_radius, dpi) * 2;
        RoundRect(device, bounds.left, bounds.top, bounds.right, bounds.bottom, radius, radius);
        SelectObject(device, old_pen);
        SelectObject(device, old_brush);
        DeleteObject(pen);
        DeleteObject(brush);
        SetTextColor(device, self->palette.chip_text);
    }
    else
    {
        SetTextColor(device, self->palette.header_text);
    }
    DrawTextW(device, chip_label(chip), -1, &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
}

static void draw_close(const struct folio_window *_Nonnull self, HDC device)
{
    RECT bounds = close_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int inset = scale(base_close_glyph_inset, dpi);
    HPEN pen = CreatePen(PS_SOLID, scale(1, dpi), self->palette.header_text);
    HGDIOBJ previous = SelectObject(device, pen);
    MoveToEx(device, bounds.left + inset, bounds.top + inset, nullptr);
    LineTo(device, bounds.right - inset, bounds.bottom - inset);
    MoveToEx(device, bounds.right - inset, bounds.top + inset, nullptr);
    LineTo(device, bounds.left + inset, bounds.bottom - inset);
    SelectObject(device, previous);
    DeleteObject(pen);
}

static enum utf8_text_outcome command_query(const struct folio_window *_Nonnull self,
                                            struct utf8_text *_Nullable *_Nonnull out)
{
    wchar_t units[command_input_capacity];
    int count = self->command_input == nullptr
                    ? 0
                    : GetWindowTextW(self->command_input, units, (int)command_input_capacity);
    return utf8_text_create((const char16_t *)units, count > 0 ? (size_t)count : 0, out);
}

static bool command_at_query(const struct utf8_text *_Nonnull query, size_t wanted,
                             enum folio_command *_Nonnull out)
{
    size_t visible = 0;
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        if (!folio_command_matches(command, utf8_text_bytes(query), utf8_text_length(query)))
        {
            continue;
        }
        if (visible == wanted)
        {
            *out = command;
            return true;
        }
        visible += 1;
    }
    return false;
}

static size_t command_matches_count(const struct utf8_text *_Nonnull query)
{
    size_t count = 0;
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        count +=
            folio_command_matches(command, utf8_text_bytes(query), utf8_text_length(query)) ? 1 : 0;
    }
    return count;
}

static void draw_aliases(const struct folio_window *_Nonnull self, HDC device,
                         enum folio_command command, RECT bounds)
{
    SetTextColor(device, self->palette.header_text);
    for (size_t index = 0; index < folio_command_alias_count(command); ++index)
    {
        const char *_Nonnull alias = folio_command_alias(command, index);
        int width = measure_utf8(device, alias);
        bounds.left -= width;
        draw_utf8(device, alias, bounds);
        bounds.right = bounds.left - scale(base_command_alias_gap, GetDpiForWindow(self->handle));
        bounds.left = bounds.right;
    }
}

static void draw_command_row(const struct folio_window *_Nonnull self, HDC device,
                             struct command_row row)
{
    if (row.selected)
    {
        HBRUSH brush = CreateSolidBrush(self->palette.selected_background);
        FillRect(device, &row.bounds, brush);
        DeleteObject(brush);
    }
    RECT label = row.bounds;
    UINT dpi = GetDpiForWindow(self->handle);
    label.left += scale(base_command_row_padding, dpi);
    label.right -= scale(base_command_alias_room, dpi);
    SetTextColor(device, row.selected ? self->palette.selected_text : self->palette.current_text);
    draw_utf8(device, folio_command_label(row.command), label);
    RECT aliases = row.bounds;
    aliases.right -= scale(base_command_row_padding, dpi);
    aliases.left = aliases.right;
    draw_aliases(self, device, row.command, aliases);
}

static void draw_command_status(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    const char *_Nonnull line = "";
    if (self->command_failure != FOLIO_STATE_READY)
    {
        line = folio_state_failure_line(self->command_failure);
    }
    else if (self->command_unknown)
    {
        line = "一致する操作がありません。";
    }
    if (line[0] == '\0')
    {
        return;
    }
    SetTextColor(device, self->palette.header_text);
    draw_utf8(device, line, bounds);
}

static void draw_command_shortcuts(const struct folio_window *_Nonnull self, HDC device,
                                   RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int row_height = scale(base_command_help_row_height, dpi);
    int rows = (int)(sizeof command_shortcuts / sizeof command_shortcuts[0]);
    bounds.bottom -= scale(base_command_status_height + base_command_palette_padding, dpi);
    bounds.top = bounds.bottom - row_height * rows;
    bounds.left += scale(base_command_palette_padding, dpi);
    bounds.right -= scale(base_command_palette_padding, dpi);
    SetTextColor(device, self->palette.current_text);
    for (int index = 0; index < rows; ++index)
    {
        RECT row = {bounds.left, bounds.top, bounds.right, bounds.top + row_height};
        draw_utf8(device, command_shortcuts[index], row);
        bounds.top = row.bottom;
    }
}

static void draw_palette_rows(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    int padding = scale(base_command_palette_padding, dpi);
    int top = bounds.top + padding + scale(base_command_input_height + base_command_gap, dpi);
    size_t visible = 0;
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        if (!folio_command_matches(command, utf8_text_bytes(query), utf8_text_length(query)))
        {
            continue;
        }
        RECT row = {bounds.left + padding, top, bounds.right - padding,
                    top + scale(base_command_row_height, dpi)};
        draw_command_row(self, device,
                         (struct command_row){row, command, visible == self->command_selection});
        top = row.bottom;
        visible += 1;
    }
    RECT status = {bounds.left + padding, bounds.bottom - scale(base_command_status_height, dpi),
                   bounds.right - padding, bounds.bottom};
    draw_command_status(self, device, status);
    draw_command_shortcuts(self, device, bounds);
    utf8_text_destroy(query);
}

static void draw_command_surface(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    if (self->command_brush == nullptr)
    {
        return;
    }
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
        return;
    case COMMAND_SURFACE_EX:
    {
        FillRect(device, &bounds, self->command_brush);
        RECT input = command_input_rect(self);
        RECT status = {
            bounds.left + scale(base_command_gap, GetDpiForWindow(self->handle)), bounds.top,
            bounds.right - scale(base_command_gap, GetDpiForWindow(self->handle)), input.top};
        draw_command_status(self, device, status);
        return;
    }
    case COMMAND_SURFACE_PALETTE:
        FillRect(device, &bounds, self->command_brush);
        draw_palette_rows(self, device, bounds);
        return;
    }
}

/* 点が札の上なら true。当たり判定は描画と同じ chip_rect を使う（ADR 0006 の決定 7）。 */
static bool chip_at(const struct folio_window *_Nonnull self, POINT point,
                    enum pane_mode *_Nonnull chip)
{
    HDC device = GetDC(self->handle);
    if (device == nullptr)
    {
        return false;
    }
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, GetDpiForWindow(self->handle)));
    RECT view = chip_rect(self, device, PANE_MODE_VIEW);
    RECT edit = chip_rect(self, device, PANE_MODE_EDIT);
    SetTextCharacterExtra(device, 0);
    SelectObject(device, previous);
    ReleaseDC(self->handle, device);
    *chip = PtInRect(&view, point) ? PANE_MODE_VIEW : PANE_MODE_EDIT;
    return PtInRect(&view, point) || PtInRect(&edit, point);
}

/* 縁と札を先に見て、残りの頭の帯を掴んで動かせるようにする。 */
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
    POINT client = point;
    ScreenToClient(self->handle, &client);
    RECT close = close_rect(self);
    if (PtInRect(&close, client))
    {
        return HTCLIENT;
    }
    enum pane_mode chip = PANE_MODE_VIEW;
    if (chip_at(self, client, &chip))
    {
        return HTCLIENT;
    }
    bool in_caption = point.x >= window.left + scale(base_drawer_width, dpi) &&
                      point.y < window.top + scale(base_caption_height, dpi);
    return in_caption ? HTCAPTION : HTCLIENT;
}

/* 右ペインの地と頭。本文は note_pane が持つ。 */
static void draw_pane(const struct folio_window *_Nonnull self, HDC device, RECT client)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT pane = {scale(base_drawer_width, dpi), 0, client.right, client.bottom};
    HBRUSH brush = CreateSolidBrush(self->palette.pane);
    FillRect(device, &pane, brush);
    DeleteObject(brush);
    SetBkMode(device, TRANSPARENT);
    SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    draw_breadcrumb(self, device, caption_rect(self));
    draw_chip(self, device, PANE_MODE_VIEW);
    draw_chip(self, device, PANE_MODE_EDIT);
    draw_close(self, device);
    SetTextCharacterExtra(device, 0);
}

static void paint_pane(struct folio_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC target = BeginPaint(self->handle, &painting);
    RECT client;
    GetClientRect(self->handle, &client);
    HDC memory = CreateCompatibleDC(target);
    HBITMAP surface =
        memory == nullptr ? nullptr : CreateCompatibleBitmap(target, client.right, client.bottom);
    if (surface != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, surface);
        draw_pane(self, memory, client);
        int left = scale(base_drawer_width, GetDpiForWindow(self->handle));
        BitBlt(target, left, 0, client.right - left, client.bottom, memory, left, 0, SRCCOPY);
        SelectObject(memory, previous);
        DeleteObject(surface);
    }
    if (memory != nullptr)
    {
        DeleteDC(memory);
    }
    EndPaint(self->handle, &painting);
}

/* 編集中の本文を UI から取り出す。取り出せない理由は 1 つに畳む。 */
static enum folio_state_outcome take_text(const struct folio_window *_Nonnull self,
                                          const char16_t *_Nonnull *_Nonnull units,
                                          size_t *_Nonnull count)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    switch (note_pane_text(self->pane, units, count))
    {
    case NOTE_PANE_TEXT_TAKEN:
        return FOLIO_STATE_READY;
    case NOTE_PANE_TEXT_UNAVAILABLE:
    case NOTE_PANE_TEXT_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_OUT_OF_MEMORY;
}

/* ドロワーへ描き直しを頼む。選択の印もフォーカスの印もドロワーが描く（ADR 0013 の決定 8）。 */
static void redraw_drawer(const struct folio_window *_Nonnull self)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    if (drawer != nullptr)
    {
        InvalidateRect(drawer, nullptr, FALSE);
    }
}

/* フォーカスを本文の区画へ移す（「編集」の札と Enter・ADR 0013 の決定 1 / 2）。 */
static void focus_pane(const struct folio_window *_Nonnull self)
{
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    if (pane != nullptr)
    {
        SetFocus(pane);
    }
}

/* 選択中のノートを、いまのモードのまま右ペインへ写す（ADR 0013 の決定 4 の (iii)）。
 * 編集中は本文を平文で流し込むだけで、フォーカスは動かさない。 */
static enum folio_state_outcome show_note(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        render_pane(self);
        return FOLIO_STATE_READY;
    }
    struct utf16_text *_Nullable wide = nullptr;
    if (self->pane == nullptr ||
        utf16_text_create(folio_state_pane_text(self->state),
                          folio_state_pane_text_length(self->state), &wide) != UTF16_TEXT_CONVERTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    note_pane_edit(self->pane, utf16_text_units(wide), utf16_text_length(wide));
    utf16_text_destroy(wide);
    InvalidateRect(self->handle, nullptr, FALSE);
    return FOLIO_STATE_READY;
}

/* 「編集」の札。編集モードへ入り、本文を平文で流してからフォーカスを本文へ渡す。 */
static void enter_edit(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = folio_state_begin_edit(self->state);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = show_note(self);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, outcome);
        return;
    }
    focus_pane(self);
}

/* 編集中なら保存して閲覧へ戻す。閲覧中なら何もしない（「閲覧」の札だけが使う）。 */
static enum folio_state_outcome flush_edit(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        return FOLIO_STATE_READY;
    }
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = take_text(self, &units, &count);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = folio_state_end_edit(self->state, units, count);
    }
    if (outcome == FOLIO_STATE_READY)
    {
        render_pane(self);
        /* 閲覧へ戻ったので、鍵が索引へ返るようにフォーカスを主窓へ（ADR 0013 の決定 1）。 */
        SetFocus(self->handle);
    }
    return outcome;
}

/* 編集中の本文を取り出して保存する。取り出せない理由も保存の失敗も 1 つの結果に写す。 */
static enum folio_state_outcome store_body(const struct folio_window *_Nonnull self)
{
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = take_text(self, &units, &count);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = folio_state_store_note(self->state, units, count);
    }
    return outcome;
}

static enum folio_state_outcome command_save(struct folio_window *_Nonnull self)
{
    struct note_ref selected = {.category = 0, .note = 0};
    if (!folio_state_selection(self->state, &selected))
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    return folio_state_pane_mode(self->state) == PANE_MODE_VIEW ? FOLIO_STATE_READY
                                                                : store_body(self);
}

static enum folio_state_outcome command_quit(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        return FOLIO_STATE_READY;
    }
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = take_text(self, &units, &count);
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = folio_state_note_changed(self->state, units, count, &changed);
    }
    if (outcome == FOLIO_STATE_READY && changed == FOLIO_NOTE_CHANGED)
    {
        return FOLIO_STATE_UNSAVED_CHANGES;
    }
    return outcome;
}

static void redraw_command_layer(const struct folio_window *_Nonnull self)
{
    if (self->command_layer != nullptr)
    {
        InvalidateRect(self->command_layer, nullptr, FALSE);
    }
}

static void command_failure(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    bool inline_failure =
        self->command_surface != COMMAND_SURFACE_CLOSED &&
        (outcome == FOLIO_STATE_NOTHING_SELECTED || outcome == FOLIO_STATE_UNSAVED_CHANGES);
    if (!inline_failure)
    {
        failure_box_show(self->handle, outcome);
        if (self->command_surface != COMMAND_SURFACE_CLOSED && self->command_input != nullptr)
        {
            SetFocus(self->command_input);
        }
        return;
    }
    self->command_failure = outcome;
    self->command_unknown = false;
    arrange_command_input(self);
    redraw_command_layer(self);
    if (self->command_input != nullptr)
    {
        SetFocus(self->command_input);
    }
}

static void open_command_surface(struct folio_window *_Nonnull self,
                                 enum command_surface_mode surface)
{
    if (self->command_input == nullptr)
    {
        return;
    }
    if (self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        if (self->command_surface == COMMAND_SURFACE_EX && surface == COMMAND_SURFACE_PALETTE)
        {
            show_command_palette(self);
            return;
        }
        SetFocus(self->command_input);
        return;
    }
    self->command_return_focus = GetFocus();
    self->command_surface = surface;
    self->command_selection = 0;
    self->command_unknown = false;
    self->command_failure = FOLIO_STATE_READY;
    SetWindowTextW(self->command_input, surface == COMMAND_SURFACE_EX ? L":" : L"");
    arrange_command_input(self);
    SetFocus(self->command_input);
    SendMessageW(self->command_input, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    redraw_command_layer(self);
}

static void hide_command_surface(struct folio_window *_Nonnull self)
{
    self->command_surface = COMMAND_SURFACE_CLOSED;
    self->command_return_focus = nullptr;
    self->command_selection = 0;
    self->command_unknown = false;
    self->command_failure = FOLIO_STATE_READY;
    arrange_command_input(self);
    InvalidateRect(self->handle, nullptr, FALSE);
}

static void close_command_surface(struct folio_window *_Nonnull self)
{
    HWND focus = self->command_return_focus;
    hide_command_surface(self);
    SetFocus(focus != nullptr && IsWindow(focus) ? focus : self->handle);
}

static void dismiss_command_surface(struct folio_window *_Nonnull self)
{
    hide_command_surface(self);
}

static void show_command_palette(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        return;
    }
    self->command_surface = COMMAND_SURFACE_PALETTE;
    self->command_selection = 0;
    self->command_unknown = false;
    self->command_failure = FOLIO_STATE_READY;
    if (self->command_input != nullptr)
    {
        SetWindowTextW(self->command_input, L"");
    }
    arrange_command_input(self);
    if (self->command_input != nullptr)
    {
        SetFocus(self->command_input);
    }
    redraw_command_layer(self);
}

static void execute_save_command(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = command_save(self);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    if (self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        close_command_surface(self);
    }
}

static void execute_quit_command(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = command_quit(self);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    DestroyWindow(self->handle);
}

static void execute_save_quit_command(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome =
        folio_state_pane_mode(self->state) == PANE_MODE_VIEW ? FOLIO_STATE_READY : store_body(self);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    DestroyWindow(self->handle);
}

/* Ex・パレット・既存入口が共有する唯一の HWND 操作 dispatcher（ADR 0016 の決定 2）。 */
static void execute_command(struct folio_window *_Nonnull self, enum folio_command command)
{
    switch (command)
    {
    case FOLIO_COMMAND_SAVE:
        execute_save_command(self);
        return;
    case FOLIO_COMMAND_QUIT:
        execute_quit_command(self);
        return;
    case FOLIO_COMMAND_SAVE_QUIT:
        execute_save_quit_command(self);
        return;
    case FOLIO_COMMAND_FORCE_QUIT:
        DestroyWindow(self->handle);
        return;
    case FOLIO_COMMAND_HELP:
        show_command_palette(self);
        return;
    }
}

/* Ctrl+S と本文の Esc。保存して編集モードのまま残る。失敗なら 1 行を出して false。 */
static bool store_edit(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = store_body(self);
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, outcome);
        return false;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    return true;
}

/* ノートを切り替える前の保存。閲覧中なら何もしない（ADR 0013 の決定 4 の (i)）。 */
static enum folio_state_outcome save_edit(const struct folio_window *_Nonnull self)
{
    return folio_state_pane_mode(self->state) == PANE_MODE_VIEW ? FOLIO_STATE_READY
                                                                : store_body(self);
}

/* 選択の意図が通ったら、同じモードで開き直して索引も描き直す（決定 4 の (iii) / (iv)）。 */
static enum folio_state_outcome opened(struct folio_window *_Nonnull self,
                                       enum folio_state_outcome selected)
{
    if (selected != FOLIO_STATE_READY)
    {
        return selected;
    }
    enum folio_state_outcome outcome = show_note(self);
    redraw_drawer(self);
    return outcome;
}

/* ノートを切り替える唯一の経路（ADR 0013 の決定 4）。保存できなければ選択は動かない。 */
static enum folio_state_outcome switch_note(struct folio_window *_Nonnull self, size_t category,
                                            size_t note)
{
    enum folio_state_outcome outcome = save_edit(self);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    return opened(self, folio_state_select_note(self->state, category, note));
}

/* 歩みの行き先がノート行か（ADR 0015 の決定 2）。カテゴリ行と端では保存も開き直しも要らない。 */
static bool steps_to_note(const struct folio_window *_Nonnull self, enum folio_step step)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    return folio_state_step_kind(self->state, step, &kind) && kind == FOLIO_CURSOR_NOTE;
}

/* 索引の鍵でカーソルを動かし、動いた行を見える位置へ寄せる（ADR 0015 の決定 2 / 6）。
 * カテゴリ行に止まるときは保存も開き直しもせず、描き直して寄せるだけ。 */
static void switch_adjacent(struct folio_window *_Nonnull self, enum folio_step step)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    if (steps_to_note(self, step))
    {
        outcome = save_edit(self);
        if (outcome == FOLIO_STATE_READY)
        {
            outcome = opened(self, folio_state_select_adjacent(self->state, step));
        }
    }
    else
    {
        outcome = folio_state_select_adjacent(self->state, step);
        redraw_drawer(self);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, outcome);
        return;
    }
    if (self->drawer != nullptr)
    {
        drawer_window_reveal_cursor(self->drawer);
    }
}

/* 展開で別のノートが選ばれたときだけ本文を開き直す（ADR 0015 の決定 3）。
 * 同じノートのままなら、編集中の本文を上書きしないために流し込まない。 */
static enum folio_state_outcome reopen_if_moved(struct folio_window *_Nonnull self, bool had,
                                                struct note_ref before)
{
    struct note_ref after = {.category = 0, .note = 0};
    if (!folio_state_selection(self->state, &after))
    {
        return FOLIO_STATE_READY;
    }
    if (had && after.category == before.category && after.note == before.note)
    {
        return FOLIO_STATE_READY;
    }
    return show_note(self);
}

/* h / l。カーソルの行のカテゴリを折り畳む／展開する（ADR 0015 の決定 3）。
 * カーソルがカテゴリ行のときの展開は中のノートを選び直すことがあるので、先に保存する
 * （ADR 0013 の決定 4 の (i)）。カーソルが移るので、最後は歩みと同じように見える位置へ寄せる。 */
static void expand_cursor(struct folio_window *_Nonnull self, bool expanded)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {.category = 0, .note = 0};
    if (!folio_state_cursor(self->state, &kind, &cursor))
    {
        return;
    }
    struct note_ref before = {.category = 0, .note = 0};
    bool had = folio_state_selection(self->state, &before);
    enum folio_state_outcome outcome =
        expanded && kind == FOLIO_CURSOR_CATEGORY ? save_edit(self) : FOLIO_STATE_READY;
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = folio_state_set_category_expanded(self->state, cursor.category, expanded);
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = reopen_if_moved(self, had, before);
    }
    redraw_drawer(self);
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, outcome);
        return;
    }
    if (self->drawer != nullptr)
    {
        /* 開閉でもカーソルは移るので、歩みと同じように見える位置へ寄せる（ADR 0013 の決定 6）。 */
        drawer_window_reveal_cursor(self->drawer);
    }
}

/* 「閲覧」の札の出口。別ノートの選択は save_edit、終了は操作 dispatcher を使う。 */
static bool leave_edit(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = flush_edit(self);
    if (outcome == FOLIO_STATE_READY)
    {
        return true;
    }
    failure_box_show(self->handle, outcome);
    return false;
}

static void click_command_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    if (self->command_surface != COMMAND_SURFACE_PALETTE)
    {
        if (self->command_input != nullptr)
        {
            SetFocus(self->command_input);
        }
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    int top =
        bounds.top +
        scale(base_command_palette_padding + base_command_input_height + base_command_gap, dpi);
    int row_height = scale(base_command_row_height, dpi);
    if (point.y < top || point.y >= top + row_height * (int)folio_command_count())
    {
        if (self->command_input != nullptr)
        {
            SetFocus(self->command_input);
        }
        return;
    }
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_command command = FOLIO_COMMAND_SAVE;
    bool found = command_at_query(query, (size_t)((point.y - top) / row_height), &command);
    utf8_text_destroy(query);
    if (found)
    {
        execute_command(self, command);
    }
}

/* 無効な側の札のクリックだけが意図になる。 */
static void click_caption(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    RECT close = close_rect(self);
    if (PtInRect(&close, point))
    {
        execute_command(self, FOLIO_COMMAND_SAVE_QUIT);
        return;
    }
    enum pane_mode chip = PANE_MODE_VIEW;
    if (!chip_at(self, point, &chip) || chip == folio_state_pane_mode(self->state))
    {
        return;
    }
    switch (chip)
    {
    case PANE_MODE_VIEW:
        leave_edit(self);
        break;
    case PANE_MODE_EDIT:
        enter_edit(self);
        break;
    }
}

/* 本文の Esc。編集中なら保存してから、フォーカスを索引へ返す（ADR 0013 の決定 3）。
 * 保存できなければ 1 行が出て、区画も本文もそのまま。 */
static void leave_pane(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT && !store_edit(self))
    {
        return;
    }
    SetFocus(self->handle);
}

/* RichEdit の鍵の通知。保存とパレットは操作 ID へ変換し、Esc は既存の本文出口を使う。 */
static LRESULT on_notify(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const NMHDR *_Nonnull header = (const NMHDR *)lparam;
    if (header->code != EN_MSGFILTER)
    {
        return 0;
    }
    const MSGFILTER *_Nonnull filter = (const MSGFILTER *)lparam;
    if (filter->msg == WM_CHAR && filter->wParam == store_character)
    {
        execute_command(self, FOLIO_COMMAND_SAVE);
        return 1;
    }
    if (filter->msg == WM_CHAR && filter->wParam == palette_character)
    {
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        return 1;
    }
    if (filter->msg == WM_KEYDOWN && filter->wParam == VK_ESCAPE)
    {
        leave_pane(self);
        return 1;
    }
    return 0;
}

/* ポインタの下がドロワーならホイールをそこへ渡す（ADR 0009 の決定 5）。
 * lparam はスクリーン座標なので、位置を読むために OS へ問い合わせない（ARC-007）。 */
static void forward_wheel(const struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    if (drawer == nullptr)
    {
        return;
    }
    RECT bounds = {0, 0, 0, 0};
    GetWindowRect(drawer, &bounds);
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (PtInRect(&bounds, point))
    {
        SendMessageW(drawer, WM_MOUSEWHEEL, wparam, lparam);
    }
}

/* カーソルが折り畳んだ／空のカテゴリ行にあるか（ADR 0015 の決定 4）。 */
static bool cursor_on_category(const struct folio_window *_Nonnull self)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {.category = 0, .note = 0};
    return folio_state_cursor(self->state, &kind, &cursor) && kind == FOLIO_CURSOR_CATEGORY;
}

/* 索引の鍵（ADR 0013 の決定 2）。Enter で本文へ（カーソルがカテゴリ行なら何もしない・
 * ADR 0015 の決定 4）、↑↓ / PgUp / PgDn はドロワーのスクロールへ渡す（ADR 0009 の決定 5）。
 * Escape はここでは何もしない（窓を閉じるのは Alt+F4 と閉じる操作だけ）。 */
static void press_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        if (key == VK_ESCAPE)
        {
            close_command_surface(self);
        }
        return;
    }
    if (key != 'G')
    {
        /* 'G' は g / G の仮想キー。ほかの鍵が挟まれば gg の 1 打目は忘れる。 */
        self->pending_g = false;
    }
    if (key == VK_RETURN)
    {
        if (!cursor_on_category(self))
        {
            focus_pane(self);
        }
        return;
    }
    if (self->drawer != nullptr)
    {
        drawer_window_scroll_key(self->drawer, key);
    }
}

/* 索引の文字の鍵（ADR 0013 の決定 2）。扱わない文字では何も起きない。
 * Ctrl+S は両方の区画で効くように、ここでも本文と同じ保存を行う。
 * IME が ON のときは未確定文字になってここへ届かない（ADR 0013 の「失う・残る」）。 */
static void type_key(struct folio_window *_Nonnull self, WPARAM character)
{
    if (self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        return;
    }
    bool twice = self->pending_g && character == 'g';
    self->pending_g = character == 'g' && !twice;
    switch (character)
    {
    case ':':
        open_command_surface(self, COMMAND_SURFACE_EX);
        break;
    case '?':
        execute_command(self, FOLIO_COMMAND_HELP);
        break;
    case palette_character:
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        break;
    case 'j':
        switch_adjacent(self, FOLIO_STEP_NEXT);
        break;
    case 'k':
        switch_adjacent(self, FOLIO_STEP_PREVIOUS);
        break;
    case 'G':
        switch_adjacent(self, FOLIO_STEP_LAST);
        break;
    case 'g':
        if (twice)
        {
            switch_adjacent(self, FOLIO_STEP_FIRST);
        }
        break;
    case 'h':
        expand_cursor(self, false);
        break;
    case 'l':
        expand_cursor(self, true);
        break;
    case store_character:
        execute_command(self, FOLIO_COMMAND_SAVE);
        break;
    default:
        /* WM_CHAR の文字は開いた集合（C-017）。 */
        break;
    }
}

static void command_not_found(struct folio_window *_Nonnull self)
{
    self->command_unknown = true;
    self->command_failure = FOLIO_STATE_READY;
    arrange_command_input(self);
    redraw_command_layer(self);
}

static void execute_command_input(struct folio_window *_Nonnull self)
{
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    size_t length = utf8_text_length(query);
    const char *_Nonnull bytes = utf8_text_bytes(query);
    enum folio_command command = FOLIO_COMMAND_SAVE;
    bool found = false;
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
        utf8_text_destroy(query);
        return;
    case COMMAND_SURFACE_EX:
        if (length == 0 || (length == 1 && bytes[0] == ':'))
        {
            utf8_text_destroy(query);
            close_command_surface(self);
            return;
        }
        found = folio_command_parse(bytes, length, &command);
        break;
    case COMMAND_SURFACE_PALETTE:
        found = command_at_query(query, self->command_selection, &command);
        break;
    }
    utf8_text_destroy(query);
    if (!found)
    {
        command_not_found(self);
        return;
    }
    execute_command(self, command);
}

static void move_command_selection(struct folio_window *_Nonnull self, WPARAM key)
{
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    size_t count = command_matches_count(query);
    utf8_text_destroy(query);
    if (count == 0)
    {
        return;
    }
    if (key == VK_UP)
    {
        self->command_selection =
            self->command_selection == 0 ? count - 1 : self->command_selection - 1;
    }
    else
    {
        self->command_selection =
            self->command_selection + 1 == count ? 0 : self->command_selection + 1;
    }
    redraw_command_layer(self);
}

static void update_command_composition(struct folio_window *_Nonnull self, UINT message)
{
    if (message == WM_IME_STARTCOMPOSITION)
    {
        self->command_composing = true;
    }
    if (message == WM_IME_ENDCOMPOSITION)
    {
        self->command_composing = false;
    }
}

static bool command_key_down(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->command_composing)
    {
        return false;
    }
    if (key == VK_ESCAPE)
    {
        close_command_surface(self);
        return true;
    }
    if (key == VK_RETURN)
    {
        execute_command_input(self);
        return true;
    }
    bool moves =
        self->command_surface == COMMAND_SURFACE_PALETTE && (key == VK_UP || key == VK_DOWN);
    if (moves)
    {
        move_command_selection(self, key);
    }
    return moves;
}

static bool command_character(struct folio_window *_Nonnull self, WPARAM character)
{
    if (character == palette_character)
    {
        show_command_palette(self);
        return true;
    }
    return !self->command_composing && character == '\r';
}

static LRESULT CALLBACK command_input_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr || self->command_input_original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    update_command_composition(self, message);
    if (message == WM_KEYDOWN && command_key_down(self, wparam))
    {
        return 0;
    }
    if (message == WM_CHAR && command_character(self, wparam))
    {
        return 0;
    }
    if (message == WM_KILLFOCUS)
    {
        PostMessageW(self->handle, folio_message_command_focus_lost, 0, 0);
    }
    return CallWindowProcW(self->command_input_original, window, message, wparam, lparam);
}

static LRESULT on_command(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if ((HWND)lparam == self->command_input && HIWORD(wparam) == EN_CHANGE)
    {
        self->command_selection = 0;
        self->command_unknown = false;
        self->command_failure = FOLIO_STATE_READY;
        arrange_command_input(self);
        redraw_command_layer(self);
    }
    return 0;
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
        note_pane_create(window, self->palette.pane, self->palette.editor_text, &self->pane) !=
            NOTE_PANE_CREATED)
    {
        return -1;
    }
    self->command_brush = CreateSolidBrush(self->palette.window);
    if (self->command_brush == nullptr)
    {
        return -1;
    }
    self->command_layer =
        CreateWindowExW(0, command_layer_class, L"", WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                        0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), self);
    if (self->command_layer == nullptr)
    {
        return -1;
    }
    self->command_input = CreateWindowExW(0, edit_class, L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0, 0,
                                          self->command_layer, (HMENU)(INT_PTR)command_control_id,
                                          GetModuleHandleW(nullptr), nullptr);
    if (self->command_input == nullptr)
    {
        return -1;
    }
    SetWindowLongPtrW(self->command_input, GWLP_USERDATA, (LONG_PTR)self);
    self->command_input_original = (WNDPROC)SetWindowLongPtrW(self->command_input, GWLP_WNDPROC,
                                                              (LONG_PTR)command_input_procedure);
    if (self->command_input_original == nullptr)
    {
        return -1;
    }
    SendMessageW(self->command_input, EM_LIMITTEXT, command_input_capacity - 1, 0);
    SendMessageW(self->command_input, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    render_pane(self);
    return 0;
}

/* OS が勧める矩形へ移し、字の大きさを取り直す（FR-013）。 */
static void apply_dpi(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const RECT *_Nonnull suggested = (const RECT *)lparam;
    refresh_font(self, GetDpiForWindow(self->handle));
    SetWindowPos(self->handle, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

/* 最大化中の窓の矩形は枠ぶん画面より大きいので、client をモニタの作業領域に収める。 */
static void fit_work_area(HWND window, RECT *_Nonnull client)
{
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    if (IsZoomed(window) &&
        GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitor))
    {
        *client = monitor.rcWork;
    }
}

/* 枠なし窓の client は窓の矩形そのもの（ADR 0011 の決定 1）。TRUE でも FALSE でも 0 を返し、
 * 既定処理には一度も渡さない（FALSE の lParam は RECT * で、触らなければ窓の矩形のまま）。
 * 窓の構造体を要らないので、GWLP_USERDATA を結ぶ前に届く計算にも自分で答える。 */
static LRESULT calculate_client(HWND window, WPARAM wparam, LPARAM lparam)
{
    if (wparam)
    {
        NCCALCSIZE_PARAMS *_Nonnull calculation = (NCCALCSIZE_PARAMS *)lparam;
        fit_work_area(window, &calculation->rgrc[0]);
        return 0;
    }
    fit_work_area(window, (RECT *)lparam);
    return 0;
}

/* 窓の最小の大きさ（ADR 0011 の決定 3）。これも窓の構造体を要らないので作成中にも答える。 */
static LRESULT limit_size(HWND window, LPARAM lparam)
{
    MINMAXINFO *_Nonnull limits = (MINMAXINFO *)lparam;
    UINT dpi = GetDpiForWindow(window);
    limits->ptMinTrackSize.x = scale(base_min_width, dpi);
    limits->ptMinTrackSize.y = scale(base_min_height, dpi);
    return 0;
}

static LRESULT color_command_input(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if ((HWND)lparam != self->command_input || self->command_brush == nullptr)
    {
        return DefWindowProcW(self->handle, WM_CTLCOLOREDIT, wparam, lparam);
    }
    HDC device = (HDC)wparam;
    SetTextColor(device, self->palette.current_text);
    SetBkColor(device, self->palette.window);
    return (LRESULT)self->command_brush;
}

static void paint_command_layer(struct folio_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC target = BeginPaint(self->command_layer, &painting);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    HDC memory = CreateCompatibleDC(target);
    HBITMAP surface =
        memory == nullptr ? nullptr : CreateCompatibleBitmap(target, bounds.right, bounds.bottom);
    if (surface != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, surface);
        SetBkMode(memory, TRANSPARENT);
        SelectObject(memory, self->mono_font);
        SetTextCharacterExtra(memory, scale(base_tracking, GetDpiForWindow(self->handle)));
        draw_command_surface(self, memory, bounds);
        BitBlt(target, 0, 0, bounds.right, bounds.bottom, memory, 0, 0, SRCCOPY);
        SelectObject(memory, previous);
        DeleteObject(surface);
    }
    if (memory != nullptr)
    {
        DeleteDC(memory);
    }
    EndPaint(self->command_layer, &painting);
}

static LRESULT CALLBACK command_layer_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    if (message == WM_CREATE)
    {
        const CREATESTRUCTW *_Nonnull creation = (const CREATESTRUCTW *)lparam;
        SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)creation->lpCreateParams);
        return 0;
    }
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    switch (message)
    {
    case WM_PAINT:
        paint_command_layer(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        click_command_surface(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return 0;
    case WM_COMMAND:
        return on_command(self, wparam, lparam);
    case WM_CTLCOLOREDIT:
        return color_command_input(self, wparam, lparam);
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

static void window_destroyed(struct folio_window *_Nonnull self)
{
    if (self->mono_font != nullptr)
    {
        DeleteObject(self->mono_font);
        self->mono_font = nullptr;
    }
    self->handle = nullptr;
    PostQuitMessage(0);
}

static void dismiss_command_if_focus_moved(struct folio_window *_Nonnull self)
{
    HWND focus = GetFocus();
    bool moved_within_window = focus != nullptr && focus != self->command_input &&
                               GetAncestor(focus, GA_ROOT) == self->handle;
    if (self->command_surface != COMMAND_SURFACE_CLOSED && moved_within_window)
    {
        dismiss_command_surface(self);
    }
}

static LRESULT on_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                          LPARAM lparam)
{
    switch (message)
    {
    case WM_NCHITTEST:
        return hit_test(self, lparam);
    case WM_SIZE:
        arrange(self);
        /* 頭のパンくずと札は幅に依存する位置に描く（ADR 0011 の決定 2）。 */
        InvalidateRect(self->handle, nullptr, FALSE);
        return 0;
    case WM_DPICHANGED:
        apply_dpi(self, lparam);
        return 0;
    case WM_PAINT:
        paint_pane(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        click_caption(self, lparam);
        return 0;
    case WM_NOTIFY:
        return on_notify(self, lparam);
    case folio_message_command_focus_lost:
        dismiss_command_if_focus_moved(self);
        return 0;
    case folio_message_select_note:
        /* ドロワーからのノート行のクリック。結果は enum folio_state_outcome で返す。 */
        return (LRESULT)switch_note(self, (size_t)wparam, (size_t)lparam);
    case WM_KEYDOWN:
        press_key(self, wparam);
        return 0;
    case WM_CHAR:
        type_key(self, wparam);
        return 0;
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
        /* フォーカスの印は索引の選択行に描く（ADR 0013 の決定 8）。 */
        redraw_drawer(self);
        return 0;
    case WM_MOUSEWHEEL:
        forward_wheel(self, wparam, lparam);
        return 0;
    case WM_CLOSE:
        execute_command(self, FOLIO_COMMAND_SAVE_QUIT);
        return 0;
    case WM_DESTROY:
        window_destroyed(self);
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(self->handle, message, wparam, lparam);
    }
}

static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    /* 窓の構造体を要らないものは、結び付けの前に届いても自分で答える（ADR 0011 の決定 1 / 3）。 */
    switch (message)
    {
    case WM_CREATE:
        return on_create(window, lparam);
    case WM_NCCALCSIZE:
        return calculate_client(window, wparam, lparam);
    case WM_GETMINMAXINFO:
        return limit_size(window, lparam);
    /* `DefWindowProcW` は client の計算ではなく `WS_THICKFRAME` の有無で枠を描き、その描画は
     * `WM_NCPAINT` を経由しない。非アクティブ化で枠の色の帯が出るので、どちらも自分で答えて
     * 既定処理へ渡さない（ADR 0014）。 */
    case WM_NCACTIVATE:
        return TRUE;
    case WM_NCPAINT:
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        break;
    }
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    return on_message(self, message, wparam, lparam);
}

static bool ensure_named_class(HINSTANCE instance, const wchar_t *_Nonnull name,
                               WNDPROC _Nonnull procedure)
{
    WNDCLASSEXW existing = {.cbSize = sizeof existing};
    if (GetClassInfoExW(instance, name, &existing))
    {
        return true;
    }
    WNDCLASSEXW description = {
        .cbSize = sizeof description,
        .lpfnWndProc = procedure,
        .hInstance = instance,
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .lpszClassName = name,
    };
    return RegisterClassExW(&description) != 0;
}

static bool ensure_classes(HINSTANCE instance)
{
    return ensure_named_class(instance, class_name, window_procedure) &&
           ensure_named_class(instance, command_layer_class, command_layer_procedure);
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
    if (!ensure_classes(instance))
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
    DWORD style =
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN;
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
    if (window->command_brush != nullptr)
    {
        DeleteObject(window->command_brush);
    }
    free(window);
}
