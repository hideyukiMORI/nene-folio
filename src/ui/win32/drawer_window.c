#include "drawer_window.h"
#include "accessibility_host.h"
#include "accessibility_node.h"
#include "accessible_request.h"
#include "accessible_view.h"

#include "drawer_header_button.h"
#include "drawer_layout.h"
#include "drawer_menu_item.h"
#include "failure_box.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "icon_paint.h"
#include "note_ref.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"

#include <commdlg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

/* ドラッグ中の一時状態はここだけが持つ（ARC-004 / ARC-005 の許可区画）。 */
struct drawer_window
{
    HWND _Nullable handle;
    struct folio_state *_Nonnull state;
    struct accessibility_host *_Nonnull accessibility; /* 親窓より先に破棄する借用。 */
    struct drawer_layout *_Nullable accessible_layout;
    uint64_t accessible_revision;
    UINT accessible_dpi;
    int accessible_height;
    struct folio_palette palette;
    HFONT _Nullable category_font;
    HFONT _Nullable note_font;
    HFONT _Nullable mono_font;
    bool pressed; /* 左ボタンを押した行を覚えているか */
    enum drawer_header_button header_pressed;
    bool header_inside;   /* 押したボタンの矩形の内にポインタがある（外なら押していない見た目） */
    bool header_hover;    /* ポインタがカテゴリ＋の上にある */
    bool header_tracking; /* TrackMouseEvent の WM_MOUSELEAVE を待っている */
    size_t pressed_row;
    int pressed_y; /* 押したときの y。しきい値の判定に使う */
    int pointer_y; /* 最後に見たポインタの y。スクロール中に落とし先を引き直す */
    /* しきい値を超えて動かしたか。絞り込み中で並び替えに入れなくても立つので、
     * 離したときにクリックへ落とさない（ADR 0024 の補正 2）。 */
    bool moved;
    bool dragging;             /* しきい値を超えて動かしているか */
    struct drop_target target; /* dragging のときの落とし先 */
    /* 色の選択のカスタム色。実行中だけ持ち、保存しない（ADR 0010 の決定 4）。 */
    COLORREF custom_colors[16];
};

static const wchar_t class_name[] = L"NeNeFolioDrawer";
static const wchar_t mono_face[] = L"Consolas";
/* 1 回の描画で UTF-16 へ写せる単位数（ADR 0030 の決定 5）。表の 1 行もカテゴリ名・ノート名
 * （255 バイト）もここに収まる。収まらなければ何も描かない。 */
constexpr size_t draw_unit_limit = 1024;

/* 96 DPI での寸法（デザイン「案2 堅」）。描くときに DPI で拡大する（FR-013）。 */
constexpr int base_dpi = 96;
constexpr int base_header_indent = 16;
/* 常設の「すべてのノートを検索」の欄が占める帯（ADR 0024 の決定 6）。欄そのものは主窓の子で、
 * ここは行を置かない余白として数える。 */
constexpr int base_filter_height = 36;
constexpr int base_category_band_height = 36;
constexpr int base_filter_inset = 16; /* 欄の左右の余白（窓の縁 16・#349） */
constexpr int base_filter_margin = 4; /* 欄の上下に空ける間 */
/* 「一致 / 総数」の 1 行が要る大きさ（20 桁 2 つ ＋ 区切り ＋ 終端で足りる）。 */
constexpr size_t filter_count_capacity = 48;
constexpr int base_row_height = 30;
constexpr int base_category_height = 34;
constexpr int base_category_gap = 6;
constexpr int base_category_indent = 16;
constexpr int base_name_offset = 26; /* 番号の左端から名前の左端まで */
constexpr int base_note_indent = 40;
constexpr int base_right_inset = 16;
constexpr int base_bottom_padding = 16; /* 最後の行の下に空ける余白（左右の余白と同じ） */
constexpr int base_fade_height = 24;    /* あふれを示すフェードの高さ（ADR 0009 の決定 7） */
constexpr int base_mark_size = 6;
constexpr int base_mark_gap = 6; /* カテゴリ行で折畳の印とカーソルの角の間に空ける幅 */
/* 折畳の印の箱（24 の viewBox と 1 対 1）。板は 2 単位なので、箱が 20px 未満だと
 * 塗りが 50〜75% の灰色になる（ADR 0033 の決定 6・実測）。 */
constexpr int base_toggle_box = 24;
constexpr int base_line_thickness = 2; /* ドラッグ中の挿入線の太さ */
constexpr int base_category_font = 12;
constexpr int base_note_font = 14;
constexpr int base_mono_font = 11;
constexpr int base_tracking = 2; /* カテゴリ名と頭の文字の字間 */
constexpr int wheel_rows = 3;    /* ホイール 1 刻みで動くノート行の数（ADR 0009 の決定 4） */
constexpr int fade_full = 256;   /* フェードの端で地の色に寄せる重み（256 分の 1） */

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

static HFONT _Nullable create_font(UINT dpi, int height, int weight, const wchar_t *_Nonnull face)
{
    return CreateFontW(-scale(height, dpi), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, face);
}

static void release_font(HFONT _Nullable *_Nonnull font)
{
    if (*font != nullptr)
    {
        DeleteObject(*font);
        *font = nullptr;
    }
}

static void release_fonts(struct drawer_window *_Nonnull self)
{
    release_font(&self->category_font);
    release_font(&self->note_font);
    release_font(&self->mono_font);
}

/* 文字の書体は言語ごとの face（ADR 0032 の決定 4）。番号の等幅は Consolas のまま。
 * DPI が変わったときと言語を採り直したときの両方がここを通る（第 2 の経路を作らない）。 */
static void refresh_fonts(struct drawer_window *_Nonnull self, UINT dpi)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(folio_state_language(self->state), face);
    release_fonts(self);
    self->category_font = create_font(dpi, base_category_font, FW_BOLD, face);
    self->note_font = create_font(dpi, base_note_font, FW_NORMAL, face);
    self->mono_font = create_font(dpi, base_mono_font, FW_NORMAL, mono_face);
}

/* 固定の検索欄・カテゴリ帯。行配置/clip/hit/scrollはこの高さを共用する。 */
static int band_height(const struct drawer_window *_Nonnull self)
{
    return scale(base_filter_height + base_category_band_height, GetDpiForWindow(self->handle));
}

static RECT new_category_rect(const struct drawer_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT client;
    GetClientRect(self->handle, &client);
    int box = scale(ui_part_height, dpi); /* C 様式の当たり 28×28（ADR0060 の決定 1） */
    int inset = scale(ui_part_edge, dpi);
    int middle = band_height(self) - scale(base_category_band_height, dpi) / 2;
    return (RECT){client.right - inset - box, middle - box / 2, client.right - inset,
                  middle - box / 2 + box};
}

static RECT filter_rect(const struct drawer_window *_Nonnull drawer)
{
    RECT bounds = {0, 0, 0, 0};
    if (drawer->handle == nullptr)
    {
        return bounds;
    }
    UINT dpi = GetDpiForWindow(drawer->handle);
    RECT client = {0, 0, 0, 0};
    GetClientRect(drawer->handle, &client);
    int inset = scale(base_filter_inset, dpi);
    int margin = scale(base_filter_margin, dpi);
    bounds.left = inset;
    bounds.top = margin;
    bounds.right = client.right - inset;
    bounds.bottom = scale(base_filter_height, dpi) - margin;
    return bounds;
}

