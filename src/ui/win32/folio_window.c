#include "folio_window.h"

#include "drawer_window.h"
#include "failure_box.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "folio_step.h"
#include "note_pane.h"
#include "note_ref.h"
#include "utf16_text.h"

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
    bool pending_g; /* 直前の文字の鍵が g だった（gg の 2 打・ADR 0013 の決定 2） */
};

static const wchar_t class_name[] = L"NeNeFolioWindow";
static const wchar_t mono_face[] = L"Consolas";
static const wchar_t view_label[] = L"閲覧";
static const wchar_t edit_label[] = L"編集";

/* Ctrl+S が WM_CHAR で届く制御文字（GetKeyState を読まない・ARC-007）。 */
constexpr WPARAM store_character = 0x13;

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
constexpr int base_chip_inset = 16;
constexpr int base_chip_gap = 8;
constexpr int base_chip_radius = 3;
constexpr int base_mono_font = 11;
constexpr int base_tracking = 1;
/* 省略しても残すノート名の幅（ADR 0011 の決定 4）。 */
constexpr int base_breadcrumb_note = 48;

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

/* 番号（2 桁・カテゴリ色）を描き、描いた幅を返す。番号は縮めない（ADR 0011 の決定 4）。 */
static int draw_ordinal(HDC device, size_t ordinal, struct rgb_color color, RECT bounds)
{
    wchar_t digits[3] = {(wchar_t)(L'0' + (ordinal / 10) % 10), (wchar_t)(L'0' + ordinal % 10),
                         L'\0'};
    SetTextColor(device, RGB(color.red, color.green, color.blue));
    RECT measured = bounds;
    DrawTextW(device, digits, 2, &measured, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_CALCRECT);
    DrawTextW(device, digits, 2, &bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    return measured.right - measured.left;
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
    int right = caption.right - scale(base_chip_inset, dpi);
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

/* 頭のパンくず: 番号（カテゴリ色）・カテゴリ・/・ノート。選択が無ければ何も描かない。
 * 右端は札 2 つの手前で、残りをカテゴリ名とノート名に割り当てる（ADR 0011 の決定 4）。 */
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
    cursor.left += draw_ordinal(device, title.ordinal, title.color, cursor) + gap;
    int separator = measure_utf8(device, "/");
    int category = measure_utf8(device, title.category);
    int note = measure_utf8(device, title.note);
    int budget = chip_rect(self, device, PANE_MODE_VIEW).left - gap * 3 - cursor.left - separator;
    breadcrumb_room(budget < 0 ? 0 : budget, scale(base_breadcrumb_note, dpi), &category, &note);
    SetTextColor(device, self->palette.header_text);
    cursor.right = cursor.left + category;
    draw_utf8(device, title.category, cursor);
    cursor.left += category + gap;
    SetTextColor(device, self->palette.border);
    cursor.right = cursor.left + separator;
    draw_utf8(device, "/", cursor);
    cursor.left += separator + gap;
    SetTextColor(device, self->palette.current_text);
    cursor.right = cursor.left + note;
    draw_utf8(device, title.note, cursor);
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
    enum pane_mode chip = PANE_MODE_VIEW;
    if (chip_at(self, client, &chip))
    {
        return HTCLIENT;
    }
    bool in_caption = point.x >= window.left + scale(base_drawer_width, dpi) &&
                      point.y < window.top + scale(base_caption_height, dpi);
    return in_caption ? HTCAPTION : HTCLIENT;
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
    SetBkMode(device, TRANSPARENT);
    SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    draw_breadcrumb(self, device, caption_rect(self));
    draw_chip(self, device, PANE_MODE_VIEW);
    draw_chip(self, device, PANE_MODE_EDIT);
    SetTextCharacterExtra(device, 0);
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

/* 編集中なら保存して閲覧へ戻す。閲覧中なら何もしない（ADR 0006 の決定 1・「閲覧」の札と WM_CLOSE
 * だけが使う・ADR 0013 の決定 4）。 */
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

/* 「閲覧」の札・別ノートの選択・窓を閉じる操作の共通の出口。保存できなければ 1 行を出す。 */
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

/* 無効な側の札のクリックだけが意図になる。 */
static void click_caption(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
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

/* RichEdit の鍵の通知（EN_MSGFILTER）。Ctrl+S と Esc だけを意図にして既定処理を止める。 */
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
        /* 失敗しても既定処理へは渡さない（1 行は store_edit が出す）。 */
        (void)store_edit(self);
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
    bool twice = self->pending_g && character == 'g';
    self->pending_g = character == 'g' && !twice;
    switch (character)
    {
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
        /* Ctrl+S は索引の区画でも効く（本文の EN_MSGFILTER と同じ保存）。閲覧中は何もしない。 */
        if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
        {
            (void)store_edit(self);
        }
        break;
    default:
        /* WM_CHAR の文字は開いた集合（C-017）。 */
        break;
    }
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
        if (leave_edit(self))
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
