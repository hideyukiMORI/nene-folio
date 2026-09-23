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
#include "folio_theme_choice.h"
#include "gdiplus_flat.h"
#include "icon_paint.h"
#include "name_prompt.h"
#include "note_name.h"
#include "note_pane.h"
#include "note_ref.h"
#include "note_search.h"
#include "replace_edit.h"
#include "settings_row_kind.h"
#include "ui_face.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <dwmapi.h>
#include <imm.h>
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
    /* レイヤーから外した IME の文脈。窓を壊す前に戻す（MSDN の作法） */
    HIMC _Nullable layer_context;
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
    /* ヘルプが箱に入りきらないときに送り出す先頭の行（PgUp / PgDn・#69）。 */
    size_t command_help_first;
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
    /* GDI+ の寿命の印（ADR 0033 の決定 4）。0 なら起動していない。窓を作る前に起動し、
     * 窓を壊したあと（子窓の WM_NCDESTROY も済んだあと）に終える。 */
    ULONG_PTR gdiplus_token;
};

static const wchar_t class_name[] = L"NeNeFolioWindow";
static const wchar_t command_layer_class[] = L"NeNeFolioCommandLayer";
static const wchar_t mono_face[] = L"Consolas";
static const wchar_t edit_class[] = L"EDIT";
/* 一覧に出すキー操作の行（ADR 0030 の決定 4）。文言は core の ui_text が持ち、
 * ここは並びだけを持つ。1 行 1 ID で、欄分けは単位 C（ADR 0032）。 */
static const enum ui_text command_shortcuts[] = {
    UI_TEXT_HELP_PALETTE,       UI_TEXT_HELP_EDITOR_MOTION,   UI_TEXT_HELP_GLOBAL_KEYS,
    UI_TEXT_HELP_GLOBAL_FILTER, UI_TEXT_HELP_FILTER_FIELD,    UI_TEXT_HELP_FILTER_LIMITS,
    UI_TEXT_HELP_FILE_KEYS,     UI_TEXT_HELP_INDEX_COMMAND,   UI_TEXT_HELP_EX_SET_NUMBER,
    UI_TEXT_HELP_EX_SET_THEME,  UI_TEXT_HELP_EX_SET_LANGUAGE, UI_TEXT_HELP_REPLACE_FIELD,
    UI_TEXT_HELP_EX_SUBSTITUTE, UI_TEXT_HELP_SEARCH_KEYS,     UI_TEXT_HELP_SEARCH_STEP,
    UI_TEXT_HELP_SEARCH_FIELD,  UI_TEXT_HELP_INDEX_MOTION,    UI_TEXT_HELP_INDEX_FOLD,
    UI_TEXT_HELP_INDEX_SCROLL,  UI_TEXT_HELP_EDITOR_ESCAPE,
};

/* 設定画面の行（ADR 0031 の決定 7・ADR 0032 の決定 7）。見出しと選択肢を**種別で**持ち、
 * value はその種別の列挙の値（THEME なら folio_theme_choice・LANGUAGE なら folio_language）。
 * 段を足すときはここに行を足し、採用の閉じた switch に枝を足す（添字を型へ鋳込まない）。 */
static const struct
{
    enum ui_text label;
    enum settings_row_kind kind;
    unsigned char value;
} settings_rows[] = {
    {UI_TEXT_SETTINGS_THEME, SETTINGS_ROW_HEADING, 0},
    {UI_TEXT_SETTINGS_THEME_SYSTEM, SETTINGS_ROW_THEME, FOLIO_THEME_CHOICE_SYSTEM},
    {UI_TEXT_SETTINGS_THEME_LIGHT, SETTINGS_ROW_THEME, FOLIO_THEME_CHOICE_LIGHT},
    {UI_TEXT_SETTINGS_THEME_DARK, SETTINGS_ROW_THEME, FOLIO_THEME_CHOICE_DARK},
    {UI_TEXT_SETTINGS_LANGUAGE, SETTINGS_ROW_HEADING, 0},
    {UI_TEXT_SETTINGS_LANGUAGE_JA, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_JA},
    {UI_TEXT_SETTINGS_LANGUAGE_EN, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_EN},
    {UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_ZH_HANS},
};
/* 選択肢が始まる行（最初の見出しの次）。カーソルはこの行より上へは行かない。 */
constexpr size_t settings_first_choice_row = 1;

static size_t settings_row_count(void)
{
    return sizeof settings_rows / sizeof settings_rows[0];
}

static bool settings_row_choice(size_t index)
{
    return settings_rows[index].kind != SETTINGS_ROW_HEADING;
}

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
/* 絞り込みが効いているあいだ欄の右端に出す「×」（ADR 0024 の 2026-09-23 の補正 6）。
 * 箱は欄の高さに収まる 20px の正方形で、24 の viewBox はここへ写る（ADR 0033 の補正 9）。 */
constexpr int base_filter_clear_size = 20;
constexpr size_t command_input_capacity = 256;
/* 置換の欄の 2 つの EDIT の control id（ADR 0028 の決定 8(a)）。1 = Ex/パレット・2 = 絞り込み・
 * 3 = 本文の RichEdit の次に続く。 */
constexpr int replace_pattern_control_id = 4;
constexpr int replace_replacement_control_id = 5;
/* 「k / n 件」と向きの言い換えを 1 行に組む領域（UTF-8）。 */
constexpr size_t search_status_capacity = 128;
/* 「対象名 / k 件」と失敗の 1 行（＋「 位置 N」）を組む領域。ノート名は 255 バイトまで。 */
constexpr size_t replace_status_capacity = 512;
/* 1 回の描画・測定で UTF-16 へ写せる単位数（ADR 0030 の決定 5）。表の 1 行（ui_text_unit_limit）・
 * 状態の 1 行（replace_status_capacity）・ノート名（255 バイト）のどれもここに収まる。
 * 収まらなければ何も描かない（描画のたびの確保をやめる代わりの上限）。 */
constexpr size_t draw_unit_limit = 1024;

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
/* ヘルプを出しても操作の一覧に残す行数。最小寸法（560×360）でここが 0 になっていた（#69）。 */
constexpr int command_row_floor = 3;
/* 札の右に出す「n/m」の幅と、それを組む領域（数字と `/` だけ・翻訳の対象を増やさない・#69）。 */
constexpr int base_command_pages_width = 44;
constexpr size_t command_pages_capacity = 16;
constexpr int base_ex_height = 32;
constexpr int base_command_row_padding = 12;
constexpr int base_command_alias_room = 96;
constexpr int base_command_alias_gap = 12;
constexpr int base_command_input_vertical_inset = 2;
constexpr int base_search_button_width = 64;
/* 操作行の常設ボタンを出せる最小の余白（これより狭ければ「操作 ▾」のメニューへ移る）。 */
constexpr int base_action_margin = 8;
/* 頭の釦の字の左の字下げと右の余白、釦と釦の間隔（96 DPI・#122）。 */
constexpr int base_action_inset = 8;
constexpr int base_action_trail = 6;
constexpr int base_action_gap = 6;
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
/* OS の「アプリのモード」の変更を知らせる WM_SETTINGCHANGE の lParam（ADR 0031 の決定 4）。 */
static const wchar_t immersive_color_set[] = L"ImmersiveColorSet";
/* 設定の歯車と、現在の選択の印の寸法（96 DPI）。歯車は close_rect と同じ 24px の箱へ
 * 面のパスで塗る（ADR 0033 の決定 6）ので、放射線の長さの定数は要らない。 */