/* いまの DPI と client の高さで寸法を測る。スクロール上限は core がここから決める（FR-012）。 */
static struct drawer_metrics metrics_for(const struct drawer_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT client = {0, 0, 0, 0};
    GetClientRect(self->handle, &client);
    struct drawer_metrics metrics = {
        .top_padding = band_height(self),
        .row_height = scale(base_row_height, dpi),
        .category_height = scale(base_category_height, dpi),
        .category_gap = scale(base_category_gap, dpi),
        .category_indent = scale(base_category_indent, dpi),
        .note_indent = scale(base_note_indent, dpi),
        .viewport_height = client.bottom,
        .bottom_padding = scale(base_bottom_padding, dpi),
    };
    return metrics;
}

static COLORREF to_colorref(struct rgb_color color)
{
    return RGB(color.red, color.green, color.blue);
}

/* 装飾色も、その行で実際に載る背景と同じOS対を使う（ADR0057）。 */
static COLORREF row_ink(const struct drawer_window *_Nonnull self, struct drawer_row row)
{
    if (self->palette.contrast == FOLIO_CONTRAST_HIGH)
    {
        return row.selected ? self->palette.selected_text : self->palette.category_text;
    }
    return to_colorref(row.color);
}

/* UTF-8 を確保せずに呼び出し側の入れ物へ写す（ADR 0030 の決定 5）。
 * 収まらない・壊れている場合は 0 を返し、呼び出し側は何も出さない。 */
static int wide_units(const char *_Nonnull text, char16_t *_Nonnull out)
{
    size_t written = 0;
    if (utf16_text_fill(text, out, draw_unit_limit, &written) != UTF16_TEXT_FILL_READY)
    {
        return 0;
    }
    return (int)written;
}

