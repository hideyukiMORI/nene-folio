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
#include "name_prompt.h"
#include "note_name.h"
#include "note_pane.h"
#include "note_ref.h"
#include "note_search.h"
#include "replace_edit.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <dwmapi.h>
#include <richedit.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

/* 置換の欄が持つ EDIT の数（パターンと置換文字列・ADR 0028 の決定 8(a)）。 */
constexpr size_t replace_input_count = 2;

struct folio_window
{
    HWND _Nullable handle;
    struct folio_state *_Nonnull state;
    struct folio_palette palette;
    struct drawer_window *_Nullable drawer;
    struct note_pane *_Nullable pane;
    HACCEL _Nullable commands;
    /* 検索欄にいるあいだだけ使う Enter / Shift+Enter（ADR 0023 の決定 4） */
    HACCEL _Nullable search_keys;
    HFONT _Nullable mono_font;
    HWND _Nullable command_layer;
    HWND _Nullable command_input;
    /* 置換の欄の 2 つの EDIT（0 = パターン・1 = 置換文字列）。入力面の「自分の欄」は
     * この 2 つと command_input の集合になる（ADR 0016 の 2026-09-22 の補正）。 */
    HWND _Nullable replace_inputs[replace_input_count];
    /* 入力面の EDIT はどれも同じ EDIT クラスなので、元の手続きは 1 つで足りる。 */
    WNDPROC _Nullable command_input_original;
    /* ドロワーの頭に常設する「すべてのノートを検索」の欄（ADR 0024 の決定 6）。主窓の子。 */
    HWND _Nullable filter_input;
    WNDPROC _Nullable filter_original;
    bool filter_composing;
    HWND _Nullable command_return_focus;
    HBRUSH _Nullable command_brush;
    enum command_surface_mode command_surface;
    size_t command_selection;
    size_t command_first;
    bool command_keys_visible;
    bool command_composing;
    bool command_unknown;
    /* `:%s` が 1 件も一致しなかった。欄は閉じず、置換の欄と同じ 1 行を出す（ADR 0028 の補正） */
    bool command_no_match;
    enum folio_state_outcome command_failure;
    /* 直近の下見の結果。READY なら「対象名 / k 件」を出す（ADR 0028 の決定 8(a)） */
    enum folio_state_outcome replace_outcome;
    /* 直近の検索の結果と「k / n 件」。判断は core が持ち、ここは表示値だけ（ADR 0023 の決定 4） */
    enum note_search_outcome search_outcome;
    size_t search_total;
    size_t search_ordinal;
    bool pending_g; /* 直前の文字の鍵が g だった（gg の 2 打・ADR 0013 の決定 2） */
    /* 等幅書体の数字 1 文字の幅。書体を作り直すときだけ測る（ADR 0026 の補正 7） */
    int digit_width;
};

static const wchar_t class_name[] = L"NeNeFolioWindow";
static const wchar_t command_layer_class[] = L"NeNeFolioCommandLayer";
static const wchar_t mono_face[] = L"Consolas";
/* 未完了の改名があるあいだ、パンくずが実ファイル名の代わりに出す文字（ADR 0022 の決定 2）。 */
static const char recovering_label[] = "名前変更の復旧待ち";
static const wchar_t view_label[] = L"閲覧";
static const wchar_t edit_label[] = L"編集";
static const wchar_t edit_class[] = L"EDIT";
/* 欄が空のあいだに薄い文字で出す用途（ADR 0024 の決定 6）。 */
static const wchar_t filter_placeholder[] = L"すべてのノートを検索";
static const char *_Nonnull const command_shortcuts[] = {
    "一覧  ↑↓ 選択 / Enter 実行 / Esc 戻る / Tab 説明",
    "編集本文  Ctrl+h/j/k/l ←/↓/↑/→",
    "全区画  F1 ヘルプ / Ctrl+P 一覧 / Ctrl+F このノート内を検索",
    "全区画  Ctrl+Shift+F すべてのノートを検索（索引を絞り込む）",
    "絞り込み欄  Esc・Enter 索引へ戻る（絞り込みは残る）/ 空にすると解除",
    "絞り込み中  並び替えと折畳/展開はできない（色・保存・編集は可）",
    "入力欄でも  Ctrl+N 新規 / Ctrl+S 保存 / Ctrl+Shift+S 別名保存 / F2 名前変更",
    "索引・閲覧本文  : コマンド / i 編集",
    "Ex  :set number / :set nonumber / :set nu!（編集中の原文の行番号）",
    "置換欄  Tab 欄を移動 / Enter 1 件 / Ctrl+Enter すべて / Esc 閉じる",
    "Ex  :%s/パターン/置換/[g]（g なしは各行の最初の一致・正規表現）",
    "索引・閲覧本文  / 次を検索 / ? 前を検索 / n・N 繰り返し",
    "全区画  F3 次の一致 / Shift+F3 前の一致（向きは変えない）",
    "検索欄  Enter 次 / Shift+Enter 逆 / Esc 閉じる（選択は残る）",
    "索引  j/k 次/前 / gg/G 先頭/末尾",
    "索引  h/l 折畳/展開 / Enter 本文",
    "索引  ↑↓ / PgUp/PgDn スクロール",
    "本文  Esc 保存して索引へ（編集は維持）",
};

/* Ctrl+S が WM_CHAR で届く制御文字（GetKeyState を読まない・ARC-007）。 */
constexpr WPARAM store_character = 0x13;
constexpr WPARAM palette_character = 0x10;
constexpr WPARAM new_character = 0x0E;
constexpr int command_control_id = 1;
constexpr WORD save_as_accelerator = 100;
constexpr WORD rename_accelerator = 101;
constexpr WORD find_accelerator = 102;
constexpr WORD search_next_accelerator = 103;
constexpr WORD search_previous_accelerator = 104;
constexpr WORD search_forward_accelerator = 105;
constexpr WORD search_backward_accelerator = 106;
constexpr WORD filter_accelerator = 107;
constexpr int filter_control_id = 2;
constexpr size_t filter_input_capacity = 128;
constexpr int base_filter_text_inset = 4;
constexpr size_t command_input_capacity = 256;
/* 置換の欄の 2 つの EDIT の control id（ADR 0028 の決定 8(a)）。1 = Ex/パレット・2 = 絞り込み・
 * 3 = 本文の RichEdit の次に続く。 */
constexpr int replace_pattern_control_id = 4;
constexpr int replace_replacement_control_id = 5;
/* 「k / n 件」と向きの言い換えを 1 行に組む領域（UTF-8）。 */
constexpr size_t search_status_capacity = 128;
/* 「対象名 / k 件」と失敗の 1 行（＋「 位置 N」）を組む領域。ノート名は 255 バイトまで。 */
constexpr size_t replace_status_capacity = 512;

/* 96 DPI での寸法（デザイン「案2 堅」）。 */
constexpr int base_dpi = 96;
constexpr int base_width = 960;
constexpr int base_height = 640;
/* 最小の大きさ。ドロワー・番号・パンくず・札 2 つが収まる（ADR 0011 の決定 3）。 */
constexpr int base_min_width = 560;
constexpr int base_min_height = 360;
constexpr int base_drawer_width = 240;
constexpr int base_caption_height = 44; /* 右ペインの頭。窓を掴んで動かせる帯 */
constexpr int base_action_height = 36;
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
constexpr int base_search_button_width = 64;
/* 操作行の常設ボタンを出せる最小の余白（これより狭ければ「操作 ▾」のメニューへ移る）。 */
constexpr int base_action_margin = 8;
/* 番号の右端と本文の左端のあいだ（96 DPI・ADR 0026 の決定 1）。 */
constexpr int base_gutter_gap = 8;
/* 数字を測れなかったときの目安（96 DPI の Consolas 11px の実寸に近い値）。 */
constexpr int base_digit_fallback = 7;
/* 主窓がスタックに持つ番号の行の本数。あふれたら描けるぶんだけ描く（決定 3）。 */
constexpr size_t gutter_row_capacity = 256;

/* DWM の窓の角と縁（Windows 11）。dwmapi.h の版によっては未定義なので数値で持つ。 */
constexpr DWORD attribute_corner_preference = 33;
constexpr DWORD attribute_border_color = 34;
constexpr DWORD corner_round_small = 3;

static void execute_command(struct folio_window *_Nonnull self, enum folio_command command,
                            const char *_Nonnull argument);
static enum folio_state_outcome store_body(const struct folio_window *_Nonnull self);
static enum folio_state_outcome command_save_as(const struct folio_window *_Nonnull self,
                                                const char *_Nonnull argument);
static void close_command_surface(struct folio_window *_Nonnull self);
static void command_not_found(struct folio_window *_Nonnull self);
static void show_command_palette(struct folio_window *_Nonnull self);
static void move_command_selection(struct folio_window *_Nonnull self, WPARAM key);
static void search_input_changed(struct folio_window *_Nonnull self);
static LRESULT CALLBACK command_input_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam);
static void filter_input_changed(struct folio_window *_Nonnull self);
static LRESULT CALLBACK filter_input_procedure(HWND window, UINT message, WPARAM wparam,
                                               LPARAM lparam);
static LRESULT CALLBACK command_layer_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam);

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

static int command_explanation_height(const struct folio_window *_Nonnull self)
{
    int rows = self->command_keys_visible
                   ? (int)(sizeof command_shortcuts / sizeof command_shortcuts[0])
                   : 0;
    return base_command_help_row_height * (rows + 2) + base_command_status_height;
}

static struct folio_window *_Nullable self_of(HWND window)
{
    return (struct folio_window *)GetWindowLongPtrW(window, GWLP_USERDATA);
}

/* 入力面が「自分の欄」と見なす HWND の集合（ADR 0016 の 2026-09-22 の補正）。
 * 集合の中でフォーカスが移るあいだは入力面を閉じない。 */
static bool command_owns(const struct folio_window *_Nonnull self, HWND _Nullable window)
{
    if (window == nullptr)
    {
        return false;
    }
    if (window == self->command_input)
    {
        return true;
    }
    for (size_t index = 0; index < replace_input_count; ++index)
    {
        if (window == self->replace_inputs[index])
        {
            return true;
        }
    }
    return false;
}

/* 開いている面で最初にフォーカスを受ける欄。置換の欄だけパターンの側になる。 */
static HWND _Nullable command_focus_target(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_REPLACE ? self->replace_inputs[0]
                                                            : self->command_input;
}

static void focus_command_input(struct folio_window *_Nonnull self)
{
    HWND _Nullable target = command_focus_target(self);
    if (target != nullptr)
    {
        SetFocus(target);
    }
}

/* 欄の上の 1 行を消す（失敗・未知の操作・`:%s` の 0 件）。 */
static void clear_command_status(struct folio_window *_Nonnull self)
{
    self->command_unknown = false;
    self->command_no_match = false;
    self->command_failure = FOLIO_STATE_READY;
}

/* 等幅書体の数字 1 文字の幅（字間 0・決定 1）。測れなければ DPI 換算の目安を使う。
 * 呼ぶのは書体を作り直す refresh_font だけで、毎回の WM_PAINT では測らない（補正 7）。 */
static int measure_digit(const struct folio_window *_Nonnull self, UINT dpi)
{
    HDC device = GetDC(self->handle);
    if (device == nullptr)
    {
        return scale(base_digit_fallback, dpi);
    }
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, 0);
    SIZE measured = {.cx = 0, .cy = 0};
    bool measurable = GetTextExtentPoint32W(device, L"0", 1, &measured) != 0;
    SelectObject(device, previous);
    ReleaseDC(self->handle, device);
    return measurable ? (int)measured.cx : scale(base_digit_fallback, dpi);
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
    /* 置換の 2 欄も同じ等幅で、DPI が変わったら破棄済みの HFONT を持たないように付け直す。 */
    for (size_t index = 0; index < replace_input_count; ++index)
    {
        if (self->replace_inputs[index] != nullptr)
        {
            SendMessageW(self->replace_inputs[index], WM_SETFONT, (WPARAM)self->mono_font, TRUE);
        }
    }
    /* 常設の絞り込みの欄も同じ等幅を使う。作り直した書体をここで付け直さないと、
     * DPI が変わった欄が破棄済みの HFONT を持ったままになる（ADR 0024 の補正 5）。 */
    if (self->filter_input != nullptr)
    {
        SendMessageW(self->filter_input, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    }
    /* 帯の幅の材料はここで 1 回だけ測る。DPI の変更でもこの関数が走るので測り直る（補正 7）。 */
    self->digit_width = measure_digit(self, dpi);
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
    /* 箱の高さは「出す操作の数」で決める。登録表の総数で取ると 1 行ぶん余る（補正 9）。 */
    int rows = (int)folio_command_listed_count();
    int height =
        scale(base_command_palette_padding * 2 + base_command_input_height + base_command_gap +
                  base_command_row_height * rows + base_command_status_height,
              dpi);
    height += scale(command_explanation_height(self), dpi);
    int top = scale(client.bottom < scale(480, dpi) ? base_command_palette_compact_top
                                                    : base_command_palette_top,
                    dpi);
    int available_height = client.bottom - top - scale(base_command_palette_edge, dpi);
    if (height > available_height)
    {
        height = available_height;
    }
    RECT bounds = {(client.right - width) / 2, top, (client.right + width) / 2, top + height};
    return bounds;
}