constexpr int base_settings_gap = 6;
constexpr int base_settings_indent = 20;
/* 選択の印の箱は 24 の viewBox と 1 対 1 の正方形で、行の左端から 4px の所に置く。
 * 印の ink は箱の左端から 8 単位なので、描かれる位置は線で描いていたとき（row.left + 12）と
 * 変わらない。選択肢の札は row.left + 32 から始まるので重ならない（ADR 0033 の決定 6）。 */
constexpr int base_settings_mark_inset = 4;
constexpr int base_settings_mark_box = 24;

static void execute_command(struct folio_window *_Nonnull self, enum folio_command command,
                            const char *_Nonnull argument);
static enum folio_state_outcome store_body(const struct folio_window *_Nonnull self);
static enum folio_state_outcome command_save_as(const struct folio_window *_Nonnull self,
                                                const char *_Nonnull argument);
static void close_command_surface(struct folio_window *_Nonnull self);
static void decorate(HWND handle, struct folio_palette palette);
static void render_pane(const struct folio_window *_Nonnull self);
static void show_settings_surface(struct folio_window *_Nonnull self);
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

/* 面が持つ操作の行数。#75 の設定画面（ADR 0029 の決定 9）はここへ自分の行数を足す。 */
static int command_surface_rows(const struct folio_window *_Nonnull self)
{
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_PALETTE:
        return (int)folio_command_listed_count();
    case COMMAND_SURFACE_SETTINGS:
        return (int)settings_row_count();
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
        return 0;
    }
    return 0;
}

/* 面が持つヘルプの行数。command_shortcuts[] を数えるのはここだけ（#69）。 */
static int command_help_rows(const struct folio_window *_Nonnull self)
{
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_PALETTE:
        return self->command_keys_visible
                   ? (int)(sizeof command_shortcuts / sizeof command_shortcuts[0])
                   : 0;
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
    case COMMAND_SURFACE_SETTINGS:
        return 0;
    }
    return 0;
}

/* 箱の望む高さ（96 DPI の基準値）。面ごとの行数を受ける唯一の式で、#75 も同じ式を使う（#69）。
 * 内訳は 余白×2 ＋ 欄 ＋ 間 ＋ 操作 32×r ＋ 札 ＋ 案内 2 行 ＋ ヘルプ 14×h ＋ 状態。 */
static int command_box_height(int rows, int help_rows)
{
    return base_command_palette_padding * 2 + base_command_input_height + base_command_gap +
           base_command_row_height * rows + base_command_status_height +
           base_command_help_row_height * (help_rows + 2) + base_command_status_height;
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
    /* 設定画面は EDIT を持たず、レイヤー自身がフォーカスを取る（ADR 0031 の決定 7）。 */
    if (self->command_surface == COMMAND_SURFACE_SETTINGS && window == self->command_layer)
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
    if (self->command_surface == COMMAND_SURFACE_REPLACE)
    {
        return self->replace_inputs[0];
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return self->command_layer;
    }
    return self->command_input;
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
    /* 箱の高さは「面が出す行の数」で決める。登録表の総数で取ると 1 行ぶん余る（補正 9）。
     * 望む高さであり、窓に入らなければ下で丸める。入った分の配り方は command_visible_help。 */
    int height =
        scale(command_box_height(command_surface_rows(self), command_help_rows(self)), dpi);
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
    case COMMAND_SURFACE_SETTINGS:
        return command_palette_rect(self);
    }
    return command_palette_rect(self);
}

static RECT command_input_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    if (self->command_surface == COMMAND_SURFACE_PALETTE ||
        self->command_surface == COMMAND_SURFACE_SETTINGS)
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

/* 箱の中で「操作の行」と「ヘルプの行」が分け合う帯。上は欄の下、下はヘルプが 0 行のときの
 * 札の上端（#69）。札・間・状態の 3 つは下端に固定で、ヘルプはこの帯の下から積む。 */
static RECT command_body_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT body = {bounds.left, command_input_rect(self).bottom + scale(base_command_gap, dpi),
                 bounds.right,
                 bounds.bottom - scale(base_command_status_height * 2 + base_command_gap, dpi)};
    if (body.bottom < body.top)
    {
        body.bottom = body.top;
    }
    return body;
}

/* いま出せるヘルプの行数。操作の一覧に command_row_floor 行を残し、余りをヘルプへ回す。
 * 最小寸法（560×360 と 840×540）では操作 3 行・ヘルプ 6 行になる（#69）。 */
static int command_visible_help(const struct folio_window *_Nonnull self)
{
    int wanted = command_help_rows(self);
    if (wanted == 0)
    {
        return 0;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    RECT body = command_body_rect(self);
    int room = body.bottom - body.top - scale(base_command_row_height, dpi) * command_row_floor;
    int fits = room < 0 ? 0 : room / scale(base_command_help_row_height, dpi);
    return fits < wanted ? fits : wanted;
}

/* 送り出すヘルプの先頭の行。窓が広がって全部入るようになったら先頭へ戻る（#69）。 */
static size_t command_help_offset(const struct folio_window *_Nonnull self)
{
    size_t visible = (size_t)command_visible_help(self);
    size_t total = (size_t)command_help_rows(self);
    if (visible == 0 || visible >= total)
    {
        return 0;
    }
    size_t last = total - visible;
    return self->command_help_first > last ? last : self->command_help_first;
}

static RECT command_toggle_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    UINT dpi = GetDpiForWindow(self->handle);
    int keys = scale(base_command_help_row_height, dpi) * command_visible_help(self);
    int inset = scale(base_command_palette_padding, dpi);
    int top = command_body_rect(self).bottom - keys;
    return (RECT){inset, top, bounds.right - inset, top + scale(base_command_status_height, dpi)};
}

static RECT command_rows_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_input_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    bounds.top = command_body_rect(self).top;
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
    if (self->command_surface == COMMAND_SURFACE_PALETTE ||
        self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        self->command_first = 0;
        reveal_command_selection(self);
    }
    arrange_replace_inputs(self);
    /* 置換は 2 つの欄を使い、設定は欄を持たないのでどちらも隠す。 */
    bool hidden = self->command_surface == COMMAND_SURFACE_REPLACE ||
                  self->command_surface == COMMAND_SURFACE_SETTINGS;
    ShowWindow(self->command_input, hidden ? SW_HIDE : SW_SHOW);
    ShowWindow(self->command_layer, SW_SHOW);
    BringWindowToTop(self->command_layer);
}

/* 常設の欄はドロワーの頭の帯に重ねる。寸法はドロワーが答える（ADR 0024 の決定 6）。
 * 右端は「×」のぶんだけ常に空ける。出るのは絞り込み中だけだが、余白を出し入れすると
 * 打っている途中で本文が動くので、いつも同じ幅にしておく（補正 6）。 */