/* UTF-8 を 1 行で描く。文字の伸びは呼び出し側が SetTextCharacterExtra で決める。 */
static void draw_utf8(HDC device, const char *_Nonnull text, RECT bounds, UINT format)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(text, units);
    if (count == 0)
    {
        return;
    }
    DrawTextW(device, units, count, &bounds, format | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void fill_rect(HDC device, RECT bounds, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(device, &bounds, brush);
    DeleteObject(brush);
}

/* 1 px の枠だけを描く（フォーカスが本文にあるときの印・ADR 0013 の決定 8）。 */
static void frame_rect(HDC device, RECT bounds, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FrameRect(device, &bounds, brush);
    DeleteObject(brush);
}

/* 索引の区画にフォーカスがあるか。Win32 のフォーカスがそのまま区画である（ADR 0013 の決定 1）。
 * ドロワーはフォーカスを取らないので、見るのは主窓（ADR 0009 の決定 5）。 */
static bool index_focused(const struct drawer_window *_Nonnull self)
{
    return SendMessageW(GetParent(self->handle), folio_message_index_focused, 0, 0) != 0;
}

/* 10 進の文字列にする。反転して並べ直す。 */
static int format_count(size_t value, wchar_t *_Nonnull out)
{
    int written = 0;
    do
    {
        out[written++] = (wchar_t)(L'0' + value % 10);
        value /= 10;
    } while (value > 0 && written < 20);
    for (int index = 0; index < written / 2; ++index)
    {
        wchar_t swap = out[index];
        out[index] = out[written - 1 - index];
        out[written - 1 - index] = swap;
    }
    return written;
}

/* 頭の帯の右の数。ふだんはノートの総数、**絞り込みが効いているあいだは「一致 / 総数」**
 * （ADR 0024 の 2026-09-22 の補正 7）。数は application の一致集合から取り、UI は数えない。 */
static void draw_header_count(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    size_t total = folio_state_note_count(self->state);
    if (!folio_state_filtering(self->state))
    {
        wchar_t count[24];
        int written = format_count(total, count);
        DrawTextW(device, count, written, &bounds,
                  DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
        return;
    }
    char line[filter_count_capacity];
    struct ui_text_request request = {.id = UI_TEXT_STATUS_FILTER_COUNT,
                                      .language = folio_state_language(self->state),
                                      .k = folio_state_index_filter_count(self->state),
                                      .n = total};
    if (ui_text_format(&request, line, filter_count_capacity) != UI_TEXT_FORMAT_READY)
    {
        return;
    }
    draw_utf8(device, line, bounds, DT_RIGHT);
}

/* 欄そのものは主窓の子で、ドロワーの DC からは欄の矩形が除かれている。EDIT は箱の中で上下左右を
 * focus の太さだけ空けて置かれるので、その空きに静止 1px・focus 2px の枠を箱の内側へ描く
 * （ADR0060 の決定 3・#349。ADR 0024 の補正 5 を置き換える）。 */
static void draw_filter_frame(const struct drawer_window *_Nonnull self, HDC device)
{
    bool focused = SendMessageW(GetParent(self->handle), folio_message_filter_focused, 0, 0) != 0;
    int focus = focused ? ui_part_focus_width(GetDpiForWindow(self->handle)) : 0;
    /* 欄の EDIT は字の高さで縦中央に置かれ、周りの空きを EDIT と同じ地で塗ってから内側に枠を
     * 描く。focus の 2px は空きの中で太り、字は動かない（#349）。 */
    ui_part_fill_field(device, filter_rect(self), self->palette.window);
    ui_part_draw_field(device, &self->palette, filter_rect(self), focus);
}

static bool header_focused(const struct drawer_window *_Nonnull self)
{
    struct accessible_item focus = {.kind = ACCESSIBLE_WINDOW};
    return accessibility_host_focus(self->accessibility, ACCESSIBLE_DRAWER, &focus) == S_OK &&
           focus.kind == ACCESSIBLE_NEW_CATEGORY &&
           SendMessageW(GetParent(self->handle), folio_message_focus_cues, 0, 0) != 0;
}

/* カテゴリ＋は C 様式。状態の表現は主窓の部品と同じ ui_part の 1 本を通る。 */
static void draw_category_band(const struct drawer_window *_Nonnull self, HDC device)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT button = new_category_rect(self);
    RECT label = {scale(base_header_indent, dpi),
                  band_height(self) - scale(base_category_band_height, dpi),
                  button.left - scale(80, dpi), band_height(self)};
    SelectObject(device, self->category_font);
    SetTextColor(device, self->palette.category_text);
    draw_utf8(device, ui_text_line(UI_TEXT_TITLE_CATEGORIES, folio_state_language(self->state)),
              label, 0);
    RECT count = {label.right + scale(8, dpi), label.top, button.left - scale(8, dpi),
                  label.bottom};
    SelectObject(device, self->mono_font);
    SetTextColor(device, self->palette.note_text);
    draw_header_count(self, device, count);
    bool pressed = self->header_pressed == DRAWER_HEADER_NEW_CATEGORY;
    struct ui_part_paint part = {
        .bounds = button,
        .style = UI_PART_STYLE_ICON,
        .dpi = dpi,
        .has_icon = true,
        .icon = ICON_PAINT_ADD,
        .hovered = self->header_hover && !pressed,
        .pressed = pressed && self->header_inside,
        .focused = header_focused(self),
    };
    ui_part_draw(device, &self->palette, &part);
}

/* 固定帯。共通のブランド/窓操作の頭は親が所有する（ADR0047）。 */
static void draw_header(const struct drawer_window *_Nonnull self, HDC device)
{
    SetTextCharacterExtra(device, 0);
    draw_filter_frame(self, device);
    draw_category_band(self, device);
}

/* カーソルの行の右端に置く角（カテゴリ色の四角）。right はその右端の x。
 * 索引に区画があるとき塗り、本文にあるとき枠だけ（ADR 0013 の決定 8・ADR 0015 の決定 7）。 */
static void draw_cursor_mark(const struct drawer_window *_Nonnull self, HDC device,
                             struct drawer_row row, int right)
{
    int mark = scale(base_mark_size, GetDpiForWindow(self->handle));
    int top = row.top + row.height / 2 - mark / 2;
    RECT square = {right - mark, top, right, top + mark};
    if (index_focused(self))
    {
        fill_rect(device, square, row_ink(self, row));
    }
    else
    {
        frame_rect(device, square, row_ink(self, row));
    }
}

/* 折畳の印が要る幅（箱と間）。印が字形でなく面のパスになったので、書体の実測ではなく
 * 定数で決まる（ADR 0033 の決定 6・ADR 0015 の決定 7 の補正）。
 * カーソルの角はこのぶんだけ左に置いて重なりを避ける。 */
static int toggle_room(UINT dpi)
{
    return scale(base_toggle_box, dpi) + scale(base_mark_gap, dpi);
}

/* カテゴリ行: 番号（カテゴリ色・等幅）、名前（太字・字間広め）、右端に折畳の印。
 * カーソルがこの行にあるときは折畳の印の左に角を置く（ADR 0015 の決定 7）。 */
static void draw_category(const struct drawer_window *_Nonnull self, HDC device,
                          struct drawer_row row, int width)
{
    UINT dpi = GetDpiForWindow(self->handle);
    wchar_t ordinal[3] = {(wchar_t)(L'0' + (row.ordinal / 10) % 10),
                          (wchar_t)(L'0' + row.ordinal % 10), L'\0'};
    RECT number = {row.indent, row.top, width, row.top + row.height};
    SelectObject(device, self->mono_font);
    SetTextColor(device, row_ink(self, row));
    DrawTextW(device, ordinal, 2, &number, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    RECT name = {row.indent + scale(base_name_offset, dpi), row.top,
                 width - scale(base_right_inset, dpi) * 2, row.top + row.height};
    SelectObject(device, self->category_font);
    SetTextColor(device, self->palette.category_text);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    draw_utf8(device, row.text, name, DT_END_ELLIPSIS);
    SetTextCharacterExtra(device, 0);
    RECT mark = {row.indent, row.top, width - scale(base_right_inset, dpi), row.top + row.height};
    int box = scale(base_toggle_box, dpi);
    int top = row.top + (row.height - box) / 2;
    RECT toggle = {mark.right - box, top, mark.right, top + box};
    icon_paint_fill(device, toggle,
                    row.expanded ? ICON_PAINT_FOLD_COLLAPSE : ICON_PAINT_FOLD_EXPAND,
                    self->palette.header_text);
    if (row.cursor)
    {
        draw_cursor_mark(self, device, row, mark.right - toggle_room(dpi));
    }
}

/* ノート行: 選択中なら面を敷き、カーソルの行なら右端にカテゴリ色の角を置く
 * （面は選択・角はカーソル・ADR 0015 の決定 8）。 */
static void draw_note(const struct drawer_window *_Nonnull self, HDC device, struct drawer_row row,
                      int width)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int inset = scale(base_right_inset, dpi);
    if (row.selected)
    {
        RECT face = {0, row.top, width, row.top + row.height};
        fill_rect(device, face, self->palette.selected_background);
    }
    if (row.cursor)
    {
        draw_cursor_mark(self, device, row, width - inset);
    }
    RECT bounds = {row.indent, row.top, width - inset * 2, row.top + row.height};
    SelectObject(device, self->note_font);
    SetTextColor(device, row.selected ? self->palette.selected_text : self->palette.note_text);
    draw_utf8(device, row.text, bounds, DT_END_ELLIPSIS);
}

/* ドラッグ中の挿入線。掴んだ行の色で、その字下げから右の余白まで引く（ADR 0007 の決定 6）。 */
static void draw_drop_line(const struct drawer_window *_Nonnull self, HDC device,
                           const struct drawer_layout *_Nonnull layout, int width)
{
    if (!self->dragging || self->pressed_row >= drawer_layout_row_count(layout))
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    struct drawer_row row = drawer_layout_row(layout, self->pressed_row);
    RECT line = {row.indent, self->target.line_y, width - scale(base_right_inset, dpi),
                 self->target.line_y + scale(base_line_thickness, dpi)};
    fill_rect(device, line,
              self->palette.contrast == FOLIO_CONTRAST_HIGH ? self->palette.category_text
                                                            : row_ink(self, row));
}

static void draw_rows(const struct drawer_window *_Nonnull self, HDC device,
                      const struct drawer_layout *_Nonnull layout, int width)
{
    size_t count = drawer_layout_row_count(layout);
    for (size_t index = 0; index < count; ++index)
    {
        struct drawer_row row = drawer_layout_row(layout, index);
        switch (row.kind)
        {
        case DRAWER_ROW_CATEGORY:
            draw_category(self, device, row, width);
            break;
        case DRAWER_ROW_NOTE:
            draw_note(self, device, row, width);
            break;
        }
    }
    draw_drop_line(self, device, layout, width);
}

/* いまの配置を作る。作れなければ false（描き直しの機会に回復する）。 */
static bool current_layout(const struct drawer_window *_Nonnull self,
                           struct drawer_layout *_Nullable *_Nonnull out)
{
    return folio_state_drawer_layout(self->state, metrics_for(self), out) == FOLIO_STATE_READY;
}

/* COLORREF（0x00BBGGRR）を 32 bit の DIB の 1 画素（0x00RRGGBB）にする。 */
static uint32_t to_pixel(COLORREF color)
{
    return ((uint32_t)GetRValue(color) << 16) | ((uint32_t)GetGValue(color) << 8) |
           (uint32_t)GetBValue(color);
}

/* 画素の 1 成分を地の色へ weight / fade_full だけ寄せる。 */
static uint32_t mix_channel(uint32_t pixel, uint32_t ground, int shift, int weight)
{
    int value = (int)((pixel >> shift) & 0xFFu);
    int base = (int)((ground >> shift) & 0xFFu);
    return (uint32_t)(value + (base - value) * weight / fade_full) << shift;
}

static uint32_t mix(uint32_t pixel, uint32_t ground, int weight)
{
    return mix_channel(pixel, ground, 16, weight) | mix_channel(pixel, ground, 8, weight) |
           mix_channel(pixel, ground, 0, weight);
}

/* あふれを示すフェードを 1 本、画素ごとに混ぜる（ADR 0009 の決定 7）。
 * above なら頭の帯の直下から下へ、そうでなければ client の下端から上へ、端で地の色 100%。 */
static void draw_fade(const struct drawer_window *_Nonnull self, uint32_t *_Nonnull pixels,
                      RECT client, bool above)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int height = scale(base_fade_height, dpi);
    int start = above ? band_height(self) : client.bottom - height;
    uint32_t ground = to_pixel(self->palette.window);
    for (int row = 0; row < height; ++row)
    {
        int y = start + row;
        int weight = fade_full - fade_full * (above ? row : height - 1 - row) / height;
        if (y < 0 || y >= client.bottom)
        {
            continue;
        }
        for (int x = 0; x < client.right; ++x)
        {
            uint32_t *_Nonnull pixel = &pixels[(size_t)y * (size_t)client.right + (size_t)x];
            *pixel = mix(*pixel, ground, weight);
        }
    }
}