static RECT command_ex_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int status = self->command_failure != FOLIO_STATE_READY || self->command_unknown ||
                         self->command_no_match
                     ? scale(base_command_status_height, dpi)
                     : 0;
    RECT bounds = {0, client.bottom - scale(base_ex_height, dpi) - status, client.right,
                   client.bottom};
    return bounds;
}

/* 検索欄は Ex と同じ下端の帯で、件数の行を常に持ち、右端に「前へ」「次へ」を置く。 */
static RECT command_search_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds = {0, client.bottom - scale(base_ex_height + base_command_status_height, dpi),
                   client.right, client.bottom};
    return bounds;
}

/* 置換の欄も同じ下端の帯で、状態の 1 行と 2 つの欄の高さを持つ（ADR 0028 の決定 8(a)）。 */
static RECT command_replace_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds = {0, client.bottom - scale(base_ex_height * 2 + base_command_status_height, dpi),
                   client.right, client.bottom};
    return bounds;
}

static RECT command_surface_rect(const struct folio_window *_Nonnull self)
{
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_EX:
        return command_ex_rect(self);
    case COMMAND_SURFACE_SEARCH:
        return command_search_rect(self);
    case COMMAND_SURFACE_REPLACE:
        return command_replace_rect(self);
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_PALETTE:
        return command_palette_rect(self);
    }
    return command_palette_rect(self);
}

static RECT command_input_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        int padding = scale(base_command_palette_padding, dpi);
        int top = padding + scale(base_command_help_row_height * 2, dpi);
        RECT rows = {padding, top, bounds.right - padding,
                     top + scale(base_command_input_height, dpi)};
        return rows;
    }
    int inset = scale(base_command_gap, dpi);
    int vertical = scale(base_command_input_vertical_inset, dpi);
    int room = self->command_surface == COMMAND_SURFACE_SEARCH
                   ? scale((base_search_button_width + base_command_gap) * 2, dpi)
                   : 0;
    RECT input = {bounds.left + inset, bounds.bottom - scale(base_ex_height, dpi) + vertical,
                  bounds.right - inset - room, bounds.bottom - vertical};
    return input;
}

/* 置換の欄の行（index 0 = パターン・1 = 置換文字列）。ボタンぶんの空きを両方で同じにして、
 * 2 つの欄の右端を揃える（ADR 0028 の決定 8(a)）。 */
static RECT replace_input_rect(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    int row = scale(base_ex_height, dpi);
    int inset = scale(base_command_gap, dpi);
    int vertical = scale(base_command_input_vertical_inset, dpi);
    int room = scale((base_search_button_width + base_command_gap) * 2, dpi);
    int top = bounds.bottom - row * (index == 0 ? 2 : 1);
    RECT input = {bounds.left + inset, top + vertical, bounds.right - inset - room,
                  top + row - vertical};
    return input;
}

/* 下端の帯の右端に置く 2 つのボタン。検索欄の「前へ」「次へ」と置換の欄の「1 件」「すべて」が
 * 同じ位置を共有し、描画と当たり判定もここを共有する。 */
static RECT surface_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    int width = scale(base_search_button_width, dpi);
    int gap = scale(base_command_gap, dpi);
    int right = bounds.right - gap - (index == 0 ? width + gap : 0);
    RECT button = {right - width, bounds.bottom - scale(base_ex_height, dpi), right, bounds.bottom};
    return button;
}

static RECT command_toggle_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    UINT dpi = GetDpiForWindow(self->handle);
    int bottom = bounds.bottom - scale(base_command_status_height + base_command_gap, dpi);
    int keys = self->command_keys_visible
                   ? scale(base_command_help_row_height, dpi) *
                         (int)(sizeof command_shortcuts / sizeof command_shortcuts[0])
                   : 0;
    int inset = scale(base_command_palette_padding, dpi);
    return (RECT){inset, bottom - keys - scale(base_command_status_height, dpi),
                  bounds.right - inset, bottom - keys};
}

static RECT command_rows_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_input_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    bounds.top = bounds.bottom + scale(base_command_gap, dpi);
    bounds.bottom = command_toggle_rect(self).top;
    int row = scale(base_command_row_height, dpi);
    if (bounds.bottom < bounds.top)
    {
        bounds.bottom = bounds.top;
    }
    bounds.bottom = bounds.top + (bounds.bottom - bounds.top) / row * row;
    return bounds;
}

static size_t command_visible_rows(const struct folio_window *_Nonnull self)
{
    RECT rows = command_rows_rect(self);
    return (size_t)((rows.bottom - rows.top) /
                    scale(base_command_row_height, GetDpiForWindow(self->handle)));
}

static void reveal_command_selection(struct folio_window *_Nonnull self)
{
    size_t rows = command_visible_rows(self);
    if (self->command_selection < self->command_first)
    {
        self->command_first = self->command_selection;
    }
    if (rows > 0 && self->command_selection >= self->command_first + rows)
    {
        self->command_first = self->command_selection - rows + 1;
    }
}

/* 置換の 2 欄を置く。他の面では隠す（ADR 0016 の 2026-09-22 の補正）。 */
static void arrange_replace_inputs(struct folio_window *_Nonnull self)
{
    bool replacing = self->command_surface == COMMAND_SURFACE_REPLACE;
    for (size_t index = 0; index < replace_input_count; ++index)
    {
        HWND _Nullable input = self->replace_inputs[index];
        if (input == nullptr)
        {
            continue;
        }
        RECT bounds = replace_input_rect(self, index);
        MoveWindow(input, bounds.left, bounds.top, bounds.right - bounds.left,
                   bounds.bottom - bounds.top, TRUE);
        ShowWindow(input, replacing ? SW_SHOW : SW_HIDE);
    }
}

static void arrange_command_input(struct folio_window *_Nonnull self)
{
    if (self->command_layer == nullptr || self->command_input == nullptr)
    {
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        ShowWindow(self->command_layer, SW_HIDE);
        return;
    }
    RECT layer = command_surface_rect(self);
    MoveWindow(self->command_layer, layer.left, layer.top, layer.right - layer.left,
               layer.bottom - layer.top, TRUE);
    RECT bounds = command_input_rect(self);
    MoveWindow(self->command_input, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        self->command_first = 0;
        reveal_command_selection(self);
    }
    arrange_replace_inputs(self);
    ShowWindow(self->command_input,
               self->command_surface == COMMAND_SURFACE_REPLACE ? SW_HIDE : SW_SHOW);
    ShowWindow(self->command_layer, SW_SHOW);
    BringWindowToTop(self->command_layer);
}

/* 常設の欄はドロワーの頭の帯に重ねる。寸法はドロワーが答える（ADR 0024 の決定 6）。 */
static void arrange_filter_input(struct folio_window *_Nonnull self)
{
    if (self->filter_input == nullptr || self->drawer == nullptr)
    {
        return;
    }
    RECT bounds = drawer_window_filter_rect(self->drawer);
    MoveWindow(self->filter_input, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

/* 出すのは「number が真・編集モード・文書がある」のときだけ（ADR 0026 の決定 5）。
 * 無題の編集中も出す。UI は第 2 の真偽を持たず、application の値から導く（ARC-004）。 */
static bool gutter_visible(const struct folio_window *_Nonnull self)
{
    return self->pane != nullptr && folio_state_number(self->state) &&
           folio_state_pane_mode(self->state) == PANE_MODE_EDIT &&
           folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NONE;
}

/* ドロワーの右端から本文の左端までの空き。既存の 36px に収まるあいだ本文は動かない（決定 6）。 */
static int pane_inset(const struct folio_window *_Nonnull self, UINT dpi)
{
    int base = scale(base_pane_left, dpi);
    if (!gutter_visible(self))
    {
        return base;
    }
    int needed =
        self->digit_width * (int)note_pane_line_digits(self->pane) + scale(base_gutter_gap, dpi);
    return needed > base ? needed : base;
}

/* いま本文が置かれている矩形（主窓の client 座標）。帯はこの左隣に描く。 */
static RECT pane_bounds(const struct folio_window *_Nonnull self)
{
    RECT bounds = {0, 0, 0, 0};
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    if (pane == nullptr)
    {
        return bounds;
    }
    GetWindowRect(pane, &bounds);
    MapWindowPoints(nullptr, self->handle, (POINT *)&bounds, 2);
    return bounds;
}

/* 番号の帯。上下は本文と同じで、右端は本文の左端から 8px 手前で終える（決定 1）。
 * 桁数ではなく本文の実際の位置から決めるので、無効化の経路で平文を取り出さない（決定 4）。 */
static RECT gutter_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT body = pane_bounds(self);
    RECT band = {scale(base_drawer_width, dpi), body.top, body.left - scale(base_gutter_gap, dpi),
                 body.bottom};
    return band;
}

/* 番号の帯を描き直す契機を 1 つの関数へ集める（決定 4）。
 * すぐ描く（UpdateWindow）のはスクロールの契機だけで、本文の増減は印と無効化だけにする。 */
static void refresh_gutter(const struct folio_window *_Nonnull self, bool immediate)
{
    RECT band = gutter_rect(self);
    InvalidateRect(self->handle, &band, FALSE);
    if (immediate)
    {
        UpdateWindow(self->handle);
    }
}

static void arrange(struct folio_window *_Nonnull self)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int drawer_width = scale(base_drawer_width, dpi);
    int top = scale(base_caption_height + base_action_height + base_pane_top, dpi);
    int left = drawer_width + pane_inset(self, dpi);
    if (drawer != nullptr)
    {
        MoveWindow(drawer, 0, 0, drawer_width, client.bottom, TRUE);
        arrange_filter_input(self);
    }
    if (pane != nullptr)
    {
        MoveWindow(pane, left, top, client.right - left - scale(base_pane_right, dpi),
                   client.bottom - top - scale(base_pane_bottom, dpi), TRUE);
    }
    arrange_command_input(self);
    refresh_gutter(self, false);
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

/* パンくずのノート区画に実際に描く文字。幅の見積りも描画もこの 1 本を使う（ADR 0022 の決定 2）。 */
static const char *_Nonnull breadcrumb_note(struct pane_title_view title)
{
    return title.recovering ? recovering_label : title.note;
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
    int note = measure_utf8(device, breadcrumb_note(title));
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
    /* 復旧待ちの言い換えはここ 1 か所だけが持つ。application は実名を返す（ADR 0022 の決定 2）。 */
    SetTextColor(device,
                 title.recovering ? self->palette.selected_text : self->palette.current_text);
    draw_breadcrumb_label(device, breadcrumb_note(title), cells.note, cells.padding + cells.tip);
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
                             enum folio_language language, enum folio_command *_Nonnull out)
{
    size_t visible = 0;
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        /* 引数を渡せない面なので、語を要る Ex の文法は出さない（ADR 0026 の決定 8 の補正）。 */
        if (!folio_command_listed(command) ||
            !folio_command_matches(command, utf8_text_bytes(query), utf8_text_length(query),
                                   language))
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

static size_t command_matches_count(const struct utf8_text *_Nonnull query,
                                    enum folio_language language)
{
    size_t count = 0;
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        enum folio_command command = folio_command_at(index);
        bool shown = folio_command_listed(command) &&
                     folio_command_matches(command, utf8_text_bytes(query), utf8_text_length(query),
                                           language);
        count += shown ? 1 : 0;
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
    draw_utf8(device, folio_command_label(row.command, folio_state_language(self->state)), label);
    RECT aliases = row.bounds;
    aliases.right -= scale(base_command_row_padding, dpi);
    aliases.left = aliases.right;
    draw_aliases(self, device, row.command, aliases);
}

static size_t append_text(char *_Nonnull out, size_t at, const char *_Nonnull text)
{
    size_t length = strlen(text);
    memcpy(out + at, text, length);
    return at + length;
}

/* 10 進で書く。件数は本文の長さを超えないので 20 桁で足りる。 */
static size_t append_number(char *_Nonnull out, size_t at, size_t value)
{
    char digits[20];
    size_t count = 0;
    do
    {
        digits[count] = (char)('0' + value % 10);
        count += 1;
        value /= 10;
    } while (value > 0 && count < sizeof digits);
    for (size_t index = 0; index < count; ++index)
    {
        out[at + index] = digits[count - 1 - index];
    }
    return at + count;
}

/* いまの向きを欄に示す（`/` で開けば次へ、`?` で開けば前へ・ADR 0023 の決定 4）。 */
static const char *_Nonnull search_direction_label(const struct folio_window *_Nonnull self)
{
    switch (folio_state_search_direction(self->state))
    {
    case SEARCH_DIRECTION_FORWARD:
        return "このノート内を検索 ／ 次へ  ";
    case SEARCH_DIRECTION_BACKWARD:
        return "このノート内を検索 ／ 前へ  ";
    }
    return "このノート内を検索  ";
}

/* 向きと「k / n 件」。0 件も壊れた語も同じ場所に出し、選択は変えない。 */
static void search_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    size_t at = append_text(out, 0, search_direction_label(self));
    switch (self->search_outcome)
    {
    case NOTE_SEARCH_NO_TERM:
        break;
    case NOTE_SEARCH_MALFORMED:
        at = append_text(out, at, "検索できない文字があります");
        break;
    case NOTE_SEARCH_BAD_SPAN:
        at = append_text(out, at, "選択の範囲を読めません");
        break;
    case NOTE_SEARCH_NOT_FOUND:
        at = append_text(out, at, "見つかりません");
        break;
    case NOTE_SEARCH_FOUND:
        at = append_number(out, at, self->search_ordinal);
        at = append_text(out, at, " / ");
        at = append_number(out, at, self->search_total);
        at = append_text(out, at, " 件");
        break;
    }
    out[at] = '\0';
}

static void draw_search_surface(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int gap = scale(base_command_gap, dpi);
    RECT status = {bounds.left + gap, bounds.top, bounds.right - gap,
                   bounds.top + scale(base_command_status_height, dpi)};
    char line[search_status_capacity];
    search_status(self, line);
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, line, status);
    SetTextColor(device, self->palette.selected_text);
    draw_utf8(device, "◀ 前へ", surface_button_rect(self, 0));
    draw_utf8(device, "次へ ▶", surface_button_rect(self, 1));
}