static void arrange_filter_input(struct folio_window *_Nonnull self)
{
    if (self->filter_input == nullptr || self->drawer == nullptr)
    {
        return;
    }
    RECT bounds = drawer_window_filter_rect(self->drawer);
    MoveWindow(self->filter_input, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
    int room = scale(base_filter_clear_size, GetDpiForWindow(self->handle));
    SendMessageW(self->filter_input, EM_SETMARGINS, EC_RIGHTMARGIN, MAKELPARAM(0, room));
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

/* UTF-8 の 1 行を測る。測れなければ 0。 */
static int measure_utf8(HDC device, const char *_Nonnull text)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(text, units);
    if (count == 0)
    {
        return 0;
    }
    RECT measured = {0, 0, 0, 0};
    DrawTextW(device, units, count, &measured, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    return measured.right - measured.left;
}

/* UTF-8 を 1 行で描く。bounds に収まらなければ末尾を省略記号にする（ADR 0011 の決定 4）。 */
static void draw_utf8(HDC device, const char *_Nonnull text, RECT bounds)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(text, units);
    if (count == 0)
    {
        return;
    }
    DrawTextW(device, units, count, &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
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

/* 閉じるアイコンの左隣に置く設定の歯車（ADR 0031 の決定 8(a)）。
 * 描画・当たり判定・札の右端の三つがこの 1 本を共有する。 */
static RECT settings_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = close_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int shift = scale(base_close_size + base_settings_gap, dpi);
    OffsetRect(&bounds, -shift, 0);
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

static enum ui_text chip_label(enum pane_mode chip)
{
    switch (chip)
    {
    case PANE_MODE_VIEW:
        return UI_TEXT_CHIP_VIEW;
    case PANE_MODE_EDIT:
        return UI_TEXT_CHIP_EDIT;
    }
    return UI_TEXT_CHIP_VIEW;
}

/* 札に描く 1 行。札の幅も当たり判定もこの 1 本を通る。 */
static const char *_Nonnull chip_text(const struct folio_window *_Nonnull self, enum pane_mode chip)
{
    return ui_text_line(chip_label(chip), folio_state_language(self->state));
}

/* 札 1 つの幅（左右の余白込み）。等幅フォントを選んだ device で測る。 */
static int chip_width(const struct folio_window *_Nonnull self, HDC device, enum pane_mode chip)
{
    return measure_utf8(device, chip_text(self, chip)) +
           scale(base_chip_padding, GetDpiForWindow(self->handle)) * 2;
}

/* 札の矩形。右端が「編集」、その左が「閲覧」。描画と当たり判定はこの 1 か所を共有する。 */
static RECT chip_rect(const struct folio_window *_Nonnull self, HDC device, enum pane_mode chip)
{
    RECT caption = caption_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int height = scale(base_chip_height, dpi);
    int middle = (caption.top + caption.bottom) / 2;
    int right = settings_rect(self).left - scale(base_chip_gap, dpi);
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
static const char *_Nonnull breadcrumb_note(struct pane_title_view title,
                                            enum folio_language language)
{
    return title.recovering ? ui_text_line(UI_TEXT_TITLE_RECOVERING, language) : title.note;
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
    int note = measure_utf8(device, breadcrumb_note(title, folio_state_language(self->state)));
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
    draw_breadcrumb_label(device, breadcrumb_note(title, folio_state_language(self->state)),
                          cells.note, cells.padding + cells.tip);
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
    char16_t units[draw_unit_limit];
    int count = wide_units(chip_text(self, chip), units);
    DrawTextW(device, units, count, &bounds, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_NOPREFIX);
}

/* 「×」は 24 の viewBox の面のパス（ADR 0033 の決定 6）。箱は close_rect のまま。 */
static void draw_close(const struct folio_window *_Nonnull self, HDC device)
{
    icon_paint_fill(device, close_rect(self), ICON_PAINT_CLOSE, self->palette.header_text);
}

/* 歯車も同じ 1 本で塗る（字形は使わない・CNF-010）。箱は settings_rect のまま。 */
static void draw_settings_icon(const struct folio_window *_Nonnull self, HDC device)
{
    icon_paint_fill(device, settings_rect(self), ICON_PAINT_SETTINGS, self->palette.header_text);
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

/* いまの向きを欄に示す（`/` で開けば次へ、`?` で開けば前へ・ADR 0023 の決定 4）。 */
static enum ui_text search_direction_label(const struct folio_window *_Nonnull self)
{
    switch (folio_state_search_direction(self->state))
    {
    case SEARCH_DIRECTION_FORWARD:
        return UI_TEXT_STATUS_SEARCH_FORWARD;
    case SEARCH_DIRECTION_BACKWARD:
        return UI_TEXT_STATUS_SEARCH_BACKWARD;
    }
    return UI_TEXT_STATUS_SEARCH_FORWARD;
}

/* 向きの札のうしろに並べる句。語がまだ無いあいだは何も添えない。 */
static enum ui_text search_result_line(enum note_search_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_SEARCH_NO_TERM:
        return UI_TEXT_EMPTY;
    case NOTE_SEARCH_MALFORMED:
        return UI_TEXT_STATUS_SEARCH_MALFORMED;
    case NOTE_SEARCH_BAD_SPAN:
        return UI_TEXT_STATUS_SEARCH_BAD_SPAN;
    case NOTE_SEARCH_NOT_FOUND:
        return UI_TEXT_STATUS_SEARCH_NOT_FOUND;
    case NOTE_SEARCH_FOUND:
        return UI_TEXT_STATUS_SEARCH_FOUND;
    }
    return UI_TEXT_EMPTY;
}

/* 向きと「k / n 件」。0 件も壊れた語も同じ場所に出し、選択は変えない。
 * 句どうしは並置し、句の中の数は置換子で埋める（ADR 0030 の決定 3）。 */
static void search_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    size_t at = append_text(out, 0, ui_text_line(search_direction_label(self), language));
    struct ui_text_request request = {.id = search_result_line(self->search_outcome),
                                      .language = language,
                                      .k = self->search_ordinal,
                                      .n = self->search_total};
    if (ui_text_format(&request, out + at, search_status_capacity - at) != UI_TEXT_FORMAT_READY)
    {
        /* 表の値は単体で収まりが固定されるので、ここへは来ない。来たら札だけを残す。 */
        out[at] = '\0';
    }
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
    enum folio_language language = folio_state_language(self->state);
    draw_utf8(device, ui_text_line(UI_TEXT_ACTION_SEARCH_PREVIOUS, language),
              surface_button_rect(self, 0));
    draw_utf8(device, ui_text_line(UI_TEXT_ACTION_SEARCH_NEXT, language),
              surface_button_rect(self, 1));
}

/* 失敗の 1 行。REPLACE_BAD_PATTERN のときだけ末尾に位置を添える（ADR 0028 の決定 9）。
 * out は replace_status_capacity の入れ物（呼び出し側が持つ大きさ）。 */
static void failure_status(const struct folio_window *_Nonnull self,
                           enum folio_state_outcome outcome, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    size_t at = append_text(out, 0, folio_state_failure_line(outcome, language));
    out[at] = '\0';
    size_t offset = folio_state_replace_error_offset(self->state);
    if (outcome != FOLIO_STATE_REPLACE_BAD_PATTERN || offset == 0)
    {
        return;
    }
    struct ui_text_request request = {
        .id = UI_TEXT_STATUS_REPLACE_POSITION, .language = language, .offset = offset};
    if (ui_text_format(&request, out + at, replace_status_capacity - at) != UI_TEXT_FORMAT_READY)
    {
        out[at] = '\0';
    }
}

/* 置換の対象名と件数。対象名はパンくずと同じ実名で、無題は「無題（未保存）」になる。 */
static void replace_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    struct pane_title_view title = folio_state_pane_title(self->state);
    struct ui_text_request request = {.id = UI_TEXT_STATUS_REPLACE_COUNT,
                                      .language = folio_state_language(self->state),
                                      .k = folio_state_replace_count(self->state),
                                      .name = title.any ? title.note : nullptr};
    if (ui_text_format(&request, out, replace_status_capacity) != UI_TEXT_FORMAT_READY)
    {
        /* ノート名が長すぎて収まらない。現行どおり何も出さない（決定 3）。 */
        out[0] = '\0';
    }
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
    enum folio_language language = folio_state_language(self->state);
    draw_utf8(device, ui_text_line(UI_TEXT_ACTION_REPLACE_ONE, language),
              surface_button_rect(self, 0));
    draw_utf8(device, ui_text_line(UI_TEXT_ACTION_REPLACE_ALL, language),
              surface_button_rect(self, 1));
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
        line = ui_text_line(UI_TEXT_STATUS_COMMAND_NOT_FOUND, folio_state_language(self->state));
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
    int rows = command_visible_help(self);
    if (rows == 0)
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    int row_height = scale(base_command_help_row_height, dpi);
    size_t first = command_help_offset(self);
    bounds.bottom -= scale(base_command_status_height + base_command_gap, dpi);
    bounds.top = bounds.bottom - row_height * rows;
    bounds.left += scale(base_command_palette_padding, dpi);
    bounds.right -= scale(base_command_palette_padding, dpi);
    SetTextColor(device, self->palette.current_text);
    enum folio_language language = folio_state_language(self->state);
    for (int index = 0; index < rows; ++index)
    {
        RECT row = {bounds.left, bounds.top, bounds.right, bounds.top + row_height};
        draw_utf8(device, ui_text_line(command_shortcuts[first + (size_t)index], language), row);
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

/* 札の右、「↑ 前」の左に置くページの印の場所（#69）。 */
static RECT command_pages_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_toggle_rect(self);
    bounds.right = command_previous_rect(self).left;
    bounds.left = bounds.right - scale(base_command_pages_width, GetDpiForWindow(self->handle));
    return bounds;
}

/* ヘルプが 1 枚に入らないときだけ「n/m」を出す。数字と `/` だけなので翻訳する文言は増えない。 */
static void draw_command_pages(const struct folio_window *_Nonnull self, HDC device)
{
    size_t visible = (size_t)command_visible_help(self);
    size_t total = (size_t)command_help_rows(self);
    if (visible == 0 || visible >= total)
    {
        return;
    }
    char line[command_pages_capacity];
    struct ui_text_request request = {.id = UI_TEXT_STATUS_PALETTE_PAGES,
                                      .language = folio_state_language(self->state),
                                      .k = command_help_offset(self) / visible + 1,
                                      .n = (total + visible - 1) / visible};
    if (ui_text_format(&request, line, command_pages_capacity) != UI_TEXT_FORMAT_READY)
    {
        return;
    }
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, line, command_pages_rect(self));
}

static void draw_command_guidance(const struct folio_window *_Nonnull self, HDC device)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT line = command_input_rect(self);
    int row = scale(base_command_help_row_height, dpi);
    line.bottom = line.top - row;
    line.top -= row * 2;
    enum folio_language language = folio_state_language(self->state);
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, ui_text_line(UI_TEXT_STATUS_PALETTE_CLICK, language), line);
    OffsetRect(&line, 0, row);
    draw_utf8(device, ui_text_line(UI_TEXT_STATUS_PALETTE_FILTER, language), line);
    RECT toggle = command_toggle_rect(self);
    toggle.right = command_pages_rect(self).left;
    SetTextColor(device, self->palette.selected_text);
    draw_utf8(device,
              ui_text_line(self->command_keys_visible ? UI_TEXT_ACTION_KEYS_HIDE
                                                      : UI_TEXT_ACTION_KEYS_SHOW,
                           language),
              toggle);
    draw_utf8(device, ui_text_line(UI_TEXT_ACTION_PALETTE_PREVIOUS, language),
              command_previous_rect(self));
    draw_utf8(device, ui_text_line(UI_TEXT_ACTION_PALETTE_NEXT, language), command_next_rect(self));
    draw_command_pages(self, device);
}