/* 32 bit・top-down の DIB セクション。画素は 0x00RRGGBB で並ぶ（ADR 0009 の決定 7）。
 * GDI+ が塗った画素はアルファ欄が 0xFF になるが、mix は RGB だけを組むのでフェード帯では
 * 0 に戻り、BitBlt(SRCCOPY) はアルファを見ない（ADR 0033 の決定 5）。 */
static HBITMAP _Nullable create_surface(HDC device, RECT client,
                                        uint32_t *_Nullable *_Nonnull pixels)
{
    BITMAPINFO info = {.bmiHeader = {.biSize = sizeof info.bmiHeader,
                                     .biWidth = client.right,
                                     .biHeight = -client.bottom,
                                     .biPlanes = 1,
                                     .biBitCount = 32,
                                     .biCompression = BI_RGB}};
    /* CreateDIBSection の出力引数は void ** でしか受けられない（Win32 の境界・C-006）。 */
    void *bits = nullptr;
    HBITMAP surface = CreateDIBSection(device, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    *pixels = bits;
    return surface;
}

/* 面を 1 枚仕上げる。行は頭の帯の下から client の下端までにクリップして描く（決定 7）。 */
static void paint_surface(const struct drawer_window *_Nonnull self, HDC device,
                          uint32_t *_Nonnull pixels, RECT client)
{
    fill_rect(device, client, self->palette.window);
    SetBkMode(device, TRANSPARENT);
    struct drawer_layout *_Nullable layout = nullptr;
    if (!current_layout(self, &layout))
    {
        draw_header(self, device);
        return;
    }
    int header = band_height(self);
    IntersectClipRect(device, 0, header, client.right, client.bottom);
    draw_rows(self, device, layout, client.right);
    SelectClipRgn(device, nullptr);
    draw_header(self, device);
    /* 画素へ触る前に GDI の溜めを吐き出す。 */
    GdiFlush();
    if (self->palette.contrast != FOLIO_CONTRAST_HIGH && drawer_layout_overflow_above(layout))
    {
        draw_fade(self, pixels, client, true);
    }
    if (self->palette.contrast != FOLIO_CONTRAST_HIGH && drawer_layout_overflow_below(layout))
    {
        draw_fade(self, pixels, client, false);
    }
    drawer_layout_destroy(layout);
}

/* DIB セクションで完成させてから転送する（C-017）。 */
static void paint(struct drawer_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC target = BeginPaint(self->handle, &painting);
    RECT client = {0, 0, 0, 0};
    GetClientRect(self->handle, &client);
    HDC memory = CreateCompatibleDC(target);
    uint32_t *_Nullable pixels = nullptr;
    HBITMAP surface = memory == nullptr ? nullptr : create_surface(memory, client, &pixels);
    if (surface != nullptr && pixels != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, surface);
        paint_surface(self, memory, pixels, client);
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

/* ノート行のクリックは主窓へ渡す。「編集中なら保存 → 選択 → 同じモードで開く」の順は
 * 主窓の 1 か所が持つ（ADR 0013 の決定 4 / 7）。 */
static enum folio_state_outcome select_note(struct drawer_window *_Nonnull self,
                                            struct drawer_row row)
{
    /* LRESULT で返る値は主窓が入れた enum folio_state_outcome（Win32 の境界）。 */
    return (enum folio_state_outcome)SendMessageW(
        GetParent(self->handle), folio_message_select_note, (WPARAM)row.category, (LPARAM)row.note);
}

/* 行への意図を application へ渡し、結果を写す。
 * 絞り込み中の開閉は application が `FOLIO_STATE_FILTERED` で断るが、クリックのたびに
 * モーダルの失敗箱を出さず、ここで静かに戻る（ADR 0024 の補正 1）。 */
static void act_on_row(struct drawer_window *_Nonnull self, struct drawer_row row)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    switch (row.kind)
    {
    case DRAWER_ROW_CATEGORY:
        if (folio_state_filtering(self->state))
        {
            return;
        }
        outcome = folio_state_toggle_category(self->state, row.category);
        break;
    case DRAWER_ROW_NOTE:
        outcome = select_note(self, row);
        break;
    }
    if (outcome == FOLIO_STATE_READY)
    {
        InvalidateRect(self->handle, nullptr, FALSE);
        return;
    }
    failure_box_show(GetAncestor(self->handle, GA_ROOT), outcome, self->state);
}

/* 落とし先を意図にする（ノートは別カテゴリへの移動も同じ 1 本・ADR 0008 の決定 2 / 6）。
 * 並び替えも移動も選択の番号を動かすので、親にも描き直しを頼む。 */
static void apply_drop(struct drawer_window *_Nonnull self, struct drawer_row source,
                       struct drop_target target)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    switch (target.kind)
    {
    case DROP_CATEGORY:
        outcome = folio_state_move_category(self->state, target.category, target.index);
        break;
    case DROP_NOTE:
        outcome = folio_state_move_note(
            self->state, (struct note_ref){.category = source.category, .note = source.note},
            (struct note_ref){.category = target.category, .note = target.index});
        break;
    }
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(GetAncestor(self->handle, GA_ROOT), outcome, self->state);
        return;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    /* 右ペインの頭のカテゴリ番号が変わりうる。本文は編集中でも触らない（ADR 0007 の決定 7）。 */
    InvalidateRect(GetParent(self->handle), nullptr, FALSE);
}

/* OS の色の選択が返す COLORREF を core の色にする。逆向きは to_colorref（C-014）。 */
static struct rgb_color to_rgb_color(COLORREF color)
{
    struct rgb_color converted = {
        .red = GetRValue(color), .green = GetGValue(color), .blue = GetBValue(color)};
    return converted;
}

/* いまの色を初期値に OS の色の選択を出し、選ばれたら意図にする（ADR 0010 の決定 4〜6）。
 * 色が妥当かも変わったかも UI は判断しない（ARC-011）。 */
static void choose_color(struct drawer_window *_Nonnull self, struct drawer_row row)
{
    CHOOSECOLORW choice = {
        .lStructSize = sizeof choice,
        .hwndOwner = GetParent(self->handle),
        .rgbResult = to_colorref(row.color),
        .lpCustColors = self->custom_colors,
        .Flags = CC_RGBINIT | CC_FULLOPEN,
    };
    if (!ChooseColorW(&choice))
    {
        /* キャンセルは何もしない（決定 4）。 */
        return;
    }
    enum folio_state_outcome outcome =
        folio_state_recolor_category(self->state, row.category, to_rgb_color(choice.rgbResult));
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(GetAncestor(self->handle, GA_ROOT), outcome, self->state);
        return;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    /* 右ペインの頭の色も台帳から引く。本文は触らない（決定 6）。 */
    InvalidateRect(GetParent(self->handle), nullptr, FALSE);
}

static enum ui_text menu_label(enum drawer_menu_item item)
{
    switch (item)
    {
    case DRAWER_MENU_RECOLOR:
        return UI_TEXT_MENU_RECOLOR;
    case DRAWER_MENU_RENAME_NOTE:
        return UI_TEXT_MENU_RENAME_NOTE;
    case DRAWER_MENU_NEW_CATEGORY:
        return UI_TEXT_MENU_NEW_CATEGORY;
    case DRAWER_MENU_NEW_NOTE:
        return UI_TEXT_MENU_NEW_NOTE_IN_CATEGORY;
    case DRAWER_MENU_TRASH_NOTE:
        return UI_TEXT_MENU_TRASH_NOTE;
    case DRAWER_MENU_DELETE_CATEGORY:
        return UI_TEXT_MENU_DELETE_CATEGORY;
    case DRAWER_MENU_RENAME_CATEGORY:
        return UI_TEXT_MENU_RENAME_CATEGORY;
    }
    return UI_TEXT_MENU_NEW_CATEGORY;
}

