#include "drawer_window.h"

#include "drawer_layout.h"
#include "failure_box.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "note_ref.h"
#include "utf16_text.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

/* ドラッグ中の一時状態はここだけが持つ（ARC-004 / ARC-005 の許可区画）。 */
struct drawer_window
{
    HWND _Nullable handle;
    struct folio_state *_Nonnull state;
    struct folio_palette palette;
    HFONT _Nullable category_font;
    HFONT _Nullable note_font;
    HFONT _Nullable mono_font;
    bool pressed; /* 左ボタンを押した行を覚えているか */
    size_t pressed_row;
    int pressed_y;             /* 押したときの y。しきい値の判定に使う */
    int pointer_y;             /* 最後に見たポインタの y。スクロール中に落とし先を引き直す */
    bool dragging;             /* しきい値を超えて動かしているか */
    struct drop_target target; /* dragging のときの落とし先 */
};

static const wchar_t class_name[] = L"NeNeFolioDrawer";
static const wchar_t text_face[] = L"Yu Gothic UI";
static const wchar_t mono_face[] = L"Consolas";
static const wchar_t header_label[] = L"NENE FOLIO";

/* 96 DPI での寸法（デザイン「案2 堅」）。描くときに DPI で拡大する（FR-013）。 */
constexpr int base_dpi = 96;
constexpr int base_header_height = 44;
constexpr int base_header_indent = 16;
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

static void refresh_fonts(struct drawer_window *_Nonnull self, UINT dpi)
{
    release_fonts(self);
    self->category_font = create_font(dpi, base_category_font, FW_BOLD, text_face);
    self->note_font = create_font(dpi, base_note_font, FW_NORMAL, text_face);
    self->mono_font = create_font(dpi, base_mono_font, FW_NORMAL, mono_face);
}