/* 設定画面の案内 2 行（ADR 0031 の決定 7）。1 行目は操作の説明、
 * 2 行目は設定を読めないセッションだけその理由（開いた瞬間から出す）。 */
static void draw_settings_guidance(const struct folio_window *_Nonnull self, HDC device)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT line = command_input_rect(self);
    int row = scale(base_command_help_row_height, dpi);
    line.bottom = line.top - row;
    line.top -= row * 2;
    enum folio_language language = folio_state_language(self->state);
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, ui_text_line(UI_TEXT_SETTINGS_GUIDANCE, language), line);
    enum folio_state_outcome notice = folio_state_settings_notice(self->state);
    if (notice != FOLIO_STATE_READY)
    {
        OffsetRect(&line, 0, row);
        SetTextColor(device, self->palette.header_text);
        draw_utf8(device, folio_state_failure_line(notice, language), line);
    }
}

/* 現在の選択の行の左に出す角丸の印（字形は使わない・CNF-010・ADR 0033 の決定 6）。 */
static void draw_settings_mark(const struct folio_window *_Nonnull self, HDC device, RECT row)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int box = scale(base_settings_mark_box, dpi);
    int left = row.left + scale(base_settings_mark_inset, dpi);
    int middle = (row.top + row.bottom) / 2;
    RECT bounds = {left, middle - box / 2, left + box, middle - box / 2 + box};
    icon_paint_fill(device, bounds, ICON_PAINT_SELECTION, self->palette.chip_background);
}

/* その行がいまの値か（印を付ける行）。段を足したら枝を足させる（C-002）。 */
static bool settings_row_current(const struct folio_window *_Nonnull self, size_t index)
{
    switch (settings_rows[index].kind)
    {
    case SETTINGS_ROW_HEADING:
        return false;
    case SETTINGS_ROW_THEME:
        return (unsigned char)folio_state_theme_choice(self->state) == settings_rows[index].value;
    case SETTINGS_ROW_LANGUAGE:
        return (unsigned char)folio_state_language(self->state) == settings_rows[index].value;
    }
    return false;
}

/* 1 行分を描く。見出しは薄い文字で印もカーソルも付かない。 */
static void draw_settings_row(const struct folio_window *_Nonnull self, HDC device, RECT row,
                              size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    bool choice = settings_row_choice(index);
    bool selected = choice && index == self->command_selection;
    if (selected)
    {
        HBRUSH brush = CreateSolidBrush(self->palette.selected_background);
        FillRect(device, &row, brush);
        DeleteObject(brush);
    }
    RECT label = row;
    label.left += scale(base_command_row_padding, dpi);
    label.right -= scale(base_command_row_padding, dpi);
    if (choice)
    {
        label.left += scale(base_settings_indent, dpi);
        if (settings_row_current(self, index))
        {
            draw_settings_mark(self, device, row);
        }
    }
    SetTextColor(device, choice
                             ? (selected ? self->palette.selected_text : self->palette.current_text)
                             : self->palette.header_text);
    draw_utf8(device, ui_text_line(settings_rows[index].label, folio_state_language(self->state)),
              label);
}