/* 項目を 1 つ足す。メニュー ID は値 + 1（0 は「何も選ばれなかった」）。 */
static bool append_item(HMENU menu, enum drawer_menu_item item, enum folio_language language)
{
    char16_t units[draw_unit_limit];
    if (wide_units(ui_text_line(menu_label(item), language), units) == 0)
    {
        return false;
    }
    return AppendMenuW(menu, MF_STRING, (UINT_PTR)item + 1, units) != 0;
}

/* 押した所に効く項目を並べる（ADR 0039 の決定 11）。on_row が false なら行の無い所。
 * 項目は灰色にしない。断る理由は実行したときに application が返す（ARC-011）。 */
static bool fill_menu(HMENU menu, bool on_row, struct drawer_row row, enum folio_language language)
{
    if (!on_row)
    {
        return append_item(menu, DRAWER_MENU_NEW_CATEGORY, language);
    }
    switch (row.kind)
    {
    case DRAWER_ROW_CATEGORY:
        return append_item(menu, DRAWER_MENU_NEW_NOTE, language) &&
               AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) != 0 &&
               append_item(menu, DRAWER_MENU_RECOLOR, language) &&
               append_item(menu, DRAWER_MENU_RENAME_CATEGORY, language) &&
               append_item(menu, DRAWER_MENU_DELETE_CATEGORY, language) &&
               AppendMenuW(menu, MF_SEPARATOR, 0, nullptr) != 0 &&
               append_item(menu, DRAWER_MENU_NEW_CATEGORY, language);
    case DRAWER_ROW_NOTE:
        return append_item(menu, DRAWER_MENU_RENAME_NOTE, language) &&
               append_item(menu, DRAWER_MENU_TRASH_NOTE, language);
    }
    return false;
}

/* 選ばれた項目を実行する。色だけはドロワーが今までと同じ色の選択を開き（ADR 0039 の決定 12 の
 * 例外）、ほかは主窓の既存の受け口へ対象付きで同期に渡す（ARC-001）。 */
static void run_item(struct drawer_window *_Nonnull self, enum drawer_menu_item item,
                     struct drawer_row row)
{
    switch (item)
    {
    case DRAWER_MENU_RECOLOR:
        choose_color(self, row);
        return;
    case DRAWER_MENU_RENAME_NOTE:
    case DRAWER_MENU_NEW_CATEGORY:
    case DRAWER_MENU_NEW_NOTE:
    case DRAWER_MENU_TRASH_NOTE:
    case DRAWER_MENU_DELETE_CATEGORY:
    case DRAWER_MENU_RENAME_CATEGORY:
        break;
    }
    struct note_ref target = {.category = row.category, .note = row.note};
    SendMessageW(GetParent(self->handle), folio_message_drawer_menu, (WPARAM)item, (LPARAM)&target);
}

/* 右ボタンを離した位置の行に効く操作のメニューを出す（FR-038・ADR 0039 の決定 11 / 12。
 * ADR 0010 の 2026-10-07 の補正）。頭の帯では出さない。左ボタンを押している間は捕捉中の再入を
 * 避けて無視する。右クリックは選択もカーソルもフォーカスも動かさない。 */
static void show_menu(struct drawer_window *_Nonnull self, LPARAM position)
{
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (self->pressed || self->header_pressed != DRAWER_HEADER_NONE || point.y < band_height(self))
    {
        return;
    }
    struct drawer_layout *_Nullable layout = nullptr;
    if (!current_layout(self, &layout))
    {
        return;
    }
    size_t index = 0;
    bool found = drawer_layout_hit(layout, point.y, &index);
    struct drawer_row row = found ? drawer_layout_row(layout, index) : (struct drawer_row){0};
    drawer_layout_destroy(layout);
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr || !fill_menu(menu, found, row, folio_state_language(self->state)))
    {
        if (menu != nullptr)
        {
            DestroyMenu(menu);
        }
        failure_box_show(GetAncestor(self->handle, GA_ROOT), FOLIO_STATE_OUT_OF_MEMORY,
                         self->state);
        return;
    }
    ClientToScreen(self->handle, &point);
    int chosen =
        TrackPopupMenuEx(menu, TPM_RETURNCMD, point.x, point.y, GetParent(self->handle), nullptr);
    DestroyMenu(menu);
    if (chosen > 0)
    {
        run_item(self, (enum drawer_menu_item)(chosen - 1), row);
    }
}

/* 押した行を覚えて捕捉する。クリックの確定は離すときに行う（ADR 0007 の決定 6）。
 * 押した側が区画を取るので、フォーカスは主窓へ渡す（ドロワー自身は取らない・ADR 0013 の決定 1）。
 */
static void press_row(struct drawer_window *_Nonnull self, int y)
{
    SendMessageW(GetParent(self->handle), folio_message_index_focus, 0, 0);
    struct drawer_layout *_Nullable layout = nullptr;
    if (!current_layout(self, &layout))
    {
        return;
    }
    size_t index = 0;
    if (drawer_layout_hit(layout, y, &index))
    {
        self->pressed = true;
        self->pressed_row = index;
        self->pressed_y = y;
        self->moved = false;
        self->dragging = false;
        SetCapture(self->handle);
    }
    drawer_layout_destroy(layout);
}

static RECT header_button_rect(const struct drawer_window *_Nonnull self,
                               enum drawer_header_button button)
{
    switch (button)
    {
    case DRAWER_HEADER_NONE:
        return (RECT){0, 0, 0, 0};
    case DRAWER_HEADER_NEW_CATEGORY:
        return new_category_rect(self);
    }
    return (RECT){0, 0, 0, 0};
}

static bool press_header(struct drawer_window *_Nonnull self, POINT point)
{
    RECT button = new_category_rect(self);
    if (!PtInRect(&button, point))
    {
        return false;
    }
    self->header_pressed = DRAWER_HEADER_NEW_CATEGORY;
    self->header_inside = true;
    SendMessageW(GetParent(self->handle), folio_message_index_focus,
                 (WPARAM)ACCESSIBLE_NEW_CATEGORY, 0);
    SetCapture(self->handle);
    InvalidateRect(self->handle, &button, FALSE);
    return true;
}

static void press(struct drawer_window *_Nonnull self, POINT point)
{
    if (self->pressed || self->header_pressed != DRAWER_HEADER_NONE)
    {
        return;
    }
    if (press_header(self, point))
    {
        return;
    }
    RECT filter = filter_rect(self);
    if (PtInRect(&filter, point))
    {
        SendMessageW(GetParent(self->handle), folio_message_index_focus, (WPARAM)ACCESSIBLE_FILTER,
                     0);
        return;
    }
    if (point.y >= band_height(self))
    {
        press_row(self, point.y);
    }
}

/* 帯のボタンの hover と、押したまま外へ出たときの見た目（ADR0060 の決定 3・4）。 */
static void track_header(struct drawer_window *_Nonnull self, POINT point)
{
    RECT button = new_category_rect(self);
    bool over = PtInRect(&button, point) != 0;
    if (self->header_pressed != DRAWER_HEADER_NONE)
    {
        if (over != self->header_inside)
        {
            self->header_inside = over;
            InvalidateRect(self->handle, &button, FALSE);
        }
        return;
    }
    if (!self->header_tracking)
    {
        TRACKMOUSEEVENT track = {
            .cbSize = sizeof track, .dwFlags = TME_LEAVE, .hwndTrack = self->handle};
        self->header_tracking = TrackMouseEvent(&track) != 0;
    }
    if (over != self->header_hover)
    {
        self->header_hover = over;
        InvalidateRect(self->handle, &button, FALSE);
    }
}