/* 失敗の 1 行。REPLACE_BAD_PATTERN のときだけ末尾に位置を添える（ADR 0028 の決定 9）。 */
static void failure_status(const struct folio_window *_Nonnull self,
                           enum folio_state_outcome outcome, char *_Nonnull out)
{
    size_t at =
        append_text(out, 0, folio_state_failure_line(outcome, folio_state_language(self->state)));
    size_t offset = folio_state_replace_error_offset(self->state);
    if (outcome == FOLIO_STATE_REPLACE_BAD_PATTERN && offset > 0)
    {
        at = append_text(out, at, " 位置 ");
        at = append_number(out, at, offset);
    }
    out[at] = '\0';
}

/* 置換の対象名と件数。対象名はパンくずと同じ実名で、無題は「無題（未保存）」になる。 */
static void replace_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    struct pane_title_view title = folio_state_pane_title(self->state);
    size_t at = append_text(out, 0, title.any ? title.note : "");
    at = append_text(out, at, " / ");
    at = append_number(out, at, folio_state_replace_count(self->state));
    at = append_text(out, at, " 件");
    out[at] = '\0';
}

/* 欄の状態の 1 行。失敗の 1 行が「対象名 / k 件」より優先する（決定 8(a)）。 */
static void replace_line(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    if (self->replace_outcome == FOLIO_STATE_READY)
    {
        replace_status(self, out);
        return;
    }
    failure_status(self, self->replace_outcome, out);
}

static void draw_replace_surface(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int gap = scale(base_command_gap, dpi);
    RECT status = {bounds.left + gap, bounds.top, bounds.right - gap,
                   bounds.top + scale(base_command_status_height, dpi)};
    char line[replace_status_capacity];
    replace_line(self, line);
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, line, status);
    SetTextColor(device, self->palette.selected_text);
    draw_utf8(device, "1 件", surface_button_rect(self, 0));
    draw_utf8(device, "すべて", surface_button_rect(self, 1));
}

static void draw_command_status(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    char buffer[replace_status_capacity];
    const char *_Nonnull line = "";
    if (self->command_failure != FOLIO_STATE_READY)
    {
        failure_status(self, self->command_failure, buffer);
        line = buffer;
    }
    else if (self->command_unknown)
    {
        line = "一致する操作がありません。";
    }
    else if (self->command_no_match)
    {
        /* `:%s` が 1 件も一致しなかった。欄は閉じず、置換の欄と同じ形で出す（ADR 0028 の補正）。 */
        replace_status(self, buffer);
        line = buffer;
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
    if (!self->command_keys_visible)
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    int row_height = scale(base_command_help_row_height, dpi);
    int rows = (int)(sizeof command_shortcuts / sizeof command_shortcuts[0]);
    bounds.bottom -= scale(base_command_status_height + base_command_gap, dpi);
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

static RECT command_previous_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_toggle_rect(self);
    bounds.left = bounds.right - scale(88, GetDpiForWindow(self->handle));
    bounds.right -= scale(44, GetDpiForWindow(self->handle));
    return bounds;
}

static RECT command_next_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_toggle_rect(self);
    bounds.left = bounds.right - scale(44, GetDpiForWindow(self->handle));
    return bounds;
}

static void draw_command_guidance(const struct folio_window *_Nonnull self, HDC device)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT line = command_input_rect(self);
    int row = scale(base_command_help_row_height, dpi);
    line.bottom = line.top - row;
    line.top -= row * 2;
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, "操作をクリックすると実行します。", line);
    OffsetRect(&line, 0, row);
    draw_utf8(device, "下の欄に名前を入力すると絞り込めます。", line);
    RECT toggle = command_toggle_rect(self);
    toggle.right = command_previous_rect(self).left;
    SetTextColor(device, self->palette.selected_text);
    draw_utf8(device, self->command_keys_visible ? "キー操作を閉じる" : "キー操作を表示", toggle);
    draw_utf8(device, "↑ 前", command_previous_rect(self));
    draw_utf8(device, "↓ 次", command_next_rect(self));
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
    RECT rows = command_rows_rect(self);
    int top = rows.top;
    size_t count = command_visible_rows(self);
    for (size_t visible = 0; visible < count; ++visible)
    {
        enum folio_command command = FOLIO_COMMAND_SAVE;
        size_t index = self->command_first + visible;
        if (!command_at_query(query, index, folio_state_language(self->state), &command))
        {
            break;
        }
        RECT row = {rows.left, top, rows.right, top + scale(base_command_row_height, dpi)};
        draw_command_row(self, device,
                         (struct command_row){row, command, index == self->command_selection});
        top = row.bottom;
    }
    RECT status = {bounds.left + padding, bounds.bottom - scale(base_command_status_height, dpi),
                   bounds.right - padding, bounds.bottom};
    draw_command_status(self, device, status);
    draw_command_shortcuts(self, device, bounds);
    draw_command_guidance(self, device);
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
    case COMMAND_SURFACE_SEARCH:
        FillRect(device, &bounds, self->command_brush);
        draw_search_surface(self, device, bounds);
        return;
    case COMMAND_SURFACE_REPLACE:
        FillRect(device, &bounds, self->command_brush);
        draw_replace_surface(self, device, bounds);
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

static RECT action_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    const int offsets[] = {0, 110, 168, 226, 302};
    const int widths[] = {104, 52, 52, 70, 160};
    int left = scale(base_drawer_width + 8 + offsets[index], dpi);
    int top = scale(base_caption_height + 4, dpi);
    return (RECT){left, top, left + scale(widths[index], dpi), top + scale(28, dpi)};
}

/* 幅が足りない窓（560px など）では常設ボタンを出さず、「操作 ▾」のメニューへ委ねる
 * （登録表に載っている操作はメニューに必ず出る・ADR 0018 / ADR 0023 の決定 4）。 */
static bool action_button_fits(const struct folio_window *_Nonnull self, size_t index)
{
    RECT client;
    GetClientRect(self->handle, &client);
    return action_button_rect(self, index).right +
               scale(base_action_margin, GetDpiForWindow(self->handle)) <=
           client.right;
}

static void draw_action_button(const struct folio_window *_Nonnull self, HDC device, RECT bounds,
                               const char *_Nonnull label)
{
    HBRUSH brush = CreateSolidBrush(self->palette.window);
    FillRect(device, &bounds, brush);
    DeleteObject(brush);
    SetTextColor(device, self->palette.current_text);
    bounds.left += scale(8, GetDpiForWindow(self->handle));
    draw_utf8(device, label, bounds);
}

static void draw_actions(const struct folio_window *_Nonnull self, HDC device)
{
    draw_action_button(self, device, action_button_rect(self, 0),
                       folio_command_label(FOLIO_COMMAND_NEW, folio_state_language(self->state)));
    draw_action_button(self, device, action_button_rect(self, 1),
                       folio_command_label(FOLIO_COMMAND_SAVE, folio_state_language(self->state)));
    draw_action_button(self, device, action_button_rect(self, 2), "操作 ▾");
    draw_action_button(self, device, action_button_rect(self, 3),
                       folio_command_label(FOLIO_COMMAND_HELP, folio_state_language(self->state)));
    if (action_button_fits(self, 4))
    {
        draw_action_button(
            self, device, action_button_rect(self, 4),
            folio_command_label(FOLIO_COMMAND_FIND, folio_state_language(self->state)));
    }
}

/* 番号を UTF-16 の桁へ写す（描画の中で確保しない）。返すのは桁数で、収まらなければ 0。 */
static int number_units(size_t value, wchar_t *_Nonnull out, int capacity)
{
    wchar_t digits[24];
    int length = 0;
    do
    {
        digits[length] = (wchar_t)(L'0' + value % 10);
        value /= 10;
        length += 1;
    } while (value > 0 && length < 24);
    if (length > capacity)
    {
        return 0;
    }
    for (int at = 0; at < length; ++at)
    {
        out[at] = digits[length - 1 - at];
    }
    return length;
}

/* 本文の左の空きに、見えている論理行の番号を右寄せで描く（ADR 0026 の決定 1 / 3）。
 * y は note_pane が主窓の client 座標へ直して返す。
 * 帯に掛からない更新矩形では番号の一巡（EM_LINEINDEX / EM_POSFROMCHAR）ごと飛ばし、
 * 升目は帯でクリップして上の操作行へはみ出させない（ADR 0026 の補正 7）。 */
static void draw_gutter(const struct folio_window *_Nonnull self, HDC device, RECT damage)
{
    RECT band = gutter_rect(self);
    RECT touched = {0, 0, 0, 0};
    if (!gutter_visible(self) || !IntersectRect(&touched, &band, &damage))
    {
        return;
    }
    struct gutter_row rows[gutter_row_capacity];
    size_t count = 0;
    if (!note_pane_visible_rows(self->pane, rows, gutter_row_capacity, &count))
    {
        return;
    }
    int saved = SaveDC(device);
    if (saved == 0)
    {
        return;
    }
    IntersectClipRect(device, band.left, band.top, band.right, band.bottom);
    SetTextColor(device, self->palette.gutter_text);
    SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, 0);
    for (size_t at = 0; at < count; ++at)
    {
        wchar_t label[24];
        int length = number_units(rows[at].number, label, 24);
        RECT cell = {band.left, rows[at].top, band.right, rows[at].top + rows[at].height};
        DrawTextW(device, label, length, &cell, DT_SINGLELINE | DT_RIGHT | DT_TOP | DT_NOPREFIX);
    }
    RestoreDC(device, saved);
}

/* 右ペインの地と頭。本文は note_pane が持つ。 */
static void draw_pane(const struct folio_window *_Nonnull self, HDC device, RECT client,
                      RECT damage)
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
    draw_actions(self, device);
    SetTextCharacterExtra(device, 0);
    draw_gutter(self, device, damage);
}

/* 更新矩形と右ペインの重なりへ、client 座標のまま 1 枚だけ描き写す。
 * memory の原点をずらしてあるので draw_pane は今までどおり client 座標で描ける。 */
static void blit_pane(struct folio_window *_Nonnull self, HDC target, RECT client, RECT damage)
{
    HDC memory = CreateCompatibleDC(target);
    int width = damage.right - damage.left;
    int height = damage.bottom - damage.top;
    HBITMAP surface = memory == nullptr ? nullptr : CreateCompatibleBitmap(target, width, height);
    if (surface != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, surface);
        SetViewportOrgEx(memory, -damage.left, -damage.top, nullptr);
        draw_pane(self, memory, client, damage);
        BitBlt(target, damage.left, damage.top, width, height, memory, damage.left, damage.top,
               SRCCOPY);
        SelectObject(memory, previous);
        DeleteObject(surface);
    }
    if (memory != nullptr)
    {
        DeleteDC(memory);
    }
}

/* 帯の有無と桁数で本文の左端が変わっていたら、配り直す（ADR 0026 の決定 6）。
 * 描画の途中で窓を動かさないので、意図は 1 通のメッセージにして次の回で受ける。
 * 契機を 1 か所に保つため、描く矩形が空でも毎回の WM_PAINT で確かめる。 */
static void check_pane_inset(const struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    if (pane_bounds(self).left != scale(base_drawer_width, dpi) + pane_inset(self, dpi))
    {
        PostMessageW(self->handle, folio_message_gutter_resized, 0, 0);
    }
}

