#include "folio_window.h"
#include "accessibility_host.h"
#include "accessibility_node.h"
#include "accessible_request.h"
#include "accessible_surface_button.h"
#include "accessible_view.h"

#include "breadcrumb_layout.h"
#include "category_name.h"
#include "close_phase.h"
#include "command_band_layout.h"
#include "command_candidates.h"
#include "command_help_metrics.h"
#include "command_row.h"
#include "command_surface_metrics.h"
#include "command_surface_mode.h"
#include "drawer_menu_item.h"
#include "drawer_window.h"
#include "failure_box.h"
#include "folio_command.h"
#include "folio_menu_action.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_regions.h"
#include "folio_shortcut.h"
#include "folio_state.h"
#include "folio_step.h"
#include "folio_theme_choice.h"
#include "gdiplus_flat.h"
#include "history_row_view.h"
#include "icon_paint.h"
#include "layout_metrics.h"
#include "menu_request.h"
#include "modal_owner.h"
#include "name_prompt.h"
#include "name_prompt_metrics.h"
#include "note_name.h"
#include "note_pane.h"
#include "note_ref.h"
#include "note_search.h"
#include "note_text.h"
#include "pane_notice.h"
#include "product_version.h"
#include "replace_edit.h"
#include "session_phase.h"
#include "settings_row_kind.h"
#include "text_draw_request.h"
#include "text_wrap.h"
#include "theme_candidates.h"
#include "theme_row.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_part_metrics.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"

#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <imm.h>
#include <richedit.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <windowsx.h>

constexpr size_t surface_button_count = 8;
constexpr size_t surface_pointer_count = surface_button_count + 2;
constexpr UINT_PTR surface_tooltip_base = 256;
/* 1 回の描画・測定で UTF-16 へ写せる単位数（ADR 0030 の決定 5）。表の 1 行（ui_text_unit_limit）・
 * 状態の 1 行（replace_status_capacity）・ノート名（255 バイト）のどれもここに収まる。
 * 収まらなければ何も描かない（描画のたびの確保をやめる代わりの上限）。 */
constexpr size_t draw_unit_limit = 1024;
/* SDK V2の末尾lParamまで。SDKマクロのNULL参照を実行時に評価しない（ADR0048）。 */
constexpr UINT tooltip_info_size = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM);

struct folio_window
{
    HWND _Nullable handle;
    struct folio_state *_Nonnull state;
    struct accessibility_host *_Nullable accessibility;
    enum accessible_kind keyboard_target;
    bool menu_active;
    struct menu_request menu;
    size_t menu_pressed;
    WPARAM menu_key;
    char result_line[1024];
    bool command_context;
    enum folio_command reporting_command;
    HACCEL _Nullable region_keys;
    HACCEL _Nullable traversal_keys;
    struct folio_palette palette;
    bool appearance_pending;         /* 本文投入/終了処理中のOS通知を一つにまとめる。 */
    bool appearance_recolor_pending; /* 色が同じでも失敗した本文の再着色は再試行する。 */
    struct drawer_window *_Nullable drawer;
    struct note_pane *_Nullable pane;
    HACCEL _Nullable commands;
    /* 検索欄にいるあいだだけ使う Enter / Shift+Enter（ADR 0023 の決定 4） */
    HACCEL _Nullable search_keys;
    HFONT _Nullable mono_font;
    HFONT _Nullable gutter_font; /* mono_fontと本文倍率から作る行番号の派生資源（ADR0067）。 */
    HWND _Nullable command_layer;
    HWND _Nullable tooltip;
    char16_t tooltip_text[draw_unit_limit];
    HWND _Nullable settings_backdrop;
    /* native DialogBoxの暗幕は同じownerが持つ。復帰は現surface/DPIから再導出する。 */
    size_t native_modal_depth;
    struct modal_owner *_Nullable modal_owner;
    struct name_prompt_metrics *_Nullable native_metrics;
    enum close_phase close_phase;
    size_t dispatch_depth;
    bool modal_arrange_failed;
    struct command_band_layout band;
    size_t surface_hover;
    size_t surface_pressed;
    bool surface_pressed_inside;
    bool surface_tracking;
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
    bool filter_saving; /* 保存中のmodalから戻るfocus通知の再入だけを抑止する。 */
    bool root_composing;
    /* 押された部品は 1 つの値で持つ（ACCESSIBLE_WINDOW = 押されていない・ADR0060 の決定 4）。
     * マウスはcapture中に矩形の内外を pressed_inside に写し、Space は離すまで押下の見た目にする。
     */
    enum accessible_kind pressed_part;
    bool pressed_inside;
    bool pressed_by_key;
    enum accessible_kind hover_part; /* ポインタの下の部品。地は塗らず内側の枠で示す。 */
    /* 保存の状態の写し（ADR0060 の決定 8）。application の folio_state_unsaved の答えを、
     * 入力の後の 1 か所（settle_save_state）でだけ取り直す。描画はこの写しだけを読む。 */
    bool unsaved;
    uint64_t store_revision; /* 前回の照合での folio_state_store_revision */
    /* 本文の EN_CHANGE があった（変更の疑い）。名前付きの編集本文はこの印か、世代・文書・mode の
     * 変化があるときだけ写して比べる（矢印など本文を変えない入力では比べない）。 */
    bool body_touched;
    enum folio_document_kind settled_kind;
    enum pane_mode settled_mode;
    bool hover_tracking; /* TrackMouseEvent の WM_MOUSELEAVE を待っている。 */
    /* キーボードで動かした後だけ focus を描く（ADR0060 の決定 3）。押下で下ろす。 */
    bool focus_cues;
    bool focus_cues_always; /* OS の「常にキーボードの印を表示」（SPI_GETKEYBOARDCUES） */
    HWND _Nullable command_return_focus;
    HBRUSH _Nullable command_brush;
    enum command_surface_mode command_surface;
    uint64_t surface_revision; /* 補助面/クエリの表示参照の世代（ADR0058）。 */
    struct command_candidates *_Nullable command_candidates;
    struct theme_candidates *_Nullable theme_candidates;
    size_t command_selection;
    size_t command_first;
    int theme_wheel_remainder; /* 1候補分の刻みに満たないdelta（ADR0061・#471）。 */
    bool command_keys_visible;
    /* ヘルプが箱に入りきらないときに送り出す先頭の行（PgUp / PgDn・#69）。 */
    size_t command_help_scroll;
    bool command_composing;
    bool command_unknown;
    enum pane_notice notice;
    struct command_surface_metrics settings_metrics;
    struct command_help_metrics help_metrics;
    struct ui_part_metrics part_metrics;
    bool command_initializing; /* 同期の初期WM_SETTEXTを利用者のEN_CHANGEから区別する。 */
    bool removal_running;
    enum session_phase session_phase;
    HWND _Nullable session_return_focus;
    enum ui_text session_reason;
    bool session_reason_owned;
    bool layout_changing; /* 同期WM_SIZEを含む一つの表示切替の間だけ立つ。 */
    int wide_width;       /* DRAWER直前の通常幅（logical px）。 */
    WINDOWPLACEMENT wide_placement;
    bool restore_maximized;
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
static const wchar_t settings_backdrop_class[] = L"NeNeFolioSettingsBackdrop";
static const wchar_t mono_face[] = L"Consolas";
static const wchar_t edit_class[] = L"EDIT";
/* 一覧に出すキー操作の行（ADR 0030 の決定 4）。文言は core の ui_text が持ち、
 * ここは並びだけを持つ。1 行 1 ID で、欄分けは単位 C（ADR 0032）。 */
static const enum ui_text command_shortcuts[] = {
    UI_TEXT_HELP_PALETTE,       UI_TEXT_HELP_EDITOR_MOTION,   UI_TEXT_HELP_BODY_ZOOM,
    UI_TEXT_HELP_GLOBAL_KEYS,   UI_TEXT_HELP_GLOBAL_FILTER,   UI_TEXT_HELP_FILTER_FIELD,
    UI_TEXT_HELP_FILTER_LIMITS, UI_TEXT_HELP_FILE_KEYS,       UI_TEXT_HELP_INDEX_COMMAND,
    UI_TEXT_HELP_EX_SET_NUMBER, UI_TEXT_HELP_EX_SET_THEME,    UI_TEXT_HELP_EX_SET_LANGUAGE,
    UI_TEXT_HELP_REPLACE_FIELD, UI_TEXT_HELP_EX_SUBSTITUTE,   UI_TEXT_HELP_SEARCH_KEYS,
    UI_TEXT_HELP_SEARCH_STEP,   UI_TEXT_HELP_SEARCH_FIELD,    UI_TEXT_HELP_INDEX_MOTION,
    UI_TEXT_HELP_INDEX_FOLD,    UI_TEXT_HELP_INDEX_SCROLL,    UI_TEXT_HELP_EDITOR_ESCAPE,
    UI_TEXT_HELP_TRASH_NOTE,    UI_TEXT_HELP_DELETE_CATEGORY, UI_TEXT_HELP_RENAME_CATEGORY,
    UI_TEXT_HELP_ACCESSIBILITY, UI_TEXT_HELP_RECYCLE,
};

/* 設定画面の行（ADR 0031 の決定 7・ADR 0032 の決定 7）。見出しと選択肢を**種別で**持ち、
 * value はその種別の列挙の値（THEME なら folio_theme_choice・LANGUAGE なら folio_language・
 * NUMBER なら採用状態の1）。group/positionは表示上の配置で、値enumから独立する。
 * 段を足すときはここに行を足し、採用の閉じた switch に枝を足す（添字を型へ鋳込まない）。 */
static const struct
{
    enum ui_text label;
    enum settings_row_kind kind;
    unsigned char value;
    unsigned char group;
    unsigned char position;
} settings_rows[] = {
    {UI_TEXT_SETTINGS_THEME, SETTINGS_ROW_HEADING, 0, 0, 0},
    {UI_TEXT_THEME_CHOOSE, SETTINGS_ROW_THEME, 0, 0, 0},
    {UI_TEXT_SETTINGS_LANGUAGE, SETTINGS_ROW_HEADING, 0, 1, 0},
    {UI_TEXT_SETTINGS_LANGUAGE_JA, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_JA, 1, 0},
    {UI_TEXT_SETTINGS_LANGUAGE_EN, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_EN, 1, 1},
    {UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_ZH_HANS, 1, 2},
    {UI_TEXT_SETTINGS_NUMBER_CHECK, SETTINGS_ROW_NUMBER, 1, 2, 0},
};
static const struct
{
    enum folio_layout_mode mode;
    enum folio_command command;
    enum accessible_kind kind;
} layout_buttons[] = {
    {FOLIO_LAYOUT_DRAWER, FOLIO_COMMAND_LAYOUT_DRAWER, ACCESSIBLE_LAYOUT_DRAWER},
    {FOLIO_LAYOUT_BOTH, FOLIO_COMMAND_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH},
    {FOLIO_LAYOUT_NOTE, FOLIO_COMMAND_LAYOUT_NOTE, ACCESSIBLE_LAYOUT_NOTE},
};

static_assert(sizeof command_shortcuts / sizeof command_shortcuts[0] +
                  sizeof layout_buttons / sizeof layout_buttons[0] <=
              command_help_capacity);

/* 自前操作の親子順・名前・focus可否と、表示名・UIA Name・AcceleratorKey・tooltip の 1 つの元
 * （ADR0060 の決定 5）。key は AcceleratorKey と tooltip に、hint は tooltip だけに出る。
 * native入力欄はこのtreeに複製しない。 */
static const struct
{
    enum accessible_kind parent;
    enum ui_text label;
    bool focusable;
    enum ui_text hint;
} accessible_controls[] = {
    [ACCESSIBLE_WINDOW] = {ACCESSIBLE_WINDOW, UI_TEXT_APP_LOGO, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_DRAWER] = {ACCESSIBLE_WINDOW, UI_TEXT_TITLE_CATEGORIES, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_HEADER] = {ACCESSIBLE_WINDOW, UI_TEXT_ACCESSIBLE_HEADER, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_NEW_NOTE] = {ACCESSIBLE_HEADER, UI_TEXT_COMMAND_NEW, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_LAYOUT_GROUP] = {ACCESSIBLE_HEADER, UI_TEXT_LAYOUT_GROUP, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_LAYOUT_DRAWER] = {ACCESSIBLE_LAYOUT_GROUP, UI_TEXT_LAYOUT_BUTTON_DRAWER, true,
                                  UI_TEXT_LAYOUT_NARROW_HINT},
    [ACCESSIBLE_LAYOUT_BOTH] = {ACCESSIBLE_LAYOUT_GROUP, UI_TEXT_LAYOUT_BUTTON_BOTH, true,
                                UI_TEXT_EMPTY},
    [ACCESSIBLE_LAYOUT_NOTE] = {ACCESSIBLE_LAYOUT_GROUP, UI_TEXT_LAYOUT_BUTTON_NOTE, true,
                                UI_TEXT_EMPTY},
    [ACCESSIBLE_LAYOUT_CHOOSER] = {ACCESSIBLE_HEADER, UI_TEXT_LAYOUT_GROUP, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_MINIMIZE] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_MINIMIZE, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_MAXIMIZE] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_MAXIMIZE, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_HELP] = {ACCESSIBLE_HEADER, UI_TEXT_COMMAND_HELP, true, UI_TEXT_TOOLTIP_HELP},
    [ACCESSIBLE_SETTINGS] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_SETTINGS, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_CLOSE] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_CLOSE, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_PANE_TOOLS] = {ACCESSIBLE_WINDOW, UI_TEXT_ACCESSIBLE_PANE_TOOLS, false,
                               UI_TEXT_EMPTY},
    [ACCESSIBLE_BREADCRUMB] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_MODE_GROUP] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_ACCESSIBLE_MODE, false,
                               UI_TEXT_EMPTY},
    [ACCESSIBLE_VIEW] = {ACCESSIBLE_MODE_GROUP, UI_TEXT_CHIP_VIEW, true, UI_TEXT_COMMAND_VIEW},
    [ACCESSIBLE_EDIT] = {ACCESSIBLE_MODE_GROUP, UI_TEXT_CHIP_EDIT, true, UI_TEXT_TOOLTIP_EDIT},
    [ACCESSIBLE_ACTION_GROUP] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_ACCESSIBLE_NOTE_ACTIONS, false,
                                 UI_TEXT_EMPTY},
    [ACCESSIBLE_SAVE] = {ACCESSIBLE_ACTION_GROUP, UI_TEXT_COMMAND_SAVE, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_FIND] = {ACCESSIBLE_ACTION_GROUP, UI_TEXT_COMMAND_FIND, true, UI_TEXT_EMPTY},
    /* 名前は「操作」。▾は描く字（UI_TEXT_ACTION_OPERATIONS）だけが持つ表示の記号。 */
    [ACCESSIBLE_MENU] = {ACCESSIBLE_ACTION_GROUP, UI_TEXT_ACCESSIBLE_OPERATIONS, true,
                         UI_TEXT_TOOLTIP_OPERATIONS},
    [ACCESSIBLE_NEW_CATEGORY] = {ACCESSIBLE_DRAWER, UI_TEXT_COMMAND_NEW_CATEGORY, true,
                                 UI_TEXT_EMPTY},
    [ACCESSIBLE_TREE] = {ACCESSIBLE_DRAWER, UI_TEXT_TITLE_CATEGORIES, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_CATEGORY] = {ACCESSIBLE_TREE, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_NOTE] = {ACCESSIBLE_CATEGORY, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_FILTER] = {ACCESSIBLE_WINDOW, UI_TEXT_PLACEHOLDER_FILTER, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_BODY] = {ACCESSIBLE_WINDOW, UI_TEXT_ACCESSIBLE_BODY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_NOTICE] = {ACCESSIBLE_WINDOW, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_GUIDE] = {ACCESSIBLE_WINDOW, UI_TEXT_EMPTY_HEADING, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE] = {ACCESSIBLE_WINDOW, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_TABS] = {ACCESSIBLE_SURFACE, UI_TEXT_COMMAND_HELP, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_TAB] = {ACCESSIBLE_SURFACE_TABS, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_LIST] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_CHOICE] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_LANGUAGE] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_TOGGLE] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_BUTTON] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_TEXT] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_HELP] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_SURFACE_STATUS] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY},
    [ACCESSIBLE_COMMAND_INPUT] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY},
    [ACCESSIBLE_REPLACE_PATTERN] = {ACCESSIBLE_SURFACE, UI_TEXT_ACCESSIBLE_PATTERN, true,
                                    UI_TEXT_EMPTY},
    [ACCESSIBLE_REPLACE_TEXT] = {ACCESSIBLE_SURFACE, UI_TEXT_ACCESSIBLE_REPLACEMENT, true,
                                 UI_TEXT_EMPTY},
};

/* 自前で描く操作部品と、その様式（ADR0060 の決定 1）。hover・押下・tooltipの対象もこの表。 */
static const struct
{
    enum accessible_kind kind;
    enum ui_part_style style;
} main_parts[] = {
    {ACCESSIBLE_NEW_NOTE, UI_PART_STYLE_COMMAND},
    {ACCESSIBLE_LAYOUT_DRAWER, UI_PART_STYLE_CHOICE},
    {ACCESSIBLE_LAYOUT_BOTH, UI_PART_STYLE_CHOICE},
    {ACCESSIBLE_LAYOUT_NOTE, UI_PART_STYLE_CHOICE},
    {ACCESSIBLE_LAYOUT_CHOOSER, UI_PART_STYLE_COMMAND},
    {ACCESSIBLE_HELP, UI_PART_STYLE_ICON},
    {ACCESSIBLE_SETTINGS, UI_PART_STYLE_ICON},
    {ACCESSIBLE_MINIMIZE, UI_PART_STYLE_ICON},
    {ACCESSIBLE_MAXIMIZE, UI_PART_STYLE_ICON},
    {ACCESSIBLE_CLOSE, UI_PART_STYLE_ICON},
    {ACCESSIBLE_VIEW, UI_PART_STYLE_CHOICE},
    {ACCESSIBLE_EDIT, UI_PART_STYLE_CHOICE},
    {ACCESSIBLE_SAVE, UI_PART_STYLE_COMMAND}, /* 変更ありの間だけ B（part_style） */
    {ACCESSIBLE_FIND, UI_PART_STYLE_COMMAND},
    {ACCESSIBLE_MENU, UI_PART_STYLE_COMMAND},
};

static size_t main_part_count(void)
{
    return sizeof main_parts / sizeof main_parts[0];
}

static bool main_part(enum accessible_kind kind)
{
    for (size_t index = 0; index < main_part_count(); ++index)
    {
        if (main_parts[index].kind == kind)
        {
            return true;
        }
    }
    return false;
}

/* 本文操作帯の群 [保存｜このノート内を検索｜操作▾]（ADR0060 の決定 11。添字は action_button_rect
 * と共有）。絵は 5 秒で意味が分かるものだけ。ヘルプはヘッダーの C 様式へ移した（決定 10）。 */
constexpr size_t action_count = 3;
static const struct
{
    enum accessible_kind kind;
    bool has_icon;
    enum icon_paint_kind icon;
} action_parts[action_count] = {
    {ACCESSIBLE_SAVE, true, ICON_PAINT_SAVE},
    {ACCESSIBLE_FIND, true, ICON_PAINT_SEARCH},
    {ACCESSIBLE_MENU, true, ICON_PAINT_MENU},
};

static size_t layout_button_count(void)
{
    return sizeof layout_buttons / sizeof layout_buttons[0];
}

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
constexpr int command_control_id = 1;
constexpr int filter_control_id = 2;
constexpr size_t filter_input_capacity = 128;
/* 絞り込みが効いているあいだ欄の右端に出す「×」（ADR 0024 の 2026-09-22 の補正 6）。
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
/* 96 DPI での寸法（デザイン「案2 堅」）。 */
constexpr int base_dpi = 96;
constexpr int base_width = 960;
constexpr int base_height = 640;
/* 最小の大きさ。ドロワー・番号・パンくず・札 2 つが収まる（ADR 0011 の決定 3）。 */
constexpr int base_min_width = 560;
constexpr int base_min_height = 360;
constexpr int base_drawer_width = 240;
constexpr int base_common_header_height = 44;
constexpr int base_new_note_drag_gap = 24;
constexpr int base_drawer_window_width = 320;
constexpr int base_layout_button_width = 56; /* 表示3択の区画（4 の倍数・ADR0060 の決定 2） */
/* 一覧のみ（最小幅 560 未満）ではヘルプ「?」のぶん区画を 44 に縮める（ADR0060 の決定 10）。 */
constexpr int base_layout_button_compact_width = 44;
/* 閲覧/編集の区画。Read/Edit・閲覧/編集・查看/编辑 の字＋左右 12 が収まる 4 の倍数の固定値。 */
constexpr int base_mode_item_width = 56;
/* 未保存印（ノート名の直後の chip_background の丸・ADR0060 の決定 8）とノート名との間。 */
constexpr int base_unsaved_mark = 8;
constexpr int base_unsaved_gap = 6;
/* 空状態の案内の行間（ADR0060 の決定 13）。 */
constexpr int base_guide_gap = 8;
constexpr int base_caption_height = 44; /* 右ペインの頭。窓を掴んで動かせる帯 */
constexpr int base_action_height = 36;
constexpr int base_pane_left = 36;
constexpr int base_pane_right = 56;
constexpr int base_pane_top = 16;
constexpr int base_pane_bottom = 32;
constexpr int base_mono_font = 11;
constexpr int base_tracking = 1;
/* SetTextCharacterExtra が失敗したときの戻り値（wingdi.h に名前が無い）。 */
constexpr int tracking_unavailable = 0x8000000;
/* 省略しても残すノート名の幅（ADR 0011 の決定 4）。 */
constexpr int base_breadcrumb_note = 48;
constexpr int base_breadcrumb_height = 24;
constexpr int base_breadcrumb_padding = 8;
constexpr int base_breadcrumb_tip = 10;
constexpr int base_breadcrumb_compact_width = 320;
constexpr int base_breadcrumb_compact_padding = 4;
constexpr int base_breadcrumb_compact_tip = 6;
constexpr int base_close_margin = 16;
constexpr int base_command_gap = 8;
constexpr int base_command_input_height = 28;
constexpr int base_command_palette_width = 460;
constexpr int base_command_palette_padding = 16;
constexpr int base_command_row_height = 32;
constexpr int base_command_status_height = 24;
constexpr int base_command_help_row_height = 14;
/* 札の右に出す「n/m」の幅と、それを組む領域（数字と `/` だけ・翻訳の対象を増やさない・#69）。 */
constexpr int base_command_pages_width = 44;
constexpr size_t command_pages_capacity = 16;
/* 履歴の行（ADR 0038 の決定 12）。最初の行の写しと、置換子を埋めた 1 行（UTF-8・終端込み）。 */
constexpr size_t history_excerpt_capacity = 256;
constexpr size_t history_line_capacity = 320;
constexpr unsigned char utf8_lead_mask = 0xC0;
constexpr unsigned char utf8_continuation = 0x80;
constexpr int base_ex_height = 32;
constexpr int base_command_row_padding = 12;
constexpr int base_command_alias_room = 96;
constexpr int base_command_alias_gap = 12;
constexpr int base_command_input_vertical_inset = 2;
constexpr int base_search_button_width = 64;
/* 操作行の常設ボタンを出せる最小の余白（これより狭ければ「操作 ▾」のメニューへ移る）。 */
/* 頭の釦の字の左の字下げと右の余白、釦と釦の間隔（96 DPI・#122）。 */
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
/* 設定の歯車と、現在の選択の印の寸法（96 DPI）。歯車は close_rect と同じ 24px の箱へ
 * 面のパスで塗る（ADR 0033 の決定 6）ので、放射線の長さの定数は要らない。 */
/* ---- 共通ヘッダーの寸法（余白の原則: 窓の縁 16・枠の内側 8・4 の倍数。#349 で設計席が確定）----
 * 縁は ui_part_edge（16）。描画・当たり判定・UIA・tooltip は close_rect から順に導く。 */
constexpr int base_settings_gap = 8;            /* 常設のC様式5部品の間 */
constexpr int base_brand_gap = 16;              /* 新しいノートとブランドの間 */
constexpr int base_layout_help_gap = 16;        /* 表示3択の群とヘルプ「?」の間 */
constexpr int base_layout_help_compact_gap = 8; /* 狭幅で移動領域が足りないときの群間 */
/* 狭幅で区画 44 でも移動領域 24（ADR0059）が残らないときの区画（群間 8 の次に縮める）。 */
constexpr int base_layout_button_narrow_width = 40;
/* チェック枠と採用印は24の箱へ置き、ラベルはrow.left + 32から始める。 */
constexpr int base_settings_mark_inset = 4;
constexpr int base_settings_mark_box = 24;
constexpr int base_settings_width = 480;
constexpr int base_settings_height = 384;
constexpr int base_theme_height = 560;
constexpr int base_theme_row_height = 32;
constexpr int base_theme_choice_height = 32;
constexpr int base_settings_padding = 16;
constexpr int base_settings_label = 24;
constexpr int base_settings_item_height = 32;
constexpr int base_settings_item_gap = 8;
constexpr int base_settings_group_gap = 20;

static size_t append_text(char *_Nonnull out, size_t at, const char *_Nonnull text);
static WPARAM shortcut_character_for(enum folio_command command);
static bool shortcut_keydown(struct folio_window *_Nonnull self, WPARAM key);
static bool accessible_command(enum accessible_kind kind, enum folio_command *_Nonnull out);
static const char *_Nonnull part_shortcut(enum accessible_kind kind, enum folio_language language);
static RECT menu_rect(const struct folio_window *_Nonnull self);
static RECT menu_row_rect(const struct folio_window *_Nonnull self, size_t index);
static size_t menu_visible_count(const struct folio_window *_Nonnull self);
static size_t menu_scroll_limit(const struct folio_window *_Nonnull self);
static void arrange_drawer_region(struct folio_window *_Nonnull self, RECT bounds);
static void menu_reveal(struct folio_window *_Nonnull self);
static void draw_menu_surface(const struct folio_window *_Nonnull self, HDC device);
static void show_menu_surface(struct folio_window *_Nonnull self,
                              const struct menu_request *_Nonnull request);
static bool menu_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                         LPARAM lparam);
static void menu_execute(struct folio_window *_Nonnull self, size_t index);
static bool menu_navigate(struct folio_window *_Nonnull self, WPARAM key);
static enum folio_state_outcome menu_availability(const struct folio_window *_Nonnull self,
                                                  size_t index);
static void menu_accessible(const struct folio_window *_Nonnull self, size_t index,
                            struct accessible_view *_Nonnull out);
static void report_outcome(struct folio_window *_Nonnull self, enum folio_state_outcome outcome);
[[nodiscard]] static bool preview_theme(struct folio_window *_Nonnull self,
                                        enum folio_theme_choice choice);
static bool refresh_band_layout(struct folio_window *_Nonnull self);
static enum ui_text surface_button_label(const struct folio_window *_Nonnull self, size_t slot);
static bool surface_button_visible(const struct folio_window *_Nonnull self, size_t slot);
static RECT accessible_surface_button_rect(const struct folio_window *_Nonnull self, size_t slot);
static HRESULT surface_button_action(struct folio_window *_Nonnull self, size_t slot);
static struct name_prompt_result
show_name_prompt(struct folio_window *_Nonnull self,
                 const struct name_prompt_request *_Nonnull request);
static void show_failure_box(struct folio_window *_Nonnull self, enum folio_state_outcome outcome);
static void execute_command(struct folio_window *_Nonnull self, enum folio_command command,
                            const char *_Nonnull argument);
static enum folio_state_outcome store_body(struct folio_window *_Nonnull self);
static enum folio_state_outcome command_save_as(struct folio_window *_Nonnull self,
                                                const char *_Nonnull argument);
static void close_command_surface(struct folio_window *_Nonnull self);
static void cancel_part_press(struct folio_window *_Nonnull self);
static void normalize_header_target(struct folio_window *_Nonnull self);
static void close_geometry_menu(struct folio_window *_Nonnull self);
static void apply_layout_mode(struct folio_window *_Nonnull self, enum folio_layout_mode mode);
static void show_pane_layout(struct folio_window *_Nonnull self);
static void show_drawer_layout(struct folio_window *_Nonnull self);
static void restore_valid_focus(const struct folio_window *_Nonnull self, HWND _Nullable focus);
static HWND surface_return_target(const struct folio_window *_Nonnull self, HWND _Nullable window);
static void redraw_drawer(const struct folio_window *_Nonnull self);
static void redraw_command_layer(const struct folio_window *_Nonnull self);
static void rearrange_keeping_line(struct folio_window *_Nonnull self);
static bool keyboard_accelerator(struct folio_window *_Nonnull self, WORD id);
static bool create_keyboard_accelerators(struct folio_window *_Nonnull self);
static bool translate_keyboard(const struct folio_window *_Nonnull self,
                               const MSG *_Nonnull message);
static void keyboard_activate(struct folio_window *_Nonnull self);
static bool choice_step(struct folio_window *_Nonnull self, WPARAM key);
static bool part_selected(const struct folio_window *_Nonnull self, enum accessible_kind kind);
static void set_hover(struct folio_window *_Nonnull self, enum accessible_kind kind);
static void show_focus_cues(struct folio_window *_Nonnull self, bool shown);
static void pointer_focus(struct folio_window *_Nonnull self, POINT point);
static void accessible_command_invoked(struct folio_window *_Nonnull self,
                                       enum folio_command command);
static bool pane_streaming(const struct folio_window *_Nonnull self);
static bool accessible_visible(const struct folio_window *_Nonnull self, enum accessible_kind kind);
static RECT accessible_main_rect(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind);
static LRESULT on_accessibility_message(struct folio_window *_Nonnull self, UINT message,
                                        WPARAM wparam, LPARAM lparam);

static bool session_busy(const struct folio_window *_Nonnull self)
{
    return self->session_phase != SESSION_PHASE_IDLE;
}

/* disableだけでは既に届いたmessageを止めない。通常loopと各入力窓が同じ集合を使う。 */
static bool session_input_message(UINT message)
{
    if ((message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
        (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST))
    {
        return true;
    }
    switch (message)
    {
    case WM_COMMAND:
    case WM_SYSCOMMAND:
    case WM_CLOSE:
    case WM_CONTEXTMENU:
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_CHAR:
    case WM_IME_KEYDOWN:
    case WM_IME_KEYUP:
    case folio_message_select_note:
    case folio_message_drawer_menu:
    case folio_message_command_focus_lost:
    case folio_message_open_menu:
    case folio_message_report_outcome:
    case folio_message_choose_color:
        return true;
    default:
        /* Win32のmessageは開いた集合（C-017）。 */
        return false;
    }
}

static bool session_blocks_message(const struct folio_window *_Nonnull self, UINT message)
{
    return (session_busy(self) || self->native_modal_depth > 0 ||
            self->close_phase != CLOSE_PHASE_IDLE) &&
           session_input_message(message);
}

/* shellと内容の可否を分け、UIA/pointer/描画/tooltipで共有する。 */
static bool shell_ready(const struct folio_window *_Nonnull self)
{
    return self->handle != nullptr && IsWindowEnabled(self->handle) && !session_busy(self) &&
           (self->native_modal_depth > 0 ||
            (!self->removal_running && !pane_streaming(self) && !self->layout_changing));
}

static bool part_enabled(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (accessible_item_shell(kind) || kind == ACCESSIBLE_WINDOW || kind == ACCESSIBLE_HEADER)
        return shell_ready(self);
    return folio_window_accessible_ready(self) && self->command_surface == COMMAND_SURFACE_CLOSED;
}

static void dispatch_finished(struct folio_window *_Nonnull self)
{
    self->dispatch_depth -= 1;
    if (self->dispatch_depth == 0 && self->native_modal_depth == 0 &&
        self->close_phase == CLOSE_PHASE_REQUESTED && self->handle != nullptr)
        PostMessageW(self->handle, folio_message_close_requested, 0, 0);
}

void folio_window_begin_dispatch(struct folio_window *_Nonnull self)
{
    self->dispatch_depth += 1;
}

void folio_window_end_dispatch(struct folio_window *_Nonnull self)
{
    dispatch_finished(self);
}

static bool root_blocks_message(const struct folio_window *_Nonnull self, UINT message,
                                WPARAM wparam);

/* 同じthreadの別トップレベルやモーダルの入力は、主窓の承認待ちで消費しない。 */
static bool session_blocks_queued_input(const struct folio_window *_Nonnull self,
                                        const MSG *_Nonnull message)
{
    if (message->hwnd == self->handle)
        return root_blocks_message(self, message->message, message->wParam);
    return session_blocks_message(self, message->message) && IsChild(self->handle, message->hwnd);
}

static bool shell_system_command(WPARAM command)
{
    switch (command & 0xFFF0)
    {
    case SC_CLOSE:
    case SC_MOVE:
    case SC_SIZE:
    case SC_MINIMIZE:
    case SC_MAXIMIZE:
    case SC_RESTORE:
    case SC_KEYMENU:
    case SC_MOUSEMENU:
        return true;
    default:
        return false; /* OSのsystem commandは開いた集合。 */
    }
}

static bool root_blocks_message(const struct folio_window *_Nonnull self, UINT message,
                                WPARAM wparam)
{
    if (session_busy(self))
        return session_input_message(message);
    if (self->native_modal_depth == 0 && self->close_phase == CLOSE_PHASE_IDLE)
        return false;
    if (message == WM_CLOSE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
        message == WM_MOUSEMOVE || message == WM_MOUSELEAVE || message == WM_CAPTURECHANGED ||
        message == WM_CANCELMODE)
        return false;
    if (message == WM_SYSCOMMAND)
        return !shell_system_command(wparam);
    return session_input_message(message);
}

/* 先行復旧は元要求より前。READYは復旧だけの成功で、対象/本文/名前は呼出し元で取り直す。 */
static enum folio_state_outcome prepare_name_operation(struct folio_window *_Nonnull self)
{
    struct rename_view pending = {.from = "", .to = ""};
    if (!folio_state_rename_pending(self->state, &pending))
    {
        return FOLIO_STATE_READY;
    }
    HWND focus = GetFocus();
    struct name_prompt_request request = {
        .state = self->state, .kind = NAME_PROMPT_RETRY_RENAME, .units = u"", .count = 0};
    enum folio_state_outcome outcome = show_name_prompt(self, &request).outcome;
    /* 復旧の採用は元要求が失敗/未実行でも表示へ写す。本文には触らない。 */
    redraw_drawer(self);
    redraw_command_layer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    restore_valid_focus(self, focus);
    return outcome;
}
static void set_pane_notice(struct folio_window *_Nonnull self, enum pane_notice notice);
static bool holds_document(const struct folio_window *_Nonnull self, struct note_ref target);
static void decorate(HWND handle, struct folio_palette palette);
static void refresh_tooltip_style(const struct folio_window *_Nonnull self);
static void refresh_tooltip_rects(const struct folio_window *_Nonnull self);
static void activate_tooltip(const struct folio_window *_Nonnull self);
static void refresh_surface_tooltip_rects(const struct folio_window *_Nonnull self);
static enum folio_state_outcome render_pane(struct folio_window *_Nonnull self);
static enum folio_state_outcome from_pane_text(enum note_pane_text_outcome outcome);
static void show_settings_surface(struct folio_window *_Nonnull self,
                                  const char *_Nonnull argument);
static void command_not_found(struct folio_window *_Nonnull self);
static void show_command_palette(struct folio_window *_Nonnull self);
static void move_command_help(struct folio_window *_Nonnull self, WPARAM key);
static void move_command_selection(struct folio_window *_Nonnull self, WPARAM key);
static bool settings_navigate(struct folio_window *_Nonnull self, WPARAM key);
static size_t settings_initial_row(const struct folio_window *_Nonnull self);
static void close_command_surface(struct folio_window *_Nonnull self);
static void search_input_changed(struct folio_window *_Nonnull self);
static LRESULT CALLBACK command_input_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam);
static void filter_input_changed(struct folio_window *_Nonnull self);
static LRESULT CALLBACK filter_input_procedure(HWND window, UINT message, WPARAM wparam,
                                               LPARAM lparam);
static LRESULT CALLBACK command_layer_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam);
static LRESULT CALLBACK settings_backdrop_procedure(HWND window, UINT message, WPARAM wparam,
                                                    LPARAM lparam);
static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

/* 表示/配置/paint/hitが同じclient・DPI・application値から領域を導く。 */
static struct folio_regions regions_for(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int top = scale(base_common_header_height, dpi);
    struct folio_regions regions = {.header = {0, 0, client.right, top},
                                    .drawer = {0, top, 0, client.bottom},
                                    .pane = {0, top, client.right, client.bottom}};
    switch (folio_state_layout_mode(self->state))
    {
    case FOLIO_LAYOUT_BOTH:
        regions.drawer.right = scale(base_drawer_width, dpi);
        regions.pane.left = regions.drawer.right;
        break;
    case FOLIO_LAYOUT_DRAWER:
        regions.drawer.right = client.right;
        regions.pane.left = client.right;
        break;
    case FOLIO_LAYOUT_NOTE:
        break;
    }
    return regions;
}

static bool command_keys(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_PALETTE && self->command_keys_visible;
}

static bool settings_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_SETTINGS ||
           self->command_surface == COMMAND_SURFACE_THEMES;
}

static bool pane_visible(const struct folio_window *_Nonnull self)
{
    RECT pane = regions_for(self).pane;
    return pane.right > pane.left;
}

/* 文書（名前付きか無題）を開いているか。偽なら本文操作帯・閲覧/編集・本文を出さず、
 * 本文領域に空状態の案内を描く（ADR0060 の決定 13）。 */
static bool document_open(const struct folio_window *_Nonnull self)
{
    return folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NONE;
}

static bool drawer_visible(const struct folio_window *_Nonnull self)
{
    RECT drawer = regions_for(self).drawer;
    return drawer.right > drawer.left;
}

/* 面が持つ操作の行数。#75 の設定画面（ADR 0029 の決定 9）はここへ自分の行数を足す。 */
static int command_surface_rows(const struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return (int)folio_menu_action_count(self->menu.context);

    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_PALETTE:
        return (int)folio_command_listed_count();
    case COMMAND_SURFACE_SETTINGS:
        return (int)settings_row_count();
    case COMMAND_SURFACE_THEMES:
        return (int)folio_theme_choice_count();
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
        return (int)folio_state_history_count(self->state);
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
                   ? (int)(sizeof command_shortcuts / sizeof command_shortcuts[0] +
                           layout_button_count())
                   : 0;
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
    case COMMAND_SURFACE_SETTINGS:
    case COMMAND_SURFACE_THEMES:
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
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

/* EDIT を持たず、レイヤー自身がフォーカスと鍵を受ける一覧の面（設定・履歴。
 * ADR 0031 の決定 7・ADR 0038 の決定 12）。 */
static bool command_surface_listing(enum command_surface_mode surface)
{
    return surface == COMMAND_SURFACE_SETTINGS || surface == COMMAND_SURFACE_HISTORY ||
           surface == COMMAND_SURFACE_MENU;
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
    /* EDITと自前候補の間の移動も、自面の中に留まる（ADR0058）。 */
    if (self->command_surface != COMMAND_SURFACE_CLOSED && window == self->command_layer)
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
    if (command_surface_listing(self->command_surface) || command_keys(self))
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
    set_pane_notice(self, PANE_NOTICE_NONE);
    self->command_unknown = false;
    self->command_no_match = false;
    self->command_failure = FOLIO_STATE_READY;
    self->result_line[0] = '\0';
}

static HFONT _Nullable gutter_font(const struct folio_window *_Nonnull self)
{
    return self->gutter_font != nullptr ? self->gutter_font : self->mono_font;
}

/* 行番号の数字幅（字間0）。font/DPI/倍率の変更時だけ測り、毎回のpaintでは測らない。 */
static int measure_digit(const struct folio_window *_Nonnull self, UINT dpi)
{
    HDC device = GetDC(self->handle);
    if (device == nullptr)
    {
        return scale(base_digit_fallback, dpi);
    }
    HGDIOBJ previous = SelectObject(device, gutter_font(self));
    SetTextCharacterExtra(device, 0);
    SIZE measured = {.cx = 0, .cy = 0};
    bool measurable = GetTextExtentPoint32W(device, L"0", 1, &measured) != 0;
    SelectObject(device, previous);
    ReleaseDC(self->handle, device);
    return measurable ? (int)measured.cx : scale(base_digit_fallback, dpi);
}

static void refresh_gutter_font(struct folio_window *_Nonnull self, UINT dpi)
{
    LOGFONTW font;
    if (GetObjectW(self->mono_font, sizeof font, &font) != 0)
    {
        int percent = self->pane == nullptr ? 100 : note_pane_zoom_percent(self->pane);
        font.lfHeight = MulDiv(font.lfHeight, percent, 100);
        HFONT next = CreateFontIndirectW(&font);
        if (next != nullptr)
        {
            if (self->gutter_font != nullptr)
                DeleteObject(self->gutter_font);
            self->gutter_font = next;
        }
        else
        {
            MessageBeep(MB_ICONWARNING);
        }
    }
    self->digit_width = measure_digit(self, dpi);
}

static void refresh_part_metrics(struct folio_window *_Nonnull self, UINT dpi);

static void refresh_font(struct folio_window *_Nonnull self, UINT dpi)
{
    if (self->native_metrics != nullptr)
        name_prompt_metrics_invalidate(self->native_metrics);
    self->band.valid = false;
    self->settings_metrics.width = 0;
    self->help_metrics.valid = false;
    self->part_metrics.valid = false;
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
    refresh_gutter_font(self, dpi);
    refresh_part_metrics(self, dpi);
    refresh_tooltip_style(self);
}

/* 下端の 1 行は本文の既存余白だけを使い、面の下端に同じ高さを予約する。 */
static bool pane_unavailable(const struct folio_window *_Nonnull self)
{
    return self->pane != nullptr &&
           note_pane_readiness(self->pane) == NOTE_PANE_READINESS_UNAVAILABLE;
}

static bool pane_streaming(const struct folio_window *_Nonnull self)
{
    return self->pane != nullptr &&
           note_pane_readiness(self->pane) == NOTE_PANE_READINESS_STREAMING;
}

/* 同期投入が借りているapplicationのRTFを、再入操作で作り直させない。
 * EN_CHANGE/EN_VSCROLLや描画は止めず、既存の行索引の無効化へ通す。 */
static bool load_blocks_input(const struct folio_window *_Nonnull self, UINT message)
{
    return pane_streaming(self) &&
           (message == WM_KEYDOWN || message == WM_CHAR || message == WM_LBUTTONDOWN ||
            message == WM_CLOSE || message == folio_message_drawer_menu);
}

static int pane_notice_height(const struct folio_window *_Nonnull self)
{
    return (self->notice == PANE_NOTICE_COMMAND_RESULT ||
            (folio_state_layout_mode(self->state) != FOLIO_LAYOUT_DRAWER &&
             (self->notice != PANE_NOTICE_NONE || pane_unavailable(self))))
               ? scale(base_command_status_height, GetDpiForWindow(self->handle))
               : 0;
}

static RECT command_palette_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int margin = scale(ui_part_edge, dpi);
    int available = client.right - margin * 2;
    int width = scale(self->command_surface == COMMAND_SURFACE_HISTORY ? base_settings_width
                                                                       : base_command_palette_width,
                      dpi);
    if (width > available)
    {
        width = available;
    }
    /* 箱の高さは「面が出す行の数」で決める。登録表の総数で取ると 1 行ぶん余る（補正 9）。
     * 望む高さであり、窓に入らなければ下で丸める。入った分の配り方は command_visible_help。 */
    int height =
        scale(command_box_height(command_surface_rows(self), command_help_rows(self)), dpi);
    int header = scale(base_common_header_height, dpi);
    int top = header + scale(ui_part_edge, dpi);
    int available_height =
        client.bottom - pane_notice_height(self) - top - scale(ui_part_edge, dpi);
    if (height > available_height)
    {
        height = available_height;
    }
    top += (available_height - height) / 2;
    RECT bounds = {(client.right - width) / 2, top, (client.right + width) / 2, top + height};
    return bounds;
}

static RECT command_ex_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    client.bottom -= pane_notice_height(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int status = self->command_failure != FOLIO_STATE_READY || self->command_unknown ||
                         self->command_no_match
                     ? scale(base_command_status_height, dpi)
                     : 0;
    RECT bounds = {0, client.bottom - scale(base_ex_height, dpi) - status, client.right,
                   client.bottom};
    return bounds;
}

/* SEARCH/REPLACEの所有と矩形は同じ帯。値はarrangeで一度だけ測る。 */
static bool command_band(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_SEARCH ||
           self->command_surface == COMMAND_SURFACE_REPLACE;
}

static RECT command_band_rect(const struct folio_window *_Nonnull self)
{
    RECT pane = regions_for(self).pane;
    int top =
        pane.top + scale(base_caption_height + base_action_height, GetDpiForWindow(self->handle));
    return (RECT){pane.left, top, pane.right, top + self->band.height};
}

static RECT search_close_rect(const struct folio_window *_Nonnull self)
{
    return self->band.close;
}

static RECT search_header_rect(const struct folio_window *_Nonnull self)
{
    return self->band.label[0];
}

static RECT search_result_rect(const struct folio_window *_Nonnull self)
{
    return self->band.result;
}

static RECT search_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    return self->band.action[index];
}

static bool prepare_surface_metrics(struct folio_window *_Nonnull self);

static RECT surface_frame_rect(const struct folio_window *_Nonnull self,
                               enum ui_part_surface_region region)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    return ui_part_surface_rect(bounds, GetDpiForWindow(self->handle), region);
}

static RECT settings_dialog_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int edge = scale(base_settings_padding, dpi) * 2;
    int width = scale(base_settings_width, dpi);
    int wanted =
        self->command_surface == COMMAND_SURFACE_THEMES ? base_theme_height : base_settings_height;
    int height = scale(wanted, dpi);
    width = width < client.right - edge ? width : client.right - edge;
    int header = scale(base_common_header_height, dpi);
    height = height < client.bottom - header - edge ? height : client.bottom - header - edge;
    int top = header + (client.bottom - header - height) / 2;
    return (RECT){(client.right - width) / 2, top, (client.right + width) / 2, top + height};
}

static enum folio_state_outcome settings_notice(const struct folio_window *_Nonnull self)
{
    if (self->command_failure != FOLIO_STATE_READY)
    {
        return self->command_failure;
    }
    enum folio_state_outcome settings = folio_state_settings_notice(self->state);
    return settings != FOLIO_STATE_READY ? settings : folio_state_appearance_notice(self->state);
}

static bool settings_has_notice(const struct folio_window *_Nonnull self)
{
    return settings_notice(self) != FOLIO_STATE_READY;
}

static int settings_footer_height(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_THEMES ? self->settings_metrics.themes_footer
                                                           : self->settings_metrics.settings_footer;
}

static RECT settings_content_rect(const struct folio_window *_Nonnull self)
{
    RECT content = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    content.bottom -= settings_footer_height(self) + scale(8, GetDpiForWindow(self->handle));
    if (content.bottom < content.top)
        content.bottom = content.top;
    return content;
}

static int settings_columns(const struct folio_window *_Nonnull self)
{
    RECT content = settings_content_rect(self);
    return content.right - content.left >= scale(360, GetDpiForWindow(self->handle)) ? 3 : 1;
}

static RECT settings_item_rect(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds = settings_content_rect(self);
    int columns = settings_columns(self);
    int gap = scale(base_settings_item_gap, dpi);
    int height = scale(base_settings_item_height, dpi);
    int label = scale(base_settings_label, dpi);
    int stride = label + (3 / columns) * (height + gap) - gap + scale(base_settings_group_gap, dpi);
    int theme = label + scale(base_theme_choice_height + base_settings_group_gap, dpi);
    int group = settings_rows[index].group;
    bounds.top += (group > 0 ? theme + (group - 1) * stride : 0) - (int)self->command_first;
    if (settings_rows[index].kind == SETTINGS_ROW_HEADING)
    {
        bounds.bottom = bounds.top + label;
        return bounds;
    }
    if (settings_rows[index].kind == SETTINGS_ROW_THEME)
    {
        bounds.top += label;
        bounds.bottom = bounds.top + scale(base_theme_choice_height, dpi);
        return bounds;
    }
    if (settings_rows[index].kind == SETTINGS_ROW_NUMBER)
    {
        bounds.bottom = bounds.top + height;
        return bounds;
    }
    int value = settings_rows[index].position;
    int width = (bounds.right - bounds.left - gap * (columns - 1)) / columns;
    bounds.left += value % columns * (width + gap);
    bounds.right = bounds.left + width;
    bounds.top += label + value / columns * (height + gap);
    bounds.bottom = bounds.top + height;
    return bounds;
}

static size_t settings_scroll_limit(const struct folio_window *_Nonnull self)
{
    RECT content = settings_content_rect(self);
    RECT last = settings_item_rect(self, settings_row_count() - 1);
    int excess = last.bottom + (int)self->command_first - content.bottom;
    return excess > 0 ? (size_t)excess : 0;
}

static void reveal_settings_selection(struct folio_window *_Nonnull self)
{
    RECT content = settings_content_rect(self);
    RECT item = settings_item_rect(self, self->command_selection);
    int offset = (int)self->command_first;
    if (item.top < content.top)
    {
        offset -= content.top - item.top;
    }
    if (item.bottom > content.bottom)
    {
        offset += item.bottom - content.bottom;
    }
    self->command_first = offset > 0 ? (size_t)offset : 0;
    size_t limit = settings_scroll_limit(self);
    if (self->command_first > limit)
    {
        self->command_first = limit;
    }
}

static RECT settings_close_rect(const struct folio_window *_Nonnull self)
{
    return surface_frame_rect(self, UI_PART_SURFACE_CLOSE);
}

static RECT command_surface_rect(const struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return menu_rect(self);

    switch (self->command_surface)
    {
    case COMMAND_SURFACE_EX:
        return command_ex_rect(self);
    case COMMAND_SURFACE_SEARCH:
        return command_band_rect(self);
    case COMMAND_SURFACE_REPLACE:
        return command_band_rect(self);
    case COMMAND_SURFACE_SETTINGS:
    case COMMAND_SURFACE_THEMES:
        return settings_dialog_rect(self);
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_PALETTE:
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
        return command_palette_rect(self);
    }
    return command_palette_rect(self);
}

static RECT theme_input_rect(const struct folio_window *_Nonnull self)
{
    RECT input = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    UINT dpi = GetDpiForWindow(self->handle);
    input.top += scale(20, dpi);
    input.bottom = input.top + scale(ui_part_height, dpi);
    return input;
}

static RECT theme_footer_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    bounds.top = bounds.bottom - settings_footer_height(self);
    return bounds;
}

static RECT theme_rows_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = theme_input_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    bounds.top = bounds.bottom + scale(8, dpi);
    int bottom = theme_footer_rect(self).top - scale(8, dpi);
    int height = bottom > bounds.top ? bottom - bounds.top : 0;
    int row = scale(base_theme_row_height, dpi);
    /* 短い面だけ二つの余白を8→4 logicalまで縮め、固定脚と候補1行を保つ（ADR0064）。 */
    int missing = row > height ? row - height : 0;
    int capacity = scale(8, dpi) - scale(4, dpi);
    int before = missing < capacity ? missing : capacity;
    missing -= before;
    int after = missing < capacity ? missing : capacity;
    bounds.top -= before;
    height = bottom + after > bounds.top ? bottom + after - bounds.top : 0;
    bounds.bottom = bounds.top + height / row * row;
    return bounds;
}

static RECT theme_back_rect(const struct folio_window *_Nonnull self)
{
    RECT back = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    back.top = back.bottom - scale(ui_part_height, GetDpiForWindow(self->handle));
    back.right = back.left + scale(100, GetDpiForWindow(self->handle));
    return back;
}

static RECT command_tab_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    RECT tab = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    UINT dpi = GetDpiForWindow(self->handle);
    int width = scale(88, dpi);
    tab.left += (int)slot * width;
    tab.right = tab.left + width;
    tab.bottom = tab.top + scale(ui_part_height, dpi);
    return tab;
}

static RECT command_input_rect(const struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        return theme_input_rect(self);
    }
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        return self->band.field[0];
    }
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        RECT input = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
        input.top = command_tab_rect(self, 0).bottom + scale(8, dpi);
        input.bottom = input.top + scale(base_command_input_height, dpi);
        return input;
    }
    if (command_surface_listing(self->command_surface))
    {
        int padding = scale(base_command_palette_padding, dpi);
        int top = padding + scale(base_command_help_row_height * 2, dpi);
        RECT rows = {padding, top, bounds.right - padding,
                     top + scale(base_command_input_height, dpi)};
        return rows;
    }
    int inset = scale(base_command_gap, dpi);
    int vertical = scale(base_command_input_vertical_inset, dpi);
    RECT input = {bounds.left + inset, bounds.bottom - scale(base_ex_height, dpi) + vertical,
                  bounds.right - inset, bounds.bottom - vertical};
    return input;
}

/* 描画・native EDIT・hit・UIAが同じ帯の値を参照する。 */
static RECT replace_input_rect(const struct folio_window *_Nonnull self, size_t index)
{
    return self->band.field[index];
}

static RECT surface_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    return self->band.action[index];
}

static RECT command_body_rect(const struct folio_window *_Nonnull self)
{
    RECT body = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    UINT dpi = GetDpiForWindow(self->handle);
    body.top = command_keys(self) ? command_tab_rect(self, 0).bottom + scale(8, dpi)
                                  : command_input_rect(self).bottom + scale(8, dpi);
    body.bottom -= scale(base_command_status_height + 8, dpi);
    if (body.bottom < body.top)
        body.bottom = body.top;
    return body;
}

/* いま出せるヘルプの行数。操作の一覧に command_row_floor 行を残し、余りをヘルプへ回す。
 * 最小寸法（560×360 と 840×540）では操作 3 行・ヘルプ 6 行になる（#69）。 */
static RECT command_help_geometry(const struct folio_window *_Nonnull self, size_t slot,
                                  int *_Nullable maximum_line_height);
static int command_help_total(const struct folio_window *_Nonnull self);

static int command_visible_help(const struct folio_window *_Nonnull self)
{
    if (!command_keys(self))
        return 0;
    RECT area = command_body_rect(self);
    return area.bottom - area.top;
}

/* 累積pixel位置を現在のviewportへ丸める。全内容が入る場合は先頭へ戻す。 */
static size_t command_help_offset(const struct folio_window *_Nonnull self)
{
    int total = command_help_total(self);
    int visible = command_visible_help(self);
    size_t limit = total > visible ? (size_t)(total - visible) : 0;
    return self->command_help_scroll < limit ? self->command_help_scroll : limit;
}

static RECT command_toggle_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    bounds.top = bounds.bottom - scale(ui_part_height, GetDpiForWindow(self->handle));
    return bounds;
}

static RECT command_rows_rect(const struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        RECT r;
        GetClientRect(self->command_layer, &r);
        return r;
    }

    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        return theme_rows_rect(self);
    }
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        RECT rows = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
        rows.bottom -= scale(base_command_status_height * 2, GetDpiForWindow(self->handle));
        return rows;
    }
    RECT bounds = command_input_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    bounds.top = command_body_rect(self).top;
    bounds.bottom = command_body_rect(self).bottom;
    int row = scale(base_command_row_height, dpi);
    if (bounds.bottom < bounds.top)
    {
        bounds.bottom = bounds.top;
    }
    bounds.bottom = bounds.top + (bounds.bottom - bounds.top) / row * row;
    return bounds;
}

static size_t palette_visible_count(const struct folio_window *_Nonnull self);
static void reveal_palette_selection(struct folio_window *_Nonnull self);
static void reveal_theme_selection(struct folio_window *_Nonnull self);

static size_t command_visible_rows(const struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return menu_visible_count(self);
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
        return palette_visible_count(self);

    RECT rows = command_rows_rect(self);
    int height = self->command_surface == COMMAND_SURFACE_THEMES ? base_theme_row_height
                                                                 : base_command_row_height;
    return (size_t)((rows.bottom - rows.top) / scale(height, GetDpiForWindow(self->handle)));
}

static void reveal_command_selection(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        reveal_theme_selection(self);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        reveal_palette_selection(self);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        menu_reveal(self);
        return;
    }

    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        reveal_settings_selection(self);
        return;
    }
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

/* 背景の配置正本は実HWND。位置不変ならMoveWindowによる再描画を増やさない。 */
static void move_child(HWND window, RECT bounds)
{
    RECT current;
    if (GetWindowRect(window, &current))
    {
        MapWindowPoints(nullptr, GetParent(window), (POINT *)&current, 2);
        if (EqualRect(&current, &bounds))
            return;
    }
    MoveWindow(window, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

static void show_child(HWND window, bool visible)
{
    bool shown = (GetWindowLongPtrW(window, GWL_STYLE) & WS_VISIBLE) != 0;
    if (shown != visible)
        ShowWindow(window, visible ? SW_SHOWNA : SW_HIDE);
}

/* 入力欄の箱に単一行EDITを置く唯一の経路。枠とfocusは箱の外に描く（#349）。 */
static void place_field(HWND _Nullable edit, RECT box, int right_room)
{
    if (edit == nullptr)
    {
        return;
    }
    RECT bounds = ui_part_field_edit(box, ui_part_field_text_height(edit), GetDpiForWindow(edit));
    move_child(edit, bounds);
    ui_part_field_margins(edit, GetDpiForWindow(edit), right_room);
}

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
        place_field(input, replace_input_rect(self, index), 0);
        ShowWindow(input, replacing ? SW_SHOW : SW_HIDE);
    }
}

/* Layered childの合成でもカードを暗くしないよう、同じ矩形を暗幕から除く。 */
static HRGN _Nullable settings_backdrop_region(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    RECT card = settings_dialog_rect(self);
    HRGN _Nullable region =
        CreateRectRgn(0, scale(base_common_header_height, GetDpiForWindow(self->handle)),
                      client.right, client.bottom);
    HRGN _Nullable hole = CreateRectRgn(card.left, card.top, card.right, card.bottom);
    if (region == nullptr || hole == nullptr)
    {
        DeleteObject(region);
        DeleteObject(hole);
        return nullptr;
    }
    int result = CombineRgn(region, region, hole, RGN_DIFF);
    DeleteObject(hole);
    if (result == ERROR)
    {
        DeleteObject(region);
        return nullptr;
    }
    return region;
}

static bool set_settings_backdrop_region(const struct folio_window *_Nonnull self)
{
    HRGN _Nullable region = settings_backdrop_region(self);
    if (region == nullptr)
    {
        return false;
    }
    if (!SetWindowRgn(self->settings_backdrop, region, TRUE))
    {
        DeleteObject(region);
        return false;
    }
    return true; /* 成功後のregionはOSが所有する。 */
}

static bool arrange_settings_backdrop(const struct folio_window *_Nonnull self)
{
    if (self->settings_backdrop == nullptr)
    {
        return true;
    }
    bool native = self->native_modal_depth > 0;
    bool visible = native || settings_surface(self);
    if (native)
    {
        RECT client;
        GetClientRect(self->handle, &client);
        HRGN region =
            CreateRectRgn(0, scale(base_common_header_height, GetDpiForWindow(self->handle)),
                          client.right, client.bottom);
        if (region == nullptr)
            return false;
        if (!SetWindowRgn(self->settings_backdrop, region, TRUE))
        {
            DeleteObject(region);
            return false;
        }
    }
    if (!native && visible && !set_settings_backdrop_region(self))
    {
        return false;
    }
    RECT client;
    GetClientRect(self->handle, &client);
    return SetWindowPos(self->settings_backdrop, HWND_TOP, 0, 0, client.right, client.bottom,
                        SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW)) != 0;
}

static void enable_surface_background(const struct folio_window *_Nonnull self)
{
    /* native0では元focusを復帰できる状態へ戻す。終了phaseの業務入力は共通guardが止める。 */
    bool enabled = !settings_surface(self) && self->native_modal_depth == 0;
    HWND windows[] = {self->filter_input,
                      self->pane == nullptr ? nullptr : note_pane_handle(self->pane),
                      self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer)};
    for (size_t at = 0; at < sizeof windows / sizeof windows[0]; ++at)
    {
        if (windows[at] != nullptr && (IsWindowEnabled(windows[at]) != 0) != enabled)
            EnableWindow(windows[at], enabled);
    }
    if (self->command_layer != nullptr)
        EnableWindow(self->command_layer, self->native_modal_depth == 0);
}

static void refresh_modal_availability(const struct folio_window *_Nonnull self)
{
    enable_surface_background(self);
    refresh_tooltip_rects(self);
    refresh_surface_tooltip_rects(self);
    activate_tooltip(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    accessibility_host_schedule(self->accessibility);
}

/* 配置本体は通常更新とnotice更新が共有する。暗幕はnoticeの高さに依存しない。 */
static void arrange_surface_selection(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_MENU || command_keys(self))
        return;
    if (self->command_surface != COMMAND_SURFACE_THEMES &&
        (settings_surface(self) || command_surface_listing(self->command_surface)))
        self->command_first = 0;
    if (self->command_surface == COMMAND_SURFACE_PALETTE || settings_surface(self) ||
        command_surface_listing(self->command_surface))
        reveal_command_selection(self);
}

static void arrange_command_contents(struct folio_window *_Nonnull self)
{
    enable_surface_background(self);
    if (self->native_modal_depth > 0)
        return;
    if (self->command_layer == nullptr || self->command_input == nullptr)
    {
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        ShowWindow(self->command_layer, SW_HIDE);
        refresh_surface_tooltip_rects(self);
        return;
    }
    RECT layer = command_surface_rect(self);
    MoveWindow(self->command_layer, layer.left, layer.top, layer.right - layer.left,
               layer.bottom - layer.top, TRUE);
    if (!prepare_surface_metrics(self))
    {
        close_command_surface(self);
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    if (command_keys(self))
        self->command_help_scroll = command_help_offset(self);
    place_field(self->command_input, command_input_rect(self), 0);
    arrange_surface_selection(self);
    arrange_replace_inputs(self);
    /* 置換は 2 つの欄を使い、設定と履歴は欄を持たないのでどれも隠す。 */
    bool hidden = self->command_surface == COMMAND_SURFACE_REPLACE || command_keys(self) ||
                  command_surface_listing(self->command_surface);
    ShowWindow(self->command_input, hidden ? SW_HIDE : SW_SHOW);
    ShowWindow(self->command_layer, SW_SHOW);
    if (self->native_modal_depth == 0)
        BringWindowToTop(self->command_layer);
    refresh_surface_tooltip_rects(self);
    redraw_command_layer(self);
}

/* 深さはDialogBoxの前後の再入も塞ぐ。準備失敗でもendまでは深さを保持する。 */
static bool begin_native_modal(struct folio_window *_Nonnull self)
{
    self->native_modal_depth += 1;
    refresh_modal_availability(self);
    return self->settings_backdrop != nullptr && arrange_settings_backdrop(self);
}

/* regionの写しは戻さず、resize/DPIやネスト後の現surfaceから作り直す。 */
static bool end_native_modal(struct folio_window *_Nonnull self)
{
    self->native_modal_depth -= 1;
    bool restored = arrange_settings_backdrop(self);
    if (!restored && self->settings_backdrop != nullptr)
        ShowWindow(self->settings_backdrop, SW_HIDE);
    arrange_command_contents(self);
    refresh_modal_availability(self);
    if (self->native_modal_depth > 0)
    {
        self->modal_arrange_failed = self->modal_arrange_failed || !restored;
        return true;
    }
    bool deferred = self->modal_arrange_failed;
    self->modal_arrange_failed = false;
    return restored && !deferred;
}

/* 復元失敗の通知は暗幕を作り直さない。DialogBox前後の入力guardだけを対にする。 */
static void native_modal_resource_failure(struct folio_window *_Nonnull self)
{
    if (self->close_phase == CLOSE_PHASE_REQUESTED)
        return;
    self->native_modal_depth += 1;
    refresh_modal_availability(self);
    failure_box_show(modal_owner_current(self->modal_owner), FOLIO_STATE_OUT_OF_MEMORY, self->state,
                     nullptr);
    self->native_modal_depth -= 1;
    refresh_modal_availability(self);
}

static struct name_prompt_result
show_name_prompt(struct folio_window *_Nonnull self,
                 const struct name_prompt_request *_Nonnull request)
{
    HWND _Nullable focus = GetFocus();
    if (!begin_native_modal(self))
    {
        (void)end_native_modal(self);
        restore_valid_focus(self, focus);
        return (struct name_prompt_result){FOLIO_STATE_OUT_OF_MEMORY, FOLIO_UNTITLED_PROTECT};
    }
    struct name_prompt_result result = name_prompt_show(
        modal_owner_current(self->modal_owner), request, &self->palette, self->native_metrics);
    if (!end_native_modal(self))
    {
        /* 業務操作の結果を変えず、暗幕なしで資源失敗を一度だけ知らせる。 */
        native_modal_resource_failure(self);
    }
    restore_valid_focus(self, focus);
    if (self->close_phase == CLOSE_PHASE_REQUESTED)
        return (struct name_prompt_result){FOLIO_STATE_CANCELLED, FOLIO_UNTITLED_PROTECT};
    return result;
}

static void show_failure_box(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    HWND _Nullable focus = GetFocus();
    enum folio_command command = self->reporting_command;
    const enum folio_command *_Nullable context = self->command_context ? &command : nullptr;
    /* 暗幕の準備を失敗しても、元の理由は同じnative/OS退避経路で知らせる。 */
    (void)begin_native_modal(self);
    failure_box_show(modal_owner_current(self->modal_owner), outcome, self->state, context);
    if (!end_native_modal(self) && outcome != FOLIO_STATE_OUT_OF_MEMORY)
        native_modal_resource_failure(self);
    restore_valid_focus(self, focus);
}

static LRESULT choose_category_color(struct folio_window *_Nonnull self,
                                     CHOOSECOLORW *_Nonnull choice)
{
    HWND focus = GetFocus();
    if (!begin_native_modal(self))
    {
        (void)end_native_modal(self);
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        restore_valid_focus(self, focus);
        return FALSE;
    }
    choice->hwndOwner = modal_owner_current(self->modal_owner);
    BOOL accepted = ChooseColorW(choice);
    if (!end_native_modal(self))
        native_modal_resource_failure(self);
    restore_valid_focus(self, focus);
    return accepted && self->close_phase != CLOSE_PHASE_REQUESTED;
}

static void arrange_command_input(struct folio_window *_Nonnull self)
{
    activate_tooltip(self);
    if (self->command_layer == nullptr || self->command_input == nullptr)
    {
        return;
    }
    if (!refresh_band_layout(self))
    {
        if (self->native_modal_depth > 0)
        {
            self->modal_arrange_failed = true;
            ShowWindow(self->settings_backdrop, SW_HIDE);
            return;
        }
        close_command_surface(self);
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    if (!arrange_settings_backdrop(self))
    {
        if (self->native_modal_depth > 0)
        {
            self->modal_arrange_failed = true;
            ShowWindow(self->settings_backdrop, SW_HIDE);
            return; /* native内の再配置から失敗箱を再帰させない。 */
        }
        close_command_surface(self);
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    arrange_command_contents(self);
}

/* 印の立て下げと面の配置更新はここだけ。本文の HWND と矩形には触らない。 */
static void set_pane_notice(struct folio_window *_Nonnull self, enum pane_notice notice)
{
    if (self->notice == notice)
    {
        return;
    }
    bool boundary =
        self->notice == PANE_NOTICE_COMMAND_RESULT || notice == PANE_NOTICE_COMMAND_RESULT;
    self->notice = notice;
    accessibility_host_schedule(self->accessibility);
    if (self->command_initializing)
    {
        InvalidateRect(self->handle, nullptr, FALSE);
        return; /* 最終の面・選択・入力が揃ってから同じ配置経路へ渡す。 */
    }
    if (boundary && self->drawer != nullptr)
        arrange_drawer_region(self, regions_for(self).drawer);
    /* 暗幕はclient/cardだけに依存し、notice変更では再確保不要。配置本体には失敗の対話が無い。 */
    arrange_command_contents(self);
    InvalidateRect(self->handle, nullptr, FALSE);
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
    place_field(self->filter_input, drawer_window_filter_rect(self->drawer),
                scale(base_filter_clear_size, GetDpiForWindow(self->handle)));
}

/* 出すのは「number が真・編集モード・文書がある」のときだけ（ADR 0026 の決定 5）。
 * 無題の編集中も出す。UI は第 2 の真偽を持たず、application の値から導く（ARC-004）。 */
static bool gutter_visible(const struct folio_window *_Nonnull self)
{
    return pane_visible(self) && self->pane != nullptr && folio_state_number(self->state) &&
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
    RECT band = {regions_for(self).pane.left, body.top, body.left - scale(base_gutter_gap, dpi),
                 body.bottom};
    return band;
}

/* 番号の帯を描き直す契機を 1 つの関数へ集める（決定 4）。
 * すぐ描く（UpdateWindow）のはスクロールの契機だけで、本文の増減は印と無効化だけにする。 */
static void refresh_gutter(const struct folio_window *_Nonnull self, bool immediate)
{
    if (!pane_visible(self))
    {
        return;
    }
    RECT band = gutter_rect(self);
    InvalidateRect(self->handle, &band, FALSE);
    if (immediate)
    {
        UpdateWindow(self->handle);
    }
}

static void arrange_drawer(const struct folio_window *_Nonnull self, RECT bounds)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    bool visible = bounds.right > bounds.left;
    if (drawer != nullptr)
    {
        show_child(drawer, visible);
        if (visible)
        {
            move_child(drawer, bounds);
        }
    }
    if (self->filter_input != nullptr)
    {
        show_child(self->filter_input, visible);
    }
}

/* 本文の RichEdit を見せるか。文書が無いあいだは隠し、主窓が空状態の案内を描く（決定 13）。 */
static bool body_shown(const struct folio_window *_Nonnull self)
{
    return pane_visible(self) && document_open(self);
}

static void arrange_drawer_region(struct folio_window *_Nonnull self, RECT bounds)
{
    if (folio_state_layout_mode(self->state) == FOLIO_LAYOUT_DRAWER &&
        self->notice == PANE_NOTICE_COMMAND_RESULT)
        bounds.bottom -= pane_notice_height(self);
    arrange_drawer(self, bounds);
}

static RECT body_content_rect(const struct folio_window *_Nonnull self, RECT pane, UINT dpi)
{
    int top = command_band(self) ? self->band.pane_top : scale(base_pane_top, dpi);
    int bottom = command_band(self) ? self->band.pane_bottom : scale(base_pane_bottom, dpi);
    top += pane.top + scale(base_caption_height + base_action_height, dpi);
    top += command_band(self) ? self->band.height : 0;
    return (RECT){pane.left + pane_inset(self, dpi), top, pane.right - scale(base_pane_right, dpi),
                  pane.bottom - bottom};
}

static bool reflow_body(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr || note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY)
        return true;
    return note_pane_reflow(self->pane, folio_state_pane_mode(self->state)) !=
           NOTE_PANE_LOAD_FAILED;
}

static void arrange(struct folio_window *_Nonnull self)
{
    refresh_part_metrics(self, GetDpiForWindow(self->handle));
    normalize_header_target(self);
    arrange_command_input(self); /* 帯高を確定してから同じ高さだけ本文を下げる。 */
    struct folio_regions regions = regions_for(self);
    arrange_drawer_region(self, regions.drawer);
    if (drawer_visible(self))
    {
        arrange_filter_input(self);
    }
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    if (pane != nullptr)
    {
        bool visible = body_shown(self);
        show_child(pane, visible);
        if (visible)
        {
            UINT dpi = GetDpiForWindow(self->handle);
            RECT body = body_content_rect(self, regions.pane, dpi);
            move_child(pane, body);
        }
    }
    if (!reflow_body(self))
    {
        arrange(self); /* UNAVAILABLE now skips reflow; synchronize the existing retry notice. */
        InvalidateRect(self->handle, nullptr, FALSE);
        return;
    }
    refresh_gutter(self, false);
    refresh_tooltip_rects(self);
}

static void arrange_zoomed_pane(struct folio_window *_Nonnull self, size_t first_line)
{
    if (self->pane == nullptr)
        return;
    refresh_gutter_font(self, GetDpiForWindow(self->handle));
    arrange(self);
    note_pane_scroll_to_line(self->pane, first_line);
    InvalidateRect(self->handle, nullptr, FALSE);
    UpdateWindow(note_pane_handle(self->pane));
    refresh_gutter(self, true);
}

static enum folio_state_outcome from_pane_load(enum note_pane_load_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_PANE_LOAD_READY:
        return FOLIO_STATE_READY;
    case NOTE_PANE_LOAD_UNAVAILABLE:
        return FOLIO_STATE_PANE_UNAVAILABLE;
    case NOTE_PANE_LOAD_FAILED:
    case NOTE_PANE_LOAD_BUSY:
        return FOLIO_STATE_PANE_LOAD_FAILED;
    }
    return FOLIO_STATE_PANE_LOAD_FAILED;
}

/* 未準備の通知は状態から導く。高さが変わるときだけ、既存surfaceの下端予約を同期で更新する。 */
static enum folio_state_outcome loaded_pane(struct folio_window *_Nonnull self,
                                            bool was_unavailable,
                                            enum note_pane_load_outcome outcome)
{
    if (was_unavailable != pane_unavailable(self))
    {
        arrange_command_contents(self);
    }
    /* 文書の有無が変わった投入（未選択→選択・ごみ箱へ移した後）は、本文の表示と tooltip の
     * 矩形をここで同期して配り直す。フォーカスを本文へ渡す前に済ませる。 */
    HWND body = note_pane_handle(self->pane);
    bool shown = body != nullptr && (GetWindowLongW(body, GWL_STYLE) & WS_VISIBLE) != 0;
    if (body != nullptr && shown != body_shown(self))
    {
        arrange(self);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    if (outcome == NOTE_PANE_LOAD_READY && self->appearance_recolor_pending)
    {
        PostMessageW(self->handle, folio_message_appearance_changed, 0, 0);
    }
    return from_pane_load(outcome);
}

static enum folio_state_outcome render_pane(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    bool was_unavailable = pane_unavailable(self);
    enum note_pane_load_outcome outcome =
        note_pane_load(self->pane, folio_state_pane_rtf(self->state),
                       folio_state_pane_rtf_length(self->state), PANE_MODE_VIEW);
    return loaded_pane(self, was_unavailable, outcome);
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

/* ASCII 以外の UTF-16 単位を 1 つでも含む行は、GDI のフォントリンクで描く行である。
 * DC に字間（SetTextCharacterExtra ≠ 0）が掛かっているとフォントリンクが壊れるので、
 * その行は字間 0 で測り、字間 0 で描く（ADR 0018 の補正 #260）。 */
static bool font_linked_units(const char16_t *_Nonnull units, int count)
{
    for (int index = 0; index < count; index++)
    {
        if (units[index] >= 0x80)
        {
            return true;
        }
    }
    return false;
}

/* CALCRECTが数えないASCII字間を、既存単行測定と同じ単位数で補う。 */
static SIZE measured_units(HDC device, const char16_t *_Nonnull units, int count, int tracking)
{
    int previous = SetTextCharacterExtra(device, 0);
    RECT box = {0};
    DrawTextW(device, units, count, &box, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    if (previous != tracking_unavailable)
        SetTextCharacterExtra(device, previous);
    return (SIZE){box.right - box.left + count * tracking, box.bottom - box.top};
}

static int wrap_next_unit(const char16_t *_Nonnull units, int count, int at)
{
    bool pair = units[at] >= 0xD800 && units[at] <= 0xDBFF && at + 1 < count &&
                units[at + 1] >= 0xDC00 && units[at + 1] <= 0xDFFF;
    return at + (pair ? 2 : 1);
}

static int wrap_trim(const struct text_wrap *_Nonnull text, int start, int stop)
{
    while (stop > start && (text->units[stop - 1] == u' ' || text->units[stop - 1] == u'\t'))
        stop -= 1;
    return stop;
}

static int wrap_prefix_width(const struct text_wrap *_Nonnull text, int start, int stop)
{
    int trimmed = wrap_trim(text, start, stop);
    return trimmed == start
               ? 0
               : measured_units(text->device, text->units + start, trimmed - start, text->tracking)
                     .cx;
}

/* 改行を越えず、計測する候補をsurrogate pairの後ろへ揃える。 */
static int wrap_boundaries(const struct text_wrap *_Nonnull text, int start, int *_Nonnull ends)
{
    int count = 0;
    while (start < text->count && text->units[start] != u'\r' && text->units[start] != u'\n')
    {
        start = wrap_next_unit(text->units, text->count, start);
        ends[count++] = start;
    }
    return count;
}

static int wrap_probe_position(int estimate, int low, int high)
{
    return estimate <= low ? low + 1 : (estimate >= high ? high - 1 : estimate);
}

/* 実測幅から次の候補を見積もり、収まる端とあふれる端を狭める（ADR0062）。 */
static int wrap_fitting(const struct text_wrap *_Nonnull text, int start, const int *_Nonnull ends,
                        int count)
{
    int width = text->bounds.right - text->bounds.left;
    if (count == 0 || width <= 0)
        return 0;
    int high_width = wrap_prefix_width(text, start, ends[count - 1]);
    if (high_width <= width)
        return count;
    int low = 0;
    int low_width = 0;
    int high = count;
    while (high - low > 1)
    {
        int estimate =
            low + (int)((int64_t)(width - low_width) * (high - low) / (high_width - low_width));
        int next = wrap_probe_position(estimate, low, high);
        int measured = wrap_prefix_width(text, start, ends[next - 1]);
        if (measured <= width)
        {
            low = next;
            low_width = measured;
        }
        else
        {
            high = next;
            high_width = measured;
        }
    }
    return low;
}

static int wrap_word(const struct text_wrap *_Nonnull text, int start, const int *_Nonnull ends,
                     int fitting)
{
    int word = 0;
    for (int index = 0; index < fitting; ++index)
    {
        char16_t unit = text->units[index == 0 ? start : ends[index - 1]];
        if (unit == u' ' || unit == u'\t' || unit >= 0x80)
            word = index + 1;
    }
    return word > 0 ? word : fitting;
}

static int wrapped_line_end(const struct text_wrap *_Nonnull text, int start,
                            bool *_Nonnull newline)
{
    int ends[draw_unit_limit];
    int count = wrap_boundaries(text, start, ends);
    int fitting = wrap_fitting(text, start, ends, count);
    if (fitting == count)
    {
        int stop = count == 0 ? start : ends[count - 1];
        *newline = stop < text->count;
        return stop;
    }
    *newline = false;
    int chosen = wrap_word(text, start, ends, fitting);
    /* 文脈依存の字形ではprefix幅の単調性を仮定せず、採用行を必ず実測する。 */
    while (chosen > 0 && wrap_prefix_width(text, start, ends[chosen - 1]) >
                             text->bounds.right - text->bounds.left)
        chosen -= 1;
    return chosen == 0 ? start : ends[chosen - 1];
}

static int wrapped_line(const struct text_wrap *_Nonnull text, int start, int stop, int offset)
{
    int trimmed = wrap_trim(text, start, stop);
    SIZE measured = trimmed == start ? measured_units(text->device, u" ", 1, text->tracking)
                                     : measured_units(text->device, text->units + start,
                                                      trimmed - start, text->tracking);
    if (text->paint && trimmed > start)
    {
        RECT line = {text->bounds.left, text->bounds.top + offset, text->bounds.right,
                     text->bounds.top + offset + measured.cy};
        DrawTextW(text->device, text->units + start, trimmed - start, &line,
                  DT_SINGLELINE | DT_NOPREFIX);
    }
    return measured.cy;
}

static int wrap_advance(const struct text_wrap *_Nonnull text, int stop, bool newline)
{
    int at = stop;
    if (newline)
    {
        char16_t first = text->units[at++];
        if (first == u'\r' && at < text->count && text->units[at] == u'\n')
            at += 1;
    }
    while (at < text->count && (text->units[at] == u' ' || text->units[at] == u'\t'))
        at += 1;
    return at;
}

/* 測定/描画で同じ行を選び、元文字列のfont-link規則を全折返し行へ維持する。 */
static int wrapped_units(const struct text_wrap *_Nonnull text)
{
    int start = 0;
    int height = 0;
    while (start < text->count)
    {
        bool newline = false;
        int stop = wrapped_line_end(text, start, &newline);
        if (stop == start && !newline)
            return 0;
        int line = wrapped_line(text, start, stop, height);
        if (line <= 0)
            return 0;
        if (text->maximum_line_height != nullptr && line > *text->maximum_line_height)
            *text->maximum_line_height = line;
        height += line;
        start = wrap_advance(text, stop, newline);
    }
    return height;
}

/* UTF8変換とfont-linkの切替は従来の単行/新しい折返しで共有する。 */
static int draw_text(HDC device, const char *_Nonnull text,
                     const struct text_draw_request *_Nonnull request)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(text, units);
    if (count == 0)
        return 0;
    bool linked = font_linked_units(units, count);
    int previous = linked ? SetTextCharacterExtra(device, 0) : 0;
    int tracking = GetTextCharacterExtra(device);
    struct text_wrap wrap = {.device = device,
                             .units = units,
                             .count = count,
                             .bounds = request->bounds,
                             .tracking = tracking == tracking_unavailable ? 0 : tracking,
                             .paint = (request->format & DT_CALCRECT) == 0,
                             .maximum_line_height = request->maximum_line_height};
    RECT bounds = request->bounds;
    int height = request->wrapped ? wrapped_units(&wrap)
                                  : DrawTextW(device, units, count, &bounds, request->format);
    if (linked)
        SetTextCharacterExtra(device, previous == tracking_unavailable ? 0 : previous);
    return height;
}

static int draw_units(HDC device, const char *_Nonnull text, RECT bounds, UINT format)
{
    struct text_draw_request request = {.bounds = bounds, .format = format};
    return draw_text(device, text, &request);
}

static int wrapped_text(HDC device, const char *_Nonnull text, RECT bounds, bool paint)
{
    struct text_draw_request request = {
        .bounds = bounds, .format = paint ? 0 : DT_CALCRECT, .wrapped = true};
    return draw_text(device, text, &request);
}

/* UTF-8 の 1 行を、描く経路 draw_units と同じ字間で測る。測れなければ 0。
 * 字間 0 で DT_CALCRECT を取り、ASCII だけの行のときだけ単位数 × 字間を足す
 * （Consolas だけの行では DT_CALCRECT が字間を含めない）。フォントリンクで描く行は
 * 字間 0 で描くので、DT_CALCRECT の値そのままが描いた幅になる（ADR 0018 の補正 #260）。 */
static int measure_utf8(HDC device, const char *_Nonnull text)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(text, units);
    if (count == 0)
    {
        return 0;
    }
    int tracking = GetTextCharacterExtra(device);
    tracking = tracking == tracking_unavailable ? 0 : tracking;
    return measured_units(device, units, count, font_linked_units(units, count) ? 0 : tracking).cx;
}

/* UTF-8 を 1 行で描く。bounds に収まらなければ末尾を省略記号にする（ADR 0011 の決定 4）。 */
static void draw_utf8(HDC device, const char *_Nonnull text, RECT bounds)
{
    draw_units(device, text, bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* 右ペインの頭の帯（窓を掴んで動かせる帯）。 */
static RECT caption_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT pane = regions_for(self).pane;
    RECT caption = {pane.left + scale(base_pane_left, dpi), pane.top, client.right,
                    pane.top + scale(base_caption_height, dpi)};
    return caption;
}

/* 共通ヘッダーの部品は 44 の帯の中央に高さ 28 で置く（8+28+8・ADR0060 の決定 2）。 */
static RECT header_part_rect(const struct folio_window *_Nonnull self, int left, int width)
{
    RECT header = regions_for(self).header;
    int size = scale(ui_part_height, GetDpiForWindow(self->handle));
    int top = (header.top + header.bottom - size) / 2;
    return (RECT){left, top, left + width, top + size};
}

/* 窓の × は C 様式（当たり 28×28）。窓の縁から 16（ui_part_edge）。 */
static RECT close_rect(const struct folio_window *_Nonnull self)
{
    RECT header = regions_for(self).header;
    UINT dpi = GetDpiForWindow(self->handle);
    int size = scale(ui_part_height, dpi);
    return header_part_rect(self, header.right - scale(ui_part_edge, dpi) - size, size);
}

/* ×から左へmax/restore、min、設定、helpを同じ寸法で導く（ADR0075）。 */
static RECT header_offset_rect(const struct folio_window *_Nonnull self, int index)
{
    RECT bounds = close_rect(self);
    int shift = scale(ui_part_height + base_settings_gap, GetDpiForWindow(self->handle));
    OffsetRect(&bounds, -shift * index, 0);
    return bounds;
}

static RECT maximize_rect(const struct folio_window *_Nonnull self)
{
    return header_offset_rect(self, 1);
}

static RECT minimize_rect(const struct folio_window *_Nonnull self)
{
    return header_offset_rect(self, 2);
}

static RECT settings_rect(const struct folio_window *_Nonnull self)
{
    return header_offset_rect(self, 3);
}

static RECT help_rect(const struct folio_window *_Nonnull self)
{
    return header_offset_rect(self, 4);
}

/* 同じDPI済み残量から3択/chooserを決める。移動24は新規の最小icon幅の外側。 */
static struct layout_metrics layout_metrics_for(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT client;
    GetClientRect(self->handle, &client);
    if (client.right >= scale(base_min_width, dpi))
        return (struct layout_metrics){base_layout_button_width, base_layout_help_gap, false};
    const struct layout_metrics steps[] = {
        {base_layout_button_compact_width, base_layout_help_gap, false},
        {base_layout_button_compact_width, base_layout_help_compact_gap, false},
        {base_layout_button_narrow_width, base_layout_help_compact_gap, false}};
    int minimum = scale(ui_part_edge + ui_part_height + base_new_note_drag_gap, dpi);
    for (size_t at = 0; at < sizeof steps / sizeof steps[0]; ++at)
    {
        int left = help_rect(self).left - scale(steps[at].gap, dpi) -
                   scale(steps[at].width, dpi) * (int)layout_button_count();
        if (left >= minimum)
            return steps[at];
    }
    return (struct layout_metrics){52, base_layout_help_compact_gap, true};
}

static int layout_button_width(const struct folio_window *_Nonnull self)
{
    return scale(layout_metrics_for(self).width, GetDpiForWindow(self->handle));
}

/* 選択群の範囲（項目の高さ 28）。外枠は持たず、項目間の 1px の区切りと選択中の塗り＋棒だけで
 * 示す（枠は押すと実行する A 様式と入力欄だけ・#349）。表示3択と閲覧/編集が同じ形を使う。 */
static RECT choice_group_rect(int right, int middle, int items, UINT dpi)
{
    int height = scale(ui_part_height, dpi);
    int top = middle - height / 2;
    return (RECT){right - items, top, right, top + height};
}

/* 群の index 番の項目。項目は等幅で、間は 0（区切りは項目の左端の 1px）。 */
static RECT choice_item_rect(RECT group, int width, size_t index)
{
    int left = group.left + width * (int)index;
    return (RECT){left, group.top, left + width, group.bottom};
}

/* 隠れた群とは別のstateを持たず、現在の表示slotを全利用者へ返す。 */
static RECT layout_slot_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT header = regions_for(self).header;
    struct layout_metrics metrics = layout_metrics_for(self);
    int right = help_rect(self).left - scale(metrics.gap, dpi);
    int count = metrics.chooser ? 1 : (int)layout_button_count();
    return choice_group_rect(right, (header.top + header.bottom) / 2,
                             scale(metrics.width, dpi) * count, dpi);
}

static RECT layout_group_rect(const struct folio_window *_Nonnull self)
{
    return layout_metrics_for(self).chooser ? (RECT){0} : layout_slot_rect(self);
}

static RECT layout_chooser_rect(const struct folio_window *_Nonnull self)
{
    return layout_metrics_for(self).chooser ? layout_slot_rect(self) : (RECT){0};
}

/* 字の幅は描画と同じ等幅の書体と字間で測る。測れなければ 0。 */
static int measure_part_text_uncached(const struct folio_window *_Nonnull self, enum ui_text text)
{
    HDC device = GetDC(self->handle);
    if (device == nullptr)
    {
        return 0;
    }
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, GetDpiForWindow(self->handle)));
    int width = measure_utf8(device, ui_text_line(text, folio_state_language(self->state)));
    SetTextCharacterExtra(device, 0);
    SelectObject(device, previous);
    ReleaseDC(self->handle, device);
    return width;
}

static const enum accessible_kind measured_parts[] = {ACCESSIBLE_NEW_NOTE, ACCESSIBLE_SAVE,
                                                      ACCESSIBLE_FIND, ACCESSIBLE_MENU};

static_assert(sizeof measured_parts / sizeof measured_parts[0] ==
              sizeof(((struct ui_part_metrics){0}).widths) / sizeof(int));

static void refresh_part_metrics(struct folio_window *_Nonnull self, UINT dpi)
{
    enum folio_language language = folio_state_language(self->state);
    int tracking = scale(base_tracking, dpi);
    const struct ui_part_metrics *_Nonnull current = &self->part_metrics;
    if (current->valid && current->font == self->mono_font && current->dpi == dpi &&
        current->language == language && current->tracking == tracking)
        return;
    struct ui_part_metrics metrics = {
        .font = self->mono_font, .dpi = dpi, .language = language, .tracking = tracking};
    if (self->mono_font == nullptr)
    {
        self->part_metrics.valid = false;
        return;
    }
    for (size_t at = 0; at < sizeof measured_parts / sizeof measured_parts[0]; ++at)
    {
        metrics.widths[at] =
            measure_part_text_uncached(self, accessible_controls[measured_parts[at]].label);
        if (metrics.widths[at] <= 0)
        {
            self->part_metrics.valid = false;
            return;
        }
    }
    metrics.valid = true;
    self->part_metrics = metrics;
}

static int measure_part_text(const struct folio_window *_Nonnull self, enum ui_text text)
{
    const struct ui_part_metrics *_Nonnull metrics = &self->part_metrics;
    UINT dpi = GetDpiForWindow(self->handle);
    bool current = metrics->valid && metrics->font == self->mono_font && metrics->dpi == dpi &&
                   metrics->tracking == scale(base_tracking, dpi) &&
                   metrics->language == folio_state_language(self->state);
    if (current)
    {
        for (size_t at = 0; at < sizeof measured_parts / sizeof measured_parts[0]; ++at)
        {
            if (accessible_controls[measured_parts[at]].label == text)
                return metrics->widths[at];
        }
    }
    return measure_part_text_uncached(self, text);
}

/* 新しいノートは A 様式の単独の釦。狭幅では字を落として絵だけ（28×28）に縮め、消さない。
 * 文字を省く境界も描画/hit/tooltip/UIAで共有する。狭幅でも右の表示群との移動領域を残す。 */
static RECT new_note_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int size = scale(ui_part_height, dpi);
    int left = scale(ui_part_edge, dpi);
    int room = layout_slot_rect(self).left - left - scale(base_new_note_drag_gap, dpi);
    int needed = ui_part_command_width(measure_part_text(self, UI_TEXT_COMMAND_NEW), true, dpi);
    return header_part_rect(self, left, needed <= room ? needed : size);
}

static bool new_note_labeled(const struct folio_window *_Nonnull self)
{
    RECT bounds = new_note_rect(self);
    return bounds.right - bounds.left > bounds.bottom - bounds.top;
}

static RECT layout_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    return layout_metrics_for(self).chooser
               ? (RECT){0}
               : choice_item_rect(layout_group_rect(self), layout_button_width(self), index);
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

static enum accessible_kind mode_kind(enum pane_mode mode)
{
    switch (mode)
    {
    case PANE_MODE_VIEW:
        return ACCESSIBLE_VIEW;
    case PANE_MODE_EDIT:
        return ACCESSIBLE_EDIT;
    }
    return ACCESSIBLE_VIEW;
}

/* 閲覧/編集は表示3択と同じ S 様式の群（ADR0060 の決定 1）。位置はパンくず段の右端のまま。
 * 項目は 3 言語で同じ 4 の倍数の固定幅（決定 2）。描画と当たり判定はこの 1 か所を共有する。 */
static int mode_item_width(const struct folio_window *_Nonnull self)
{
    return scale(base_mode_item_width, GetDpiForWindow(self->handle));
}

static RECT mode_group_rect(const struct folio_window *_Nonnull self)
{
    RECT caption = caption_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    int right = caption.right - scale(ui_part_edge, dpi);
    return choice_group_rect(right, (caption.top + caption.bottom) / 2, mode_item_width(self) * 2,
                             dpi);
}

/* 表示3択の選択中の項目。 */
static enum accessible_kind selected_layout_kind(const struct folio_window *_Nonnull self)
{
    for (size_t index = 0; index < layout_button_count(); ++index)
    {
        if (folio_state_layout_mode(self->state) == layout_buttons[index].mode)
        {
            return layout_buttons[index].kind;
        }
    }
    return ACCESSIBLE_LAYOUT_BOTH;
}

/* 実root focusが隠れた表示入口にいる時だけ写す。他HWNDへは触れない。 */
static void normalize_header_target(struct folio_window *_Nonnull self)
{
    if (GetFocus() != self->handle)
        return;
    bool chooser = layout_metrics_for(self).chooser;
    enum accessible_kind kind = self->keyboard_target;
    bool hidden_choice = chooser && accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP;
    bool hidden_chooser = !chooser && kind == ACCESSIBLE_LAYOUT_CHOOSER;
    if (!hidden_choice && !hidden_chooser)
        return;
    cancel_part_press(self);
    self->keyboard_target = chooser ? ACCESSIBLE_LAYOUT_CHOOSER : selected_layout_kind(self);
    accessibility_host_schedule(self->accessibility);
}

/* 選択群の描画・UIA IsSelected・押下は同じ選択元を使う。 */
static bool part_selected(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP)
    {
        return selected_layout_kind(self) == kind;
    }
    bool chip = kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT;
    return chip && mode_kind(folio_state_pane_mode(self->state)) == kind;
}

static RECT mode_item_rect(const struct folio_window *_Nonnull self, enum pane_mode mode)
{
    size_t index = mode == PANE_MODE_EDIT ? 1 : 0;
    return choice_item_rect(mode_group_rect(self), mode_item_width(self), index);
}

/* パンくずのノート区画に実際に描く文字。幅の見積りも描画もこの 1 本を使う（ADR 0022 の決定 2）。 */
static const char *_Nonnull breadcrumb_note(struct pane_title_view title,
                                            enum folio_language language)
{
    return title.recovering == PANE_RECOVERY_NOTE ? ui_text_line(UI_TEXT_TITLE_RECOVERING, language)
                                                  : title.note;
}

static const char *_Nonnull breadcrumb_category(struct pane_title_view title,
                                                enum folio_language language)
{
    return title.recovering == PANE_RECOVERY_CATEGORY
               ? ui_text_line(UI_TEXT_TITLE_CATEGORY_RECOVERING, language)
               : title.category;
}

/* 未保存印に取る幅（間＋丸）。保存済みなら 0 で、パンくずの幅の割当は従来と同じ。 */
static int unsaved_mark_room(const struct folio_window *_Nonnull self)
{
    return self->unsaved
               ? scale(base_unsaved_gap + base_unsaved_mark, GetDpiForWindow(self->handle))
               : 0;
}

static struct breadcrumb_layout breadcrumb_cells(const struct folio_window *_Nonnull self,
                                                 HDC device, struct pane_title_view title,
                                                 RECT caption)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int right = mode_group_rect(self).left - scale(ui_part_group_gap, dpi);
    int available = right - caption.left;
    bool compact = available < scale(base_breadcrumb_compact_width, dpi);
    int padding = scale(compact ? base_breadcrumb_compact_padding : base_breadcrumb_padding, dpi);
    int tip = scale(compact ? base_breadcrumb_compact_tip : base_breadcrumb_tip, dpi);
    char digits[3];
    ordinal_label(title.ordinal, digits);
    int ordinal = measure_utf8(device, digits) + padding * 2;
    int category =
        measure_utf8(device, breadcrumb_category(title, folio_state_language(self->state)));
    int note = measure_utf8(device, breadcrumb_note(title, folio_state_language(self->state))) +
               unsaved_mark_room(self);
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

/* ノート名の直後の 8px の丸（chip_background・ADR0060 の決定 8）。left は字の右端。 */
static void draw_unsaved_mark(const struct folio_window *_Nonnull self, HDC device, int left,
                              int middle)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int size = scale(base_unsaved_mark, dpi);
    int x = left + scale(base_unsaved_gap, dpi);
    int y = middle - size / 2;
    HGDIOBJ brush = SelectObject(device, GetStockObject(DC_BRUSH));
    HGDIOBJ pen = SelectObject(device, GetStockObject(NULL_PEN));
    SetDCBrushColor(device, self->palette.unsaved_fill); /* 保存釦の B と同じ合図の色 */
    /* NULL_PEN の Ellipse は右下の 1px を塗らないので、箱を 1px 広げて 8px の丸にする。 */
    Ellipse(device, x, y, x + size + 1, y + size + 1);
    SelectObject(device, pen);
    SelectObject(device, brush);
}

/* ノート名と、変更ありの間だけその直後の未保存印。印の幅はノート名の区画が先に取ってある。 */
static void draw_breadcrumb_note(const struct folio_window *_Nonnull self, HDC device,
                                 const char *_Nonnull note,
                                 const struct breadcrumb_layout *_Nonnull cells)
{
    RECT label = cells->note;
    label.right -= unsaved_mark_room(self);
    int inset = cells->padding + cells->tip;
    draw_breadcrumb_label(device, note, label, inset);
    if (!self->unsaved)
    {
        return;
    }
    int room = label.right - label.left - inset;
    int text = measure_utf8(device, note);
    int drawn = text < room ? text : room;
    draw_unsaved_mark(self, device, label.left + inset + (drawn > 0 ? drawn : 0),
                      (label.top + label.bottom) / 2);
}

/* パンくずの描いた範囲（UIA の Text の矩形）。番号からノート名の区画の右端まで。 */
static RECT breadcrumb_rect(const struct folio_window *_Nonnull self)
{
    struct pane_title_view title = folio_state_pane_title(self->state);
    HDC device = title.any ? GetDC(self->handle) : nullptr;
    if (device == nullptr)
    {
        return (RECT){0};
    }
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, GetDpiForWindow(self->handle)));
    struct breadcrumb_layout cells = breadcrumb_cells(self, device, title, caption_rect(self));
    SetTextCharacterExtra(device, 0);
    SelectObject(device, previous);
    ReleaseDC(self->handle, device);
    int right =
        mode_group_rect(self).left - scale(ui_part_group_gap, GetDpiForWindow(self->handle));
    return (RECT){cells.ordinal.left, cells.ordinal.top,
                  cells.note.right < right ? cells.note.right : right, cells.ordinal.bottom};
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
    int right =
        mode_group_rect(self).left - scale(ui_part_group_gap, GetDpiForWindow(self->handle));
    IntersectClipRect(device, caption.left, caption.top, right, caption.bottom);
    if (cells.category.right > cells.category.left)
    {
        draw_breadcrumb_segment(device, cells.category, cells.tip,
                                self->palette.breadcrumb_background);
        SetTextColor(device, self->palette.breadcrumb_text);
        draw_breadcrumb_label(device, breadcrumb_category(title, folio_state_language(self->state)),
                              cells.category, cells.padding + cells.tip);
    }
    bool contrast = self->palette.contrast == FOLIO_CONTRAST_HIGH;
    COLORREF color = contrast ? self->palette.chip_background
                              : RGB(title.color.red, title.color.green, title.color.blue);
    draw_breadcrumb_segment(device, cells.ordinal, cells.tip, color);
    SetTextColor(device, contrast ? self->palette.chip_text : folio_palette_ink(color));
    char digits[3];
    ordinal_label(title.ordinal, digits);
    draw_breadcrumb_label(device, digits, cells.ordinal, cells.padding);
    /* 復旧待ちの言い換えはここ 1 か所だけが持つ。application は実名を返す（ADR 0022 の決定 2）。 */
    SetTextColor(device, !contrast && title.recovering == PANE_RECOVERY_NOTE
                             ? self->palette.selected_text
                             : self->palette.current_text);
    draw_breadcrumb_note(self, device, breadcrumb_note(title, folio_state_language(self->state)),
                         &cells);
    RestoreDC(device, saved);
}

/* キーボードで動かした後（または OS が常に印を出す設定）だけ focus を描く（ADR0060 の決定 3）。 */
static bool part_focus_shown(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    return (self->focus_cues || self->focus_cues_always) && self->keyboard_target == kind &&
           GetFocus() == self->handle;
}

/* 部品の表示値。矩形は UIA・focus・当たり判定と同じ accessible_main_rect から取る。 */
static struct ui_part_paint part_paint(const struct folio_window *_Nonnull self,
                                       enum accessible_kind kind, enum ui_part_style style)
{
    return (struct ui_part_paint){
        .bounds = accessible_main_rect(self, kind),
        .style = style,
        .dpi = GetDpiForWindow(self->handle),
        .grouped = style == UI_PART_STYLE_CHOICE,
        .labeled = style != UI_PART_STYLE_ICON,
        .hovered = part_enabled(self, kind) && self->hover_part == kind &&
                   self->pressed_part == ACCESSIBLE_WINDOW,
        .pressed = self->pressed_part == kind && self->pressed_inside,
        .focused = part_focus_shown(self, kind),
        .disabled = !part_enabled(self, kind),
    };
}

static enum ui_text part_label(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_MAXIMIZE)
        return IsZoomed(self->handle) ? UI_TEXT_TOOLTIP_RESTORE : UI_TEXT_TOOLTIP_MAXIMIZE;
    return accessible_controls[kind].label;
}

static void layout_chooser_name(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    enum ui_text label = accessible_controls[selected_layout_kind(self)].label;
    struct ui_text_request request = {
        .id = UI_TEXT_LAYOUT_CHOOSER, .language = language, .name = ui_text_line(label, language)};
    if (ui_text_format(&request, out, 1024) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

/* 描く字。表示名＝UIA Name の元（accessible_controls）から引き、「操作」だけは表示の記号▾の
 * 付いた字を描く（ADR0060 の補正 2。▾は UIA 名に入れない）。測る経路もこの 1 本を使う。 */
static enum ui_text part_display_text(enum accessible_kind kind)
{
    return accessible_controls[kind].label;
}

/* 部品の字は表示名＝UIA Name の 1 つの元（accessible_controls）から引く。 */
static void draw_part_label(const struct folio_window *_Nonnull self, HDC device,
                            enum accessible_kind kind, struct ui_part_ink ink)
{
    UINT align = kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT ||
                         accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP
                     ? DT_CENTER
                     : DT_LEFT;
    SetTextColor(device, ink.text);
    draw_units(device, ui_text_line(part_display_text(kind), folio_state_language(self->state)),
               ink.label, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

static void draw_choice_item(const struct folio_window *_Nonnull self, HDC device,
                             enum accessible_kind kind, bool selected)
{
    struct ui_part_paint part = part_paint(self, kind, UI_PART_STYLE_CHOICE);
    part.leading_rule = kind != ACCESSIBLE_VIEW && kind != ACCESSIBLE_LAYOUT_DRAWER;
    part.selected = selected;
    draw_part_label(self, device, kind, ui_part_draw(device, &self->palette, &part));
}

static void draw_mode_group(const struct folio_window *_Nonnull self, HDC device)
{
    enum pane_mode mode = folio_state_pane_mode(self->state);
    draw_choice_item(self, device, ACCESSIBLE_VIEW, mode == PANE_MODE_VIEW);
    draw_choice_item(self, device, ACCESSIBLE_EDIT, mode == PANE_MODE_EDIT);
}

/* 歯車と × は C 様式（絵は面のパス・字形は使わない・CNF-010）。 */
static void draw_icon_part(const struct folio_window *_Nonnull self, HDC device,
                           enum accessible_kind kind, enum icon_paint_kind icon)
{
    struct ui_part_paint part = part_paint(self, kind, UI_PART_STYLE_ICON);
    part.has_icon = true;
    part.icon = icon;
    ui_part_draw(device, &self->palette, &part);
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

/* EDIT初期化中は同期の再入から候補を確定しない（ADR0065）。 */
static HRESULT prepare_palette_candidates(const struct folio_window *_Nonnull self)
{
    if (self->command_initializing)
        return accessibility_node_unavailable;
    if (self->command_candidates == nullptr)
        return E_OUTOFMEMORY;
    enum folio_language language = folio_state_language(self->state);
    if (command_candidates_current(self->command_candidates, language, self->surface_revision))
        return S_OK;
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        command_candidates_invalidate(self->command_candidates);
        return E_OUTOFMEMORY;
    }
    command_candidates_prepare(self->command_candidates, query, language, self->surface_revision);
    utf8_text_destroy(query);
    return S_OK;
}

static bool palette_command_at(const struct folio_window *_Nonnull self, size_t index,
                               enum folio_command *_Nonnull out)
{
    return prepare_palette_candidates(self) == S_OK &&
           command_candidates_at(self->command_candidates, index, out);
}

static size_t palette_matches_count(const struct folio_window *_Nonnull self)
{
    return prepare_palette_candidates(self) == S_OK
               ? command_candidates_count(self->command_candidates)
               : 0;
}

static RECT palette_candidate_rect(const struct folio_window *_Nonnull self, size_t first,
                                   size_t wanted)
{
    if (wanted < first || prepare_palette_candidates(self) != S_OK)
        return (RECT){0};
    size_t first_ordinal = 0;
    size_t wanted_ordinal = 0;
    if (!command_candidates_ordinal(self->command_candidates, COMMAND_CANDIDATE_COMMAND, first,
                                    &first_ordinal) ||
        !command_candidates_ordinal(self->command_candidates, COMMAND_CANDIDATE_COMMAND, wanted,
                                    &wanted_ordinal))
        return (RECT){0};
    RECT rows = command_rows_rect(self);
    int height = scale(base_command_row_height, GetDpiForWindow(self->handle));
    int top = rows.top + (int)(wanted_ordinal - first_ordinal + 1) * height;
    return top + height <= rows.bottom ? (RECT){rows.left, top, rows.right, top + height}
                                       : (RECT){0};
}

static size_t palette_visible_count(const struct folio_window *_Nonnull self)
{
    if (prepare_palette_candidates(self) != S_OK)
        return 0;
    size_t count = 0;
    RECT row = palette_candidate_rect(self, self->command_first, self->command_first);
    while (!IsRectEmpty(&row))
    {
        count += 1;
        row = palette_candidate_rect(self, self->command_first, self->command_first + count);
    }
    return count;
}

static size_t palette_scroll_limit(const struct folio_window *_Nonnull self, size_t count)
{
    if (count == 0)
        return 0;
    if (prepare_palette_candidates(self) != S_OK)
        return 0;
    size_t first = 0;
    RECT last = palette_candidate_rect(self, first, count - 1);
    while (IsRectEmpty(&last) && first < count - 1)
    {
        first += 1;
        last = palette_candidate_rect(self, first, count - 1);
    }
    return first;
}

static void reveal_palette_selection(struct folio_window *_Nonnull self)
{
    if (prepare_palette_candidates(self) != S_OK)
        return;
    if (self->command_selection < self->command_first)
        self->command_first = self->command_selection;
    RECT row = palette_candidate_rect(self, self->command_first, self->command_selection);
    while (IsRectEmpty(&row) && self->command_first < self->command_selection)
    {
        self->command_first += 1;
        row = palette_candidate_rect(self, self->command_first, self->command_selection);
    }
}

static int layout_command_index(enum folio_command command)
{
    for (size_t index = 0; index < layout_button_count(); ++index)
    {
        if (layout_buttons[index].command == command)
        {
            return (int)index;
        }
    }
    return -1;
}

static void layout_phrase(const struct folio_window *_Nonnull self, size_t index,
                          enum ui_text format, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    struct ui_text_request request = {
        .id = format,
        .language = language,
        .name = folio_command_label(layout_buttons[index].command, language),
        .from = folio_shortcut_command_hint(layout_buttons[index].command, language),
        .to = layout_buttons[index].mode == FOLIO_LAYOUT_DRAWER
                  ? ui_text_line(UI_TEXT_LAYOUT_NARROW_HINT, language)
                  : ""};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
    {
        out[0] = '\0';
    }
}

static void layout_chooser_help(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    size_t used = 0;
    out[0] = '\0';
    for (size_t at = 0; at < layout_button_count(); ++at)
    {
        char line[draw_unit_limit];
        layout_phrase(self, at, UI_TEXT_LAYOUT_HELP, line);
        if (used + strlen(line) + 2 > draw_unit_limit)
        {
            out[0] = '\0';
            return;
        }
        if (used != 0)
            used = append_text(out, used, "\n");
        used = append_text(out, used, line);
    }
    out[used] = '\0';
}

static void command_help_text(const struct folio_window *_Nonnull self, size_t slot,
                              char *_Nonnull out)
{
    size_t generic = sizeof command_shortcuts / sizeof command_shortcuts[0];
    out[0] = '\0';
    if (slot < generic)
    {
        if (folio_shortcut_help(command_shortcuts[slot], folio_state_language(self->state), out,
                                draw_unit_limit) != UI_TEXT_FORMAT_READY)
            out[0] = '\0';
        return;
    }
    layout_phrase(self, slot - generic, UI_TEXT_LAYOUT_HELP, out);
}

static bool measure_help_metrics(const struct folio_window *_Nonnull self, HDC device,
                                 struct command_help_metrics *_Nonnull metrics)
{
    RECT bounds = {0, 0, metrics->width, 0};
    for (size_t index = 0; index < metrics->count; ++index)
    {
        char text[draw_unit_limit];
        command_help_text(self, index, text);
        struct text_draw_request request = {.bounds = bounds,
                                            .format = DT_CALCRECT,
                                            .wrapped = true,
                                            .maximum_line_height = &metrics->maximum_line_height};
        int height = draw_text(device, text, &request);
        if (height <= 0)
            return false;
        metrics->offsets[index + 1] = metrics->offsets[index] + height + scale(8, metrics->dpi);
    }
    return metrics->maximum_line_height > 0;
}

static bool prepare_help_metrics(struct folio_window *_Nonnull self)
{
    RECT bounds = command_body_rect(self);
    struct command_help_metrics metrics = {.width = bounds.right - bounds.left,
                                           .dpi = GetDpiForWindow(self->handle),
                                           .font = self->mono_font,
                                           .language = folio_state_language(self->state),
                                           .count = (size_t)command_help_rows(self)};
    if (self->help_metrics.valid && self->help_metrics.width == metrics.width &&
        self->help_metrics.dpi == metrics.dpi && self->help_metrics.font == metrics.font &&
        self->help_metrics.language == metrics.language)
        return true;
    HDC device = GetDC(self->command_layer);
    if (device == nullptr)
        return false;
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, metrics.dpi));
    bool ready = measure_help_metrics(self, device, &metrics);
    SelectObject(device, previous);
    ReleaseDC(self->command_layer, device);
    if (ready)
    {
        metrics.valid = true;
        self->help_metrics = metrics;
    }
    return ready;
}

static RECT command_help_geometry(const struct folio_window *_Nonnull self, size_t slot,
                                  int *_Nullable maximum_line_height)
{
    if (!self->help_metrics.valid || slot >= self->help_metrics.count)
        return (RECT){0};
    RECT row = command_body_rect(self);
    int top = row.top;
    row.top = top + self->help_metrics.offsets[slot];
    row.bottom = top + self->help_metrics.offsets[slot + 1];
    if (maximum_line_height != nullptr)
        *maximum_line_height = self->help_metrics.maximum_line_height;
    return row;
}

static int command_help_total(const struct folio_window *_Nonnull self)
{
    if (!command_keys(self) || !self->help_metrics.valid)
        return 0;
    return self->help_metrics.offsets[self->help_metrics.count];
}

/* 実際の折返し行を一行以上重ね、ページ境界で文字行を読めなくしない。 */
static size_t command_help_step(const struct folio_window *_Nonnull self)
{
    int step = command_visible_help(self) - self->help_metrics.maximum_line_height;
    return step > 0 ? (size_t)step : 1;
}

static void draw_layout_command(const struct folio_window *_Nonnull self, HDC device,
                                struct command_row row, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT line = row.bounds;
    line.left += scale(base_command_row_padding, dpi);
    line.right -= scale(base_command_alias_room, dpi);
    enum folio_language language = folio_state_language(self->state);
    draw_utf8(device, folio_command_label(row.command, language), line);
    line.left = line.right;
    line.right = row.bounds.right - scale(base_command_row_padding, dpi);
    draw_utf8(device, folio_shortcut_command_hint(layout_buttons[index].command, language), line);
}

static void draw_aliases(const struct folio_window *_Nonnull self, HDC device,
                         enum folio_command command, RECT bounds)
{
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
    SetTextColor(device, row.selected ? self->palette.selected_text : self->palette.current_text);
    int layout = layout_command_index(row.command);
    if (layout >= 0)
    {
        draw_layout_command(self, device, row, (size_t)layout);
        return;
    }
    RECT label = row.bounds;
    UINT dpi = GetDpiForWindow(self->handle);
    label.left += scale(base_command_row_padding, dpi);
    label.right -= scale(base_command_alias_room, dpi);
    SetTextColor(device, row.selected ? self->palette.selected_text : self->palette.current_text);
    char caption[draw_unit_limit];
    if (folio_command_caption(row.command, folio_state_language(self->state), caption,
                              sizeof caption) == UI_TEXT_FORMAT_READY)
        draw_utf8(device, caption, label);
    RECT aliases = row.bounds;
    aliases.right -= scale(base_command_row_padding, dpi);
    aliases.left = aliases.right;
    SetTextColor(device, row.selected ? self->palette.selected_text : self->palette.header_text);
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

/* 結果は対象/Enter方向と別の矩形へ出す。狭幅でも失敗理由に行の全幅を使う（ADR0052）。 */
static void search_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    struct ui_text_request request = {.id = search_result_line(self->search_outcome),
                                      .language = language,
                                      .k = self->search_ordinal,
                                      .n = self->search_total};
    if (ui_text_format(&request, out, search_status_capacity) != UI_TEXT_FORMAT_READY)
    {
        /* 表の値は単体で収まりが固定される。 */
        out[0] = '\0';
    }
}

/* 入力欄の箱を EDIT の地で塗り、内側に静止 1px / focus 2px の枠を描く（ADR0060 の決定 3・#349）。
 */
static void draw_input_field(const struct folio_window *_Nonnull self, HDC device,
                             HWND _Nullable edit, RECT box)
{
    if (edit == nullptr || !IsWindowVisible(edit))
    {
        return;
    }
    int focus = GetFocus() == edit ? ui_part_focus_width(GetDpiForWindow(self->handle)) : 0;
    ui_part_fill_field(device, box, self->palette.window);
    ui_part_draw_field(device, &self->palette, box, focus);
}

static void draw_surface_button(const struct folio_window *_Nonnull self, HDC device, size_t slot)
{
    bool close = slot == ACCESSIBLE_SURFACE_CLOSE;
    struct ui_part_paint part = {.bounds = accessible_surface_button_rect(self, slot),
                                 .style = close ? UI_PART_STYLE_ICON : UI_PART_STYLE_COMMAND,
                                 .dpi = GetDpiForWindow(self->handle),
                                 .labeled = !close,
                                 .has_icon = close,
                                 .icon = ICON_PAINT_CLOSE,
                                 .hovered = self->surface_hover == slot,
                                 .pressed =
                                     self->surface_pressed == slot && self->surface_pressed_inside,
                                 .disabled = !folio_window_accessible_ready(self)};
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    if (!close)
    {
        SetTextColor(device, ink.text);
        draw_utf8(device,
                  ui_text_line(surface_button_label(self, slot), folio_state_language(self->state)),
                  ink.label);
    }
}

static void draw_search_surface(const struct folio_window *_Nonnull self, HDC device)
{
    enum folio_language language = folio_state_language(self->state);
    char line[search_status_capacity];
    search_status(self, line);
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, ui_text_line(search_direction_label(self), language),
              search_header_rect(self));
    (void)wrapped_text(device,
                       self->command_failure == FOLIO_STATE_READY ? line : self->result_line,
                       search_result_rect(self), true);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_CLOSE);
    draw_input_field(self, device, self->command_input, command_input_rect(self));
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_PREVIOUS);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_NEXT);
}

/* 失敗の 1 行。REPLACE_BAD_PATTERN のときだけ末尾に位置を添える（ADR 0028 の決定 9）。
 * out は replace_status_capacity の入れ物（呼び出し側が持つ大きさ）。 */
static void failure_status(const struct folio_window *_Nonnull self,
                           enum folio_state_outcome outcome, char *_Nonnull out)
{
    if (outcome == self->command_failure && self->result_line[0] != '\0')
    {
        (void)strcpy_s(out, replace_status_capacity, self->result_line);
        return;
    }
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
    struct ui_text_request request = {.id = self->command_surface == COMMAND_SURFACE_REPLACE
                                                ? UI_TEXT_STATUS_REPLACE_COUNT_ONLY
                                                : UI_TEXT_STATUS_REPLACE_COUNT,
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
    if (self->command_failure != FOLIO_STATE_READY)
    {
        failure_status(self, self->command_failure, out);
        return;
    }

    if (self->replace_outcome == FOLIO_STATE_READY &&
        GetWindowTextLengthW(self->replace_inputs[0]) > 0 &&
        !folio_state_has_replace_preview(self->state))
    {
        failure_status(self, FOLIO_STATE_REPLACE_STALE, out);
        return;
    }
    if (self->replace_outcome == FOLIO_STATE_READY)
    {
        replace_status(self, out);
        return;
    }
    failure_status(self, self->replace_outcome, out);
}

static void draw_replace_surface(const struct folio_window *_Nonnull self, HDC device)
{
    enum folio_language language = folio_state_language(self->state);
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, ui_text_line(UI_TEXT_ACCESSIBLE_PATTERN, language), self->band.label[0]);
    draw_utf8(device, ui_text_line(UI_TEXT_ACCESSIBLE_REPLACEMENT, language), self->band.label[1]);
    char line[replace_status_capacity];
    replace_line(self, line);
    (void)wrapped_text(device, line, self->band.result, true);
    for (size_t index = 0; index < replace_input_count; ++index)
        draw_input_field(self, device, self->replace_inputs[index],
                         replace_input_rect(self, index));
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_CLOSE);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_REPLACE_ONE);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_REPLACE_ALL);
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
    (void)bounds;
    if (!command_keys(self))
        return;
    RECT area = command_body_rect(self);
    int saved = SaveDC(device);
    IntersectClipRect(device, area.left, area.top, area.right, area.bottom);
    int offset = (int)command_help_offset(self);
    SetTextColor(device, self->palette.current_text);
    for (size_t index = 0; index < (size_t)command_help_rows(self); ++index)
    {
        RECT row = command_help_geometry(self, index, nullptr);
        OffsetRect(&row, 0, -offset);
        if (row.bottom <= area.top || row.top >= area.bottom)
            continue;
        char text[draw_unit_limit];
        command_help_text(self, index, text);
        (void)wrapped_text(device, text, row, true);
    }
    RestoreDC(device, saved);
}

static RECT command_previous_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_toggle_rect(self);
    bounds.left = bounds.right - scale(176, GetDpiForWindow(self->handle));
    bounds.right -= scale(88, GetDpiForWindow(self->handle));
    return bounds;
}

static RECT command_next_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = command_toggle_rect(self);
    bounds.left = bounds.right - scale(88, GetDpiForWindow(self->handle));
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
    size_t total = (size_t)command_help_total(self);
    if (visible == 0 || total <= visible)
        return;
    size_t step = command_help_step(self);
    size_t limit = total - visible;
    char line[command_pages_capacity];
    struct ui_text_request request = {.id = UI_TEXT_STATUS_PALETTE_PAGES,
                                      .language = folio_state_language(self->state),
                                      .k = (command_help_offset(self) + step - 1) / step + 1,
                                      .n = (limit + step - 1) / step + 1};
    if (ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY)
    {
        SetTextColor(device, self->palette.current_text);
        draw_utf8(device, line, command_pages_rect(self));
    }
}

static void draw_command_guidance(const struct folio_window *_Nonnull self, HDC device)
{
    enum folio_language language = folio_state_language(self->state);
    RECT first = command_tab_rect(self, 0);
    RECT group = first;
    group.right = command_tab_rect(self, 1).right;
    ui_part_frame_group(device, &self->palette, group);
    for (size_t index = 0; index < 2; ++index)
    {
        struct ui_part_paint part = {.bounds = command_tab_rect(self, index),
                                     .dpi = GetDpiForWindow(self->handle),
                                     .style = UI_PART_STYLE_CHOICE,
                                     .labeled = true,
                                     .grouped = true,
                                     .leading_rule = index != 0,
                                     .hovered = self->surface_hover == surface_button_count + index,
                                     .pressed =
                                         self->surface_pressed == surface_button_count + index &&
                                         self->surface_pressed_inside,
                                     .selected = self->command_keys_visible == (index == 1)};
        struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
        SetTextColor(device, ink.text);
        draw_units(
            device,
            ui_text_line(index == 0 ? UI_TEXT_HELP_TAB_COMMANDS : UI_TEXT_HELP_TAB_KEYS, language),
            ink.label, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    if (command_keys(self))
    {
        draw_surface_button(self, device, ACCESSIBLE_SURFACE_PREVIOUS);
        draw_surface_button(self, device, ACCESSIBLE_SURFACE_NEXT);
        draw_command_pages(self, device);
    }
}

/* 固定footerは内容のスクロールに参加しない。 */
static RECT settings_footer_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    bounds.top = bounds.bottom - settings_footer_height(self);
    return bounds;
}

static void draw_settings_footer(const struct folio_window *_Nonnull self, HDC device)
{
    RECT line = settings_footer_rect(self);
    enum folio_language language = folio_state_language(self->state);
    SetTextColor(device, self->palette.current_text);
    line.top +=
        wrapped_text(device, ui_text_line(UI_TEXT_SETTINGS_IMMEDIATE, language), line, true);
    SetTextColor(device, self->palette.dialog_secondary);
    char text[64];
    struct ui_text_request version = {
        .id = UI_TEXT_SETTINGS_VERSION, .language = language, .name = product_version_text()};
    if (ui_text_format(&version, text, sizeof text) == UI_TEXT_FORMAT_READY)
        line.top += wrapped_text(device, text, line, true);
    line.top += wrapped_text(device, ui_text_line(UI_TEXT_SETTINGS_GUIDANCE, language), line, true);
    line.top = line.bottom - self->settings_metrics.notice;
    if (settings_has_notice(self))
    {
        SetTextColor(device, self->palette.current_text);
        (void)wrapped_text(device, folio_state_failure_line(settings_notice(self), language), line,
                           true);
    }
}

static void fill_settings_rect(HDC device, RECT bounds, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    if (brush != nullptr)
    {
        FillRect(device, &bounds, brush);
        DeleteObject(brush);
    }
}

static void frame_settings_rect(HDC device, RECT bounds, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    if (brush != nullptr)
    {
        FrameRect(device, &bounds, brush);
        DeleteObject(brush);
    }
}

/* OS色では同じ輪郭色になり得るので、注目は内枠の有無でも示す。 */

static RECT settings_mark_rect(const struct folio_window *_Nonnull self, RECT row)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int box = scale(base_settings_mark_box, dpi);
    int left = row.left + scale(base_settings_mark_inset, dpi);
    int middle = (row.top + row.bottom) / 2;
    return (RECT){left, middle - box / 2, left + box, middle - box / 2 + box};
}

/* 採用済みの印とキーボードの注目の輪郭は別にする。 */
static void draw_settings_mark(const struct folio_window *_Nonnull self, HDC device, RECT row)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds = settings_mark_rect(self, row);
    frame_settings_rect(device, bounds, self->palette.dialog_border);
    if (folio_state_number(self->state))
    {
        InflateRect(&bounds, -scale(4, dpi), -scale(4, dpi));
        icon_paint_fill(device, bounds, ICON_PAINT_SELECTION, self->palette.current_text);
    }
}

/* その行がいまの値か（印を付ける行）。段を足したら枝を足させる（C-002）。 */
static bool settings_row_current(const struct folio_window *_Nonnull self, size_t index)
{
    switch (settings_rows[index].kind)
    {
    case SETTINGS_ROW_HEADING:
        return false;
    case SETTINGS_ROW_THEME:
        return false;
    case SETTINGS_ROW_LANGUAGE:
        return (unsigned char)folio_state_language(self->state) == settings_rows[index].value;
    case SETTINGS_ROW_NUMBER:
        return (unsigned char)folio_state_number(self->state) == settings_rows[index].value;
    }
    return false;
}

static void draw_settings_theme(const struct folio_window *_Nonnull self, HDC device, RECT row)
{
    UINT dpi = GetDpiForWindow(self->handle);
    row.left += scale(12, dpi);
    row.right -= scale(12, dpi);
    SetTextColor(device, self->palette.current_text);
    draw_units(device,
               folio_theme_choice_label(folio_state_theme_choice(self->state),
                                        folio_state_language(self->state)),
               row, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void draw_settings_row(const struct folio_window *_Nonnull self, HDC device, RECT row,
                              size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    bool choice = settings_row_choice(index);
    if (choice)
    {
        struct ui_part_paint part = {.bounds = row,
                                     .dpi = dpi,
                                     .style = UI_PART_STYLE_COMMAND,
                                     .focus_inside = true,
                                     .selected = settings_row_current(self, index),
                                     .focused = (self->focus_cues || self->focus_cues_always) &&
                                                GetFocus() == self->command_layer &&
                                                index == self->command_selection};
        ui_part_draw(device, &self->palette, &part);
        if (settings_rows[index].kind == SETTINGS_ROW_NUMBER)
        {
            draw_settings_mark(self, device, row);
        }
        if (settings_rows[index].kind != SETTINGS_ROW_NUMBER && settings_row_current(self, index))
        {
            RECT mark = settings_mark_rect(self, row);
            icon_paint_fill(device, mark, ICON_PAINT_SELECTION, self->palette.current_text);
        }
    }
    if (settings_rows[index].kind == SETTINGS_ROW_THEME)
    {
        draw_settings_theme(self, device, row);
        return;
    }
    RECT label = row;
    label.left += scale(choice ? 32 : 0, dpi);
    label.right -= scale(4, dpi);
    SetTextColor(device, self->palette.current_text);
    draw_units(device, ui_text_line(settings_rows[index].label, folio_state_language(self->state)),
               label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void draw_settings_rows(const struct folio_window *_Nonnull self, HDC device)
{
    RECT content = settings_content_rect(self);
    int saved = SaveDC(device);
    IntersectClipRect(device, content.left, content.top, content.right, content.bottom);
    for (size_t index = 0; index < settings_row_count(); ++index)
    {
        draw_settings_row(self, device, settings_item_rect(self, index), index);
    }
    RestoreDC(device, saved);
}

static HRESULT prepare_theme_candidates(const struct folio_window *_Nonnull self)
{
    if (self->command_initializing)
        return accessibility_node_unavailable;
    if (self->theme_candidates == nullptr)
        return E_OUTOFMEMORY;
    enum folio_language language = folio_state_language(self->state);
    if (theme_candidates_current(self->theme_candidates, language, self->surface_revision))
        return S_OK;
    if (self->command_composing)
        return accessibility_node_unavailable;
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        theme_candidates_invalidate(self->theme_candidates);
        return E_OUTOFMEMORY;
    }
    theme_candidates_prepare(self->theme_candidates, query, language, self->surface_revision);
    utf8_text_destroy(query);
    return S_OK;
}

static bool theme_candidate_at(const struct folio_window *_Nonnull self, size_t index,
                               enum folio_theme_choice *_Nonnull out)
{
    return prepare_theme_candidates(self) == S_OK &&
           theme_candidates_at(self->theme_candidates, index, out);
}

static size_t theme_matches_count(const struct folio_window *_Nonnull self)
{
    return prepare_theme_candidates(self) == S_OK ? theme_candidates_count(self->theme_candidates)
                                                  : 0;
}

static size_t theme_window_rows(const struct folio_window *_Nonnull self)
{
    size_t rows = command_visible_rows(self);
    return rows > 0 ? rows : 1;
}

static size_t theme_scroll_limit(const struct folio_window *_Nonnull self)
{
    size_t count = theme_matches_count(self);
    size_t rows = theme_window_rows(self);
    return count > rows ? count - rows : 0;
}

static void reveal_theme_selection(struct folio_window *_Nonnull self)
{
    size_t limit = theme_scroll_limit(self);
    if (self->command_first > limit)
        self->command_first = limit;
    if (self->command_selection >= theme_matches_count(self))
        return;
    if (self->command_selection < self->command_first)
        self->command_first = self->command_selection;
    size_t rows = theme_window_rows(self);
    if (self->command_selection - self->command_first >= rows)
        self->command_first = self->command_selection - rows + 1;
}

static void draw_theme_sample(HDC device, RECT bounds, COLORREF background, COLORREF foreground)
{
    fill_settings_rect(device, bounds, background);
    SetTextColor(device, foreground);
    draw_units(device, ui_text_line(UI_TEXT_THEME_SAMPLE, FOLIO_LANGUAGE_EN), bounds,
               DT_SINGLELINE | DT_CENTER | DT_VCENTER | DT_NOPREFIX);
}

static void draw_theme_samples(const struct folio_window *_Nonnull self, HDC device, RECT row,
                               struct folio_palette palette)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT sample = {row.left + scale(32, dpi), row.bottom - scale(21, dpi),
                   row.left + scale(76, dpi), row.bottom - scale(5, dpi)};
    draw_theme_sample(device, sample, palette.pane, palette.editor_text);
    OffsetRect(&sample, scale(48, dpi), 0);
    draw_theme_sample(device, sample, palette.selected_background, palette.selected_text);
    OffsetRect(&sample, scale(48, dpi), 0);
    draw_theme_sample(device, sample, palette.chip_background, palette.chip_text);
}

static void draw_theme_row(const struct folio_window *_Nonnull self, HDC device,
                           struct theme_row row)
{
    RECT box = row.bounds;
    UINT dpi = GetDpiForWindow(self->handle);
    struct ui_part_paint part = {.bounds = box,
                                 .dpi = dpi,
                                 .style = UI_PART_STYLE_COMMAND,
                                 .focus_inside = true,
                                 .selected = row.choice == folio_state_theme_choice(self->state),
                                 .focused = row.cursor};
    ui_part_draw(device, &self->palette, &part);
    if (row.choice == folio_state_theme_choice(self->state))
    {
        RECT mark = settings_mark_rect(self, box);
        icon_paint_fill(device, mark, ICON_PAINT_SELECTION, self->palette.current_text);
    }
    bool samples_visible = box.right - box.left >= scale(388, dpi);
    RECT label = {box.left + scale(32, dpi), box.top + scale(2, dpi),
                  box.right - scale(samples_visible ? 156 : 6, dpi), box.bottom};
    SetTextColor(device, self->palette.current_text);
    draw_units(device, folio_theme_choice_label(row.choice, folio_state_language(self->state)),
               label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    if (!samples_visible)
        return;
    enum folio_theme resolved = row.choice == FOLIO_THEME_CHOICE_SYSTEM
                                    ? folio_state_system_theme(self->state)
                                    : folio_theme_choice_resolve(row.choice, FOLIO_THEME_LIGHT);
    struct folio_palette palette = self->palette.contrast == FOLIO_CONTRAST_HIGH
                                       ? self->palette
                                       : folio_palette_from(folio_theme_palette(resolved));
    RECT samples = box;
    samples.left = box.right - scale(180, dpi);
    draw_theme_samples(self, device, samples, palette);
}

static void draw_theme_rows(const struct folio_window *_Nonnull self, HDC device)
{
    RECT bounds = theme_rows_rect(self);
    int height = scale(base_theme_row_height, GetDpiForWindow(self->handle));
    size_t visible = command_visible_rows(self);
    for (size_t offset = 0; offset < visible; ++offset)
    {
        enum folio_theme_choice choice = FOLIO_THEME_CHOICE_SYSTEM;
        size_t index = self->command_first + offset;
        if (!theme_candidate_at(self, index, &choice))
        {
            break;
        }
        RECT row = {bounds.left, bounds.top + (int)offset * height, bounds.right,
                    bounds.top + ((int)offset + 1) * height};
        draw_theme_row(self, device,
                       (struct theme_row){row, choice, index == self->command_selection});
    }
    if (theme_matches_count(self) == 0)
    {
        SetTextColor(device, self->palette.current_text);
        draw_units(device, ui_text_line(UI_TEXT_THEME_EMPTY, folio_state_language(self->state)),
                   bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

static void draw_theme_footer(const struct folio_window *_Nonnull self, HDC device, size_t count)
{
    RECT line = theme_footer_rect(self);
    line.bottom = theme_back_rect(self).top - scale(8, GetDpiForWindow(self->handle));
    enum folio_language language = folio_state_language(self->state);
    char text[ui_text_unit_limit];
    struct ui_text_request request = {.id = UI_TEXT_THEME_COUNT,
                                      .language = language,
                                      .k = count,
                                      .n = folio_theme_choice_count()};
    SetTextColor(device, self->palette.current_text);
    if (ui_text_format(&request, text, sizeof text) == UI_TEXT_FORMAT_READY)
        line.top += wrapped_text(device, text, line, true);
    enum ui_text guidance = self->palette.contrast == FOLIO_CONTRAST_HIGH
                                ? UI_TEXT_HIGH_CONTRAST_ACTIVE
                                : UI_TEXT_THEME_GUIDANCE;
    SetTextColor(device, self->palette.dialog_secondary);
    (void)wrapped_text(device, ui_text_line(guidance, language), line, true);
    line.top = line.bottom - self->settings_metrics.notice;
    if (settings_has_notice(self))
    {
        SetTextColor(device, self->palette.current_text);
        (void)wrapped_text(device, folio_state_failure_line(settings_notice(self), language), line,
                           true);
    }
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_BACK);
}

static void draw_theme_header(const struct folio_window *_Nonnull self, HDC device)
{
    enum folio_language language = folio_state_language(self->state);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT title = surface_frame_rect(self, UI_PART_SURFACE_TITLE_TEXT);
    SetTextColor(device, self->palette.current_text);
    draw_units(device, ui_text_line(UI_TEXT_THEME_TITLE, language), title,
               DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_CLOSE);
    RECT input = theme_input_rect(self);
    RECT label = {input.left, input.top - scale(20, dpi), input.right, input.top};
    draw_units(device, ui_text_line(UI_TEXT_THEME_FILTER, language), label,
               DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

static void draw_theme_dialog(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    ui_part_draw_surface(device, &self->palette, bounds);
    draw_theme_header(self, device);
    draw_input_field(self, device, self->command_input, command_input_rect(self));
    if (prepare_theme_candidates(self) != S_OK)
    {
        return;
    }
    draw_theme_rows(self, device);
    draw_theme_footer(self, device, theme_matches_count(self));
}

static void draw_settings_dialog(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    ui_part_draw_surface(device, &self->palette, bounds);
    RECT title = surface_frame_rect(self, UI_PART_SURFACE_TITLE_TEXT);
    SetTextColor(device, self->palette.current_text);
    draw_units(device, ui_text_line(UI_TEXT_COMMAND_SETTINGS, folio_state_language(self->state)),
               title, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_CLOSE);
    draw_settings_rows(self, device);
    draw_settings_footer(self, device);
}

/* 履歴の面の案内 1 行（ADR 0038 の決定 12）。設定画面の案内と同じ位置に出す。 */
static void draw_floating_header(const struct folio_window *_Nonnull self, HDC device,
                                 enum ui_text label)
{
    SetTextColor(device, self->palette.current_text);
    RECT title = surface_frame_rect(self, UI_PART_SURFACE_TITLE_TEXT);
    draw_units(device, ui_text_line(label, folio_state_language(self->state)), title,
               DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_CLOSE);
}

static void draw_history_guidance(const struct folio_window *_Nonnull self, HDC device)
{
    RECT line = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    line.top = line.bottom - scale(base_command_status_height, GetDpiForWindow(self->handle));
    SetTextColor(device, self->palette.dialog_secondary);
    (void)wrapped_text(device,
                       ui_text_line(UI_TEXT_HISTORY_GUIDANCE, folio_state_language(self->state)),
                       line, true);
}

/* 版の行の字面（「{n}  {name}」・読めない版は「{n}  （読めません）」）。最初の行は写しの大きさで
 * UTF-8 の文字の境目に切り、残りは描画の省略記号に任せる。句は表の置換子で組む（ADR 0030）。 */
static void history_row_text(const struct folio_window *_Nonnull self,
                             const struct history_row_view *_Nonnull view, char *_Nonnull out)
{
    char excerpt[history_excerpt_capacity];
    size_t length = view->first_line_length;
    if (length >= history_excerpt_capacity)
    {
        length = history_excerpt_capacity - 1;
        while (length > 0 &&
               ((unsigned char)view->first_line[length] & utf8_lead_mask) == utf8_continuation)
        {
            length -= 1;
        }
    }
    memcpy(excerpt, view->first_line, length);
    excerpt[length] = '\0';
    struct ui_text_request request = {.id = view->readable ? UI_TEXT_HISTORY_ROW
                                                           : UI_TEXT_HISTORY_ROW_UNREADABLE,
                                      .language = folio_state_language(self->state),
                                      .n = view->version,
                                      .name = excerpt};
    if (ui_text_format(&request, out, history_line_capacity) != UI_TEXT_FORMAT_READY)
    {
        out[0] = '\0';
    }
}

/* 1 行分を描く。読めない版は薄い文字で、選べるが Enter は欄の 1 行で断られる（決定 12）。
 * 行が無ければ false（application が一覧を捨てた後など）。 */
static bool draw_history_row(const struct folio_window *_Nonnull self, HDC device, RECT row,
                             size_t index)
{
    struct history_row_view view = {
        .version = 0, .first_line = "", .first_line_length = 0, .readable = false};
    if (!folio_state_history_row(self->state, index, &view))
    {
        return false;
    }
    bool selected = index == self->command_selection;
    if (selected)
    {
        HBRUSH brush = CreateSolidBrush(self->palette.selected_background);
        FillRect(device, &row, brush);
        DeleteObject(brush);
    }
    UINT dpi = GetDpiForWindow(self->handle);
    RECT label = row;
    label.left += scale(base_command_row_padding, dpi);
    label.right -= scale(base_command_row_padding, dpi);
    char text[history_line_capacity];
    history_row_text(self, &view, text);
    COLORREF color = view.readable ? self->palette.current_text : self->palette.header_text;
    SetTextColor(device, selected ? self->palette.selected_text : color);
    draw_utf8(device, text, label);
    return true;
}

static void draw_history_rows(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT rows = command_rows_rect(self);
    int height = scale(base_command_row_height, dpi);
    size_t count = command_visible_rows(self);
    for (size_t visible = 0; visible < count; ++visible)
    {
        RECT row = {rows.left, rows.top + (int)visible * height, rows.right,
                    rows.top + (int)(visible + 1) * height};
        if (!draw_history_row(self, device, row, self->command_first + visible))
        {
            break;
        }
    }
    int padding = scale(base_command_palette_padding, dpi);
    RECT status = {bounds.left + padding, bounds.bottom - scale(base_command_status_height, dpi),
                   bounds.right - padding, bounds.bottom};
    draw_command_status(self, device, status);
    draw_history_guidance(self, device);
}

static void draw_palette_empty(const struct folio_window *_Nonnull self, HDC device)
{
    enum folio_language language = folio_state_language(self->state);
    if (palette_matches_count(self) != 0)
        return;
    SetTextColor(device, self->palette.dialog_secondary);
    (void)wrapped_text(device, ui_text_line(UI_TEXT_ACCESSIBLE_NO_RESULTS, language),
                       command_rows_rect(self), true);
}

static void draw_palette_rows(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    if (prepare_palette_candidates(self) != S_OK)
        return;
    enum folio_command previous = FOLIO_COMMAND_SAVE;
    for (size_t index = self->command_first;; ++index)
    {
        RECT row = palette_candidate_rect(self, self->command_first, index);
        enum folio_command command = FOLIO_COMMAND_SAVE;
        if (IsRectEmpty(&row) || !palette_command_at(self, index, &command))
            break;
        if (index == self->command_first ||
            folio_command_scope(previous) != folio_command_scope(command))
        {
            RECT heading = row;
            OffsetRect(&heading, 0, row.top - row.bottom);
            SetTextColor(device, self->palette.dialog_secondary);
            draw_utf8(device,
                      ui_text_line(
                          command_candidates_group_label(command_candidates_group_index(command)),
                          folio_state_language(self->state)),
                      heading);
        }
        draw_command_row(self, device,
                         (struct command_row){row, command, index == self->command_selection});
        previous = command;
    }
    int padding = scale(base_command_palette_padding, GetDpiForWindow(self->handle));
    RECT status = {padding,
                   bounds.bottom - scale(base_command_status_height, GetDpiForWindow(self->handle)),
                   bounds.right - padding, bounds.bottom};
    draw_command_status(self, device, status);
    draw_command_shortcuts(self, device, bounds);
    draw_command_guidance(self, device);
    draw_palette_empty(self, device);
}

static void draw_help_surface(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    ui_part_draw_surface(device, &self->palette, bounds);
    draw_floating_header(self, device, UI_TEXT_COMMAND_HELP);
    if (command_keys(self))
    {
        draw_command_shortcuts(self, device, bounds);
        draw_command_guidance(self, device);
        return;
    }
    draw_palette_rows(self, device, bounds);
    draw_input_field(self, device, self->command_input, command_input_rect(self));
}

static void draw_command_surface(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        draw_menu_surface(self, device);
        return;
    }

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
        draw_input_field(self, device, self->command_input, input);
        return;
    }
    case COMMAND_SURFACE_PALETTE:
        draw_help_surface(self, device, bounds);
        return;
    case COMMAND_SURFACE_SEARCH:
        ui_part_draw_surface(device, &self->palette, bounds);
        draw_search_surface(self, device);
        return;
    case COMMAND_SURFACE_REPLACE:
        ui_part_draw_surface(device, &self->palette, bounds);
        draw_replace_surface(self, device);
        return;
    case COMMAND_SURFACE_THEMES:
        draw_theme_dialog(self, device, bounds);
        return;
    case COMMAND_SURFACE_SETTINGS:
        draw_settings_dialog(self, device, bounds);
        return;
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
        ui_part_draw_surface(device, &self->palette, bounds);
        draw_floating_header(self, device, UI_TEXT_COMMAND_HISTORY);
        draw_history_rows(self, device, bounds);
        return;
    }
}

/* 主窓と外周を覆う子が共有する、screen座標とDPIによる唯一のresize判定。 */
static LRESULT resize_hit(const struct folio_window *_Nonnull self, LPARAM lparam)
{
    if (IsZoomed(self->handle))
    {
        return HTNOWHERE;
    }
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    RECT window;
    GetWindowRect(self->handle, &window);
    UINT dpi = GetDpiForWindow(self->handle);
    int border = GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
                 GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    return edge_hit(point, window, border);
}

/* 行dragや押下のcaptureは子に残し、新しい窓縁の操作だけを主窓へ通す。 */
static bool resize_through(const struct folio_window *_Nonnull self, LPARAM lparam)
{
    return GetCapture() == nullptr && resize_hit(self, lparam) != HTNOWHERE;
}

/* 共通頭のbutton以外の空白を掴んで動かす。縁は既存のresize判定。 */
static LRESULT hit_test(const struct folio_window *_Nonnull self, LPARAM lparam)
{
    LRESULT edge = resize_hit(self, lparam);
    if (edge != HTNOWHERE)
    {
        return edge;
    }
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(self->handle, &point);
    RECT header = regions_for(self).header;
    if (!PtInRect(&header, point))
    {
        return HTCLIENT;
    }
    RECT close = close_rect(self);
    RECT settings = settings_rect(self);
    RECT help = help_rect(self);
    RECT minimize = minimize_rect(self);
    RECT maximize = maximize_rect(self);
    RECT group = layout_slot_rect(self);
    RECT fresh = new_note_rect(self);
    bool button = PtInRect(&fresh, point) || PtInRect(&group, point) || PtInRect(&close, point) ||
                  PtInRect(&settings, point) || PtInRect(&help, point) ||
                  PtInRect(&minimize, point) || PtInRect(&maximize, point);
    return button ? HTCLIENT : HTCAPTION;
}

/* 本文操作帯の A 様式の群（帯 36 = 4+28+4）。幅は現在の言語の字面を頭と同じ等幅の書体と字間で
 * 測る（#122）。字を出すと窓の縁 16 に収まらないときだけ、絵を持つ項目を検索・保存の順に
 * 絵だけへ縮め、消さない（ADR0060 の決定 1）。描画・当たり判定・UIA・メニューの位置が共有する。 */
static void action_layout(const struct folio_window *_Nonnull self, RECT *_Nonnull out,
                          bool *_Nonnull labeled)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT pane = regions_for(self).pane;
    RECT client;
    GetClientRect(self->handle, &client);
    int left = pane.left + scale(base_pane_left, dpi);
    int height = scale(ui_part_height, dpi);
    int top =
        pane.top + scale(base_caption_height, dpi) + (scale(base_action_height, dpi) - height) / 2;
    int widths[action_count] = {};
    int total = 0;
    for (size_t index = 0; index < action_count; ++index)
    {
        int text = measure_part_text(self, part_display_text(action_parts[index].kind));
        widths[index] = ui_part_command_width(text, action_parts[index].has_icon, dpi);
        labeled[index] = true;
        total += widths[index];
    }
    int room = client.right - scale(ui_part_edge, dpi) - left - 2; /* 窓の縁 16 */
    const size_t shrink[] = {1, 0}; /* 検索、保存の順（操作▾は絵を持たないので縮めない） */
    for (size_t at = 0; at < sizeof shrink / sizeof shrink[0] && total > room; ++at)
    {
        total -= widths[shrink[at]] - height;
        widths[shrink[at]] = height;
        labeled[shrink[at]] = false;
    }
    int x = left + 1; /* 群の 1px の枠の内側 */
    for (size_t index = 0; index < action_count; ++index)
    {
        out[index] = (RECT){x, top, x + widths[index], top + height};
        x += widths[index];
    }
}

static RECT action_button_rect(const struct folio_window *_Nonnull self, size_t index)
{
    RECT rects[action_count];
    bool labeled[action_count];
    action_layout(self, rects, labeled);
    return rects[index];
}

/* 群の枠は項目の 1px 外側。 */
static RECT action_group_rect(const struct folio_window *_Nonnull self)
{
    RECT rects[action_count];
    bool labeled[action_count];
    action_layout(self, rects, labeled);
    RECT group = {rects[0].left, rects[0].top, rects[action_count - 1].right, rects[0].bottom};
    InflateRect(&group, 1, 1);
    return group;
}

/* 保存は変更ありの間だけ B 主操作、保存済みは A 中立（ADR0060 の決定 8）。押せる状態は保つ。 */
static enum ui_part_style part_style(const struct folio_window *_Nonnull self,
                                     enum accessible_kind kind)
{
    return kind == ACCESSIBLE_SAVE && self->unsaved ? UI_PART_STYLE_PRIMARY : UI_PART_STYLE_COMMAND;
}

static void draw_actions(const struct folio_window *_Nonnull self, HDC device)
{
    RECT rects[action_count];
    bool labeled[action_count];
    action_layout(self, rects, labeled);
    ui_part_frame_group(device, &self->palette, action_group_rect(self));
    for (size_t index = 0; index < action_count; ++index)
    {
        struct ui_part_paint part =
            part_paint(self, action_parts[index].kind, part_style(self, action_parts[index].kind));
        part.bounds = rects[index];
        part.grouped = true;
        part.leading_rule = index > 0;
        part.labeled = labeled[index];
        part.has_icon = action_parts[index].has_icon;
        part.icon = action_parts[index].icon;
        struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
        if (labeled[index])
        {
            draw_part_label(self, device, action_parts[index].kind, ink);
        }
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
    SelectObject(device, gutter_font(self));
    SetTextCharacterExtra(device, 0);
    SetTextAlign(device, TA_RIGHT | TA_BASELINE | TA_NOUPDATECP);
    for (size_t at = 0; at < count; ++at)
    {
        wchar_t label[24];
        int length = number_units(rows[at].number, label, 24);
        TextOutW(device, band.right, rows[at].baseline, label, length);
    }
    RestoreDC(device, saved);
}

static enum ui_text pane_notice_line(const struct folio_window *_Nonnull self)
{
    enum ui_text line = UI_TEXT_TRASH_NOTE_COMPLETED;
    if (pane_unavailable(self))
    {
        line = UI_TEXT_FAILURE_PANE_LOAD_FAILED;
    }
    else
        switch (self->notice)
        {
        case PANE_NOTICE_NONE:
            return UI_TEXT_EMPTY;
        case PANE_NOTICE_COMMAND_RESULT:
            line = UI_TEXT_COMMAND_CAPTION;
            break;
        case PANE_NOTICE_NOTE_UNEDITABLE:
            line = UI_TEXT_NOTE_OPENED_IN_VIEW;
            break;
        case PANE_NOTICE_SESSION_BLOCKED:
            line = self->session_reason;
            break;
        }
    return line;
}

static RECT pane_notice_rect(const struct folio_window *_Nonnull self, RECT client)
{
    UINT dpi = GetDpiForWindow(self->handle);
    return (RECT){folio_state_layout_mode(self->state) == FOLIO_LAYOUT_DRAWER
                      ? scale(16, dpi)
                      : pane_bounds(self).left,
                  client.bottom - pane_notice_height(self),
                  client.right - scale(base_close_margin, dpi), client.bottom};
}

static void draw_pane_notice(const struct folio_window *_Nonnull self, HDC device, RECT client)
{
    enum ui_text line = pane_notice_line(self);
    if (line == UI_TEXT_EMPTY)
        return;
    SelectObject(device, self->mono_font);
    SetBkMode(device, TRANSPARENT);
    SetTextColor(device, self->palette.header_text);
    draw_utf8(device,
              self->notice == PANE_NOTICE_COMMAND_RESULT
                  ? self->result_line
                  : ui_text_line(line, folio_state_language(self->state)),
              pane_notice_rect(self, client));
}

/* 新しいノートの作成先の 1 行（ADR0060 の決定 12・13）。作成先の規則は Ctrl+N と同じ
 * folio_state_current_category。tooltip・UIA の HelpText・空状態の案内がこの 1 本を使う。
 * カテゴリが 0 件なら作れない理由。out は draw_unit_limit バイトの入れ物。 */
static void new_note_destination(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    struct ui_text_request request = {.id = UI_TEXT_NEW_NOTE_NO_CATEGORY, .language = language};
    if (folio_state_category_count(self->state) > 0)
    {
        request.id = UI_TEXT_TOOLTIP_NEW_NOTE;
        request.name =
            folio_state_category_name(self->state, folio_state_current_category(self->state));
        request.from = folio_shortcut_command_hint(FOLIO_COMMAND_NEW, language);
    }
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
    {
        out[0] = '\0';
    }
}

/* 空状態の案内の矩形。本文が置かれる矩形と同じ左上から始める（ADR0060 の決定 13）。 */
static RECT guide_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    RECT pane = regions_for(self).pane;
    int top = pane.top + scale(base_caption_height + base_action_height + base_pane_top, dpi);
    return (RECT){pane.left + scale(base_pane_left, dpi), top,
                  client.right - scale(base_pane_right, dpi),
                  client.bottom - scale(base_pane_bottom, dpi)};
}

/* 空状態の案内の 4 行（見出し・本文・作成先・キー）。文言は ui_text が持つ。 */
static void guide_lines(const struct folio_window *_Nonnull self,
                        const char *_Nonnull *_Nonnull out, char *_Nonnull destination)
{
    enum folio_language language = folio_state_language(self->state);
    new_note_destination(self, destination);
    out[0] = ui_text_line(UI_TEXT_EMPTY_HEADING, language);
    out[1] = ui_text_line(UI_TEXT_EMPTY_BODY, language);
    out[2] = destination;
    out[3] = ui_text_line(UI_TEXT_EMPTY_KEYS, language);
}

static void draw_guide(const struct folio_window *_Nonnull self, HDC device)
{
    if (!pane_visible(self))
    {
        return;
    }
    const char *lines[4] = {"", "", "", ""};
    char destination[draw_unit_limit];
    guide_lines(self, lines, destination);
    const COLORREF colors[4] = {self->palette.category_text, self->palette.current_text,
                                self->palette.header_text, self->palette.header_text};
    RECT bounds = guide_rect(self);
    int gap = scale(base_guide_gap, GetDpiForWindow(self->handle));
    for (size_t index = 0; index < 4 && bounds.top < bounds.bottom; ++index)
    {
        SetTextColor(device, colors[index]);
        bounds.top += draw_units(device, lines[index], bounds, DT_WORDBREAK | DT_NOPREFIX) + gap;
    }
}

/* 新しいノートは A 様式の単独の釦。狭幅では絵だけに縮める（ADR0059 / ADR0060）。 */
static void draw_new_note(const struct folio_window *_Nonnull self, HDC device)
{
    struct ui_part_paint part = part_paint(self, ACCESSIBLE_NEW_NOTE, UI_PART_STYLE_COMMAND);
    part.labeled = new_note_labeled(self);
    part.has_icon = true;
    part.icon = ICON_PAINT_NEW_NOTE;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    if (part.labeled)
    {
        draw_part_label(self, device, ACCESSIBLE_NEW_NOTE, ink);
    }
}

static void draw_layout_chooser(const struct folio_window *_Nonnull self, HDC device)
{
    static const enum icon_paint_kind icons[] = {[FOLIO_LAYOUT_DRAWER] = ICON_PAINT_LAYOUT_DRAWER,
                                                 [FOLIO_LAYOUT_BOTH] = ICON_PAINT_LAYOUT_BOTH,
                                                 [FOLIO_LAYOUT_NOTE] = ICON_PAINT_LAYOUT_NOTE};
    struct ui_part_paint part = part_paint(self, ACCESSIBLE_LAYOUT_CHOOSER, UI_PART_STYLE_COMMAND);
    part.labeled = false;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    int size = scale(ui_part_icon, part.dpi);
    int padding = scale(8, part.dpi);
    int top = (part.bounds.top + part.bounds.bottom - size) / 2;
    RECT mode = {part.bounds.left + padding, top, part.bounds.left + padding + size, top + size};
    RECT chevron = {part.bounds.right - padding - size, top, part.bounds.right - padding,
                    top + size};
    icon_paint_fill(device, mode, icons[folio_state_layout_mode(self->state)], ink.text);
    icon_paint_fill(device, chevron, ICON_PAINT_MENU, ink.text);
}

static void draw_common_header(const struct folio_window *_Nonnull self, HDC device)
{
    UINT dpi = GetDpiForWindow(self->handle);
    SelectObject(device, self->mono_font);
    SetBkMode(device, TRANSPARENT);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    RECT group = layout_slot_rect(self);
    RECT logo = regions_for(self).header;
    logo.left = new_note_rect(self).right + scale(base_brand_gap, dpi);
    logo.right = group.left - scale(8, dpi);
    const char *heading = ui_text_line(UI_TEXT_LAYOUT_GROUP, folio_state_language(self->state));
    int heading_width = measure_utf8(device, heading);
    if (logo.right - logo.left >= heading_width + scale(128, dpi))
    {
        RECT label = logo;
        label.left = label.right - heading_width;
        SetTextColor(device, self->palette.current_text);
        draw_units(device, heading, label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        logo.right = label.left - scale(16, dpi);
    }
    const char *text = ui_text_line(UI_TEXT_APP_LOGO, folio_state_language(self->state));
    if (measure_utf8(device, text) <= logo.right - logo.left)
    {
        SetTextColor(device, self->palette.header_text);
        draw_units(device, text, logo, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }
    enum folio_layout_mode mode = folio_state_layout_mode(self->state);
    if (layout_metrics_for(self).chooser)
        draw_layout_chooser(self, device);
    if (!layout_metrics_for(self).chooser)
    {
        for (size_t index = 0; index < layout_button_count(); ++index)
            draw_choice_item(self, device, layout_buttons[index].kind,
                             mode == layout_buttons[index].mode);
    }
    draw_new_note(self, device);
    draw_icon_part(self, device, ACCESSIBLE_HELP, ICON_PAINT_HELP);
    draw_icon_part(self, device, ACCESSIBLE_SETTINGS, ICON_PAINT_SETTINGS);
    draw_icon_part(self, device, ACCESSIBLE_MINIMIZE, ICON_PAINT_MINIMIZE);
    draw_icon_part(self, device, ACCESSIBLE_MAXIMIZE,
                   IsZoomed(self->handle) ? ICON_PAINT_RESTORE : ICON_PAINT_MAXIMIZE);
    draw_icon_part(self, device, ACCESSIBLE_CLOSE, ICON_PAINT_CLOSE);
    SetTextCharacterExtra(device, 0);
}

/* 右ペインの地と頭。本文は note_pane が持つ。 */
static void draw_pane(const struct folio_window *_Nonnull self, HDC device, RECT damage)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT pane = regions_for(self).pane;
    HBRUSH brush = CreateSolidBrush(self->palette.pane);
    FillRect(device, &pane, brush);
    DeleteObject(brush);
    SetBkMode(device, TRANSPARENT);
    SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    if (document_open(self))
    {
        draw_breadcrumb(self, device, caption_rect(self));
        draw_mode_group(self, device);
        draw_actions(self, device);
    }
    SetTextCharacterExtra(device, 0);
    if (!document_open(self))
    {
        draw_guide(self, device);
    }
    draw_gutter(self, device, damage);
}

/* 共通頭と可視paneを更新矩形へ一枚だけ描く。memory原点をずらしてclient座標を共用する。 */
static void blit_window(struct folio_window *_Nonnull self, HDC target, RECT client, RECT damage)
{
    HDC memory = CreateCompatibleDC(target);
    int width = damage.right - damage.left;
    int height = damage.bottom - damage.top;
    HBITMAP surface = memory == nullptr ? nullptr : CreateCompatibleBitmap(target, width, height);
    if (surface != nullptr)
    {
        HGDIOBJ previous = SelectObject(memory, surface);
        SetViewportOrgEx(memory, -damage.left, -damage.top, nullptr);
        HBRUSH brush = CreateSolidBrush(self->palette.window);
        FillRect(memory, &damage, brush);
        DeleteObject(brush);
        draw_common_header(self, memory);
        if (pane_visible(self))
        {
            draw_pane(self, memory, damage);
        }
        draw_pane_notice(self, memory, client);
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
 * 契機を 1 か所に保つため、描く矩形が空でも毎回の WM_PAINT で確かめる。
 * 本文を置くかは arrange と同じ body_shown で決める。隠した本文は配り直しても動かないので、
 * 空状態で比べると WM_PAINT と配り直しが互いを呼び続ける（#356）。 */
static void check_pane_inset(const struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr || !body_shown(self))
    {
        return;
    }
    UINT dpi = GetDpiForWindow(self->handle);
    if (pane_bounds(self).left != regions_for(self).pane.left + pane_inset(self, dpi))
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
    if (!IsRectEmpty(&painting.rcPaint))
    {
        blit_window(self, target, client, painting.rcPaint);
    }
    EndPaint(self->handle, &painting);
    check_pane_inset(self);
}

/* 保存も検索も同じ本文取得の結果変換を使う。 */
static enum folio_state_outcome take_text(const struct folio_window *_Nonnull self,
                                          const char16_t *_Nonnull *_Nonnull units,
                                          size_t *_Nonnull count)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    return from_pane_text(note_pane_text(self->pane, units, count));
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
static void focus_pane(struct folio_window *_Nonnull self)
{
    show_pane_layout(self);
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    if (pane != nullptr && IsWindowVisible(pane) && IsWindowEnabled(pane))
    {
        SetFocus(pane);
    }
}

/* application が置いた編集 RTF（folio_state_pane_edit_rtf）を編集本文へ流し込む。
 * 編集の入口はすべてここを通る（ADR 0006 の #245 補正）。 */
static enum folio_state_outcome pour_edit_rtf(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    bool was_unavailable = pane_unavailable(self);
    enum note_pane_load_outcome outcome =
        note_pane_load(self->pane, folio_state_edit_rtf(self->state),
                       folio_state_edit_rtf_length(self->state), PANE_MODE_EDIT);
    return loaded_pane(self, was_unavailable, outcome);
}

/* 選択中のノートを、いまのモードのまま右ペインへ写す（ADR 0013 の決定 4 の (iii)）。
 * 編集中は本文を閲覧と同じ UTF-8 RTF で流し込むだけで、フォーカスは動かさない（ADR 0006 の #245
 * 補正）。 */
static enum folio_state_outcome reflect_note(struct folio_window *_Nonnull self)
{
    set_pane_notice(self, PANE_NOTICE_NONE);
    switch (folio_state_pane_mode(self->state))
    {
    case PANE_MODE_VIEW:
        return render_pane(self);
    case PANE_MODE_EDIT:
        return pour_edit_rtf(self);
    }
    return FOLIO_STATE_PANE_LOAD_FAILED;
}

static enum folio_state_outcome show_note(struct folio_window *_Nonnull self)
{
    if (pane_streaming(self))
    {
        return FOLIO_STATE_PANE_LOAD_FAILED;
    }
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
    {
        enum folio_state_outcome prepared = folio_state_pane_edit_rtf(self->state);
        if (prepared != FOLIO_STATE_READY)
        {
            return prepared;
        }
    }
    return reflect_note(self);
}

/* 初回だけ本文を用意して編集へ入る。同じmodeへの操作は未保存本文とUndoを保つ。 */
static enum folio_state_outcome enter_edit(struct folio_window *_Nonnull self)
{
    if (pane_streaming(self))
    {
        return FOLIO_STATE_PANE_LOAD_FAILED;
    }
    if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT && self->pane != nullptr &&
        note_pane_readiness(self->pane) == NOTE_PANE_READINESS_READY)
    {
        return FOLIO_STATE_READY;
    }
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        return FOLIO_STATE_NOTHING_SELECTED;
    }
    /* RTF は begin_edit より先に作る。確保に失敗したら application は何も変えていない。 */
    if (self->pane == nullptr || folio_state_pane_edit_rtf(self->state) != FOLIO_STATE_READY)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome outcome = folio_state_begin_edit(self->state);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = pour_edit_rtf(self);
    }
    return outcome;
}

/* 編集中なら保存して閲覧へ戻す。閲覧中なら何もしない（「閲覧」の札だけが使う）。 */
static enum folio_state_outcome flush_edit(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        return pane_unavailable(self) ? render_pane(self) : store_body(self);
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
        outcome = render_pane(self);
        /* NOTEでは可視pane、それ以外は従来の索引へ戻す（ADR0047）。 */
        SetFocus(surface_return_target(self, self->handle));
    }
    return outcome;
}

/* 編集中の本文を取り出して保存する。取り出せない理由も保存の失敗も 1 つの結果に写す。 */
static enum folio_state_outcome store_body(struct folio_window *_Nonnull self)
{
    if (self->pane != nullptr && note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY)
    {
        return FOLIO_STATE_PANE_LOAD_FAILED;
    }
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        enum folio_state_outcome outcome = folio_state_store_note(self->state, u"", 0);
        return outcome == FOLIO_STATE_NOTHING_SELECTED &&
                       folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE
                   ? FOLIO_STATE_READY
                   : outcome;
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

/* Leaving an untitled document reuses the first-save dialog without a second confirmation. */
static struct name_prompt_result depart_document(struct folio_window *_Nonnull self)
{
    struct name_prompt_result result = {FOLIO_STATE_READY, FOLIO_UNTITLED_PROTECT};
    result.outcome = prepare_name_operation(self);
    if (result.outcome != FOLIO_STATE_READY)
    {
        return result;
    }
    if (folio_state_document_kind(self->state) != FOLIO_DOCUMENT_UNTITLED)
    {
        result.outcome = store_body(self);
        return result;
    }
    const char16_t *_Nonnull units = u"";
    size_t count = 0;
    result.outcome = take_text(self, &units, &count);
    enum folio_note_change changed = FOLIO_NOTE_SAME;
    if (result.outcome == FOLIO_STATE_READY)
    {
        result.outcome = folio_state_note_changed(self->state, units, count, &changed);
    }
    if (result.outcome != FOLIO_STATE_READY)
    {
        return result;
    }
    struct name_prompt_request request = {
        .state = self->state, .kind = NAME_PROMPT_LEAVE_UNTITLED, .units = units, .count = count};
    result = show_name_prompt(self, &request);
    /* A partial first-save publication changed title/index even when departure stops. */
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    return result;
}

static enum folio_state_outcome command_save(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
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
 * 全値を true / false で明示し、値を足して行を書き忘れると CNF-009 が落ちるので、
 * 分け方を必ず決めさせる（C-002・ADR 0027 の 2026-10-06 の補正）。範囲検査は書かない。 */
static const bool inline_outcomes[] = {
    [FOLIO_STATE_NOTHING_SELECTED] = true,
    [FOLIO_STATE_NOT_EDITING] = true,
    [FOLIO_STATE_UNSAVED_CHANGES] = true,
    [FOLIO_STATE_REPLACE_NO_PATTERN] = true,
    [FOLIO_STATE_REPLACE_BAD_PATTERN] = true,
    [FOLIO_STATE_REPLACE_BAD_TEMPLATE] = true,
    [FOLIO_STATE_REPLACE_TIMED_OUT] = true,
    [FOLIO_STATE_REPLACE_TOO_COMPLEX] = true,
    [FOLIO_STATE_REPLACE_TOO_MANY] = true,
    [FOLIO_STATE_REPLACE_TOO_LARGE] = true,
    [FOLIO_STATE_REPLACE_STALE] = true,
    [FOLIO_STATE_HISTORY_EMPTY] = true,      /* ADR 0038 の決定 13: 面を開かず欄の 1 行 */
    [FOLIO_STATE_HISTORY_UNREADABLE] = true, /* 読めない版を選んだ。箱を出すほどではない */
    /* 編集で保てない本文（#242）。面が開いていれば欄の 1 行。札からの編集は面が無いので箱 */
    [FOLIO_STATE_NOTE_UNEDITABLE] = true,
    [FOLIO_STATE_REPLACE_BAD_SPAN] = true,
    /* 設定の失敗は欄の中の 1 行（ADR 0031 の決定 7）。設定画面は閉じずに理由を出す。
     * `:set number` の失敗も同じ値なので、他の Ex の失敗と同じく欄の 1 行になる。 */
    [FOLIO_STATE_SETTINGS_UNREADABLE] = true,
    [FOLIO_STATE_APPEARANCE_UNAVAILABLE] = true,
    [FOLIO_STATE_SETTINGS_STORE_FAILED] = false,
    [FOLIO_STATE_READY] = false,
    [FOLIO_STATE_DATA_UNREADABLE] = false,
    [FOLIO_STATE_LEDGER_MALFORMED] = false,
    [FOLIO_STATE_STORE_FAILED] = false,
    [FOLIO_STATE_NO_SUCH_CATEGORY] = true,
    [FOLIO_STATE_NO_SUCH_NOTE] = true,
    [FOLIO_STATE_NOTE_UNREADABLE] = false,
    [FOLIO_STATE_NOTE_MALFORMED] = true,
    [FOLIO_STATE_NOTE_TOO_LARGE] = true,
    [FOLIO_STATE_NOTE_STORE_FAILED] = false,
    [FOLIO_STATE_HISTORY_FAILED] = false,
    [FOLIO_STATE_NAME_TAKEN] = true,
    [FOLIO_STATE_LEDGER_STALE] = false,
    [FOLIO_STATE_LEDGER_UNSYNCED] = false,
    [FOLIO_STATE_RENAME_PENDING] = false,
    [FOLIO_STATE_RENAME_UNLOCKED] = false,
    [FOLIO_STATE_RENAME_UNSUPPORTED] = true,
    [FOLIO_STATE_RENAME_IDENTITY_FAILED] = false,
    [FOLIO_STATE_RENAME_JOURNAL_FAILED] = false,
    [FOLIO_STATE_RENAME_JOURNAL_BROKEN] = false,
    [FOLIO_STATE_RENAME_HALTED] = false,
    [FOLIO_STATE_SEARCH_MALFORMED] = true,
    [FOLIO_STATE_FILTERED] = true,
    [FOLIO_STATE_PANE_UNAVAILABLE] = false,
    [FOLIO_STATE_PANE_LOAD_FAILED] = false,
    [FOLIO_STATE_PANE_RECOLOR_FAILED] = false,
    [FOLIO_STATE_OUT_OF_MEMORY] = false,
    [FOLIO_STATE_NAME_REQUIRED] = true,
    [FOLIO_STATE_INVALID_NAME] = true,
    [FOLIO_STATE_ALREADY_NAMED] = true,
    [FOLIO_STATE_NAME_UNCHANGED] = true,
    [FOLIO_STATE_CANCELLED] = false,
    /* 起動時に 1 回だけ箱で知らせる（ADR 0036 の決定 5）。欄から来ることは無い */
    [FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE] = false,
    /* カテゴリの操作の失敗は箱で知らせる（ADR 0039 の補正 1） */
    [FOLIO_STATE_CATEGORY_NAME_INVALID] = true,
    [FOLIO_STATE_CATEGORY_NAME_TAKEN] = true,
    [FOLIO_STATE_CATEGORY_NOT_CREATED] = false,
    [FOLIO_STATE_CATEGORY_LEDGER_STALE] = false,
    [FOLIO_STATE_TRASH_UNAVAILABLE] = false,
    [FOLIO_STATE_TRASH_BUSY] = false,
    [FOLIO_STATE_TRASH_FAILED] = false,
    [FOLIO_STATE_CATEGORY_NOT_EMPTY] = true,
    [FOLIO_STATE_CATEGORY_HAS_FILES] = true,
    [FOLIO_STATE_CATEGORY_HISTORY_NOT_RECYCLED] = false,
    [FOLIO_STATE_CATEGORY_REMOVE_FAILED] = false,
    [FOLIO_STATE_CATEGORY_NOT_SELECTED] = true,
};

static bool inline_outcome(enum folio_state_outcome outcome)
{
    return inline_outcomes[outcome];
}

static bool measure_band_reason(HDC device, const char *_Nonnull text, RECT bounds,
                                int *_Nonnull maximum)
{
    if (text[0] == '\0')
        return true;
    int height = wrapped_text(device, text, bounds, false);
    if (height <= 0)
        return false;
    *maximum = height > *maximum ? height : *maximum;
    return true;
}

/* 既存outcome表の網羅を再利用する。別の失敗一覧を作らない。 */
static bool band_outcomes_height(HDC device, enum folio_language language, RECT bounds,
                                 int *_Nonnull maximum)
{
    for (size_t row = 0; row < sizeof inline_outcomes / sizeof inline_outcomes[0]; ++row)
        if (!measure_band_reason(device,
                                 folio_state_failure_line((enum folio_state_outcome)row, language),
                                 bounds, maximum))
            return false;
    return true;
}

static int surface_footer_text_height(const struct folio_window *_Nonnull self, HDC device,
                                      RECT bounds, bool themes)
{
    enum folio_language language = folio_state_language(self->state);
    char text[ui_text_unit_limit];
    if (themes)
    {
        struct ui_text_request count = {.id = UI_TEXT_THEME_COUNT,
                                        .language = language,
                                        .k = folio_theme_choice_count(),
                                        .n = folio_theme_choice_count()};
        if (ui_text_format(&count, text, sizeof text) != UI_TEXT_FORMAT_READY)
            return 0;
        int standard =
            wrapped_text(device, ui_text_line(UI_TEXT_THEME_GUIDANCE, language), bounds, false);
        int high = wrapped_text(device, ui_text_line(UI_TEXT_HIGH_CONTRAST_ACTIVE, language),
                                bounds, false);
        return wrapped_text(device, text, bounds, false) + (standard > high ? standard : high);
    }
    struct ui_text_request version = {
        .id = UI_TEXT_SETTINGS_VERSION, .language = language, .name = product_version_text()};
    if (ui_text_format(&version, text, sizeof text) != UI_TEXT_FORMAT_READY)
        return 0;
    return wrapped_text(device, text, bounds, false) +
           wrapped_text(device, ui_text_line(UI_TEXT_SETTINGS_IMMEDIATE, language), bounds, false) +
           wrapped_text(device, ui_text_line(UI_TEXT_SETTINGS_GUIDANCE, language), bounds, false);
}

static bool prepare_surface_metrics(struct folio_window *_Nonnull self)
{
    if (command_keys(self))
        return prepare_help_metrics(self);
    if (!settings_surface(self))
        return true;
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    struct command_surface_metrics metrics = {.width = bounds.right - bounds.left,
                                              .dpi = GetDpiForWindow(self->handle),
                                              .font = self->mono_font,
                                              .language = folio_state_language(self->state)};
    if (self->settings_metrics.width == metrics.width &&
        self->settings_metrics.dpi == metrics.dpi && self->settings_metrics.font == metrics.font &&
        self->settings_metrics.language == metrics.language)
        return true;
    HDC device = GetDC(self->command_layer);
    if (device == nullptr)
        return false;
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, metrics.dpi));
    bool ready = band_outcomes_height(device, metrics.language, bounds, &metrics.notice);
    metrics.settings_footer =
        metrics.notice + surface_footer_text_height(self, device, bounds, false);
    metrics.themes_footer = metrics.notice +
                            surface_footer_text_height(self, device, bounds, true) +
                            scale(ui_part_height + 8, metrics.dpi);
    SelectObject(device, previous);
    ReleaseDC(self->command_layer, device);
    if (ready)
        self->settings_metrics = metrics;
    return ready;
}

static bool band_commands_height(HDC device, enum folio_language language, RECT bounds,
                                 int *_Nonnull maximum)
{
    for (size_t row = 0; row < folio_command_count(); ++row)
    {
        char text[draw_unit_limit];
        struct ui_text_request request = {.id = UI_TEXT_FAILURE_COMMAND_NOTE_REQUIRED,
                                          .language = language,
                                          .name =
                                              folio_command_label(folio_command_at(row), language)};
        if (ui_text_format(&request, text, sizeof text) != UI_TEXT_FORMAT_READY)
            return false;
        if (!measure_band_reason(device, text, bounds, maximum))
            return false;
    }
    return true;
}

static bool band_status_height(HDC device, enum folio_language language, RECT bounds,
                               int *_Nonnull maximum)
{
    const enum ui_text statuses[] = {UI_TEXT_STATUS_SEARCH_MALFORMED,
                                     UI_TEXT_STATUS_SEARCH_BAD_SPAN,
                                     UI_TEXT_STATUS_SEARCH_NOT_FOUND, UI_TEXT_STATUS_SEARCH_FOUND,
                                     UI_TEXT_STATUS_REPLACE_COUNT_ONLY};
    for (size_t row = 0; row < sizeof statuses / sizeof statuses[0]; ++row)
    {
        char text[draw_unit_limit];
        struct ui_text_request request = {
            .id = statuses[row], .language = language, .k = SIZE_MAX, .n = SIZE_MAX};
        if (ui_text_format(&request, text, sizeof text) != UI_TEXT_FORMAT_READY)
            return false;
        if (!measure_band_reason(device, text, bounds, maximum))
            return false;
    }
    return true;
}

static bool band_offset_height(HDC device, enum folio_language language, RECT bounds,
                               int *_Nonnull maximum)
{
    char text[draw_unit_limit];
    size_t at =
        append_text(text, 0, folio_state_failure_line(FOLIO_STATE_REPLACE_BAD_PATTERN, language));
    struct ui_text_request request = {
        .id = UI_TEXT_STATUS_REPLACE_POSITION, .language = language, .offset = SIZE_MAX};
    if (ui_text_format(&request, text + at, sizeof text - at) != UI_TEXT_FORMAT_READY)
        return false;
    return measure_band_reason(device, text, bounds, maximum);
}

static bool band_reason_height(HDC device, UINT dpi, int width, int *_Nonnull out)
{
    RECT bounds = {0, 0, width, 0};
    int maximum = 0;
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        enum folio_language language = (enum folio_language)column;
        if (!band_outcomes_height(device, language, bounds, &maximum) ||
            !band_commands_height(device, language, bounds, &maximum) ||
            !band_status_height(device, language, bounds, &maximum) ||
            !band_offset_height(device, language, bounds, &maximum))
            return false;
    }
    int logical = (maximum * 96 + (int)dpi - 1) / (int)dpi;
    *out = scale((logical + 3) / 4 * 4, dpi);
    return *out > 0;
}

static int band_label_width(HDC device, enum ui_text first, enum ui_text second)
{
    int maximum = 0;
    for (size_t column = 0; column < folio_language_count; ++column)
    {
        int a = measure_utf8(device, ui_text_line(first, (enum folio_language)column));
        int b = measure_utf8(device, ui_text_line(second, (enum folio_language)column));
        int width = a > b ? a : b;
        maximum = width > maximum ? width : maximum;
    }
    return maximum;
}

static void band_action_layout(struct command_band_layout *_Nonnull layout, int right, int top)
{
    int button = scale(base_search_button_width, layout->dpi);
    int gap = scale(8, layout->dpi);
    int height = scale(ui_part_height, layout->dpi);
    layout->action[0] = (RECT){right - button * 2 - gap, top, right - button - gap, top + height};
    layout->action[1] = (RECT){right - button, top, right, top + height};
}

static void band_narrow_layout(struct command_band_layout *_Nonnull layout, RECT content)
{
    UINT dpi = layout->dpi;
    bool replacing = layout->mode == COMMAND_SURFACE_REPLACE;
    int gap = scale(8, dpi);
    int control = scale(ui_part_height, dpi);
    int second_top = content.top + control + scale(16, dpi);
    band_action_layout(layout, content.right, replacing ? second_top : content.top);
    layout->field[0] =
        (RECT){content.left, content.top, replacing ? content.right : layout->action[0].left - gap,
               content.top + control};
    layout->field[1] =
        (RECT){content.left, second_top, layout->action[0].left - gap, second_top + control};
    layout->label[1] =
        (RECT){content.left, layout->field[0].bottom, layout->field[1].right, second_top};
    int top = (replacing ? layout->field[1].bottom : layout->field[0].bottom) + scale(4, dpi);
    layout->result = (RECT){content.left, top, content.right, 0};
}

static void band_wide_layout(struct command_band_layout *_Nonnull layout, RECT content, int label)
{
    UINT dpi = layout->dpi;
    bool replacing = layout->mode == COMMAND_SURFACE_REPLACE;
    int gap = scale(8, dpi);
    int control = scale(ui_part_height, dpi);
    int left = content.left + label + gap;
    int first_top = scale(8, dpi);
    int second_top = scale(52, dpi);
    int right = replacing ? content.right : layout->close.left - gap;
    band_action_layout(layout, right, replacing ? second_top : first_top);
    layout->field[0] =
        (RECT){left, first_top, replacing ? layout->close.left - gap : layout->action[0].left - gap,
               first_top + control};
    layout->field[1] = (RECT){left, second_top, layout->action[0].left - gap, second_top + control};
    layout->label[0].right = left - gap;
    layout->label[1] = (RECT){content.left, second_top, left - gap, second_top + control};
    layout->result = (RECT){content.left, scale(replacing ? 96 : 52, dpi), content.right, 0};
}

static void band_geometry(struct command_band_layout *_Nonnull layout, HDC device)
{
    UINT dpi = layout->dpi;
    RECT bounds = {0, 0, layout->width, 0};
    RECT content = ui_part_surface_rect(bounds, dpi, UI_PART_SURFACE_CONTENT);
    layout->close = ui_part_surface_rect(bounds, dpi, UI_PART_SURFACE_CLOSE);
    layout->label[0] = ui_part_surface_rect(bounds, dpi, UI_PART_SURFACE_TITLE_TEXT);
    layout->label[0].top += scale(8, dpi);
    layout->label[0].bottom -= scale(8, dpi);
    bool replacing = layout->mode == COMMAND_SURFACE_REPLACE;
    enum ui_text first = replacing ? UI_TEXT_ACCESSIBLE_PATTERN : UI_TEXT_STATUS_SEARCH_FORWARD;
    enum ui_text second =
        replacing ? UI_TEXT_ACCESSIBLE_REPLACEMENT : UI_TEXT_STATUS_SEARCH_BACKWARD;
    int label = band_label_width(device, first, second);
    int left = content.left + label + scale(8, dpi);
    int right = replacing ? content.right : layout->close.left - scale(8, dpi);
    int remaining = right - scale(base_search_button_width * 2 + 16, dpi) - left;
    bool wide = remaining >= scale(144, dpi) &&
                layout->close.left - scale(8, dpi) - left >= scale(144, dpi);
    layout->narrow = !wide;
    if (wide)
        band_wide_layout(layout, content, label);
    if (!wide)
        band_narrow_layout(layout, content);
    int narrow = scale(288, dpi);
    int measured = content.right - content.left;
    layout->measured_width = measured < narrow ? measured : narrow;
}

/* 通常値/下限は4logical単位、resize中の不足分はpixelのまま配分して逆行を防ぐ。 */
static void reduce_band_spacing(int *_Nonnull spacing, int minimum, int *_Nonnull shortage)
{
    int available = *spacing - minimum;
    int taken = *shortage < available ? *shortage : available;
    *spacing -= taken;
    *shortage -= taken;
}

static void shift_band_content(struct command_band_layout *_Nonnull layout, int shift)
{
    for (size_t index = 0; index < replace_input_count; ++index)
    {
        OffsetRect(&layout->field[index], 0, -shift);
        OffsetRect(&layout->action[index], 0, -shift);
    }
    OffsetRect(&layout->label[1], 0, -shift);
    OffsetRect(&layout->result, 0, -shift);
}

static void fit_band_height(struct command_band_layout *_Nonnull layout)
{
    UINT dpi = layout->dpi;
    layout->pane_top = scale(base_pane_top, dpi);
    layout->pane_bottom = scale(base_pane_bottom, dpi);
    int trailing = scale(ui_part_edge, dpi);
    int title_gap = scale(16, dpi);
    int minimum = scale(layout->mode == COMMAND_SURFACE_REPLACE ? 32 : 76, dpi);
    int body = layout->available_height - layout->result.bottom - trailing - layout->pane_top -
               layout->pane_bottom;
    int shortage = minimum > body ? minimum - body : 0;
    reduce_band_spacing(&layout->pane_bottom, scale(8, dpi), &shortage);
    reduce_band_spacing(&layout->pane_top, scale(8, dpi), &shortage);
    reduce_band_spacing(&trailing, scale(8, dpi), &shortage);
    if (layout->narrow)
        reduce_band_spacing(&title_gap, scale(4, dpi), &shortage);
    shift_band_content(layout, scale(16, dpi) - title_gap);
    layout->height = layout->result.bottom + trailing;
}

static bool refresh_band_layout(struct folio_window *_Nonnull self)
{
    if (!command_band(self))
        return true;
    RECT pane = regions_for(self).pane;
    int width = pane.right - pane.left;
    UINT dpi = GetDpiForWindow(self->handle);
    int available = pane.bottom - pane.top - scale(base_caption_height + base_action_height, dpi);
    if (self->band.valid && self->band.width == width && self->band.available_height == available &&
        self->band.dpi == dpi && self->band.font == self->mono_font &&
        self->band.mode == self->command_surface)
        return true;
    HDC device = GetDC(self->handle);
    if (device == nullptr)
        return false;
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    int tracking = SetTextCharacterExtra(device, scale(base_tracking, dpi));
    struct command_band_layout layout = {.width = width,
                                         .available_height = available,
                                         .dpi = dpi,
                                         .font = self->mono_font,
                                         .mode = self->command_surface};
    band_geometry(&layout, device);
    bool reusable = self->band.valid && self->band.dpi == dpi &&
                    self->band.font == self->mono_font &&
                    self->band.measured_width == layout.measured_width;
    layout.result_height = self->band.result_height;
    bool measured =
        reusable || band_reason_height(device, dpi, layout.measured_width, &layout.result_height);
    if (tracking != tracking_unavailable)
        SetTextCharacterExtra(device, tracking);
    SelectObject(device, previous);
    ReleaseDC(self->handle, device);
    if (!measured)
        return false;
    layout.result.bottom = layout.result.top + layout.result_height;
    fit_band_height(&layout);
    layout.valid = true;
    self->band = layout;
    return true;
}

static bool result_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_EX ||
           self->command_surface == COMMAND_SURFACE_PALETTE ||
           self->command_surface == COMMAND_SURFACE_SEARCH ||
           self->command_surface == COMMAND_SURFACE_REPLACE ||
           self->command_surface == COMMAND_SURFACE_THEMES;
}

static void outcome_line(const struct folio_window *_Nonnull self, enum folio_state_outcome outcome,
                         char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    bool note_required = outcome == FOLIO_STATE_NOTHING_SELECTED && self->command_context;
    struct ui_text_request request = {
        .id = note_required ? UI_TEXT_FAILURE_COMMAND_NOTE_REQUIRED : UI_TEXT_COMMAND_CAPTION,
        .language = language,
        .name = note_required ? folio_command_label(self->reporting_command, language)
                              : folio_state_failure_line(outcome, language)};
    if (ui_text_format(&request, out, 1024) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void clear_report_outcome(struct folio_window *_Nonnull self)
{
    self->command_failure = FOLIO_STATE_READY;
    self->result_line[0] = '\0';
    if (self->notice == PANE_NOTICE_COMMAND_RESULT)
        set_pane_notice(self, PANE_NOTICE_NONE);
    redraw_command_layer(self);
}

static void report_outcome(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    if (outcome == FOLIO_STATE_READY)
    {
        clear_report_outcome(self);
        return;
    }
    if (outcome == FOLIO_STATE_CANCELLED)
    {
        if (result_surface(self))
            focus_command_input(self);
        return;
    }
    if (result_surface(self))
    {
        clear_command_status(self);
        outcome_line(self, outcome, self->result_line);
        self->command_failure = outcome;
        arrange_command_input(self);
        redraw_command_layer(self);
        focus_command_input(self);
        return;
    }
    if (!inline_outcome(outcome))
    {
        show_failure_box(self, outcome);
        return;
    }
    outcome_line(self, outcome, self->result_line);
    set_pane_notice(self, PANE_NOTICE_COMMAND_RESULT);
    arrange(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    char16_t units[draw_unit_limit];
    if (wide_units(self->result_line, units) > 0)
        accessibility_host_notify(self->accessibility,
                                  (struct accessible_item){.kind = ACCESSIBLE_NOTICE}, units);
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
    case NOTE_PANE_TEXT_NOT_READY:
        return FOLIO_STATE_PANE_LOAD_FAILED;
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
    return take_text(self, units, count);
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
        report_outcome(self, taken);
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
    clear_command_status(self);

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
    show_pane_layout(self);
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
    if (self->command_composing)
    {
        return;
    }
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
    show_pane_layout(self);
    update_search(self, direction, true);
    report_search(self);
}

/* n / Enter は覚えている向き、N / Shift+Enter はその逆向きへ 1 回だけ進む（ADR 0023 の決定 5）。 */
static void repeat_search(struct folio_window *_Nonnull self, bool reverse)
{
    enum search_direction direction = folio_state_search_direction(self->state);
    step_search(self, reverse ? reversed_direction(direction) : direction);
}

/* 面を閉じたときの区画: 本文が開いていれば本文、そうでなければ主窓
 * （ADR 0013 / ADR 0016 の補正 5）。 */
static HWND surface_focus_region(const struct folio_window *_Nonnull self)
{
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    if (pane != nullptr && pane_visible(self) && IsWindowVisible(pane) && IsWindowEnabled(pane) &&
        (folio_state_layout_mode(self->state) == FOLIO_LAYOUT_NOTE ||
         folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NONE))
    {
        return pane;
    }
    return self->handle;
}

/* rootはNOTEでは索引への復帰先ではない。空本文でも可視paneへ解釈する。 */
static HWND surface_return_target(const struct folio_window *_Nonnull self, HWND _Nullable window)
{
    if (window == self->handle && folio_state_layout_mode(self->state) == FOLIO_LAYOUT_NOTE)
    {
        return surface_focus_region(self);
    }
    if (window != nullptr && IsWindow(window) && IsWindowVisible(window))
    {
        return window;
    }
    return surface_focus_region(self);
}

static bool remember_wide_window(struct folio_window *_Nonnull self)
{
    self->wide_placement.length = sizeof self->wide_placement;
    RECT bounds;
    if (!GetWindowPlacement(self->handle, &self->wide_placement) ||
        !GetWindowRect(self->handle, &bounds))
    {
        return false;
    }
    self->restore_maximized = IsZoomed(self->handle);
    RECT normal = self->restore_maximized ? self->wide_placement.rcNormalPosition : bounds;
    self->wide_width =
        MulDiv(normal.right - normal.left, base_dpi, (int)GetDpiForWindow(self->handle));
    return true;
}

/* 現位置/高さを保つ。画面外の場合だけ幅と共通頭の位置をwork areaへ収める。 */
static bool set_layout_width(const struct folio_window *_Nonnull self, int logical_width)
{
    RECT bounds;
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    if (!GetWindowRect(self->handle, &bounds) ||
        !GetMonitorInfoW(MonitorFromWindow(self->handle, MONITOR_DEFAULTTONEAREST), &monitor))
    {
        return false;
    }
    int width = scale(logical_width, GetDpiForWindow(self->handle));
    int available = monitor.rcWork.right - monitor.rcWork.left;
    width = width > available ? available : width;
    int left = bounds.left;
    if (left + width > monitor.rcWork.right)
    {
        left = monitor.rcWork.right - width;
    }
    if (left < monitor.rcWork.left)
    {
        left = monitor.rcWork.left;
    }
    int top = bounds.top;
    int header = regions_for(self).header.bottom;
    if (top + header > monitor.rcWork.bottom)
    {
        top = monitor.rcWork.bottom - header;
    }
    if (top < monitor.rcWork.top)
    {
        top = monitor.rcWork.top;
    }
    return SetWindowPos(self->handle, nullptr, left, top, width, bounds.bottom - bounds.top,
                        SWP_NOZORDER | SWP_NOACTIVATE);
}

static bool resize_layout_window(struct folio_window *_Nonnull self,
                                 enum folio_layout_mode previous, enum folio_layout_mode mode)
{
    if (mode == FOLIO_LAYOUT_DRAWER)
    {
        if (self->restore_maximized)
        {
            ShowWindow(self->handle, SW_SHOWNOACTIVATE);
        }
        return set_layout_width(self, base_drawer_window_width);
    }
    if (previous != FOLIO_LAYOUT_DRAWER)
    {
        return true;
    }
    if (self->restore_maximized)
    {
        return SetWindowPlacement(self->handle, &self->wide_placement);
    }
    if (IsZoomed(self->handle))
    {
        ShowWindow(self->handle, SW_SHOWNOACTIVATE);
        if (IsZoomed(self->handle) || IsIconic(self->handle))
            return false;
    }
    return set_layout_width(self, self->wide_width);
}

/* 一連の同期WM_SIZEも同じapplication値を使い、失敗なら元の配置へ戻す。 */
static bool change_layout_geometry(struct folio_window *_Nonnull self,
                                   enum folio_layout_mode previous, enum folio_layout_mode mode)
{
    WINDOWPLACEMENT before = {.length = sizeof before};
    if (!GetWindowPlacement(self->handle, &before))
    {
        return false;
    }
    if (mode == FOLIO_LAYOUT_DRAWER && !remember_wide_window(self))
    {
        return false;
    }
    self->layout_changing = true;
    folio_state_set_layout_mode(self->state, mode);
    bool changed = resize_layout_window(self, previous, mode);
    if (!changed)
    {
        folio_state_set_layout_mode(self->state, previous);
        SetWindowPlacement(self->handle, &before);
    }
    arrange(self);
    self->layout_changing = false;
    return changed;
}

/* ノートのみで文書が無いと本文も索引も見えないので、F6 の先頭（新しいノート）に印を出して
 * focus を置く（#349）。表示切替の復帰と、OS が root へ戻した focus の正規化が同じ 1 本を使う。 */
static void focus_header_entry(struct folio_window *_Nonnull self)
{
    self->keyboard_target = ACCESSIBLE_NEW_NOTE;
    show_focus_cues(self, true);
    SetFocus(self->handle);
    InvalidateRect(self->handle, nullptr, FALSE);
}

static void restore_layout_focus(struct folio_window *_Nonnull self, HWND _Nullable focus)
{
    if (focus == self->handle && accessible_visible(self, self->keyboard_target))
    {
        SetFocus(self->handle);
        return;
    }
    switch (folio_state_layout_mode(self->state))
    {
    case FOLIO_LAYOUT_DRAWER:
        self->keyboard_target = ACCESSIBLE_TREE;
        SetFocus(self->handle);
        return;
    case FOLIO_LAYOUT_NOTE:
        if (!body_shown(self))
        {
            focus_header_entry(self);
            return;
        }
        SetFocus(self->pane == nullptr ? self->handle : note_pane_handle(self->pane));
        return;
    case FOLIO_LAYOUT_BOTH:
        SetFocus(surface_return_target(self, focus));
        return;
    }
}

static void apply_layout_mode(struct folio_window *_Nonnull self, enum folio_layout_mode mode)
{
    enum folio_layout_mode previous = folio_state_layout_mode(self->state);
    if (previous == mode)
    {
        return;
    }
    HWND focus = GetFocus();
    size_t line = self->pane == nullptr ? 0 : note_pane_first_visible_line(self->pane);
    bool changed = change_layout_geometry(self, previous, mode);
    if (self->pane != nullptr && pane_visible(self))
    {
        note_pane_scroll_to_line(self->pane, line);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    restore_layout_focus(self, focus);
    if (!changed)
    {
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
    }
}

static void show_pane_layout(struct folio_window *_Nonnull self)
{
    if (!pane_visible(self))
    {
        apply_layout_mode(self, FOLIO_LAYOUT_BOTH);
    }
}

static void show_drawer_layout(struct folio_window *_Nonnull self)
{
    if (!drawer_visible(self))
    {
        apply_layout_mode(self, FOLIO_LAYOUT_BOTH);
    }
}

static void invalidate_surface_items(struct folio_window *_Nonnull self)
{
    if (self->surface_revision != UINT64_MAX)
    {
        self->surface_revision += 1;
    }
    accessibility_host_schedule(self->accessibility);
}

[[nodiscard]] static bool set_command_surface(struct folio_window *_Nonnull self,
                                              enum command_surface_mode mode)
{
    if (self->command_surface == COMMAND_SURFACE_THEMES && mode != COMMAND_SURFACE_THEMES &&
        !preview_theme(self, folio_state_theme_choice(self->state)))
        return false;
    self->surface_tracking = false;
    bool captured = self->surface_pressed < surface_pointer_count;
    self->surface_hover = surface_pointer_count;
    self->surface_pressed = surface_pointer_count;
    self->surface_pressed_inside = false;
    if (captured && GetCapture() == self->command_layer)
        ReleaseCapture();
    if (mode != COMMAND_SURFACE_CLOSED)
    {
        set_hover(self, ACCESSIBLE_WINDOW); /* 面の下の部品は hover できない */
    }
    bool closing = self->command_surface == COMMAND_SURFACE_MENU && mode != COMMAND_SURFACE_MENU;
    if (closing)
        accessibility_host_menu(self->accessibility, ACCESSIBLE_MENU_CLOSED);
    invalidate_surface_items(self);
    self->command_surface = mode;
    if (closing)
    {
        self->menu_pressed = SIZE_MAX;
        self->menu_key = 0;
        if (GetCapture() == self->command_layer)
            ReleaseCapture();
    }
    return true;
}

/* WM_SETTEXTが同期で送るEN_CHANGEは初期化途中の配置をしない。利用者入力には使わない。 */
static void initialize_command_input(struct folio_window *_Nonnull self,
                                     const wchar_t *_Nonnull text)
{
    bool previous = self->command_initializing;
    self->command_initializing = true;
    clear_command_status(self);
    if (self->command_input != nullptr)
        SetWindowTextW(self->command_input, text);
    if (self->command_candidates != nullptr)
        command_candidates_invalidate(self->command_candidates);
    if (self->theme_candidates != nullptr)
        theme_candidates_invalidate(self->theme_candidates);
    self->command_initializing = previous;
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
    self->command_return_focus = surface_return_target(self, GetFocus());
    if (!set_command_surface(self, surface))
        return;
    self->command_selection = surface == COMMAND_SURFACE_SETTINGS ? settings_initial_row(self) : 0;
    self->command_first = 0;
    self->command_keys_visible = false;
    self->command_help_scroll = 0;
    initialize_command_input(self, surface == COMMAND_SURFACE_EX ? L":" : L"");
    rearrange_keeping_line(self);
    focus_command_input(self);
    /* WM_SETTEXT はキャレットを先頭へ置き、EM_SETSEL(-1, -1) は選択を解くだけで動かさない。
     * 打った字が `:` の前に入らないよう、末尾（Ex は 1・他の面は 0）へ明示的に置く（#85）。 */
    LRESULT caret = (LRESULT)GetWindowTextLengthW(self->command_input);
    SendMessageW(self->command_input, EM_SETSEL, (WPARAM)caret, (LPARAM)caret);
    redraw_command_layer(self);
}

/* 履歴の面を離れるときは application が持つ版を捨てる（ADR 0038 の決定 10）。
 * 面を隠す経路と、開いた面を別の面へ差し替える経路の両方がここを通る。 */
static void release_history(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        folio_state_close_history(self->state);
    }
}

/* 面を隠す経路はここ 1 か所。面がフォーカスを持っていれば、隠す前に覚えた戻り先
 * （可視でなければ区画）へ返す。Windows は隠した窓のフォーカスを動かさない
 * （ADR 0016 の補正 5・#132）。 */
[[nodiscard]] static bool hide_command_surface(struct folio_window *_Nonnull self)
{
    release_history(self);
    HWND _Nullable focus = GetFocus();
    bool surface_focused = focus != nullptr && self->command_layer != nullptr &&
                           (focus == self->command_layer || IsChild(self->command_layer, focus));
    HWND _Nullable remembered = self->command_return_focus;
    if (!set_command_surface(self, COMMAND_SURFACE_CLOSED))
        return false;
    self->command_return_focus = nullptr;
    self->command_selection = 0;
    self->command_help_scroll = 0;
    clear_command_status(self);
    rearrange_keeping_line(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    if (surface_focused)
    {
        SetFocus(surface_return_target(self, remembered));
    }
    return true;
}

/* OS幾何入口だけでMENUを失効。hideが実focus所有時だけ復帰する。 */
static void close_geometry_menu(struct folio_window *_Nonnull self)
{
    if (self->command_surface != COMMAND_SURFACE_MENU)
        return;
    cancel_part_press(self);
    (void)hide_command_surface(self);
}

static void close_command_surface(struct folio_window *_Nonnull self)
{
    HWND _Nullable focus = self->command_return_focus;
    if (!hide_command_surface(self))
        return;
    SetFocus(surface_return_target(self, focus));
}

static void dismiss_command_surface(struct folio_window *_Nonnull self)
{
    if (!hide_command_surface(self))
        return;
}

static void show_command_palette(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        return;
    }
    release_history(self);
    if (!set_command_surface(self, COMMAND_SURFACE_PALETTE))
        return;
    self->command_selection = 0;
    self->command_first = 0;
    self->command_keys_visible = false;
    self->command_help_scroll = 0;
    initialize_command_input(self, L"");
    rearrange_keeping_line(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

/* 検索欄は ADR 0016 の入力面そのもの。元のフォーカスは open_command_surface が覚え、
 * Esc の close_command_surface が返す。覚えている語を初期値にして全選択する
 * （語は欄を空にする EN_CHANGE より先に写しておく）。 */
static void show_search_surface(struct folio_window *_Nonnull self)
{
    show_pane_layout(self);
    struct utf16_text *_Nullable term = nullptr;
    if (utf16_text_create(folio_state_search_term(self->state),
                          folio_state_search_term_length(self->state),
                          &term) != UTF16_TEXT_CONVERTED)
    {
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_SEARCH);
    }
    else
    {
        release_history(self);
        if (!set_command_surface(self, COMMAND_SURFACE_SEARCH))
        {
            utf16_text_destroy(term);
            return;
        }
        clear_command_status(self);
        rearrange_keeping_line(self);
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
        report_outcome(self, FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    show_search_surface(self);
}

/* 置換の欄も ADR 0016 の入力面そのもの。開き方・戻り先 HWND・Esc の規則は検索欄と同じで、
 * 違うのは EDIT が 2 つあることだけ（ADR 0016 の 2026-09-22 の補正）。
 * 語は欄が覚えたまま残し、開いた瞬間に下見を取り直して件数を出す。 */
static void show_replace_surface(struct folio_window *_Nonnull self)
{
    show_pane_layout(self);
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_REPLACE);
    }
    else
    {
        release_history(self);
        if (!set_command_surface(self, COMMAND_SURFACE_REPLACE))
            return;
        clear_command_status(self);
        rearrange_keeping_line(self);
        focus_command_input(self);
    }
    update_replace_preview(self);
}

/* 表示中のノートが無ければ欄を開かない（検索と同じ）。閲覧中は開いて NOT_EDITING の 1 行を
 * 出し、本文もモードも変えない（ADR 0028 の決定 8(c)）。 */
static void begin_replace(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        report_outcome(self, FOLIO_STATE_NOTHING_SELECTED);
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
static void show_settings_surface(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_SETTINGS);
        return; /* open_command_surfaceが初期選択と配置まで確定する。 */
    }
    else
    {
        release_history(self);
        if (!set_command_surface(self, COMMAND_SURFACE_SETTINGS))
            return;
        initialize_command_input(self, L"");
    }
    /* 他の入力面から来た場合も、現在値の行を決めてから配置する。 */
    self->command_selection = settings_initial_row(self);
    rearrange_keeping_line(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

/* テーマ候補は同じ設定レイヤーと検索 EDIT を共有する（ADR0049）。 */
static void show_theme_surface(struct folio_window *_Nonnull self)
{
    /* Clear while the previous face still owns EN_CHANGE; opening must not preview row zero. */
    initialize_command_input(self, L"");
    if (!set_command_surface(self, COMMAND_SURFACE_THEMES))
        return;
    clear_command_status(self);
    self->command_selection = (size_t)folio_state_theme_choice(self->state);
    self->command_first = 0;
    self->theme_wheel_remainder = 0;
    rearrange_keeping_line(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

/* 履歴の面（ADR 0038 の決定 12・14）。設定画面と同じく EDIT を持たず、レイヤーがフォーカスを取る。
 * 行を決めてから配置する（show_settings_surface と同じ順）。最初の行（1 = 最新）を選ぶ。 */
static void show_history_surface(struct folio_window *_Nonnull self)
{
    show_pane_layout(self);
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        open_command_surface(self, COMMAND_SURFACE_HISTORY);
    }
    else
    {
        if (!set_command_surface(self, COMMAND_SURFACE_HISTORY))
            return;
        clear_command_status(self);
    }
    self->command_selection = 0;
    rearrange_keeping_line(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

/* 履歴の一覧を開く（決定 11・13）。空・無題・未選択は面を開かず、既存の失敗の出し方に任せる。 */
static void execute_history_command(struct folio_window *_Nonnull self,
                                    const char *_Nonnull argument)
{
    (void)argument;
    enum folio_state_outcome outcome = folio_state_open_history(self->state);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    show_history_surface(self);
}

/* `/` と `?` は向きを決めてから同じ欄を開く（ADR 0023 の決定 4）。 */
static void open_search(struct folio_window *_Nonnull self, enum search_direction direction)
{
    folio_state_set_search_direction(self->state, direction);
    begin_search(self);
}

/* 初回/別名の名前を同じ入力面またはExから受け、同じ作成へ渡す。 */
static enum folio_state_outcome save_destination(struct folio_window *_Nonnull self,
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
        return show_name_prompt(self, &request).outcome;
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

static enum folio_state_outcome command_save_as(struct folio_window *_Nonnull self,
                                                const char *_Nonnull argument)
{
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
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
static enum folio_state_outcome rename_destination(struct folio_window *_Nonnull self,
                                                   const char *_Nonnull argument,
                                                   const char16_t *_Nonnull units, size_t count)
{
    if (argument[0] == '\0')
    {
        struct name_prompt_request request = {
            .state = self->state, .kind = NAME_PROMPT_RENAME, .units = units, .count = count};
        return show_name_prompt(self, &request).outcome;
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
static enum folio_state_outcome command_rename(struct folio_window *_Nonnull self,
                                               const char *_Nonnull argument)
{
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        return prepared;
    }
    switch (folio_state_document_kind(self->state))
    {
    case FOLIO_DOCUMENT_NONE:
        return FOLIO_STATE_NOTHING_SELECTED;
    case FOLIO_DOCUMENT_UNTITLED:
        return FOLIO_STATE_NAME_REQUIRED;
    case FOLIO_DOCUMENT_NAMED:
        break;
    }
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
        report_outcome(self, outcome);
        return false;
    }
    report_outcome(self, FOLIO_STATE_READY);
    InvalidateRect(self->handle, nullptr, FALSE);
    return true;
}

static void finish_save_command(struct folio_window *_Nonnull self,
                                enum folio_state_outcome outcome)
{
    /* 公開後の台帳失敗でも版は破棄される。理由の箱へ入る前に古い面を閉じる（ADR 0038）。 */
    bool empty_history = self->command_surface == COMMAND_SURFACE_HISTORY &&
                         folio_state_history_count(self->state) == 0;
    HWND _Nullable focus = nullptr;
    if (empty_history)
    {
        close_command_surface(self);
        focus = GetFocus();
    }
    if (report_save(self, outcome) && self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        close_command_surface(self);
    }
    restore_valid_focus(self, focus);
}

/* 改名は本文を流し込み直さず、保持する入力面と元focusを触らない（ADR0041）。 */
static void finish_rename_command(struct folio_window *_Nonnull self,
                                  enum folio_state_outcome outcome, HWND _Nullable focus)
{
    bool empty_history = self->command_surface == COMMAND_SURFACE_HISTORY &&
                         folio_state_history_count(self->state) == 0;
    bool completed_command =
        outcome == FOLIO_STATE_READY && (self->command_surface == COMMAND_SURFACE_EX ||
                                         self->command_surface == COMMAND_SURFACE_PALETTE);
    if (empty_history || completed_command)
    {
        close_command_surface(self);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
    }
    redraw_drawer(self);
    redraw_command_layer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    restore_valid_focus(self, focus);
}

/* 入力面（Ex・パレット・検索欄・置換の 2 欄・ドロワーの絞り込み欄）の中の Ctrl+S
 * （ADR 0016 の 2026-09-22 の補正 2）。保存は本文の Ctrl+S と同じ command_save を通り、
 * 違うのは**欄を閉じないこと**だけである。欄の語・キャレット・選択は触らない。
 * 失敗の箱や名前入力の面はフォーカスを動かすので、押した欄へ返す。 */
static void store_from_surface(struct folio_window *_Nonnull self)
{
    HWND _Nullable focus = GetFocus();
    (void)report_save(self, command_save(self));
    /* 保存が済むと application は履歴の一覧を捨てる（ADR 0038 の決定 10）。古い行を残さない。 */
    if (self->command_surface == COMMAND_SURFACE_HISTORY &&
        folio_state_history_count(self->state) == 0)
    {
        close_command_surface(self);
        return;
    }
    if (focus != nullptr && IsWindow(focus))
    {
        SetFocus(focus);
    }
}

static void execute_save_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        finish_save_command(self, prepared);
        return;
    }
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

/* 登録失敗を成功にせず、解除失敗の所有は次の取消/承認/終了まで保つ。 */
static void clear_session_reason(struct folio_window *_Nonnull self)
{
    if (self->session_reason_owned && self->handle != nullptr &&
        ShutdownBlockReasonDestroy(self->handle) != 0)
    {
        self->session_reason_owned = false;
    }
    self->session_reason = UI_TEXT_EMPTY;
    if (self->notice == PANE_NOTICE_SESSION_BLOCKED)
    {
        set_pane_notice(self, PANE_NOTICE_NONE);
    }
}

static LRESULT reject_session(struct folio_window *_Nonnull self, enum ui_text reason)
{
    self->session_reason = reason;
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    const char *_Nonnull text = ui_text_line(reason, folio_state_language(self->state));
    if (utf16_text_fill(text, units, ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY &&
        ShutdownBlockReasonCreate(self->handle, units) != 0)
    {
        self->session_reason_owned = true;
    }
    set_pane_notice(self, PANE_NOTICE_SESSION_BLOCKED);
    InvalidateRect(self->handle, nullptr, FALSE);
    return FALSE;
}

static bool session_composing(const struct folio_window *_Nonnull self)
{
    return self->root_composing || self->command_composing || self->filter_composing ||
           (self->pane != nullptr && note_pane_composing(self->pane));
}

/* store_bodyの改名復旧や無題保存は対話を開くため、呼び出す前に拒否する。 */
static enum ui_text session_unready_reason(const struct folio_window *_Nonnull self)
{
    if (!IsWindowEnabled(self->handle) || self->removal_running || self->native_modal_depth > 0 ||
        self->close_phase != CLOSE_PHASE_IDLE)
    {
        return UI_TEXT_SESSION_BUSY;
    }
    if (session_composing(self))
    {
        return UI_TEXT_SESSION_COMPOSING;
    }
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_UNTITLED)
    {
        return UI_TEXT_SESSION_UNTITLED;
    }
    struct rename_view pending = {.from = "", .to = ""};
    return folio_state_rename_pending(self->state, &pending) ? UI_TEXT_SESSION_RENAME_PENDING
                                                             : UI_TEXT_EMPTY;
}

/* 保存中の取消はCANCELLEDに留め、外側QUERYが戻るまで新しい保存を始めさせない。 */
static void cancel_session(struct folio_window *_Nonnull self)
{
    switch (self->session_phase)
    {
    case SESSION_PHASE_IDLE:
    case SESSION_PHASE_CANCELLED:
        break;
    case SESSION_PHASE_CHECKING:
        self->session_phase = SESSION_PHASE_CANCELLED;
        break;
    case SESSION_PHASE_APPROVED:
        self->session_phase = SESSION_PHASE_IDLE;
        EnableWindow(self->handle, TRUE);
        restore_valid_focus(self, self->session_return_focus);
        self->session_return_focus = nullptr;
        break;
    }
    clear_session_reason(self);
}

static LRESULT query_session_end(struct folio_window *_Nonnull self)
{
    switch (self->session_phase)
    {
    case SESSION_PHASE_APPROVED:
        return TRUE;
    case SESSION_PHASE_CHECKING:
    case SESSION_PHASE_CANCELLED:
        return reject_session(self, UI_TEXT_SESSION_BUSY);
    case SESSION_PHASE_IDLE:
        break;
    }
    enum ui_text reason = session_unready_reason(self);
    if (reason != UI_TEXT_EMPTY)
    {
        return reject_session(self, reason);
    }
    self->session_phase = SESSION_PHASE_CHECKING;
    enum folio_state_outcome saved = store_body(self);
    if (self->session_phase == SESSION_PHASE_CANCELLED)
    {
        self->session_phase = SESSION_PHASE_IDLE;
        clear_session_reason(self);
        return FALSE;
    }
    if (saved != FOLIO_STATE_READY)
    {
        self->session_phase = SESSION_PHASE_IDLE;
        return reject_session(self, UI_TEXT_SESSION_SAVE_INCOMPLETE);
    }
    clear_session_reason(self);
    if (self->session_phase == SESSION_PHASE_CANCELLED)
    {
        self->session_phase = SESSION_PHASE_IDLE;
        return FALSE;
    }
    self->session_phase = SESSION_PHASE_APPROVED;
    self->session_return_focus = GetFocus();
    EnableWindow(self->handle, FALSE);
    return self->session_phase == SESSION_PHASE_APPROVED ? TRUE : FALSE;
}

static void finish_quit(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    clear_session_reason(self);
    DestroyWindow(self->handle);
}

static LRESULT end_session(struct folio_window *_Nonnull self, WPARAM ending)
{
    if (ending == FALSE)
    {
        cancel_session(self);
    }
    if (ending != FALSE && self->session_phase == SESSION_PHASE_APPROVED)
    {
        finish_quit(self, FOLIO_STATE_READY);
    }
    return 0;
}

static void run_normal_close(struct folio_window *_Nonnull self)
{
    self->close_phase = CLOSE_PHASE_RUNNING;
    finish_quit(self, depart_document(self).outcome);
    self->close_phase = CLOSE_PHASE_IDLE;
    if (self->handle != nullptr)
        refresh_modal_availability(self);
}

static void close_window(struct folio_window *_Nonnull self)
{
    if (!shell_ready(self))
        return;
    if (self->close_phase == CLOSE_PHASE_RUNNING)
    {
        modal_owner_capture_focus(self->modal_owner);
        modal_owner_restore_focus(self->modal_owner);
        return;
    }
    if (self->close_phase == CLOSE_PHASE_IDLE)
    {
        self->close_phase = CLOSE_PHASE_REQUESTED;
        accessibility_host_invoked(self->accessibility,
                                   (struct accessible_item){.kind = ACCESSIBLE_CLOSE});
    }
    if (self->native_modal_depth == 0)
    {
        return;
    }
    self->close_phase = CLOSE_PHASE_REQUESTED;
    switch (modal_owner_cancel(self->modal_owner))
    {
    case MODAL_CANCEL_ACCEPTED:
    case MODAL_CANCEL_NOT_VISIBLE:
        break;
    case MODAL_CANCEL_REJECTED:
        self->close_phase = CLOSE_PHASE_IDLE;
        break;
    }
}

static void resume_normal_close(struct folio_window *_Nonnull self)
{
    if (self->close_phase == CLOSE_PHASE_REQUESTED && self->native_modal_depth == 0 &&
        self->dispatch_depth == 1 && !session_busy(self))
        run_normal_close(self);
}

static void execute_quit_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    finish_quit(self, command_quit(self));
}

static void execute_save_quit_command(struct folio_window *_Nonnull self,
                                      const char *_Nonnull argument)
{
    (void)argument;
    finish_quit(self, store_body(self));
}

static void execute_mode_command(struct folio_window *_Nonnull self, enum pane_mode mode)
{
    show_pane_layout(self);
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
        report_outcome(self, outcome);
        return;
    }
    if (!hide_command_surface(self))
        return;
    switch (mode)
    {
    case PANE_MODE_EDIT:
        focus_pane(self);
        return;
    case PANE_MODE_VIEW:
        SetFocus(surface_return_target(self, self->handle));
        return;
    }
}

/* 作成先は離脱確認の前に確定する。旧無題の保存先を変えても新規の宛先を変えない（ADR0051）。 */
static void new_note_at(struct folio_window *_Nonnull self, size_t category)
{
    struct name_prompt_result departure = depart_document(self);
    enum folio_state_outcome saved = departure.outcome;
    if (saved == FOLIO_STATE_READY)
    {
        saved = folio_state_new_note(self->state, category, departure.untitled);
    }
    if (saved != FOLIO_STATE_READY)
    {
        report_outcome(self, saved);
        return;
    }
    show_pane_layout(self);
    if (!hide_command_surface(self))
        return;
    saved = pour_edit_rtf(self);
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    if (saved != FOLIO_STATE_READY)
    {
        report_outcome(self, saved);
        return;
    }
    focus_pane(self);
}

/* Ex・パレット・既存入口は現在カテゴリを解き、対象付きの共通受け口へ渡す。 */
static void execute_new_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    new_note_at(self, folio_state_current_category(self->state));
}

/* 語ごとの行き先（ADR 0026 の決定 8）。切替はいまの値から導き、UI に第 2 の真偽を持たない。 */
/* 本文の矩形が変わる操作の前後で、最初に見える論理行を保つ（ADR 0026 の決定 6）。
 * 本文・Undo・選択には触らない。折り返しの途中から見えていた場合は行の先頭へ寄る。
 * 左端・幅・高さが変わったときだけ戻す。layoutの同期WM_SIZE全体は外側で保持する。 */
static void rearrange_keeping_line(struct folio_window *_Nonnull self)
{
    RECT before = pane_bounds(self);
    size_t line = self->pane == nullptr ? 0 : note_pane_first_visible_line(self->pane);
    arrange(self);
    RECT after = pane_bounds(self);
    if (self->pane != nullptr && pane_visible(self) && !EqualRect(&before, &after))
    {
        note_pane_scroll_to_line(self->pane, line);
    }
    if (!settings_surface(self) || !EqualRect(&before, &after))
        InvalidateRect(self->handle, nullptr, FALSE);
}

/* 3 つの入口（`:set` の語・パレット／メニューの切替・#38 の設定画面）が通る唯一の意図。 */
static void apply_number(struct folio_window *_Nonnull self, bool number)
{
    enum folio_state_outcome outcome = folio_state_set_number(self->state, number);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    if (settings_surface(self))
    {
        clear_command_status(self);
    }
    else
    {
        if (!hide_command_surface(self))
            return;
    }
    rearrange_keeping_line(self);
    redraw_command_layer(self);
}

/* 閲覧の本文を新しい RTF で流し直し、選択を戻す（ADR 0031 の決定 6 の補正 1）。
 * **実測の補正**: EM_EXSETSEL は EM_SCROLLCARET を送らなくても選択を見える位置へ寄せる。
 * 空の選択を戻すと先頭へ飛ぶので、選んでいる一致があるときだけ戻す（ADR 0023 の
 * ハイライトは保ち、選んでいないときは流し直しが保ったスクロール位置をそのまま残す）。 */
static enum folio_state_outcome restream_pane(struct folio_window *_Nonnull self)
{
    size_t start = 0;
    size_t end = 0;
    bool had = note_pane_selection(self->pane, &start, &end);
    enum folio_state_outcome outcome = render_pane(self);
    if (outcome == FOLIO_STATE_READY && had && end > start)
    {
        note_pane_restore_selection(self->pane, (struct note_search_span){start, end});
    }
    return outcome;
}

/* 色を持つものを全部当て直す（ADR 0031 の決定 6）。palette の写しは主窓・ドロワー・
 * command_brush の 3 か所で、縁の色は decorate を呼び直す（何度でも可・実測）。
 * 閲覧は選択を退避して流し直し、編集は色を当て直すだけ（本文・Undo・変更印を守る）。
 * **最初に見える論理行を退避して戻す**（ADR 0031 の 2026-09-22 の補正 2）。
 * キャレットが画面の外にあると EM_SETCHARFORMAT がキャレットまでスクロールするため。 */
static enum folio_state_outcome recolor_pane(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    enum pane_mode mode = folio_state_pane_mode(self->state);
    size_t line = note_pane_first_visible_line(self->pane);
    enum note_pane_recolor_outcome colored =
        note_pane_recolor(self->pane, self->palette.pane, self->palette.editor_text, mode);
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    if (colored == NOTE_PANE_RECOLOR_FAILED)
    {
        outcome = FOLIO_STATE_PANE_RECOLOR_FAILED;
    }
    if (outcome == FOLIO_STATE_READY && mode == PANE_MODE_VIEW)
    {
        outcome = restream_pane(self);
    }
    if (line > 0 && !pane_unavailable(self))
    {
        note_pane_scroll_to_line(self->pane, line);
    }
    return outcome;
}

static void apply_pane_face(const struct folio_window *_Nonnull self)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(folio_state_language(self->state), face);
    note_pane_reface(self->pane, face);
}

/* 言語の切り替えで本文の書体を当て直す（ADR 0032 の決定 6(c)）。
 * 編集は既定書式の face だけを当て、閲覧は新しい fonttbl の RTF を流し直す（順が違う）。
 * face は再折り返しを起こすので、最初に見える論理行を退避して最後に戻す。 */
static enum folio_state_outcome reface_pane(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    size_t line = note_pane_first_visible_line(self->pane);
    /* **既定書式の face はモードによらず当てる。** `SCF_DEFAULT` は既存の run を塗らないので、
     * RTF が face を明示している閲覧の見た目は変わらない。当てないと、起動直後（閲覧）や
     * 閲覧のまま言語を変えたあとに `i` で編集へ入った本文が RichEdit の既定 face になる
     * （平文の流し込みは既定書式で描かれる）。`note_pane_recolor` が閲覧でも `SCF_DEFAULT` を
     * 無条件に当てているのと同じ理屈である（ADR 0031 の補正 9・ADR 0032 の補正 4）。 */
    apply_pane_face(self);
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        outcome = restream_pane(self);
    }
    if (outcome == FOLIO_STATE_READY && line > 0)
    {
        note_pane_scroll_to_line(self->pane, line);
    }
    return outcome;
}

static void recolor_window(struct folio_window *_Nonnull self)
{
    self->palette = folio_palette_from(folio_state_palette(self->state));
    if (self->settings_backdrop != nullptr)
    {
        SetLayeredWindowAttributes(self->settings_backdrop, 0, self->palette.dialog_backdrop_alpha,
                                   LWA_ALPHA);
        InvalidateRect(self->settings_backdrop, nullptr, TRUE);
    }
    refresh_tooltip_style(self);
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
    enum folio_state_outcome outcome = recolor_pane(self);
    self->appearance_recolor_pending = outcome != FOLIO_STATE_READY;
    if (outcome == FOLIO_STATE_READY && self->command_failure == FOLIO_STATE_PANE_RECOLOR_FAILED)
    {
        clear_command_status(self);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    /* 主窓は WS_CLIPCHILDREN なので、上の無効化は子の EDIT に届かない（#121）。 */
    if (self->filter_input != nullptr)
    {
        InvalidateRect(self->filter_input, nullptr, TRUE);
    }
    arrange_command_input(self);
    redraw_command_layer(self);
    if (outcome != FOLIO_STATE_READY && !session_busy(self) && IsWindowEnabled(self->handle))
    {
        report_outcome(self, outcome);
    }
}

/* One UI path for preview and cancellation, including retry of native recolor failures. */
[[nodiscard]] static bool preview_theme(struct folio_window *_Nonnull self,
                                        enum folio_theme_choice choice)
{
    if (pane_streaming(self))
        return false;
    uint64_t before = folio_state_palette_revision(self->state);
    enum folio_state_outcome outcome = folio_state_preview_theme(self->state, choice);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return false;
    }
    if (folio_state_palette_revision(self->state) != before || self->appearance_recolor_pending)
        recolor_window(self);
    return !self->appearance_recolor_pending;
}

/* 2 つの入口（設定画面の Enter と `:set theme=`）が通る唯一の意図（ADR 0031 の決定 3）。
 * 設定画面は採用しても閉じず、印を移したまま残る（決定 7）。 */
static void apply_theme(struct folio_window *_Nonnull self, enum folio_theme_choice choice)
{
    if (pane_streaming(self))
    {
        return;
    }
    uint64_t before = folio_state_palette_revision(self->state);
    enum folio_state_outcome outcome = folio_state_set_theme(self->state, choice);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    if (settings_surface(self))
    {
        clear_command_status(self);
    }
    else
    {
        if (!hide_command_surface(self))
            return;
    }
    if (folio_state_palette_revision(self->state) != before || self->appearance_recolor_pending)
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
    enum folio_state_outcome outcome = reface_pane(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    redraw_command_layer(self);
    if (outcome != FOLIO_STATE_READY && !session_busy(self))
    {
        report_outcome(self, outcome);
    }
}

/* 2 つの入口（設定画面の Enter と `:set language=`）が通る唯一の意図（ADR 0032 の決定 5）。
 * 設定画面は採用しても閉じず、印を移したまま残る（apply_theme と同じ流儀）。 */
static void apply_language(struct folio_window *_Nonnull self, enum folio_language language)
{
    if (pane_streaming(self))
    {
        return;
    }
    enum folio_language before = folio_state_language(self->state);
    enum folio_state_outcome outcome = folio_state_set_language(self->state, language);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    refresh_part_metrics(self, GetDpiForWindow(self->handle));
    if (settings_surface(self))
    {
        clear_command_status(self);
    }
    else
    {
        if (!hide_command_surface(self))
            return;
    }
    if (folio_state_language(self->state) != before)
    {
        refresh_tooltip_rects(self);
        relanguage_window(self);
        return;
    }
    redraw_command_layer(self);
}

/* 未知の語・語なし・余計な語は、未知のコマンドと同じ 1 行で、入力を消さない（決定 8）。
 * テーマ名は正本catalogで解析し、行番号・言語とともに既存採用経路へ渡す（ADR0049）。 */
static void execute_set_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    enum folio_theme_choice theme = FOLIO_THEME_CHOICE_SYSTEM;
    if (folio_command_parse_theme(argument, strlen(argument), &theme))
    {
        apply_theme(self, theme);
        return;
    }
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
        report_outcome(self, applied);
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
        report_outcome(self, previewed);
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
static void execute_discard_command(struct folio_window *_Nonnull self,
                                    const char *_Nonnull argument)
{
    (void)argument;
    enum folio_state_outcome outcome = folio_state_discard_edits(self->state);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = show_note(self);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    if (!hide_command_surface(self))
        return;
    focus_pane(self);
}

/* 以下は、ほかの呼び出し元を持つ関数をコマンドの表の形（窓と引数）で受ける口。
 * 引数を使わない操作は引数を捨てるだけで、行き先は表へ移す前の switch と同じ。 */
static void execute_save_as_command(struct folio_window *_Nonnull self,
                                    const char *_Nonnull argument)
{
    finish_save_command(self, command_save_as(self, argument));
}

static void execute_force_quit_command(struct folio_window *_Nonnull self,
                                       const char *_Nonnull argument)
{
    (void)argument;
    finish_quit(self, FOLIO_STATE_READY);
}

static void execute_help_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    show_command_palette(self);
}

static void execute_edit_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    execute_mode_command(self, PANE_MODE_EDIT);
}

static void execute_view_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    execute_mode_command(self, PANE_MODE_VIEW);
}

static void execute_rename_command(struct folio_window *_Nonnull self,
                                   const char *_Nonnull argument)
{
    HWND focus = GetFocus();
    enum folio_state_outcome outcome = command_rename(self, argument);
    finish_rename_command(self, outcome, focus);
}

/* カテゴリを作る（ADR 0039 の決定 10・補正 2）。名前が無ければ名前入力面を開く。
 * 面は READY と CATEGORY_LEDGER_STALE でだけ閉じて返り、ほかの失敗は面の中の 1 行で出る。 */
static enum folio_state_outcome create_category(struct folio_window *_Nonnull self,
                                                const char *_Nonnull argument)
{
    if (argument[0] == '\0')
    {
        struct name_prompt_request request = {
            .state = self->state, .kind = NAME_PROMPT_NEW_CATEGORY, .units = u"", .count = 0};
        return show_name_prompt(self, &request).outcome;
    }
    struct category_name *_Nullable name = nullptr;
    switch (category_name_create(argument, strlen(argument), &name))
    {
    case CATEGORY_NAME_ACCEPTED:
        break;
    case CATEGORY_NAME_INVALID:
        return FOLIO_STATE_CATEGORY_NAME_INVALID;
    case CATEGORY_NAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome outcome = folio_state_create_category(self->state, name);
    category_name_destroy(name);
    return outcome;
}

/* 作れたら入力面を閉じ、新しい行（カーソル）を見える位置へ寄せ、フォーカスを索引へ返す。
 * 開いている文書・本文・モード・選択は変えず、保存も走らせない（補正 2 の (c)）。
 * 台帳だけ書けなかったときは、作れた後で失敗の箱を 1 回出す。 */
static void finish_new_category(struct folio_window *_Nonnull self,
                                enum folio_state_outcome outcome)
{
    if (outcome != FOLIO_STATE_READY && outcome != FOLIO_STATE_CATEGORY_LEDGER_STALE)
    {
        report_outcome(self, outcome);
        return;
    }
    if (!hide_command_surface(self))
        return;
    show_drawer_layout(self);
    redraw_drawer(self);
    if (self->drawer != nullptr)
    {
        drawer_window_reveal_cursor(self->drawer);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    self->keyboard_target = ACCESSIBLE_TREE;
    SetFocus(self->handle);
    if (outcome == FOLIO_STATE_CATEGORY_LEDGER_STALE)
    {
        report_outcome(self, outcome);
    }
}

static void execute_new_category_command(struct folio_window *_Nonnull self,
                                         const char *_Nonnull argument)
{
    finish_new_category(self, create_category(self, argument));
}

/* 一時的な入力停止と失敗の箱の後で、元の有効な欄だけへ戻す。 */
static void restore_valid_focus(const struct folio_window *_Nonnull self, HWND _Nullable focus)
{
    if (focus == nullptr || !IsWindow(focus) || !IsWindowEnabled(focus))
    {
        return;
    }
    HWND target = surface_return_target(self, focus);
    if (IsWindowVisible(target))
    {
        HWND root = GetAncestor(target, GA_ROOT);
        if (GetActiveWindow() != root)
            SetActiveWindow(root);
        if (GetFocus() != target)
            SetFocus(target);
    }
}

/* ごみ箱とカテゴリ削除が通る、同期 Shell 呼出しの入力停止。 */
static bool block_removal_input(struct folio_window *_Nonnull self)
{
    bool enabled = IsWindowEnabled(self->handle) != 0;
    self->removal_running = true;
    EnableWindow(self->handle, FALSE);
    return enabled;
}

static void release_removal_input(struct folio_window *_Nonnull self, bool enabled,
                                  HWND _Nullable focus)
{
    EnableWindow(self->handle, enabled);
    if (enabled)
    {
        restore_valid_focus(self, focus);
    }
    self->removal_running = false;
    activate_tooltip(self);
}

/* 同期の Shell 境界で入力を止める。元の面とフォーカスは別文書の操作では保つ。 */
static enum folio_state_outcome run_trash_note(struct folio_window *_Nonnull self,
                                               struct note_ref target)
{
    const char16_t *units = u"";
    size_t count = 0;
    if (holds_document(self, target) && folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
    {
        enum folio_state_outcome outcome = take_text(self, &units, &count);
        if (outcome != FOLIO_STATE_READY)
        {
            return outcome;
        }
    }
    HWND focus = GetFocus();
    bool enabled = block_removal_input(self);
    enum folio_state_outcome outcome = folio_state_trash_note(self->state, target, units, count);
    release_removal_input(self, enabled, focus);
    return outcome;
}

/* 入口に依らず押した対象を直接渡す。別文書の本文を流し込み直す経路は持たない。 */
static void notify_note_removed(struct folio_window *_Nonnull self,
                                const char *_Nonnull removed_name)
{
    struct ui_text_request request = {.id = UI_TEXT_NOTIFY_NOTE_REMOVED,
                                      .language = folio_state_language(self->state),
                                      .name = removed_name};
    char text[draw_unit_limit];
    char16_t units[draw_unit_limit];
    if (ui_text_format(&request, text, sizeof text) == UI_TEXT_FORMAT_READY &&
        wide_units(text, units) > 0)
        accessibility_host_notify(self->accessibility,
                                  (struct accessible_item){.kind = ACCESSIBLE_WINDOW}, units);
}

/* After removal of the held document, retire the face before rendering the empty pane. */
[[nodiscard]] static bool reflect_removed_document(struct folio_window *_Nonnull self)
{
    if (!hide_command_surface(self))
        return false;
    enum folio_state_outcome loaded = render_pane(self);
    arrange(self);
    SetFocus(surface_return_target(self, self->handle));
    if (loaded != FOLIO_STATE_READY)
    {
        report_outcome(self, loaded);
        redraw_drawer(self);
        return false;
    }
    return true;
}

static void trash_note_at(struct folio_window *_Nonnull self, struct note_ref target)
{
    if (self->removal_running)
    {
        return;
    }
    char removed_name[draw_unit_limit];
    (void)strcpy_s(removed_name, sizeof removed_name, folio_state_note_name(self->state, target));
    bool current = holds_document(self, target);
    HWND focus = GetFocus();
    enum folio_state_outcome outcome = run_trash_note(self, target);
    if (outcome != FOLIO_STATE_READY && outcome != FOLIO_STATE_LEDGER_STALE)
    {
        report_outcome(self, outcome);
        return;
    }
    if (current && !reflect_removed_document(self))
        return;
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    if (outcome == FOLIO_STATE_LEDGER_STALE)
    {
        report_outcome(self, outcome);
        if (!current)
        {
            restore_valid_focus(self, focus);
        }
        return;
    }
    notify_note_removed(self, removed_name);
}

static void execute_trash_note_command(struct folio_window *_Nonnull self,
                                       const char *_Nonnull argument)
{
    (void)argument;
    struct note_ref target = {.category = 0, .note = 0};
    if (folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NAMED ||
        !folio_state_selection(self->state, &target))
    {
        report_outcome(self, FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    trash_note_at(self, target);
}

/* カテゴリ削除の完了は一覧とパンくずだけへ写す。本文や残す入力面を設定し直さない。 */
static HWND _Nullable finish_delete_category(struct folio_window *_Nonnull self,
                                             HWND _Nullable focus)
{
    bool close = self->command_surface == COMMAND_SURFACE_EX ||
                 self->command_surface == COMMAND_SURFACE_PALETTE;
    bool closed_focus = close && (command_owns(self, focus) || focus == self->command_layer);
    if (close)
    {
        if (!hide_command_surface(self))
            return focus;
    }
    redraw_drawer(self);
    if (self->drawer != nullptr)
    {
        drawer_window_reveal_cursor(self->drawer);
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    return closed_focus ? self->handle : focus;
}

/* 行メニューと共通コマンドの受け口。保存も文書選択もUIには挟まない（ADR 0039 補正 6）。 */
static void delete_category_at(struct folio_window *_Nonnull self, size_t category)
{
    if (self->removal_running)
    {
        return;
    }
    HWND focus = GetFocus();
    bool enabled = block_removal_input(self);
    enum folio_state_outcome outcome = folio_state_delete_category(self->state, category);
    release_removal_input(self, enabled, focus);
    if (outcome != FOLIO_STATE_READY && outcome != FOLIO_STATE_CATEGORY_LEDGER_STALE)
    {
        report_outcome(self, outcome);
        restore_valid_focus(self, focus);
        return;
    }
    focus = finish_delete_category(self, focus);
    if (outcome == FOLIO_STATE_CATEGORY_LEDGER_STALE)
    {
        report_outcome(self, outcome);
    }
    restore_valid_focus(self, focus);
}

/* 最初のカテゴリへのfallbackはせず、cursorが無い時だけ現在文書の宛先を使う。 */
static bool category_command_target(const struct folio_window *_Nonnull self,
                                    size_t *_Nonnull category)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref cursor = {.category = 0, .note = 0};
    if (folio_state_cursor(self->state, &kind, &cursor))
    {
        *category = cursor.category;
        return true;
    }
    switch (folio_state_document_kind(self->state))
    {
    case FOLIO_DOCUMENT_NAMED:
    case FOLIO_DOCUMENT_UNTITLED:
        *category = folio_state_document_category(self->state);
        return true;
    case FOLIO_DOCUMENT_NONE:
        return false;
    }
    return false;
}

static void execute_delete_category_command(struct folio_window *_Nonnull self,
                                            const char *_Nonnull argument)
{
    (void)argument;
    size_t category = 0;
    if (!category_command_target(self, &category))
    {
        HWND focus = GetFocus();
        report_outcome(self, FOLIO_STATE_CATEGORY_NOT_SELECTED);
        restore_valid_focus(self, focus);
        return;
    }
    delete_category_at(self, category);
}

static enum folio_state_outcome
category_rename_destination(struct folio_window *_Nonnull self, const char *_Nonnull argument,
                            const struct name_prompt_request *_Nonnull request)
{
    if (argument[0] == '\0')
    {
        return show_name_prompt(self, request).outcome;
    }
    struct category_name *_Nullable name = nullptr;
    switch (category_name_create(argument, strlen(argument), &name))
    {
    case CATEGORY_NAME_ACCEPTED:
        break;
    case CATEGORY_NAME_INVALID:
        return FOLIO_STATE_CATEGORY_NAME_INVALID;
    case CATEGORY_NAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct category_rename_target target = {.category = request->target, .name = name};
    enum folio_state_outcome outcome =
        folio_state_rename_category(self->state, &target, request->units, request->count);
    category_name_destroy(name);
    return outcome;
}

static enum folio_state_outcome run_category_rename(struct folio_window *_Nonnull self,
                                                    size_t category, const char *_Nonnull argument)
{
    if (category >= folio_state_category_count(self->state))
    {
        return FOLIO_STATE_NO_SUCH_CATEGORY;
    }
    const char16_t *units = u"";
    size_t count = 0;
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NAMED &&
        folio_state_document_category(self->state) == category &&
        folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
    {
        enum folio_state_outcome taken = take_text(self, &units, &count);
        if (taken != FOLIO_STATE_READY)
        {
            return taken;
        }
    }
    struct name_prompt_request request = {.state = self->state,
                                          .kind = NAME_PROMPT_RENAME_CATEGORY,
                                          .target = category,
                                          .units = units,
                                          .count = count};
    return category_rename_destination(self, argument, &request);
}

/* カテゴリ行は押した添字をそのまま渡す。選択/保存/renderをUIで先に行わない。 */
static void rename_category_at(struct folio_window *_Nonnull self, size_t category,
                               const char *_Nonnull argument)
{
    HWND focus = GetFocus();
    enum folio_state_outcome outcome = prepare_name_operation(self);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = run_category_rename(self, category, argument);
    }
    finish_rename_command(self, outcome, focus);
}

static void execute_rename_category_command(struct folio_window *_Nonnull self,
                                            const char *_Nonnull argument)
{
    HWND focus = GetFocus();
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        finish_rename_command(self, prepared, focus);
        return;
    }
    size_t category = 0;
    if (!category_command_target(self, &category))
    {
        finish_rename_command(self, FOLIO_STATE_CATEGORY_NOT_SELECTED, focus);
        return;
    }
    rename_category_at(self, category, argument);
}

static void execute_find_command(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    begin_search(self);
}

static void execute_toggle_number_command(struct folio_window *_Nonnull self,
                                          const char *_Nonnull argument)
{
    (void)argument;
    apply_number(self, !folio_state_number(self->state));
}

/* 表示コマンドは一時入力面を閉じ、非保存の表示採用だけへ合流する。 */
static void execute_layout_drawer(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (!hide_command_surface(self))
        return;
    apply_layout_mode(self, FOLIO_LAYOUT_DRAWER);
}

static void execute_layout_both(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (!hide_command_surface(self))
        return;
    apply_layout_mode(self, FOLIO_LAYOUT_BOTH);
}

static void execute_layout_note(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (!hide_command_surface(self))
        return;
    apply_layout_mode(self, FOLIO_LAYOUT_NOTE);
}

/* コマンドごとの行き先を 1 か所に置く（ADR 0027 の 2026-10-06 の補正）。全値を明示し、
 * 値を足して行を書き忘れると CNF-009 が落ちる（switch を離れて失う -Wswitch-enum の代わり）。
 * enum の外の値は作られないので添字の範囲検査は書かない。 */
static void (*_Nonnull const command_runs[])(struct folio_window *_Nonnull self,
                                             const char *_Nonnull argument) = {
    [FOLIO_COMMAND_SAVE] = execute_save_command,
    [FOLIO_COMMAND_SAVE_AS] = execute_save_as_command,
    [FOLIO_COMMAND_QUIT] = execute_quit_command,
    [FOLIO_COMMAND_SAVE_QUIT] = execute_save_quit_command,
    [FOLIO_COMMAND_FORCE_QUIT] = execute_force_quit_command,
    [FOLIO_COMMAND_HELP] = execute_help_command,
    [FOLIO_COMMAND_EDIT] = execute_edit_command,
    [FOLIO_COMMAND_VIEW] = execute_view_command,
    [FOLIO_COMMAND_NEW] = execute_new_command,
    [FOLIO_COMMAND_RENAME] = execute_rename_command,
    [FOLIO_COMMAND_FIND] = execute_find_command,
    [FOLIO_COMMAND_SET] = execute_set_command,
    [FOLIO_COMMAND_TOGGLE_NUMBER] = execute_toggle_number_command,
    [FOLIO_COMMAND_REPLACE] = begin_replace,
    [FOLIO_COMMAND_SUBSTITUTE] = execute_substitute_command,
    [FOLIO_COMMAND_SETTINGS] = show_settings_surface,
    [FOLIO_COMMAND_LAYOUT_DRAWER] = execute_layout_drawer,
    [FOLIO_COMMAND_LAYOUT_BOTH] = execute_layout_both,
    [FOLIO_COMMAND_LAYOUT_NOTE] = execute_layout_note,
    [FOLIO_COMMAND_DISCARD_EDITS] = execute_discard_command,
    [FOLIO_COMMAND_HISTORY] = execute_history_command,
    [FOLIO_COMMAND_NEW_CATEGORY] = execute_new_category_command,
    [FOLIO_COMMAND_TRASH_NOTE] = execute_trash_note_command,
    [FOLIO_COMMAND_DELETE_CATEGORY] = execute_delete_category_command,
    [FOLIO_COMMAND_RENAME_CATEGORY] = execute_rename_category_command,
};

/* GUI・キー・Exで同じ操作と引数を実行する（ADR0020）。 */
static void execute_command(struct folio_window *_Nonnull self, enum folio_command command,
                            const char *_Nonnull argument)
{
    if (self->removal_running || session_busy(self) || pane_streaming(self) ||
        self->native_modal_depth > 0 || self->close_phase != CLOSE_PHASE_IDLE)
    {
        return;
    }
    bool preserves_theme = command == FOLIO_COMMAND_SAVE || command == FOLIO_COMMAND_QUIT ||
                           command == FOLIO_COMMAND_SAVE_QUIT ||
                           command == FOLIO_COMMAND_FORCE_QUIT;
    if (!preserves_theme && self->command_surface == COMMAND_SURFACE_THEMES &&
        !preview_theme(self, folio_state_theme_choice(self->state)))
        return;
    if (self->command_surface == COMMAND_SURFACE_MENU)
        close_command_surface(self);
    set_pane_notice(self, PANE_NOTICE_NONE);
    bool previous_context = self->command_context;
    enum folio_command previous_command = self->reporting_command;
    self->command_context = true;
    self->reporting_command = command;
    accessible_command_invoked(self, command);
    command_runs[command](self, argument);
    self->command_context = previous_context;
    self->reporting_command = previous_command;
}

/* Ctrl+S と本文の Esc。保存して編集モードのまま残る。失敗なら 1 行を出して false。 */
static bool store_edit(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = store_body(self);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return false;
    }
    InvalidateRect(self->handle, nullptr, FALSE);
    return true;
}

/* ノートを切り替える前の保存。閲覧中なら何もしない（ADR 0013 の決定 4 の (i)）。 */
static struct name_prompt_result save_edit(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        return (struct name_prompt_result){FOLIO_STATE_READY, FOLIO_UNTITLED_PROTECT};
    }
    return depart_document(self);
}

/* 選択の意図が通ったら、同じモードで開き直して索引も描き直す（決定 4 の (iii) / (iv)）。 */
static enum folio_state_outcome opened(struct folio_window *_Nonnull self,
                                       enum folio_state_outcome selected)
{
    if (selected != FOLIO_STATE_READY)
    {
        return selected;
    }
    enum folio_state_outcome loaded = reflect_note(self);
    redraw_drawer(self);
    return loaded;
}

/* ノートを切り替える唯一の経路（ADR 0013 の決定 4）。保存できなければ選択は動かない。 */
static enum folio_state_outcome switch_note(struct folio_window *_Nonnull self, size_t category,
                                            size_t note)
{
    if (pane_streaming(self))
    {
        return FOLIO_STATE_PANE_LOAD_FAILED;
    }
    if (self->removal_running || session_busy(self))
    {
        return FOLIO_STATE_CANCELLED;
    }
    set_pane_notice(self, PANE_NOTICE_NONE);
    struct name_prompt_result departure = save_edit(self);
    enum folio_state_outcome outcome = departure.outcome;
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    outcome =
        opened(self, folio_state_select_note(self->state, category, note, departure.untitled));
    /* show_note が入口で印を下げるので、その後に立てる（#258）。 */
    if (outcome == FOLIO_STATE_READY &&
        folio_state_select_notice(self->state) == FOLIO_STATE_NOTE_UNEDITABLE)
    {
        set_pane_notice(self, PANE_NOTICE_NOTE_UNEDITABLE);
    }
    return outcome;
}

/* 押した行が、いま開いている名前付きの文書か。同じノートを選び直すと本文を読み直して
 * Undo を失うので、開いている文書は選び直さない（FR-038）。 */
static bool holds_document(const struct folio_window *_Nonnull self, struct note_ref target)
{
    struct note_ref selected = {.category = 0, .note = 0};
    return folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NAMED &&
           folio_state_selection(self->state, &selected) && selected.category == target.category &&
           selected.note == target.note;
}

/* ノート行のメニューの「名前を変更…」。違う行ならクリックと同じ switch_note で選んでから、
 * コマンドの「名前を変更」と同じ受け口へ渡す。選べなければその失敗を出して止まる。 */
static void rename_from_drawer(struct folio_window *_Nonnull self, struct note_ref target)
{
    HWND focus = GetFocus();
    enum folio_state_outcome prepared = prepare_name_operation(self);
    if (prepared != FOLIO_STATE_READY)
    {
        finish_rename_command(self, prepared, focus);
        return;
    }
    if (!holds_document(self, target))
    {
        enum folio_state_outcome outcome = switch_note(self, target.category, target.note);
        if (outcome != FOLIO_STATE_READY)
        {
            report_outcome(self, outcome);
            return;
        }
    }
    execute_rename_command(self, "");
}

/* ドロワーの右クリックのメニューで選ばれた項目（FR-038・ADR 0039 の決定 11）。
 * 行き先は既存のコマンドの受け口で、ここは対象の行を選ぶことだけを足す（ARC-001）。 */
static void run_drawer_menu(struct folio_window *_Nonnull self, enum drawer_menu_item item,
                            const struct note_ref *_Nonnull target)
{
    if (self->removal_running || session_busy(self))
    {
        return;
    }
    set_pane_notice(self, PANE_NOTICE_NONE);
    switch (item)
    {
    case DRAWER_MENU_DELETE_CATEGORY:
        delete_category_at(self, target->category);
        return;
    case DRAWER_MENU_RENAME_CATEGORY:
        rename_category_at(self, target->category, "");
        return;
    case DRAWER_MENU_TRASH_NOTE:
        trash_note_at(self, *target);
        return;
    case DRAWER_MENU_RECOLOR:
        /* 色はドロワーが自分で色の選択を開く（決定 12 の例外）。ここへは来ないので何もしない。 */
        return;
    case DRAWER_MENU_RENAME_NOTE:
        rename_from_drawer(self, *target);
        return;
    case DRAWER_MENU_NEW_CATEGORY:
        execute_new_category_command(self, "");
        return;
    case DRAWER_MENU_NEW_NOTE:
        new_note_at(self, target->category);
        return;
    }
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
        outcome = folio_state_select_adjacent(self->state, step, FOLIO_UNTITLED_PROTECT);
        redraw_drawer(self);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
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
    return reflect_note(self);
}

/* h / l。カーソルの行のカテゴリを折り畳む／展開する（ADR 0015 の決定 3）。
 * カーソルがカテゴリ行のときの展開は中のノートを選び直すことがあるので、先に保存する
 * （ADR 0013 の決定 4 の (i)）。カーソルが移るので、最後は歩みと同じように見える位置へ寄せる。
 * 絞り込み中は開閉できない。`h` は絞り込み無しでは日常の移動なので、打鍵のたびに失敗箱を出さず
 * 静かに戻る（ADR 0024 の補正 1。断りの値そのものは application が持つ）。 */
static enum folio_state_outcome expand_category(struct folio_window *_Nonnull self, size_t category,
                                                bool expanded)
{
    if (folio_state_filtering(self->state))
    {
        return FOLIO_STATE_FILTERED;
    }
    struct name_prompt_result departure = {FOLIO_STATE_READY, FOLIO_UNTITLED_PROTECT};
    if (folio_state_category_replaces_document(self->state, category, expanded))
    {
        departure = save_edit(self);
    }
    enum folio_state_outcome outcome = departure.outcome;
    /* 初回保存で名前が付いても、本文自体は同じRichEdit。保存後から比較する。 */
    struct note_ref before = {.category = 0, .note = 0};
    bool had = folio_state_selection(self->state, &before);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome =
            folio_state_set_category_expanded(self->state, category, expanded, departure.untitled);
    }
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = reopen_if_moved(self, had, before);
    }
    redraw_drawer(self);
    return outcome;
}

static void expand_cursor(struct folio_window *_Nonnull self, bool expanded)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {.category = 0, .note = 0};
    if (!folio_state_cursor(self->state, &kind, &cursor) || folio_state_filtering(self->state))
    {
        return;
    }
    enum folio_state_outcome outcome = expand_category(self, cursor.category, expanded);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
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
    invalidate_surface_items(self);
    self->command_keys_visible = !self->command_keys_visible;
    self->command_help_scroll = 0;
    arrange_command_input(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

static bool click_command_footer(struct folio_window *_Nonnull self, POINT point)
{
    for (size_t index = 0; index < 2; ++index)
    {
        RECT tab = command_tab_rect(self, index);
        if (PtInRect(&tab, point))
        {
            if (self->command_keys_visible != (index == 1))
                toggle_command_keys(self);
            return true;
        }
    }
    if (!command_keys(self))
        return false;
    RECT previous = command_previous_rect(self);
    RECT next = command_next_rect(self);
    if (!PtInRect(&previous, point) && !PtInRect(&next, point))
        return false;
    move_command_help(self, PtInRect(&previous, point) ? VK_PRIOR : VK_NEXT);
    return true;
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
        show_theme_surface(self);
        return;
    case SETTINGS_ROW_LANGUAGE:
        apply_language(self, (enum folio_language)settings_rows[index].value);
        return;
    case SETTINGS_ROW_NUMBER:
        apply_number(self, !folio_state_number(self->state));
        return;
    }
}

/* 描画と同じ矩形だけを採用する。header/footerは値の選択に参加しない。 */
static void click_settings_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT close = settings_close_rect(self);
    if (PtInRect(&close, point))
    {
        close_command_surface(self);
        return;
    }
    RECT content = settings_content_rect(self);
    if (!PtInRect(&content, point))
    {
        return;
    }
    for (size_t index = 0; index < settings_row_count(); ++index)
    {
        RECT item = settings_item_rect(self, index);
        if (settings_row_choice(index) && PtInRect(&item, point))
        {
            self->command_selection = index;
            adopt_settings_row(self);
            return;
        }
    }
}

/* 版の本文を編集中の本文へ流し込む（ADR 0038 の決定 1 の 2）。閲覧中だったなら、先に保存済みの
 * 本文の編集 RTF で RichEdit を編集にしてから全文を置き換えるので、Ctrl+Z 1 回で保存済みの本文へ
 * 戻る。版の本文は従来どおり UTF-16 の置き換えで入れる（ADR 0006 の #245 補正）。 */
static enum folio_state_outcome pour_history(struct folio_window *_Nonnull self,
                                             const struct utf16_text *_Nonnull restored,
                                             bool needs_base)
{
    if (needs_base)
    {
        enum folio_state_outcome loaded = pour_edit_rtf(self);
        if (loaded != FOLIO_STATE_READY)
        {
            return loaded;
        }
    }
    const char16_t *_Nonnull text = u"";
    size_t length = 0;
    enum folio_state_outcome outcome = take_display_text(self, &text, &length);
    if (outcome != FOLIO_STATE_READY)
    {
        return outcome;
    }
    note_pane_replace(self->pane, (struct note_search_span){.start = 0, .end = length},
                      utf16_text_units(restored));
    note_pane_select(self->pane, 0, 0);
    note_pane_scroll_to_line(self->pane, 1);
    return FOLIO_STATE_READY;
}

/* 採用の前に作る写し（版の本文の UTF-16 と、閲覧中なら保存済みの本文の編集 RTF）。確保に
 * 失敗しても application は本文もモードも変えていない（編集 RTF の slot は前のまま保たれる）。 */
static bool history_units(struct folio_window *_Nonnull self, const struct note_text *_Nonnull body,
                          struct utf16_text *_Nullable *_Nonnull restored,
                          bool *_Nonnull needs_base)
{
    if (utf16_text_create(note_text_bytes(body), note_text_length(body), restored) !=
        UTF16_TEXT_CONVERTED)
    {
        return false;
    }
    *needs_base = folio_state_pane_mode(self->state) == PANE_MODE_VIEW ||
                  note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY;
    return !*needs_base || folio_state_pane_edit_rtf(self->state) == FOLIO_STATE_READY;
}

/* カーソルの版を採用する（決定 9・14）。写しは restore より先に作る。restore が閲覧から EDIT へ
 * 移すので enter_edit は呼ばない（enter_edit は EDIT なら何もせず、先に呼ぶと読めない版でも
 * 編集へ入ってしまう）。RichEdit 側の切り替えは pour_history が note_pane_load で行う。 */
static void adopt_history_row(struct folio_window *_Nonnull self)
{
    if (pane_streaming(self))
    {
        return;
    }
    size_t index = self->command_selection;
    const struct note_text *_Nullable body = folio_state_history_body(self->state, index);
    if (body == nullptr || self->pane == nullptr)
    {
        /* 読めない版・範囲外は application が HISTORY_UNREADABLE で断り、何も変えない。 */
        report_outcome(self, self->pane == nullptr
                                 ? FOLIO_STATE_PANE_UNAVAILABLE
                                 : folio_state_restore_history(self->state, index));
        return;
    }
    struct utf16_text *_Nullable restored = nullptr;
    bool needs_base = false;
    enum folio_state_outcome outcome = history_units(self, body, &restored, &needs_base)
                                           ? folio_state_restore_history(self->state, index)
                                           : FOLIO_STATE_OUT_OF_MEMORY;
    if (outcome == FOLIO_STATE_READY && restored != nullptr)
    {
        outcome = pour_history(self, restored, needs_base);
    }
    utf16_text_destroy(restored);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    /* 流し込んでから一覧を捨てる（hide_command_surface が close を呼ぶ）。フォーカスは本文へ。 */
    if (!hide_command_surface(self))
        return;
    InvalidateRect(self->handle, nullptr, FALSE);
    focus_pane(self);
}

/* クリックはパレットと同じ行の当たり。行を押すとその場で採用する。 */
static void click_history_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT close = settings_close_rect(self);
    if (PtInRect(&close, point))
    {
        close_command_surface(self);
        return;
    }
    RECT rows = command_rows_rect(self);
    if (!PtInRect(&rows, point))
    {
        return;
    }
    int height = scale(base_command_row_height, GetDpiForWindow(self->handle));
    size_t index = self->command_first + (size_t)((point.y - rows.top) / height);
    if (index >= folio_state_history_count(self->state))
    {
        return;
    }
    self->command_selection = index;
    adopt_history_row(self);
}

static void preview_theme_selection(struct folio_window *_Nonnull self)
{
    if (self->command_composing)
        return;
    if (prepare_theme_candidates(self) != S_OK)
    {
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_theme_choice choice = folio_state_theme_choice(self->state);
    bool found = theme_candidate_at(self, self->command_selection, &choice);
    if (!found)
        choice = folio_state_theme_choice(self->state);
    if (!preview_theme(self, choice))
        return;
    redraw_command_layer(self);
}

static void adopt_theme_at(struct folio_window *_Nonnull self, size_t index)
{
    if (self->command_composing)
    {
        return;
    }
    if (prepare_theme_candidates(self) != S_OK)
    {
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_theme_choice choice = FOLIO_THEME_CHOICE_SYSTEM;
    bool found = theme_candidate_at(self, index, &choice);
    if (found)
    {
        self->command_selection = index;
        apply_theme(self, choice);
    }
}

static void click_theme_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT back = theme_back_rect(self);
    RECT close = settings_close_rect(self);
    if (PtInRect(&back, point))
    {
        show_settings_surface(self, "");
        return;
    }
    if (PtInRect(&close, point))
    {
        close_command_surface(self);
        return;
    }
    RECT rows = theme_rows_rect(self);
    if (PtInRect(&rows, point) && !self->command_composing)
    {
        int height = scale(base_theme_row_height, GetDpiForWindow(self->handle));
        size_t index = self->command_first + (size_t)((point.y - rows.top) / height);
        adopt_theme_at(self, index);
    }
    focus_command_input(self);
}

static void move_theme_selection(struct folio_window *_Nonnull self, WPARAM key, size_t count,
                                 size_t step)
{
    if (key == VK_UP || key == VK_PRIOR)
    {
        self->command_selection =
            self->command_selection < step ? 0 : self->command_selection - step;
    }
    else
    {
        size_t next = self->command_selection + step;
        self->command_selection = next < count ? next : count - 1;
    }
}

static void navigate_theme_selection(struct folio_window *_Nonnull self, WPARAM key, size_t step)
{
    if (prepare_theme_candidates(self) != S_OK)
    {
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    size_t count = theme_matches_count(self);
    if (count == 0)
    {
        return;
    }
    move_theme_selection(self, key, count, step);
    reveal_command_selection(self);
    preview_theme_selection(self);
}

static bool theme_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_UP && key != VK_DOWN && key != VK_PRIOR && key != VK_NEXT)
        return false;
    size_t step = key == VK_PRIOR || key == VK_NEXT ? command_visible_rows(self) : 1;
    navigate_theme_selection(self, key, step > 0 ? step : 1);
    return true;
}

static void theme_query_changed(struct folio_window *_Nonnull self)
{
    if (self->command_composing)
    {
        return;
    }
    self->command_selection = 0;
    self->command_first = 0;
    self->theme_wheel_remainder = 0;
    clear_command_status(self);
    arrange_command_input(self);
    preview_theme_selection(self);
}

static bool palette_point_command(const struct folio_window *_Nonnull self, POINT point,
                                  enum folio_command *_Nonnull out)
{
    for (size_t index = self->command_first;; ++index)
    {
        RECT row = palette_candidate_rect(self, self->command_first, index);
        if (IsRectEmpty(&row))
            return false;
        if (PtInRect(&row, point))
            return palette_command_at(self, index, out);
    }
}

static void click_palette_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT close = settings_close_rect(self);
    if (PtInRect(&close, point))
    {
        close_command_surface(self);
        return;
    }
    if (click_command_footer(self, point) || command_keys(self))
        return;
    if (prepare_palette_candidates(self) != S_OK)
    {
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_command command = FOLIO_COMMAND_SAVE;
    bool found = palette_point_command(self, point, &command);
    if (found)
        execute_command(self, command, "");
}

static void click_command_surface(struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        click_theme_surface(self, point);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        click_settings_surface(self, point);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        click_history_surface(self, point);
        return;
    }
    if (self->command_surface != COMMAND_SURFACE_PALETTE)
    {
        /* 置換の欄は 2 つあるので、既に自分の欄にいるならフォーカスを奪わない。 */
        if (!command_owns(self, GetFocus()))
        {
            focus_command_input(self);
        }
        return;
    }
    click_palette_surface(self, point);
}

static void show_part_menu(struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        close_command_surface(self);
        return;
    }
    struct menu_request request = {
        .context = kind == ACCESSIBLE_LAYOUT_CHOOSER ? FOLIO_MENU_LAYOUT : FOLIO_MENU_OPERATIONS,
        .anchor = accessible_main_rect(self, kind),
        .keyboard = self->focus_cues};
    self->keyboard_target = kind;
    show_menu_surface(self, &request);
}

/* 点の下の自前の操作部品。矩形は描画・UIA・focus と同じ accessible_main_rect から取る。 */
static enum accessible_kind part_at(const struct folio_window *_Nonnull self, POINT point)
{
    for (size_t index = 0; index < main_part_count(); ++index)
    {
        enum accessible_kind kind = main_parts[index].kind;
        RECT bounds = accessible_main_rect(self, kind);
        if (accessible_visible(self, kind) && PtInRect(&bounds, point))
        {
            return kind;
        }
    }
    return ACCESSIBLE_WINDOW;
}

/* 部品の矩形と、単独部品の外側の focus のぶんを描き直す。 */
static void invalidate_part(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (!main_part(kind))
    {
        return;
    }
    RECT bounds = accessible_main_rect(self, kind);
    if (IsRectEmpty(&bounds))
    {
        return;
    }
    int reach = ui_part_focus_reach(GetDpiForWindow(self->handle));
    InflateRect(&bounds, reach, reach);
    InvalidateRect(self->handle, &bounds, FALSE);
}

/* キーボードの印の有無。Tab・F6・矢印・Enter・Space で立て、押下で下ろす。 */
static void show_focus_cues(struct folio_window *_Nonnull self, bool shown)
{
    if (self->focus_cues == shown)
    {
        return;
    }
    self->focus_cues = shown;
    InvalidateRect(self->handle, nullptr, FALSE);
    redraw_drawer(self);
}

/* マウス・キーボード・UIA の 3 つが同じ実行の経路を通る（ADR0060 の決定 4）。
 * 選択群の項目は Select（選択中なら何もしない）、他は
 * Invoke。復旧待ちの本文の札は選択中でも実行する。 */
static enum accessible_action part_action(const struct folio_window *_Nonnull self,
                                          enum accessible_kind kind)
{
    bool chip = kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT;
    bool recovering = chip && (self->pane == nullptr ||
                               note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY);
    bool choice = chip || accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP;
    return choice && !recovering ? ACCESSIBLE_ACTION_SELECT : ACCESSIBLE_ACTION_INVOKE;
}

static void activate_part(struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    set_hover(self, ACCESSIBLE_WINDOW); /* 面・モーダル・メニューの後に枠を残さない */
    struct accessible_request request = {.item = {.kind = kind}, .action = part_action(self, kind)};
    (void)folio_window_accessible_action(self, &request);
    accessibility_host_schedule(self->accessibility);
}

/* 押下を取り消す唯一の経路。マウスの押下なら capture も返す（値を下ろしてから返すので、
 * 再入する WM_CAPTURECHANGED は何もしない）。Space・Tab・F6・矢印・Enter・focus 喪失から呼ばれる。
 */
static void cancel_part_press(struct folio_window *_Nonnull self)
{
    enum accessible_kind kind = self->pressed_part;
    if (kind == ACCESSIBLE_WINDOW)
    {
        return;
    }
    bool mouse = !self->pressed_by_key;
    self->pressed_part = ACCESSIBLE_WINDOW;
    self->pressed_inside = false;
    self->pressed_by_key = false;
    invalidate_part(self, kind);
    if (mouse && GetCapture() == self->handle)
    {
        ReleaseCapture();
    }
}

static void begin_part_press(struct folio_window *_Nonnull self, enum accessible_kind kind,
                             bool by_key)
{
    cancel_part_press(self);
    self->pressed_part = kind;
    self->pressed_inside = true;
    self->pressed_by_key = by_key;
    invalidate_part(self, kind);
}

/* 押下。操作▾だけは OS の作法どおり押下で開き、ほかは capture して離すのを待つ。
 * 選択中の項目は押しても何も起きないので押下の見た目にもしない。 */
static bool dismiss_menu_press(struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_surface != COMMAND_SURFACE_MENU)
        return false;
    bool opener =
        PtInRect(&self->menu.anchor, point) != 0 &&
        (self->menu.context == FOLIO_MENU_OPERATIONS || self->menu.context == FOLIO_MENU_LAYOUT);
    close_command_surface(self);
    return opener;
}

static void press_part(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (dismiss_menu_press(self, point))
        return;
    show_focus_cues(self, false);
    if (self->native_modal_depth == 0 && self->command_surface == COMMAND_SURFACE_CLOSED)
        pointer_focus(self, point);
    enum accessible_kind kind = part_at(self, point);
    if (kind == ACCESSIBLE_WINDOW || !part_enabled(self, kind))
    {
        return;
    }
    if (accessible_item_menu(kind))
    {
        activate_part(self, kind);
        return;
    }
    if (part_action(self, kind) == ACCESSIBLE_ACTION_SELECT && part_selected(self, kind))
    {
        return;
    }
    begin_part_press(self, kind, false);
    SetCapture(self->handle);
}

/* 矩形の内で離したときだけ実行し、外で離せば取り消す（WM_CAPTURECHANGED からの取消は無害）。 */
static void release_part(struct folio_window *_Nonnull self, LPARAM lparam)
{
    enum accessible_kind kind = self->pressed_part;
    if (kind == ACCESSIBLE_WINDOW || self->pressed_by_key)
    {
        return;
    }
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    RECT bounds = accessible_main_rect(self, kind);
    bool inside = PtInRect(&bounds, point) && accessible_visible(self, kind);
    cancel_part_press(self);
    if (inside)
    {
        activate_part(self, kind);
    }
}

static void set_hover(struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (kind != ACCESSIBLE_WINDOW && !part_enabled(self, kind))
        kind = ACCESSIBLE_WINDOW;
    if (self->hover_part == kind)
    {
        return;
    }
    invalidate_part(self, self->hover_part);
    self->hover_part = kind;
    invalidate_part(self, kind);
}

/* capture 中は押下の内外だけを写す（外へ出たら押していない見た目に戻す）。それ以外は hover。 */
static void track_pointer(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (self->pressed_part != ACCESSIBLE_WINDOW && !self->pressed_by_key)
    {
        RECT bounds = accessible_main_rect(self, self->pressed_part);
        bool inside = PtInRect(&bounds, point) != 0;
        if (inside != self->pressed_inside)
        {
            self->pressed_inside = inside;
            invalidate_part(self, self->pressed_part);
        }
        return;
    }
    if (!self->hover_tracking)
    {
        TRACKMOUSEEVENT track = {
            .cbSize = sizeof track, .dwFlags = TME_LEAVE, .hwndTrack = self->handle};
        self->hover_tracking = TrackMouseEvent(&track) != 0;
    }
    bool open =
        folio_window_accessible_ready(self) && self->command_surface == COMMAND_SURFACE_CLOSED;
    set_hover(self, open ? part_at(self, point) : ACCESSIBLE_WINDOW);
}

static void leave_pointer(struct folio_window *_Nonnull self)
{
    self->hover_tracking = false;
    set_hover(self, ACCESSIBLE_WINDOW);
}

/* Space は押下の見た目にして離した時に実行する。自動繰り返しは呼び出し側で捨てる。 */
static void release_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_SPACE || !self->pressed_by_key)
    {
        return;
    }
    bool same = self->pressed_part == self->keyboard_target;
    cancel_part_press(self);
    if (same && self->command_surface == COMMAND_SURFACE_CLOSED)
    {
        keyboard_activate(self);
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
    show_drawer_layout(self);
    self->keyboard_target = ACCESSIBLE_TREE;
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
    if (character == shortcut_character_for(FOLIO_COMMAND_NEW))
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return 1;
    }
    if (character == shortcut_character_for(FOLIO_COMMAND_SAVE))
    {
        execute_command(self, FOLIO_COMMAND_SAVE, "");
        return 1;
    }
    if (character == shortcut_character_for(FOLIO_COMMAND_HELP))
    {
        open_command_surface(self, COMMAND_SURFACE_PALETTE);
        return 1;
    }
    return view_character(self, character) ? 1 : 0;
}

static LRESULT pane_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (shortcut_keydown(self, key))
    {
        return 1;
    }
    if (key == VK_ESCAPE)
    {
        leave_pane(self);
        return 1;
    }
    return 0;
}

/* 字を出している部品か。絵だけの C 様式と、狭幅で絵へ縮めた A 様式は偽。 */
static bool part_labeled(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_SETTINGS || accessible_item_shell(kind) || kind == ACCESSIBLE_HELP ||
        kind == ACCESSIBLE_LAYOUT_CHOOSER)
    {
        return false;
    }
    if (kind == ACCESSIBLE_NEW_NOTE)
    {
        return new_note_labeled(self);
    }
    RECT rects[action_count];
    bool labeled[action_count];
    action_layout(self, rects, labeled);
    for (size_t index = 0; index < action_count; ++index)
    {
        if (action_parts[index].kind == kind)
        {
            return labeled[index];
        }
    }
    return true;
}

static void help_tooltip(enum folio_language language, char *_Nonnull out)
{
    const char *key = part_shortcut(ACCESSIBLE_HELP, language);

    const char *palette = "";
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_route(binding) == FOLIO_SHORTCUT_CHARACTER &&
            folio_shortcut_command(binding) == FOLIO_COMMAND_HELP)
            palette = folio_shortcut_label(binding, language);
    }
    struct ui_text_request help = {
        .id = UI_TEXT_TOOLTIP_HELP_KEYS, .language = language, .from = key, .to = palette};
    if (ui_text_format(&help, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void part_tooltip(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                         char *_Nonnull out)
{
    if (kind == ACCESSIBLE_LAYOUT_CHOOSER)
    {
        layout_chooser_help(self, out);
        return;
    }
    if (kind == ACCESSIBLE_NEW_NOTE)
    {
        /* 表示のたびに作成先を組む（ADR0060 の決定 12）。字の有無に依らず同じ 1 行。 */
        new_note_destination(self, out);
        return;
    }
    enum folio_language language = folio_state_language(self->state);
    const char *key = part_shortcut(kind, language);
    bool labeled = part_labeled(self, kind);
    if (kind == ACCESSIBLE_HELP)
    {
        help_tooltip(language, out);
        return;
    }
    struct ui_text_request request = {
        .id = labeled ? UI_TEXT_TOOLTIP_KEY : UI_TEXT_TOOLTIP_NAMED_KEY,
        .language = language,
        .name = ui_text_line(part_label(self, kind), language),
        .from = key,
        .to = labeled ? ui_text_line(accessible_controls[kind].hint, language) : ""};
    if (key[0] == '\0')
    {
        request.id = UI_TEXT_TOOLTIP_KEY;
        request.from = labeled ? "" : request.name;
    }
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
    {
        out[0] = '\0';
    }
}

static LRESULT tooltip_display(struct folio_window *_Nonnull self, LPARAM lparam)
{
    NMTTDISPINFOW *_Nonnull notice = (NMTTDISPINFOW *)lparam;
    self->tooltip_text[0] = u'\0';
    if (notice->hdr.idFrom >= surface_tooltip_base &&
        notice->hdr.idFrom - surface_tooltip_base < surface_button_count)
    {
        size_t slot = notice->hdr.idFrom - surface_tooltip_base;
        (void)wide_units(
            ui_text_line(surface_button_label(self, slot), folio_state_language(self->state)),
            self->tooltip_text);
    }
    if (notice->hdr.idFrom < sizeof accessible_controls / sizeof accessible_controls[0] &&
        main_part((enum accessible_kind)notice->hdr.idFrom))
    {
        char text[draw_unit_limit];
        part_tooltip(self, (enum accessible_kind)notice->hdr.idFrom, text);
        (void)wide_units(text, self->tooltip_text);
    }
    notice->lpszText = (wchar_t *)self->tooltip_text;
    return 0;
}

/* RichEdit の鍵の通知。IME変換中は親の操作に変換しない。 */
static LRESULT on_notify(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const NMHDR *_Nonnull header = (const NMHDR *)lparam;
    if ((session_busy(self) || pane_streaming(self)) && header->code == EN_MSGFILTER)
    {
        return 1;
    }
    if (header->hwndFrom == self->tooltip && header->code == TTN_GETDISPINFOW)
    {
        return tooltip_display(self, lparam);
    }
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

static bool forward_body_wheel(const struct folio_window *_Nonnull self, WPARAM keys,
                               LPARAM position)
{
    if (self->pane == nullptr || !pane_visible(self) ||
        note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY)
        return false;
    HWND pane = note_pane_handle(self->pane);
    if (!IsWindowVisible(pane) || !IsWindowEnabled(pane))
        return false;
    RECT bounds;
    GetWindowRect(pane, &bounds);
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (!PtInRect(&bounds, point))
        return false;
    SendMessageW(pane, WM_MOUSEWHEEL, keys, position);
    return true;
}

/* 本文上のwheelは本文へ、ドロワー上のwheelは従来の索引へ渡す。
 * lparam はスクリーン座標なので、位置を読むために OS へ問い合わせない（ARC-007）。 */
static void forward_wheel(const struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (forward_body_wheel(self, wparam, lparam))
        return;
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    if (drawer == nullptr || !drawer_visible(self))
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
/* Enter は押下で、Space は離した時に実行する。押し続けた自動繰り返しで新規や保存が連続しないように
 * 繰り返しの打鍵は捨てる（ADR0060 の決定 4）。索引の行では Space を使わない。 */
static bool press_activation(struct folio_window *_Nonnull self, WPARAM key, LPARAM lparam)
{
    if (key != VK_RETURN && (key != VK_SPACE || self->keyboard_target == ACCESSIBLE_TREE))
    {
        return false;
    }
    if ((HIWORD(lparam) & KF_REPEAT) != 0)
    {
        return true;
    }
    show_focus_cues(self, true);
    if (key == VK_SPACE)
    {
        begin_part_press(self, self->keyboard_target, true);
        return true;
    }
    cancel_part_press(self); /* マウスや Space で押している操作を二重に実行しない */
    keyboard_activate(self);
    return true;
}

/* 索引の鍵（ADR 0013 の決定 2）。Enter で本文へ（カーソルがカテゴリ行なら何もしない・
 * ADR 0015 の決定 4）、↑↓ / PgUp / PgDn はドロワーのスクロールへ渡す（ADR 0009 の決定 5）。
 * Escape はここでは何もしない（窓を閉じるのは Alt+F4 と閉じる操作だけ）。 */
static void press_key(struct folio_window *_Nonnull self, WPARAM key, LPARAM lparam)
{
    if (shortcut_keydown(self, key))
        return;
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
    if (press_activation(self, key, lparam) || choice_step(self, key))
    {
        show_focus_cues(self, true);
        return;
    }
    if (self->drawer != nullptr && self->keyboard_target == ACCESSIBLE_TREE)
    {
        drawer_window_scroll_key(self->drawer, key);
    }
}

/* 索引の文字の鍵（ADR 0013 の決定 2）。扱わない文字では何も起きない。
 * Ctrl+S は両方の区画で効くように、ここでも本文と同じ保存を行う。
 * IME が ON のときは未確定文字になってここへ届かない（ADR 0013 の「失う・残る」）。 */
static void type_key(struct folio_window *_Nonnull self, WPARAM character)
{
    if (character == shortcut_character_for(FOLIO_COMMAND_SAVE) ||
        character == shortcut_character_for(FOLIO_COMMAND_NEW) ||
        character == shortcut_character_for(FOLIO_COMMAND_HELP))
    {
        (void)pane_character(self, character);
        return;
    }

    if (self->command_surface != COMMAND_SURFACE_CLOSED || self->keyboard_target != ACCESSIBLE_TREE)
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

static void execute_palette_selection(struct folio_window *_Nonnull self)
{
    if (prepare_palette_candidates(self) != S_OK)
    {
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_command command = FOLIO_COMMAND_SAVE;
    if (!command_candidates_at(self->command_candidates, self->command_selection, &command))
    {
        command_not_found(self);
        return;
    }
    execute_command(self, command, "");
}

static void execute_command_input(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        execute_palette_selection(self);
        return;
    }
    /* 検索欄の Enter は欄を閉じず、覚えている向きで次の一致へ進む（ADR 0023 の決定 4）。 */
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        repeat_search(self, false);
        return;
    }
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
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
    case COMMAND_SURFACE_PALETTE:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
    /* 設定画面の Enter は command_return が行を採用するので、ここには来ない（ADR 0031 の決定 7）。
     */
    case COMMAND_SURFACE_SETTINGS:
    case COMMAND_SURFACE_THEMES:
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
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
    if (prepare_palette_candidates(self) != S_OK)
    {
        show_failure_box(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    size_t count = palette_matches_count(self);
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
    size_t total = (size_t)command_help_total(self);
    if (visible == 0 || visible >= total)
        return;
    size_t first = command_help_offset(self);
    size_t last = total - visible;
    size_t step = command_help_step(self);
    if (key == VK_PRIOR)
        self->command_help_scroll = first < step ? 0 : first - step;
    else
        self->command_help_scroll = first + step > last ? last : first + step;
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

/* Tabは設定では選択肢の巡回、パレットでは説明、置換では2つの欄の往復。 */
static bool command_tab(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        toggle_command_keys(self);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return settings_navigate(self, VK_TAB);
    }
    if (command_surface_listing(self->command_surface))
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
    if (command_keys(self))
        return true;
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        menu_execute(self, self->command_selection);
        return true;
    }

    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        adopt_theme_at(self, self->command_selection);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_REPLACE)
    {
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        adopt_settings_row(self);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        adopt_history_row(self);
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

/* 横配置では列を保って次のグループへ。1列では表の順で辿る。 */
static size_t settings_vertical_choice(const struct folio_window *_Nonnull self, bool downwards)
{
    size_t from = self->command_selection;
    if (settings_columns(self) == 1)
    {
        return settings_next_choice(from, downwards);
    }
    int group = settings_rows[from].group + (downwards ? 1 : -1);
    for (size_t index = 0; index < settings_row_count(); ++index)
    {
        if (!settings_row_choice(index) || settings_rows[index].group != group)
        {
            continue;
        }
        if (settings_rows[index].position == settings_rows[from].position ||
            settings_rows[index].kind == SETTINGS_ROW_NUMBER ||
            settings_rows[index].kind == SETTINGS_ROW_THEME)
        {
            return index;
        }
    }
    return from;
}

static bool settings_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    size_t from = self->command_selection;
    size_t next = from;
    switch (key)
    {
    case VK_TAB:
        next = settings_next_choice(from, true);
        next = next == from ? settings_first_choice_row : next;
        break;
    case VK_LEFT:
    case VK_RIGHT:
        next = settings_next_choice(from, key == VK_RIGHT);
        next = settings_rows[next].group == settings_rows[from].group ? next : from;
        break;
    case VK_UP:
    case VK_DOWN:
        next = settings_vertical_choice(self, key == VK_DOWN);
        break;
    default:
        return false; /* Win32の仮想キーは開いた集合。 */
    }
    self->command_selection = next;
    reveal_command_selection(self);
    redraw_command_layer(self);
    return true;
}

/* 履歴の面の ↑↓。巡回はせず、端では動かない（設定画面と同じ）。 */
static bool history_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_UP && key != VK_DOWN)
    {
        return false;
    }
    size_t count = folio_state_history_count(self->state);
    if (key == VK_UP && self->command_selection > 0)
    {
        self->command_selection -= 1;
    }
    if (key == VK_DOWN && self->command_selection + 1 < count)
    {
        self->command_selection += 1;
    }
    reveal_command_selection(self);
    redraw_command_layer(self);
    return true;
}

/* パレットの移動の鍵。↑↓ は操作の選択、PgUp / PgDn はヘルプのページ（#69）。 */
static bool command_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        return theme_navigate(self, key);
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return settings_navigate(self, key);
    }
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        return history_navigate(self, key);
    }
    if (self->command_surface != COMMAND_SURFACE_PALETTE)
    {
        return false;
    }
    if (key == VK_UP || key == VK_DOWN)
    {
        if (!command_keys(self))
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
    show_focus_cues(self, true);
    if (shortcut_keydown(self, key))
    {
        return true;
    }
    if (key == VK_TAB)
    {
        return command_tab(self);
    }
    if (key == VK_ESCAPE)
    {
        if (self->command_surface == COMMAND_SURFACE_THEMES)
        {
            show_settings_surface(self, "");
            return true;
        }
        close_command_surface(self);
        return true;
    }
    if (key == VK_RETURN || (key == VK_SPACE && self->command_surface == COMMAND_SURFACE_SETTINGS))
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
    if (character == shortcut_character_for(FOLIO_COMMAND_SAVE))
    {
        /* 保存して欄は開いたまま（ADR 0016 の補正 2）。鍵の状態は読まない（ARC-007）。 */
        store_from_surface(self);
        return true;
    }
    if (character == shortcut_character_for(FOLIO_COMMAND_NEW))
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return true;
    }
    if (character == shortcut_character_for(FOLIO_COMMAND_HELP))
    {
        show_command_palette(self);
        return true;
    }
    return character == '\r' || character == '\t' ||
           (character == ' ' && self->command_surface == COMMAND_SURFACE_SETTINGS);
}

static void scroll_settings_content(struct folio_window *_Nonnull self, int delta)
{
    int offset =
        (int)self->command_first +
        scale(base_settings_item_height, GetDpiForWindow(self->handle)) * (delta > 0 ? -1 : 1);
    size_t limit = settings_scroll_limit(self);
    self->command_first = offset > 0 ? (size_t)offset : 0;
    if (self->command_first > limit)
    {
        self->command_first = limit;
    }
    redraw_command_layer(self);
}

static void command_help_wheel(struct folio_window *_Nonnull self, WPARAM key)
{
    move_command_help(self, key == VK_UP ? VK_PRIOR : VK_NEXT);
}

static int theme_wheel_steps(struct folio_window *_Nonnull self, int delta)
{
    int remainder = self->theme_wheel_remainder;
    bool reversed = (remainder > 0 && delta < 0) || (remainder < 0 && delta > 0);
    /* WM_MOUSEWHEELのsigned 16bit deltaと余り119はintに収まる。 */
    int scaled = (reversed ? 0 : remainder) + delta;
    self->theme_wheel_remainder = scaled % WHEEL_DELTA;
    return -(scaled / WHEEL_DELTA);
}

static void wheel_theme_selection(struct folio_window *_Nonnull self, int delta)
{
    int steps = theme_wheel_steps(self, delta);
    if (steps == 0)
        return;
    navigate_theme_selection(self, steps < 0 ? VK_UP : VK_DOWN,
                             (size_t)(steps < 0 ? -steps : steps));
}

static bool command_wheel(struct folio_window *_Nonnull self, WPARAM wparam)
{
    bool listed = self->command_surface == COMMAND_SURFACE_PALETTE || settings_surface(self) ||
                  command_surface_listing(self->command_surface);
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
    if (command_keys(self))
    {
        command_help_wheel(self, key);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        wheel_theme_selection(self, delta);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        scroll_settings_content(self, delta);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        return history_navigate(self, key);
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
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        invalidate_surface_items(self);
        theme_query_changed(self);
    }
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        search_input_changed(self);
    }
    if (self->command_surface == COMMAND_SURFACE_REPLACE)
    {
        update_replace_preview(self);
    }
}

/* 欄の枠は focus の出入りで太さが変わる（ADR0060 の決定 3）。外へ出たら主窓が閉じるか判断する。 */
static void command_focus_changed(struct folio_window *_Nonnull self, UINT message)
{
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
    {
        redraw_command_layer(self);
    }
    if (message == WM_KILLFOCUS && !self->removal_running)
    {
        PostMessageW(self->handle, folio_message_command_focus_lost, 0, 0);
    }
}

static LRESULT CALLBACK command_input_message(HWND window, UINT message, WPARAM wparam,
                                              LPARAM lparam)
{
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr || self->command_input_original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    if (session_blocks_message(self, message) || load_blocks_input(self, message))
    {
        return 0;
    }
    if (self->command_surface == COMMAND_SURFACE_MENU &&
        menu_message(self, message, wparam, lparam))
        return 0;
    accessibility_host_changed(self->accessibility, message);
    update_command_composition(self, message);
    if (command_input_handled(self, message, wparam))
    {
        return 0;
    }
    command_focus_changed(self, message);
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
    clear_command_status(self);
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
        report_outcome(self, kept);
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
    if (message == WM_KEYDOWN && shortcut_keydown(self, wparam))
    {
        return true;
    }
    if (message != WM_CHAR)
    {
        return false;
    }
    if (wparam == shortcut_character_for(FOLIO_COMMAND_NEW))
    {
        execute_command(self, FOLIO_COMMAND_NEW, "");
        return true;
    }
    if (wparam == shortcut_character_for(FOLIO_COMMAND_HELP))
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
        self->keyboard_target = ACCESSIBLE_TREE;
        SetFocus(self->handle);
        return true;
    }
    if (message == WM_CHAR && wparam == shortcut_character_for(FOLIO_COMMAND_SAVE))
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
    SendMessageW(window, EM_GETRECT, 0, (LPARAM)&bounds);
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
    int height = client.bottom - client.top;
    size = size < height ? size : height;
    int middle = (client.top + client.bottom) / 2;
    RECT bounds = {client.right - size, middle - size / 2, client.right, middle - size / 2 + size};
    return bounds;
}

/* 「×」は頭の draw_close と同じ面のパスで描く（ADR 0033 の決定 3・補正 9）。× を描く経路は 1 本。
 * 箱は filter_clear_rect が欄の高さに収め、倍率は icon_paint_fill が bounds から出す。
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
    SetFocus(window);
    if (GetFocus() == window)
    {
        SetWindowTextW(window, L"");
    }
    return true;
}

/* 検索は保存済み本文だけを見る。名前付きEDITのfocus入口で共通保存を先に通す。 */
static void save_on_filter_focus(struct folio_window *_Nonnull self)
{
    if (self->filter_saving || session_busy(self) || pane_streaming(self) ||
        self->removal_running || folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NAMED ||
        folio_state_pane_mode(self->state) != PANE_MODE_EDIT)
    {
        return;
    }
    if (self->pane != nullptr && note_pane_composing(self->pane))
    {
        focus_pane(self);
        return;
    }
    self->filter_saving = true;
    enum folio_state_outcome outcome = store_body(self);
    if (outcome != FOLIO_STATE_READY)
    {
        focus_pane(self);
        if (outcome != FOLIO_STATE_CANCELLED)
        {
            report_outcome(self, outcome);
        }
        focus_pane(self);
    }
    self->filter_saving = false;
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
}

/* 欄の枠は focus 中だけ 2px なので描き直す（ADR0060 の決定 3）。focus を受けたら先に保存する。 */
static void filter_focus_changed(struct folio_window *_Nonnull self, UINT message)
{
    if (message == WM_SETFOCUS || message == WM_KILLFOCUS)
    {
        redraw_drawer(self);
    }
    if (message == WM_SETFOCUS)
    {
        save_on_filter_focus(self);
    }
}

static LRESULT CALLBACK filter_input_message(HWND window, UINT message, WPARAM wparam,
                                             LPARAM lparam)
{
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr || self->filter_original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    if (session_blocks_message(self, message))
    {
        return 0;
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
    filter_focus_changed(self, message);
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

/* 準備開始/取消/完成/失敗は同じ表示更新へ合流する。本文の投入は行わない。 */
static void refresh_index_filter(struct folio_window *_Nonnull self, bool was_filtering)
{
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

/* 打つたびに語を覚え直し、索引を作り直す（ADR 0024 の決定 6）。
 * IME の未確定入力のあいだは動かさず、確定の WM_IME_ENDCOMPOSITION のあとで 1 回だけ動く。
 * 壊れた語は前の絞り込みを保つだけで、打つたびにモーダルを出さない。 */
static void filter_input_changed(struct folio_window *_Nonnull self)
{
    if (self->filter_composing || self->filter_saving || self->filter_input == nullptr)
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
        report_outcome(self, filtered);
        return;
    }
    refresh_index_filter(self, was_filtering);
}

/* Ctrl+Shift+F は常設の欄へフォーカスを移し、いまの語を全選択する（ADR 0024 の決定 6）。 */
static void focus_filter_input(struct folio_window *_Nonnull self)
{
    show_drawer_layout(self);
    if (self->filter_input == nullptr || !IsWindowVisible(self->filter_input) ||
        !IsWindowEnabled(self->filter_input))
    {
        return;
    }
    SetFocus(self->filter_input);
    SendMessageW(self->filter_input, EM_SETSEL, 0, (LPARAM)-1);
}

static bool command_query_deferred(const struct folio_window *_Nonnull self)
{
    return self->command_initializing ||
           (self->command_surface == COMMAND_SURFACE_THEMES && self->command_composing);
}

static LRESULT on_command(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (!command_owns(self, (HWND)lparam) || HIWORD(wparam) != EN_CHANGE)
    {
        return 0;
    }
    if (command_query_deferred(self))
        return 0;
    if (self->command_surface == COMMAND_SURFACE_PALETTE ||
        self->command_surface == COMMAND_SURFACE_THEMES)
    {
        invalidate_surface_items(self);
    }
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        theme_query_changed(self);
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
 * 欄の画素が塗り潰されて語が消える（#86 / ADR 0024 の 2026-09-22 の補正 1・2）。
 * 守るのはドロワー側の `WS_CLIPSIBLINGS` と z 順の両方で、欄に `WS_CLIPSIBLINGS` を
 * 付けても効かない（実測 out/design/2026-09-23/filter-visible-probe/）。
 * 欄の `WS_CLIPSIBLINGS` は別の用で、z 順で欄より前にいる入力面の層を欄が塗らないためにある。
 * 無いと、窓が低いとき（圧縮配置）に層の案内の行へ欄が重なって描かれる（#155）。 */
static bool create_filter_input(struct folio_window *_Nonnull self, HWND window)
{
    self->filter_input = CreateWindowExW(
        0, edit_class, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_AUTOHSCROLL, 0, 0, 0, 0,
        window, (HMENU)(INT_PTR)filter_control_id, GetModuleHandleW(nullptr), nullptr);
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

/* 見えていない部品の tool は空の矩形にして出さない。矩形は描画と同じ 1 本。 */
static RECT tooltip_rect(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    bool visible = part_enabled(self, kind) && accessible_visible(self, kind);
    return visible ? accessible_main_rect(self, kind) : (RECT){0};
}

static void activate_tooltip(const struct folio_window *_Nonnull self)
{
    if (self->tooltip == nullptr)
    {
        return;
    }
    bool active = shell_ready(self);
    if (!active)
    {
        SendMessageW(self->tooltip, TTM_POP, 0, 0);
    }
    SendMessageW(self->tooltip, TTM_ACTIVATE, active, 0);
}

static void refresh_tooltip_style(const struct folio_window *_Nonnull self)
{
    if (self->tooltip != nullptr)
    {
        SendMessageW(self->tooltip, WM_SETFONT, (WPARAM)self->mono_font, FALSE);
        SendMessageW(self->tooltip, TTM_SETTIPBKCOLOR, self->palette.dialog_surface, 0);
        SendMessageW(self->tooltip, TTM_SETTIPTEXTCOLOR, self->palette.current_text, 0);
        SendMessageW(self->tooltip, TTM_SETMAXTIPWIDTH, 0,
                     scale(320, GetDpiForWindow(self->handle)));
    }
}

/* ドロワーのカテゴリ＋（C 様式）の tool。名前は UIA と同じ表から引き、矩形はドロワーの client
 * 座標。 文言を固定で渡すので、言語と配置が変わるたびに描き直しの経路から写し直す。 */
static TOOLINFOW drawer_tool(const struct folio_window *_Nonnull self, char16_t *_Nonnull text)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    TOOLINFOW tool = {.cbSize = tooltip_info_size,
                      .uFlags = TTF_SUBCLASS,
                      .hwnd = drawer,
                      .uId = ACCESSIBLE_NEW_CATEGORY,
                      .lpszText = (wchar_t *)text};
    text[0] = u'\0';
    (void)wide_units(ui_text_line(accessible_controls[ACCESSIBLE_NEW_CATEGORY].label,
                                  folio_state_language(self->state)),
                     text);
    struct accessible_item item = {.kind = ACCESSIBLE_NEW_CATEGORY};
    struct accessible_view view = {0};
    if (drawer != nullptr && drawer_visible(self) &&
        self->command_surface == COMMAND_SURFACE_CLOSED && self->native_modal_depth == 0 &&
        SUCCEEDED(drawer_window_accessible_read(self->drawer, &item, &view)))
    {
        tool.rect = view.bounds;
        MapWindowPoints(nullptr, drawer, (POINT *)&tool.rect, 2);
    }
    return tool;
}

static TOOLINFOW surface_tool(const struct folio_window *_Nonnull self, size_t slot)
{
    RECT rect = {0};
    if (self->native_modal_depth == 0 && surface_button_visible(self, slot))
        rect = accessible_surface_button_rect(self, slot);
    return (TOOLINFOW){.cbSize = tooltip_info_size,
                       .uFlags = TTF_SUBCLASS,
                       .hwnd = self->command_layer,
                       .uId = surface_tooltip_base + slot,
                       .rect = rect,
                       .lpszText = LPSTR_TEXTCALLBACKW};
}

static void refresh_surface_tooltip_rects(const struct folio_window *_Nonnull self)
{
    if (self->tooltip == nullptr)
        return;
    SendMessageW(self->tooltip, TTM_POP, 0, 0);
    for (size_t slot = 0; slot < surface_button_count; ++slot)
    {
        TOOLINFOW tool = surface_tool(self, slot);
        SendMessageW(self->tooltip, TTM_NEWTOOLRECTW, 0, (LPARAM)&tool);
    }
}

static bool create_surface_tooltips(const struct folio_window *_Nonnull self)
{
    for (size_t slot = 0; slot < surface_button_count; ++slot)
    {
        TOOLINFOW tool = surface_tool(self, slot);
        if (!SendMessageW(self->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool))
            return false;
    }
    return true;
}

static void refresh_tooltip_rects(const struct folio_window *_Nonnull self)
{
    if (self->tooltip == nullptr)
    {
        return;
    }
    char16_t text[draw_unit_limit];
    TOOLINFOW drawer = drawer_tool(self, text);
    if (drawer.hwnd != nullptr)
    {
        SendMessageW(self->tooltip, TTM_NEWTOOLRECTW, 0, (LPARAM)&drawer);
        SendMessageW(self->tooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&drawer);
    }
    SendMessageW(self->tooltip, TTM_POP, 0, 0);
    for (size_t index = 0; index < main_part_count(); ++index)
    {
        TOOLINFOW tool = {.cbSize = tooltip_info_size,
                          .hwnd = self->handle,
                          .uId = main_parts[index].kind,
                          .rect = tooltip_rect(self, main_parts[index].kind)};
        SendMessageW(self->tooltip, TTM_NEWTOOLRECTW, 0, (LPARAM)&tool);
    }
}

static bool create_tooltip(struct folio_window *_Nonnull self)
{
    INITCOMMONCONTROLSEX controls = {.dwSize = sizeof controls, .dwICC = ICC_WIN95_CLASSES};
    if (!InitCommonControlsEx(&controls))
    {
        return false;
    }
    self->tooltip = CreateWindowExW(WS_EX_NOACTIVATE, TOOLTIPS_CLASSW, nullptr,
                                    WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP, CW_USEDEFAULT,
                                    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, self->handle,
                                    nullptr, GetModuleHandleW(nullptr), nullptr);
    if (self->tooltip == nullptr || !SetWindowPos(self->tooltip, HWND_TOPMOST, 0, 0, 0, 0,
                                                  SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE))
    {
        return false;
    }
    for (size_t index = 0; index < main_part_count(); ++index)
    {
        TOOLINFOW tool = {.cbSize = tooltip_info_size,
                          .uFlags = TTF_SUBCLASS,
                          .hwnd = self->handle,
                          .uId = main_parts[index].kind,
                          .rect = tooltip_rect(self, main_parts[index].kind),
                          .lpszText = LPSTR_TEXTCALLBACKW};
        if (!SendMessageW(self->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool))
        {
            return false;
        }
    }
    char16_t text[draw_unit_limit];
    TOOLINFOW drawer = drawer_tool(self, text);
    if (drawer.hwnd == nullptr || !SendMessageW(self->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&drawer))
    {
        return false;
    }
    refresh_tooltip_style(self);
    activate_tooltip(self);
    return create_surface_tooltips(self);
}

static bool create_settings_backdrop(struct folio_window *_Nonnull self)
{
    self->settings_backdrop =
        CreateWindowExW(WS_EX_LAYERED | WS_EX_NOACTIVATE, settings_backdrop_class, L"", WS_CHILD, 0,
                        0, 0, 0, self->handle, nullptr, GetModuleHandleW(nullptr), self);
    return self->settings_backdrop != nullptr &&
           SetLayeredWindowAttributes(self->settings_backdrop, 0,
                                      self->palette.dialog_backdrop_alpha, LWA_ALPHA);
}

/* OS の「キーボードの印を常に表示」。読むのは作成時と WM_SETTINGCHANGE だけ（ADR0060 の決定 3）。
 */
static void read_focus_cues(struct folio_window *_Nonnull self)
{
    BOOL always = FALSE;
    bool known = SystemParametersInfoW(SPI_GETKEYBOARDCUES, 0, &always, 0) != 0;
    bool shown = known && always != FALSE;
    if (shown != self->focus_cues_always)
    {
        self->focus_cues_always = shown;
        InvalidateRect(self->handle, nullptr, FALSE);
        redraw_drawer(self);
    }
}

static bool create_modal_resources(struct folio_window *_Nonnull self)
{
    return modal_owner_create(self->handle, &self->modal_owner) == MODAL_OWNER_READY &&
           name_prompt_metrics_create(&self->native_metrics) == NAME_PROMPT_METRICS_CREATED;
}

static bool create_surface_resources(struct folio_window *_Nonnull self)
{
    return create_modal_resources(self) &&
           command_candidates_create(&self->command_candidates) == CANDIDATE_CREATED &&
           theme_candidates_create(&self->theme_candidates) == CANDIDATE_CREATED;
}

static LRESULT on_create(HWND window, LPARAM lparam)
{
    /* Win32 のコールバック引数を境界で受ける唯一の void *（C-006）。 */
    const CREATESTRUCTW *_Nonnull creation = (const CREATESTRUCTW *)lparam;
    struct folio_window *_Nonnull self = creation->lpCreateParams;
    SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)self);
    self->handle = window;
    if (!create_surface_resources(self))
        return -1;
    refresh_font(self, GetDpiForWindow(window));
    read_focus_cues(self);
    if (drawer_window_create(window, self->state, self->accessibility, &self->drawer) !=
            DRAWER_WINDOW_CREATED ||
        note_pane_create(window, self->palette.pane, self->palette.editor_text, &self->pane) !=
            NOTE_PANE_CREATED)
    {
        return -1;
    }
    /* 既定書式の face は core の表が決める（ADR 0032 の決定 4）。作った直後に 1 回当てる。 */
    apply_pane_face(self);
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
    if (self->command_layer == nullptr || !create_settings_backdrop(self))
    {
        return -1;
    }
    /* レイヤーは文字を受けないので IME の文脈を外す（ADR 0031 の決定 7・窓ごとの設定）。
     * 同じプロセスの EDIT の文脈は残る。 */
    self->layer_context = ImmAssociateContext(self->command_layer, nullptr);
    if (!create_command_inputs(self) || !create_tooltip(self))
    {
        return -1;
    }
    return render_pane(self) == FOLIO_STATE_READY ? 0 : -1;
}

/* OS が勧める矩形へ移し、字の大きさを取り直す（FR-013）。 */
static void apply_dpi(struct folio_window *_Nonnull self, LPARAM lparam)
{
    close_geometry_menu(self);
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
    const struct folio_window *_Nullable self = self_of(window);
    int width = self != nullptr && folio_state_layout_mode(self->state) == FOLIO_LAYOUT_DRAWER
                    ? base_drawer_window_width
                    : base_min_width;
    limits->ptMinTrackSize.x = scale(width, dpi);
    limits->ptMinTrackSize.y = scale(base_min_height, dpi);
    return 0;
}

static LRESULT color_command_input(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr)
        return DefWindowProcW(window, message, wparam, lparam);
    bool ours = command_owns(self, (HWND)lparam) || (HWND)lparam == self->filter_input;
    if (!ours || self->command_brush == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
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

static bool layer_input_handled(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    if (session_blocks_message(self, message) || load_blocks_input(self, message))
    {
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_MENU &&
        menu_message(self, message, wparam, lparam))
        return true;
    accessibility_host_changed(self->accessibility, message);
    update_command_composition(self, message);
    if ((message == WM_KEYDOWN || message == WM_CHAR) &&
        command_input_handled(self, message, wparam))
    {
        return true;
    }
    return false;
}

static size_t surface_tab_at(const struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_surface != COMMAND_SURFACE_PALETTE)
        return surface_pointer_count;
    for (size_t index = 0; index < 2; ++index)
    {
        RECT tab = command_tab_rect(self, index);
        if (PtInRect(&tab, point))
            return surface_button_count + index;
    }
    return surface_pointer_count;
}

static size_t surface_part_at(const struct folio_window *_Nonnull self, POINT point)
{
    for (size_t slot = 0; slot < surface_button_count; ++slot)
    {
        if (!surface_button_visible(self, slot))
            continue;
        RECT bounds = accessible_surface_button_rect(self, slot);
        if (PtInRect(&bounds, point))
            return slot;
    }
    return surface_tab_at(self, point);
}

static void cancel_surface_press(struct folio_window *_Nonnull self)
{
    if (self->surface_pressed < surface_pointer_count)
    {
        self->surface_pressed = surface_pointer_count;
        self->surface_pressed_inside = false;
        redraw_command_layer(self);
    }
}

static void surface_pointer(struct folio_window *_Nonnull self, POINT point)
{
    size_t hover = surface_part_at(self, point);
    bool inside = self->surface_pressed < surface_pointer_count && hover == self->surface_pressed;
    bool changed = hover != self->surface_hover || inside != self->surface_pressed_inside;
    self->surface_hover = hover;
    self->surface_pressed_inside = inside;
    if (!self->surface_tracking)
    {
        TRACKMOUSEEVENT track = {
            .cbSize = sizeof track, .dwFlags = TME_LEAVE, .hwndTrack = self->command_layer};
        self->surface_tracking = TrackMouseEvent(&track) != 0;
    }
    if (changed)
        redraw_command_layer(self);
}

static bool press_surface_button(struct folio_window *_Nonnull self, POINT point)
{
    size_t slot = surface_part_at(self, point);
    if (slot == surface_pointer_count)
        return false;
    if (folio_window_accessible_ready(self))
    {
        self->surface_pressed = slot;
        self->surface_pressed_inside = true;
        self->surface_hover = slot;
        SetCapture(self->command_layer);
        redraw_command_layer(self);
    }
    return true;
}

static void release_surface_button(struct folio_window *_Nonnull self, POINT point)
{
    size_t slot = self->surface_pressed;
    bool run = slot < surface_pointer_count && surface_part_at(self, point) == slot &&
               folio_window_accessible_ready(self);
    cancel_surface_press(self);
    if (GetCapture() == self->command_layer)
        ReleaseCapture();
    if (run && slot < surface_button_count)
        (void)surface_button_action(self, slot);
    if (run && slot >= surface_button_count &&
        self->command_keys_visible != (slot - surface_button_count == 1))
        toggle_command_keys(self);
}

static bool surface_pointer_message(struct folio_window *_Nonnull self, UINT message, LPARAM lparam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
    {
        POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
        if (!press_surface_button(self, point))
            click_command_surface(self, point);
        return true;
    }
    case WM_LBUTTONUP:
        release_surface_button(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return true;
    case WM_MOUSEMOVE:
        surface_pointer(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return true;
    case WM_MOUSELEAVE:
        self->surface_tracking = false;
        self->surface_hover = surface_pointer_count;
        self->surface_pressed_inside = false;
        redraw_command_layer(self);
        return true;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        cancel_surface_press(self);
        return true;
    default:
        return false; /* Win32のmessageは開いた集合。 */
    }
}

static LRESULT CALLBACK command_layer_message(HWND window, UINT message, WPARAM wparam,
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
    if (layer_input_handled(self, message, wparam, lparam))
        return 0;
    if (surface_pointer_message(self, message, lparam))
        return 0;
    switch (message)
    {
    case WM_NOTIFY:
        return on_notify(self, lparam);
    case WM_GETOBJECT:
        return accessibility_host_object(self->accessibility, ACCESSIBLE_SURFACE, wparam, lparam);
    case WM_PAINT:
        paint_command_layer(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEWHEEL:
        command_wheel(self, wparam);
        return 0;
    /* 設定画面はレイヤーがフォーカスと鍵を受ける（ADR 0031 の決定 7）。
     * 鍵の扱いは EDIT のサブクラスと同じ command_key_down / command_character を共有する。 */
    case WM_SETFOCUS:
        redraw_command_layer(self);
        return 0;
    case WM_KILLFOCUS:
        if (!self->removal_running)
        {
            PostMessageW(self->handle, folio_message_command_focus_lost, 0, 0);
        }
        return 0;
    case WM_COMMAND:
        return on_command(self, wparam, lparam);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        return color_command_input(window, message, wparam, lparam);
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

/* 暗幕は表示と入力の遮断だけを所有し、設定の値を持たない。 */
static void paint_settings_backdrop(const struct folio_window *_Nonnull self, HWND window)
{
    PAINTSTRUCT painting;
    HDC device = BeginPaint(window, &painting);
    SetDCBrushColor(device, self->palette.dialog_backdrop);
    FillRect(device, &painting.rcPaint, (HBRUSH)GetStockObject(DC_BRUSH));
    EndPaint(window, &painting);
}

static LRESULT CALLBACK settings_backdrop_procedure(HWND window, UINT message, WPARAM wparam,
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
    if (session_blocks_message(self, message))
    {
        return 0;
    }
    switch (message)
    {
    case WM_NCHITTEST:
        return resize_through(self, lparam) ? HTTRANSPARENT
                                            : DefWindowProcW(window, message, wparam, lparam);
    case WM_PAINT:
        paint_settings_backdrop(self, window);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_MOUSEWHEEL:
        focus_command_input(self);
        return 0;
    default:
        return DefWindowProcW(window, message, wparam, lparam);
    }
}

static void window_destroyed(struct folio_window *_Nonnull self)
{
    if (self->accessibility != nullptr)
    {
        accessibility_host_detach(self->accessibility);
    }
    if (self->tooltip != nullptr)
    {
        DestroyWindow(self->tooltip);
        self->tooltip = nullptr;
    }
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
    if (self->gutter_font != nullptr)
    {
        DeleteObject(self->gutter_font);
        self->gutter_font = nullptr;
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
    if (self->removal_running || session_busy(self))
    {
        return;
    }
    HWND focus = GetFocus();
    bool moved_within_window = focus != nullptr && !command_owns(self, focus) &&
                               GetAncestor(focus, GA_ROOT) == self->handle;
    if (self->command_surface != COMMAND_SURFACE_CLOSED && moved_within_window)
    {
        dismiss_command_surface(self);
    }
}

static bool layout_shortcut_allowed(const struct folio_window *_Nonnull self)
{
    if (settings_surface(self) || self->removal_running || !IsWindowEnabled(self->handle) ||
        self->root_composing || self->command_composing || self->filter_composing ||
        (self->pane != nullptr && note_pane_composing(self->pane)))
    {
        return false;
    }
    HWND focus = GetFocus();
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    return focus != nullptr && (focus == self->handle || focus == self->filter_input ||
                                (pane != nullptr && focus == pane) || command_owns(self, focus));
}

static void run_shortcut(struct folio_window *_Nonnull self,
                         const struct folio_shortcut *_Nonnull binding)
{
    switch (folio_shortcut_action(binding))
    {
    case FOLIO_SHORTCUT_ACTION_COMMAND:
    {
        enum folio_command command = folio_shortcut_command(binding);
        if (folio_command_scope(command) != FOLIO_COMMAND_SCOPE_LAYOUT ||
            layout_shortcut_allowed(self))
            execute_command(self, command, "");
        return;
    }
    case FOLIO_SHORTCUT_ACTION_CONTEXT_MENU:
        if (self->drawer != nullptr)
            drawer_window_context_menu(self->drawer);
        return;
    case FOLIO_SHORTCUT_ACTION_FILTER:
        focus_filter_input(self);
        return;
    case FOLIO_SHORTCUT_ACTION_SEARCH_FORWARD:
        step_search(self, SEARCH_DIRECTION_FORWARD);
        return;
    case FOLIO_SHORTCUT_ACTION_SEARCH_BACKWARD:
        step_search(self, SEARCH_DIRECTION_BACKWARD);
        return;
    case FOLIO_SHORTCUT_ACTION_SEARCH_NEXT:
        repeat_search(self, false);
        return;
    case FOLIO_SHORTCUT_ACTION_SEARCH_PREVIOUS:
        repeat_search(self, true);
        return;
    }
}

static void execute_accelerator(struct folio_window *_Nonnull self, WPARAM wparam)
{
    if (HIWORD(wparam) != 1 || keyboard_accelerator(self, LOWORD(wparam)))
        return;
    WORD id = LOWORD(wparam);
    if (id >= 200 && (size_t)(id - 200) < folio_shortcut_count())
        run_shortcut(self, folio_shortcut_at((size_t)(id - 200)));
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
        self->body_touched = true;
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

/* OSが設定面のrootへ戻した実focusを、既存の有効可視入力へ正規化する。 */
static void normalize_settings_focus(struct folio_window *_Nonnull self)
{
    if (!settings_surface(self) || self->native_modal_depth != 0 || IsIconic(self->handle) ||
        GetFocus() != self->handle || !IsWindowEnabled(self->handle) ||
        !IsWindowVisible(self->handle))
    {
        return;
    }
    HWND _Nullable target = command_focus_target(self);
    if (target != nullptr && IsWindowEnabled(target) && IsWindowVisible(target))
    {
        focus_command_input(self);
    }
}

/* OSがNOTEのrootへ戻した実focusだけを、可視paneへ正規化する。文書が無く本文を隠している間は
 * 見えない索引に残さず、F6 の先頭（新しいノート）へ寄せて印を出す（#349）。 */
static void normalize_root_focus(struct folio_window *_Nonnull self)
{
    normalize_settings_focus(self);
    normalize_header_target(self);
    if (self->keyboard_target != ACCESSIBLE_TREE ||
        folio_state_layout_mode(self->state) != FOLIO_LAYOUT_NOTE ||
        self->command_surface != COMMAND_SURFACE_CLOSED || self->pane == nullptr)
    {
        return;
    }
    if (!document_open(self))
    {
        focus_header_entry(self);
        return;
    }
    HWND pane = note_pane_handle(self->pane);
    if (pane != nullptr && pane_visible(self) && IsWindowVisible(pane) && IsWindowEnabled(pane) &&
        IsWindowEnabled(self->handle) && GetFocus() == self->handle)
    {
        SetFocus(pane);
    }
}

static LRESULT on_window_command(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (pane_streaming(self))
    {
        (void)pane_notification(self, wparam, lparam);
        return 0;
    }
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

/* 通常のresizeは本文矩形の変化を保持する。表示切替中の同期WM_SIZEは外側の退避に含める。 */
static void resize(struct folio_window *_Nonnull self)
{
    close_geometry_menu(self);
    if (self->layout_changing)
    {
        arrange(self);
        return;
    }
    rearrange_keeping_line(self);
}

/* OS通知とモーダルの再取得要求はここへ合流する。enumのテーマ名で色変更を判定しない。 */
static void system_colors_changed(struct folio_window *_Nonnull self)
{
    read_focus_cues(self);
    if (pane_streaming(self) || session_busy(self))
    {
        self->appearance_pending = true;
        return;
    }
    self->appearance_pending = false;
    uint64_t before = folio_state_palette_revision(self->state);
    enum folio_state_outcome outcome = folio_state_refresh_theme(self->state);
    if (outcome == FOLIO_STATE_READY &&
        (folio_state_palette_revision(self->state) != before || self->appearance_recolor_pending))
    {
        recolor_window(self);
        return;
    }
    if (settings_surface(self))
    {
        arrange_command_input(self);
        redraw_command_layer(self);
    }
}

static void resume_appearance(struct folio_window *_Nonnull self)
{
    if (self->appearance_pending && self->handle != nullptr && !pane_streaming(self) &&
        !session_busy(self))
    {
        self->appearance_pending =
            !PostMessageW(self->handle, folio_message_appearance_changed, 0, 0);
    }
}

/* 部品の押下・hover・Space の離鍵（ADR0060 の決定 3・4）。 */
static void window_activation(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (LOWORD(wparam) != WA_INACTIVE)
        return;
    if (self->native_modal_depth > 0 && lparam != 0)
        modal_owner_anchor(self->handle, (HWND)lparam);
    cancel_part_press(self);
    if (self->command_surface == COMMAND_SURFACE_MENU)
        close_command_surface(self);
}

static LRESULT on_part_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                               LPARAM lparam)
{
    switch (message)
    {
    case WM_LBUTTONUP:
        release_part(self, lparam);
        return 0;
    case WM_CAPTURECHANGED:
        if (!self->pressed_by_key)
        {
            cancel_part_press(self);
        }
        return 0;
    case WM_CANCELMODE:
        cancel_part_press(self);
        return DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_MOUSEMOVE:
        track_pointer(self, lparam);
        return 0;
    case WM_MOUSELEAVE:
        leave_pointer(self);
        return 0;
    case WM_KEYUP:
        release_key(self, wparam);
        return 0;
    case WM_ACTIVATE:
        window_activation(self, wparam, lparam);
        return DefWindowProcW(self->handle, message, wparam, lparam);
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return DefWindowProcW(self->handle, message, wparam, lparam);
    }
}

/* 鍵・フォーカス・ホイール・終了。on_message を C-012 の行数に収めるために分けてある。 */
static LRESULT on_input_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    if (self->removal_running && self->native_modal_depth == 0 &&
        (message == WM_COMMAND || message == WM_KEYDOWN || message == WM_CHAR ||
         message == WM_CLOSE))
    {
        return 0;
    }
    switch (message)
    {
    case WM_LBUTTONUP:
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_KEYUP:
    case WM_ACTIVATE:
        return on_part_message(self, message, wparam, lparam);
    case WM_QUERYENDSESSION:
        return query_session_end(self);
    case WM_ENABLE:
        activate_tooltip(self);
        if (wparam == FALSE)
        {
            set_hover(self, ACCESSIBLE_WINDOW);
        }
        return 0;
    case WM_ENDSESSION:
        return end_session(self, wparam);
    case WM_COMMAND:
        return on_window_command(self, wparam, lparam);
    case WM_KEYDOWN:
        press_key(self, wparam, lparam);
        return 0;
    case WM_CHAR:
        type_key(self, wparam);
        return 0;
    case WM_SETFOCUS:
        normalize_root_focus(self);
        redraw_drawer(self);
        return 0;
    case WM_KILLFOCUS:
        cancel_part_press(self);
        redraw_drawer(self);
        InvalidateRect(self->handle, nullptr, FALSE);
        return 0;
    case WM_MOUSEWHEEL:
        forward_wheel(self, wparam, lparam);
        return 0;
    case WM_CLOSE:
        close_window(self);
        return 0;
    case WM_DESTROY:
        window_destroyed(self);
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return on_accessibility_message(self, message, wparam, lparam);
    }
}

static LRESULT on_child_request(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    switch (message)
    {
    case folio_message_resize_through:
        return resize_through(self, lparam);
    case folio_message_pane_zoomed:
        arrange_zoomed_pane(self, (size_t)wparam);
        return 0;
    case folio_message_choose_color:
        return choose_category_color(self, (CHOOSECOLORW *)lparam);
    case folio_message_close_requested:
        resume_normal_close(self);
        return 0;
    case folio_message_open_menu:
        show_menu_surface(self, (const struct menu_request *)lparam);
        return 0;
    case folio_message_report_outcome:
        report_outcome(self, (enum folio_state_outcome)wparam);
        return 0;
    default:
        return on_input_message(self, message, wparam, lparam);
    }
}

static LRESULT shell_system_action(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (self->native_modal_depth > 0)
        modal_owner_capture_focus(self->modal_owner);
    LRESULT result = DefWindowProcW(self->handle, WM_SYSCOMMAND, wparam, lparam);
    if (self->handle != nullptr && self->native_modal_depth > 0 && !IsIconic(self->handle))
        modal_owner_restore_focus(self->modal_owner);
    return result;
}

static LRESULT on_shell_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    switch (message)
    {
    case WM_WINDOWPOSCHANGED:
    {
        LRESULT result = DefWindowProcW(self->handle, message, wparam, lparam);
        if (self->modal_owner != nullptr)
            modal_owner_arrange(self->modal_owner);
        return result;
    }
    case WM_MOUSEACTIVATE:
        if (self->native_modal_depth > 0)
        {
            modal_owner_capture_focus(self->modal_owner);
            return MA_NOACTIVATE;
        }
        return DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_EXITSIZEMOVE:
        if (self->native_modal_depth > 0)
            modal_owner_restore_focus(self->modal_owner);
        return DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_SYSCOMMAND:
        return shell_system_action(self, wparam, lparam);
    default:
        return on_child_request(self, message, wparam, lparam);
    }
}

static LRESULT on_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                          LPARAM lparam)
{
    switch (message)
    {
    case WM_IME_STARTCOMPOSITION:
        self->root_composing = true;
        return DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_IME_ENDCOMPOSITION:
        self->root_composing = false;
        return DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_NCHITTEST:
        return hit_test(self, lparam);
    case WM_SIZE:
        resize(self);
        return 0;
    case WM_DPICHANGED:
        apply_dpi(self, lparam);
        return 0;
    case WM_SETTINGCHANGE:
    case WM_SYSCOLORCHANGE:
    case folio_message_appearance_changed:
        system_colors_changed(self);
        return 0;
    case WM_PAINT:
        paint_pane(self);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_LBUTTONDOWN:
        if (!self->removal_running || self->native_modal_depth > 0)
        {
            press_part(self, lparam);
        }
        return 0;
    case WM_NOTIFY:
        return on_notify(self, lparam);
    /* 常設絞込と入力面EDITは同じ配色経路を使う。 */
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        return color_command_input(self->handle, message, wparam, lparam);
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
    case folio_message_drawer_menu:
        /* lParam は送る側のスタックにある対象（同期の SendMessageW なので生きている）。 */
        run_drawer_menu(self, (enum drawer_menu_item)wparam, (const struct note_ref *)lparam);
        return 0;
    default:
        return on_shell_message(self, message, wparam, lparam);
    }
}

static LRESULT CALLBACK window_message(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
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
    if (root_blocks_message(self, message, wparam) ||
        (self->native_modal_depth == 0 && load_blocks_input(self, message)))
    {
        return 0;
    }
    accessibility_host_changed(self->accessibility, message);
    LRESULT result = on_message(self, message, wparam, lparam);
    resume_appearance(self);
    return result;
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
           ensure_named_class(instance, command_layer_class, command_layer_procedure) &&
           ensure_named_class(instance, settings_backdrop_class, settings_backdrop_procedure);
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

/* 同梱の書体を読めなかったことは、窓が退避の face で最初に描かれたあとに 1 回だけ知らせる
 * （ADR 0036 の決定 5）。folio_window_create からだけ呼び、create は 1 回しか走らないので
 * 「見せたか」の真偽は持たない。 */
static void announce_fonts(struct folio_window *_Nonnull self, enum font_bundle_outcome fonts)
{
    if (fonts != FONT_BUNDLE_READY)
    {
        UpdateWindow(self->handle);
        show_failure_box(self, FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE);
    }
}

/* 主窓の 2 つのアクセラレータの表を作る。両方を作ってから成否を見る（どちらかが
 * nullptr なら偽）。作れた側の後始末は呼び出し元の folio_window_destroy が行う。 */
static WORD shortcut_virtual_key(enum folio_shortcut_key key)
{
    switch (key)
    {
    case FOLIO_SHORTCUT_KEY_S:
        return 'S';
    case FOLIO_SHORTCUT_KEY_N:
        return 'N';
    case FOLIO_SHORTCUT_KEY_P:
        return 'P';
    case FOLIO_SHORTCUT_KEY_F:
        return 'F';
    case FOLIO_SHORTCUT_KEY_ONE:
        return '1';
    case FOLIO_SHORTCUT_KEY_TWO:
        return '2';
    case FOLIO_SHORTCUT_KEY_THREE:
        return '3';
    case FOLIO_SHORTCUT_KEY_F1:
        return VK_F1;
    case FOLIO_SHORTCUT_KEY_F2:
        return VK_F2;
    case FOLIO_SHORTCUT_KEY_F3:
        return VK_F3;
    case FOLIO_SHORTCUT_KEY_F4:
        return VK_F4;
    case FOLIO_SHORTCUT_KEY_F10:
        return VK_F10;
    case FOLIO_SHORTCUT_KEY_APPS:
        return VK_APPS;
    case FOLIO_SHORTCUT_KEY_ENTER:
        return VK_RETURN;
    }
    return 0;
}

static BYTE shortcut_virtual_modifiers(enum folio_shortcut_modifiers modifiers)
{
    switch (modifiers)
    {
    case FOLIO_SHORTCUT_MOD_NONE:
        return FVIRTKEY;
    case FOLIO_SHORTCUT_MOD_SHIFT:
        return FVIRTKEY | FSHIFT;
    case FOLIO_SHORTCUT_MOD_CONTROL:
        return FVIRTKEY | FCONTROL;
    case FOLIO_SHORTCUT_MOD_CONTROL_SHIFT:
        return FVIRTKEY | FCONTROL | FSHIFT;
    case FOLIO_SHORTCUT_MOD_ALT:
        return FVIRTKEY | FALT;
    }
    return FVIRTKEY;
}

static HACCEL _Nullable shortcut_table(enum folio_shortcut_route route)
{
    /* Catalog is fixed and bounded; no variable-length array or product test hook. */
    ACCEL rows[32];
    size_t count = 0;
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_route(binding) != route)
            continue;
        if (count == sizeof rows / sizeof rows[0])
            return nullptr;
        rows[count++] =
            (ACCEL){.fVirt = shortcut_virtual_modifiers(folio_shortcut_modifiers(binding)),
                    .key = shortcut_virtual_key(folio_shortcut_key(binding)),
                    .cmd = (WORD)(200 + at)};
    }
    return CreateAcceleratorTableW(rows, (int)count);
}

static bool create_accelerators(struct folio_window *_Nonnull self)
{
    self->commands = shortcut_table(FOLIO_SHORTCUT_HACCEL);
    self->search_keys = shortcut_table(FOLIO_SHORTCUT_SEARCH_HACCEL);
    return self->commands != nullptr && self->search_keys != nullptr;
}

static WPARAM shortcut_character_for(enum folio_command command)
{
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_route(binding) == FOLIO_SHORTCUT_CHARACTER &&
            folio_shortcut_command(binding) == command)
            return (WPARAM)(shortcut_virtual_key(folio_shortcut_key(binding)) & 0x1f);
    }
    return 0;
}

static bool shortcut_keydown(struct folio_window *_Nonnull self, WPARAM key)
{
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_route(binding) == FOLIO_SHORTCUT_KEYDOWN &&
            key == shortcut_virtual_key(folio_shortcut_key(binding)))
        {
            run_shortcut(self, binding);
            return true;
        }
    }
    return false;
}

enum folio_window_outcome folio_window_create(struct folio_state *_Nonnull state,
                                              enum font_bundle_outcome fonts,
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
    self->surface_hover = surface_pointer_count;
    self->surface_pressed = surface_pointer_count;
    if (!create_accelerators(self) || !create_keyboard_accelerators(self))
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
    self->state = state;
    self->keyboard_target = ACCESSIBLE_TREE;
    if (FAILED(accessibility_host_create(self, &self->accessibility)))
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
    self->palette = folio_palette_from(folio_state_palette(state));
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
    accessibility_host_schedule(self->accessibility);
    *out = self;
    self->dispatch_depth += 1;
    announce_fonts(self, fonts);
    dispatch_finished(self);
    return FOLIO_WINDOW_CREATED;
}

/* 名前入力など別のモーダルへ主窓のキーを漏らさない。compositionも各所有者へ確認する。 */
static bool command_target(const struct folio_window *_Nonnull window, HWND target)
{
    if (window->native_modal_depth > 0)
        return false;
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

static bool translate_menu_system_key(const struct folio_window *_Nonnull window,
                                      const MSG *_Nonnull message)
{
    if (window->command_surface == COMMAND_SURFACE_MENU && message->message == WM_SYSKEYDOWN &&
        (message->wParam == VK_MENU || message->wParam == VK_F10))
    {
        SendMessageW(window->command_layer, message->message, message->wParam, message->lParam);
        return true;
    }
    return false;
}

static bool translate_command_shortcut(const struct folio_window *_Nonnull window,
                                       const MSG *_Nonnull message)
{
    if (command_target(window, message->hwnd))
    {
        MSG translated = *message;
        if (TranslateAcceleratorW(window->handle, window->commands, &translated) != 0)
        {
            return true;
        }
    }
    return false;
}

bool folio_window_translate(const struct folio_window *_Nonnull window, const MSG *_Nonnull message)
{
    if (session_blocks_queued_input(window, message))
    {
        return true;
    }
    if (translate_menu_system_key(window, message))
        return true;
    if (translate_keyboard(window, message))
    {
        return true;
    }
    if (search_target(window, message->hwnd))
    {
        MSG stepping = *message;
        if (TranslateAcceleratorW(window->handle, window->search_keys, &stepping) != 0)
        {
            return true;
        }
    }
    if (translate_command_shortcut(window, message))
        return true;
    if (window->pane == nullptr || !command_target(window, message->hwnd))
    {
        return false;
    }
    return note_pane_translate(window->pane, message, folio_state_pane_mode(window->state));
}

static void release_accelerators(struct folio_window *_Nonnull window)
{
    HACCEL tables[] = {window->commands, window->search_keys, window->region_keys,
                       window->traversal_keys};
    for (size_t index = 0; index < sizeof tables / sizeof tables[0]; ++index)
    {
        if (tables[index] != nullptr)
        {
            DestroyAcceleratorTable(tables[index]);
        }
    }
}

void folio_window_destroy(struct folio_window *_Nullable window)
{
    if (window == nullptr)
    {
        return;
    }
    if (window->handle != nullptr)
    {
        clear_session_reason(window);
        DestroyWindow(window->handle);
    }
    release_accelerators(window);
    modal_owner_destroy(window->modal_owner);
    name_prompt_metrics_destroy(window->native_metrics);
    note_pane_destroy(window->pane);
    drawer_window_destroy(window->drawer);
    if (window->accessibility != nullptr)
    {
        accessibility_host_detach(window->accessibility);
        accessibility_host_release(window->accessibility);
    }
    command_candidates_destroy(window->command_candidates);
    theme_candidates_destroy(window->theme_candidates);
    if (window->mono_font != nullptr)
    {
        DeleteObject(window->mono_font);
    }
    if (window->gutter_font != nullptr)
        DeleteObject(window->gutter_font);
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

/* 補助面の表示参照。操作/選択は既存のcommand_*の正本へ渡す（ADR0058）。 */
static struct accessible_item surface_item(const struct folio_window *_Nonnull self,
                                           enum accessible_kind kind, size_t slot)
{
    return (struct accessible_item){.kind = kind,
                                    .slot = slot,
                                    .revision =
                                        kind == ACCESSIBLE_SURFACE ? 0 : self->surface_revision};
}

static enum ui_text surface_title(const struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return UI_TEXT_ACCESSIBLE_OPERATIONS;

    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
        return UI_TEXT_EMPTY;
    case COMMAND_SURFACE_EX:
        return UI_TEXT_ACCESSIBLE_COMMAND;
    case COMMAND_SURFACE_PALETTE:
        return UI_TEXT_COMMAND_HELP;
    case COMMAND_SURFACE_SEARCH:
        return UI_TEXT_COMMAND_FIND;
    case COMMAND_SURFACE_REPLACE:
        return UI_TEXT_COMMAND_REPLACE;
    case COMMAND_SURFACE_SETTINGS:
        return UI_TEXT_COMMAND_SETTINGS;
    case COMMAND_SURFACE_THEMES:
        return UI_TEXT_THEME_TITLE;
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
        return UI_TEXT_COMMAND_HISTORY;
    }
    return UI_TEXT_EMPTY;
}

static HRESULT surface_row_count(const struct folio_window *_Nonnull self, size_t *_Nonnull out)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        *out = folio_menu_action_count(self->menu.context);
        return S_OK;
    }

    *out = 0;
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        *out = settings_row_count();
        return S_OK;
    }
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        *out = folio_state_history_count(self->state);
        return S_OK;
    }
    if (command_keys(self))
    {
        *out = 0;
        return S_OK;
    }
    if (self->command_surface != COMMAND_SURFACE_PALETTE &&
        self->command_surface != COMMAND_SURFACE_THEMES)
    {
        return S_OK;
    }
    HRESULT result = self->command_surface == COMMAND_SURFACE_THEMES
                         ? prepare_theme_candidates(self)
                         : prepare_palette_candidates(self);
    if (result == S_OK)
        *out = self->command_surface == COMMAND_SURFACE_THEMES ? theme_matches_count(self)
                                                               : palette_matches_count(self);
    return result;
}

static bool surface_has_list(const struct folio_window *_Nonnull self)
{
    return (self->command_surface == COMMAND_SURFACE_PALETTE && !command_keys(self)) ||
           settings_surface(self) || self->command_surface == COMMAND_SURFACE_HISTORY ||
           self->command_surface == COMMAND_SURFACE_MENU;
}

static enum accessible_kind surface_row_kind(const struct folio_window *_Nonnull self, size_t at)
{
    if (self->command_surface != COMMAND_SURFACE_SETTINGS)
    {
        return ACCESSIBLE_SURFACE_CHOICE;
    }
    switch (settings_rows[at].kind)
    {
    case SETTINGS_ROW_HEADING:
        return ACCESSIBLE_SURFACE_TEXT;
    case SETTINGS_ROW_THEME:
        return ACCESSIBLE_SURFACE_BUTTON;
    case SETTINGS_ROW_LANGUAGE:
        return ACCESSIBLE_SURFACE_LANGUAGE;
    case SETTINGS_ROW_NUMBER:
        return ACCESSIBLE_SURFACE_TOGGLE;
    }
    return ACCESSIBLE_SURFACE_TEXT;
}

static bool surface_button_visible(const struct folio_window *_Nonnull self, size_t slot)
{
    if (slot == ACCESSIBLE_SURFACE_CLOSE)
        return settings_surface(self) || command_band(self) ||
               self->command_surface == COMMAND_SURFACE_HISTORY ||
               self->command_surface == COMMAND_SURFACE_PALETTE;
    if (slot == ACCESSIBLE_SURFACE_BACK)
        return self->command_surface == COMMAND_SURFACE_THEMES;
    if (slot == ACCESSIBLE_SURFACE_PREVIOUS || slot == ACCESSIBLE_SURFACE_NEXT)
        return self->command_surface == COMMAND_SURFACE_SEARCH || command_keys(self);
    if (slot == ACCESSIBLE_SURFACE_SHOW_HELP)
        return false;
    if (slot == ACCESSIBLE_SURFACE_REPLACE_ONE || slot == ACCESSIBLE_SURFACE_REPLACE_ALL)
        return self->command_surface == COMMAND_SURFACE_REPLACE;
    return false; /* theme button is a settings list child. */
}

static HRESULT surface_root_child(const struct folio_window *_Nonnull self, size_t index,
                                  struct accessible_item *_Nonnull out)
{
    const enum accessible_kind kinds[] = {ACCESSIBLE_SURFACE_TABS, ACCESSIBLE_SURFACE_LIST,
                                          ACCESSIBLE_SURFACE_STATUS};
    if (index < sizeof kinds / sizeof kinds[0])
    {
        bool visible[] = {self->command_surface == COMMAND_SURFACE_PALETTE, surface_has_list(self),
                          true};
        *out = surface_item(self, kinds[index], 0);
        return visible[index] ? S_OK : S_FALSE;
    }
    if (index < 3 + surface_button_count)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_BUTTON, index - 3);
        return surface_button_visible(self, index - 3) ? S_OK : S_FALSE;
    }
    size_t slot = index - 3 - surface_button_count;
    *out = surface_item(self, ACCESSIBLE_SURFACE_HELP, slot);
    return slot < (size_t)command_help_rows(self) ? S_OK : S_FALSE;
}

static bool command_group_start(const struct folio_window *_Nonnull self, size_t index)
{
    if (index == 0)
        return true;
    enum folio_command command = FOLIO_COMMAND_SAVE;
    enum folio_command previous = FOLIO_COMMAND_SAVE;
    return palette_command_at(self, index, &command) &&
           palette_command_at(self, index - 1, &previous) &&
           folio_command_scope(command) != folio_command_scope(previous);
}

static HRESULT surface_palette_child(const struct folio_window *_Nonnull self, size_t wanted,
                                     struct accessible_item *_Nonnull out)
{
    if (command_keys(self))
        return S_FALSE;
    HRESULT result = prepare_palette_candidates(self);
    if (FAILED(result))
        return result;
    enum command_candidate_kind kind = COMMAND_CANDIDATE_GROUP;
    size_t slot = 0;
    if (!command_candidates_child(self->command_candidates, wanted, &kind, &slot))
        return S_FALSE;
    switch (kind)
    {
    case COMMAND_CANDIDATE_GROUP:
        *out = surface_item(self, ACCESSIBLE_SURFACE_TEXT, slot);
        return S_OK;
    case COMMAND_CANDIDATE_COMMAND:
        *out = surface_item(self, ACCESSIBLE_SURFACE_CHOICE, slot);
        return S_OK;
    }
    return S_FALSE;
}

static HRESULT surface_list_child(const struct folio_window *_Nonnull self, size_t index,
                                  struct accessible_item *_Nonnull out)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
        return surface_palette_child(self, index, out);
    size_t count = 0;
    HRESULT result = surface_row_count(self, &count);
    if (FAILED(result))
        return result;
    if (index >= count)
        return S_FALSE;
    enum accessible_kind kind = surface_row_kind(self, index);
    size_t slot = kind == ACCESSIBLE_SURFACE_BUTTON ? ACCESSIBLE_SURFACE_THEME : index;
    *out = surface_item(self, kind, slot);
    return S_OK;
}

static HRESULT surface_child(const struct folio_window *_Nonnull self, enum accessible_kind parent,
                             size_t index, struct accessible_item *_Nonnull out)
{
    if (parent == ACCESSIBLE_SURFACE)
        return surface_root_child(self, index, out);
    if (parent == ACCESSIBLE_SURFACE_TABS)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_TAB, index);
        return self->command_surface == COMMAND_SURFACE_PALETTE && index < 2 ? S_OK : S_FALSE;
    }
    if (parent != ACCESSIBLE_SURFACE_LIST || !surface_has_list(self))
        return S_FALSE;
    return surface_list_child(self, index, out);
}

static enum accessible_kind surface_parent(struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        return ACCESSIBLE_SURFACE_TABS;
    if (item.kind == ACCESSIBLE_SURFACE_CHOICE || item.kind == ACCESSIBLE_SURFACE_LANGUAGE ||
        item.kind == ACCESSIBLE_SURFACE_TOGGLE || item.kind == ACCESSIBLE_SURFACE_TEXT ||
        (item.kind == ACCESSIBLE_SURFACE_BUTTON && item.slot == ACCESSIBLE_SURFACE_THEME))
        return ACCESSIBLE_SURFACE_LIST;
    return ACCESSIBLE_SURFACE;
}

static size_t surface_selection_ordinal(struct accessible_item item)
{
    if (surface_parent(item) == ACCESSIBLE_SURFACE_LIST)
        return item.kind == ACCESSIBLE_SURFACE_BUTTON ? settings_first_choice_row : item.slot;
    if (item.kind == ACCESSIBLE_SURFACE_LIST)
        return 0;
    if (item.kind == ACCESSIBLE_SURFACE_STATUS)
        return 1;
    return item.slot + 2 + (item.kind == ACCESSIBLE_SURFACE_HELP ? surface_button_count : 0);
}

static size_t surface_palette_ordinal(const struct folio_window *_Nonnull self,
                                      struct accessible_item item)
{
    if (prepare_palette_candidates(self) != S_OK)
        return SIZE_MAX;
    size_t ordinal = SIZE_MAX;
    if (item.kind == ACCESSIBLE_SURFACE_TEXT)
        (void)command_candidates_ordinal(self->command_candidates, COMMAND_CANDIDATE_GROUP,
                                         item.slot, &ordinal);
    if (item.kind == ACCESSIBLE_SURFACE_CHOICE)
        (void)command_candidates_ordinal(self->command_candidates, COMMAND_CANDIDATE_COMMAND,
                                         item.slot, &ordinal);
    return ordinal;
}

static size_t surface_navigation_ordinal(const struct folio_window *_Nonnull self,
                                         struct accessible_item item)
{
    if (surface_parent(item) == ACCESSIBLE_SURFACE_TABS)
        return item.slot;
    if (item.kind == ACCESSIBLE_SURFACE_TABS)
        return 0;
    if (surface_parent(item) == ACCESSIBLE_SURFACE_LIST)
    {
        if (self->command_surface != COMMAND_SURFACE_PALETTE)
            return surface_selection_ordinal(item);
        return surface_palette_ordinal(self, item);
    }
    if (item.kind == ACCESSIBLE_SURFACE_LIST)
        return 1;
    if (item.kind == ACCESSIBLE_SURFACE_STATUS)
        return 2;
    return item.slot + 3 + (item.kind == ACCESSIBLE_SURFACE_HELP ? surface_button_count : 0);
}

static HRESULT surface_valid(const struct folio_window *_Nonnull self, struct accessible_item item)
{
    if (self->command_layer == nullptr)
        return accessibility_node_unavailable;
    if (item.kind == ACCESSIBLE_SURFACE)
        return S_OK;
    if (self->command_surface == COMMAND_SURFACE_CLOSED ||
        item.revision != self->surface_revision || item.revision == UINT64_MAX)
        return accessibility_node_unavailable;
    struct accessible_item child;
    HRESULT result =
        surface_child(self, surface_parent(item), surface_navigation_ordinal(self, item), &child);
    if (FAILED(result))
        return result;
    return result == S_OK && accessible_item_equal(item, child) ? S_OK
                                                                : accessibility_node_unavailable;
}

static HRESULT surface_palette_child_count(const struct folio_window *_Nonnull self,
                                           size_t *_Nonnull out)
{
    *out = 0;
    if (command_keys(self))
        return S_OK;
    HRESULT result = prepare_palette_candidates(self);
    if (result == S_OK)
        *out = command_candidates_child_count(self->command_candidates);
    return result;
}

static HRESULT surface_child_count(const struct folio_window *_Nonnull self,
                                   enum accessible_kind parent, size_t *_Nonnull out)
{
    if (parent == ACCESSIBLE_SURFACE_TABS)
    {
        *out = self->command_surface == COMMAND_SURFACE_PALETTE ? 2 : 0;
        return S_OK;
    }
    if (parent == ACCESSIBLE_SURFACE_LIST)
    {
        if (self->command_surface != COMMAND_SURFACE_PALETTE)
            return surface_row_count(self, out);
        return surface_palette_child_count(self, out);
    }
    *out = parent == ACCESSIBLE_SURFACE ? 3 + surface_button_count + (size_t)command_help_rows(self)
                                        : 0;
    return S_OK;
}

static bool surface_edge(enum accessible_navigation direction)
{
    return direction == ACCESSIBLE_NAVIGATION_FIRST || direction == ACCESSIBLE_NAVIGATION_LAST;
}

static int surface_navigation_start(const struct folio_window *_Nonnull self,
                                    struct accessible_item item,
                                    enum accessible_navigation direction, size_t count)
{
    switch (direction)
    {
    case ACCESSIBLE_NAVIGATION_PARENT:
        return -1;
    case ACCESSIBLE_NAVIGATION_FIRST:
        return 0;
    case ACCESSIBLE_NAVIGATION_LAST:
        return (int)count - 1;
    case ACCESSIBLE_NAVIGATION_NEXT:
        return (int)surface_navigation_ordinal(self, item) + 1;
    case ACCESSIBLE_NAVIGATION_PREVIOUS:
        return (int)surface_navigation_ordinal(self, item) - 1;
    }
    return -1;
}

static HRESULT surface_move(const struct folio_window *_Nonnull self,
                            const struct accessible_item *_Nonnull item,
                            enum accessible_navigation direction,
                            struct accessible_item *_Nonnull out)
{
    enum accessible_kind parent = surface_edge(direction) ? item->kind : surface_parent(*item);
    size_t count = 0;
    HRESULT result = surface_child_count(self, parent, &count);
    if (FAILED(result))
        return result;
    bool forward =
        direction == ACCESSIBLE_NAVIGATION_FIRST || direction == ACCESSIBLE_NAVIGATION_NEXT;
    int step = forward ? 1 : -1;
    for (int at = surface_navigation_start(self, *item, direction, count);
         at >= 0 && (size_t)at < count; at += step)
    {
        result = surface_child(self, parent, (size_t)at, out);
        if (result != S_FALSE)
            return result;
    }
    return S_FALSE;
}

static HRESULT surface_navigate(const struct folio_window *_Nonnull self,
                                const struct accessible_item *_Nonnull item,
                                enum accessible_navigation direction,
                                struct accessible_item *_Nonnull out)
{
    HRESULT result = surface_valid(self, *item);
    if (FAILED(result))
        return result;
    if (self->command_surface == COMMAND_SURFACE_CLOSED)
        return S_FALSE;
    if (!surface_edge(direction) && item->kind == ACCESSIBLE_SURFACE)
        return S_FALSE;
    if (direction == ACCESSIBLE_NAVIGATION_PARENT)
    {
        *out = surface_item(self, surface_parent(*item), 0);
        return S_OK;
    }
    return surface_move(self, item, direction, out);
}

static HRESULT surface_history_name(const struct folio_window *_Nonnull self, size_t slot,
                                    char16_t *_Nonnull out)
{
    struct history_row_view row = {
        .version = 0, .first_line = "", .first_line_length = 0, .readable = false};
    if (!folio_state_history_row(self->state, slot, &row))
        return accessibility_node_unavailable;
    char line[history_line_capacity];
    history_row_text(self, &row, line);
    (void)wide_units(line, out);
    return S_OK;
}

static bool surface_query_name(const struct folio_window *_Nonnull self, size_t slot,
                               struct accessible_view *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        enum folio_theme_choice choice = FOLIO_THEME_CHOICE_SYSTEM;
        if (!theme_candidate_at(self, slot, &choice))
            return false;
        (void)wide_units(folio_theme_choice_label(choice, language), out->name);
        if (choice == folio_state_theme_choice(self->state))
            (void)wide_units(ui_text_line(UI_TEXT_ACCESSIBLE_CURRENT, language), out->help);
        return true;
    }
    enum folio_command command = FOLIO_COMMAND_HELP;
    if (!palette_command_at(self, slot, &command))
        return false;
    char caption[draw_unit_limit];
    if (folio_command_caption(command, language, caption, sizeof caption) == UI_TEXT_FORMAT_READY)
        (void)wide_units(caption, out->name);
    return true;
}

static HRESULT surface_choice_name(const struct folio_window *_Nonnull self, size_t slot,
                                   struct accessible_view *_Nonnull out)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        menu_accessible(self, slot, out);
        return S_OK;
    }

    if (self->command_surface == COMMAND_SURFACE_HISTORY)
        return surface_history_name(self, slot, out->name);
    HRESULT result = self->command_surface == COMMAND_SURFACE_THEMES
                         ? prepare_theme_candidates(self)
                         : prepare_palette_candidates(self);
    if (FAILED(result))
        return result;
    return surface_query_name(self, slot, out) ? S_OK : accessibility_node_unavailable;
}

static enum ui_text surface_button_label(const struct folio_window *_Nonnull self, size_t slot)
{
    if (slot == ACCESSIBLE_SURFACE_PREVIOUS)
        return self->command_surface == COMMAND_SURFACE_SEARCH ? UI_TEXT_ACTION_SEARCH_PREVIOUS
                                                               : UI_TEXT_ACTION_PALETTE_PREVIOUS;
    if (slot == ACCESSIBLE_SURFACE_NEXT)
        return self->command_surface == COMMAND_SURFACE_SEARCH ? UI_TEXT_ACTION_SEARCH_NEXT
                                                               : UI_TEXT_ACTION_PALETTE_NEXT;
    if (slot == ACCESSIBLE_SURFACE_SHOW_HELP)
        return self->command_keys_visible ? UI_TEXT_ACTION_KEYS_HIDE : UI_TEXT_ACTION_KEYS_SHOW;
    const enum ui_text labels[] = {UI_TEXT_TOOLTIP_CLOSE,
                                   UI_TEXT_THEME_BACK,
                                   UI_TEXT_EMPTY,
                                   UI_TEXT_EMPTY,
                                   UI_TEXT_EMPTY,
                                   UI_TEXT_ACTION_REPLACE_ONE,
                                   UI_TEXT_ACTION_REPLACE_ALL,
                                   UI_TEXT_THEME_CHOOSE};
    return labels[slot];
}

static void surface_empty_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    if (!surface_has_list(self))
        return;
    size_t count = 0;
    if (SUCCEEDED(surface_row_count(self, &count)) && count == 0)
        (void)strcpy_s(
            out, replace_status_capacity,
            ui_text_line(UI_TEXT_ACCESSIBLE_NO_RESULTS, folio_state_language(self->state)));
}

static void surface_status_text(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    out[0] = '\0';
    if (self->command_failure != FOLIO_STATE_READY)
    {
        failure_status(self, self->command_failure, out);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        search_status(self, out);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_REPLACE || self->command_no_match)
    {
        replace_line(self, out);
        return;
    }
    if (self->command_unknown)
    {
        (void)strcpy_s(
            out, replace_status_capacity,
            ui_text_line(UI_TEXT_STATUS_COMMAND_NOT_FOUND, folio_state_language(self->state)));
        return;
    }
    if (settings_surface(self) && settings_has_notice(self))
    {
        failure_status(self, settings_notice(self), out);
        return;
    }
    surface_empty_status(self, out);
}

static void surface_help_name(const struct folio_window *_Nonnull self, size_t slot,
                              char16_t *_Nonnull out)
{
    char text[draw_unit_limit];
    command_help_text(self, slot, text);
    (void)wide_units(text, out);
}

static enum ui_text surface_control_label(const struct folio_window *_Nonnull self,
                                          struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        return item.slot == 0 ? UI_TEXT_HELP_TAB_COMMANDS : UI_TEXT_HELP_TAB_KEYS;
    if (item.kind == ACCESSIBLE_SURFACE_TEXT && self->command_surface == COMMAND_SURFACE_PALETTE)
        return command_candidates_group_label(item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON)
        return surface_button_label(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_LANGUAGE || item.kind == ACCESSIBLE_SURFACE_TOGGLE ||
        item.kind == ACCESSIBLE_SURFACE_TEXT)
        return settings_rows[item.slot].label;
    return surface_title(self);
}

static HRESULT surface_name(const struct folio_window *_Nonnull self, struct accessible_item item,
                            struct accessible_view *_Nonnull out)
{
    if (item.kind == ACCESSIBLE_SURFACE_CHOICE)
        return surface_choice_name(self, item.slot, out);
    if (item.kind == ACCESSIBLE_SURFACE_STATUS)
    {
        char text[replace_status_capacity];
        surface_status_text(self, text);
        (void)wide_units(text, out->name);
        return S_OK;
    }
    if (item.kind == ACCESSIBLE_SURFACE_HELP)
    {
        surface_help_name(self, item.slot, out->name);
        return S_OK;
    }
    enum folio_language language = folio_state_language(self->state);
    (void)wide_units(ui_text_line(surface_control_label(self, item), language), out->name);
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON && item.slot == ACCESSIBLE_SURFACE_THEME)
        (void)wide_units(folio_theme_choice_label(folio_state_theme_choice(self->state), language),
                         out->help);
    return S_OK;
}

static RECT surface_step_rect(const struct folio_window *_Nonnull self, bool previous)
{
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
        return search_button_rect(self, previous ? 0 : 1);
    return previous ? command_previous_rect(self) : command_next_rect(self);
}

static RECT accessible_surface_button_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    switch ((enum accessible_surface_button)slot)
    {
    case ACCESSIBLE_SURFACE_CLOSE:
        return command_band(self) ? search_close_rect(self) : settings_close_rect(self);
    case ACCESSIBLE_SURFACE_BACK:
        return theme_back_rect(self);
    case ACCESSIBLE_SURFACE_THEME:
    {
        RECT row = settings_item_rect(self, settings_first_choice_row);
        RECT content = settings_content_rect(self);
        RECT visible = {0};
        IntersectRect(&visible, &row, &content);
        return visible;
    }
    case ACCESSIBLE_SURFACE_PREVIOUS:
        return surface_step_rect(self, true);
    case ACCESSIBLE_SURFACE_NEXT:
        return surface_step_rect(self, false);
    case ACCESSIBLE_SURFACE_SHOW_HELP:
    {
        RECT bounds = command_toggle_rect(self);
        bounds.right = command_pages_rect(self).left;
        return bounds;
    }
    case ACCESSIBLE_SURFACE_REPLACE_ONE:
        return surface_button_rect(self, 0);
    case ACCESSIBLE_SURFACE_REPLACE_ALL:
        return surface_button_rect(self, 1);
    }
    return (RECT){0};
}

static RECT surface_palette_heading(const struct folio_window *_Nonnull self, size_t group)
{
    for (size_t index = self->command_first;; ++index)
    {
        enum folio_command command = FOLIO_COMMAND_SAVE;
        RECT row = palette_candidate_rect(self, self->command_first, index);
        if (IsRectEmpty(&row) || !palette_command_at(self, index, &command))
            return (RECT){0};
        if (command_candidates_group_index(command) != group)
            continue;
        if (index == self->command_first || command_group_start(self, index))
        {
            OffsetRect(&row, 0, row.top - row.bottom);
            return row;
        }
    }
}

static RECT surface_palette_rect(const struct folio_window *_Nonnull self, size_t slot,
                                 bool heading)
{
    if (prepare_palette_candidates(self) != S_OK)
        return (RECT){0};
    RECT row = {0};
    if (heading)
        row = surface_palette_heading(self, slot);
    if (!heading && slot >= self->command_first)
        row = palette_candidate_rect(self, self->command_first, slot);
    return row;
}

static RECT surface_row_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
        return command_keys(self) ? (RECT){0} : surface_palette_rect(self, slot, false);
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return menu_row_rect(self, slot);

    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        RECT row = settings_item_rect(self, slot);
        RECT content = settings_content_rect(self);
        RECT visible = {0};
        IntersectRect(&visible, &row, &content);
        return visible;
    }
    if (slot < self->command_first || slot - self->command_first >= command_visible_rows(self))
        return (RECT){0};
    RECT bounds = command_rows_rect(self);
    int height = scale(self->command_surface == COMMAND_SURFACE_THEMES ? base_theme_row_height
                                                                       : base_command_row_height,
                       GetDpiForWindow(self->handle));
    bounds.top += (int)(slot - self->command_first) * height;
    bounds.bottom = bounds.top + height;
    return bounds;
}

static RECT surface_help_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    if (!command_keys(self) || slot >= (size_t)command_help_rows(self))
        return (RECT){0};
    RECT row = command_help_geometry(self, slot, nullptr);
    OffsetRect(&row, 0, -(int)command_help_offset(self));
    RECT viewport = command_body_rect(self);
    RECT visible = {0};
    IntersectRect(&visible, &row, &viewport);
    return visible;
}

static RECT surface_status_rect(const struct folio_window *_Nonnull self)
{
    if (command_band(self))
        return self->band.result;
    if (self->command_surface == COMMAND_SURFACE_THEMES)
        return theme_footer_rect(self);
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
        return settings_footer_rect(self);
    RECT bounds = {0};
    GetClientRect(self->command_layer, &bounds);
    int height = scale(base_command_status_height, GetDpiForWindow(self->handle));
    bounds.top = bounds.bottom - height;
    return bounds;
}

static RECT surface_list_bounds(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_SETTINGS ? settings_content_rect(self)
                                                             : command_rows_rect(self);
}

static RECT surface_bounds(const struct folio_window *_Nonnull self, struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        return command_tab_rect(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_TABS)
    {
        RECT bounds = command_tab_rect(self, 0);
        bounds.right = command_tab_rect(self, 1).right;
        return bounds;
    }
    if (item.kind == ACCESSIBLE_SURFACE_TEXT && self->command_surface == COMMAND_SURFACE_PALETTE)
        return surface_palette_rect(self, item.slot, true);
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON)
        return accessible_surface_button_rect(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_HELP)
        return surface_help_rect(self, item.slot);
    if (surface_parent(item) == ACCESSIBLE_SURFACE_LIST)
        return surface_row_rect(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_LIST)
        return surface_list_bounds(self);
    if (item.kind == ACCESSIBLE_SURFACE_STATUS)
        return surface_status_rect(self);
    RECT bounds = {0};
    GetClientRect(self->command_layer, &bounds);
    return bounds;
}

static bool surface_row_focusable(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_SURFACE_CHOICE || kind == ACCESSIBLE_SURFACE_LANGUAGE ||
           kind == ACCESSIBLE_SURFACE_TOGGLE;
}

static HRESULT surface_list_view(const struct folio_window *_Nonnull self,
                                 struct accessible_view *_Nonnull out)
{
    size_t count = 0;
    HRESULT result = surface_row_count(self, &count);
    out->selection_required = count != 0;
    out->scroll = (int)self->command_first;
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        RECT content = settings_content_rect(self);
        out->viewport = content.bottom - content.top;
        out->scroll_limit = (int)settings_scroll_limit(self);
        return result;
    }
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        out->viewport = (int)menu_visible_count(self);
        out->scroll_limit = (int)menu_scroll_limit(self);
        return result;
    }
    out->viewport = (int)command_visible_rows(self);
    out->scroll_limit = self->command_surface == COMMAND_SURFACE_PALETTE
                            ? (int)palette_scroll_limit(self, count)
                            : ((int)count > out->viewport ? (int)count - out->viewport : 0);
    return result;
}

static void surface_selection_view(const struct folio_window *_Nonnull self,
                                   struct accessible_item item,
                                   struct accessible_view *_Nonnull out)
{
    out->selection_required = item.kind == ACCESSIBLE_SURFACE_TABS;
    out->selected = item.kind == ACCESSIBLE_SURFACE_CHOICE && item.slot == self->command_selection;
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        out->selected = self->command_keys_visible == (item.slot == 1);
    if (item.kind == ACCESSIBLE_SURFACE_LANGUAGE || item.kind == ACCESSIBLE_SURFACE_TOGGLE)
        out->selected = settings_row_current(self, item.slot);
}

static void surface_control_view(const struct folio_window *_Nonnull self,
                                 struct accessible_item item, struct accessible_view *_Nonnull out)
{
    out->enabled =
        folio_window_accessible_ready(self) && self->command_surface != COMMAND_SURFACE_CLOSED;
    out->dialog = item.kind == ACCESSIBLE_SURFACE && settings_surface(self);
    out->live = item.kind == ACCESSIBLE_SURFACE_STATUS;
    if (item.kind == ACCESSIBLE_SURFACE && command_keys(self))
    {
        out->focusable = true;
        out->focused = GetFocus() == self->command_layer;
        return;
    }
    surface_selection_view(self, item, out);
    out->focusable = surface_row_focusable(item.kind) || (item.kind == ACCESSIBLE_SURFACE_BUTTON &&
                                                          item.slot == ACCESSIBLE_SURFACE_THEME);
    out->focused = out->focusable && GetFocus() == self->command_layer &&
                   self->command_selection == surface_selection_ordinal(item);
}

static void surface_menu_view(const struct folio_window *_Nonnull self, struct accessible_item item,
                              struct accessible_view *_Nonnull out)
{
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        out->menu = true;
        if (item.kind == ACCESSIBLE_SURFACE_CHOICE)
        {
            out->enabled = out->enabled && menu_availability(self, item.slot) == FOLIO_STATE_READY;
            out->focusable = out->enabled;
        }
    }
}

static HRESULT surface_read(struct folio_window *_Nonnull self, struct accessible_item item,
                            struct accessible_view *_Nonnull out)
{
    HRESULT result = surface_valid(self, item);
    if (FAILED(result))
        return result;
    result = surface_name(self, item, out);
    if (FAILED(result))
        return result;
    out->bounds = surface_bounds(self, item);
    out->offscreen = IsRectEmpty(&out->bounds) || self->command_surface == COMMAND_SURFACE_CLOSED ||
                     !IsWindowVisible(self->command_layer) || IsIconic(self->handle);
    if (item.kind == ACCESSIBLE_SURFACE_STATUS)
        out->offscreen |= out->name[0] == u'\0';
    if (out->offscreen)
        out->bounds = (RECT){0};
    else
        MapWindowPoints(self->command_layer, nullptr, (POINT *)&out->bounds, 2);
    surface_control_view(self, item, out);
    surface_menu_view(self, item, out);
    return item.kind == ACCESSIBLE_SURFACE_LIST ? surface_list_view(self, out) : S_OK;
}

static HRESULT surface_selected_row(const struct folio_window *_Nonnull self,
                                    struct accessible_item *_Nonnull out)
{
    size_t at = self->command_selection;
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_CHOICE, at);
        return surface_valid(self, *out) == S_OK ? S_OK : S_FALSE;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        for (size_t index = 0; index < settings_row_count(); ++index)
        {
            if (settings_rows[index].kind == SETTINGS_ROW_LANGUAGE &&
                settings_row_current(self, index))
                at = index;
        }
    }
    return surface_child(self, ACCESSIBLE_SURFACE_LIST, at, out);
}

static HRESULT surface_selected(const struct folio_window *_Nonnull self,
                                const struct accessible_item *_Nonnull item,
                                struct accessible_item *_Nonnull out)
{
    HRESULT result = surface_valid(self, *item);
    if (FAILED(result))
        return result;
    if (item->kind == ACCESSIBLE_SURFACE_TABS)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_TAB, (size_t)self->command_keys_visible);
        return S_OK;
    }
    if (item->kind != ACCESSIBLE_SURFACE_LIST || !surface_has_list(self))
        return S_FALSE;
    return surface_selected_row(self, out);
}

static HRESULT surface_focus(const struct folio_window *_Nonnull self,
                             struct accessible_item *_Nonnull out)
{
    if (GetFocus() != self->command_layer || !folio_window_accessible_ready(self) ||
        self->command_surface == COMMAND_SURFACE_CLOSED)
        return S_FALSE;
    if (command_keys(self))
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE, 0);
        return S_OK;
    }
    if (!surface_has_list(self))
        return S_FALSE;
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_CHOICE, self->command_selection);
        return surface_valid(self, *out) == S_OK ? S_OK : S_FALSE;
    }
    return surface_child(self, ACCESSIBLE_SURFACE_LIST, self->command_selection, out);
}

static HRESULT surface_child_at(struct folio_window *_Nonnull self, POINT point,
                                struct accessible_item *_Nonnull parent)
{
    struct accessible_item child;
    HRESULT result = surface_navigate(self, parent, ACCESSIBLE_NAVIGATION_FIRST, &child);
    while (result == S_OK)
    {
        struct accessible_view view = {0};
        result = surface_read(self, child, &view);
        if (FAILED(result))
            return result;
        if (!view.offscreen && PtInRect(&view.bounds, point))
        {
            *parent = child;
            return S_OK;
        }
        result = surface_navigate(self, &child, ACCESSIBLE_NAVIGATION_NEXT, &child);
    }
    return result;
}

static HRESULT surface_at(struct folio_window *_Nonnull self, POINT point,
                          struct accessible_item *_Nonnull out)
{
    struct accessible_item item = surface_item(self, ACCESSIBLE_SURFACE, 0);
    struct accessible_view view = {0};
    HRESULT result = surface_read(self, item, &view);
    if (FAILED(result))
        return result;
    if (view.offscreen || !PtInRect(&view.bounds, point))
        return S_FALSE;
    *out = item;
    for (int depth = 0; depth < 2; ++depth)
    {
        result = surface_child_at(self, point, out);
        if (FAILED(result))
            return result;
        if (result == S_FALSE)
            break;
    }
    return S_OK;
}

static void surface_step(struct folio_window *_Nonnull self, bool previous)
{
    if (command_keys(self))
    {
        move_command_help(self, previous ? VK_PRIOR : VK_NEXT);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
        step_search(self, previous ? SEARCH_DIRECTION_BACKWARD : SEARCH_DIRECTION_FORWARD);
    else
        move_command_selection(self, previous ? VK_UP : VK_DOWN);
}

static HRESULT surface_button_action(struct folio_window *_Nonnull self, size_t slot)
{
    switch ((enum accessible_surface_button)slot)
    {
    case ACCESSIBLE_SURFACE_CLOSE:
        close_command_surface(self);
        break;
    case ACCESSIBLE_SURFACE_BACK:
        show_settings_surface(self, "");
        break;
    case ACCESSIBLE_SURFACE_THEME:
        show_theme_surface(self);
        break;
    case ACCESSIBLE_SURFACE_SHOW_HELP:
        toggle_command_keys(self);
        break;
    case ACCESSIBLE_SURFACE_REPLACE_ONE:
        apply_replace(self, REPLACE_ONE);
        break;
    case ACCESSIBLE_SURFACE_REPLACE_ALL:
        apply_replace(self, REPLACE_ALL);
        break;
    case ACCESSIBLE_SURFACE_PREVIOUS:
        surface_step(self, true);
        break;
    case ACCESSIBLE_SURFACE_NEXT:
        surface_step(self, false);
        break;
    }
    return S_OK;
}

static void surface_reveal(struct folio_window *_Nonnull self, struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_SURFACE_HELP)
    {
        RECT row = command_help_geometry(self, item.slot, nullptr);
        RECT area = command_body_rect(self);
        self->command_help_scroll = (size_t)(row.top - area.top);
        self->command_help_scroll = command_help_offset(self);
        redraw_command_layer(self);
        return;
    }
    size_t selected = self->command_selection;
    self->command_selection = surface_selection_ordinal(item);
    reveal_command_selection(self);
    self->command_selection = selected;
    redraw_command_layer(self);
}

static HRESULT focus_surface_row(struct folio_window *_Nonnull self)
{
    SetFocus(self->command_layer);
    redraw_command_layer(self);
    return GetFocus() == self->command_layer ? S_OK : accessibility_node_invalid;
}

static HRESULT surface_row_action(struct folio_window *_Nonnull self,
                                  const struct accessible_request *_Nonnull request)
{
    if (self->command_surface == COMMAND_SURFACE_MENU &&
        menu_availability(self, request->item.slot) != FOLIO_STATE_READY)
        return accessibility_node_disabled;

    struct accessible_item item = request->item;
    self->command_selection = surface_selection_ordinal(item);
    reveal_command_selection(self);
    if (self->command_surface == COMMAND_SURFACE_THEMES &&
        request->action != ACCESSIBLE_ACTION_INVOKE)
        preview_theme_selection(self);
    if (request->action == ACCESSIBLE_ACTION_FOCUS)
        return focus_surface_row(self);
    if (item.kind == ACCESSIBLE_SURFACE_LANGUAGE || item.kind == ACCESSIBLE_SURFACE_TOGGLE)
        adopt_settings_row(self);
    if (item.kind == ACCESSIBLE_SURFACE_CHOICE && request->action == ACCESSIBLE_ACTION_INVOKE)
        (void)command_return(self);
    redraw_command_layer(self);
    return S_OK;
}

static HRESULT surface_tab_action(struct folio_window *_Nonnull self,
                                  const struct accessible_request *_Nonnull request)
{
    if (request->action != ACCESSIBLE_ACTION_SELECT)
        return accessibility_node_invalid;
    if (self->command_keys_visible != (request->item.slot == 1))
        toggle_command_keys(self);
    return S_OK;
}

static HRESULT surface_root_action(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request)
{
    if (request->action != ACCESSIBLE_ACTION_FOCUS || !command_keys(self))
        return accessibility_node_invalid;
    focus_command_input(self);
    return GetFocus() == self->command_layer ? S_OK : accessibility_node_invalid;
}

static HRESULT surface_action(struct folio_window *_Nonnull self,
                              const struct accessible_request *_Nonnull request)
{
    struct accessible_item item = request->item;
    if (item.kind == ACCESSIBLE_SURFACE)
        return surface_root_action(self, request);
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        return surface_tab_action(self, request);
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON && request->action == ACCESSIBLE_ACTION_INVOKE)
        return surface_button_action(self, item.slot);
    if (request->action == ACCESSIBLE_ACTION_SCROLL && item.kind == ACCESSIBLE_SURFACE_LIST)
    {
        self->command_first = (size_t)request->scroll_target;
        redraw_command_layer(self);
        return S_OK;
    }
    if (request->action == ACCESSIBLE_ACTION_REVEAL)
    {
        surface_reveal(self, item);
        return S_OK;
    }
    bool row = surface_row_focusable(item.kind) ||
               (item.kind == ACCESSIBLE_SURFACE_BUTTON && item.slot == ACCESSIBLE_SURFACE_THEME);
    if (!row)
        return accessibility_node_invalid;
    return surface_row_action(self, request);
}

static bool accessible_pane_control(enum accessible_kind kind)
{
    enum accessible_kind parent = accessible_controls[kind].parent;
    return kind == ACCESSIBLE_PANE_TOOLS || parent == ACCESSIBLE_PANE_TOOLS ||
           parent == ACCESSIBLE_MODE_GROUP || parent == ACCESSIBLE_ACTION_GROUP ||
           kind == ACCESSIBLE_BODY;
}

/* 空状態では操作帯・閲覧/編集・本文を描かず、hit と UIA にも出さない（ADR0060 の決定 13）。 */
static bool pane_control_visible(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_BREADCRUMB && !folio_state_pane_title(self->state).any)
    {
        return false;
    }
    return body_shown(self);
}

static bool notice_visible(const struct folio_window *_Nonnull self)
{
    return (pane_visible(self) || self->notice == PANE_NOTICE_COMMAND_RESULT) &&
           pane_notice_line(self) != UI_TEXT_EMPTY;
}

/* 同じheader表示slotに属する静的参照。可視判定は単一geometryに任せる。 */
static bool layout_control(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_LAYOUT_CHOOSER || kind == ACCESSIBLE_LAYOUT_GROUP ||
           accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP;
}

static bool accessible_visible(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (layout_control(kind))
        return layout_metrics_for(self).chooser == (kind == ACCESSIBLE_LAYOUT_CHOOSER);
    if (kind == ACCESSIBLE_NOTICE)
        return notice_visible(self);
    if (kind == ACCESSIBLE_COMMAND_INPUT)
        return IsWindowVisible(self->command_input);
    if (kind == ACCESSIBLE_REPLACE_PATTERN)
        return IsWindowVisible(self->replace_inputs[0]);
    if (kind == ACCESSIBLE_REPLACE_TEXT)
        return IsWindowVisible(self->replace_inputs[1]);
    struct accessible_item item = {.kind = kind};
    if (accessible_item_host(item) == ACCESSIBLE_DRAWER || kind == ACCESSIBLE_FILTER)
    {
        return drawer_visible(self);
    }
    if (kind == ACCESSIBLE_GUIDE)
    {
        return pane_visible(self) && !document_open(self);
    }
    return !accessible_pane_control(kind) || pane_control_visible(self, kind);
}

static RECT accessible_main_group_rect(const struct folio_window *_Nonnull self,
                                       enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_PANE_TOOLS)
    {
        RECT bounds = caption_rect(self);
        bounds.bottom = action_button_rect(self, 0).bottom;
        return bounds;
    }
    if (kind == ACCESSIBLE_MODE_GROUP)
    {
        return mode_group_rect(self);
    }
    if (kind == ACCESSIBLE_ACTION_GROUP)
    {
        return action_group_rect(self);
    }
    RECT bounds = {0};
    GetClientRect(self->handle, &bounds);
    return bounds;
}

/* 右ペインの自前要素の矩形。本文操作帯の添字は action_parts と同じ。 */
static RECT accessible_pane_rect(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_BREADCRUMB)
    {
        return breadcrumb_rect(self);
    }
    if (kind == ACCESSIBLE_GUIDE)
    {
        return guide_rect(self);
    }
    if (kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT)
    {
        return mode_item_rect(self, kind == ACCESSIBLE_EDIT ? PANE_MODE_EDIT : PANE_MODE_VIEW);
    }
    for (size_t index = 0; index < action_count; ++index)
    {
        if (action_parts[index].kind == kind)
        {
            return action_button_rect(self, index);
        }
    }
    return (RECT){0};
}

static RECT accessible_header_rect(const struct folio_window *_Nonnull self,
                                   enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_HEADER)
        return regions_for(self).header;
    const struct
    {
        enum accessible_kind kind;
        RECT (*_Nonnull bounds)(const struct folio_window *_Nonnull);
    } controls[] = {{ACCESSIBLE_LAYOUT_GROUP, layout_group_rect},
                    {ACCESSIBLE_LAYOUT_CHOOSER, layout_chooser_rect},
                    {ACCESSIBLE_MINIMIZE, minimize_rect},
                    {ACCESSIBLE_MAXIMIZE, maximize_rect},
                    {ACCESSIBLE_SETTINGS, settings_rect},
                    {ACCESSIBLE_CLOSE, close_rect},
                    {ACCESSIBLE_HELP, help_rect},
                    {ACCESSIBLE_NEW_NOTE, new_note_rect}};
    for (size_t at = 0; at < sizeof controls / sizeof controls[0]; ++at)
    {
        if (controls[at].kind == kind)
            return controls[at].bounds(self);
    }
    for (size_t at = 0; at < layout_button_count(); ++at)
    {
        if (layout_buttons[at].kind == kind)
            return layout_button_rect(self, at);
    }
    return (RECT){0};
}

static RECT accessible_main_rect(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind)
{
    switch (kind)
    {
    case ACCESSIBLE_HEADER:
    case ACCESSIBLE_LAYOUT_GROUP:
    case ACCESSIBLE_LAYOUT_DRAWER:
    case ACCESSIBLE_LAYOUT_BOTH:
    case ACCESSIBLE_LAYOUT_NOTE:
    case ACCESSIBLE_LAYOUT_CHOOSER:
    case ACCESSIBLE_MINIMIZE:
    case ACCESSIBLE_MAXIMIZE:
    case ACCESSIBLE_SETTINGS:
    case ACCESSIBLE_CLOSE:
    case ACCESSIBLE_HELP:
    case ACCESSIBLE_NEW_NOTE:
        return accessible_header_rect(self, kind);
    case ACCESSIBLE_PANE_TOOLS:
    case ACCESSIBLE_MODE_GROUP:
    case ACCESSIBLE_ACTION_GROUP:
    case ACCESSIBLE_WINDOW:
        return accessible_main_group_rect(self, kind);

    case ACCESSIBLE_BREADCRUMB:
    case ACCESSIBLE_GUIDE:
    case ACCESSIBLE_VIEW:
    case ACCESSIBLE_EDIT:
    case ACCESSIBLE_SAVE:
    case ACCESSIBLE_FIND:
    case ACCESSIBLE_MENU:
        return accessible_pane_rect(self, kind);
    case ACCESSIBLE_DRAWER:
    case ACCESSIBLE_NEW_CATEGORY:
    case ACCESSIBLE_TREE:
    case ACCESSIBLE_CATEGORY:
    case ACCESSIBLE_NOTE:
    case ACCESSIBLE_FILTER:
    case ACCESSIBLE_BODY:
    case ACCESSIBLE_NOTICE:
    case ACCESSIBLE_SURFACE:
    case ACCESSIBLE_SURFACE_TABS:
    case ACCESSIBLE_SURFACE_TAB:
    case ACCESSIBLE_SURFACE_LIST:
    case ACCESSIBLE_SURFACE_CHOICE:
    case ACCESSIBLE_SURFACE_LANGUAGE:
    case ACCESSIBLE_SURFACE_TOGGLE:
    case ACCESSIBLE_SURFACE_BUTTON:
    case ACCESSIBLE_SURFACE_TEXT:
    case ACCESSIBLE_SURFACE_HELP:
    case ACCESSIBLE_SURFACE_STATUS:
    case ACCESSIBLE_COMMAND_INPUT:
    case ACCESSIBLE_REPLACE_PATTERN:
    case ACCESSIBLE_REPLACE_TEXT:
        return (RECT){0};
    }
    return (RECT){0};
}

bool folio_window_accessible_ready(const struct folio_window *_Nonnull self)
{
    if (self->handle == nullptr || !IsWindowEnabled(self->handle) || self->removal_running ||
        session_busy(self) || pane_streaming(self) || self->layout_changing || self->menu_active ||
        self->native_modal_depth > 0 || self->close_phase != CLOSE_PHASE_IDLE)
    {
        return false;
    }
    return !self->root_composing && !self->command_composing && !self->filter_composing &&
           (self->pane == nullptr || !note_pane_composing(self->pane));
}

uint64_t folio_window_accessible_surface_revision(const struct folio_window *_Nonnull self)
{
    return self->surface_revision;
}

uint64_t folio_window_accessible_revision(const struct folio_window *_Nonnull self)
{
    return folio_state_index_revision(self->state);
}

static HWND _Nullable accessible_input_window(const struct folio_window *_Nonnull self,
                                              enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_COMMAND_INPUT)
        return self->command_input;
    if (kind == ACCESSIBLE_REPLACE_PATTERN)
        return self->replace_inputs[0];
    if (kind == ACCESSIBLE_REPLACE_TEXT)
        return self->replace_inputs[1];
    return self->filter_input;
}

HWND _Nullable folio_window_accessible_window(struct folio_window *_Nonnull self,
                                              enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_SURFACE)
        return self->command_layer;
    if (kind == ACCESSIBLE_COMMAND_INPUT || kind == ACCESSIBLE_REPLACE_PATTERN ||
        kind == ACCESSIBLE_REPLACE_TEXT || kind == ACCESSIBLE_FILTER)
        return accessible_input_window(self, kind);
    if (kind == ACCESSIBLE_DRAWER)
    {
        return self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    }
    if (kind == ACCESSIBLE_BODY)
    {
        return self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    }
    return self->handle;
}

/* 描いたパンくずと同じ表示名。変更ありの間だけ「未保存」を含める（ADR0060 の決定 8）。
 * out は 1024 バイトの入れ物。組めなければ偽。 */
static bool breadcrumb_name(const struct folio_window *_Nonnull self, struct pane_title_view title,
                            char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    struct ui_text_request request = {.id = self->unsaved ? UI_TEXT_ACCESSIBLE_BREADCRUMB_UNSAVED
                                                          : UI_TEXT_ACCESSIBLE_BREADCRUMB,
                                      .language = language,
                                      .name = breadcrumb_note(title, language),
                                      .from = breadcrumb_category(title, language)};
    return ui_text_format(&request, out, 1024) == UI_TEXT_FORMAT_READY;
}

static const char *_Nonnull accessible_label_line(const struct folio_window *_Nonnull self,
                                                  enum accessible_kind kind)
{
    enum ui_text label =
        kind == ACCESSIBLE_COMMAND_INPUT ? surface_title(self) : part_label(self, kind);
    if (kind == ACCESSIBLE_COMMAND_INPUT && self->command_surface == COMMAND_SURFACE_THEMES)
        label = UI_TEXT_THEME_FILTER;
    if (kind == ACCESSIBLE_NOTICE)
        label = pane_notice_line(self);
    if (kind == ACCESSIBLE_NOTICE && self->notice == PANE_NOTICE_COMMAND_RESULT)
        return self->result_line;
    return ui_text_line(label, folio_state_language(self->state));
}

static void accessible_name(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                            char16_t *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    const char *line = accessible_label_line(self, kind);
    struct pane_title_view title = folio_state_pane_title(self->state);
    char body[1024];
    if (kind == ACCESSIBLE_LAYOUT_CHOOSER)
    {
        layout_chooser_name(self, body);
        line = body;
    }
    if (kind == ACCESSIBLE_BODY && title.any)
    {
        struct ui_text_request request = {
            .id = UI_TEXT_ACCESSIBLE_NAMED_BODY, .language = language, .name = title.note};
        if (ui_text_format(&request, body, sizeof body) == UI_TEXT_FORMAT_READY)
        {
            line = body;
        }
    }
    if (kind == ACCESSIBLE_BREADCRUMB && title.any && breadcrumb_name(self, title, body))
    {
        line = body;
    }
    (void)wide_units(line, out);
}

/* HelpText と ItemStatus。新しいノートは作成先、保存は未保存／保存済み、空状態の案内は
 * 見出しの後の 3 行（描いた字と同じ元）。 */
/* 空状態の案内の見出しの後の 3 行を改行でつないだ HelpText。収まらない行は捨てる。 */
static void guide_help(const struct folio_window *_Nonnull self, char16_t *_Nonnull out)
{
    const char *lines[4] = {"", "", "", ""};
    char destination[draw_unit_limit];
    guide_lines(self, lines, destination);
    char joined[draw_unit_limit];
    size_t at = 0;
    for (size_t index = 1; index < 4 && at + strlen(lines[index]) + 2 <= sizeof joined; ++index)
    {
        at = append_text(joined, at, lines[index]);
        at = append_text(joined, at, index < 3 ? "\n" : "");
    }
    joined[at] = '\0';
    (void)wide_units(joined, out);
}

static void save_status(const struct folio_window *_Nonnull self,
                        struct accessible_view *_Nonnull out)
{
    size_t written = 0;
    enum ui_text status = self->unsaved ? UI_TEXT_STATUS_UNSAVED : UI_TEXT_STATUS_SAVED;
    if (utf16_text_fill(ui_text_line(status, folio_state_language(self->state)), out->status,
                        sizeof out->status / sizeof out->status[0],
                        &written) != UTF16_TEXT_FILL_READY)
    {
        out->status[0] = u'\0';
    }
}

static void accessible_details(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                               struct accessible_view *_Nonnull out)
{
    if (kind == ACCESSIBLE_NEW_NOTE)
    {
        char line[draw_unit_limit];
        new_note_destination(self, line);
        (void)wide_units(line, out->help);
    }
    if (kind == ACCESSIBLE_LAYOUT_CHOOSER)
    {
        char line[draw_unit_limit];
        layout_chooser_help(self, line);
        (void)wide_units(line, out->help);
    }
    if (kind == ACCESSIBLE_SAVE)
    {
        save_status(self, out);
    }
    if (kind == ACCESSIBLE_GUIDE)
    {
        guide_help(self, out->help);
    }
}

/* AcceleratorKey は tooltip と同じキーの文言から写す（ADR0060 の決定 5）。out は 64 単位。 */
static const char *_Nonnull part_shortcut(enum accessible_kind kind, enum folio_language language)
{
    enum folio_command command = FOLIO_COMMAND_HELP;
    if (kind == ACCESSIBLE_CLOSE)
        command = FOLIO_COMMAND_QUIT;
    else if (!accessible_command(kind, &command))
        return "";
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_action(binding) == FOLIO_SHORTCUT_ACTION_COMMAND &&
            folio_shortcut_command(binding) == command)
            return folio_shortcut_label(binding, language);
    }
    return "";
}

static void accessible_accelerator(const struct folio_window *_Nonnull self,
                                   enum accessible_kind kind, char16_t *_Nonnull out)
{
    size_t written = 0;
    if (utf16_text_fill(part_shortcut(kind, folio_state_language(self->state)), out, 64,
                        &written) != UTF16_TEXT_FILL_READY)
        out[0] = u'\0';
}

static void accessible_main_bounds(struct folio_window *_Nonnull self, enum accessible_kind kind,
                                   struct accessible_view *_Nonnull out)
{
    if (layout_control(kind) && !accessible_visible(self, kind))
    {
        out->bounds = (RECT){0};
        return;
    }
    if (kind == ACCESSIBLE_WINDOW)
    {
        GetWindowRect(self->handle, &out->bounds);
        return;
    }
    if (kind == ACCESSIBLE_FILTER || kind == ACCESSIBLE_BODY || kind == ACCESSIBLE_COMMAND_INPUT ||
        kind == ACCESSIBLE_REPLACE_PATTERN || kind == ACCESSIBLE_REPLACE_TEXT)
    {
        HWND window = folio_window_accessible_window(self, kind);
        if (window != nullptr)
        {
            GetWindowRect(window, &out->bounds);
            out->enabled = out->enabled && IsWindowEnabled(window);
        }
        return;
    }
    if (kind == ACCESSIBLE_NOTICE)
    {
        RECT client;
        GetClientRect(self->handle, &client);
        out->bounds = pane_notice_rect(self, client);
        out->live = true;
    }
    else
        out->bounds = accessible_main_rect(self, kind);
    MapWindowPoints(self->handle, nullptr, (POINT *)&out->bounds, 2);
}

static void accessible_selection_view(struct folio_window *_Nonnull self, enum accessible_kind kind,
                                      struct accessible_view *_Nonnull out)
{
    out->selected = part_selected(self, kind);
    if (kind == ACCESSIBLE_LAYOUT_GROUP || kind == ACCESSIBLE_MODE_GROUP || kind == ACCESSIBLE_TREE)
    {
        struct accessible_item item = {.kind = kind};
        struct accessible_item selected = {.kind = ACCESSIBLE_WINDOW};
        out->selection_required = folio_window_accessible_selected(self, &item, &selected) == S_OK;
    }
}

static bool part_focusable(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (!accessible_controls[kind].focusable || !accessible_visible(self, kind))
        return false;
    return !accessible_item_shell(kind) ||
           (self->native_modal_depth == 0 && self->command_surface == COMMAND_SURFACE_CLOSED);
}

static void accessible_availability(const struct folio_window *_Nonnull self,
                                    enum accessible_kind kind, struct accessible_view *_Nonnull out)
{
    out->enabled = part_enabled(self, kind) && accessible_visible(self, kind);
    if (kind == ACCESSIBLE_HEADER)
        out->layout_chooser = layout_metrics_for(self).chooser;
    if (accessible_item_menu(kind))
    {
        enum folio_menu_context context =
            kind == ACCESSIBLE_LAYOUT_CHOOSER ? FOLIO_MENU_LAYOUT : FOLIO_MENU_OPERATIONS;
        out->expanded =
            self->command_surface == COMMAND_SURFACE_MENU && self->menu.context == context;
        out->enabled = out->enabled || out->expanded;
    }
    if (kind == ACCESSIBLE_COMMAND_INPUT || kind == ACCESSIBLE_REPLACE_PATTERN ||
        kind == ACCESSIBLE_REPLACE_TEXT)
        out->enabled = folio_window_accessible_ready(self);
    out->focusable = part_focusable(self, kind);
}

HRESULT folio_window_accessible_read(struct folio_window *_Nonnull self,
                                     const struct accessible_item *_Nonnull item,
                                     struct accessible_view *_Nonnull out)
{
    if (self->handle == nullptr)
    {
        return accessibility_node_unavailable;
    }
    *out = (struct accessible_view){0};
    if (accessible_item_surface(*item))
        return surface_read(self, *item, out);
    accessible_name(self, item->kind, out->name);
    accessible_accelerator(self, item->kind, out->accelerator);
    accessible_details(self, item->kind, out);
    accessible_availability(self, item->kind, out);
    if (accessible_item_host(*item) == ACCESSIBLE_DRAWER)
    {
        if (self->drawer == nullptr)
        {
            return accessibility_node_unavailable;
        }
        HRESULT result = drawer_window_accessible_read(self->drawer, item, out);
        if (FAILED(result))
        {
            return result;
        }
    }
    else
    {
        accessible_main_bounds(self, item->kind, out);
    }
    out->offscreen = out->offscreen || !accessible_visible(self, item->kind) ||
                     !IsWindowVisible(self->handle) || IsIconic(self->handle);
    struct accessible_item focus = {.kind = ACCESSIBLE_WINDOW};
    out->focused = folio_window_accessible_focus(self, ACCESSIBLE_WINDOW, &focus) == S_OK &&
                   accessible_item_equal(focus, *item);
    accessible_selection_view(self, item->kind, out);
    return S_OK;
}

HRESULT folio_window_accessible_selected(struct folio_window *_Nonnull self,
                                         const struct accessible_item *_Nonnull item,
                                         struct accessible_item *_Nonnull out)
{
    if (accessible_item_surface(*item))
        return surface_selected(self, item, out);
    if (item->kind == ACCESSIBLE_LAYOUT_GROUP)
    {
        const enum accessible_kind layouts[] = {[FOLIO_LAYOUT_DRAWER] = ACCESSIBLE_LAYOUT_DRAWER,
                                                [FOLIO_LAYOUT_BOTH] = ACCESSIBLE_LAYOUT_BOTH,
                                                [FOLIO_LAYOUT_NOTE] = ACCESSIBLE_LAYOUT_NOTE};
        *out = (struct accessible_item){.kind = layouts[folio_state_layout_mode(self->state)]};
        return S_OK;
    }
    if (item->kind == ACCESSIBLE_MODE_GROUP)
    {
        *out = (struct accessible_item){.kind = folio_state_pane_mode(self->state) == PANE_MODE_VIEW
                                                    ? ACCESSIBLE_VIEW
                                                    : ACCESSIBLE_EDIT};
        return S_OK;
    }
    struct note_ref selected;
    if (item->kind != ACCESSIBLE_TREE || !folio_state_selection(self->state, &selected))
    {
        return S_FALSE;
    }
    *out = (struct accessible_item){.kind = ACCESSIBLE_NOTE,
                                    .ref = selected,
                                    .revision = folio_state_row_revision(self->state)};
    struct accessible_view view = {0};
    HRESULT result = self->drawer == nullptr
                         ? accessibility_node_unavailable
                         : drawer_window_accessible_read(self->drawer, out, &view);
    return result == accessibility_node_unavailable ? S_FALSE : result;
}

static struct accessible_item accessible_cursor(const struct folio_window *_Nonnull self)
{
    struct accessible_item item = {.kind = ACCESSIBLE_TREE};
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref ref;
    if (folio_state_cursor(self->state, &kind, &ref))
    {
        item = (struct accessible_item){.kind = kind == FOLIO_CURSOR_CATEGORY ? ACCESSIBLE_CATEGORY
                                                                              : ACCESSIBLE_NOTE,
                                        .ref = ref,
                                        .revision = folio_state_row_revision(self->state)};
    }
    return item;
}

HRESULT folio_window_accessible_focus(struct folio_window *_Nonnull self, enum accessible_kind root,
                                      struct accessible_item *_Nonnull out)
{
    if (self->command_surface != COMMAND_SURFACE_CLOSED)
    {
        return root == ACCESSIBLE_DRAWER ? S_FALSE : surface_focus(self, out);
    }
    if (GetFocus() != self->handle || !folio_window_accessible_ready(self))
    {
        return S_FALSE; /* native入力欄・modalには既定providerが答える。 */
    }
    *out = self->keyboard_target == ACCESSIBLE_TREE
               ? accessible_cursor(self)
               : (struct accessible_item){.kind = self->keyboard_target};
    if (!accessible_visible(self, out->kind) ||
        (root == ACCESSIBLE_DRAWER && accessible_item_host(*out) != ACCESSIBLE_DRAWER))
    {
        return S_FALSE;
    }
    return S_OK;
}

static bool accessible_virtual(enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind};
    return !accessible_item_surface(item) && kind != ACCESSIBLE_COMMAND_INPUT &&
           kind != ACCESSIBLE_REPLACE_PATTERN && kind != ACCESSIBLE_REPLACE_TEXT &&
           !accessible_item_row(item) && kind != ACCESSIBLE_FILTER && kind != ACCESSIBLE_BODY &&
           !accessible_item_root(item);
}

static bool accessible_edge_direction(enum accessible_navigation direction)
{
    return direction == ACCESSIBLE_NAVIGATION_FIRST || direction == ACCESSIBLE_NAVIGATION_LAST;
}

static bool accessible_forward_direction(enum accessible_navigation direction)
{
    return direction == ACCESSIBLE_NAVIGATION_FIRST || direction == ACCESSIBLE_NAVIGATION_NEXT;
}

static int accessible_navigation_start(enum accessible_kind kind,
                                       enum accessible_navigation direction)
{
    int count = (int)(sizeof accessible_controls / sizeof accessible_controls[0]);
    bool forward = accessible_forward_direction(direction);
    if (accessible_edge_direction(direction))
    {
        return forward ? 0 : count - 1;
    }
    return (int)kind + (forward ? 1 : -1);
}

static HRESULT accessible_static_next(const struct folio_window *_Nonnull self,
                                      const struct accessible_item *_Nonnull item,
                                      enum accessible_navigation direction,
                                      struct accessible_item *_Nonnull out)
{
    enum accessible_kind parent =
        accessible_edge_direction(direction) ? item->kind : accessible_controls[item->kind].parent;
    int step = accessible_forward_direction(direction) ? 1 : -1;
    int count = (int)(sizeof accessible_controls / sizeof accessible_controls[0]);
    int index = accessible_navigation_start(item->kind, direction);
    for (; index >= 0 && index < count; index += step)
    {
        enum accessible_kind kind = (enum accessible_kind)index;
        if (accessible_virtual(kind) && accessible_controls[kind].parent == parent &&
            accessible_visible(self, kind))
        {
            *out = (struct accessible_item){.kind = kind};
            return S_OK;
        }
    }
    return S_FALSE;
}

HRESULT folio_window_accessible_navigate(struct folio_window *_Nonnull self,
                                         const struct accessible_item *_Nonnull item,
                                         enum accessible_navigation direction,
                                         struct accessible_item *_Nonnull out)
{
    if (accessible_item_surface(*item))
        return surface_navigate(self, item, direction, out);
    struct accessible_view view = {0};
    HRESULT result = folio_window_accessible_read(self, item, &view);
    if (FAILED(result))
    {
        return result;
    }
    bool tree_child = item->kind == ACCESSIBLE_TREE && accessible_edge_direction(direction);
    if (accessible_item_row(*item) || tree_child)
    {
        return drawer_window_accessible_navigate(self->drawer, item, direction, out);
    }
    if (accessible_item_root(*item) && !accessible_edge_direction(direction))
    {
        return S_FALSE;
    }
    if (direction == ACCESSIBLE_NAVIGATION_PARENT)
    {
        *out = (struct accessible_item){.kind = accessible_controls[item->kind].parent};
        return S_OK;
    }
    return accessible_static_next(self, item, direction, out);
}

static bool accessible_native(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_FILTER || kind == ACCESSIBLE_BODY;
}

static bool accessible_contains(struct folio_window *_Nonnull self, enum accessible_kind kind,
                                POINT point)
{
    struct accessible_item item = {.kind = kind};
    if (!accessible_visible(self, kind) || !accessible_controls[kind].focusable ||
        accessible_item_host(item) == ACCESSIBLE_DRAWER)
    {
        return false;
    }
    struct accessible_view view = {0};
    return SUCCEEDED(folio_window_accessible_read(self, &item, &view)) &&
           PtInRect(&view.bounds, point);
}

static bool accessible_drawer_contains(struct folio_window *_Nonnull self, POINT point)
{
    HWND drawer = folio_window_accessible_window(self, ACCESSIBLE_DRAWER);
    RECT bounds;
    return drawer != nullptr && IsWindowVisible(drawer) && GetWindowRect(drawer, &bounds) &&
           PtInRect(&bounds, point);
}

static HRESULT accessible_control_at(struct folio_window *_Nonnull self, POINT point,
                                     struct accessible_item *_Nonnull out)
{
    size_t count = sizeof accessible_controls / sizeof accessible_controls[0];
    for (size_t index = count; index > 0; --index)
    {
        enum accessible_kind kind = (enum accessible_kind)(index - 1);
        if (accessible_contains(self, kind, point))
        {
            *out = (struct accessible_item){.kind = kind};
            return accessible_native(kind) ? S_FALSE : S_OK;
        }
    }
    *out = (struct accessible_item){.kind = ACCESSIBLE_WINDOW};
    return S_OK;
}

HRESULT folio_window_accessible_at(struct folio_window *_Nonnull self, enum accessible_kind root,
                                   POINT point, struct accessible_item *_Nonnull out)
{
    if (root == ACCESSIBLE_SURFACE)
        return surface_at(self, point, out);
    HWND window = folio_window_accessible_window(self, root);
    RECT bounds;
    if (window == nullptr || !GetWindowRect(window, &bounds) || !PtInRect(&bounds, point))
    {
        return S_FALSE;
    }
    if (root == ACCESSIBLE_DRAWER || accessible_drawer_contains(self, point))
    {
        return drawer_window_accessible_at(self->drawer, point, out);
    }
    return accessible_control_at(self, point, out);
}

static HRESULT accessible_outcome(struct folio_window *_Nonnull self,
                                  enum folio_state_outcome outcome)
{
    if (outcome == FOLIO_STATE_READY)
    {
        return S_OK;
    }
    if (outcome != FOLIO_STATE_CANCELLED)
    {
        report_outcome(self, outcome);
    }
    return accessibility_node_invalid;
}

static HRESULT accessible_native_focus(struct folio_window *_Nonnull self,
                                       enum accessible_kind kind)
{
    HWND window = folio_window_accessible_window(self, kind);
    if (window == nullptr || !IsWindowVisible(window) || !IsWindowEnabled(window))
    {
        return accessibility_node_disabled;
    }
    SetFocus(window);
    return GetFocus() == window ? S_OK : accessibility_node_invalid;
}

static HRESULT accessible_row_focus(struct folio_window *_Nonnull self, struct accessible_item item)
{
    enum folio_cursor_kind kind =
        item.kind == ACCESSIBLE_CATEGORY ? FOLIO_CURSOR_CATEGORY : FOLIO_CURSOR_NOTE;
    enum folio_state_outcome result = folio_state_focus_cursor(self->state, kind, item.ref);
    if (result != FOLIO_STATE_READY)
    {
        return accessibility_node_unavailable;
    }
    self->keyboard_target = ACCESSIBLE_TREE;
    drawer_window_reveal_cursor(self->drawer);
    return S_OK;
}

static HRESULT accessible_set_focus(struct folio_window *_Nonnull self, struct accessible_item item)
{
    if (!accessible_controls[item.kind].focusable)
    {
        return accessibility_node_invalid;
    }
    if (accessible_native(item.kind))
    {
        return accessible_native_focus(self, item.kind);
    }
    if (accessible_item_row(item))
    {
        HRESULT result = accessible_row_focus(self, item);
        if (FAILED(result))
        {
            return result;
        }
    }
    else
    {
        self->keyboard_target = item.kind;
    }
    self->pending_g = false;
    SetFocus(self->handle);
    InvalidateRect(self->handle, nullptr, FALSE);
    redraw_drawer(self);
    return GetFocus() == self->handle ? S_OK : accessibility_node_invalid;
}

static bool accessible_command(enum accessible_kind kind, enum folio_command *_Nonnull out)
{
    static const struct
    {
        enum accessible_kind kind;
        enum folio_command command;
    } commands[] = {{ACCESSIBLE_LAYOUT_DRAWER, FOLIO_COMMAND_LAYOUT_DRAWER},
                    {ACCESSIBLE_LAYOUT_BOTH, FOLIO_COMMAND_LAYOUT_BOTH},
                    {ACCESSIBLE_LAYOUT_NOTE, FOLIO_COMMAND_LAYOUT_NOTE},
                    {ACCESSIBLE_SETTINGS, FOLIO_COMMAND_SETTINGS},
                    {ACCESSIBLE_VIEW, FOLIO_COMMAND_VIEW},
                    {ACCESSIBLE_EDIT, FOLIO_COMMAND_EDIT},
                    {ACCESSIBLE_NEW_NOTE, FOLIO_COMMAND_NEW},
                    {ACCESSIBLE_NEW_CATEGORY, FOLIO_COMMAND_NEW_CATEGORY},
                    {ACCESSIBLE_SAVE, FOLIO_COMMAND_SAVE},
                    {ACCESSIBLE_HELP, FOLIO_COMMAND_HELP},
                    {ACCESSIBLE_FIND, FOLIO_COMMAND_FIND}};
    for (size_t index = 0; index < sizeof commands / sizeof commands[0]; ++index)
    {
        if (commands[index].kind == kind)
        {
            *out = commands[index].command;
            return true;
        }
    }
    return false;
}

static void accessible_command_invoked(struct folio_window *_Nonnull self,
                                       enum folio_command command)
{
    const enum accessible_kind kinds[] = {ACCESSIBLE_SETTINGS,     ACCESSIBLE_SAVE,
                                          ACCESSIBLE_HELP,         ACCESSIBLE_FIND,
                                          ACCESSIBLE_NEW_CATEGORY, ACCESSIBLE_NEW_NOTE};
    enum folio_command mapped = FOLIO_COMMAND_HELP;
    if (accessible_command(self->keyboard_target, &mapped) && mapped == command &&
        accessible_visible(self, self->keyboard_target))
    {
        accessibility_host_invoked(self->accessibility,
                                   (struct accessible_item){.kind = self->keyboard_target});
        return;
    }
    for (size_t index = 0; index < sizeof kinds / sizeof kinds[0]; ++index)
    {
        if (accessible_command(kinds[index], &mapped) && mapped == command &&
            accessible_visible(self, kinds[index]))
        {
            accessibility_host_invoked(self->accessibility,
                                       (struct accessible_item){.kind = kinds[index]});
            return;
        }
    }
}

static HRESULT accessible_select_note(struct folio_window *_Nonnull self,
                                      const struct accessible_request *_Nonnull request)
{
    if (request->action == ACCESSIBLE_ACTION_INVOKE)
    {
        accessibility_host_invoked(self->accessibility, request->item);
    }
    enum folio_state_outcome result =
        holds_document(self, request->item.ref)
            ? folio_state_focus_cursor(self->state, FOLIO_CURSOR_NOTE, request->item.ref)
            : switch_note(self, request->item.ref.category, request->item.ref.note);
    if (result == FOLIO_STATE_READY && request->action == ACCESSIBLE_ACTION_INVOKE)
    {
        focus_pane(self);
    }
    redraw_drawer(self);
    return accessible_outcome(self, result);
}

/* 可否とInvokeの検査後だけ呼ぶ。captureを返して同じOS command経路へ合流する。 */
static void invoke_shell(struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    WPARAM command = kind == ACCESSIBLE_CLOSE      ? SC_CLOSE
                     : kind == ACCESSIBLE_MINIMIZE ? SC_MINIMIZE
                     : IsZoomed(self->handle)      ? SC_RESTORE
                                                   : SC_MAXIMIZE;
    cancel_part_press(self);
    SendMessageW(self->handle, WM_SYSCOMMAND, command, 0);
}

static HRESULT accessible_activate(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request)
{
    if (accessible_item_shell(request->item.kind) && request->action == ACCESSIBLE_ACTION_INVOKE)
    {
        invoke_shell(self, request->item.kind);
        return S_OK;
    }
    if (request->item.kind == ACCESSIBLE_NOTE)
    {
        return accessible_select_note(self, request);
    }
    if (accessible_item_menu(request->item.kind))
    {
        show_part_menu(self, request->item.kind);
        return S_OK;
    }
    enum folio_command command = FOLIO_COMMAND_HELP;
    if (!accessible_command(request->item.kind, &command))
    {
        return accessibility_node_invalid;
    }
    execute_command(self, command, "");
    if (request->action == ACCESSIBLE_ACTION_SELECT)
    {
        struct accessible_view view = {0};
        HRESULT result = folio_window_accessible_read(self, &request->item, &view);
        return SUCCEEDED(result) && view.selected ? S_OK : accessibility_node_invalid;
    }
    return S_OK;
}

static HRESULT accessible_expand(struct folio_window *_Nonnull self,
                                 const struct accessible_request *_Nonnull request)
{
    if (accessible_item_menu(request->item.kind))
    {
        enum folio_menu_context context = request->item.kind == ACCESSIBLE_LAYOUT_CHOOSER
                                              ? FOLIO_MENU_LAYOUT
                                              : FOLIO_MENU_OPERATIONS;
        if (request->action == ACCESSIBLE_ACTION_EXPAND &&
            !(self->command_surface == COMMAND_SURFACE_MENU && self->menu.context == context))
        {
            show_part_menu(self, request->item.kind);
        }
        return S_OK;
    }
    if (request->item.kind != ACCESSIBLE_CATEGORY || folio_state_filtering(self->state))
    {
        return accessibility_node_invalid;
    }
    HRESULT result = accessible_outcome(
        self, folio_state_set_expansion(self->state, request->item.ref.category,
                                        request->action == ACCESSIBLE_ACTION_EXPAND));
    redraw_drawer(self);
    return result;
}

static HRESULT accessible_dispatch(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request, bool selected)
{
    if (accessible_item_surface(request->item))
        return surface_action(self, request);
    if (request->action == ACCESSIBLE_ACTION_SELECT && selected &&
        request->item.kind != ACCESSIBLE_NOTE)
    {
        return S_OK;
    }
    switch (request->action)
    {
    case ACCESSIBLE_ACTION_FOCUS:
        return accessible_set_focus(self, request->item);
    case ACCESSIBLE_ACTION_INVOKE:
    case ACCESSIBLE_ACTION_SELECT:
        return accessible_activate(self, request);
    case ACCESSIBLE_ACTION_EXPAND:
    case ACCESSIBLE_ACTION_COLLAPSE:
        return accessible_expand(self, request);
    case ACCESSIBLE_ACTION_REVEAL:
    case ACCESSIBLE_ACTION_SCROLL:
        return accessible_outcome(self, drawer_window_accessible_scroll(self->drawer, request));
    }
    return accessibility_node_invalid;
}

static HRESULT accessible_action(struct folio_window *_Nonnull self,
                                 const struct accessible_request *_Nonnull request)
{
    struct accessible_view view = {0};
    HRESULT result = folio_window_accessible_read(self, &request->item, &view);
    if (FAILED(result))
    {
        return result;
    }
    if (accessible_item_menu(request->item.kind) && request->action == ACCESSIBLE_ACTION_COLLAPSE &&
        view.expanded)
    {
        close_command_surface(self);
        return S_OK;
    }
    bool shell_invoke =
        accessible_item_shell(request->item.kind) && request->action == ACCESSIBLE_ACTION_INVOKE;
    if (!view.enabled || (!shell_invoke && !folio_window_accessible_ready(self)))
    {
        return accessibility_node_disabled;
    }
    if (request->action == ACCESSIBLE_ACTION_FOCUS && !view.focusable)
        return accessibility_node_invalid;
    return accessible_dispatch(self, request, view.selected);
}

HRESULT folio_window_accessible_action(struct folio_window *self,
                                       const struct accessible_request *request)
{
    self->dispatch_depth += 1;
    HRESULT result = accessible_action(self, request);
    dispatch_finished(self);
    return result;
}

/* F6は区画、Tabは区画内。本文・IME・modalはこの表へ入れない。 */
static enum accessible_kind keyboard_kind(const struct folio_window *_Nonnull self)
{
    HWND focus = GetFocus();
    if (focus == self->filter_input)
    {
        return ACCESSIBLE_FILTER;
    }
    if (self->pane != nullptr && focus == note_pane_handle(self->pane))
    {
        return ACCESSIBLE_BODY;
    }
    return self->keyboard_target;
}

static enum accessible_kind keyboard_region(enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_BODY)
    {
        return ACCESSIBLE_BODY;
    }
    if (kind == ACCESSIBLE_FILTER ||
        accessible_item_host((struct accessible_item){.kind = kind}) == ACCESSIBLE_DRAWER)
    {
        return ACCESSIBLE_DRAWER;
    }
    return accessible_pane_control(kind) ? ACCESSIBLE_PANE_TOOLS : ACCESSIBLE_HEADER;
}

static bool keyboard_focus(struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind};
    if (kind == ACCESSIBLE_TREE)
    {
        item = accessible_cursor(self);
    }
    HRESULT result = accessible_set_focus(self, item);
    accessibility_host_schedule(self->accessibility);
    return SUCCEEDED(result);
}

/* 選択群は Tab で 1 回だけ止まる。群の中の項目は群の先頭の項目を代表にする（ADR0060 の決定 5）。 */
static enum accessible_kind keyboard_slot(enum accessible_kind kind)
{
    if (accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP)
    {
        return ACCESSIBLE_LAYOUT_DRAWER;
    }
    return accessible_controls[kind].parent == ACCESSIBLE_MODE_GROUP ? ACCESSIBLE_VIEW : kind;
}

/* 群へ入るときは選択中の項目に止まる。 */
static enum accessible_kind keyboard_entry(const struct folio_window *_Nonnull self,
                                           enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_LAYOUT_DRAWER)
    {
        return layout_metrics_for(self).chooser ? ACCESSIBLE_LAYOUT_CHOOSER
                                                : selected_layout_kind(self);
    }
    return kind == ACCESSIBLE_VIEW ? mode_kind(folio_state_pane_mode(self->state)) : kind;
}

/* 群の中の隣の項目（端で折り返す）。兄弟は列挙で連続している。 */
static enum accessible_kind choice_neighbor(enum accessible_kind kind, bool forward)
{
    constexpr size_t count = sizeof accessible_controls / sizeof accessible_controls[0];
    enum accessible_kind parent = accessible_controls[kind].parent;
    size_t first = kind;
    while (first > 0 && accessible_controls[first - 1].parent == parent)
    {
        --first;
    }
    size_t last = kind;
    while (last + 1 < count && accessible_controls[last + 1].parent == parent)
    {
        ++last;
    }
    size_t span = last - first + 1;
    size_t at = (size_t)kind - first;
    at = forward ? (at + 1) % span : (at + span - 1) % span;
    return (enum accessible_kind)(first + at);
}

/* 選択群の中では矢印が focus だけを動かし、選択は Enter/Space で確定する。
 * 矢印で選ぶと一覧経由で窓が縮む・閲覧で保存が走るため（ADR0060 の決定 5）。 */
static bool choice_step(struct folio_window *_Nonnull self, WPARAM key)
{
    bool forward = key == VK_RIGHT || key == VK_DOWN;
    bool backward = key == VK_LEFT || key == VK_UP;
    enum accessible_kind parent = accessible_controls[self->keyboard_target].parent;
    if ((!forward && !backward) ||
        (parent != ACCESSIBLE_LAYOUT_GROUP && parent != ACCESSIBLE_MODE_GROUP))
    {
        return false;
    }
    cancel_part_press(self);
    (void)keyboard_focus(self, choice_neighbor(self->keyboard_target, forward));
    return true;
}

static void keyboard_region_step(struct folio_window *_Nonnull self, bool forward)
{
    const enum accessible_kind regions[] = {ACCESSIBLE_HEADER, ACCESSIBLE_DRAWER,
                                            ACCESSIBLE_PANE_TOOLS, ACCESSIBLE_BODY};
    const enum accessible_kind entries[] = {ACCESSIBLE_NEW_NOTE, ACCESSIBLE_FILTER, ACCESSIBLE_VIEW,
                                            ACCESSIBLE_BODY};
    enum accessible_kind current = keyboard_region(keyboard_kind(self));
    size_t index = 0;
    for (size_t at = 0; at < 4; ++at)
    {
        if (regions[at] == current)
        {
            index = at;
        }
    }
    for (size_t step = 0; step < 4; ++step)
    {
        index = forward ? (index + 1) % 4 : (index + 3) % 4;
        enum accessible_kind entry = keyboard_entry(self, entries[index]);
        if (accessible_visible(self, entry) && keyboard_focus(self, entry))
        {
            return;
        }
    }
}

static bool keyboard_same_region(enum accessible_kind left, enum accessible_kind right)
{
    return keyboard_region(left) == keyboard_region(right);
}

static void keyboard_tab_step(struct folio_window *_Nonnull self, bool forward)
{
    const enum accessible_kind order[] = {
        ACCESSIBLE_NEW_NOTE,     ACCESSIBLE_LAYOUT_DRAWER, ACCESSIBLE_HELP,  ACCESSIBLE_SETTINGS,
        ACCESSIBLE_MINIMIZE,     ACCESSIBLE_MAXIMIZE,      ACCESSIBLE_CLOSE, ACCESSIBLE_FILTER,
        ACCESSIBLE_NEW_CATEGORY, ACCESSIBLE_TREE,          ACCESSIBLE_VIEW,  ACCESSIBLE_SAVE,
        ACCESSIBLE_FIND,         ACCESSIBLE_MENU};
    size_t count = sizeof order / sizeof order[0];
    enum accessible_kind current = keyboard_slot(keyboard_kind(self));
    if (current == ACCESSIBLE_LAYOUT_CHOOSER)
        current = ACCESSIBLE_LAYOUT_DRAWER;
    size_t index = 0;
    for (size_t at = 0; at < count; ++at)
    {
        if (order[at] == current)
        {
            index = at;
        }
    }
    for (size_t step = 0; step < count; ++step)
    {
        index = forward ? (index + 1) % count : (index + count - 1) % count;
        enum accessible_kind entry = keyboard_entry(self, order[index]);
        if (keyboard_same_region(current, entry) && accessible_visible(self, entry) &&
            keyboard_focus(self, entry))
        {
            return;
        }
    }
}

static bool keyboard_accelerator(struct folio_window *_Nonnull self, WORD id)
{
    if (id < 111 || id > 114)
    {
        return false;
    }
    if ((self->command_surface != COMMAND_SURFACE_CLOSED || !folio_window_accessible_ready(self)))
    {
        return true;
    }
    cancel_part_press(self);
    show_focus_cues(self, true);
    if (id <= 112)
    {
        keyboard_region_step(self, id == 111);
    }
    else
    {
        keyboard_tab_step(self, id == 113);
    }
    return true;
}

static bool create_keyboard_accelerators(struct folio_window *_Nonnull self)
{
    ACCEL regions[] = {{.fVirt = FVIRTKEY, .key = VK_F6, .cmd = 111},
                       {.fVirt = FVIRTKEY | FSHIFT, .key = VK_F6, .cmd = 112}};
    ACCEL traversal[] = {{.fVirt = FVIRTKEY, .key = VK_TAB, .cmd = 113},
                         {.fVirt = FVIRTKEY | FSHIFT, .key = VK_TAB, .cmd = 114}};
    self->region_keys = CreateAcceleratorTableW(regions, 2);
    self->traversal_keys = CreateAcceleratorTableW(traversal, 2);
    return self->region_keys != nullptr && self->traversal_keys != nullptr;
}

static bool translate_keyboard(const struct folio_window *_Nonnull self,
                               const MSG *_Nonnull message)
{
    if ((self->command_surface != COMMAND_SURFACE_CLOSED || !folio_window_accessible_ready(self)) ||
        !command_target(self, message->hwnd))
    {
        return false;
    }
    MSG translated = *message;
    if (TranslateAcceleratorW(self->handle, self->region_keys, &translated) != 0)
    {
        return true;
    }
    if (message->hwnd == self->handle || message->hwnd == self->filter_input)
    {
        return TranslateAcceleratorW(self->handle, self->traversal_keys, &translated) != 0;
    }
    return false;
}

static void keyboard_activate(struct folio_window *_Nonnull self)
{
    if (self->keyboard_target != ACCESSIBLE_TREE)
    {
        activate_part(self, self->keyboard_target);
        return;
    }
    struct accessible_item item = accessible_cursor(self);
    if (item.kind == ACCESSIBLE_CATEGORY || item.kind == ACCESSIBLE_TREE)
    {
        return;
    }
    struct accessible_request request = {.item = item, .action = ACCESSIBLE_ACTION_INVOKE};
    (void)folio_window_accessible_action(self, &request);
    accessibility_host_schedule(self->accessibility);
}

static void pointer_focus(struct folio_window *_Nonnull self, POINT point)
{
    ClientToScreen(self->handle, &point);
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    if (folio_window_accessible_at(self, ACCESSIBLE_WINDOW, point, &item) == S_OK &&
        accessible_controls[item.kind].focusable)
    {
        (void)accessible_set_focus(self, item);
    }
}

/* いまの本文が未保存か。application の 1 つの問い合わせ（folio_state_unsaved）に、名前付きの
 * 編集中だけ本文を渡す。本文を取れない間（流し込み中・未準備）は前の答えを保つ。 */
static bool unsaved_needs_compare(const struct folio_window *_Nonnull self, uint64_t revision)
{
    return self->body_touched || revision != self->store_revision ||
           folio_state_document_kind(self->state) != self->settled_kind ||
           folio_state_pane_mode(self->state) != self->settled_mode;
}

/* 本文を評価できたら真で、out に答えを置く。流し込み中・本文を取れない回は偽（前の答えを保つ）。 */
static bool current_unsaved(const struct folio_window *_Nonnull self, uint64_t revision,
                            bool *_Nonnull out)
{
    const char16_t *units = u"";
    size_t count = 0;
    *out = self->unsaved;
    if (pane_streaming(self))
    {
        return false;
    }
    if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT &&
        folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NAMED)
    {
        if (!unsaved_needs_compare(self, revision))
        {
            return true;
        }
        if (take_text(self, &units, &count) != FOLIO_STATE_READY)
        {
            return false;
        }
    }
    *out = folio_state_unsaved(self->state, units, count) == FOLIO_NOTE_CHANGED;
    return true;
}

/* 保存の状態を取り直す唯一の経路（ADR0060 の決定 8）。入力・保存・Esc・ノート切替・新規の後に
 * 同じ合流点（folio_message_accessibility_changed）から呼ばれ、釦の様式・未保存印・UIA を戻す。
 * md を書いていたら真を返し、呼び出し側が UIA の通知を 1 回出す。 */
static bool settle_save_state(struct folio_window *_Nonnull self)
{
    uint64_t revision = folio_state_store_revision(self->state);
    bool unsaved = self->unsaved;
    if (!current_unsaved(self, revision, &unsaved))
    {
        return false; /* 評価できない回は世代も印も持ち越し、次の合流点で取り直す */
    }
    /* md を書いた（世代が進んだ）ことだけで知らせる。Ctrl+N の離脱保存の後に無題が開いても出す。 */
    bool saved = revision != self->store_revision;
    self->store_revision = revision;
    self->body_touched = false;
    self->settled_kind = folio_state_document_kind(self->state);
    self->settled_mode = folio_state_pane_mode(self->state);
    if (unsaved != self->unsaved)
    {
        self->unsaved = unsaved;
        RECT caption = caption_rect(self);
        InvalidateRect(self->handle, &caption, FALSE);
        invalidate_part(self, ACCESSIBLE_SAVE);
    }
    return saved;
}

/* 画面に出さない保存の知らせ（トースト・timer・新しい面は足さない）。 */
static void announce_saved(struct folio_window *_Nonnull self)
{
    char16_t text[draw_unit_limit];
    (void)wide_units(ui_text_line(UI_TEXT_NOTIFY_SAVED, folio_state_language(self->state)), text);
    accessibility_host_notify(self->accessibility,
                              (struct accessible_item){.kind = ACCESSIBLE_WINDOW}, text);
}

/* posted通知が続いても、無効化済みの自前面を既存の描画経路へ渡す（ADR0065）。 */
static void present_pending_surface(HWND window)
{
    if (window != nullptr && IsWindowVisible(window) && GetUpdateRect(window, nullptr, FALSE))
        UpdateWindow(window);
}

/* 保存状態を確定し、描画してからUIA差分と保存の知らせを出す。 */
static void refresh_after_input(struct folio_window *_Nonnull self)
{
    bool saved = settle_save_state(self);
    present_pending_surface(self->handle);
    present_pending_surface(self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer));
    present_pending_surface(self->settings_backdrop);
    present_pending_surface(self->command_layer);
    accessibility_host_refresh(self->accessibility);
    if (saved)
    {
        announce_saved(self);
    }
}

/* 空queueとWaitMessageの間でも呼ぶため、副作用/queue確認を持たない（ADR0068）。 */
bool folio_window_idle_work_ready(const struct folio_window *_Nonnull self)
{
    return folio_state_index_filter_pending(self->state) && !self->filter_saving &&
           self->command_surface == COMMAND_SURFACE_CLOSED && folio_window_accessible_ready(self);
}

void folio_window_continue_index_filter(struct folio_window *_Nonnull self)
{
    if (!folio_window_idle_work_ready(self))
        return;
    folio_window_begin_dispatch(self);
    bool was_filtering = folio_state_filtering(self->state);
    enum folio_state_outcome outcome = folio_state_continue_index_filter(self->state);
    if (!folio_state_index_filter_pending(self->state))
    {
        refresh_index_filter(self, was_filtering);
        if (part_enabled(self, ACCESSIBLE_TREE))
            refresh_after_input(self);
        if (outcome != FOLIO_STATE_READY && part_enabled(self, ACCESSIBLE_TREE))
            report_outcome(self, outcome);
    }
    folio_window_end_dispatch(self);
}

static LRESULT on_accessibility_message(struct folio_window *_Nonnull self, UINT message,
                                        WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_GETOBJECT:
        return accessibility_host_object(self->accessibility, ACCESSIBLE_WINDOW, wparam, lparam);
    case WM_ENTERMENULOOP:
        self->menu_active = true;
        set_hover(self, ACCESSIBLE_WINDOW);
        return 0;
    case WM_EXITMENULOOP:
        self->menu_active = false;
        return 0;
    case folio_message_accessibility_changed:
        refresh_after_input(self);
        return 0;
    case folio_message_accessibility_invoke:
        accessibility_host_dispatch(self->accessibility);
        return 0;
    case folio_message_index_focused:
        return GetFocus() == self->handle && self->keyboard_target == ACCESSIBLE_TREE;
    case folio_message_focus_cues:
        return self->focus_cues || self->focus_cues_always;
    case folio_message_filter_focused:
        return self->filter_input != nullptr && GetFocus() == self->filter_input;
    case folio_message_index_focus:
        show_focus_cues(self, false); /* drawer のポインタ操作だけが送る */
        if ((enum accessible_kind)wparam == ACCESSIBLE_FILTER)
        {
            focus_filter_input(self);
            return 0;
        }
        self->keyboard_target = wparam == 0 ? ACCESSIBLE_TREE : (enum accessible_kind)wparam;
        SetFocus(self->handle);
        InvalidateRect(self->handle, nullptr, FALSE);
        accessibility_host_schedule(self->accessibility);
        return 0;
    default:
        return DefWindowProcW(self->handle, message, wparam, lparam);
    }
}

/* MENU is a mode of the existing CommandLayer. The static descriptors own its vocabulary. */
static const struct folio_menu_action *_Nonnull menu_action(
    const struct folio_window *_Nonnull self, size_t index)
{
    return folio_menu_action_at(self->menu.context, index);
}

static enum folio_state_outcome menu_availability(const struct folio_window *_Nonnull self,
                                                  size_t index)
{
    if (index >= folio_menu_action_count(self->menu.context))
        return FOLIO_STATE_CANCELLED;
    const struct folio_menu_action *action = menu_action(self, index);
    if (folio_menu_action_kind(action) == FOLIO_MENU_ACTION_RECOLOR_CATEGORY)
        return folio_state_recolor_availability(self->state, self->menu.target.category);
    enum folio_command_target kind = FOLIO_COMMAND_TARGET_CURRENT;
    if (self->menu.context == FOLIO_MENU_CATEGORY)
        kind = FOLIO_COMMAND_TARGET_CATEGORY;
    if (self->menu.context == FOLIO_MENU_NOTE)
        kind = FOLIO_COMMAND_TARGET_NOTE;
    return folio_state_command_availability(self->state, folio_menu_action_command(action), kind,
                                            self->menu.target);
}

static void menu_caption(const struct folio_window *_Nonnull self, size_t index, char *_Nonnull out)
{
    if (folio_menu_action_caption(menu_action(self, index), folio_state_language(self->state), out,
                                  draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void menu_label(const struct folio_window *_Nonnull self, size_t index, char *_Nonnull out)
{
    char caption[draw_unit_limit];
    menu_caption(self, index, caption);
    char key[2] = {folio_menu_action_access_key(menu_action(self, index)), '\0'};
    struct ui_text_request request = {.id = UI_TEXT_MENU_ACCESS_CAPTION,
                                      .language = folio_state_language(self->state),
                                      .name = caption,
                                      .from = key};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void menu_hint(const struct folio_window *_Nonnull self, size_t index, char *_Nonnull out)
{
    if (folio_menu_action_hint(menu_action(self, index), folio_state_language(self->state), out,
                               draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static int menu_separator(const struct folio_window *_Nonnull self, size_t index)
{
    bool separated = index > 0 && folio_menu_action_group(menu_action(self, index)) !=
                                      folio_menu_action_group(menu_action(self, index - 1));
    return separated ? scale(8, GetDpiForWindow(self->handle)) : 0;
}

static int menu_total_height(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int height = scale(8, dpi) + 2;
    for (size_t at = 0; at < folio_menu_action_count(self->menu.context); ++at)
        height += scale(32, dpi) + menu_separator(self, at);
    return height;
}

static int menu_current_column(const struct folio_window *_Nonnull self)
{
    return self->menu.context == FOLIO_MENU_LAYOUT
               ? scale(ui_part_icon + 8, GetDpiForWindow(self->handle))
               : 0;
}

static bool menu_current(const struct folio_window *_Nonnull self, size_t index)
{
    if (self->menu.context != FOLIO_MENU_LAYOUT)
        return false;
    enum folio_command command = folio_menu_action_command(menu_action(self, index));
    for (size_t at = 0; at < layout_button_count(); ++at)
    {
        if (layout_buttons[at].command == command)
            return layout_buttons[at].mode == folio_state_layout_mode(self->state);
    }
    return false;
}

static int menu_width(const struct folio_window *_Nonnull self)
{
    HDC device = GetDC(self->handle);
    if (device == nullptr)
        return scale(360, GetDpiForWindow(self->handle));
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    int tracking =
        SetTextCharacterExtra(device, scale(base_tracking, GetDpiForWindow(self->handle)));
    int width = 0;
    for (size_t at = 0; at < folio_menu_action_count(self->menu.context); ++at)
    {
        char label[draw_unit_limit];
        char hint[draw_unit_limit];
        menu_label(self, at, label);
        menu_hint(self, at, hint);
        int measured = measure_utf8(device, label) + measure_utf8(device, hint);
        if (measured > width)
            width = measured;
    }
    SelectObject(device, previous);
    if (tracking != tracking_unavailable)
        SetTextCharacterExtra(device, tracking);
    ReleaseDC(self->handle, device);
    UINT dpi = GetDpiForWindow(self->handle);
    width += scale(48, dpi) + menu_current_column(self);
    int floor = scale(240, dpi);
    int ceiling = scale(360, dpi);
    width = width < floor ? floor : width;
    return width > ceiling ? ceiling : width;
}

static RECT menu_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    int edge = scale(16, GetDpiForWindow(self->handle));
    int below = client.bottom - edge - self->menu.anchor.bottom;
    int above = self->menu.anchor.top - edge;
    bool down = menu_total_height(self) <= below || below >= above;
    int room = down ? below : above;
    int height = menu_total_height(self);
    if (height > room)
        height = room;
    if (height < 0)
        height = 0;
    int width = menu_width(self);
    if (width > client.right - 2 * edge)
        width = client.right - 2 * edge;
    int left = self->menu.anchor.left;
    if (left + width > client.right - edge)
        left = client.right - edge - width;
    if (left < edge)
        left = edge;
    int top = down ? self->menu.anchor.bottom : self->menu.anchor.top - height;
    return (RECT){left, top, left + width, top + height};
}

static RECT menu_rows_area(const struct folio_window *_Nonnull self)
{
    RECT area;
    GetClientRect(self->command_layer, &area);
    int inset = scale(4, GetDpiForWindow(self->handle)) + 1;
    bool overflow = menu_total_height(self) > area.bottom;
    int arrows = overflow ? scale(8, GetDpiForWindow(self->handle)) : 0;
    area.top += inset + arrows;
    area.bottom -= inset + arrows;
    return area;
}

static RECT menu_row_rect_at(const struct folio_window *_Nonnull self, size_t first, size_t index)
{
    if (index < first || index >= folio_menu_action_count(self->menu.context))
        return (RECT){0};
    RECT area = menu_rows_area(self);
    int height = scale(32, GetDpiForWindow(self->handle));
    int top = area.top;
    for (size_t at = first; at <= index; ++at)
    {
        if (at > first)
            top += menu_separator(self, at);
        if (top + height > area.bottom)
            return (RECT){0};
        if (at == index)
            return (RECT){area.left, top, area.right, top + height};
        top += height;
    }
    return (RECT){0};
}

static RECT menu_row_rect(const struct folio_window *_Nonnull self, size_t index)
{
    return menu_row_rect_at(self, self->command_first, index);
}

static size_t menu_visible_count(const struct folio_window *_Nonnull self)
{
    size_t visible = 0;
    for (size_t at = self->command_first; at < folio_menu_action_count(self->menu.context); ++at)
    {
        RECT bounds = menu_row_rect(self, at);
        if (IsRectEmpty(&bounds))
            break;
        visible += 1;
    }
    return visible;
}

static size_t menu_scroll_limit(const struct folio_window *_Nonnull self)
{
    size_t count = folio_menu_action_count(self->menu.context);
    for (size_t first = 0; first < count; ++first)
    {
        RECT last = menu_row_rect_at(self, first, count - 1);
        if (!IsRectEmpty(&last))
            return first;
    }
    return count - 1;
}

static void menu_reveal(struct folio_window *_Nonnull self)
{
    size_t count = folio_menu_action_count(self->menu.context);
    if (self->command_selection >= count)
        return;
    if (self->command_selection < self->command_first)
        self->command_first = self->command_selection;
    RECT bounds = menu_row_rect(self, self->command_selection);
    while (IsRectEmpty(&bounds) && self->command_first < self->command_selection)
    {
        self->command_first += 1;
        bounds = menu_row_rect(self, self->command_selection);
    }
}

static size_t menu_hit(const struct folio_window *_Nonnull self, POINT point)
{
    for (size_t at = self->command_first; at < folio_menu_action_count(self->menu.context); ++at)
    {
        RECT bounds = menu_row_rect(self, at);
        if (PtInRect(&bounds, point))
            return at;
    }
    return SIZE_MAX;
}

/* 末尾の (key) は共通のMENU文言。部分文字列もラベル全体のfont-link規則で測る。 */
static RECT menu_key_rect(HDC device, const char *_Nonnull label, RECT bounds)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(label, units);
    if (count < 3)
        return (RECT){0};
    int tracking = GetTextCharacterExtra(device);
    if (tracking == tracking_unavailable || font_linked_units(units, count))
        tracking = 0;
    SIZE full = measured_units(device, units, count, tracking);
    TEXTMETRICW metrics;
    if (full.cx > bounds.right - bounds.left || GetTextMetricsW(device, &metrics) == 0)
        return (RECT){0};
    SIZE before = measured_units(device, units, count - 2, tracking);
    SIZE through = measured_units(device, units, count - 1, tracking);
    int y = bounds.top + (bounds.bottom - bounds.top - full.cy) / 2 + metrics.tmAscent + 1;
    return (RECT){bounds.left + before.cx, y, bounds.left + through.cx - tracking, y + 1};
}

static void menu_draw_row(const struct folio_window *_Nonnull self, HDC device, size_t index)
{
    RECT row = menu_row_rect(self, index);
    if (IsRectEmpty(&row))
        return;
    UINT dpi = GetDpiForWindow(self->handle);
    struct ui_part_menu_paint part = {.current = menu_current(self, index),
                                      .current_column = menu_current_column(self),
                                      .bounds = row,
                                      .dpi = dpi,
                                      .selected = self->command_selection == index,
                                      .enabled =
                                          menu_availability(self, index) == FOLIO_STATE_READY};
    struct ui_part_ink ink = ui_part_draw_menu(device, &self->palette, &part);
    char label[draw_unit_limit];
    char hint[draw_unit_limit];
    menu_label(self, index, label);
    menu_hint(self, index, hint);
    int width = measure_utf8(device, hint);
    RECT keys = ink.label;
    keys.left = keys.right - width;
    SetTextColor(device,
                 part.enabled && !part.selected ? self->palette.dialog_secondary : ink.text);
    draw_utf8(device, hint, keys);
    ink.label.right = keys.left - scale(16, dpi);
    SetTextColor(device, ink.text);
    draw_utf8(device, label, ink.label);
    if (self->focus_cues || self->focus_cues_always)
    {
        RECT underline = menu_key_rect(device, label, ink.label);
        SetDCBrushColor(device, ink.text);
        FillRect(device, &underline, (HBRUSH)GetStockObject(DC_BRUSH));
    }
}

static void menu_draw_arrows(const struct folio_window *_Nonnull self, HDC device)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    if (menu_total_height(self) <= bounds.bottom)
        return;
    int size = scale(8, GetDpiForWindow(self->handle));
    bool more_above = self->command_first > 0;
    RECT last = menu_row_rect(self, folio_menu_action_count(self->menu.context) - 1);
    bool more_below = IsRectEmpty(&last) != 0;
    POINT up[3] = {{bounds.right / 2 - size / 2, size},
                   {bounds.right / 2, size / 2},
                   {bounds.right / 2 + size / 2, size}};
    POINT down[3] = {{bounds.right / 2 - size / 2, bounds.bottom - size},
                     {bounds.right / 2, bounds.bottom - size / 2},
                     {bounds.right / 2 + size / 2, bounds.bottom - size}};
    HGDIOBJ pen = SelectObject(device, GetStockObject(NULL_PEN));
    HGDIOBJ brush = SelectObject(device, GetStockObject(DC_BRUSH));
    SetDCBrushColor(device, self->palette.dialog_secondary);
    if (more_above)
        Polygon(device, up, 3);
    if (more_below)
        Polygon(device, down, 3);
    SelectObject(device, brush);
    SelectObject(device, pen);
}

static void draw_menu_surface(const struct folio_window *_Nonnull self, HDC device)
{
    RECT bounds;
    GetClientRect(self->command_layer, &bounds);
    SetDCBrushColor(device, self->palette.dialog_surface);
    FillRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    SetDCBrushColor(device, self->palette.dialog_border);
    FrameRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    for (size_t at = self->command_first; at < folio_menu_action_count(self->menu.context); ++at)
    {
        RECT row = menu_row_rect(self, at);
        if (IsRectEmpty(&row))
            break;
        if (at > self->command_first && menu_separator(self, at) > 0)
        {
            RECT line = {scale(4, GetDpiForWindow(self->handle)),
                         row.top - menu_separator(self, at) / 2,
                         bounds.right - scale(4, GetDpiForWindow(self->handle)),
                         row.top - menu_separator(self, at) / 2 + 1};
            SetDCBrushColor(device, self->palette.dialog_border);
            FillRect(device, &line, (HBRUSH)GetStockObject(DC_BRUSH));
        }
        menu_draw_row(self, device, at);
    }
    menu_draw_arrows(self, device);
}

static size_t menu_advance(size_t at, size_t count, bool forward)
{
    if (at >= count)
        return forward ? 0 : count - 1;
    return forward ? (at + 1) % count : (at + count - 1) % count;
}

static bool menu_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    size_t count = folio_menu_action_count(self->menu.context);
    size_t at = self->command_selection;
    if (key == VK_HOME || key == VK_END)
        at = key == VK_HOME ? count - 1 : 0;
    bool forward = key == VK_DOWN || key == VK_HOME;
    for (size_t tried = 0; tried < count; ++tried)
    {
        at = menu_advance(at, count, forward);
        if (menu_availability(self, at) == FOLIO_STATE_READY)
        {
            self->command_selection = at;
            menu_reveal(self);
            redraw_command_layer(self);
            accessibility_host_schedule(self->accessibility);
            return true;
        }
    }
    self->command_selection = SIZE_MAX;
    return true;
}

static void show_menu_surface(struct folio_window *_Nonnull self,
                              const struct menu_request *_Nonnull request)
{
    if (self->removal_running || session_busy(self) || pane_streaming(self))
        return;
    if (self->command_surface != COMMAND_SURFACE_CLOSED && !hide_command_surface(self))
        return;
    self->menu = *request;
    self->menu_pressed = SIZE_MAX;
    self->menu_key = 0;
    open_command_surface(self, COMMAND_SURFACE_MENU);
    self->command_selection = SIZE_MAX;
    self->command_first = 0;
    if (request->keyboard)
        (void)menu_navigate(self, VK_HOME);
    redraw_command_layer(self);
    accessibility_host_menu(self->accessibility, ACCESSIBLE_MENU_OPENED);
}

static void menu_run_target(struct folio_window *_Nonnull self, enum folio_command command,
                            const struct menu_request *_Nonnull request)
{
    if (request->context == FOLIO_MENU_OPERATIONS || request->context == FOLIO_MENU_LAYOUT ||
        request->context == FOLIO_MENU_EMPTY)
    {
        execute_command(self, command, "");
        return;
    }
    bool previous = self->command_context;
    enum folio_command previous_command = self->reporting_command;
    self->command_context = true;
    self->reporting_command = command;
    if (command == FOLIO_COMMAND_NEW)
        new_note_at(self, request->target.category);
    if (command == FOLIO_COMMAND_RENAME)
        rename_from_drawer(self, request->target);
    if (command == FOLIO_COMMAND_RENAME_CATEGORY)
        rename_category_at(self, request->target.category, "");
    if (command == FOLIO_COMMAND_TRASH_NOTE)
        trash_note_at(self, request->target);
    if (command == FOLIO_COMMAND_DELETE_CATEGORY)
        delete_category_at(self, request->target.category);
    if (command == FOLIO_COMMAND_NEW_CATEGORY)
        execute_new_category_command(self, "");
    self->command_context = previous;
    self->reporting_command = previous_command;
}

static void menu_execute(struct folio_window *_Nonnull self, size_t index)
{
    if (self->command_surface != COMMAND_SURFACE_MENU ||
        menu_availability(self, index) != FOLIO_STATE_READY)
        return;
    struct menu_request request = self->menu;
    const struct folio_menu_action *action = menu_action(self, index);
    close_command_surface(self);
    switch (folio_menu_action_kind(action))
    {
    case FOLIO_MENU_ACTION_COMMAND:
        menu_run_target(self, folio_menu_action_command(action), &request);
        return;
    case FOLIO_MENU_ACTION_RECOLOR_CATEGORY:
        if (self->drawer != nullptr)
            drawer_window_choose_color(self->drawer, request.target.category);
        return;
    }
}

static size_t menu_access_key(const struct folio_window *_Nonnull self, WPARAM key)
{
    for (size_t at = 0; at < folio_menu_action_count(self->menu.context); ++at)
        if (key == (WPARAM)(unsigned char)folio_menu_action_access_key(menu_action(self, at)) &&
            menu_availability(self, at) == FOLIO_STATE_READY)
            return at;
    return SIZE_MAX;
}

static bool menu_key_down(struct folio_window *_Nonnull self, WPARAM key, LPARAM lparam)
{
    show_focus_cues(self, true);
    if (key == VK_ESCAPE || key == VK_MENU || key == VK_F10)
    {
        close_command_surface(self);
        return true;
    }
    if (key == VK_UP || key == VK_DOWN || key == VK_HOME || key == VK_END)
    {
        self->menu_key = 0;
        self->menu_pressed = SIZE_MAX;
        return menu_navigate(self, key);
    }
    if ((HIWORD(lparam) & KF_REPEAT) != 0)
        return true;
    if (self->menu_key != 0)
        return true;
    if (key == VK_RETURN || key == VK_SPACE)
    {
        self->menu_pressed = self->command_selection;
        self->menu_key = key;
        return true;
    }
    size_t at = menu_access_key(self, key);
    if (at != SIZE_MAX)
    {
        self->command_selection = at;
        self->menu_pressed = at;
        self->menu_key = key;
        menu_reveal(self);
        redraw_command_layer(self);
    }
    return true;
}

static void menu_mouse(struct folio_window *_Nonnull self, UINT message, POINT point)
{
    size_t hit = menu_hit(self, point);
    if (message == WM_LBUTTONDOWN)
    {
        self->menu_pressed = menu_availability(self, hit) == FOLIO_STATE_READY ? hit : SIZE_MAX;
        SetCapture(self->command_layer);
    }
    if (message == WM_LBUTTONUP)
    {
        size_t pressed = self->menu_pressed;
        self->menu_pressed = SIZE_MAX;
        if (GetCapture() == self->command_layer)
            ReleaseCapture();
        if (pressed == hit)
            menu_execute(self, hit);
        return;
    }
    self->command_selection = menu_availability(self, hit) == FOLIO_STATE_READY ? hit : SIZE_MAX;
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static void menu_global_character(struct folio_window *_Nonnull self, WPARAM wparam)
{
    if (wparam == shortcut_character_for(FOLIO_COMMAND_SAVE) ||
        wparam == shortcut_character_for(FOLIO_COMMAND_NEW) ||
        wparam == shortcut_character_for(FOLIO_COMMAND_HELP))
    {
        close_command_surface(self);
        (void)command_character(self, wparam);
    }
}

static bool menu_keyboard_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                  LPARAM lparam)
{
    switch (message)
    {
    case WM_KEYDOWN:
        if (shortcut_keydown(self, wparam))
            return true;
        return menu_key_down(self, wparam, lparam);
    case WM_SYSKEYDOWN:
        if (wparam == VK_MENU || wparam == VK_F10)
            return menu_key_down(self, wparam, lparam);
        return false;
    case WM_KEYUP:
    case WM_SYSKEYUP:
    {
        size_t at = self->menu_pressed;
        bool run = self->menu_key == wparam && self->menu_key != 0;
        self->menu_key = 0;
        self->menu_pressed = SIZE_MAX;
        if (run)
            menu_execute(self, at);
        return true;
    }
    case WM_CHAR:
    case WM_SYSCHAR:
        menu_global_character(self, wparam);
        return true;
    default:
        return false;
    }
}

static bool menu_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                         LPARAM lparam)
{
    switch (message)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
        return menu_keyboard_message(self, message, wparam, lparam);
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MOUSEMOVE:
        menu_mouse(self, message, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return true;
    case WM_RBUTTONDOWN:
        close_command_surface(self);
        return true;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        self->menu_pressed = SIZE_MAX;
        self->menu_key = 0;
        return true;
    case WM_MOUSEWHEEL:
        return menu_navigate(self, GET_WHEEL_DELTA_WPARAM(wparam) > 0 ? VK_UP : VK_DOWN);
    default:
        return false; /* Win32 messages are an open set. */
    }
}

static void menu_command_accelerator(const struct folio_window *_Nonnull self,
                                     enum folio_command command,
                                     struct accessible_view *_Nonnull out)
{
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_action(binding) == FOLIO_SHORTCUT_ACTION_COMMAND &&
            folio_shortcut_command(binding) == command)
        {
            (void)wide_units(folio_shortcut_label(binding, folio_state_language(self->state)),
                             out->accelerator);
            break;
        }
    }
}

static void menu_accessible(const struct folio_window *_Nonnull self, size_t index,
                            struct accessible_view *_Nonnull out)
{
    char caption[draw_unit_limit];
    menu_caption(self, index, caption);
    (void)wide_units(caption, out->name);
    char key[2] = {folio_menu_action_access_key(menu_action(self, index)), '\0'};
    (void)wide_units(key, out->access_key);
    out->position = (int)index + 1;
    out->set_size = (int)folio_menu_action_count(self->menu.context);
    const struct folio_menu_action *action = menu_action(self, index);
    if (folio_menu_action_kind(action) != FOLIO_MENU_ACTION_COMMAND)
        return;
    enum folio_command command = folio_menu_action_command(action);
    menu_command_accelerator(self, command, out);
    if (menu_current(self, index))
        (void)wide_units(
            ui_text_line(UI_TEXT_ACCESSIBLE_CURRENT, folio_state_language(self->state)),
            out->status);
    if (command == FOLIO_COMMAND_TRASH_NOTE)
        (void)wide_units(ui_text_line(UI_TEXT_HELP_RECYCLE, folio_state_language(self->state)),
                         out->help);
    if (command == FOLIO_COMMAND_NEW)
    {
        struct ui_text_request request = {
            .id = UI_TEXT_TOOLTIP_NEW_NOTE,
            .language = folio_state_language(self->state),
            .name = folio_state_category_name(self->state, self->menu.target.category),
            .from = folio_shortcut_command_hint(command, folio_state_language(self->state))};
        if (ui_text_format(&request, caption, draw_unit_limit) == UI_TEXT_FORMAT_READY)
            (void)wide_units(caption, out->help);
    }
    if (command == FOLIO_COMMAND_VIEW || command == FOLIO_COMMAND_REPLACE)
    {
        menu_hint(self, index, caption);
        (void)wide_units(caption, out->help);
    }
}

/* 最外の同期入力dispatchが戻るまで終了要求を実行しない（ADR0064）。 */
static LRESULT CALLBACK command_input_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    struct folio_window *self = self_of(window);
    if (self == nullptr)
        return command_input_message(window, message, wparam, lparam);
    self->dispatch_depth += 1;
    LRESULT result = command_input_message(window, message, wparam, lparam);
    dispatch_finished(self);
    return result;
}

/* 最外の同期入力dispatchが戻るまで終了要求を実行しない（ADR0064）。 */
static LRESULT CALLBACK filter_input_procedure(HWND window, UINT message, WPARAM wparam,
                                               LPARAM lparam)
{
    struct folio_window *self = self_of(window);
    if (self == nullptr)
        return filter_input_message(window, message, wparam, lparam);
    self->dispatch_depth += 1;
    LRESULT result = filter_input_message(window, message, wparam, lparam);
    dispatch_finished(self);
    return result;
}

/* 最外の同期入力dispatchが戻るまで終了要求を実行しない（ADR0064）。 */
static LRESULT CALLBACK command_layer_procedure(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    struct folio_window *self = self_of(window);
    if (self == nullptr)
        return command_layer_message(window, message, wparam, lparam);
    self->dispatch_depth += 1;
    LRESULT result = command_layer_message(window, message, wparam, lparam);
    dispatch_finished(self);
    return result;
}

/* 最外の同期入力dispatchが戻るまで終了要求を実行しない（ADR0064）。 */
static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct folio_window *self = self_of(window);
    if (self == nullptr)
        return window_message(window, message, wparam, lparam);
    self->dispatch_depth += 1;
    LRESULT result = window_message(window, message, wparam, lparam);
    dispatch_finished(self);
    return result;
}