static void draw_settings_rows(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT rows = command_rows_rect(self);
    int height = scale(base_command_row_height, dpi);
    size_t count = command_visible_rows(self);
    size_t total = settings_row_count();
    for (size_t visible = 0; visible < count && self->command_first + visible < total; ++visible)
    {
        size_t index = self->command_first + visible;
        RECT row = {rows.left, rows.top + (int)visible * height, rows.right,
                    rows.top + (int)(visible + 1) * height};
        draw_settings_row(self, device, row, index);
    }
    int padding = scale(base_command_palette_padding, dpi);
    RECT status = {bounds.left + padding, bounds.bottom - scale(base_command_status_height, dpi),
                   bounds.right - padding, bounds.bottom};
    draw_command_status(self, device, status);
    draw_settings_guidance(self, device);
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
    case COMMAND_SURFACE_SETTINGS:
        FillRect(device, &bounds, self->command_brush);
        draw_settings_rows(self, device, bounds);
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
    RECT settings = settings_rect(self);
    if (PtInRect(&close, client) || PtInRect(&settings, client))
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

/* 頭の釦 index 番に描く 1 行。釦の幅の実測も描画もこの 1 本を通る（#122）。 */
static const char *_Nonnull action_button_label(const struct folio_window *_Nonnull self,
                                                size_t index)
{
    enum folio_language language = folio_state_language(self->state);
    if (index == 2)
    {
        return ui_text_line(UI_TEXT_ACTION_OPERATIONS, language);
    }
    static const enum folio_command commands[] = {
        [0] = FOLIO_COMMAND_NEW,
        [1] = FOLIO_COMMAND_SAVE,
        [3] = FOLIO_COMMAND_HELP,
        [4] = FOLIO_COMMAND_FIND,
    };
    return folio_command_label(commands[index], language);
}

/* 頭の釦の矩形。描画・当たり判定・「操作 ▾」のメニューの位置がこの 1 本を共有する。
 * 幅は現在の言語の字面を頭と同じ等幅フォントと字間で測った幅 ＋ 字下げ ＋ 右の余白で、
 * 日本語の画面案の幅（floors）を下限にする。字がそれより広い言語だけ釦が広がり、
 * 右の釦はその分だけ右へずれる（#122。English の「Actions ▾」が省略されない）。 */
static RECT action_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    const int floors[] = {104, 52, 52, 70, 160};
    int left = scale(base_drawer_width + 8, dpi);
    int top = scale(base_caption_height + 4, dpi);
    HDC device = GetDC(self->handle);
    HGDIOBJ previous = nullptr;
    if (device != nullptr)
    {
        previous = SelectObject(device, self->mono_font);
        SetTextCharacterExtra(device, scale(base_tracking, dpi));
    }
    int width = 0;
    for (size_t at = 0; at <= index; ++at)
    {
        left += at == 0 ? 0 : width + scale(base_action_gap, dpi);
        int measured = device == nullptr ? 0 : measure_utf8(device, action_button_label(self, at));
        int needed = scale(base_action_inset, dpi) + measured + scale(base_action_trail, dpi);
        width = needed > scale(floors[at], dpi) ? needed : scale(floors[at], dpi);
    }
    if (device != nullptr)
    {
        SetTextCharacterExtra(device, 0);
        SelectObject(device, previous);
        ReleaseDC(self->handle, device);
    }
    return (RECT){left, top, left + width, top + scale(28, dpi)};
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
    bounds.left += scale(base_action_inset, GetDpiForWindow(self->handle));
    draw_utf8(device, label, bounds);
}

static void draw_actions(const struct folio_window *_Nonnull self, HDC device)
{
    for (size_t index = 0; index < 4; ++index)
    {
        draw_action_button(self, device, action_button_rect(self, index),
                           action_button_label(self, index));
    }
    if (action_button_fits(self, 4))
    {
        draw_action_button(self, device, action_button_rect(self, 4), action_button_label(self, 4));
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
    draw_settings_icon(self, device);
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
    /* 設定の失敗は欄の中の 1 行（ADR 0031 の決定 7）。設定画面は閉じずに理由を出す。
     * `:set number` の失敗も同じ値なので、他の Ex の失敗と同じく欄の 1 行になる。 */
    case FOLIO_STATE_SETTINGS_UNREADABLE:
    case FOLIO_STATE_SETTINGS_STORE_FAILED:
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
        failure_box_show(self->handle, outcome, self->state);
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
        failure_box_show(self->handle, taken, self->state);
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
    self->command_help_first = 0;
    clear_command_status(self);
    SetWindowTextW(self->command_input, surface == COMMAND_SURFACE_EX ? L":" : L"");
    arrange_command_input(self);
    focus_command_input(self);
    /* WM_SETTEXT はキャレットを先頭へ置き、EM_SETSEL(-1, -1) は選択を解くだけで動かさない。
     * 打った字が `:` の前に入らないよう、末尾（Ex は 1・他の面は 0）へ明示的に置く（#85）。 */
    LRESULT caret = (LRESULT)GetWindowTextLengthW(self->command_input);
    SendMessageW(self->command_input, EM_SETSEL, (WPARAM)caret, (LPARAM)caret);
    redraw_command_layer(self);
}

static void hide_command_surface(struct folio_window *_Nonnull self)
{
    self->command_surface = COMMAND_SURFACE_CLOSED;
    self->command_return_focus = nullptr;
    self->command_selection = 0;
    self->command_help_first = 0;
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
    self->command_help_first = 0;
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

/* 開いたときにカーソルを置く行（いまのテーマの行）。表を引くので添字を鋳込まない。 */
static size_t settings_initial_row(const struct folio_window *_Nonnull self)
{
    for (size_t index = 0; index < settings_row_count(); ++index)
    {
        if (settings_rows[index].kind == SETTINGS_ROW_THEME && settings_row_current(self, index))
        {
            return index;
        }
    }
    return settings_first_choice_row;
}

/* 設定画面は EDIT を持たず、レイヤー自身がフォーカスを取る（ADR 0031 の決定 7）。
 * 開いたとき現在の選択の行にカーソルを置く（show_search_surface と同じ形の上書き）。
 * 閲覧中でも文書が無くても開ける。 */
static void show_settings_surface(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_SETTINGS);
    }
    else
    {
        self->command_surface = COMMAND_SURFACE_SETTINGS;
        clear_command_status(self);
    }
    /* **行を決めてから配置する**（show_command_palette と同じ順）。逆にすると
     * arrange_command_input の reveal_command_selection がパレットの添字のまま走り、
     * command_first が行数を超えて 1 行も描かれなくなる。 */
    self->command_selection = settings_initial_row(self);
    arrange_command_input(self);
    focus_command_input(self);
    redraw_command_layer(self);
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
        return name_prompt_show(self->handle, &request, &self->palette);
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
        return name_prompt_show(self->handle, &request, &self->palette);
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

/* 閲覧の本文を新しい RTF で流し直し、選択を戻す（ADR 0031 の決定 6 の補正 1）。
 * **実測の補正**: EM_EXSETSEL は EM_SCROLLCARET を送らなくても選択を見える位置へ寄せる。
 * 空の選択を戻すと先頭へ飛ぶので、選んでいる一致があるときだけ戻す（ADR 0023 の
 * ハイライトは保ち、選んでいないときは流し直しが保ったスクロール位置をそのまま残す）。 */
static void restream_pane(const struct folio_window *_Nonnull self)
{
    size_t start = 0;
    size_t end = 0;
    bool had = note_pane_selection(self->pane, &start, &end);
    render_pane(self);
    if (had && end > start)
    {
        note_pane_restore_selection(self->pane, (struct note_search_span){start, end});
    }
}

/* 色を持つものを全部当て直す（ADR 0031 の決定 6）。palette の写しは主窓・ドロワー・
 * command_brush の 3 か所で、縁の色は decorate を呼び直す（何度でも可・実測）。
 * 閲覧は選択を退避して流し直し、編集は色を当て直すだけ（本文・Undo・変更印を守る）。
 * **最初に見える論理行を退避して戻す**（ADR 0031 の 2026-09-23 の補正 2）。
 * キャレットが画面の外にあると EM_SETCHARFORMAT がキャレットまでスクロールするため。 */
static void recolor_pane(const struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return;
    }
    enum pane_mode mode = folio_state_pane_mode(self->state);
    size_t line = note_pane_first_visible_line(self->pane);
    note_pane_recolor(self->pane, self->palette.pane, self->palette.editor_text, mode);
    if (mode == PANE_MODE_VIEW)
    {
        restream_pane(self);
    }
    if (line > 0)
    {
        note_pane_scroll_to_line(self->pane, line);
    }
}