/* 毎回 client 全体を作り直さず、OS が壊れたと言う矩形だけを描く（ADR 0026 の決定 1）。 */
static void paint_pane(struct folio_window *_Nonnull self)
{
    PAINTSTRUCT painting;
    HDC target = BeginPaint(self->handle, &painting);
    RECT client;
    GetClientRect(self->handle, &client);
    RECT pane = {scale(base_drawer_width, GetDpiForWindow(self->handle)), 0, client.right,
                 client.bottom};
    RECT damage = {0, 0, 0, 0};
    if (IntersectRect(&damage, &pane, &painting.rcPaint))
    {
        blit_pane(self, target, client, damage);
    }
    EndPaint(self->handle, &painting);
    check_pane_inset(self);
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

/* 初回だけ本文を用意して編集へ入る。同じmodeへの操作は未保存本文とUndoを保つ。 */
static enum folio_state_outcome enter_edit(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
    {
        return FOLIO_STATE_READY;
    }
    struct note_ref selected = {.category = 0, .note = 0};
    if (!folio_state_selection(self->state, &selected))
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    struct utf16_text *_Nullable wide = nullptr;
    if (self->pane == nullptr ||
        utf16_text_create(folio_state_pane_text(self->state),
                          folio_state_pane_text_length(self->state), &wide) != UTF16_TEXT_CONVERTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome outcome = folio_state_begin_edit(self->state);
    if (outcome == FOLIO_STATE_READY)
    {
        note_pane_edit(self->pane, utf16_text_units(wide), utf16_text_length(wide));
        InvalidateRect(self->handle, nullptr, FALSE);
    }
    utf16_text_destroy(wide);
    return outcome;
}

/* 編集中なら保存して閲覧へ戻す。閲覧中なら何もしない（「閲覧」の札だけが使う）。 */
static enum folio_state_outcome flush_edit(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        return store_body(self);
    }
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_UNTITLED)
    {
        enum folio_state_outcome saved = store_body(self);
        if (saved != FOLIO_STATE_READY)
        {
            return saved;
        }
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
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        return folio_state_store_note(self->state, u"", 0);
    }
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_UNTITLED)
    {
        return command_save_as(self, "");
    }
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = take_text(self, &units, &count);
    return outcome == FOLIO_STATE_READY ? folio_state_store_note(self->state, units, count)
                                        : outcome;
}

static enum folio_state_outcome command_save(struct folio_window *_Nonnull self)
{
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    return store_body(self);
}

static enum folio_state_outcome command_quit(struct folio_window *_Nonnull self)
{
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = folio_state_pane_mode(self->state) == PANE_MODE_VIEW
                                           ? FOLIO_STATE_READY
                                           : take_text(self, &units, &count);
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

/* 欄の中の 1 行で済ませる失敗と、モーダルの箱で知らせる事象を分ける
 * （ADR 0028 の決定 8(b) と 2026-09-22 の補正 8）。打ち間違いに箱を出さず、
 * 記憶域と「本文を取り出せない」は従来どおり箱にする。
 * 値を足すと -Wswitch-enum がここを落とすので、分け方を必ず決めさせる（C-002）。 */
static bool inline_outcome(enum folio_state_outcome outcome)
{
    switch (outcome)
    {
    case FOLIO_STATE_NOTHING_SELECTED:
    case FOLIO_STATE_NOT_EDITING:
    case FOLIO_STATE_UNSAVED_CHANGES:
    case FOLIO_STATE_REPLACE_NO_PATTERN:
    case FOLIO_STATE_REPLACE_BAD_PATTERN:
    case FOLIO_STATE_REPLACE_BAD_TEMPLATE:
    case FOLIO_STATE_REPLACE_TIMED_OUT:
    case FOLIO_STATE_REPLACE_TOO_COMPLEX:
    case FOLIO_STATE_REPLACE_TOO_MANY:
    case FOLIO_STATE_REPLACE_TOO_LARGE:
    case FOLIO_STATE_REPLACE_STALE:
    case FOLIO_STATE_REPLACE_BAD_SPAN:
        return true;
    case FOLIO_STATE_READY:
    case FOLIO_STATE_DATA_UNREADABLE:
    case FOLIO_STATE_LEDGER_MALFORMED:
    case FOLIO_STATE_STORE_FAILED:
    case FOLIO_STATE_NO_SUCH_CATEGORY:
    case FOLIO_STATE_NO_SUCH_NOTE:
    case FOLIO_STATE_NOTE_UNREADABLE:
    case FOLIO_STATE_NOTE_MALFORMED:
    case FOLIO_STATE_NOTE_STORE_FAILED:
    case FOLIO_STATE_HISTORY_FAILED:
    case FOLIO_STATE_NAME_TAKEN:
    case FOLIO_STATE_LEDGER_STALE:
    case FOLIO_STATE_LEDGER_UNSYNCED:
    case FOLIO_STATE_RENAME_PENDING:
    case FOLIO_STATE_RENAME_UNLOCKED:
    case FOLIO_STATE_RENAME_UNSUPPORTED:
    case FOLIO_STATE_RENAME_IDENTITY_FAILED:
    case FOLIO_STATE_RENAME_JOURNAL_FAILED:
    case FOLIO_STATE_RENAME_JOURNAL_BROKEN:
    case FOLIO_STATE_RENAME_HALTED:
    case FOLIO_STATE_SEARCH_MALFORMED:
    case FOLIO_STATE_FILTERED:
    case FOLIO_STATE_SETTINGS_UNREADABLE:
    case FOLIO_STATE_SETTINGS_STORE_FAILED:
    case FOLIO_STATE_PANE_UNAVAILABLE:
    case FOLIO_STATE_OUT_OF_MEMORY:
    case FOLIO_STATE_NAME_REQUIRED:
    case FOLIO_STATE_INVALID_NAME:
    case FOLIO_STATE_ALREADY_NAMED:
    case FOLIO_STATE_CANCELLED:
        return false;
    }
    return false;
}

static void command_failure(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    if (outcome == FOLIO_STATE_CANCELLED)
    {
        return;
    }
    bool inline_failure =
        self->command_surface != COMMAND_SURFACE_CLOSED && inline_outcome(outcome);
    if (!inline_failure)
    {
        failure_box_show(self->handle, outcome);
        if (self->command_surface != COMMAND_SURFACE_CLOSED)
        {
            focus_command_input(self);
        }
        return;
    }
    clear_command_status(self);
    self->command_failure = outcome;
    arrange_command_input(self);
    redraw_command_layer(self);
    focus_command_input(self);
}

static enum search_direction reversed_direction(enum search_direction direction)
{
    switch (direction)
    {
    case SEARCH_DIRECTION_FORWARD:
        return SEARCH_DIRECTION_BACKWARD;
    case SEARCH_DIRECTION_BACKWARD:
        return SEARCH_DIRECTION_FORWARD;
    }
    return direction;
}

/* 探し始める位置。選択の正本は RichEdit で、application も core も持たない（ADR 0023 の決定 1）。
 * advance なら選択の外側（前方は終わり・後方は始まり）から、入力中（インクリメンタル）は
 * いまの一致の頭（後方なら末尾）から数え直して、打つたびに同じ場所で伸びるようにする。 */
static struct note_search_span search_anchor(const struct folio_window *_Nonnull self,
                                             enum search_direction direction, bool advance)
{
    size_t start = 0;
    size_t end = 0;
    if (self->pane == nullptr || !note_pane_selection(self->pane, &start, &end))
    {
        start = 0;
        end = 0;
    }
    if (advance)
    {
        return (struct note_search_span){.start = start, .end = end};
    }
    size_t at = direction == SEARCH_DIRECTION_FORWARD ? start : end;
    return (struct note_search_span){.start = at, .end = at};
}

/* core が決めた範囲を RichEdit の選択 1 つにする。着色もしないし、本文も Undo も触らない。
 * 件数は選択を動かす前に数える。query が借りている本文は note_pane が持つ領域なので、
 * SendMessage を挟んだ後まで指したままにしない。 */
static void apply_search(struct folio_window *_Nonnull self,
                         const struct note_search_query *_Nonnull query,
                         enum search_direction direction, bool advance)
{
    struct note_search_span found = {.start = 0, .end = 0};
    self->search_outcome =
        note_search_next(query, search_anchor(self, direction, advance), direction, &found);
    if (self->search_outcome != NOTE_SEARCH_FOUND || self->pane == nullptr)
    {
        return;
    }
    size_t total = 0;
    size_t ordinal = 0;
    if (note_search_count(query, found.start, &total, &ordinal) == NOTE_SEARCH_FOUND)
    {
        self->search_total = total;
        self->search_ordinal = ordinal;
    }
    note_pane_select(self->pane, found.start, found.end);
}

/* 表示中の本文を取り出せない理由を、見せる 1 行へ写す（C-002 で網羅）。 */
static enum folio_state_outcome from_pane_text(enum note_pane_text_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_PANE_TEXT_TAKEN:
        return FOLIO_STATE_READY;
    case NOTE_PANE_TEXT_UNAVAILABLE:
        return FOLIO_STATE_PANE_UNAVAILABLE;
    case NOTE_PANE_TEXT_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_PANE_UNAVAILABLE;
}

static enum folio_state_outcome from_utf16(enum utf16_text_outcome outcome)
{
    switch (outcome)
    {
    case UTF16_TEXT_CONVERTED:
        return FOLIO_STATE_READY;
    case UTF16_TEXT_INVALID_UTF8:
        return FOLIO_STATE_SEARCH_MALFORMED;
    case UTF16_TEXT_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_OUT_OF_MEMORY;
}

static enum folio_state_outcome take_display_text(const struct folio_window *_Nonnull self,
                                                  const char16_t *_Nonnull *_Nonnull units,
                                                  size_t *_Nonnull count)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    return from_pane_text(note_pane_display_text(self->pane, units, count));
}

/* 表示中の平文と覚えている語を core へ渡す。本文は UI が貸すだけで、判断は core が持つ。 */
static void update_search(struct folio_window *_Nonnull self, enum search_direction direction,
                          bool advance)
{
    self->search_outcome = NOTE_SEARCH_NO_TERM;
    self->search_total = 0;
    self->search_ordinal = 0;
    const char16_t *_Nonnull text = u"";
    size_t length = 0;
    struct utf16_text *_Nullable term = nullptr;
    enum folio_state_outcome taken = take_display_text(self, &text, &length);
    if (taken == FOLIO_STATE_READY)
    {
        taken = from_utf16(utf16_text_create(folio_state_search_term(self->state),
                                             folio_state_search_term_length(self->state), &term));
    }
    if (taken != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, taken);
        return;
    }
    struct note_search_query query = {.text = text,
                                      .length = length,
                                      .term = utf16_text_units(term),
                                      .term_length = utf16_text_length(term)};
    apply_search(self, &query, direction, advance);
    utf16_text_destroy(term);
}

/* 欄が開いていれば件数を描き直し、閉じているときの不成立は音だけで知らせる。 */
static void report_search(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        redraw_command_layer(self);
        return;
    }
    if (self->search_outcome != NOTE_SEARCH_FOUND)
    {
        MessageBeep(MB_ICONWARNING);
    }
}

/* 欄の今の文字を取り出す。欄がまだ無ければ 0 文字（GetWindowTextW は必ず終端を書く）。 */
static size_t input_text(HWND _Nullable input, wchar_t *_Nonnull units)
{
    units[0] = L'\0';
    int count = input == nullptr ? 0 : GetWindowTextW(input, units, (int)command_input_capacity);
    return count > 0 ? (size_t)count : 0;
}

/* 2 つの欄と RichEdit が表示している本文で下見を取り直す（ADR 0028 の決定 6）。
 * 打つたびに呼ぶので、失敗もモーダルではなく欄の中の 1 行にする（閲覧中は NOT_EDITING）。 */
static void update_replace_preview(struct folio_window *_Nonnull self)
{
    wchar_t pattern[command_input_capacity];
    wchar_t replacement[command_input_capacity];
    size_t patterns = input_text(self->replace_inputs[0], pattern);
    size_t replacements = input_text(self->replace_inputs[1], replacement);
    const char16_t *_Nonnull text = u"";
    size_t length = 0;
    self->replace_outcome = take_display_text(self, &text, &length);
    if (self->replace_outcome == FOLIO_STATE_READY)
    {
        struct replace_request request = {.text = text,
                                          .length = length,
                                          .pattern = (const char16_t *)pattern,
                                          .pattern_length = patterns,
                                          .replacement = (const char16_t *)replacement,
                                          .replacement_length = replacements};
        self->replace_outcome = folio_state_preview_replace(self->state, &request);
    }
    redraw_command_layer(self);
}

/* 当てる起点は RichEdit の現在の選択（検索と同じ経路・決定 6）。取れなければ本文の先頭。 */
static struct note_search_span replace_anchor(const struct folio_window *_Nonnull self)
{
    size_t start = 0;
    size_t end = 0;
    if (self->pane == nullptr || !note_pane_selection(self->pane, &start, &end))
    {
        return (struct note_search_span){.start = 0, .end = 0};
    }
    return (struct note_search_span){.start = start, .end = end};
}

/* 1 件は挿入した文字列の直後へ、全部は先頭へ戻して読んでいた論理行を保つ（決定 7）。
 * line は当てる前に測った「最初に見えていた論理行」で、0 なら戻さない。 */
static void finish_replace(struct folio_window *_Nonnull self, struct replace_edit *_Nonnull edit,
                           enum replace_scope scope, size_t line)
{
    struct note_search_span span = replace_edit_span(edit);
    size_t at = span.start + replace_edit_length(edit);
    note_pane_replace(self->pane, span, replace_edit_units(edit));
    if (scope == REPLACE_ONE)
    {
        note_pane_select(self->pane, at, at);
        return;
    }
    note_pane_select(self->pane, 0, 0);
    if (line > 0)
    {
        note_pane_scroll_to_line(self->pane, line);
    }
}