/* いまの DPI と client の高さで寸法を測る。スクロール上限は core がここから決める（FR-012）。 */
static struct drawer_metrics metrics_for(const struct drawer_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT client = {0, 0, 0, 0};
    GetClientRect(self->handle, &client);
    struct drawer_metrics metrics = {
        .top_padding = scale(base_header_height, dpi),
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

/* UTF-8 を 1 行で描く。文字の伸びは呼び出し側が SetTextCharacterExtra で決める。 */
static void draw_utf8(HDC device, const char *_Nonnull text, RECT bounds, UINT format)
{
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(text, strlen(text), &wide) != UTF16_TEXT_CONVERTED)
    {
        return;
    }
    DrawTextW(device, utf16_text_units(wide), (int)utf16_text_length(wide), &bounds,
              format | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    utf16_text_destroy(wide);
}

static void fill_rect(HDC device, RECT bounds, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(device, &bounds, brush);
    DeleteObject(brush);
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

/* 頭の帯: 左に NENE FOLIO、右にノートの総数。 */
static void draw_header(const struct drawer_window *_Nonnull self, HDC device, int width)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int inset = scale(base_header_indent, dpi);
    RECT bounds = {inset, 0, width - inset, scale(base_header_height, dpi)};
    SelectObject(device, self->mono_font);
    SetTextColor(device, self->palette.header_text);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    DrawTextW(device, header_label, -1, &bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SetTextCharacterExtra(device, 0);
    wchar_t count[24];
    int written = format_count(folio_state_note_count(self->state), count);
    DrawTextW(device, count, written, &bounds, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
}

/* カテゴリ行: 番号（カテゴリ色・等幅）、名前（太字・字間広め）、右端に − / +。 */
static void draw_category(const struct drawer_window *_Nonnull self, HDC device,
                          struct drawer_row row, int width)
{
    UINT dpi = GetDpiForWindow(self->handle);
    wchar_t ordinal[3] = {(wchar_t)(L'0' + (row.ordinal / 10) % 10),
                          (wchar_t)(L'0' + row.ordinal % 10), L'\0'};
    RECT number = {row.indent, row.top, width, row.top + row.height};
    SelectObject(device, self->mono_font);
    SetTextColor(device, to_colorref(row.color));
    DrawTextW(device, ordinal, 2, &number, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    RECT name = {row.indent + scale(base_name_offset, dpi), row.top,
                 width - scale(base_right_inset, dpi) * 2, row.top + row.height};
    SelectObject(device, self->category_font);
    SetTextColor(device, self->palette.category_text);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    draw_utf8(device, row.text, name, DT_END_ELLIPSIS);
    SetTextCharacterExtra(device, 0);
    RECT mark = {row.indent, row.top, width - scale(base_right_inset, dpi), row.top + row.height};
    SelectObject(device, self->mono_font);
    SetTextColor(device, self->palette.header_text);
    DrawTextW(device, row.expanded ? L"\x2212" : L"+", 1, &mark,
              DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
}

/* ノート行: 選択中なら面を敷き、右端にカテゴリ色の角を置く。 */
static void draw_note(const struct drawer_window *_Nonnull self, HDC device, struct drawer_row row,
                      int width)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int inset = scale(base_right_inset, dpi);
    if (row.selected)
    {
        RECT face = {0, row.top, width, row.top + row.height};
        fill_rect(device, face, self->palette.selected_background);
        int mark = scale(base_mark_size, dpi);
        int middle = row.top + row.height / 2;
        RECT square = {width - inset - mark, middle - mark / 2, width - inset,
                       middle - mark / 2 + mark};
        fill_rect(device, square, to_colorref(row.color));
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
    fill_rect(device, line, to_colorref(row.color));
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
    int start = above ? scale(base_header_height, dpi) : client.bottom - height;
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

/* 32 bit・top-down の DIB セクション。画素は 0x00RRGGBB で並ぶ（ADR 0009 の決定 7）。 */
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
        draw_header(self, device, client.right);
        return;
    }
    int header = scale(base_header_height, GetDpiForWindow(self->handle));
    IntersectClipRect(device, 0, header, client.right, client.bottom);
    draw_rows(self, device, layout, client.right);
    SelectClipRgn(device, nullptr);
    draw_header(self, device, client.right);
    /* 画素へ触る前に GDI の溜めを吐き出す。 */
    GdiFlush();
    if (drawer_layout_overflow_above(layout))
    {
        draw_fade(self, pixels, client, true);
    }
    if (drawer_layout_overflow_below(layout))
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

/* 別のノートを選ぶ前に、主窓へ「編集中なら先に保存」を頼む（ADR 0006 の決定 5）。 */
static enum folio_state_outcome select_note(struct drawer_window *_Nonnull self,
                                            struct drawer_row row)
{
    HWND parent = GetParent(self->handle);
    /* LRESULT で返る値は主窓が入れた enum folio_state_outcome（Win32 の境界）。 */
    enum folio_state_outcome outcome =
        (enum folio_state_outcome)SendMessageW(parent, folio_message_edit_flush, 0, 0);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    outcome = folio_state_select_note(self->state, row.category, row.note);
    if (outcome == FOLIO_STATE_READY)
    {
        SendMessageW(parent, folio_message_selection_changed, 0, 0);
    }
    return outcome;
}

/* 行への意図を application へ渡し、結果を写す。 */
static void act_on_row(struct drawer_window *_Nonnull self, struct drawer_row row)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    switch (row.kind)
    {
    case DRAWER_ROW_CATEGORY:
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
    failure_box_show(self->handle, outcome);
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
        failure_box_show(self->handle, outcome);
        return;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    /* 右ペインの頭のカテゴリ番号が変わりうる。本文は編集中でも触らない（ADR 0007 の決定 7）。 */
    InvalidateRect(GetParent(self->handle), nullptr, FALSE);
}

/* 押した行を覚えて捕捉する。クリックの確定は離すときに行う（ADR 0007 の決定 6）。 */
static void press(struct drawer_window *_Nonnull self, int y)
{
    /* 頭の帯はドロワーの装飾であって行ではない。スクロールで帯の下へ潜った行を掴ませない
     * （描画も同じ境界でクリップしている・ADR 0009 の決定 7）。 */
    if (y < scale(base_header_height, GetDpiForWindow(self->handle)))
    {
        return;
    }
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
        self->dragging = false;
        SetCapture(self->handle);
    }
    drawer_layout_destroy(layout);
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
    if (!self->dragging && travel > -threshold && travel < threshold)
    {
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

/* 捕捉を解いて、ドラッグなら並び替え、動かしていなければ今までどおりのクリック。 */
static void release(struct drawer_window *_Nonnull self)
{
    bool pressed = self->pressed;
    bool dragging = self->dragging;
    size_t index = self->pressed_row;
    struct drop_target target = self->target;
    self->pressed = false;
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
    act_on_row(self, row);
}

/* スクロールの意図を出し、結果を写す（ADR 0009 の決定 8）。
 * ドラッグ中は行が動いたので、同じポインタ位置で落とし先を引き直す（決定 6）。 */
static void scroll_by(struct drawer_window *_Nonnull self, int delta)
{
    enum folio_state_outcome outcome =
        folio_state_scroll_drawer(self->state, metrics_for(self), delta);
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, outcome);
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
    self->pressed = false;
    self->dragging = false;
    InvalidateRect(self->handle, nullptr, FALSE);
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
    case WM_LBUTTONDOWN:
        press(self, GET_Y_LPARAM(lparam));
        return 0;
    case WM_MOUSEMOVE:
        drag(self, GET_Y_LPARAM(lparam));
        return 0;
    case WM_LBUTTONUP:
        release(self);
        return 0;
    case WM_MOUSEWHEEL:
        wheel(self, wparam);
        return 0;
    case WM_CAPTURECHANGED:
        cancel(self);
        return 0;
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
                                                struct folio_state *_Nonnull state,
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
    self->palette = folio_palette_for(folio_state_theme(state));
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