static void leave_header(struct drawer_window *_Nonnull self)
{
    self->header_tracking = false;
    if (self->header_hover)
    {
        self->header_hover = false;
        RECT button = new_category_rect(self);
        InvalidateRect(self->handle, &button, FALSE);
    }
}

/* 捕捉中の移動。しきい値を超えたらドラッグに入り、落とし先を core に決めさせる。 */
static void drag(struct drawer_window *_Nonnull self, int y)
{
    if (!self->pressed)
    {
        return;
    }
    self->pointer_y = y;
    int travel = y - self->pressed_y;
    int threshold = GetSystemMetricsForDpi(SM_CYDRAG, GetDpiForWindow(self->handle));
    if (!self->moved && travel > -threshold && travel < threshold)
    {
        return;
    }
    self->moved = true;
    if (folio_state_filtering(self->state))
    {
        /* 絞り込み中は並び替えができないので、挿入線も出さない（ADR 0024 の決定 4）。
         * 動いたことだけを覚えて、離してもクリックにしない（補正 2）。 */
        return;
    }
    struct drawer_layout *_Nullable layout = nullptr;
    if (!current_layout(self, &layout))
    {
        return;
    }
    if (self->pressed_row < drawer_layout_row_count(layout))
    {
        self->dragging = true;
        self->target = drawer_layout_drop(layout, self->pressed_row, y);
        InvalidateRect(self->handle, nullptr, FALSE);
    }
    drawer_layout_destroy(layout);
}

/* 捕捉を解いて、ドラッグなら並び替え、動かしていなければ今までどおりのクリック。
 * 閾値を超えて動かした操作は、並び替えに入れなかったときもクリックにしない（補正 2）。 */
static void release_row(struct drawer_window *_Nonnull self)
{
    bool pressed = self->pressed;
    bool moved = self->moved;
    bool dragging = self->dragging;
    size_t index = self->pressed_row;
    struct drop_target target = self->target;
    self->pressed = false;
    self->moved = false;
    self->dragging = false;
    ReleaseCapture();
    struct drawer_layout *_Nullable layout = nullptr;
    if (!pressed || !current_layout(self, &layout))
    {
        return;
    }
    bool valid = index < drawer_layout_row_count(layout);
    struct drawer_row row = valid ? drawer_layout_row(layout, index) : (struct drawer_row){0};
    drawer_layout_destroy(layout);
    if (!valid)
    {
        return;
    }
    if (dragging)
    {
        apply_drop(self, row, target);
        return;
    }
    if (moved)
    {
        return;
    }
    act_on_row(self, row);
}

static void run_header(struct drawer_window *_Nonnull self, enum drawer_header_button button)
{
    struct drawer_row row = {0};
    switch (button)
    {
    case DRAWER_HEADER_NONE:
        return;
    case DRAWER_HEADER_NEW_CATEGORY:
        run_item(self, DRAWER_MENU_NEW_CATEGORY, row);
        return;
    }
}

static void release(struct drawer_window *_Nonnull self, POINT point)
{
    enum drawer_header_button pressed = self->header_pressed;
    if (pressed == DRAWER_HEADER_NONE)
    {
        release_row(self);
        return;
    }
    self->header_pressed = DRAWER_HEADER_NONE;
    RECT button = header_button_rect(self, pressed);
    ReleaseCapture();
    InvalidateRect(self->handle, &button, FALSE);
    if (PtInRect(&button, point))
    {
        self->header_hover = false; /* ダイアログの後に hover の枠を残さない */
        run_header(self, pressed);
    }
}

/* スクロールの意図を出し、結果を写す（ADR 0009 の決定 8）。
 * ドラッグ中は行が動いたので、同じポインタ位置で落とし先を引き直す（決定 6）。 */
static void scroll_by(struct drawer_window *_Nonnull self, int delta)
{
    enum folio_state_outcome outcome =
        folio_state_scroll_drawer(self->state, metrics_for(self), delta);
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(GetAncestor(self->handle, GA_ROOT), outcome, self->state);
        return;
    }
    if (self->dragging)
    {
        drag(self, self->pointer_y);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
}

/* ホイール 1 メッセージぶんの画素。上へ回す（正の delta）と内容が下がる＝量は減る（決定 4）。 */
static void wheel(struct drawer_window *_Nonnull self, WPARAM wparam)
{
    struct drawer_metrics metrics = metrics_for(self);
    scroll_by(self, -MulDiv(GET_WHEEL_DELTA_WPARAM(wparam), wheel_rows * metrics.row_height,
                            WHEEL_DELTA));
}

/* 鍵 1 つぶんの画素。扱わない鍵は 0（決定 4）。 */
static int key_step(const struct drawer_window *_Nonnull self, WPARAM key)
{
    struct drawer_metrics metrics = metrics_for(self);
    switch (key)
    {
    case VK_UP:
        return -metrics.row_height;
    case VK_DOWN:
        return metrics.row_height;
    case VK_PRIOR:
        return -(metrics.viewport_height - metrics.row_height);
    case VK_NEXT:
        return metrics.viewport_height - metrics.row_height;
    default:
        /* Win32 の仮想キーは開いた OS の集合（C-017）。 */
        return 0;
    }
}

/* 捕捉を取り上げられたら、線も覚えた行も捨てる。 */
static void cancel(struct drawer_window *_Nonnull self)
{
    self->header_pressed = DRAWER_HEADER_NONE;
    self->pressed = false;
    self->moved = false;
    self->dragging = false;
    InvalidateRect(self->handle, nullptr, FALSE);
}