/* 下見を本文へ当てて、件数を取り直す（決定 7）。欄は開いたまま。
 * 直前の入力が失敗しているなら当てない。application も失敗した下見を捨てているので
 * 当たりはしないが、出ている失敗の 1 行を STALE で上書きしないためにここでも先に返す
 * （安全の正本は application・レビュー B1）。 */
static void apply_replace(struct folio_window *_Nonnull self, enum replace_scope scope)
{
    if (self->replace_outcome != FOLIO_STATE_READY)
    {
        redraw_command_layer(self);
        return;
    }
    const char16_t *_Nonnull text = u"";
    size_t length = 0;
    enum folio_state_outcome applied = take_display_text(self, &text, &length);
    struct replace_edit *_Nullable edit = nullptr;
    if (applied == FOLIO_STATE_READY)
    {
        struct replace_apply apply = {
            .text = text, .length = length, .anchor = replace_anchor(self), .scope = scope};
        applied = folio_state_apply_replace(self->state, &apply, &edit);
    }
    if (applied != FOLIO_STATE_READY)
    {
        self->replace_outcome = applied;
        redraw_command_layer(self);
        return;
    }
    if (edit != nullptr)
    {
        /* 本文を取り出せた以上 pane はある（take_display_text が nullptr を弾く）。 */
        size_t line = note_pane_first_visible_line(self->pane);
        finish_replace(self, edit, scope, line);
        replace_edit_destroy(edit);
    }
    update_replace_preview(self);
}

/* 向きを名指しして 1 つ進む（「前へ」「次へ」と F3 / Shift+F3）。語が空なら何もしない。
 * 覚えている向きは変えない。向きを変えるのは `/` と `?` だけ（2026-09-17 の補正）。 */
static void step_search(struct folio_window *_Nonnull self, enum search_direction direction)
{
    if (folio_state_search_term_length(self->state) == 0)
    {
        return;
    }
    update_search(self, direction, true);
    report_search(self);
}

/* n / Enter は覚えている向き、N / Shift+Enter はその逆向きへ 1 回だけ進む（ADR 0023 の決定 5）。 */
static void repeat_search(struct folio_window *_Nonnull self, bool reverse)
{
    enum search_direction direction = folio_state_search_direction(self->state);
    step_search(self, reverse ? reversed_direction(direction) : direction);
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
        focus_command_input(self);
        return;
    }
    self->command_return_focus = GetFocus();
    self->command_surface = surface;
    self->command_selection = 0;
    self->command_first = 0;
    self->command_keys_visible = false;
    clear_command_status(self);
    SetWindowTextW(self->command_input, surface == COMMAND_SURFACE_EX ? L":" : L"");
    arrange_command_input(self);
    focus_command_input(self);
    SendMessageW(self->command_input, EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    redraw_command_layer(self);
}

static void hide_command_surface(struct folio_window *_Nonnull self)
{
    self->command_surface = COMMAND_SURFACE_CLOSED;
    self->command_return_focus = nullptr;
    self->command_selection = 0;
    clear_command_status(self);
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
    self->command_first = 0;
    self->command_keys_visible = false;
    clear_command_status(self);
    if (self->command_input != nullptr)
    {
        SetWindowTextW(self->command_input, L"");
    }
    arrange_command_input(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

/* 検索欄は ADR 0016 の入力面そのもの。元のフォーカスは open_command_surface が覚え、
 * Esc の close_command_surface が返す。覚えている語を初期値にして全選択する
 * （語は欄を空にする EN_CHANGE より先に写しておく）。 */
static void show_search_surface(struct folio_window *_Nonnull self)
{
    struct utf16_text *_Nullable term = nullptr;
    if (utf16_text_create(folio_state_search_term(self->state),
                          folio_state_search_term_length(self->state),
                          &term) != UTF16_TEXT_CONVERTED)
    {
        command_failure(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_SEARCH);
    }
    else
    {
        self->command_surface = COMMAND_SURFACE_SEARCH;
        clear_command_status(self);
        arrange_command_input(self);
    }
    if (self->command_input != nullptr)
    {
        SetWindowTextW(self->command_input, utf16_text_units(term));
        SendMessageW(self->command_input, EM_SETSEL, 0, (LPARAM)-1);
        SetFocus(self->command_input);
    }
    utf16_text_destroy(term);
    redraw_command_layer(self);
}

/* 表示中のノートが無ければ欄を開かず、選択を案内する（採用済み計画 #47 の第 2 節）。 */
static void begin_search(struct folio_window *_Nonnull self)
{
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        command_failure(self, FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    show_search_surface(self);
}

/* 置換の欄も ADR 0016 の入力面そのもの。開き方・戻り先 HWND・Esc の規則は検索欄と同じで、
 * 違うのは EDIT が 2 つあることだけ（ADR 0016 の 2026-09-22 の補正）。
 * 語は欄が覚えたまま残し、開いた瞬間に下見を取り直して件数を出す。 */
static void show_replace_surface(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_REPLACE);
    }
    else
    {
        self->command_surface = COMMAND_SURFACE_REPLACE;
        clear_command_status(self);
        arrange_command_input(self);
        focus_command_input(self);
    }
    update_replace_preview(self);
}

/* 表示中のノートが無ければ欄を開かない（検索と同じ）。閲覧中は開いて NOT_EDITING の 1 行を
 * 出し、本文もモードも変えない（ADR 0028 の決定 8(c)）。 */
static void begin_replace(struct folio_window *_Nonnull self)
{
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        command_failure(self, FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    show_replace_surface(self);
}

/* `/` と `?` は向きを決めてから同じ欄を開く（ADR 0023 の決定 4）。 */
static void open_search(struct folio_window *_Nonnull self, enum search_direction direction)
{
    folio_state_set_search_direction(self->state, direction);
    begin_search(self);
}

/* 初回/別名の名前を同じ入力面またはExから受け、同じ作成へ渡す。 */
static enum folio_state_outcome save_destination(const struct folio_window *_Nonnull self,
                                                 const char *_Nonnull argument,
                                                 const char16_t *_Nonnull units, size_t count)
{
    if (argument[0] == '\0')
    {
        struct name_prompt_request request = {.state = self->state,
                                              .kind = folio_state_document_kind(self->state) ==
                                                              FOLIO_DOCUMENT_NAMED
                                                          ? NAME_PROMPT_SAVE_AS
                                                          : NAME_PROMPT_FIRST_SAVE,
                                              .units = units,
                                              .count = count};
        return name_prompt_show(self->handle, &request);
    }
    struct note_name *_Nullable name = nullptr;
    enum note_name_outcome accepted = note_name_create(argument, strlen(argument), &name);
    if (accepted != NOTE_NAME_ACCEPTED)
    {
        return accepted == NOTE_NAME_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_INVALID_NAME;
    }
    struct note_destination destination = {.category = folio_state_document_category(self->state),
                                           .name = name};
    enum folio_state_outcome outcome =
        folio_state_store_new(self->state, &destination, units, count);
    note_name_destroy(name);
    return outcome;
}

static enum folio_state_outcome command_save_as(const struct folio_window *_Nonnull self,
                                                const char *_Nonnull argument)
{
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = folio_state_pane_mode(self->state) == PANE_MODE_VIEW
                                           ? FOLIO_STATE_READY
                                           : take_text(self, &units, &count);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = save_destination(self, argument, units, count);
    }
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    return outcome;
}

/* 改名の名前も同じ入力面または Ex から受け、同じ意図へ渡す（ADR 0022 の決定 1）。 */
static enum folio_state_outcome rename_destination(const struct folio_window *_Nonnull self,
                                                   const char *_Nonnull argument,
                                                   const char16_t *_Nonnull units, size_t count)
{
    if (argument[0] == '\0')
    {
        struct name_prompt_request request = {
            .state = self->state, .kind = NAME_PROMPT_RENAME, .units = units, .count = count};
        return name_prompt_show(self->handle, &request);
    }
    struct note_name *_Nullable name = nullptr;
    enum note_name_outcome accepted = note_name_create(argument, strlen(argument), &name);
    if (accepted != NOTE_NAME_ACCEPTED)
    {
        return accepted == NOTE_NAME_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_INVALID_NAME;
    }
    enum folio_state_outcome outcome = folio_state_rename_note(self->state, name, units, count);
    note_name_destroy(name);
    return outcome;
}

/* 本文・モード・Undo は触らない。未保存の本文は application が同じ保存経路で先に確定する。 */
static enum folio_state_outcome command_rename(const struct folio_window *_Nonnull self,
                                               const char *_Nonnull argument)
{
    const char16_t *units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = folio_state_pane_mode(self->state) == PANE_MODE_VIEW
                                           ? FOLIO_STATE_READY
                                           : take_text(self, &units, &count);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = rename_destination(self, argument, units, count);
    }
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    return outcome;
}

/* 保存の結果を見せるだけ。欄にも区画にも触らない。成功したら true。 */
static bool report_save(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return false;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    return true;
}

static void finish_save_command(struct folio_window *_Nonnull self,
                                enum folio_state_outcome outcome)
{
    if (report_save(self, outcome) && self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        close_command_surface(self);
    }
}

/* 入力面（Ex・パレット・検索欄・置換の 2 欄・ドロワーの絞り込み欄）の中の Ctrl+S
 * （ADR 0016 の 2026-09-22 の補正 2）。保存は本文の Ctrl+S と同じ command_save を通り、
 * 違うのは**欄を閉じないこと**だけである。欄の語・キャレット・選択は触らない。
 * 失敗の箱や名前入力の面はフォーカスを動かすので、押した欄へ返す。 */
static void store_from_surface(struct folio_window *_Nonnull self)
{
    HWND _Nullable focus = GetFocus();
    (void)report_save(self, command_save(self));
    if (focus != nullptr && IsWindow(focus))
    {
        SetFocus(focus);
    }
}

static void execute_save_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    enum folio_state_outcome outcome = FOLIO_STATE_ALREADY_NAMED;
    if (argument[0] == '\0')
    {
        outcome = command_save(self);
    }
    else if (folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NAMED)
    {
        outcome = command_save_as(self, argument);
    }
    finish_save_command(self, outcome);
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
    enum folio_state_outcome outcome = store_body(self);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    DestroyWindow(self->handle);
}

static void execute_mode_command(struct folio_window *_Nonnull self, enum pane_mode mode)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    switch (mode)
    {
    case PANE_MODE_EDIT:
        outcome = enter_edit(self);
        break;
    case PANE_MODE_VIEW:
        outcome = flush_edit(self);
        break;
    }
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    hide_command_surface(self);
    switch (mode)
    {
    case PANE_MODE_EDIT:
        focus_pane(self);
        return;
    case PANE_MODE_VIEW:
        SetFocus(self->handle);
        return;
    }
}

/* Ex・パレット・既存入口が共有する唯一の HWND 操作 dispatcher（ADR 0016 の決定 2）。 */
static void execute_new_command(struct folio_window *_Nonnull self)
{
    size_t category = folio_state_current_category(self->state);
    enum folio_state_outcome saved = store_body(self);
    if (saved == FOLIO_STATE_READY)
    {
        saved = folio_state_new_note(self->state, category);
    }
    if (saved != FOLIO_STATE_READY)
    {
        command_failure(self, saved);
        return;
    }
    hide_command_surface(self);
    note_pane_edit(self->pane, u"", 0);
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    focus_pane(self);
}

/* 語ごとの行き先（ADR 0026 の決定 8）。切替はいまの値から導き、UI に第 2 の真偽を持たない。 */
static bool wanted_number(const struct folio_window *_Nonnull self, enum folio_option option)
{
    switch (option)
    {
    case FOLIO_OPTION_NUMBER_SHOW:
        return true;
    case FOLIO_OPTION_NUMBER_HIDE:
        return false;
    case FOLIO_OPTION_NUMBER_TOGGLE:
        return !folio_state_number(self->state);
    }
    return folio_state_number(self->state);
}

/* 本文の矩形が変わる操作の前後で、最初に見える論理行を保つ（ADR 0026 の決定 6）。
 * 本文・Undo・選択には触らない。折り返しの途中から見えていた場合は行の先頭へ寄る。
 * 戻すのは arrange() が実際に本文の左端を動かしたときだけで、3〜4 桁の切替や
 * 帯の出ない閲覧では読んでいる位置に触れない（ADR 0026 の補正 6）。 */
static void rearrange_keeping_line(struct folio_window *_Nonnull self)
{
    LONG before = pane_bounds(self).left;
    size_t line = self->pane == nullptr ? 0 : note_pane_first_visible_line(self->pane);
    arrange(self);
    if (line > 0 && self->pane != nullptr && pane_bounds(self).left != before)
    {
        note_pane_scroll_to_line(self->pane, line);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
}

/* 3 つの入口（`:set` の語・パレット／メニューの切替・#38 の設定画面）が通る唯一の意図。 */
static void apply_number(struct folio_window *_Nonnull self, bool number)
{
    enum folio_state_outcome outcome = folio_state_set_number(self->state, number);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    hide_command_surface(self);
    rearrange_keeping_line(self);
}

/* 未知の語・語なし・余計な語は、未知のコマンドと同じ 1 行で、入力を消さない（決定 8）。 */
static void execute_set_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    enum folio_option option = FOLIO_OPTION_NUMBER_SHOW;
    if (!folio_command_parse_option(argument, strlen(argument), &option))
    {
        command_not_found(self);
        return;
    }
    apply_number(self, wanted_number(self, option));
}