/* 言語の切り替えで本文の書体を当て直す（ADR 0032 の決定 6(c)）。
 * 編集は既定書式の face だけを当て、閲覧は新しい fonttbl の RTF を流し直す（順が違う）。
 * face は再折り返しを起こすので、最初に見える論理行を退避して最後に戻す。 */
static void reface_pane(const struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return;
    }
    size_t line = note_pane_first_visible_line(self->pane);
    /* **既定書式の face はモードによらず当てる。** `SCF_DEFAULT` は既存の run を塗らないので、
     * RTF が face を明示している閲覧の見た目は変わらない。当てないと、起動直後（閲覧）や
     * 閲覧のまま言語を変えたあとに `i` で編集へ入った本文が RichEdit の既定 face になる
     * （平文の流し込みは既定書式で描かれる）。`note_pane_recolor` が閲覧でも `SCF_DEFAULT` を
     * 無条件に当てているのと同じ理屈である（ADR 0031 の補正 9・ADR 0032 の補正 4）。 */
    wchar_t face[LF_FACESIZE];
    ui_face_for(folio_state_language(self->state), face);
    note_pane_reface(self->pane, face);
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        restream_pane(self);
    }
    if (line > 0)
    {
        note_pane_scroll_to_line(self->pane, line);
    }
}

static void recolor_window(struct folio_window *_Nonnull self)
{
    self->palette = folio_palette_for(folio_state_theme(self->state));
    HBRUSH _Nullable brush = CreateSolidBrush(self->palette.window);
    if (brush != nullptr)
    {
        if (self->command_brush != nullptr)
        {
            DeleteObject(self->command_brush);
        }
        self->command_brush = brush;
    }
    decorate(self->handle, self->palette);
    if (self->drawer != nullptr)
    {
        drawer_window_recolor(self->drawer);
    }
    recolor_pane(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    /* 主窓は WS_CLIPCHILDREN なので、上の無効化は子の EDIT に届かない（#121）。 */
    if (self->filter_input != nullptr)
    {
        InvalidateRect(self->filter_input, nullptr, TRUE);
    }
    redraw_command_layer(self);
}

/* 2 つの入口（設定画面の Enter と `:set theme=`）が通る唯一の意図（ADR 0031 の決定 3）。
 * 設定画面は採用しても閉じず、印を移したまま残る（決定 7）。 */
static void apply_theme(struct folio_window *_Nonnull self, enum folio_theme_choice choice)
{
    enum folio_theme before = folio_state_theme(self->state);
    enum folio_state_outcome outcome = folio_state_set_theme(self->state, choice);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        clear_command_status(self);
    }
    else
    {
        hide_command_surface(self);
    }
    if (folio_state_theme(self->state) != before)
    {
        recolor_window(self);
        return;
    }
    redraw_command_layer(self);
}

/* 言語を採り直したあとの UI（ADR 0032 の決定 6）。文言は描くたびに表を引き直すので
 * 全面を描き直すだけでよく、face を持つ部品（ドロワーの書体・本文の既定書式）だけを
 * 当て直す。メニューは開くたびに作るので触らない。 */
static void relanguage_window(struct folio_window *_Nonnull self)
{
    if (self->drawer != nullptr)
    {
        drawer_window_refont(self->drawer);
    }
    reface_pane(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    redraw_command_layer(self);
}

/* 2 つの入口（設定画面の Enter と `:set language=`）が通る唯一の意図（ADR 0032 の決定 5）。
 * 設定画面は採用しても閉じず、印を移したまま残る（apply_theme と同じ流儀）。 */
static void apply_language(struct folio_window *_Nonnull self, enum folio_language language)
{
    enum folio_language before = folio_state_language(self->state);
    enum folio_state_outcome outcome = folio_state_set_language(self->state, language);
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        clear_command_status(self);
    }
    else
    {
        hide_command_surface(self);
    }
    if (folio_state_language(self->state) != before)
    {
        relanguage_window(self);
        return;
    }
    redraw_command_layer(self);
}

/* 未知の語・語なし・余計な語は、未知のコマンドと同じ 1 行で、入力を消さない（決定 8）。
 * 語は閉じた集合なので、行番号とテーマの 2 つの意図へここで振り分ける（ADR 0031 の決定 8(c)）。 */
static void execute_set_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    enum folio_option option = FOLIO_OPTION_NUMBER_SHOW;
    if (!folio_command_parse_option(argument, strlen(argument), &option))
    {
        command_not_found(self);
        return;
    }
    switch (option)
    {
    case FOLIO_OPTION_NUMBER_SHOW:
        apply_number(self, true);
        return;
    case FOLIO_OPTION_NUMBER_HIDE:
        apply_number(self, false);
        return;
    case FOLIO_OPTION_NUMBER_TOGGLE:
        apply_number(self, !folio_state_number(self->state));
        return;
    case FOLIO_OPTION_THEME_SYSTEM:
        apply_theme(self, FOLIO_THEME_CHOICE_SYSTEM);
        return;
    case FOLIO_OPTION_THEME_LIGHT:
        apply_theme(self, FOLIO_THEME_CHOICE_LIGHT);
        return;
    case FOLIO_OPTION_THEME_DARK:
        apply_theme(self, FOLIO_THEME_CHOICE_DARK);
        return;
    case FOLIO_OPTION_LANGUAGE_JA:
        apply_language(self, FOLIO_LANGUAGE_JA);
        return;
    case FOLIO_OPTION_LANGUAGE_EN:
        apply_language(self, FOLIO_LANGUAGE_EN);
        return;
    case FOLIO_OPTION_LANGUAGE_ZH_HANS:
        apply_language(self, FOLIO_LANGUAGE_ZH_HANS);
        return;
    }
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

/* `:e!`（ADR 0016 の補正 4）。前提は application が判定し、戻す本文はノート切替と同じ
 * show_note の経路で流し込む。保存は試みず、ディスクも読み直さない。 */
static void execute_discard_command(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = folio_state_discard_edits(self->state);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = show_note(self);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        command_failure(self, outcome);
        return;
    }
    hide_command_surface(self);
    focus_pane(self);
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
    case FOLIO_COMMAND_SETTINGS:
        show_settings_surface(self);
        return;
    case FOLIO_COMMAND_DISCARD_EDITS:
        execute_discard_command(self);
        return;
    }
}