static struct drawer_window *_Nullable self_of(HWND window)
{
    return (struct drawer_window *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

static LRESULT drawer_message(struct drawer_window *_Nonnull self, UINT message, WPARAM wparam,
                              LPARAM lparam)
{
    switch (message)
    {
    case WM_PAINT:
        paint(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        press(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return 0;
    case WM_MOUSEMOVE:
        track_header(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        drag(self, GET_Y_LPARAM(lparam));
        return 0;
    case WM_MOUSELEAVE:
        leave_header(self);
        return 0;
    case WM_LBUTTONUP:
        release(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return 0;
    case WM_RBUTTONUP:
        show_menu(self, lparam);
        return 0;
    case WM_MOUSEWHEEL:
        wheel(self, wparam);
        return 0;
    case WM_CAPTURECHANGED:
        cancel(self);
        return 0;
    case WM_DPICHANGED_AFTERPARENT:
        refresh_fonts(self, GetDpiForWindow(self->handle));
        InvalidateRect(self->handle, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        release_fonts(self);
        self->handle = nullptr;
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(self->handle, message, wparam, lparam);
    }
}

static LRESULT CALLBACK drawer_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct drawer_window *_Nullable self = self_of(window);
    if (message == WM_NCDESTROY)
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        return DefWindowProcW(window, message, wparam, lparam);
    }
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
    accessibility_host_changed(self->accessibility, message);
    if (message == WM_GETOBJECT)
    {
        return accessibility_host_object(self->accessibility, ACCESSIBLE_DRAWER, wparam, lparam);
    }
    if (message == WM_ENTERMENULOOP || message == WM_EXITMENULOOP)
    {
        return SendMessageW(GetParent(window), message, wparam, lparam);
    }
    return drawer_message(self, message, wparam, lparam);
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
                                                struct folio_state *_Nonnull state,
                                                struct accessibility_host *_Nonnull accessibility,
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
    self->accessibility = accessibility;
    self->palette = folio_palette_from(folio_state_palette(state));
    /* 未設定のカスタム色は白で始める（Windows の慣例。黒だと設定済みに見える）。
     * 実行中だけ持ち、保存しない（ADR 0010 の決定 4）。 */
    for (size_t slot = 0; slot < sizeof self->custom_colors / sizeof self->custom_colors[0]; ++slot)
    {
        self->custom_colors[slot] = RGB(255, 255, 255);
    }
    HWND handle = CreateWindowExW(0, class_name, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0,
                                  0, 0, parent, nullptr, instance, self);
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

RECT drawer_window_filter_rect(const struct drawer_window *_Nonnull drawer)
{
    RECT bounds = filter_rect(drawer);
    if (drawer->handle != nullptr)
    {
        MapWindowPoints(drawer->handle, GetParent(drawer->handle), (POINT *)&bounds, 2);
    }
    return bounds;
}

void drawer_window_scroll_key(struct drawer_window *_Nonnull drawer, WPARAM key)
{
    if (drawer->handle == nullptr)
    {
        return;
    }
    int step = key_step(drawer, key);
    if (step != 0)
    {
        scroll_by(drawer, step);
    }
}

void drawer_window_reveal_cursor(struct drawer_window *_Nonnull drawer)
{
    if (drawer->handle == nullptr)
    {
        return;
    }
    enum folio_state_outcome outcome =
        folio_state_reveal_cursor(drawer->state, metrics_for(drawer));
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(GetAncestor(drawer->handle, GA_ROOT), outcome, drawer->state);
        return;
    }
    InvalidateRect(drawer->handle, nullptr, FALSE);
}

/* 言語の切り替えで書体を作り直して全面を描き直す（ADR 0032 の決定 6(b)）。
 * 文言そのものは描くたびに ui_text_line を引くので、ここは face だけを配り直す。 */
void drawer_window_refont(struct drawer_window *_Nonnull drawer)
{
    if (drawer->handle == nullptr)
    {
        return;
    }
    refresh_fonts(drawer, GetDpiForWindow(drawer->handle));
    InvalidateRect(drawer->handle, nullptr, FALSE);
}

/* テーマの切り替えで palette の写しを取り直して全面を描き直す（ADR 0031 の決定 6）。 */
void drawer_window_recolor(struct drawer_window *_Nonnull drawer)
{
    drawer->palette = folio_palette_from(folio_state_palette(drawer->state));
    if (drawer->handle != nullptr)
    {
        InvalidateRect(drawer->handle, nullptr, FALSE);
    }
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
    drawer_layout_destroy(drawer->accessible_layout);
    free(drawer);
}

static HRESULT accessible_layout(struct drawer_window *_Nonnull self)
{
    if (self->handle == nullptr)
    {
        return accessibility_node_unavailable;
    }
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    uint64_t revision = folio_state_index_revision(self->state);
    if (self->accessible_layout == nullptr || revision == UINT64_MAX ||
        self->accessible_revision != revision || self->accessible_dpi != dpi ||
        self->accessible_height != client.bottom)
    {
        drawer_layout_destroy(self->accessible_layout);
        self->accessible_layout = nullptr;
        if (!current_layout(self, &self->accessible_layout))
        {
            return E_OUTOFMEMORY;
        }
        self->accessible_revision = revision;
        self->accessible_dpi = dpi;
        self->accessible_height = client.bottom;
    }
    drawer_layout_scroll(self->accessible_layout, folio_state_drawer_scroll(self->state));
    return S_OK;
}

static int accessible_compare(struct drawer_row row, const struct accessible_item *_Nonnull item)
{
    if (row.category != item->ref.category)
    {
        return row.category < item->ref.category ? -1 : 1;
    }
    bool category = row.kind == DRAWER_ROW_CATEGORY;
    bool target_category = item->kind == ACCESSIBLE_CATEGORY;
    if (category != target_category)
    {
        return category ? -1 : 1;
    }
    if (category || row.note == item->ref.note)
    {
        return 0;
    }
    return row.note < item->ref.note ? -1 : 1;
}

static bool accessible_row_index(const struct drawer_window *_Nonnull self,
                                 const struct accessible_item *_Nonnull item, size_t *_Nonnull out)
{
    size_t low = 0;
    size_t high = drawer_layout_row_count(self->accessible_layout);
    while (low < high)
    {
        size_t middle = low + (high - low) / 2;
        struct drawer_row row = drawer_layout_row(self->accessible_layout, middle);
        int comparison = accessible_compare(row, item);
        if (comparison == 0)
        {
            *out = middle;
            return true;
        }
        if (comparison < 0)
        {
            low = middle + 1;
        }
        else
        {
            high = middle;
        }
    }
    return false;
}

static struct accessible_item accessible_row_item(const struct drawer_window *_Nonnull self,
                                                  size_t index)
{
    struct drawer_row row = drawer_layout_row(self->accessible_layout, index);
    return (struct accessible_item){.kind = row.kind == DRAWER_ROW_CATEGORY ? ACCESSIBLE_CATEGORY
                                                                            : ACCESSIBLE_NOTE,
                                    .ref = {row.category, row.note},
                                    .revision = folio_state_row_revision(self->state)};
}

static void accessible_row_view(struct drawer_window *_Nonnull self, struct drawer_row row,
                                struct accessible_view *_Nonnull out)
{
    (void)wide_units(row.text, out->name);
    RECT client;
    GetClientRect(self->handle, &client);
    RECT bounds = {0, row.top, client.right, row.top + row.height};
    RECT viewport = {0, band_height(self), client.right, client.bottom};
    out->offscreen = !IntersectRect(&out->bounds, &bounds, &viewport);
    out->expanded = row.expanded;
    out->focusable = true;
    struct note_ref selected;
    out->selected = row.kind == DRAWER_ROW_NOTE && folio_state_selection(self->state, &selected) &&
                    selected.category == row.category && selected.note == row.note;
}

static RECT accessible_fixed_rect(const struct drawer_window *_Nonnull self,
                                  enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_NEW_CATEGORY)
    {
        return new_category_rect(self);
    }
    RECT client;
    GetClientRect(self->handle, &client);
    if (kind == ACCESSIBLE_TREE)
    {
        client.top = band_height(self);
    }
    return client;
}

static void accessible_scroll_view(struct drawer_window *_Nonnull self,
                                   struct accessible_view *_Nonnull out)
{
    int limit = drawer_layout_scroll_limit(self->accessible_layout);
    int scroll = folio_state_drawer_scroll(self->state);
    out->scroll = scroll < limit ? scroll : limit;
    out->scroll_limit = limit;
    out->viewport = out->bounds.bottom > out->bounds.top ? out->bounds.bottom - out->bounds.top : 0;
}

HRESULT drawer_window_accessible_read(struct drawer_window *_Nonnull self,
                                      const struct accessible_item *_Nonnull item,
                                      struct accessible_view *_Nonnull out)
{
    HRESULT result = accessible_layout(self);
    if (FAILED(result))
    {
        return result;
    }
    if (accessible_item_row(*item))
    {
        size_t index = 0;
        if (item->revision == UINT64_MAX ||
            item->revision != folio_state_row_revision(self->state) ||
            !accessible_row_index(self, item, &index))
        {
            return accessibility_node_unavailable;
        }
        accessible_row_view(self, drawer_layout_row(self->accessible_layout, index), out);
    }
    else
    {
        out->bounds = accessible_fixed_rect(self, item->kind);
    }
    if (item->kind == ACCESSIBLE_TREE)
    {
        accessible_scroll_view(self, out);
    }
    if (!IsRectEmpty(&out->bounds))
    {
        MapWindowPoints(self->handle, nullptr, (POINT *)&out->bounds, 2);
    }
    out->offscreen = out->offscreen || !IsWindowVisible(self->handle) || IsRectEmpty(&out->bounds);
    return S_OK;
}

static HRESULT accessible_category_edge(struct drawer_window *_Nonnull self,
                                        const struct accessible_item *_Nonnull item,
                                        enum accessible_navigation direction,
                                        struct accessible_item *_Nonnull out)
{
    size_t count = drawer_layout_row_count(self->accessible_layout);
    size_t index = 0;
    if (!accessible_row_index(self, item, &index) || index + 1 >= count)
    {
        return S_FALSE;
    }
    index += 1;
    struct drawer_row row = drawer_layout_row(self->accessible_layout, index);
    if (row.kind != DRAWER_ROW_NOTE || row.category != item->ref.category)
    {
        return S_FALSE;
    }
    if (direction == ACCESSIBLE_NAVIGATION_LAST)
    {
        while (index + 1 < count &&
               drawer_layout_row(self->accessible_layout, index + 1).category == row.category)
        {
            index += 1;
        }
    }
    *out = accessible_row_item(self, index);
    return S_OK;
}

static HRESULT accessible_edge(struct drawer_window *_Nonnull self,
                               const struct accessible_item *_Nonnull item,
                               enum accessible_navigation direction,
                               struct accessible_item *_Nonnull out)
{
    size_t count = drawer_layout_row_count(self->accessible_layout);
    if (count == 0 || folio_state_row_revision(self->state) == UINT64_MAX)
    {
        return S_FALSE;
    }
    if (item->kind == ACCESSIBLE_CATEGORY)
    {
        return accessible_category_edge(self, item, direction, out);
    }
    if (item->kind != ACCESSIBLE_TREE)
    {
        return S_FALSE;
    }
    size_t index = direction == ACCESSIBLE_NAVIGATION_FIRST ? 0 : count - 1;
    while (drawer_layout_row(self->accessible_layout, index).kind != DRAWER_ROW_CATEGORY)
    {
        index -= 1;
    }
    *out = accessible_row_item(self, index);
    return S_OK;
}

static bool accessible_same_level(struct accessible_item item, struct accessible_item candidate)
{
    return item.kind != ACCESSIBLE_NOTE ||
           (candidate.kind == ACCESSIBLE_NOTE && candidate.ref.category == item.ref.category);
}

static bool accessible_has_sibling(size_t index, size_t count, bool next)
{
    return next ? index + 1 < count : index > 0;
}

static HRESULT accessible_sibling(struct drawer_window *_Nonnull self,
                                  const struct accessible_item *_Nonnull item,
                                  enum accessible_navigation direction,
                                  struct accessible_item *_Nonnull out)
{
    size_t index = 0;
    if (!accessible_row_index(self, item, &index))
    {
        return S_FALSE;
    }
    size_t count = drawer_layout_row_count(self->accessible_layout);
    bool next = direction == ACCESSIBLE_NAVIGATION_NEXT;
    while (accessible_has_sibling(index, count, next))
    {
        index = next ? index + 1 : index - 1;
        struct accessible_item candidate = accessible_row_item(self, index);
        if (!accessible_same_level(*item, candidate))
        {
            return S_FALSE;
        }
        if (candidate.kind == item->kind)
        {
            *out = candidate;
            return S_OK;
        }
    }
    return S_FALSE;
}

HRESULT drawer_window_accessible_navigate(struct drawer_window *_Nonnull self,
                                          const struct accessible_item *_Nonnull item,
                                          enum accessible_navigation direction,
                                          struct accessible_item *_Nonnull out)
{
    HRESULT result = accessible_layout(self);
    if (FAILED(result))
    {
        return result;
    }
    switch (direction)
    {
    case ACCESSIBLE_NAVIGATION_PARENT:
        *out = (struct accessible_item){.kind = ACCESSIBLE_DRAWER};
        if (item->kind == ACCESSIBLE_CATEGORY)
        {
            out->kind = ACCESSIBLE_TREE;
        }
        if (item->kind == ACCESSIBLE_NOTE)
        {
            *out = (struct accessible_item){.kind = ACCESSIBLE_CATEGORY,
                                            .ref = {item->ref.category, 0},
                                            .revision = item->revision};
        }
        return S_OK;
    case ACCESSIBLE_NAVIGATION_FIRST:
    case ACCESSIBLE_NAVIGATION_LAST:
        return accessible_edge(self, item, direction, out);
    case ACCESSIBLE_NAVIGATION_NEXT:
    case ACCESSIBLE_NAVIGATION_PREVIOUS:
        return accessible_sibling(self, item, direction, out);
    }
    return E_INVALIDARG;
}

HRESULT drawer_window_accessible_at(struct drawer_window *_Nonnull self, POINT point,
                                    struct accessible_item *_Nonnull out)
{
    HRESULT result = accessible_layout(self);
    if (FAILED(result))
    {
        return result;
    }
    ScreenToClient(self->handle, &point);
    RECT bounds = new_category_rect(self);
    *out = (struct accessible_item){.kind = ACCESSIBLE_DRAWER};
    if (PtInRect(&bounds, point))
    {
        out->kind = ACCESSIBLE_NEW_CATEGORY;
        return S_OK;
    }
    bounds = filter_rect(self);
    if (PtInRect(&bounds, point))
    {
        return S_FALSE; /* native EDITのhostへ委ねる。 */
    }
    size_t index = 0;
    if (drawer_layout_hit(self->accessible_layout, point.y, &index) &&
        folio_state_row_revision(self->state) != UINT64_MAX)
    {
        *out = accessible_row_item(self, index);
        return S_OK;
    }
    if (point.y >= band_height(self))
    {
        out->kind = ACCESSIBLE_TREE;
    }
    return S_OK;
}

enum folio_state_outcome
drawer_window_accessible_scroll(struct drawer_window *_Nonnull self,
                                const struct accessible_request *_Nonnull request)
{
    enum folio_state_outcome result = FOLIO_STATE_READY;
    if (request->action == ACCESSIBLE_ACTION_REVEAL)
    {
        struct drawer_cursor cursor = {.kind = request->item.kind == ACCESSIBLE_CATEGORY
                                                   ? DRAWER_ROW_CATEGORY
                                                   : DRAWER_ROW_NOTE,
                                       .ref = request->item.ref};
        result = folio_state_reveal_row(self->state, metrics_for(self), cursor);
    }
    if (request->action == ACCESSIBLE_ACTION_SCROLL)
    {
        if (FAILED(accessible_layout(self)))
        {
            return FOLIO_STATE_OUT_OF_MEMORY;
        }
        int limit = drawer_layout_scroll_limit(self->accessible_layout);
        int scroll = folio_state_drawer_scroll(self->state);
        scroll = scroll < limit ? scroll : limit;
        result = folio_state_scroll_drawer(self->state, metrics_for(self),
                                           request->scroll_target - scroll);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    return result;
}