/* `:%s` の部分列は UTF-8 のバイト位置なので、UTF-16 へ戻してから同じ下見へ渡す（決定 8(b)）。 */
static enum folio_state_outcome substitute_preview(struct folio_window *_Nonnull self,
                                                   const char *_Nonnull argument,
                                                   const struct ex_substitute *_Nonnull parts)
{
    const char16_t *_Nonnull text = u"";
    size_t length = 0;
    enum folio_state_outcome taken = take_display_text(self, &text, &length);
    if (taken != FOLIO_STATE_READY)
    {
        return taken;
    }
    struct utf16_text *_Nullable pattern = nullptr;
    struct utf16_text *_Nullable replacement = nullptr;
    taken =
        from_utf16(utf16_text_create(argument + parts->pattern, parts->pattern_length, &pattern));
    if (taken == FOLIO_STATE_READY)
    {
        taken = from_utf16(utf16_text_create(argument + parts->replacement,
                                             parts->replacement_length, &replacement));
    }
    if (taken == FOLIO_STATE_READY)
    {
        struct replace_request request = {.text = text,
                                          .length = length,
                                          .pattern = utf16_text_units(pattern),
                                          .pattern_length = utf16_text_length(pattern),
                                          .replacement = utf16_text_units(replacement),
                                          .replacement_length = utf16_text_length(replacement)};
        taken = folio_state_preview_replace(self->state, &request);
    }
    utf16_text_destroy(replacement);
    utf16_text_destroy(pattern);
    return taken;
}

/* 1 件も一致しなかった `:%s` は欄を閉じず、置換の欄と同じ形の 1 行を出す（ADR 0028 の補正）。 */
static void report_no_match(struct folio_window *_Nonnull self)
{
    clear_command_status(self);
    self->command_no_match = true;
    arrange_command_input(self);
    redraw_command_layer(self);
}

/* 欄を開かず直接当てる。`g` があれば全部、無ければ各論理行の最初の一致だけ（決定 8(b)）。
 * 成功したら他の Ex と同じく黙って閉じる。 */
static void apply_substitute(struct folio_window *_Nonnull self, bool global)
{
    enum replace_scope scope = global ? REPLACE_ALL : REPLACE_LINE_FIRST;
    const char16_t *_Nonnull text = u"";
    size_t length = 0;
    enum folio_state_outcome applied = take_display_text(self, &text, &length);
    struct replace_edit *_Nullable edit = nullptr;
    if (applied == FOLIO_STATE_READY)
    {
        struct replace_apply apply = {
            .text = text, .length = length, .anchor = {.start = 0, .end = 0}, .scope = scope};
        applied = folio_state_apply_replace(self->state, &apply, &edit);
    }
    if (applied != FOLIO_STATE_READY)
    {
        command_failure(self, applied);
        return;
    }
    if (edit == nullptr)
    {
        report_no_match(self);
        return;
    }
    /* 本文を取り出せた以上 pane はある（take_display_text が nullptr を弾く）。 */
    size_t line = note_pane_first_visible_line(self->pane);
    finish_replace(self, edit, scope, line);
    replace_edit_destroy(edit);
    close_command_surface(self);
}

/* 区切りの不足・空のパターン・知らない旗は未知のコマンドと同じ 1 行（決定 8(b)）。 */
static void execute_substitute_command(struct folio_window *_Nonnull self,
                                       const char *_Nonnull argument)
{
    struct ex_substitute parts = {.pattern = 0,
                                  .pattern_length = 0,
                                  .replacement = 0,
                                  .replacement_length = 0,
                                  .global = false};
    if (!folio_command_parse_substitute(argument, strlen(argument), &parts))
    {
        command_not_found(self);
        return;
    }
    enum folio_state_outcome previewed = substitute_preview(self, argument, &parts);
    if (previewed != FOLIO_STATE_READY)
    {
        command_failure(self, previewed);
        return;
    }
    if (folio_state_replace_count(self->state) == 0)
    {
        report_no_match(self);
        return;
    }
    apply_substitute(self, parts.global);
}

/* GUI・キー・Exで同じ操作と引数を実行する（ADR0020）。 */
static void execute_command(struct folio_window *_Nonnull self, enum folio_command command,
                            const char *_Nonnull argument)
{
    switch (command)
    {
    case FOLIO_COMMAND_SAVE:
        execute_save_command(self, argument);
        return;
    case FOLIO_COMMAND_SAVE_AS:
        finish_save_command(self, command_save_as(self, argument));
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
    case FOLIO_COMMAND_EDIT:
        execute_mode_command(self, PANE_MODE_EDIT);
        return;
    case FOLIO_COMMAND_VIEW:
        execute_mode_command(self, PANE_MODE_VIEW);
        return;
    case FOLIO_COMMAND_NEW:
        execute_new_command(self);
        return;
    case FOLIO_COMMAND_RENAME:
        finish_save_command(self, command_rename(self, argument));
        return;
    case FOLIO_COMMAND_FIND:
        begin_search(self);
        return;
    case FOLIO_COMMAND_SET:
        execute_set_command(self, argument);
        return;
    case FOLIO_COMMAND_TOGGLE_NUMBER:
        apply_number(self, !folio_state_number(self->state));
        return;
    case FOLIO_COMMAND_REPLACE:
        begin_replace(self);
        return;
    case FOLIO_COMMAND_SUBSTITUTE:
        execute_substitute_command(self, argument);
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
/* 索引の鍵でカーソルを動かし、動いた行を見える位置へ寄せる（ADR 0015 の決定 2 / 6）。
 * カテゴリ行に止まるときは保存も開き直しもせず、描き直して寄せるだけ。 */
static void switch_adjacent(struct folio_window *_Nonnull self, enum folio_step step)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    struct note_ref target = {.category = 0, .note = 0};
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    if (folio_state_step_kind(self->state, step, &kind, &target) && kind == FOLIO_CURSOR_NOTE)
    {
        outcome = switch_note(self, target.category, target.note);
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
 * （ADR 0013 の決定 4 の (i)）。カーソルが移るので、最後は歩みと同じように見える位置へ寄せる。
 * 絞り込み中は開閉できない。`h` は絞り込み無しでは日常の移動なので、打鍵のたびに失敗箱を出さず
 * 静かに戻る（ADR 0024 の補正 1。断りの値そのものは application が持つ）。 */
static void expand_cursor(struct folio_window *_Nonnull self, bool expanded)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {.category = 0, .note = 0};
    if (!folio_state_cursor(self->state, &kind, &cursor))
    {
        return;
    }
    if (folio_state_filtering(self->state))
    {
        return;
    }
    enum folio_state_outcome outcome =
        expanded && kind == FOLIO_CURSOR_CATEGORY ? save_edit(self) : FOLIO_STATE_READY;
    /* 初回保存で名前が付いても、本文自体は同じRichEdit。保存後から比較する。 */
    struct note_ref before = {.category = 0, .note = 0};
    bool had = folio_state_selection(self->state, &before);
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

static void toggle_command_keys(struct folio_window *_Nonnull self)
{
    self->command_keys_visible = !self->command_keys_visible;
    arrange_command_input(self);
    redraw_command_layer(self);
}

static bool click_command_footer(struct folio_window *_Nonnull self, POINT point)
{
    RECT toggle = command_toggle_rect(self);
    RECT previous = command_previous_rect(self);
    RECT next = command_next_rect(self);
    if (PtInRect(&toggle, point))
    {
        if (PtInRect(&previous, point) || PtInRect(&next, point))
        {
            move_command_selection(self, PtInRect(&previous, point) ? VK_UP : VK_DOWN);
        }
        else
        {
            toggle_command_keys(self);
        }
        return true;
    }
    return false;
}

/* 欄の中の「前へ」「次へ」。押した向きを覚えてから 1 つ進む（ADR 0023 の決定 4）。 */
static bool click_search_buttons(struct folio_window *_Nonnull self, POINT point)
{
    RECT previous = surface_button_rect(self, 0);
    RECT next = surface_button_rect(self, 1);
    if (PtInRect(&previous, point))
    {
        step_search(self, SEARCH_DIRECTION_BACKWARD);
        return true;
    }
    if (PtInRect(&next, point))
    {
        step_search(self, SEARCH_DIRECTION_FORWARD);
        return true;
    }
    return false;
}

/* 欄の中の「1 件」「すべて」。押した範囲で下見の当て方が決まる（ADR 0028 の決定 8(a)）。 */
static bool click_replace_buttons(struct folio_window *_Nonnull self, POINT point)
{
    RECT one = surface_button_rect(self, 0);
    RECT all = surface_button_rect(self, 1);
    if (PtInRect(&one, point))
    {
        apply_replace(self, REPLACE_ONE);
        return true;
    }
    if (PtInRect(&all, point))
    {
        apply_replace(self, REPLACE_ALL);
        return true;
    }
    return false;
}

/* 下端の帯が持つボタン。押されたら true。 */
static bool click_surface_buttons(struct folio_window *_Nonnull self, POINT point)
{
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_SEARCH:
        return click_search_buttons(self, point);
    case COMMAND_SURFACE_REPLACE:
        return click_replace_buttons(self, point);
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_PALETTE:
        return false;
    }
    return false;
}

static void click_command_surface(struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_surface != COMMAND_SURFACE_PALETTE)
    {
        /* 置換の欄は 2 つあるので、既に自分の欄にいるならフォーカスを奪わない。 */
        if (!click_surface_buttons(self, point) && !command_owns(self, GetFocus()))
        {
            focus_command_input(self);
        }
        return;
    }
    if (click_command_footer(self, point))
    {
        return;
    }
    RECT rows = command_rows_rect(self);
    if (!PtInRect(&rows, point))
    {
        SetFocus(self->command_input);
        return;
    }
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_command command = FOLIO_COMMAND_SAVE;
    int row_height = scale(base_command_row_height, GetDpiForWindow(self->handle));
    size_t index = self->command_first + (size_t)((point.y - rows.top) / row_height);
    bool found = command_at_query(query, index, folio_state_language(self->state), &command);
    utf8_text_destroy(query);
    if (found)
    {
        execute_command(self, command, "");
    }
}

static bool append_operation(HMENU menu, size_t index, enum folio_language language)
{
    enum folio_command command = folio_command_at(index);
    const char *_Nonnull label = folio_command_label(command, language);
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(label, strlen(label), &wide) != UTF16_TEXT_CONVERTED)
    {
        return false;
    }
    bool appended = AppendMenuW(menu, MF_STRING, index + 1, utf16_text_units(wide)) != 0;
    utf16_text_destroy(wide);
    return appended;
}

static void show_operations(struct folio_window *_Nonnull self)
{
    HMENU menu = CreatePopupMenu();
    if (menu == nullptr)
    {
        command_failure(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    for (size_t index = 0; index < folio_command_count(); ++index)
    {
        /* メニュー ID は添字 + 1 なので、飛ばしても対応は壊れない（決定 8 の補正）。 */
        if (!folio_command_listed(folio_command_at(index)))
        {
            continue;
        }
        if (!append_operation(menu, index, folio_state_language(self->state)))
        {
            DestroyMenu(menu);
            command_failure(self, FOLIO_STATE_OUT_OF_MEMORY);
            return;
        }
    }
    RECT button = action_button_rect(self, 2);
    POINT point = {button.left, button.bottom};
    ClientToScreen(self->handle, &point);
    int chosen = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY, point.x, point.y,
                                  self->handle, nullptr);
    DestroyMenu(menu);
    if (chosen > 0 && (size_t)chosen <= folio_command_count())
    {
        execute_command(self, folio_command_at((size_t)chosen - 1), "");
    }
}

static bool click_actions(struct folio_window *_Nonnull self, POINT point)
{
    RECT fresh = action_button_rect(self, 0);
    RECT save = action_button_rect(self, 1);
    RECT operations = action_button_rect(self, 2);
    RECT help = action_button_rect(self, 3);
    if (PtInRect(&fresh, point))
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return true;
    }
    if (PtInRect(&save, point))
    {
        execute_command(self, FOLIO_COMMAND_SAVE, "");
        return true;
    }
    if (PtInRect(&operations, point))
    {
        show_operations(self);
        return true;
    }
    if (PtInRect(&help, point))
    {
        execute_command(self, FOLIO_COMMAND_HELP, "");
        return true;
    }
    RECT find = action_button_rect(self, 4);
    if (action_button_fits(self, 4) && PtInRect(&find, point))
    {
        execute_command(self, FOLIO_COMMAND_FIND, "");
        return true;
    }
    return false;
}