/* Ctrl+S と本文の Esc。保存して編集モードのまま残る。失敗なら 1 行を出して false。 */
static bool store_edit(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = store_body(self);
    if (outcome != FOLIO_STATE_READY)
    {
        failure_box_show(self->handle, outcome, self->state);
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
        failure_box_show(self->handle, outcome, self->state);
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
        failure_box_show(self->handle, outcome, self->state);
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
    self->command_help_first = 0;
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
    case COMMAND_SURFACE_SETTINGS:
        return false;
    }
    return false;
}

/* カーソルの行を採用する（ADR 0031 の決定 7・ADR 0032 の決定 7）。
 * 種別の閉じた switch なので、段を足したらここが落ちる。見出しの行では何も起きない。 */
static void adopt_settings_row(struct folio_window *_Nonnull self)
{
    size_t index = self->command_selection;
    if (index >= settings_row_count())
    {
        return;
    }
    switch (settings_rows[index].kind)
    {
    case SETTINGS_ROW_HEADING:
        return;
    case SETTINGS_ROW_THEME:
        apply_theme(self, (enum folio_theme_choice)settings_rows[index].value);
        return;
    case SETTINGS_ROW_LANGUAGE:
        apply_language(self, (enum folio_language)settings_rows[index].value);
        return;
    }
}

/* クリックはパレットと同じ。選択肢の行を押すとその場で採用する。
 * 隠した EDIT へフォーカスを移さず、レイヤーに残す。 */
static void click_settings_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT rows = command_rows_rect(self);
    if (!PtInRect(&rows, point))
    {
        return;
    }
    int height = scale(base_command_row_height, GetDpiForWindow(self->handle));
    size_t index = self->command_first + (size_t)((point.y - rows.top) / height);
    if (index >= settings_row_count() || !settings_row_choice(index))
    {
        return;
    }
    self->command_selection = index;
    adopt_settings_row(self);
}

static void click_command_surface(struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        click_settings_surface(self, point);
        return;
    }
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
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY, self->state);
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
    char16_t units[draw_unit_limit];
    if (wide_units(folio_command_label(command, language), units) == 0)
    {
        return false;
    }
    return AppendMenuW(menu, MF_STRING, index + 1, units) != 0;
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
    RECT settings = settings_rect(self);
    if (PtInRect(&settings, point))
    {
        execute_command(self, FOLIO_COMMAND_SETTINGS, "");
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
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY, self->state);
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
    /* 設定画面の Enter は command_return が行を採用するので、ここには来ない（ADR 0031 の決定 7）。
     */
    case COMMAND_SURFACE_SETTINGS:
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
        failure_box_show(self->handle, FOLIO_STATE_OUT_OF_MEMORY, self->state);
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

/* ヘルプを 1 枚ぶん送る。巡回はせず、先頭と末尾で止まる（#69）。 */
static void move_command_help(struct folio_window *_Nonnull self, WPARAM key)
{
    size_t visible = (size_t)command_visible_help(self);
    size_t total = (size_t)command_help_rows(self);
    if (visible == 0 || visible >= total)
    {
        return;
    }
    size_t first = command_help_offset(self);
    size_t last = total - visible;
    if (key == VK_PRIOR)
    {
        self->command_help_first = first < visible ? 0 : first - visible;
    }
    else
    {
        self->command_help_first = first + visible > last ? last : first + visible;
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

/* Tab はパレットでは説明の開閉、置換の欄では 2 つの EDIT の往復（ADR 0016 の補正）。 */
static bool command_tab(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        toggle_command_keys(self);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return true; /* 欄が 1 つも無いので行き先が無い。鍵は飲む（パレットと同じく効く） */
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
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        adopt_settings_row(self);
        return true;
    }
    execute_command_input(self);
    return true;
}

/* 見出しを飛ばして次の選択肢の行を返す。端では動かない（ADR 0032 の決定 7）。 */
static size_t settings_next_choice(size_t from, bool downwards)
{
    size_t total = settings_row_count();
    size_t at = from;
    while (downwards ? at + 1 < total : at > settings_first_choice_row)
    {
        at = downwards ? at + 1 : at - 1;
        if (settings_row_choice(at))
        {
            return at;
        }
    }
    return from;
}

/* 設定画面の ↑↓。見出しの行は飛ばし、端では動かない（ADR 0031 の決定 7）。 */
static bool settings_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_UP && key != VK_DOWN)
    {
        return false;
    }
    self->command_selection = settings_next_choice(self->command_selection, key == VK_DOWN);
    /* 行が箱に収まらないので、選択行は必ず見える位置へ送り出す（#69 の式）。 */
    reveal_command_selection(self);
    redraw_command_layer(self);
    return true;
}

/* パレットの移動の鍵。↑↓ は操作の選択、PgUp / PgDn はヘルプのページ（#69）。 */
static bool command_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return settings_navigate(self, key);
    }
    if (self->command_surface != COMMAND_SURFACE_PALETTE)
    {
        return false;
    }
    if (key == VK_UP || key == VK_DOWN)
    {
        move_command_selection(self, key);
        return true;
    }
    if (key == VK_PRIOR || key == VK_NEXT)
    {
        move_command_help(self, key);
        return true;
    }
    return false;
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
    return command_navigate(self, key);
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
    bool listed = self->command_surface == COMMAND_SURFACE_PALETTE ||
                  self->command_surface == COMMAND_SURFACE_SETTINGS;
    if (!listed || self->command_composing)
    {
        return false;
    }
    int delta = GET_WHEEL_DELTA_WPARAM(wparam);
    if (delta == 0)
    {
        return true;
    }
    WPARAM key = delta > 0 ? VK_UP : VK_DOWN;
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return settings_navigate(self, key);
    }
    move_command_selection(self, key);
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
    char16_t units[draw_unit_limit];
    int count = wide_units(
        ui_text_line(UI_TEXT_PLACEHOLDER_FILTER, folio_state_language(self->state)), units);
    DrawTextW(device, units, count, &bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(device, previous);
    ReleaseDC(window, device);
}

/* 絞り込みが効いているあいだ欄の右端に出す「×」の矩形（欄の client 座標）。
 * 幅は EM_SETMARGINS で常に空けてあるので、本文と重ならない（補正 6）。 */
static RECT filter_clear_rect(const struct folio_window *_Nonnull self, HWND window)
{
    RECT client;
    GetClientRect(window, &client);
    int size = scale(base_filter_clear_size, GetDpiForWindow(self->handle));
    int middle = (client.top + client.bottom) / 2;
    RECT bounds = {client.right - size, middle - size / 2, client.right, middle - size / 2 + size};
    return bounds;
}

/* 「×」は頭の draw_close と同じ面のパスで描く（ADR 0033 の決定 3・補正 9）。× を描く経路は 1 本。
 * 箱は filter_clear_rect の 20px のままで、倍率は icon_paint_fill が bounds から出す。
 * 絞り込みが効いていないあいだは出さない（出ていれば押せば解けるという意味になる）。 */
static void paint_filter_clear(const struct folio_window *_Nonnull self, HWND window)
{
    if (!folio_state_filtering(self->state))
    {
        return;
    }
    HDC device = GetDC(window);
    if (device == nullptr)
    {
        return;
    }
    icon_paint_fill(device, filter_clear_rect(self, window), ICON_PAINT_CLOSE,
                    self->palette.header_text);
    ReleaseDC(window, device);
}