/* 無効な側の札のクリックだけが意図になる。 */
static void click_caption(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (click_actions(self, point))
    {
        return;
    }
    RECT close = close_rect(self);
    if (PtInRect(&close, point))
    {
        execute_command(self, FOLIO_COMMAND_SAVE_QUIT, "");
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
        execute_command(self, FOLIO_COMMAND_VIEW, "");
        break;
    case PANE_MODE_EDIT:
        execute_command(self, FOLIO_COMMAND_EDIT, "");
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

/* 索引と閲覧本文で共有する検索の 4 打（ADR 0023 の決定 4 / 5）。
 * 編集中の本文と各入力欄では呼ばないので、そこでは普通の文字入力のまま。 */
static bool search_character(struct folio_window *_Nonnull self, WPARAM character)
{
    switch (character)
    {
    case '/':
        open_search(self, SEARCH_DIRECTION_FORWARD);
        return true;
    case '?':
        open_search(self, SEARCH_DIRECTION_BACKWARD);
        return true;
    case 'n':
        repeat_search(self, false);
        return true;
    case 'N':
        repeat_search(self, true);
        return true;
    default:
        /* WM_CHAR の文字は開いた集合（C-017）。 */
        return false;
    }
}

static bool view_character(struct folio_window *_Nonnull self, WPARAM character)
{
    if (folio_state_pane_mode(self->state) != PANE_MODE_VIEW)
    {
        return false;
    }
    if (character == ':')
    {
        open_command_surface(self, COMMAND_SURFACE_EX);
        return true;
    }
    if (character == 'i')
    {
        execute_command(self, FOLIO_COMMAND_EDIT, "");
        return true;
    }
    return search_character(self, character);
}

static LRESULT pane_character(struct folio_window *_Nonnull self, WPARAM character)
{
    if (character == new_character)
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return 1;
    }
    if (character == store_character)
    {
        execute_command(self, FOLIO_COMMAND_SAVE, "");
        return 1;
    }
    if (character == palette_character)
    {
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        return 1;
    }
    return view_character(self, character) ? 1 : 0;
}

static LRESULT pane_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key == VK_F1)
    {
        execute_command(self, FOLIO_COMMAND_HELP, "");
        return 1;
    }
    if (key == VK_ESCAPE)
    {
        leave_pane(self);
        return 1;
    }
    return 0;
}

/* RichEdit の鍵の通知。IME変換中は親の操作に変換しない。 */
static LRESULT on_notify(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const NMHDR *_Nonnull header = (const NMHDR *)lparam;
    if (header->code != EN_MSGFILTER || self->pane == nullptr || note_pane_composing(self->pane))
    {
        return 0;
    }
    const MSGFILTER *_Nonnull filter = (const MSGFILTER *)lparam;
    if (filter->msg == WM_CHAR)
    {
        return pane_character(self, filter->wParam);
    }
    if (filter->msg == WM_KEYDOWN)
    {
        return pane_key(self, filter->wParam);
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
    if (key == VK_F1)
    {
        execute_command(self, FOLIO_COMMAND_HELP, "");
        return;
    }
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
    case '/':
    case '?':
    case 'n':
    case 'N':
        /* `?` はヘルプではなく後方検索（ADR 0023 の決定 6）。ヘルプは F1 / `:h` / Ctrl+P。 */
        (void)search_character(self, character);
        break;
    case 'i':
        execute_command(self, FOLIO_COMMAND_EDIT, "");
        break;
    case new_character:
        execute_command(self, FOLIO_COMMAND_NEW, "");
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
        execute_command(self, FOLIO_COMMAND_SAVE, "");
        break;
    default:
        /* WM_CHAR の文字は開いた集合（C-017）。 */
        break;
    }
}

static void command_not_found(struct folio_window *_Nonnull self)
{
    clear_command_status(self);
    self->command_unknown = true;
    arrange_command_input(self);
    redraw_command_layer(self);
}

static void execute_command_input(struct folio_window *_Nonnull self)
{
    /* 検索欄の Enter は欄を閉じず、覚えている向きで次の一致へ進む（ADR 0023 の決定 4）。 */
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        repeat_search(self, false);
        return;
    }
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
    size_t argument = length;
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
        utf8_text_destroy(query);
        return;
    case COMMAND_SURFACE_EX:
        if (length == 0 || (length == 1 && bytes[0] == ':'))
        {
            utf8_text_destroy(query);
            close_command_surface(self);
            return;
        }
        found = folio_command_parse(bytes, length, &command, &argument);
        break;
    case COMMAND_SURFACE_PALETTE:
        found = command_at_query(query, self->command_selection, folio_state_language(self->state),
                                 &command);
        break;
    }
    if (!found)
    {
        utf8_text_destroy(query);
        command_not_found(self);
        return;
    }
    execute_command(self, command, bytes + argument);
    utf8_text_destroy(query);
}

static void move_command_selection(struct folio_window *_Nonnull self, WPARAM key)
{
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    size_t count = command_matches_count(query, folio_state_language(self->state));
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
    reveal_command_selection(self);
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

/* Tab はパレットでは説明の開閉、置換の欄では 2 つの EDIT の往復（ADR 0016 の補正）。 */
static bool command_tab(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        toggle_command_keys(self);
        return true;
    }
    if (self->command_surface != COMMAND_SURFACE_REPLACE)
    {
        return false;
    }
    HWND focus = GetFocus();
    HWND _Nullable next =
        focus == self->replace_inputs[0] ? self->replace_inputs[1] : self->replace_inputs[0];
    if (next != nullptr)
    {
        SetFocus(next);
    }
    return true;
}

/* 置換の欄の Enter は素と Ctrl+ を見分けるので WM_CHAR で処理し、ここでは飲み込むだけ。 */
static bool command_return(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_REPLACE)
    {
        return true;
    }
    execute_command_input(self);
    return true;
}

static bool command_key_down(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->command_composing)
    {
        return false;
    }
    if (key == VK_F1)
    {
        execute_command(self, FOLIO_COMMAND_HELP, "");
        return true;
    }
    if (key == VK_TAB)
    {
        return command_tab(self);
    }
    if (key == VK_ESCAPE)
    {
        close_command_surface(self);
        return true;
    }
    if (key == VK_RETURN)
    {
        return command_return(self);
    }
    bool moves =
        self->command_surface == COMMAND_SURFACE_PALETTE && (key == VK_UP || key == VK_DOWN);
    if (moves)
    {
        move_command_selection(self, key);
    }
    return moves;
}

/* 置換の欄の Enter。EDIT には素の Enter が 0x0D、Ctrl+Enter が 0x0A で届くので、
 * 鍵の状態を問い合わせずに見分けられる（ARC-007）。 */
static bool replace_character(struct folio_window *_Nonnull self, WPARAM character)
{
    if (self->command_surface != COMMAND_SURFACE_REPLACE)
    {
        return false;
    }
    if (character != '\r' && character != '\n')
    {
        return false;
    }
    apply_replace(self, character == '\n' ? REPLACE_ALL : REPLACE_ONE);
    return true;
}

static bool command_character(struct folio_window *_Nonnull self, WPARAM character)
{
    if (self->command_composing)
    {
        return false;
    }
    if (replace_character(self, character))
    {
        return true;
    }
    if (character == store_character)
    {
        /* 保存して欄は開いたまま（ADR 0016 の補正 2）。鍵の状態は読まない（ARC-007）。 */
        store_from_surface(self);
        return true;
    }
    if (character == new_character)
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return true;
    }
    if (character == palette_character)
    {
        show_command_palette(self);
        return true;
    }
    return character == '\r' || character == '\t';
}

static bool command_wheel(struct folio_window *_Nonnull self, WPARAM wparam)
{
    if (self->command_surface != COMMAND_SURFACE_PALETTE || self->command_composing)
    {
        return false;
    }
    int delta = GET_WHEEL_DELTA_WPARAM(wparam);
    if (delta != 0)
    {
        move_command_selection(self, delta > 0 ? VK_UP : VK_DOWN);
    }
    return true;
}

/* 入力面が自分で受け止める鍵。受け止めたら元の手続きへ渡さない。 */
static bool command_input_handled(struct folio_window *_Nonnull self, UINT message, WPARAM wparam)
{
    if (message == WM_KEYDOWN)
    {
        return command_key_down(self, wparam);
    }
    if (message == WM_CHAR)
    {
        return command_character(self, wparam);
    }
    if (message == WM_MOUSEWHEEL)
    {
        return command_wheel(self, wparam);
    }
    return false;
}

/* 未確定入力を抜けたあとに、開いている面ごとに 1 回だけ取り直す。 */
static void command_composition_settled(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        search_input_changed(self);
    }
    if (self->command_surface == COMMAND_SURFACE_REPLACE)
    {
        update_replace_preview(self);
    }
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
    if (command_input_handled(self, message, wparam))
    {
        return 0;
    }
    if (message == WM_KILLFOCUS)
    {
        PostMessageW(self->handle, folio_message_command_focus_lost, 0, 0);
    }
    LRESULT result = CallWindowProcW(self->command_input_original, window, message, wparam, lparam);
    /* 確定した文字は composition を抜けたあとに届くので、抜けてから 1 回だけ数え直す。 */
    if (message == WM_IME_ENDCOMPOSITION)
    {
        command_composition_settled(self);
    }
    return result;
}

/* 打つたびに語を覚え直し、いまの向きで最初の一致へ進める（インクリメンタル）。
 * IME の未確定入力のあいだは動かさず、確定の WM_IME_ENDCOMPOSITION のあとで 1 回だけ進む。 */
static void search_input_changed(struct folio_window *_Nonnull self)
{
    if (self->command_composing || self->command_input == nullptr)
    {
        return;
    }
    wchar_t units[command_input_capacity];
    int count = GetWindowTextW(self->command_input, units, (int)command_input_capacity);
    enum folio_state_outcome kept = folio_state_set_search_term(
        self->state, (const char16_t *)units, count > 0 ? (size_t)count : 0);
    /* 壊れた語は 0 件と同じ欄の中の 1 行で伝える。打つたびにモーダルを出さない。 */
    if (kept == FOLIO_STATE_SEARCH_MALFORMED)
    {
        self->search_outcome = NOTE_SEARCH_MALFORMED;
        self->search_total = 0;
        self->search_ordinal = 0;
        redraw_command_layer(self);
        return;
    }
    if (kept != FOLIO_STATE_READY)
    {
        command_failure(self, kept);
        return;
    }
    update_search(self, folio_state_search_direction(self->state), false);
    redraw_command_layer(self);
}

static void update_filter_composition(struct folio_window *_Nonnull self, UINT message)
{
    if (message == WM_IME_STARTCOMPOSITION)
    {
        self->filter_composing = true;
    }
    if (message == WM_IME_ENDCOMPOSITION)
    {
        self->filter_composing = false;
    }
}

/* 欄の中でも「どこにいても」効く操作（F1・Ctrl+P・Ctrl+N）。区画の pane_character / pane_key と
 * 同じ形で共通の操作へ渡す（ADR 0024 の補正 5）。受け止めたら true。 */
static bool filter_input_anywhere(struct folio_window *_Nonnull self, UINT message, WPARAM wparam)
{
    if (message == WM_KEYDOWN && wparam == VK_F1)
    {
        execute_command(self, FOLIO_COMMAND_HELP, "");
        return true;
    }
    if (message != WM_CHAR)
    {
        return false;
    }
    if (wparam == new_character)
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return true;
    }
    if (wparam == palette_character)
    {
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        return true;
    }
    return false;
}

/* 欄が自分で受け止める鍵。**Esc と Enter は INDEX（主窓）へ戻し、絞り込みは保つ**。
 * 欄の中の Ctrl+S は共通の SAVE へ渡す。`/` `?` `n` `N` `:` は文字のまま（ADR 0024 の決定 6）。 */
static bool filter_input_handled(struct folio_window *_Nonnull self, UINT message, WPARAM wparam)
{
    if (self->filter_composing)
    {
        return false;
    }
    if (filter_input_anywhere(self, message, wparam))
    {
        return true;
    }
    if (message == WM_KEYDOWN && (wparam == VK_ESCAPE || wparam == VK_RETURN))
    {
        SetFocus(self->handle);
        return true;
    }
    if (message == WM_CHAR && wparam == store_character)
    {
        /* 共通の保存だが、開いている入力面を閉じない（ADR 0016 の補正 2）。 */
        store_from_surface(self);
        return true;
    }
    /* Enter と Esc の WM_CHAR は既定処理がビープを出すので飲む。 */
    return message == WM_CHAR && (wparam == '\r' || wparam == '\x1b');
}

/* 空のあいだは用途を薄い文字で重ねる（EM_SETCUEBANNER は comctl32 を要るので使わない）。 */
static void paint_filter_placeholder(const struct folio_window *_Nonnull self, HWND window)
{
    if (GetWindowTextLengthW(window) != 0)
    {
        return;
    }
    HDC device = GetDC(window);
    if (device == nullptr)
    {
        return;
    }
    RECT bounds;
    GetClientRect(window, &bounds);
    bounds.left += scale(base_filter_text_inset, GetDpiForWindow(self->handle));
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetBkMode(device, TRANSPARENT);
    SetTextColor(device, self->palette.header_text);
    DrawTextW(device, filter_placeholder, -1, &bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(device, previous);
    ReleaseDC(window, device);
}

static LRESULT CALLBACK filter_input_procedure(HWND window, UINT message, WPARAM wparam,
                                               LPARAM lparam)
{
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr || self->filter_original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    update_filter_composition(self, message);
    if (filter_input_handled(self, message, wparam))
    {
        return 0;
    }
    LRESULT result = CallWindowProcW(self->filter_original, window, message, wparam, lparam);
    /* 確定した文字は composition を抜けたあとに届くので、抜けてから 1 回だけ絞り直す。 */
    if (message == WM_IME_ENDCOMPOSITION)
    {
        filter_input_changed(self);
    }
    if (message == WM_PAINT)
    {
        paint_filter_placeholder(self, window);
    }
    return result;
}

/* 打つたびに語を覚え直し、索引を作り直す（ADR 0024 の決定 6）。
 * IME の未確定入力のあいだは動かさず、確定の WM_IME_ENDCOMPOSITION のあとで 1 回だけ動く。
 * 壊れた語は前の絞り込みを保つだけで、打つたびにモーダルを出さない。 */
static void filter_input_changed(struct folio_window *_Nonnull self)
{
    if (self->filter_composing || self->filter_input == nullptr)
    {
        return;
    }
    wchar_t units[filter_input_capacity];
    int count = GetWindowTextW(self->filter_input, units, (int)filter_input_capacity);
    enum folio_state_outcome filtered = folio_state_set_index_filter(
        self->state, (const char16_t *)units, count > 0 ? (size_t)count : 0);
    if (filtered == FOLIO_STATE_OUT_OF_MEMORY)
    {
        command_failure(self, filtered);
        return;
    }
    if (self->drawer != nullptr)
    {
        drawer_window_reveal_cursor(self->drawer);
    }
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
}

/* Ctrl+Shift+F は常設の欄へフォーカスを移し、いまの語を全選択する（ADR 0024 の決定 6）。 */
static void focus_filter_input(struct folio_window *_Nonnull self)
{
    if (self->filter_input == nullptr)
    {
        return;
    }
    SetFocus(self->filter_input);
    SendMessageW(self->filter_input, EM_SETSEL, 0, (LPARAM)-1);
}

static LRESULT on_command(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (!command_owns(self, (HWND)lparam) || HIWORD(wparam) != EN_CHANGE)
    {
        return 0;
    }
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        search_input_changed(self);
        return 0;
    }
    if (self->command_surface == COMMAND_SURFACE_REPLACE)
    {
        /* 打つたびに下見を取り直す。未確定入力のあいだは動かさない（検索欄と同じ流儀）。 */
        if (!self->command_composing)
        {
            update_replace_preview(self);
        }
        return 0;
    }
    self->command_selection = 0;
    self->command_first = 0;
    clear_command_status(self);
    arrange_command_input(self);
    redraw_command_layer(self);
    return 0;
}

/* 常設の絞り込みの欄を主窓の子として作る。ドロワーより後に作るので前面に重なる。 */
static bool create_filter_input(struct folio_window *_Nonnull self, HWND window)
{
    self->filter_input = CreateWindowExW(0, edit_class, L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                         0, 0, 0, 0, window, (HMENU)(INT_PTR)filter_control_id,
                                         GetModuleHandleW(nullptr), nullptr);
    if (self->filter_input == nullptr)
    {
        return false;
    }
    SetWindowLongPtrW(self->filter_input, GWLP_USERDATA, (LONG_PTR)self);
    self->filter_original = (WNDPROC)SetWindowLongPtrW(self->filter_input, GWLP_WNDPROC,
                                                       (LONG_PTR)filter_input_procedure);
    if (self->filter_original == nullptr)
    {
        return false;
    }
    SendMessageW(self->filter_input, EM_LIMITTEXT, filter_input_capacity - 1, 0);
    SendMessageW(self->filter_input, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    return true;
}

/* 入力面の EDIT を 1 つ作って主窓に結ぶ。どれも同じ EDIT クラスなので、元の手続きは
 * 1 つの場所に覚える（ADR 0016 の 2026-09-22 の補正）。 */
static bool attach_command_input(struct folio_window *_Nonnull self, HWND _Nonnull input)
{
    SetWindowLongPtrW(input, GWLP_USERDATA, (LONG_PTR)self);
    WNDPROC _Nullable original =
        (WNDPROC)SetWindowLongPtrW(input, GWLP_WNDPROC, (LONG_PTR)command_input_procedure);
    if (original == nullptr)
    {
        return false;
    }
    self->command_input_original = original;
    SendMessageW(input, EM_LIMITTEXT, command_input_capacity - 1, 0);
    SendMessageW(input, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    return true;
}

static HWND _Nullable create_command_edit(struct folio_window *_Nonnull self, int control)
{
    HWND _Nullable input = CreateWindowExW(0, edit_class, L"", WS_CHILD | ES_AUTOHSCROLL, 0, 0, 0,
                                           0, self->command_layer, (HMENU)(INT_PTR)control,
                                           GetModuleHandleW(nullptr), nullptr);
    if (input == nullptr || !attach_command_input(self, input))
    {
        return nullptr;
    }
    return input;
}

/* Ex・パレット・検索が使う 1 つと、置換の欄の 2 つ（ADR 0028 の決定 8(a)）。 */
static bool create_command_inputs(struct folio_window *_Nonnull self)
{
    static const int controls[replace_input_count] = {replace_pattern_control_id,
                                                      replace_replacement_control_id};
    self->command_input = create_command_edit(self, command_control_id);
    if (self->command_input == nullptr)
    {
        return false;
    }
    for (size_t index = 0; index < replace_input_count; ++index)
    {
        self->replace_inputs[index] = create_command_edit(self, controls[index]);
        if (self->replace_inputs[index] == nullptr)
        {
            return false;
        }
    }
    return true;
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
    if (!create_filter_input(self, window))
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
    if (!create_command_inputs(self))
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

static LRESULT color_command_input(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    bool ours = command_owns(self, (HWND)lparam) || (HWND)lparam == self->filter_input;
    if (!ours || self->command_brush == nullptr)
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
    case WM_MOUSEWHEEL:
        command_wheel(self, wparam);
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
    bool moved_within_window = focus != nullptr && !command_owns(self, focus) &&
                               GetAncestor(focus, GA_ROOT) == self->handle;
    if (self->command_surface != COMMAND_SURFACE_CLOSED && moved_within_window)
    {
        dismiss_command_surface(self);
    }
}

/* HACCEL が届けた WM_COMMAND を同じ dispatcher へ渡す（ADR 0016 の決定 2）。 */
static void execute_accelerator(struct folio_window *_Nonnull self, WPARAM wparam)
{
    if (HIWORD(wparam) != 1)
    {
        return;
    }
    if (LOWORD(wparam) == save_as_accelerator)
    {
        execute_command(self, FOLIO_COMMAND_SAVE_AS, "");
    }
    if (LOWORD(wparam) == rename_accelerator)
    {
        execute_command(self, FOLIO_COMMAND_RENAME, "");
    }
    if (LOWORD(wparam) == find_accelerator)
    {
        execute_command(self, FOLIO_COMMAND_FIND, "");
    }
    if (LOWORD(wparam) == filter_accelerator)
    {
        focus_filter_input(self);
    }
    if (LOWORD(wparam) == search_next_accelerator)
    {
        repeat_search(self, false);
    }
    if (LOWORD(wparam) == search_previous_accelerator)
    {
        repeat_search(self, true);
    }
    if (LOWORD(wparam) == search_forward_accelerator)
    {
        step_search(self, SEARCH_DIRECTION_FORWARD);
    }
    if (LOWORD(wparam) == search_backward_accelerator)
    {
        step_search(self, SEARCH_DIRECTION_BACKWARD);
    }
}

/* 主窓の WM_COMMAND には HACCEL と、常設の欄の EN_CHANGE の 2 つが届く。 */
/* 本文の RichEdit からの通知。#40 の絞り込みの欄と同じ窓に届くので、送り手の HWND と
 * control id で振り分ける（ADR 0026 の決定 4）。 */
static bool pane_notification(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    if (pane == nullptr || (HWND)lparam != pane || LOWORD(wparam) != note_pane_control_id)
    {
        return false;
    }
    if (HIWORD(wparam) == EN_CHANGE)
    {
        /* 印と無効化だけ。EM_STREAMIN の最中に本文へ問い合わせない（決定 4）。 */
        note_pane_invalidate_lines(self->pane);
        refresh_gutter(self, false);
        return true;
    }
    if (HIWORD(wparam) == EN_VSCROLL)
    {
        refresh_gutter(self, true);
        return true;
    }
    return false;
}

static LRESULT on_window_command(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if ((HWND)lparam == self->filter_input && HIWORD(wparam) == EN_CHANGE)
    {
        filter_input_changed(self);
        return 0;
    }
    if (pane_notification(self, wparam, lparam))
    {
        return 0;
    }
    /* 加速表の WM_COMMAND は lparam が 0 で HIWORD が 1 に限る（決定 4）。 */
    if (lparam == 0)
    {
        execute_accelerator(self, wparam);
    }
    return 0;
}

/* 頭のパンくずと札は幅に依存する位置に描く（ADR 0011 の決定 2）。
 * 寸法の変更では最初に見える論理行を戻さない（戻すのは帯の幅と number の切替だけ・ADR 0026）。 */
static void resize(struct folio_window *_Nonnull self)
{
    arrange(self);
    InvalidateRect(self->handle, nullptr, FALSE);
}

static LRESULT on_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                          LPARAM lparam)
{
    switch (message)
    {
    case WM_NCHITTEST:
        return hit_test(self, lparam);
    case WM_SIZE:
        resize(self);
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
    case folio_message_pane_scrolled:
        refresh_gutter(self, true);
        return 0;
    case folio_message_gutter_resized:
        rearrange_keeping_line(self);
        return 0;
    case folio_message_select_note:
        /* ドロワーからのノート行のクリック。結果は enum folio_state_outcome で返す。 */
        return (LRESULT)switch_note(self, (size_t)wparam, (size_t)lparam);
    case WM_COMMAND:
        return on_window_command(self, wparam, lparam);
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
        execute_command(self, FOLIO_COMMAND_SAVE_QUIT, "");
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
    ACCEL shortcuts[] = {
        {.fVirt = FVIRTKEY | FCONTROL | FSHIFT, .key = 'S', .cmd = save_as_accelerator},
        {.fVirt = FVIRTKEY, .key = VK_F2, .cmd = rename_accelerator},
        {.fVirt = FVIRTKEY | FCONTROL, .key = 'F', .cmd = find_accelerator},
        /* Ctrl+Shift+F は常設の絞り込みの欄へ（ADR 0024 の決定 6）。 */
        {.fVirt = FVIRTKEY | FCONTROL | FSHIFT, .key = 'F', .cmd = filter_accelerator},
        /* F3 は前方・Shift+F3 は後方を名指しする。覚えている向きは変えない（採用済み計画 #47）。 */
        {.fVirt = FVIRTKEY, .key = VK_F3, .cmd = search_forward_accelerator},
        {.fVirt = FVIRTKEY | FSHIFT, .key = VK_F3, .cmd = search_backward_accelerator}};
    self->commands =
        CreateAcceleratorTableW(shortcuts, (int)(sizeof shortcuts / sizeof shortcuts[0]));
    /* Enter と Shift+Enter は検索欄にいるあいだだけ。鍵の修飾は OS の表に読ませ、
     * こちらから鍵の状態を問い合わせない（ARC-007）。 */
    ACCEL stepping[] = {
        {.fVirt = FVIRTKEY, .key = VK_RETURN, .cmd = search_next_accelerator},
        {.fVirt = FVIRTKEY | FSHIFT, .key = VK_RETURN, .cmd = search_previous_accelerator}};
    self->search_keys =
        CreateAcceleratorTableW(stepping, (int)(sizeof stepping / sizeof stepping[0]));
    if (self->commands == nullptr || self->search_keys == nullptr)
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
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

/* 名前入力など別のモーダルへ主窓のキーを漏らさない。compositionも各所有者へ確認する。 */
static bool command_target(const struct folio_window *_Nonnull window, HWND target)
{
    if (command_owns(window, target))
    {
        return !window->command_composing;
    }
    if (target == window->filter_input)
    {
        return !window->filter_composing;
    }
    if (window->pane != nullptr && target == note_pane_handle(window->pane))
    {
        return !note_pane_composing(window->pane);
    }
    return target == window->handle;
}

/* 検索欄にいて未確定入力でないときだけ、Enter / Shift+Enter を OS の表に読ませる。 */
static bool search_target(const struct folio_window *_Nonnull window, HWND target)
{
    return window->command_surface == COMMAND_SURFACE_SEARCH && target == window->command_input &&
           !window->command_composing && window->search_keys != nullptr;
}

bool folio_window_translate(const struct folio_window *_Nonnull window, const MSG *_Nonnull message)
{
    if (search_target(window, message->hwnd))
    {
        MSG stepping = *message;
        if (TranslateAcceleratorW(window->handle, window->search_keys, &stepping) != 0)
        {
            return true;
        }
    }
    if (command_target(window, message->hwnd))
    {
        MSG translated = *message;
        if (TranslateAcceleratorW(window->handle, window->commands, &translated) != 0)
        {
            return true;
        }
    }
    if (window->pane == nullptr || folio_state_pane_mode(window->state) != PANE_MODE_EDIT)
    {
        return false;
    }
    return note_pane_translate(window->pane, message);
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
    if (window->commands != nullptr)
    {
        DestroyAcceleratorTable(window->commands);
    }
    if (window->search_keys != nullptr)
    {
        DestroyAcceleratorTable(window->search_keys);
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