/* 「×」を押したら語を空にする。解くのは欄を手で空にするのと同じ経路で、
 * 空にした EN_CHANGE が filter_input_changed を通って絞り込みが解ける（補正 6）。 */
static bool filter_clear_pressed(struct folio_window *_Nonnull self, HWND window, LPARAM lparam)
{
    if (!folio_state_filtering(self->state))
    {
        return false;
    }
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    RECT bounds = filter_clear_rect(self, window);
    if (!PtInRect(&bounds, point))
    {
        return false;
    }
    SetWindowTextW(window, L"");
    SetFocus(window);
    return true;
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
    if (message == WM_LBUTTONDOWN && filter_clear_pressed(self, window, lparam))
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
        paint_filter_clear(self, window);
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
    bool was_filtering = folio_state_filtering(self->state);
    enum folio_state_outcome filtered = folio_state_set_index_filter(
        self->state, (const char16_t *)units, count > 0 ? (size_t)count : 0);
    if (filtered == FOLIO_STATE_OUT_OF_MEMORY)
    {
        command_failure(self, filtered);
        return;
    }
    /* 「×」が出入りするのは効き始めと解けたときだけ。打つたびに欄を消して描き直すと
     * ちらつくので、変わった 1 回だけ欄ごと描き直す（補正 6）。 */
    if (folio_state_filtering(self->state) != was_filtering)
    {
        InvalidateRect(self->filter_input, nullptr, TRUE);
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

/* 常設の絞り込みの欄を主窓の子として作る。
 * **あとから作った子は z 順の後ろ（背面）に入る**ので、作っただけではドロワーの奥にいる。
 * ドロワーの WM_PAINT は client 全域を BitBlt するので、奥にいると索引を描き直すたびに
 * 欄の画素が塗り潰されて語が消える（#86 / ADR 0024 の 2026-09-23 の補正 1・2）。
 * 守るのはドロワー側の `WS_CLIPSIBLINGS` と z 順の両方で、欄に `WS_CLIPSIBLINGS` を
 * 付けても効かない（実測 out/design/2026-09-23/filter-visible-probe/）。 */
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
    SetWindowPos(self->filter_input, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
    /* 既定書式の face は core の表が決める（ADR 0032 の決定 4）。作った直後に 1 回当てる。 */
    reface_pane(self);
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
    /* レイヤーは文字を受けないので IME の文脈を外す（ADR 0031 の決定 7・窓ごとの設定）。
     * 同じプロセスの EDIT の文脈は残る。 */
    self->layer_context = ImmAssociateContext(self->command_layer, nullptr);
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
    /* 設定画面はレイヤーがフォーカスと鍵を受ける（ADR 0031 の決定 7）。
     * 鍵の扱いは EDIT のサブクラスと同じ command_key_down / command_character を共有する。 */
    case WM_SETFOCUS:
        redraw_command_layer(self);
        return 0;
    case WM_KILLFOCUS:
        PostMessageW(self->handle, folio_message_command_focus_lost, 0, 0);
        return 0;
    case WM_KEYDOWN:
        if (command_key_down(self, wparam))
        {
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
    case WM_CHAR:
        if (command_character(self, wparam))
        {
            return 0;
        }
        return DefWindowProcW(window, message, wparam, lparam);
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
    /* 子はまだ生きているので、外した IME の文脈を戻してから手放す。 */
    if (self->command_layer != nullptr && self->layer_context != nullptr)
    {
        ImmAssociateContext(self->command_layer, self->layer_context);
        self->layer_context = nullptr;
    }
    if (self->mono_font != nullptr)
    {
        DeleteObject(self->mono_font);
        self->mono_font = nullptr;
    }
    /* self->handle はここでは消さない。WM_DESTROY の後も WM_NCDESTROY までメッセージは届き、
     * 既定処理はその HWND を要る。手放すのは window_finalized 1 か所（ADR 0014 の補正 1）。 */
    PostQuitMessage(0);
}

/* 窓に届く最後のメッセージ。既定処理には**引数の HWND** を渡し（自分の写しではなく、
 * 消える前の本物を渡す）、そのあとで窓の構造体との結び付けと HWND の写しを同時に手放す。
 * ここが self->handle を消す唯一の場所である（ADR 0014 の補正 1）。 */
static LRESULT window_finalized(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    LRESULT result = DefWindowProcW(window, message, wparam, lparam);
    struct folio_window *_Nullable self = self_of(window);
    if (self != nullptr)
    {
        self->handle = nullptr;
    }
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    return result;
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

/* OS の「アプリのモード」の変更（ADR 0031 の決定 4）。WS_CHILD には届かないので主窓 1 か所。
 * 選択が SYSTEM でなければ解決値は動かないので、前後を比べて変わったときだけ再着色する。 */
static void system_colors_changed(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const wchar_t *_Nullable name = (const wchar_t *)lparam;
    if (name == nullptr || wcscmp(name, immersive_color_set) != 0)
    {
        return;
    }
    enum folio_theme before = folio_state_theme(self->state);
    if (folio_state_refresh_theme(self->state) != FOLIO_STATE_READY)
    {
        return;
    }
    if (folio_state_theme(self->state) != before)
    {
        recolor_window(self);
    }
}

/* 鍵・フォーカス・ホイール・終了。on_message を C-012 の行数に収めるために分けてある。 */
static LRESULT on_input_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    switch (message)
    {
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
    case WM_SETTINGCHANGE:
        system_colors_changed(self, lparam);
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
    /* 常設の絞り込みの欄は主窓の子なので、地と文字の色をここで答える。
     * 入力面の EDIT と同じ color_command_input を通る（第 2 の経路を作らない）。 */
    case WM_CTLCOLOREDIT:
        return color_command_input(self, wparam, lparam);
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
    default:
        return on_input_message(self, message, wparam, lparam);
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
    /* 最後のメッセージも自分で受ける。窓の構造体が無くても外すものは同じなので、ここで答える。 */
    case WM_NCDESTROY:
        return window_finalized(window, message, wparam, lparam);
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

/* GDI+ を 1 回だけ起動する（ADR 0033 の決定 4）。背景スレッドは既定のままにするので
 * GdiplusStartupOutput は渡さない。失敗したら token を 0 のままにして偽を返す。 */
static bool start_gdiplus(struct folio_window *_Nonnull self)
{
    struct gdiplus_startup_input startup = {.version = gdiplus_version_1,
                                            .debug_event_callback = nullptr,
                                            .suppress_background_thread = FALSE,
                                            .suppress_external_codecs = FALSE};
    if (GdiplusStartup(&self->gdiplus_token, &startup, nullptr) != gdiplus_ok)
    {
        self->gdiplus_token = 0;
        return false;
    }
    return true;
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
    /* 窓を作る前に GDI+ を起動する。枠なし窓で × が描けないと閉じる入口が見えなくなる
     * （ADR 0013）ので、起動できなければ窓を作らない（ADR 0033 の決定 4）。 */
    if (!start_gdiplus(self))
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
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
    /* 子窓の WM_NCDESTROY まで済んだあとに終える（ADR 0033 の決定 4）。
     * 生成の途中で失敗した経路では token が 0 のままなので 1 対 1 が保たれる。 */
    if (window->gdiplus_token != 0)
    {
        GdiplusShutdown(window->gdiplus_token);
    }
    free(window);
}
