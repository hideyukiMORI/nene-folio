#include "folio_window.h"
#include "accessibility_host.h"
#include "accessibility_node.h"
#include "accessible_main_region.h"
#include "accessible_request.h"
#include "accessible_surface_button.h"
#include "accessible_view.h"
#include "bounded_text.h"
#include "dropped_files.h"
#include "file_picker.h"
#include "folio_open_link.h"
#include "image_adoption.h"
#include "image_adoption_notice.h"
#include "name_list.h"
#include "open_place_kind.h"
#include "open_request.h"
#include "request_prompt.h"
#include "storage_path.h"
#include "text_buffer.h"
#include <stdio.h>

#include "ascii_fold.h"
#include "breadcrumb_layout.h"
#include "category_name.h"
#include "close_phase.h"
#include "code_copy_target.h"
#include "command_band_layout.h"
#include "command_candidates.h"
#include "command_help_fonts.h"
#include "command_help_metrics.h"
#include "command_row.h"
#include "command_surface_metrics.h"
#include "command_surface_mode.h"
#include "discovery_focus.h"
#include "discovery_layout.h"
#include "discovery_target.h"
#include "discovery_text.h"
#include "drawer_menu_item.h"
#include "drawer_width_range.h"
#include "drawer_window.h"
#include "failure_box.h"
#include "folio_command.h"
#include "folio_idle_work.h"
#include "folio_menu_action.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_regions.h"
#include "folio_shortcut.h"
#include "folio_state.h"
#include "folio_step.h"
#include "folio_theme_choice.h"
#include "footnote_target.h"
#include "history_row_view.h"
#include "icon_paint.h"
#include "key_guide.h"
#include "layout_metrics.h"
#include "markdown_media.h"
#include "menu_request.h"
#include "minimap_update.h"
#include "modal_owner.h"
#include "name_prompt.h"
#include "name_prompt_metrics.h"
#include "note_excerpt.h"
#include "note_identity.h"
#include "note_metadata.h"
#include "note_minimap.h"
#include "note_name.h"
#include "note_outline.h"
#include "note_pane.h"
#include "note_ref.h"
#include "note_save_mode.h"
#include "note_search.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "pane_notice.h"
#include "product_version.h"
#include "replace_edit.h"
#include "session_phase.h"
#include "settings_row_kind.h"
#include "storage_prompt.h"
#include "storage_state.h"
#include "tag_candidates.h"
#include "tag_candidates_source.h"
#include "tag_catalog.h"
#include "tag_filter_text.h"
#include "tag_management_view.h"
#include "tag_surface_focus.h"
#include "tag_update.h"
#include "text_draw_request.h"
#include "text_wrap.h"
#include "theme_candidates.h"
#include "theme_row.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_part_metrics.h"
#include "ui_pointer.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include "window_restore.h"

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

constexpr size_t surface_button_count = 30;
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
    HICON _Nullable icon_large;
    HICON _Nullable icon_small;
    struct folio_state *_Nonnull state;
    struct storage_state *_Nonnull storage;
    struct accessibility_host *_Nullable accessibility;
    enum accessible_kind keyboard_target;
    bool menu_active;
    struct menu_request menu;
    /* #1012: whether the body menu's source items can run, measured once when it opens. */
    bool menu_source_copy;
    bool menu_source_edit;
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
    struct note_minimap *_Nullable minimap;
    struct note_outline *_Nullable outline; /* #975: the headings column's own state. */
    bool outline_carved;             /* the body was last placed with room left for the column */
    size_t outline_press;            /* the column's row the pointer went down on */
    uint64_t outline_press_revision; /* ...in this load of the note */
    int outline_wheel;               /* wheel movement over the column not yet worth a row */
    enum folio_idle_work next_idle_work;
    /* #1062 / ADR0120: 落とされた画像の取り込みが走っているあいだの一時の状態。 */
    bool adopting;
    bool adopting_enabled; /* 入力を止める前に、主窓が有効だったか */
    size_t drop_position;  /* 落とした点の、本文の文字の位置 */
    size_t drop_lost;      /* 落とされたうち、パスにできなかった数 */
    uint64_t image_ticket;
    uint64_t image_origin_revision;
    size_t image_index;
    bool image_pending;
    bool image_failed;
    bool image_advisory;
    enum ui_text image_line;
    enum ui_text diagram_failure;
    unsigned image_meter;
    int minimap_pointer_offset;
    int body_left_inset; /* 配置時に確定した行番号込みの左余白。通知中は再計測しない。 */
    HACCEL _Nullable commands;
    /* 検索欄にいるあいだだけ使う Enter / Shift+Enter（ADR 0023 の決定 4） */
    HACCEL _Nullable search_keys;
    HFONT _Nullable mono_font;
    /* 言葉（釦の名前など）を描く、言語ごとの同梱の書体（#1222）。番号・日時・キーは mono_font。 */
    HFONT _Nullable word_font;
    struct command_help_fonts help_fonts;
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
    HWND _Nullable discovery_preview;
    WNDPROC _Nullable discovery_preview_original;
    enum discovery_focus discovery_focus;
    enum discovery_mode discovery_mode_focus;
    size_t discovery_tag;
    bool discovery_show_preview;
    bool discovery_preview_ready;
    bool discovery_preview_reveal;
    /* #941: the destination picker. The note to move, and the categories whose name contains
     * the query (indexes into the category list, in its order). */
    struct note_ref move_source;
    size_t *_Nullable move_rows;
    size_t move_count;
    size_t move_capacity;
    /* 折り返し表示の行番号用。原文の各論理行がpreviewの何文字目から始まるか（#933）。 */
    size_t discovery_line_starts[note_excerpt_line_limit];
    size_t discovery_line_count;
    uint64_t discovery_revision;
    uint64_t discovery_results_revision;
    UINT_PTR discovery_timer;
    unsigned char discovery_open_frames;
    unsigned char discovery_selection_frames;
    unsigned char discovery_busy_tick;
    unsigned char discovery_busy_delay;
    bool discovery_busy;
    bool discovery_motion_wait;
    bool discovery_motion_failed;
    uint64_t discovery_motion_revision;
    RECT discovery_motion_row;
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
    enum folio_state_outcome tag_strip_outcome;
    size_t tag_strip_count;
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
    struct tag_candidates *_Nullable tag_candidates;
    enum tag_surface_focus tag_focus;
    enum tag_management_view tag_management_view;
    struct note_tag *_Nullable management_source;
    bool management_merging;
    enum pane_mode tag_origin_mode;
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
    enum ui_text open_line; /* PANE_NOTICE_OPEN_LINK の文（#1071。#1070 の外の変更の 1 行も） */
    bool outside_away;      /* ほかのアプリへ移っている（WM_ACTIVATEAPP の FALSE から TRUE まで） */
    bool outside_settling;  /* data の変化の知らせをまとめている（outside_timer が鳴るまで） */
    /* #1152 / ADR0119: 依頼の面で答えてもらった。次に依頼を見るときは、たまった中のいちばん新しい
     * 1 つだけを扱う。 */
    bool request_answered;
    bool outside_due; /* そこから戻った。静かになったら data を読み直す（ADR0119 の決定 1・2） */
    char *_Nullable queued_link; /* 預かった「開く」依頼。メッセージの待ちの先頭で開く */
    size_t queued_length;
    bool session_reason_owned;
    bool layout_changing;     /* 同期WM_SIZEを含む一つの表示切替の間だけ立つ。 */
    int drawer_width;         /* 実行中に選んだlogical幅。窓縮小時の制限で上書きしない。 */
    int split_origin_width;   /* pressed_part == SPLITTERの間だけ有効な取消先。 */
    int split_pointer_offset; /* drag開始時のroot座標と境界との差（device px）。 */
    int wide_width;           /* DRAWER直前の通常幅（物理px、wide_dpiのとき。#936）。 */
    UINT wide_dpi;            /* wide_widthを測ったときの窓のDPI。 */
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

constexpr UINT_PTR minimap_timer = 84;
constexpr UINT_PTR image_timer = 85;
constexpr UINT_PTR discovery_motion_timer = 86;
/* data の変化の知らせをまとめる。最後の知らせからこの間を置いて 1 回だけ読み直す（ADR0119）。 */
constexpr UINT_PTR outside_timer = 87;
constexpr UINT outside_settle = 300;
/* #928: twice the first lengths (8 and 6 frames), which were too quick to be seen. */
constexpr unsigned char discovery_open_frame_count = 16;
constexpr unsigned char discovery_selection_frame_count = 12;
constexpr UINT minimap_delay = 120;
constexpr int base_minimap_width = 64;
constexpr int base_minimap_gap = 8;
constexpr int base_minimap_right = 12;
constexpr int base_minimap_minimum_body = 320;
/* #975 / ADR0114: the headings column, between the body and the minimap. */
constexpr int base_outline_width = 184;
constexpr int base_outline_gap = 12;

static const wchar_t *const class_name = folio_open_link_class;
static const wchar_t command_layer_class[] = L"NeNeFolioCommandLayer";
static const wchar_t settings_backdrop_class[] = L"NeNeFolioSettingsBackdrop";
static const wchar_t edit_class[] = L"EDIT";

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
    enum accessible_surface_button button;
} settings_rows[] = {
    {UI_TEXT_SETTINGS_THEME, SETTINGS_ROW_HEADING, 0, 0, 0, ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_THEME_CHOOSE, SETTINGS_ROW_THEME, 0, 0, 0, ACCESSIBLE_SURFACE_THEME},
    {UI_TEXT_SETTINGS_LANGUAGE, SETTINGS_ROW_HEADING, 0, 1, 0, ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_LANGUAGE_JA, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_JA, 1, 0,
     ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_LANGUAGE_EN, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_EN, 1, 1,
     ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS, SETTINGS_ROW_LANGUAGE, FOLIO_LANGUAGE_ZH_HANS, 1, 2,
     ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_NUMBER_CHECK, SETTINGS_ROW_NUMBER, 1, 2, 0, ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_MINIMAP, SETTINGS_ROW_MINIMAP, 1, 3, 0, ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_FAVORITES_FIRST, SETTINGS_ROW_FAVORITES_FIRST, 1, 4, 0,
     ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_MOTION, SETTINGS_ROW_MOTION, 1, 5, 0, ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_SETTINGS_WINDOW, SETTINGS_ROW_WINDOW, 1, 6, 0, ACCESSIBLE_SURFACE_CLOSE},
    {UI_TEXT_STORAGE_ENTRY, SETTINGS_ROW_STORAGE, 0, 7, 0, ACCESSIBLE_SURFACE_STORAGE},
    {UI_TEXT_ABOUT_ENTRY, SETTINGS_ROW_ABOUT, 0, 8, 0, ACCESSIBLE_SURFACE_ABOUT},
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

/* 自前操作の親子順・名前・focus可否と、表示名・UIA Name・AcceleratorKey・tooltip の 1 つの元
 * （ADR0060 の決定 5）。key は AcceleratorKey と tooltip に、hint は tooltip だけに出る。
 * native入力欄はこのtreeに複製しない。 */
static const struct
{
    enum accessible_kind parent;
    enum ui_text label;
    bool focusable;
    enum ui_text hint;
    enum accessible_main_region region;
} accessible_controls[] = {
    [ACCESSIBLE_WINDOW] = {ACCESSIBLE_WINDOW, UI_TEXT_APP_LOGO, false, UI_TEXT_EMPTY,
                           ACCESSIBLE_MAIN_GROUP},
    [ACCESSIBLE_DRAWER] = {ACCESSIBLE_WINDOW, UI_TEXT_TITLE_CATEGORIES, false, UI_TEXT_EMPTY,
                           ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SPLITTER] = {ACCESSIBLE_WINDOW, UI_TEXT_DRAWER_WIDTH, true,
                             UI_TEXT_DRAWER_WIDTH_HELP, ACCESSIBLE_MAIN_RANGE},
    [ACCESSIBLE_HEADER] = {ACCESSIBLE_WINDOW, UI_TEXT_ACCESSIBLE_HEADER, false, UI_TEXT_EMPTY,
                           ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_NEW_NOTE] = {ACCESSIBLE_HEADER, UI_TEXT_COMMAND_NEW, true, UI_TEXT_EMPTY,
                             ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_LAYOUT_GROUP] = {ACCESSIBLE_HEADER, UI_TEXT_LAYOUT_GROUP, false, UI_TEXT_EMPTY,
                                 ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_LAYOUT_DRAWER] = {ACCESSIBLE_LAYOUT_GROUP, UI_TEXT_LAYOUT_BUTTON_DRAWER, true,
                                  UI_TEXT_LAYOUT_NARROW_HINT, ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_LAYOUT_BOTH] = {ACCESSIBLE_LAYOUT_GROUP, UI_TEXT_LAYOUT_BUTTON_BOTH, true,
                                UI_TEXT_EMPTY, ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_LAYOUT_NOTE] = {ACCESSIBLE_LAYOUT_GROUP, UI_TEXT_LAYOUT_BUTTON_NOTE, true,
                                UI_TEXT_EMPTY, ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_LAYOUT_CHOOSER] = {ACCESSIBLE_HEADER, UI_TEXT_LAYOUT_GROUP, true, UI_TEXT_EMPTY,
                                   ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_MINIMIZE] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_MINIMIZE, true, UI_TEXT_EMPTY,
                             ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_MAXIMIZE] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_MAXIMIZE, true, UI_TEXT_EMPTY,
                             ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_HELP] = {ACCESSIBLE_HEADER, UI_TEXT_COMMAND_HELP, true, UI_TEXT_TOOLTIP_HELP,
                         ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_SETTINGS] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_SETTINGS, true, UI_TEXT_EMPTY,
                             ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_CLOSE] = {ACCESSIBLE_HEADER, UI_TEXT_TOOLTIP_CLOSE, true, UI_TEXT_EMPTY,
                          ACCESSIBLE_MAIN_HEADER},
    [ACCESSIBLE_PANE_TOOLS] = {ACCESSIBLE_WINDOW, UI_TEXT_ACCESSIBLE_PANE_TOOLS, false,
                               UI_TEXT_EMPTY, ACCESSIBLE_MAIN_GROUP},
    [ACCESSIBLE_BREADCRUMB] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                               ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_MODE_GROUP] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_ACCESSIBLE_MODE, false, UI_TEXT_EMPTY,
                               ACCESSIBLE_MAIN_GROUP},
    [ACCESSIBLE_VIEW] = {ACCESSIBLE_MODE_GROUP, UI_TEXT_CHIP_VIEW, true,
                         UI_TEXT_TOOLTIP_TOGGLE_MODE, ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_EDIT] = {ACCESSIBLE_MODE_GROUP, UI_TEXT_CHIP_EDIT, true,
                         UI_TEXT_TOOLTIP_TOGGLE_MODE, ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_ACTION_GROUP] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_ACCESSIBLE_NOTE_ACTIONS, false,
                                 UI_TEXT_EMPTY, ACCESSIBLE_MAIN_GROUP},
    [ACCESSIBLE_SAVE] = {ACCESSIBLE_ACTION_GROUP, UI_TEXT_COMMAND_SAVE, true, UI_TEXT_COMMAND_SAVE,
                         ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_FIND] = {ACCESSIBLE_ACTION_GROUP, UI_TEXT_COMMAND_FIND, true, UI_TEXT_COMMAND_FIND,
                         ACCESSIBLE_MAIN_PANE},
    /* 名前は「操作」。▾は描く字（UI_TEXT_ACTION_OPERATIONS）だけが持つ表示の記号。 */
    [ACCESSIBLE_MENU] = {ACCESSIBLE_ACTION_GROUP, UI_TEXT_ACCESSIBLE_OPERATIONS, true,
                         UI_TEXT_TOOLTIP_OPERATIONS, ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_TAG_EDIT] = {ACCESSIBLE_PANE_TOOLS, UI_TEXT_COMMAND_EDIT_TAGS, true, UI_TEXT_EMPTY,
                             ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_TAG_FILTER] = {ACCESSIBLE_DRAWER, UI_TEXT_COMMAND_FILTER_TAGS, true, UI_TEXT_EMPTY,
                               ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SCOPE_GROUP] = {ACCESSIBLE_DRAWER, UI_TEXT_SCOPE_GROUP, false, UI_TEXT_EMPTY,
                                ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SCOPE_ALL] = {ACCESSIBLE_SCOPE_GROUP, UI_TEXT_SCOPE_ALL, true, UI_TEXT_EMPTY,
                              ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SCOPE_FAVORITES] = {ACCESSIBLE_SCOPE_GROUP, UI_TEXT_TITLE_FAVORITES, true,
                                    UI_TEXT_EMPTY, ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_NEW_CATEGORY] = {ACCESSIBLE_DRAWER, UI_TEXT_COMMAND_NEW_CATEGORY, true,
                                 UI_TEXT_EMPTY, ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_TREE] = {ACCESSIBLE_DRAWER, UI_TEXT_TITLE_CATEGORIES, true, UI_TEXT_EMPTY,
                         ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_CATEGORY] = {ACCESSIBLE_TREE, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                             ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_NOTE] = {ACCESSIBLE_CATEGORY, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                         ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_FILTER] = {ACCESSIBLE_WINDOW, UI_TEXT_PLACEHOLDER_FILTER, true, UI_TEXT_EMPTY,
                           ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_BODY] = {ACCESSIBLE_WINDOW, UI_TEXT_ACCESSIBLE_BODY, true, UI_TEXT_EMPTY,
                         ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_OUTLINE] = {ACCESSIBLE_WINDOW, UI_TEXT_OUTLINE, true, UI_TEXT_OUTLINE_HELP,
                            ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_OUTLINE_ITEM] = {ACCESSIBLE_OUTLINE, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                                 ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_MINIMAP] = {ACCESSIBLE_WINDOW, UI_TEXT_MINIMAP, true, UI_TEXT_MINIMAP_HELP,
                            ACCESSIBLE_MAIN_RANGE},
    [ACCESSIBLE_NOTICE] = {ACCESSIBLE_WINDOW, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                           ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_GUIDE] = {ACCESSIBLE_WINDOW, UI_TEXT_EMPTY_HEADING, false, UI_TEXT_EMPTY,
                          ACCESSIBLE_MAIN_PANE},
    [ACCESSIBLE_SURFACE] = {ACCESSIBLE_WINDOW, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                            ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_TABS] = {ACCESSIBLE_SURFACE, UI_TEXT_COMMAND_HELP, false, UI_TEXT_EMPTY,
                                 ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_TAB] = {ACCESSIBLE_SURFACE_TABS, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                                ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_LIST] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                                 ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_CHOICE] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                                   ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_LANGUAGE] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                                     ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_TOGGLE] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                                   ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_BUTTON] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                                   ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_TEXT] = {ACCESSIBLE_SURFACE_LIST, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                                 ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_HELP] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                                 ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_SURFACE_STATUS] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, false, UI_TEXT_EMPTY,
                                   ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_COMMAND_INPUT] = {ACCESSIBLE_SURFACE, UI_TEXT_EMPTY, true, UI_TEXT_EMPTY,
                                  ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_REPLACE_PATTERN] = {ACCESSIBLE_SURFACE, UI_TEXT_ACCESSIBLE_PATTERN, true,
                                    UI_TEXT_EMPTY, ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_REPLACE_TEXT] = {ACCESSIBLE_SURFACE, UI_TEXT_ACCESSIBLE_REPLACEMENT, true,
                                 UI_TEXT_EMPTY, ACCESSIBLE_MAIN_NONE},
    [ACCESSIBLE_DISCOVERY_PREVIEW] = {ACCESSIBLE_SURFACE, UI_TEXT_DISCOVERY_PREVIEW, true,
                                      UI_TEXT_EMPTY, ACCESSIBLE_MAIN_NONE},
};

/* 主窓のpointer・hover・tooltip対象。buttonの様式は各描画がui_partへ渡す。 */
static const enum accessible_kind main_parts[] = {
    ACCESSIBLE_NEW_NOTE,    ACCESSIBLE_LAYOUT_DRAWER,  ACCESSIBLE_LAYOUT_BOTH,
    ACCESSIBLE_LAYOUT_NOTE, ACCESSIBLE_LAYOUT_CHOOSER, ACCESSIBLE_HELP,
    ACCESSIBLE_SETTINGS,    ACCESSIBLE_MINIMIZE,       ACCESSIBLE_MAXIMIZE,
    ACCESSIBLE_CLOSE,       ACCESSIBLE_VIEW,           ACCESSIBLE_EDIT,
    ACCESSIBLE_SAVE,        ACCESSIBLE_FIND,           ACCESSIBLE_MENU,
    ACCESSIBLE_SPLITTER,    ACCESSIBLE_MINIMAP,        ACCESSIBLE_TAG_EDIT,
    ACCESSIBLE_OUTLINE,
};

static size_t main_part_count(void)
{
    return sizeof main_parts / sizeof main_parts[0];
}

static bool main_part(enum accessible_kind kind)
{
    for (size_t index = 0; index < main_part_count(); ++index)
    {
        if (main_parts[index] == kind)
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

/* 設定buttonの公開IDと実際の行を同じ記述子で往復させる。 */
static size_t settings_button_index(size_t slot)
{
    for (size_t index = 0; index < settings_row_count(); ++index)
    {
        enum settings_row_kind kind = settings_rows[index].kind;
        bool entry = kind == SETTINGS_ROW_THEME || kind == SETTINGS_ROW_STORAGE ||
                     kind == SETTINGS_ROW_ABOUT;
        if (entry && settings_rows[index].button == slot)
            return index;
    }
    return settings_row_count();
}

static bool settings_button_item(struct accessible_item item)
{
    return item.kind == ACCESSIBLE_SURFACE_BUTTON &&
           settings_button_index(item.slot) < settings_row_count();
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
constexpr int discovery_preview_control_id = 6;
constexpr size_t discovery_input_capacity = 514;
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
/* #891: how much of the header's width must be on a screen to grab a restored window. */
constexpr int base_window_grip_width = 160;
constexpr int base_min_height = 360;
constexpr int base_drawer_width = 240;
constexpr int base_drawer_min_width = 200;
constexpr int base_pane_min_width = 320;
constexpr int base_splitter_width = 8;
constexpr int base_splitter_small_change = 4;
constexpr int base_splitter_large_change = 32;
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
constexpr int base_tag_height = 28;
constexpr int base_pane_left = 36;
constexpr int base_pane_right = 56;
constexpr int base_pane_top = 16;
constexpr int base_pane_bottom = 32;
constexpr int base_mono_font = 11;
/* 言葉の書体の大きさ。左の欄の数の単位と同じ 11（#1222）。 */
constexpr int base_word_font = 11;
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
/* 数字を測れなかったときの目安（96 DPI の 等幅の書体（Consolas 11px・IBM Plex Mono
 * 10px）の実寸に近い値）。 */
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
constexpr int base_settings_height = 440;
constexpr int base_theme_height = 560;
constexpr int base_theme_row_height = 32;
constexpr int base_theme_choice_height = 32;
constexpr int base_settings_padding = 16;
constexpr int base_settings_label = 24;
constexpr int base_settings_item_height = 32;
constexpr int base_settings_item_gap = 8;
constexpr int base_settings_group_gap = 20;

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
static bool text_menu_context(enum folio_menu_context context);
static bool menu_navigate(struct folio_window *_Nonnull self, WPARAM key);
static enum folio_state_outcome menu_availability(const struct folio_window *_Nonnull self,
                                                  size_t index);
static void menu_accessible(const struct folio_window *_Nonnull self, size_t index,
                            struct accessible_view *_Nonnull out);
static void report_outcome(struct folio_window *_Nonnull self, enum folio_state_outcome outcome);
static bool current_unsaved(const struct folio_window *_Nonnull self, uint64_t revision,
                            bool *_Nonnull out);
static void files_dropped(struct folio_window *_Nonnull self, HDROP _Nullable drop,
                          size_t position);
[[nodiscard]] static bool preview_theme(struct folio_window *_Nonnull self,
                                        enum folio_theme_choice choice);
static bool refresh_band_layout(struct folio_window *_Nonnull self);
static enum ui_text surface_button_label(const struct folio_window *_Nonnull self, size_t slot);
static bool surface_button_visible(const struct folio_window *_Nonnull self, size_t slot);
static enum ui_pointer_kind surface_cursor(const struct folio_window *_Nonnull self, POINT point);
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
/* The commands that show or hide a part beside the body (the minimap, the headings column). */
static bool view_toggle_command(enum folio_command command)
{
    return command == FOLIO_COMMAND_TOGGLE_MINIMAP || command == FOLIO_COMMAND_TOGGLE_OUTLINE;
}
static void apply_drawer_width(struct folio_window *_Nonnull self, int width);
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
static BYTE pressed_shortcut_modifiers(void);
static enum accessible_kind mode_kind(enum pane_mode mode);
static bool minimap_visible(const struct folio_window *_Nonnull self);
static void track_outline(const struct folio_window *_Nonnull self);
static bool minimap_available(const struct folio_window *_Nonnull self);
static bool outline_available(const struct folio_window *_Nonnull self);
static bool outline_visible(const struct folio_window *_Nonnull self);
static RECT outline_rect(const struct folio_window *_Nonnull self);
static RECT minimap_rect(const struct folio_window *_Nonnull self);
static void request_minimap(struct folio_window *_Nonnull self, enum minimap_update update);
static bool part_focus_shown(const struct folio_window *_Nonnull self, enum accessible_kind kind);
static bool accessible_visible(const struct folio_window *_Nonnull self, enum accessible_kind kind);
static HWND _Nullable accessible_input_window(const struct folio_window *_Nonnull self,
                                              enum accessible_kind kind);
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
    /* 画像の取り込みの途中（#1062）も、鍵と変換を本文へ通さない: 落とした位置を動かさない。 */
    return (session_busy(self) || self->adopting || self->native_modal_depth > 0 ||
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
    if (kind == ACCESSIBLE_MINIMAP)
        return minimap_available(self);
    if (kind == ACCESSIBLE_OUTLINE)
        return outline_available(self);
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
    if (session_busy(self) || self->adopting)
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
static void show_outside_change(struct folio_window *_Nonnull self);
static void open_requested(struct folio_window *_Nonnull self);
static void open_link_notice(struct folio_window *_Nonnull self, enum ui_text line);
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
static LRESULT on_copy_data(struct folio_window *_Nonnull self, LPARAM lparam);

static int scale(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, base_dpi);
}

/* 窓縮小は実幅だけを制限する。最大は切り捨て、DPIへの再投影で本文最小幅を侵さない。 */
static struct drawer_width_range drawer_width_for(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int available = client.right - scale(base_pane_min_width, dpi);
    int maximum = available > 0 ? (int)((int64_t)available * base_dpi / dpi) : 0;
    int minimum = maximum < base_drawer_min_width ? maximum : base_drawer_min_width;
    int width = self->drawer_width < minimum ? minimum : self->drawer_width;
    width = width > maximum ? maximum : width;
    return (struct drawer_width_range){.minimum = minimum, .maximum = maximum, .value = width};
}

/* 表示/配置/paint/hitが同じclient・DPI・application値から領域を導く。 */
static struct folio_regions regions_for(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int top = scale(base_common_header_height, dpi);
    struct folio_regions regions = {
        .header = {0, 0, client.right, top},
        .drawer = {0, top, 0, client.bottom - scale(base_pane_bottom, dpi)},
        .pane = {0, top, client.right, client.bottom}};
    switch (folio_state_layout_mode(self->state))
    {
    case FOLIO_LAYOUT_BOTH:
        regions.drawer.right = scale(drawer_width_for(self).value, dpi);
        regions.pane.left = regions.drawer.right;
        regions.splitter =
            (RECT){regions.pane.left, top, regions.pane.left + scale(base_splitter_width, dpi),
                   regions.drawer.bottom};
        if (regions.splitter.right > client.right)
            regions.splitter.right = client.right;
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

/* #941: the destination picker shares the settings frame and its metrics, like the theme
 * picker, but it is opened on its own and closes on Esc. */
static bool move_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_MOVE;
}

static bool settings_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_SETTINGS ||
           self->command_surface == COMMAND_SURFACE_THEMES ||
           self->command_surface == COMMAND_SURFACE_ABOUT || move_surface(self);
}

/* The two pickers that are a query field over filtered rows (themes, destinations). */
static bool choice_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_THEMES || move_surface(self);
}

static bool tag_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_TAG_EDIT ||
           self->command_surface == COMMAND_SURFACE_TAG_FILTER ||
           self->command_surface == COMMAND_SURFACE_TAG_MANAGE;
}

static bool discovery_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_DISCOVERY;
}

static bool management_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_TAG_MANAGE;
}

static bool management_running(const struct folio_window *_Nonnull self)
{
    enum tag_management_phase phase = folio_state_tag_management_phase(self->state);
    return management_surface(self) &&
           (phase == TAG_MANAGEMENT_PREPARING || phase == TAG_MANAGEMENT_APPLYING);
}

static bool management_input(const struct folio_window *_Nonnull self)
{
    return management_surface(self) && self->tag_management_view != TAG_MANAGEMENT_PLAN;
}

static bool framed_surface(const struct folio_window *_Nonnull self)
{
    return settings_surface(self) || tag_surface(self) || discovery_surface(self);
}

static struct discovery_layout discovery_geometry(const struct folio_window *_Nonnull self);
static RECT discovery_row_rect(const struct folio_window *_Nonnull self, size_t index);
static bool create_discovery_preview(struct folio_window *_Nonnull self);
static void draw_discovery(const struct folio_window *_Nonnull self, HDC device, RECT bounds);
static void arrange_discovery_preview(struct folio_window *_Nonnull self);
static void discovery_refresh(struct folio_window *_Nonnull self);
static void discovery_query_changed(struct folio_window *_Nonnull self);
static void discovery_open_selected(struct folio_window *_Nonnull self, bool edit);
static void discovery_choose_mode(struct folio_window *_Nonnull self, size_t slot);
static void discovery_set_focus(struct folio_window *_Nonnull self, enum discovery_focus focus);
static void discovery_paste(struct folio_window *_Nonnull self);
static void paint_input_placeholder(const struct folio_window *_Nonnull self, HWND window,
                                    enum ui_text label, const char *_Nonnull hint);
static void click_discovery(struct folio_window *_Nonnull self, POINT point);
static void discovery_control_view(const struct folio_window *_Nonnull self,
                                   struct accessible_item item,
                                   struct accessible_view *_Nonnull out);
static HRESULT discovery_button_focus(struct folio_window *_Nonnull self, size_t slot);
static void discovery_status_text(const struct folio_window *_Nonnull self, char *_Nonnull out);
static bool discovery_accessible_name(const struct folio_window *_Nonnull self,
                                      enum accessible_kind kind, char16_t *_Nonnull out);
static void execute_discovery(struct folio_window *_Nonnull self, const char *_Nonnull argument);
static void execute_move_note(struct folio_window *_Nonnull self, const char *_Nonnull argument);
static void show_move_surface(struct folio_window *_Nonnull self, struct note_ref source);
static bool discovery_navigate(struct folio_window *_Nonnull self, WPARAM key);
static bool discovery_switch_mode(struct folio_window *_Nonnull self, WPARAM key);
static void move_query_changed(struct folio_window *_Nonnull self);
static void adopt_move_at(struct folio_window *_Nonnull self, size_t index);
static bool move_navigate(struct folio_window *_Nonnull self, WPARAM key);
static void click_move_surface(struct folio_window *_Nonnull self, POINT point);
static bool move_row_text(const struct folio_window *_Nonnull self, size_t index,
                          char *_Nonnull out, size_t capacity);
static bool move_row_current(const struct folio_window *_Nonnull self, size_t index);
static void draw_move_dialog(const struct folio_window *_Nonnull self, HDC device, RECT bounds);
static enum discovery_focus discovery_current_focus(const struct folio_window *_Nonnull self);
static bool discovery_focus_visible(const struct folio_window *_Nonnull self,
                                    enum discovery_focus focus);
static bool discovery_tab(struct folio_window *_Nonnull self);
static bool discovery_return(struct folio_window *_Nonnull self);
static void discovery_select(struct folio_window *_Nonnull self, size_t index);
static void discovery_motion_settle(struct folio_window *_Nonnull self);
static void discovery_motion_reset(struct folio_window *_Nonnull self);
static void discovery_motion_sync(struct folio_window *_Nonnull self);
static void discovery_motion_tick(struct folio_window *_Nonnull self);
static void discovery_motion_select(struct folio_window *_Nonnull self);
static bool discovery_motion_allowed(const struct folio_window *_Nonnull self);
static void discovery_motion_invalidate(const struct folio_window *_Nonnull self);
static void discovery_action(struct folio_window *_Nonnull self, size_t slot);
static bool discovery_button_enabled(const struct folio_window *_Nonnull self, size_t slot);
static bool discovery_button_visible(const struct folio_window *_Nonnull self, size_t slot);
static enum ui_text discovery_button_label(const struct folio_window *_Nonnull self, size_t slot);
static RECT discovery_button_rect(const struct folio_window *_Nonnull self, size_t slot);
static size_t discovery_focus_slot(enum discovery_focus focus);
static LRESULT CALLBACK discovery_preview_procedure(HWND window, UINT message, WPARAM wparam,
                                                    LPARAM lparam);

static HRESULT prepare_tag_candidates(const struct folio_window *_Nonnull self);
static size_t tag_focus_slot(enum tag_surface_focus focus);
static void draw_floating_header(const struct folio_window *_Nonnull self, HDC device,
                                 enum ui_text label);
static size_t tag_matches_count(const struct folio_window *_Nonnull self);
static void tag_query_changed(struct folio_window *_Nonnull self);
static void tag_toggle_row(struct folio_window *_Nonnull self);
static void tag_add_input(struct folio_window *_Nonnull self);
static void tag_apply(struct folio_window *_Nonnull self);
static void tag_filter_mode(struct folio_window *_Nonnull self, enum tag_match_mode mode);
static void tag_clear_filters(struct folio_window *_Nonnull self);
static void management_primary(struct folio_window *_Nonnull self);
static void management_secondary(struct folio_window *_Nonnull self);
static void management_refresh(struct folio_window *_Nonnull self);
static size_t management_row_count(const struct folio_window *_Nonnull self);
static void management_row_text(const struct folio_window *_Nonnull self, size_t index,
                                char *_Nonnull out, size_t capacity);
static void management_status(const struct folio_window *_Nonnull self, char *_Nonnull out,
                              size_t capacity);
static enum ui_text management_button_label(const struct folio_window *_Nonnull self, size_t slot);
static bool management_button_enabled(const struct folio_window *_Nonnull self, size_t slot);
static bool management_tab(struct folio_window *_Nonnull self);
static bool management_return(struct folio_window *_Nonnull self);
static bool management_retryable(const struct folio_window *_Nonnull self);
static void management_intent_text(const struct folio_window *_Nonnull self, char *_Nonnull out,
                                   size_t capacity);
static void management_notice(const struct folio_window *_Nonnull self, char *_Nonnull out,
                              size_t capacity);
static bool tag_navigate(struct folio_window *_Nonnull self, WPARAM key);
static bool tag_tab(struct folio_window *_Nonnull self);
static bool tag_return(struct folio_window *_Nonnull self);
static void click_tag_surface(struct folio_window *_Nonnull self, POINT point);
static enum folio_state_outcome save_before_filter(struct folio_window *_Nonnull self);
static enum folio_state_outcome save_before_catalog(struct folio_window *_Nonnull self,
                                                    enum note_save_mode mode);

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
    case COMMAND_SURFACE_MOVE:
        return (int)folio_state_category_count(self->state);
    case COMMAND_SURFACE_TAG_EDIT:
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
        return (int)tag_matches_count(self);
    case COMMAND_SURFACE_DISCOVERY:
        return (int)folio_state_discovery_summary(self->state).shown;
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
        return (int)folio_state_history_count(self->state);
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
    case COMMAND_SURFACE_ABOUT:
        return 0;
    }
    return 0;
}

/* キー説明の件数は不変catalogから導く。 */
static int command_help_rows(const struct folio_window *_Nonnull self)
{
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_PALETTE:
        return self->command_keys_visible ? (int)key_guide_count() : 0;
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
    case COMMAND_SURFACE_SETTINGS:
    case COMMAND_SURFACE_THEMES:
    case COMMAND_SURFACE_MOVE:
    case COMMAND_SURFACE_ABOUT:
    case COMMAND_SURFACE_TAG_EDIT:
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
    case COMMAND_SURFACE_DISCOVERY:
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

/* EDIT を持たず、レイヤー自身がフォーカスと鍵を受ける一覧の面（設定・履歴・アプリの情報。
 * ADR 0031 の決定 7・ADR 0038 の決定 12・ADR0097）。 */
static bool command_surface_listing(enum command_surface_mode surface)
{
    return surface == COMMAND_SURFACE_SETTINGS || surface == COMMAND_SURFACE_HISTORY ||
           surface == COMMAND_SURFACE_MENU || surface == COMMAND_SURFACE_ABOUT;
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
    if (window == self->discovery_preview)
        return true;
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
    if (command_surface_listing(self->command_surface) || command_keys(self) ||
        (management_surface(self) && !management_input(self)))
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

static void refresh_help_fonts(struct folio_window *_Nonnull self, UINT dpi)
{
    self->help_metrics.valid = false;
    command_help_fonts_destroy(&self->help_fonts);
    self->help_fonts = command_help_fonts_create(folio_state_language(self->state), dpi);
}

/* 言葉の書体は、言語ごとの face（同梱 → その言語の退避 → 日本語の退避。ui_face_for）。
 * DPI と言語のどちらが変わっても作り直す（#1222）。 */
static void refresh_word_font(struct folio_window *_Nonnull self, UINT dpi)
{
    if (self->word_font != nullptr)
    {
        DeleteObject(self->word_font);
    }
    wchar_t face[LF_FACESIZE];
    ui_face_for(folio_state_language(self->state), face);
    self->word_font = CreateFontW(-scale(base_word_font, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                                  FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    self->part_metrics.valid = false;
}

/* 言葉を描く書体。作れなかったときは等幅で描く（前の形）。 */
static HFONT _Nullable word_font(const struct folio_window *_Nonnull self)
{
    return self->word_font != nullptr ? self->word_font : self->mono_font;
}

static void refresh_font(struct folio_window *_Nonnull self, UINT dpi)
{
    if (self->native_metrics != nullptr)
        name_prompt_metrics_invalidate(self->native_metrics);
    self->band.valid = false;
    self->settings_metrics.width = 0;
    refresh_help_fonts(self, dpi);
    self->part_metrics.valid = false;
    if (self->mono_font != nullptr)
    {
        DeleteObject(self->mono_font);
    }
    /* 等幅の書体は、同梱（IBM Plex Mono）→ 退避（Consolas）の順で選ぶ（#1217）。大きさは、
     * どちらでも字の送りが同じになるように ui_face_mono が決める。 */
    wchar_t mono_face[LF_FACESIZE];
    int mono_size = ui_face_mono(mono_face, base_mono_font);
    self->mono_font = CreateFontW(-scale(mono_size, dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, mono_face);
    if (self->command_input != nullptr)
    {
        SendMessageW(self->command_input, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    }
    if (self->discovery_preview != nullptr)
        SendMessageW(self->discovery_preview, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
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
    refresh_word_font(self, dpi);
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
    return scale(base_pane_bottom, GetDpiForWindow(self->handle));
}

static RECT command_palette_rect(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    UINT dpi = GetDpiForWindow(self->handle);
    int margin = scale(ui_part_edge, dpi);
    int available = client.right - margin * 2;
    int surface_width = self->command_surface == COMMAND_SURFACE_HISTORY
                            ? base_settings_width
                            : base_command_palette_width;
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
        surface_width = 640;
    int width = scale(surface_width, dpi);
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

static bool tag_strip_visible(const struct folio_window *_Nonnull self)
{
    return !command_band(self);
}

static int note_tools_height(const struct folio_window *_Nonnull self)
{
    return base_caption_height + base_action_height +
           (tag_strip_visible(self) ? base_tag_height : 0);
}

static RECT command_band_rect(const struct folio_window *_Nonnull self)
{
    RECT pane = regions_for(self).pane;
    int top = pane.top + scale(note_tools_height(self), GetDpiForWindow(self->handle));
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
    int width = scale(discovery_surface(self) ? 960 : base_settings_width, dpi);
    int wanted = self->command_surface != COMMAND_SURFACE_SETTINGS ? base_theme_height
                                                                   : base_settings_height;
    int height = scale(wanted, dpi);
    width = width < client.right - edge ? width : client.right - edge;
    int header = scale(base_common_header_height, dpi);
    height = height < client.bottom - header - edge ? height : client.bottom - header - edge;
    int top = header + (client.bottom - header - height) / 2;
    return (RECT){(client.right - width) / 2, top, (client.right + width) / 2, top + height};
}

static enum folio_state_outcome settings_notice(const struct folio_window *_Nonnull self)
{
    /* #941: the destination picker shows only its own refusal, not a pending settings notice. */
    if (self->command_failure != FOLIO_STATE_READY || move_surface(self))
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
    if (self->command_surface == COMMAND_SURFACE_ABOUT)
        return self->settings_metrics.about_footer;
    return choice_surface(self) ? self->settings_metrics.themes_footer
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
    if (group > 2)
        bounds.top -= (group - 2) * (stride - height - gap);
    if (group > 5)
        bounds.top += self->settings_metrics.motion_help;
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
    if (settings_rows[index].kind == SETTINGS_ROW_NUMBER ||
        settings_rows[index].kind == SETTINGS_ROW_MINIMAP ||
        settings_rows[index].kind == SETTINGS_ROW_FAVORITES_FIRST ||
        settings_rows[index].kind == SETTINGS_ROW_MOTION ||
        settings_rows[index].kind == SETTINGS_ROW_WINDOW ||
        settings_rows[index].kind == SETTINGS_ROW_STORAGE ||
        settings_rows[index].kind == SETTINGS_ROW_ABOUT)
    {
        bounds.top += settings_rows[index].position * (height + gap);
        bounds.bottom = bounds.top + height;
        if (settings_rows[index].kind == SETTINGS_ROW_MOTION)
            bounds.bottom += self->settings_metrics.motion_help;
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

/* 設定とその副面が共有する画素スクロール。itemは現在のスクロール位置での矩形。 */
static void reveal_content_rect(struct folio_window *_Nonnull self, RECT item, size_t limit)
{
    RECT content = settings_content_rect(self);
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
    if (self->command_first > limit)
    {
        self->command_first = limit;
    }
}

static void reveal_settings_selection(struct folio_window *_Nonnull self)
{
    reveal_content_rect(self, settings_item_rect(self, self->command_selection),
                        settings_scroll_limit(self));
}

/* 「このアプリについて」の文字行（ADR0097）。表の順が表示順とUIAの行順で、gapは次の行までの間。
 * 版の行だけ {name} を製品の版で埋める。 */
static const struct
{
    enum ui_text label;
    unsigned char gap;
    bool heading;
    bool secondary;
} about_texts[] = {
    {UI_TEXT_APP_LOGO, base_settings_item_gap, false, false},
    {UI_TEXT_SETTINGS_VERSION, base_settings_item_gap, false, true},
    {UI_TEXT_ABOUT_DESCRIPTION, base_settings_group_gap, false, false},
    {UI_TEXT_ABOUT_LICENSE_HEADING, 0, true, false},
    {UI_TEXT_ABOUT_LICENSE, base_settings_item_gap, false, false},
    {UI_TEXT_ABOUT_FONTS, base_settings_item_gap, false, false},
    {UI_TEXT_ABOUT_NOTICES, base_settings_group_gap, false, false},
    {UI_TEXT_ABOUT_LINKS_HEADING, base_settings_item_gap, true, false},
    {UI_TEXT_ABOUT_LINKS_HINT, 0, false, true},
};
constexpr size_t about_text_count = sizeof about_texts / sizeof about_texts[0];
static_assert(about_text_count == command_surface_about_lines,
              "metrics hold one height per About line");

/* 公式リンクの釦。行き先はcoreの表だけが持ち、ここは意図と表示名を結ぶ。 */
static const struct
{
    enum ui_text label;
    enum product_link link;
    enum accessible_surface_button button;
} about_links[] = {
    {UI_TEXT_ABOUT_LINK_REPOSITORY, PRODUCT_LINK_REPOSITORY, ACCESSIBLE_SURFACE_ABOUT_REPOSITORY},
    {UI_TEXT_ABOUT_LINK_RELEASES, PRODUCT_LINK_RELEASES, ACCESSIBLE_SURFACE_ABOUT_RELEASES},
    {UI_TEXT_ABOUT_LINK_STORE, PRODUCT_LINK_STORE, ACCESSIBLE_SURFACE_ABOUT_STORE},
    {UI_TEXT_ABOUT_LINK_ISSUES, PRODUCT_LINK_ISSUES, ACCESSIBLE_SURFACE_ABOUT_ISSUES},
};
constexpr size_t about_link_count = sizeof about_links / sizeof about_links[0];

static HFONT help_heading_font(const struct folio_window *_Nonnull self);

static bool about_surface(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_ABOUT;
}

/* 釦のslotからリンクの添字。リンクでなければ about_link_count。 */
static size_t about_link_index(size_t slot)
{
    size_t at = 0;
    while (at < about_link_count && about_links[at].button != slot)
        ++at;
    return at;
}

/* focusの順はリンク4つ→戻る。command_selectionがその序数を持つ。 */
static size_t about_focus_slot(const struct folio_window *_Nonnull self)
{
    return self->command_selection < about_link_count ? about_links[self->command_selection].button
                                                      : ACCESSIBLE_SURFACE_BACK;
}

/* 製品名だけ、主窓が既に持つ見出しの書体で題として示す。計測と描画が同じ書体を選ぶ。 */
static HFONT about_text_font(const struct folio_window *_Nonnull self, size_t index)
{
    return about_texts[index].label == UI_TEXT_APP_LOGO ? help_heading_font(self) : self->mono_font;
}

/* outはdraw_unit_limit bytes。描画・計測・UIAが同じ1行を使う。 */
static void about_text_line(const struct folio_window *_Nonnull self, size_t index,
                            char *_Nonnull out)
{
    struct ui_text_request request = {.id = about_texts[index].label,
                                      .language = folio_state_language(self->state),
                                      .name = product_version_text()};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static int about_text_height(const struct folio_window *_Nonnull self, size_t index)
{
    return about_texts[index].heading ? scale(base_settings_label, GetDpiForWindow(self->handle))
                                      : self->settings_metrics.about_text[index];
}

static int about_link_columns(const struct folio_window *_Nonnull self)
{
    return self->settings_metrics.about_single_row ? (int)about_link_count : 2;
}

static int about_links_height(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int rows = (int)about_link_count / about_link_columns(self);
    return rows * scale(ui_part_height, dpi) + (rows - 1) * scale(base_settings_item_gap, dpi);
}

static bool about_links_follow(size_t index)
{
    return about_texts[index].label == UI_TEXT_ABOUT_LINKS_HEADING;
}

/* 内容矩形の上端からの位置（スクロール前）。indexが行数なら内容全体の高さ。 */
static int about_text_top(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int top = 0;
    for (size_t at = 0; at < index; ++at)
    {
        top += about_text_height(self, at) + scale(about_texts[at].gap, dpi);
        if (about_links_follow(at))
            top += about_links_height(self);
    }
    return top;
}

static size_t about_scroll_limit(const struct folio_window *_Nonnull self)
{
    RECT content = settings_content_rect(self);
    int excess = about_text_top(self, about_text_count) - (content.bottom - content.top);
    return excess > 0 ? (size_t)excess : 0;
}

static RECT about_text_rect(const struct folio_window *_Nonnull self, size_t index)
{
    RECT row = settings_content_rect(self);
    row.top += about_text_top(self, index) - (int)self->command_first;
    row.bottom = row.top + about_text_height(self, index);
    return row;
}

/* 釦はリンクの見出しの直下。描画は内容矩形で切り、hitとUIAは交差を使う。 */
static RECT about_link_rect(const struct folio_window *_Nonnull self, size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    size_t heading = 0;
    while (heading + 1 < about_text_count && !about_links_follow(heading))
        ++heading;
    RECT bounds = about_text_rect(self, heading);
    int columns = about_link_columns(self);
    int gap = scale(base_settings_item_gap, dpi);
    int height = scale(ui_part_height, dpi);
    int width = (bounds.right - bounds.left - gap * (columns - 1)) / columns;
    bounds.left += (int)index % columns * (width + gap);
    bounds.right = bounds.left + width;
    bounds.top = bounds.bottom + (int)index / columns * (height + gap);
    bounds.bottom = bounds.top + height;
    return bounds;
}

static RECT about_notice_rect(const struct folio_window *_Nonnull self)
{
    RECT line = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    line.bottom -= scale(ui_part_height + 8, GetDpiForWindow(self->handle));
    line.top = line.bottom - self->settings_metrics.notice;
    return line;
}

/* 戻るは固定footerにあり常に見える。リンクだけ内容のスクロールで見える位置へ出す。 */
static void reveal_about_focus(struct folio_window *_Nonnull self)
{
    size_t limit = about_scroll_limit(self);
    if (self->command_selection < about_link_count)
        reveal_content_rect(self, about_link_rect(self, self->command_selection), limit);
    if (self->command_first > limit)
        self->command_first = limit;
    refresh_surface_tooltip_rects(self);
}

static size_t settings_content_limit(const struct folio_window *_Nonnull self)
{
    return about_surface(self) ? about_scroll_limit(self) : settings_scroll_limit(self);
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
    case COMMAND_SURFACE_MOVE:
    case COMMAND_SURFACE_ABOUT:
    case COMMAND_SURFACE_TAG_EDIT:
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
    case COMMAND_SURFACE_DISCOVERY:
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

static RECT tag_buttons_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    bounds.top = bounds.bottom - self->settings_metrics.tag_button_height;
    return bounds;
}

static RECT tag_footer_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    bounds.top = bounds.bottom - self->settings_metrics.tags_footer;
    bounds.bottom = tag_buttons_rect(self).top - scale(8, GetDpiForWindow(self->handle));
    return bounds;
}

static RECT tag_rows_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = theme_input_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    bounds.top = bounds.bottom + scale(8, dpi);
    int bottom = tag_footer_rect(self).top - scale(8, dpi);
    int height = bottom > bounds.top ? bottom - bounds.top : 0;
    int row = scale(base_command_row_height, dpi);
    bounds.bottom = bounds.top + height / row * row;
    return bounds;
}

static size_t tag_button_first(const struct folio_window *_Nonnull self)
{
    if (management_surface(self))
        return ACCESSIBLE_SURFACE_TAG_PRIMARY;
    return self->command_surface == COMMAND_SURFACE_TAG_EDIT ? ACCESSIBLE_SURFACE_TAG_ADD
                                                             : ACCESSIBLE_SURFACE_TAG_ALL;
}

static size_t tag_button_count(const struct folio_window *_Nonnull self)
{
    return management_surface(self) ? 2 : 3;
}

static RECT tag_button_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    RECT bounds = tag_buttons_rect(self);
    size_t first = tag_button_first(self);
    int count = (int)tag_button_count(self);
    int gap = scale(6, GetDpiForWindow(self->handle));
    int width = (bounds.right - bounds.left - (count - 1) * gap) / count;
    bounds.left += (int)(slot - first) * (width + gap);
    bounds.right = bounds.left + width;
    return bounds;
}

static RECT command_tab_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    if (discovery_surface(self))
    {
        struct discovery_layout layout = discovery_geometry(self);
        return slot == 0 ? layout.names : layout.body;
    }
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
    if (discovery_surface(self))
        return discovery_geometry(self).input;
    if (choice_surface(self) || tag_surface(self))
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
    body.top = command_keys(self) ? command_tab_rect(self, 0).bottom + scale(40, dpi)
                                  : command_input_rect(self).bottom + scale(8, dpi);
    body.bottom -= scale(base_command_status_height + 8, dpi);
    if (body.bottom < body.top)
        body.bottom = body.top;
    return body;
}

/* 排他的なキータブの可視高。固定の群見出しとページ操作はこの外に置く。 */
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

static size_t command_help_offset(const struct folio_window *_Nonnull self);

static RECT command_toggle_rect(const struct folio_window *_Nonnull self)
{
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    bounds.top = bounds.bottom - scale(ui_part_height, GetDpiForWindow(self->handle));
    return bounds;
}

static RECT command_rows_rect(const struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        return discovery_geometry(self).rows;
    if (tag_surface(self))
        return tag_rows_rect(self);
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        RECT r;
        GetClientRect(self->command_layer, &r);
        return r;
    }

    if (choice_surface(self))
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
    int height = choice_surface(self) ? base_theme_row_height : base_command_row_height;
    if (discovery_surface(self))
        height = 44;
    return (size_t)((rows.bottom - rows.top) / scale(height, GetDpiForWindow(self->handle)));
}

static void reveal_command_selection(struct folio_window *_Nonnull self)
{
    if (choice_surface(self))
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
    if (about_surface(self))
    {
        reveal_about_focus(self);
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
    bool visible = native || framed_surface(self);
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
    bool enabled = !framed_surface(self) && self->native_modal_depth == 0;
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
    if (!choice_surface(self) &&
        (settings_surface(self) || command_surface_listing(self->command_surface)))
        self->command_first = 0;
    if (self->command_surface == COMMAND_SURFACE_PALETTE || framed_surface(self) ||
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
    arrange_discovery_preview(self);
    /* 置換は 2 つの欄を使い、設定と履歴は欄を持たないのでどれも隠す。 */
    bool hidden = self->command_surface == COMMAND_SURFACE_REPLACE || command_keys(self) ||
                  command_surface_listing(self->command_surface) ||
                  (management_surface(self) && !management_input(self));
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
    discovery_motion_settle(self);
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
    discovery_motion_sync(self);
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
    discovery_motion_settle(self);
    refresh_modal_availability(self);
    failure_box_show(modal_owner_current(self->modal_owner), FOLIO_STATE_OUT_OF_MEMORY, self->state,
                     nullptr);
    self->native_modal_depth -= 1;
    refresh_modal_availability(self);
    discovery_motion_sync(self);
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

/* 保存先は本文stateを更新しない。既存native owner/dispatchの寿命で包む。 */
static void show_storage_prompt(struct folio_window *_Nonnull self)
{
    HWND focus = GetFocus();
    bool prepared = begin_native_modal(self);
    enum storage_prompt_outcome result = STORAGE_PROMPT_UNAVAILABLE;
    if (prepared)
    {
        struct storage_prompt_request request = {.storage = self->storage,
                                                 .folio = self->state,
                                                 .language = folio_state_language(self->state)};
        result =
            storage_prompt_show(modal_owner_current(self->modal_owner), &request, &self->palette);
    }
    bool restored = end_native_modal(self);
    if (!restored || result == STORAGE_PROMPT_UNAVAILABLE)
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
    if (minimap_visible(self))
    {
        RECT map = minimap_rect(self);
        InvalidateRect(self->handle, &map, FALSE);
        accessibility_host_schedule(self->accessibility);
    }
    track_outline(self);
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
    arrange_drawer(self, bounds);
}

static RECT body_available_rect(const struct folio_window *_Nonnull self, RECT pane, UINT dpi,
                                int right)
{
    int top = command_band(self) ? self->band.pane_top : scale(base_pane_top, dpi);
    int bottom = command_band(self) ? self->band.pane_bottom : scale(base_pane_bottom, dpi);
    top += pane.top + scale(note_tools_height(self), dpi);
    top += command_band(self) ? self->band.height : 0;
    int left = scale(base_pane_left, dpi);
    left = self->body_left_inset > left ? self->body_left_inset : left;
    return (RECT){pane.left + left, top, pane.right - scale(right, dpi), pane.bottom - bottom};
}

static bool minimap_visible(const struct folio_window *_Nonnull self)
{
    if (!body_shown(self) || self->pane == nullptr || !folio_state_minimap(self->state))
        return false;
    UINT dpi = GetDpiForWindow(self->handle);
    RECT area = body_available_rect(self, regions_for(self).pane, dpi, base_minimap_right);
    return area.right - area.left >=
               scale(base_minimap_minimum_body + base_minimap_width + base_minimap_gap, dpi) &&
           area.bottom - area.top >= scale(32, dpi);
}

/* #975 / ADR0114: what is left of the pane's width for the body and the headings column: the
 * available area less the minimap, when that is shown. */
static RECT outline_area(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    bool minimap = minimap_visible(self);
    RECT area = body_available_rect(self, regions_for(self).pane, dpi,
                                    minimap ? base_minimap_right : base_pane_right);
    if (minimap)
        area.right -= scale(base_minimap_width + base_minimap_gap, dpi);
    return area;
}

/* The column is shown while it is open, a note is being read (not edited), and the body keeps
 * its minimum width beside it. A window too narrow hides the column before the minimap. */
static bool outline_visible(const struct folio_window *_Nonnull self)
{
    if (!body_shown(self) || self->pane == nullptr || !folio_state_outline(self->state) ||
        folio_state_pane_mode(self->state) != PANE_MODE_VIEW)
        return false;
    UINT dpi = GetDpiForWindow(self->handle);
    RECT area = outline_area(self);
    return area.right - area.left >=
               scale(base_minimap_minimum_body + base_outline_width + base_outline_gap, dpi) &&
           area.bottom - area.top >= scale(64, dpi);
}

static RECT outline_rect(const struct folio_window *_Nonnull self)
{
    if (!outline_visible(self))
        return (RECT){0};
    RECT bounds = outline_area(self);
    bounds.left = bounds.right - scale(base_outline_width, GetDpiForWindow(self->handle));
    return bounds;
}

static RECT body_content_rect(struct folio_window *_Nonnull self, RECT pane, UINT dpi)
{
    self->body_left_inset = pane_inset(self, dpi);
    bool minimap = minimap_visible(self);
    RECT bounds =
        body_available_rect(self, pane, dpi, minimap ? base_minimap_right : base_pane_right);
    if (minimap)
        bounds.right -= scale(base_minimap_width + base_minimap_gap, dpi);
    self->outline_carved = outline_visible(self);
    if (self->outline_carved)
        bounds.right -= scale(base_outline_width + base_outline_gap, dpi);
    return bounds;
}

/* The column's place and look for drawing, hit-testing and accessibility. The labels are
 * filled by the caller that draws; the others pass empty ones. */
static struct note_outline_paint outline_paint_for(const struct folio_window *_Nonnull self,
                                                   const char16_t *_Nonnull title,
                                                   const char16_t *_Nonnull empty)
{
    return (struct note_outline_paint){.bounds = outline_rect(self),
                                       .focused = part_focus_shown(self, ACCESSIBLE_OUTLINE),
                                       .dpi = GetDpiForWindow(self->handle),
                                       .font = self->mono_font,
                                       .palette = &self->palette,
                                       .title = title,
                                       .empty = empty};
}

static struct note_outline_paint outline_place(const struct folio_window *_Nonnull self)
{
    return outline_paint_for(self, u"", u"");
}

/* After the body scrolled, was loaded or was laid out again: the heading being read may have
 * changed, and the column may show another number of rows (#1018). */
static void track_outline(const struct folio_window *_Nonnull self)
{
    if (!outline_visible(self) || note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY)
        return;
    struct note_outline_paint paint = outline_place(self);
    bool fitted = note_outline_fit(self->outline, &paint, note_pane_heading_count(self->pane));
    if (!note_outline_track(self->outline, self->pane, &paint) && !fitted)
        return;
    InvalidateRect(self->handle, &paint.bounds, FALSE);
    accessibility_host_schedule(self->accessibility);
}

static RECT minimap_rect(const struct folio_window *_Nonnull self)
{
    if (!minimap_visible(self))
        return (RECT){0};
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds = body_available_rect(self, regions_for(self).pane, dpi, base_minimap_right);
    bounds.left = bounds.right - scale(base_minimap_width, dpi);
    return bounds;
}

static bool pane_inset_changed(const struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr || !body_shown(self))
        return false;
    UINT dpi = GetDpiForWindow(self->handle);
    return pane_bounds(self).left != regions_for(self).pane.left + pane_inset(self, dpi);
}

static bool minimap_available(const struct folio_window *_Nonnull self)
{
    return minimap_visible(self) && folio_window_accessible_ready(self) &&
           self->command_surface == COMMAND_SURFACE_CLOSED &&
           note_pane_readiness(self->pane) == NOTE_PANE_READINESS_READY;
}

/* #975: the column takes the pointer and UIA actions only while a ready note is read and no
 * surface covers the window. */
static bool outline_available(const struct folio_window *_Nonnull self)
{
    return outline_visible(self) && folio_window_accessible_ready(self) &&
           self->command_surface == COMMAND_SURFACE_CLOSED &&
           note_pane_readiness(self->pane) == NOTE_PANE_READINESS_READY;
}

static bool minimap_paint_for(const struct folio_window *_Nonnull self,
                              struct note_minimap_paint *_Nonnull out)
{
    if (!minimap_visible(self) || note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY)
        return false;
    struct note_pane_geometry view;
    if (!note_pane_viewport(self->pane, &view))
        return false;
    *out = (struct note_minimap_paint){.bounds = minimap_rect(self),
                                       .dpi = GetDpiForWindow(self->handle),
                                       .view = view,
                                       .focused = part_focus_shown(self, ACCESSIBLE_MINIMAP),
                                       .active = self->hover_part == ACCESSIBLE_MINIMAP ||
                                                 self->pressed_part == ACCESSIBLE_MINIMAP};
    return true;
}

static void request_minimap(struct folio_window *_Nonnull self, enum minimap_update update)
{
    KillTimer(self->handle, minimap_timer);
    note_minimap_invalidate(self->minimap);
    if (!minimap_visible(self))
    {
        note_minimap_clear(self->minimap);
        return;
    }
    switch (update)
    {
    case MINIMAP_UPDATE_CLEAR:
        note_minimap_clear(self->minimap);
        return;
    case MINIMAP_UPDATE_SETTLE:
        if (SetTimer(self->handle, minimap_timer, minimap_delay, nullptr) == 0)
            note_minimap_begin(self->minimap);
        return;
    case MINIMAP_UPDATE_READY:
        note_minimap_begin(self->minimap);
        return;
    }
}

static void arrange_minimap(struct folio_window *_Nonnull self, RECT before, bool was_visible)
{
    RECT after = pane_bounds(self);
    if (!minimap_visible(self) || !was_visible || !EqualRect(&before, &after))
        request_minimap(self, MINIMAP_UPDATE_SETTLE);
    if ((self->keyboard_target == ACCESSIBLE_MINIMAP && !minimap_visible(self)) ||
        (self->keyboard_target == ACCESSIBLE_OUTLINE && !outline_visible(self)))
        self->keyboard_target = mode_kind(folio_state_pane_mode(self->state));
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
    RECT previous_body = pane_bounds(self);
    bool was_visible = self->pane != nullptr && IsWindowVisible(note_pane_handle(self->pane));
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
    arrange_minimap(self, previous_body, was_visible);
    RECT pane_region = regions.pane;
    InvalidateRect(self->handle, &pane_region, FALSE);
    refresh_gutter(self, false);
    refresh_tooltip_rects(self);
}

static void arrange_zoomed_pane(struct folio_window *_Nonnull self, size_t first_line)
{
    if (self->pane == nullptr)
        return;
    refresh_gutter_font(self, GetDpiForWindow(self->handle));
    arrange(self);
    request_minimap(self, MINIMAP_UPDATE_SETTLE);
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
    if (body != nullptr && (shown != body_shown(self) || pane_inset_changed(self) ||
                            self->outline_carved != outline_visible(self)))
    {
        arrange(self);
    }
    note_outline_reset(self->outline);
    self->outline_wheel = 0;
    track_outline(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    if (outcome == NOTE_PANE_LOAD_READY && self->appearance_recolor_pending)
    {
        PostMessageW(self->handle, folio_message_appearance_changed, 0, 0);
    }
    switch (outcome)
    {
    case NOTE_PANE_LOAD_READY:
        request_minimap(self, MINIMAP_UPDATE_READY);
        break;
    case NOTE_PANE_LOAD_UNAVAILABLE:
    case NOTE_PANE_LOAD_FAILED:
    case NOTE_PANE_LOAD_BUSY:
        request_minimap(self, MINIMAP_UPDATE_CLEAR);
        break;
    }
    return from_pane_load(outcome);
}

static void clear_images(struct folio_window *_Nonnull self)
{
    KillTimer(self->handle, image_timer);
    folio_state_cancel_images(self->state);
    self->image_pending = false;
    self->image_failed = false;
    self->image_advisory = false;
    self->image_line = UI_TEXT_EMPTY;
    self->diagram_failure = UI_TEXT_EMPTY;
}
static void prepare_images(struct folio_window *_Nonnull self)
{
    self->image_origin_revision = folio_state_image_origin_revision(self->state);
    const struct markdown_media *media = folio_state_media(self->state);
    if (media == nullptr)
        return;
    size_t count = markdown_media_count(media);
    self->image_advisory = markdown_images_advisory(markdown_media_images(media));
    if (markdown_media_overflow(media))
        self->diagram_failure = UI_TEXT_MEDIA_LIMIT;
    if (note_pane_prepare_images(self->pane, count) != IMAGE_READY)
        self->image_failed = true;
    if (count != 0)
        SetTimer(self->handle, image_timer, 100, nullptr);
}
static enum folio_state_outcome render_pane(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
    {
        return FOLIO_STATE_PANE_UNAVAILABLE;
    }
    bool was_unavailable = pane_unavailable(self);
    clear_images(self);
    request_minimap(self, MINIMAP_UPDATE_CLEAR);
    note_pane_code_palette(self->pane, folio_state_palette(self->state));
    enum note_pane_load_outcome outcome =
        note_pane_load(self->pane, folio_state_pane_rtf(self->state),
                       folio_state_pane_rtf_length(self->state), PANE_MODE_VIEW);
    if (outcome == NOTE_PANE_LOAD_READY)
        prepare_images(self);
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
 * （等幅の書体の ASCII だけの行では DT_CALCRECT が字間を含めない）。フォントリンクで描く行は
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

/* 字の幅は描画と同じ言葉の書体と字間で測る。測れなければ 0。 */
static int measure_part_text_uncached(const struct folio_window *_Nonnull self, enum ui_text text)
{
    HDC device = GetDC(self->handle);
    if (device == nullptr)
    {
        return 0;
    }
    HGDIOBJ previous = SelectObject(device, word_font(self));
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
    if (current->valid && current->font == word_font(self) && current->dpi == dpi &&
        current->language == language && current->tracking == tracking)
        return;
    struct ui_part_metrics metrics = {
        .font = word_font(self), .dpi = dpi, .language = language, .tracking = tracking};
    if (word_font(self) == nullptr)
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
    bool current = metrics->valid && metrics->font == word_font(self) && metrics->dpi == dpi &&
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
static enum accessible_kind selected_scope_kind(const struct folio_window *_Nonnull self)
{
    switch (folio_state_note_scope(self->state))
    {
    case NOTE_SCOPE_ALL:
        return ACCESSIBLE_SCOPE_ALL;
    case NOTE_SCOPE_FAVORITES:
        return ACCESSIBLE_SCOPE_FAVORITES;
    }
    return ACCESSIBLE_SCOPE_ALL;
}

static bool part_selected(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (accessible_controls[kind].parent == ACCESSIBLE_SCOPE_GROUP)
        return selected_scope_kind(self) == kind;
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
    /* 釦の名前は言葉の書体で描く。測る側（measure_part_text_uncached）と同じ書体（#1222）。 */
    HGDIOBJ previous = SelectObject(device, word_font(self));
    draw_units(device, ui_text_line(part_display_text(kind), folio_state_language(self->state)),
               ink.label, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    SelectObject(device, previous);
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
            (void)bounded_text_append(out, draw_unit_limit, "\n");
        used = bounded_text_append(out, draw_unit_limit, line);
    }
}

static void command_help_text(const struct folio_window *_Nonnull self, size_t slot,
                              char *_Nonnull out)
{
    if (key_guide_name(key_guide_at(slot), folio_state_language(self->state), out,
                       draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static HFONT help_body_font(const struct folio_window *_Nonnull self)
{
    return self->help_fonts.body != nullptr ? self->help_fonts.body : self->mono_font;
}

static HFONT help_key_font(const struct folio_window *_Nonnull self)
{
    return self->help_fonts.key != nullptr ? self->help_fonts.key : self->mono_font;
}

static HFONT help_heading_font(const struct folio_window *_Nonnull self)
{
    return self->help_fonts.heading != nullptr ? self->help_fonts.heading : self->mono_font;
}

static int help_measure(HDC device, const char *_Nonnull text, RECT *_Nonnull bounds,
                        struct command_help_metrics *_Nonnull metrics)
{
    struct text_draw_request request = {.bounds = *bounds,
                                        .format = DT_CALCRECT,
                                        .wrapped = true,
                                        .maximum_line_height = &metrics->maximum_line_height};
    int height = draw_text(device, text, &request);
    bounds->bottom = bounds->top + height;
    return height;
}

static int help_group_space(const struct folio_window *_Nonnull self, HDC device,
                            struct command_help_metrics *_Nonnull metrics, size_t index)
{
    if (index == 0 ||
        key_guide_group(key_guide_at(index)) == key_guide_group(key_guide_at(index - 1)))
        return 0;
    RECT *heading = &metrics->rows[index].heading;
    *heading = (RECT){0, scale(16, metrics->dpi), metrics->width, 0};
    SelectObject(device, help_heading_font(self));
    (void)help_measure(device,
                       ui_text_line(key_guide_group(key_guide_at(index)), metrics->language),
                       heading, metrics);
    return heading->bottom + scale(8, metrics->dpi);
}

static RECT help_description_bounds(const struct command_help_metrics *_Nonnull metrics,
                                    const struct command_help_placement *_Nonnull row, bool stacked)
{
    RECT bounds = {0, row->key.top, metrics->width, 0};
    if (row->key.bottom > row->key.top)
    {
        if (stacked)
            bounds.top = row->key.bottom + scale(4, metrics->dpi);
        if (!stacked)
            bounds.left = row->key.right + scale(24, metrics->dpi);
    }
    return bounds;
}

static bool measure_help_row(const struct folio_window *_Nonnull self, HDC device,
                             struct command_help_metrics *_Nonnull metrics, size_t index)
{
    const struct key_guide *entry = key_guide_at(index);
    struct command_help_placement *row = &metrics->rows[index];
    int top = help_group_space(self, device, metrics, index);
    int column = scale(176, metrics->dpi);
    char key[draw_unit_limit];
    if (key_guide_key(entry, metrics->language, key, sizeof key) != UI_TEXT_FORMAT_READY)
        return false;
    SelectObject(device, help_key_font(self));
    bool stacked = metrics->width < scale(480, metrics->dpi) || measure_utf8(device, key) > column;
    row->key = (RECT){0, top, stacked ? metrics->width : column, top};
    if (key[0] != '\0' && help_measure(device, key, &row->key, metrics) <= 0)
        return false;
    row->description = help_description_bounds(metrics, row, stacked);
    SelectObject(device, help_body_font(self));
    if (help_measure(device, key_guide_description(entry, metrics->language), &row->description,
                     metrics) <= 0)
        return false;
    int bottom =
        row->description.bottom > row->key.bottom ? row->description.bottom : row->key.bottom;
    row->height = bottom + scale(12, metrics->dpi);
    metrics->offsets[index + 1] = metrics->offsets[index] + row->height;
    return true;
}

static bool measure_help_metrics(const struct folio_window *_Nonnull self, HDC device,
                                 struct command_help_metrics *_Nonnull metrics)
{
    for (size_t index = 0; index < metrics->count; ++index)
        if (!measure_help_row(self, device, metrics, index))
            return false;
    return metrics->maximum_line_height > 0;
}

static bool prepare_help_metrics(struct folio_window *_Nonnull self)
{
    RECT bounds = command_body_rect(self);
    struct command_help_metrics metrics = {.width = bounds.right - bounds.left,
                                           .dpi = GetDpiForWindow(self->handle),
                                           .font = help_body_font(self),
                                           .language = folio_state_language(self->state),
                                           .count = key_guide_count()};
    if (self->help_metrics.valid && self->help_metrics.width == metrics.width &&
        self->help_metrics.dpi == metrics.dpi && self->help_metrics.font == metrics.font &&
        self->help_metrics.language == metrics.language)
        return true;
    if (metrics.width <= 0)
        return false;
    HDC device = GetDC(self->command_layer);
    if (device == nullptr)
        return false;
    int saved = SaveDC(device);
    SetTextCharacterExtra(device, 0);
    bool ready = measure_help_metrics(self, device, &metrics);
    RestoreDC(device, saved);
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

/* 通常項目は分割せず、表示高を超える項目だけ一行を重ねて送る。 */
static size_t next_help_page(const struct folio_window *_Nonnull self, size_t first)
{
    size_t visible = (size_t)command_visible_help(self);
    if (visible == 0 || first + visible >= (size_t)command_help_total(self))
        return first;
    for (size_t index = 0; index < self->help_metrics.count; ++index)
    {
        size_t top = (size_t)self->help_metrics.offsets[index];
        size_t bottom = (size_t)self->help_metrics.offsets[index + 1];
        if (bottom > first + visible)
            return top > first ? top : first + command_help_step(self);
    }
    return first;
}

/* resizeとUIAの位置指定も同じページ列へ丸める。 */
static size_t help_page_at(const struct folio_window *_Nonnull self, size_t target)
{
    size_t first = 0;
    size_t next = next_help_page(self, first);
    while (next > first && next <= target)
    {
        first = next;
        next = next_help_page(self, first);
    }
    return first;
}

static size_t command_help_offset(const struct folio_window *_Nonnull self)
{
    return help_page_at(self, self->command_help_scroll);
}

/* 描画とUIAは、次ページへ送った通常項目を同じく不可視にする。 */
static RECT visible_help_rect(const struct folio_window *_Nonnull self, size_t index)
{
    RECT row = command_help_geometry(self, index, nullptr);
    OffsetRect(&row, 0, -(int)command_help_offset(self));
    RECT area = command_body_rect(self);
    if (row.bottom - row.top <= area.bottom - area.top &&
        (row.top < area.top || row.bottom > area.bottom))
        return (RECT){0};
    RECT visible = {0};
    IntersectRect(&visible, &row, &area);
    return visible;
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

static bool tag_mode_selected(const struct folio_window *_Nonnull self, size_t slot)
{
    if (self->command_surface != COMMAND_SURFACE_TAG_FILTER)
        return false;
    return (slot == ACCESSIBLE_SURFACE_TAG_ALL &&
            folio_state_tag_match_mode(self->state) == TAG_MATCH_ALL) ||
           (slot == ACCESSIBLE_SURFACE_TAG_ANY &&
            folio_state_tag_match_mode(self->state) == TAG_MATCH_ANY);
}

static bool tag_button_focused(const struct folio_window *_Nonnull self, size_t slot)
{
    if (discovery_surface(self))
        return GetFocus() == self->command_layer &&
               discovery_focus_slot(self->discovery_focus) == slot &&
               (self->focus_cues || self->focus_cues_always);
    if (about_surface(self))
        return GetFocus() == self->command_layer && about_focus_slot(self) == slot &&
               (self->focus_cues || self->focus_cues_always);
    return tag_surface(self) && GetFocus() == self->command_layer &&
           tag_focus_slot(self->tag_focus) == slot && (self->focus_cues || self->focus_cues_always);
}

static void surface_button_text(const struct folio_window *_Nonnull self, HDC device, size_t slot,
                                RECT label)
{
    const char *_Nonnull text =
        ui_text_line(surface_button_label(self, slot), folio_state_language(self->state));
    char discovery[draw_unit_limit];
    if (discovery_surface(self) && slot == ACCESSIBLE_SURFACE_DISCOVERY_TAG)
    {
        discovery_text_tag(self->state, self->discovery_tag, discovery, sizeof discovery);
        text = discovery;
    }
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG)
        text = "<";
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG)
        text = ">";
    if (tag_surface(self) && slot >= ACCESSIBLE_SURFACE_TAG_ADD)
    {
        int height = wrapped_text(device, text, label, false);
        label.top += (label.bottom - label.top - height) / 2;
        (void)wrapped_text(device, text, label, true);
        return;
    }
    draw_utf8(device, text, label);
}

/* boundsは切り取り前の矩形。スクロールする内容の釦は呼び出し側のclipで切る。 */
static void draw_surface_button_in(const struct folio_window *_Nonnull self, HDC device,
                                   size_t slot, RECT bounds)
{
    bool close = slot == ACCESSIBLE_SURFACE_CLOSE;
    struct ui_part_paint part = {
        .bounds = bounds,
        .style = close ? UI_PART_STYLE_ICON
                       : (slot == ACCESSIBLE_SURFACE_TAG_ALL || slot == ACCESSIBLE_SURFACE_TAG_ANY
                              ? UI_PART_STYLE_CHOICE
                              : UI_PART_STYLE_COMMAND),
        .dpi = GetDpiForWindow(self->handle),
        .labeled = !close,
        .has_icon = close,
        .icon = ICON_PAINT_CLOSE,
        .hovered = self->surface_hover == slot,
        .pressed = self->surface_pressed == slot && self->surface_pressed_inside,
        .disabled = !folio_window_accessible_ready(self) ||
                    (management_surface(self) && !management_button_enabled(self, slot)) ||
                    (discovery_surface(self) && !discovery_button_enabled(self, slot)),
        .focused = tag_button_focused(self, slot),
        .selected = tag_mode_selected(self, slot)};
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    if (!close)
    {
        SetTextColor(device, ink.text);
        surface_button_text(self, device, slot, ink.label);
    }
}

static void draw_surface_button(const struct folio_window *_Nonnull self, HDC device, size_t slot)
{
    RECT bounds = accessible_surface_button_rect(self, slot);
    /* 検索面は更新領域で切り抜いて描く。領域の外の釦は描かない（#844）。 */
    if (!discovery_surface(self) || RectVisible(device, &bounds))
        draw_surface_button_in(self, device, slot, bounds);
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
        (void)bounded_text_copy(out, replace_status_capacity, self->result_line);
        return;
    }
    enum folio_language language = folio_state_language(self->state);
    size_t at = bounded_text_copy(out, replace_status_capacity,
                                  folio_state_failure_line(outcome, language));
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

/* 各ページの先頭にも所属群を残し、途中ページのEnter/Escを別の場所の操作と読ませない。 */
static enum ui_text visible_help_group(const struct folio_window *_Nonnull self)
{
    int offset = (int)command_help_offset(self);
    size_t index = 0;
    while (index + 1 < self->help_metrics.count && self->help_metrics.offsets[index + 1] <= offset)
        ++index;
    return key_guide_group(key_guide_at(index));
}

static RECT help_row_rect(RECT relative, RECT origin)
{
    OffsetRect(&relative, origin.left, origin.top);
    return relative;
}

static void draw_help_row(const struct folio_window *_Nonnull self, HDC device, size_t index)
{
    RECT origin = command_help_geometry(self, index, nullptr);
    OffsetRect(&origin, 0, -(int)command_help_offset(self));
    const struct command_help_placement *row = &self->help_metrics.rows[index];
    const struct key_guide *entry = key_guide_at(index);
    enum folio_language language = folio_state_language(self->state);
    if (row->heading.bottom > row->heading.top &&
        key_guide_group(entry) != visible_help_group(self))
    {
        SelectObject(device, help_heading_font(self));
        RECT heading = help_row_rect(row->heading, origin);
        (void)wrapped_text(device, ui_text_line(key_guide_group(entry), language), heading, true);
        RECT rule = {heading.left, heading.top - scale(8, self->help_metrics.dpi), heading.right,
                     heading.top - scale(8, self->help_metrics.dpi) + 1};
        HBRUSH brush = CreateSolidBrush(self->palette.dialog_border);
        FillRect(device, &rule, brush);
        DeleteObject(brush);
    }
    char key[draw_unit_limit];
    if (key_guide_key(entry, language, key, sizeof key) != UI_TEXT_FORMAT_READY)
        return;
    SelectObject(device, help_key_font(self));
    (void)wrapped_text(device, key, help_row_rect(row->key, origin), true);
    SelectObject(device, help_body_font(self));
    (void)wrapped_text(device, key_guide_description(entry, language),
                       help_row_rect(row->description, origin), true);
}

static void draw_command_shortcuts(const struct folio_window *_Nonnull self, HDC device,
                                   RECT bounds)
{
    (void)bounds;
    if (!command_keys(self) || !self->help_metrics.valid)
        return;
    RECT area = command_body_rect(self);
    int saved = SaveDC(device);
    SetTextCharacterExtra(device, 0);
    SetTextColor(device, self->palette.current_text);
    SelectObject(device, help_heading_font(self));
    RECT heading = area;
    heading.top -= scale(32, self->help_metrics.dpi);
    heading.bottom = area.top;
    draw_utf8(device, ui_text_line(visible_help_group(self), folio_state_language(self->state)),
              heading);
    IntersectClipRect(device, area.left, area.top, area.right, area.bottom);
    for (size_t index = 0; index < self->help_metrics.count; ++index)
    {
        RECT row = visible_help_rect(self, index);
        if (!IsRectEmpty(&row))
            draw_help_row(self, device, index);
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
    size_t page = 1;
    size_t count = 1;
    size_t current = command_help_offset(self);
    size_t first = 0;
    size_t next = next_help_page(self, first);
    while (next > first)
    {
        ++count;
        if (next <= current)
            page = count;
        first = next;
        next = next_help_page(self, first);
    }
    char line[command_pages_capacity];
    struct ui_text_request request = {.id = UI_TEXT_STATUS_PALETTE_PAGES,
                                      .language = folio_state_language(self->state),
                                      .k = page,
                                      .n = count};
    if (ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY)
    {
        SetTextColor(device, self->palette.current_text);
        draw_utf8(device, line, command_pages_rect(self));
    }
}

static size_t surface_current_tab(const struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        return folio_state_discovery_summary(self->state).mode == DISCOVERY_MODE_BODY ? 1 : 0;
    return (size_t)self->command_keys_visible;
}

static enum ui_text surface_tab_label(const struct folio_window *_Nonnull self, size_t slot)
{
    if (discovery_surface(self))
        return slot == 0 ? UI_TEXT_DISCOVERY_MODE_NAMES : UI_TEXT_DISCOVERY_MODE_BODY;
    return slot == 0 ? UI_TEXT_HELP_TAB_COMMANDS : UI_TEXT_HELP_TAB_KEYS;
}

static bool surface_tab_focused(const struct folio_window *_Nonnull self, size_t slot)
{
    return discovery_surface(self) && self->discovery_focus == DISCOVERY_FOCUS_MODE &&
           GetFocus() == self->command_layer &&
           (self->discovery_mode_focus == DISCOVERY_MODE_BODY) == (slot == 1);
}

static void draw_command_guidance(const struct folio_window *_Nonnull self, HDC device)
{
    enum folio_language language = folio_state_language(self->state);
    RECT first = command_tab_rect(self, 0);
    RECT group = first;
    group.right = command_tab_rect(self, 1).right;
    if (discovery_surface(self) && !RectVisible(device, &group))
        return;
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
                                     .selected = surface_current_tab(self) == index,
                                     .disabled = discovery_surface(self) && self->command_composing,
                                     .focused = surface_tab_focused(self, index) &&
                                                (self->focus_cues || self->focus_cues_always)};
        struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
        SetTextColor(device, ink.text);
        draw_units(device, ui_text_line(surface_tab_label(self, index), language), ink.label,
                   DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
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
static void draw_settings_mark(const struct folio_window *_Nonnull self, HDC device, RECT row,
                               bool selected)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT bounds = settings_mark_rect(self, row);
    frame_settings_rect(device, bounds, self->palette.dialog_border);
    if (selected)
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
    case SETTINGS_ROW_STORAGE:
    case SETTINGS_ROW_ABOUT:
        return false;
    case SETTINGS_ROW_THEME:
        return false;
    case SETTINGS_ROW_LANGUAGE:
        return (unsigned char)folio_state_language(self->state) == settings_rows[index].value;
    case SETTINGS_ROW_FAVORITES_FIRST:
        return folio_state_note_order(self->state) == NOTE_ORDER_FAVORITES_FIRST;
    case SETTINGS_ROW_MINIMAP:
        return folio_state_minimap(self->state);
    case SETTINGS_ROW_MOTION:
        return folio_state_motion(self->state);
    case SETTINGS_ROW_WINDOW:
        return storage_state_window(self->storage).restore;
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

static bool settings_row_toggle(size_t index)
{
    return settings_rows[index].kind == SETTINGS_ROW_NUMBER ||
           settings_rows[index].kind == SETTINGS_ROW_MINIMAP ||
           settings_rows[index].kind == SETTINGS_ROW_MOTION ||
           settings_rows[index].kind == SETTINGS_ROW_WINDOW ||
           settings_rows[index].kind == SETTINGS_ROW_FAVORITES_FIRST;
}

static enum ui_text settings_motion_help(const struct folio_window *_Nonnull self)
{
    return folio_state_motion_available(self->state) ? UI_TEXT_MOTION_HELP : UI_TEXT_MOTION_STATIC;
}

static void draw_settings_motion_help(const struct folio_window *_Nonnull self, HDC device,
                                      RECT row)
{
    UINT dpi = GetDpiForWindow(self->handle);
    row.top += scale(base_settings_item_height, dpi);
    row.left += scale(32, dpi);
    row.right -= scale(4, dpi);
    SetTextColor(device, self->palette.dialog_secondary);
    (void)wrapped_text(device,
                       ui_text_line(settings_motion_help(self), folio_state_language(self->state)),
                       row, true);
}

static RECT draw_settings_choice(const struct folio_window *_Nonnull self, HDC device, RECT row,
                                 size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    struct ui_part_paint part = {.bounds = row,
                                 .dpi = dpi,
                                 .style = UI_PART_STYLE_COMMAND,
                                 .focus_inside = true,
                                 .selected = settings_row_current(self, index),
                                 .focused = (self->focus_cues || self->focus_cues_always) &&
                                            GetFocus() == self->command_layer &&
                                            index == self->command_selection};
    ui_part_draw(device, &self->palette, &part);
    if (settings_rows[index].kind == SETTINGS_ROW_MOTION)
    {
        draw_settings_motion_help(self, device, row);
        row.bottom = row.top + scale(base_settings_item_height, dpi);
    }
    if (settings_row_toggle(index))
    {
        draw_settings_mark(self, device, row, settings_row_current(self, index));
    }
    if (!settings_row_toggle(index) && settings_row_current(self, index))
    {
        RECT mark = settings_mark_rect(self, row);
        icon_paint_fill(device, mark, ICON_PAINT_SELECTION, self->palette.current_text);
    }

    return row;
}

static void draw_settings_row(const struct folio_window *_Nonnull self, HDC device, RECT row,
                              size_t index)
{
    UINT dpi = GetDpiForWindow(self->handle);
    bool choice = settings_row_choice(index);
    if (choice)
        row = draw_settings_choice(self, device, row, index);
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

static const struct note_tags *_Nullable surface_tags(const struct folio_window *_Nonnull self)
{
    if (management_surface(self))
        return nullptr;
    return self->command_surface == COMMAND_SURFACE_TAG_EDIT ? folio_state_edit_tags(self->state)
                                                             : folio_state_tag_filter(self->state);
}

static size_t tag_matches_count(const struct folio_window *_Nonnull self)
{
    if (management_surface(self))
        return management_row_count(self);
    return self->tag_candidates == nullptr ? 0 : tag_candidates_count(self->tag_candidates);
}

static bool tag_row_selected(const struct folio_window *_Nonnull self, size_t index)
{
    if (management_surface(self))
        return self->tag_management_view == TAG_MANAGEMENT_CHOOSE &&
               index == self->command_selection;
    const struct note_tags *_Nullable tags = surface_tags(self);
    return tags != nullptr && index < tag_matches_count(self) &&
           note_tags_contains(tags, tag_candidates_tag(self->tag_candidates, index));
}

/* Rebuild only on an input intent or catalog completion, never from paint/UIA reads. */
static HRESULT prepare_tag_candidates(const struct folio_window *_Nonnull self)
{
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
    {
        tag_candidates_invalidate(self->tag_candidates);
        return E_OUTOFMEMORY;
    }
    struct tag_candidates_source source = {.catalog = folio_state_tag_catalog_status(self->state) ==
                                                              TAG_CATALOG_AVAILABLE
                                                          ? folio_state_tag_catalog(self->state)
                                                          : nullptr,
                                           .selected = surface_tags(self),
                                           .query = query,
                                           .revision = self->surface_revision};
    enum candidate_create_outcome outcome = tag_candidates_prepare(self->tag_candidates, &source);
    utf8_text_destroy(query);
    return outcome == CANDIDATE_CREATED ? S_OK : E_OUTOFMEMORY;
}

static void tag_row_text(const struct folio_window *_Nonnull self, size_t index, char *_Nonnull out,
                         size_t capacity)
{
    if (management_surface(self) && self->tag_management_view == TAG_MANAGEMENT_PLAN)
    {
        management_row_text(self, index, out, capacity);
        return;
    }
    struct ui_text_request request = {
        .id = UI_TEXT_TAG_USAGE,
        .language = folio_state_language(self->state),
        .name = note_tag_bytes(tag_candidates_tag(self->tag_candidates, index)),
        .n = tag_candidates_usage(self->tag_candidates, index)};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void tag_catalog_line(const struct folio_window *_Nonnull self, char *_Nonnull out,
                             size_t capacity)
{
    out[0] = '\0';
    enum tag_catalog_status status = folio_state_tag_catalog_status(self->state);
    if (status == TAG_CATALOG_PREPARING)
    {
        (void)bounded_text_copy(
            out, capacity, ui_text_line(UI_TEXT_TAG_PREPARING, folio_state_language(self->state)));
        return;
    }
    if (status != TAG_CATALOG_AVAILABLE)
        return;
    size_t unavailable = tag_catalog_unavailable(folio_state_tag_catalog(self->state));
    if (unavailable == 0)
        return;
    struct ui_text_request request = {.id = UI_TEXT_TAG_UNAVAILABLE,
                                      .language = folio_state_language(self->state),
                                      .n = unavailable};
    (void)ui_text_format(&request, out, capacity);
}

static void draw_tag_row(const struct folio_window *_Nonnull self, HDC device, RECT row,
                         size_t index)
{
    bool focused = GetFocus() == self->command_layer && self->tag_focus == TAG_SURFACE_ROWS &&
                   index == self->command_selection;
    struct ui_part_paint part = {.bounds = row,
                                 .style = UI_PART_STYLE_CHOICE,
                                 .dpi = GetDpiForWindow(self->handle),
                                 .labeled = true,
                                 .focused =
                                     focused && (self->focus_cues || self->focus_cues_always),
                                 .selected = tag_row_selected(self, index)};
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    if (!management_surface(self))
    {
        draw_settings_mark(self, device, row, part.selected);
        ink.label.left += scale(28, part.dpi);
    }
    char text[draw_unit_limit];
    tag_row_text(self, index, text, sizeof text);
    SetTextColor(device, ink.text);
    draw_utf8(device, text, ink.label);
}

static void draw_tag_rows(const struct folio_window *_Nonnull self, HDC device)
{
    RECT rows = tag_rows_rect(self);
    int height = scale(base_command_row_height, GetDpiForWindow(self->handle));
    size_t count = tag_matches_count(self);
    size_t visible = command_visible_rows(self);
    for (size_t offset = 0; offset < visible && self->command_first + offset < count; ++offset)
    {
        RECT row = {rows.left, rows.top + (int)offset * height, rows.right,
                    rows.top + ((int)offset + 1) * height};
        draw_tag_row(self, device, row, self->command_first + offset);
    }
    if (count == 0 && folio_state_tag_catalog_status(self->state) == TAG_CATALOG_AVAILABLE)
    {
        enum ui_text label = GetWindowTextLengthW(self->command_input) == 0 ? UI_TEXT_TAG_EMPTY
                                                                            : UI_TEXT_TAG_NO_MATCH;
        SetTextColor(device, self->palette.dialog_secondary);
        draw_utf8(device, ui_text_line(label, folio_state_language(self->state)), rows);
    }
}

static void tag_notice_text(const struct folio_window *_Nonnull self, char *_Nonnull out,
                            size_t capacity)
{
    if (self->command_failure != FOLIO_STATE_READY)
    {
        (void)bounded_text_copy(
            out, capacity,
            folio_state_failure_line(self->command_failure, folio_state_language(self->state)));
        return;
    }
    tag_catalog_line(self, out, capacity);
}

static size_t management_row_count(const struct folio_window *_Nonnull self)
{
    switch (self->tag_management_view)
    {
    case TAG_MANAGEMENT_CHOOSE:
        return self->tag_candidates == nullptr ? 0 : tag_candidates_count(self->tag_candidates);
    case TAG_MANAGEMENT_NAME:
        return 0;
    case TAG_MANAGEMENT_PLAN:
    {
        struct tag_management_summary summary = folio_state_tag_management_summary(self->state);
        size_t count = 0;
        for (size_t at = 0; at < summary.prepared; ++at)
            count +=
                folio_state_tag_management_status(self->state, at) != TAG_APPLY_NOT_TARGET ? 1 : 0;
        return count;
    }
    }
    return 0;
}

static size_t management_plan_index(const struct folio_window *_Nonnull self, size_t row)
{
    struct tag_management_summary summary = folio_state_tag_management_summary(self->state);
    size_t count = 0;
    for (size_t at = 0; at < summary.prepared; ++at)
    {
        if (folio_state_tag_management_status(self->state, at) == TAG_APPLY_NOT_TARGET)
            continue;
        if (count++ == row)
            return at;
    }
    return summary.prepared;
}

static void management_row_text(const struct folio_window *_Nonnull self, size_t index,
                                char *_Nonnull out, size_t capacity)
{
    const enum ui_text reasons[] = {[TAG_APPLY_NOT_TARGET] = UI_TEXT_TAG_MANAGE_UNCHANGED,
                                    [TAG_APPLY_PENDING] = UI_TEXT_TAG_MANAGE_READY,
                                    [TAG_APPLY_CHANGED] = UI_TEXT_TAG_MANAGE_CHANGED,
                                    [TAG_APPLY_UNCHANGED] = UI_TEXT_TAG_MANAGE_UNCHANGED,
                                    [TAG_APPLY_UNREADABLE] = UI_TEXT_TAG_MANAGE_UNREADABLE,
                                    [TAG_APPLY_UNAVAILABLE] = UI_TEXT_TAG_MANAGE_UNAVAILABLE,
                                    [TAG_APPLY_LIMIT] = UI_TEXT_TAG_MANAGE_LIMIT,
                                    [TAG_APPLY_CONFLICT] = UI_TEXT_TAG_MANAGE_CONFLICT,
                                    [TAG_APPLY_HISTORY_FAILED] = UI_TEXT_TAG_MANAGE_HISTORY_FAILED,
                                    [TAG_APPLY_WRITE_FAILED] = UI_TEXT_TAG_MANAGE_WRITE_FAILED,
                                    [TAG_APPLY_OUT_OF_MEMORY] = UI_TEXT_TAG_MANAGE_MEMORY_FAILED};
    size_t at = management_plan_index(self, index);
    const struct tag_update *update = folio_state_tag_management_at(self->state, at);
    enum folio_language language = folio_state_language(self->state);
    enum tag_apply_status status = folio_state_tag_management_status(self->state, at);
    enum ui_text reason = reasons[status];
    if (status == TAG_APPLY_PENDING &&
        folio_state_tag_management_phase(self->state) == TAG_MANAGEMENT_RESULT)
        reason = UI_TEXT_TAG_MANAGE_UNPROCESSED;
    struct ui_text_request request = {.id = UI_TEXT_TAG_MANAGE_ROW,
                                      .language = language,
                                      .from = tag_update_category(update),
                                      .name = tag_update_note(update),
                                      .to = ui_text_line(reason, language)};
    (void)ui_text_format(&request, out, capacity);
}

static void management_status(const struct folio_window *_Nonnull self, char *_Nonnull out,
                              size_t capacity)
{
    out[0] = '\0';
    if (self->tag_management_view != TAG_MANAGEMENT_PLAN)
    {
        tag_notice_text(self, out, capacity);
        return;
    }
    struct tag_management_summary summary = folio_state_tag_management_summary(self->state);
    struct ui_text_request request = {.language = folio_state_language(self->state)};
    switch (summary.phase)
    {
    case TAG_MANAGEMENT_CLOSED:
        request.id = UI_TEXT_EMPTY;
        break;
    case TAG_MANAGEMENT_PREPARING:
        request.id = UI_TEXT_TAG_MANAGE_PREPARING;
        request.k = summary.prepared;
        request.n = summary.total;
        break;
    case TAG_MANAGEMENT_PREVIEW:
        request.id = UI_TEXT_TAG_MANAGE_PREVIEW;
        request.k = summary.pending;
        request.n = summary.skipped;
        break;
    case TAG_MANAGEMENT_APPLYING:
        request.id = UI_TEXT_TAG_MANAGE_APPLYING;
        request.k = summary.changed;
        request.n = summary.pending;
        break;
    case TAG_MANAGEMENT_RESULT:
        request.id = UI_TEXT_TAG_MANAGE_RESULT;
        request.k = summary.changed;
        request.n = summary.skipped;
        request.offset = summary.failed;
        break;
    }
    (void)ui_text_format(&request, out, capacity);
}

static bool management_retryable(const struct folio_window *_Nonnull self)
{
    struct tag_management_summary summary = folio_state_tag_management_summary(self->state);
    return summary.phase == TAG_MANAGEMENT_RESULT &&
           (summary.pending != 0 || summary.failed != 0 || summary.skipped != 0 ||
            summary.prepared < summary.total || summary.notice != FOLIO_STATE_READY);
}

static enum ui_text management_plan_button(const struct folio_window *_Nonnull self, size_t slot)
{
    bool primary = slot == ACCESSIBLE_SURFACE_TAG_PRIMARY;
    switch (folio_state_tag_management_phase(self->state))
    {
    case TAG_MANAGEMENT_CLOSED:
        return UI_TEXT_TAG_MANAGE_CLOSE;
    case TAG_MANAGEMENT_PREPARING:
    case TAG_MANAGEMENT_APPLYING:
        return UI_TEXT_TAG_MANAGE_STOP;
    case TAG_MANAGEMENT_PREVIEW:
        return primary ? UI_TEXT_TAG_MANAGE_APPLY : UI_TEXT_TAG_MANAGE_BACK;
    case TAG_MANAGEMENT_RESULT:
        return primary && management_retryable(self) ? UI_TEXT_TAG_MANAGE_RETRY
                                                     : UI_TEXT_TAG_MANAGE_CLOSE;
    }
    return UI_TEXT_EMPTY;
}

static enum ui_text management_button_label(const struct folio_window *_Nonnull self, size_t slot)
{
    bool primary = slot == ACCESSIBLE_SURFACE_TAG_PRIMARY;
    switch (self->tag_management_view)
    {
    case TAG_MANAGEMENT_CHOOSE:
        return primary ? UI_TEXT_TAG_MANAGE_RENAME : UI_TEXT_TAG_MANAGE_REMOVE;
    case TAG_MANAGEMENT_NAME:
        return primary ? UI_TEXT_TAG_MANAGE_PREPARE : UI_TEXT_TAG_MANAGE_BACK;
    case TAG_MANAGEMENT_PLAN:
        return management_plan_button(self, slot);
    }
    return UI_TEXT_EMPTY;
}

static bool management_button_enabled(const struct folio_window *_Nonnull self, size_t slot)
{
    if (self->command_composing)
        return false;
    if (slot == ACCESSIBLE_SURFACE_CLOSE || (slot == ACCESSIBLE_SURFACE_TAG_SECONDARY &&
                                             self->tag_management_view != TAG_MANAGEMENT_CHOOSE))
        return true;
    switch (self->tag_management_view)
    {
    case TAG_MANAGEMENT_CHOOSE:
        return self->command_selection < management_row_count(self);
    case TAG_MANAGEMENT_NAME:
        return GetWindowTextLengthW(self->command_input) != 0;
    case TAG_MANAGEMENT_PLAN:
        break;
    }
    if (folio_state_tag_management_phase(self->state) == TAG_MANAGEMENT_PREVIEW)
        return folio_state_tag_management_summary(self->state).pending != 0;
    return true;
}

static void management_intent_text(const struct folio_window *_Nonnull self, char *_Nonnull out,
                                   size_t capacity)
{
    const struct note_tag *from = folio_state_tag_management_source(self->state);
    const struct note_tag *_Nullable to = folio_state_tag_management_target(self->state);
    struct ui_text_request request = {.id = to == nullptr ? UI_TEXT_TAG_MANAGE_INTENT_REMOVE
                                                          : UI_TEXT_TAG_MANAGE_INTENT_RENAME,
                                      .language = folio_state_language(self->state),
                                      .name = note_tag_bytes(from),
                                      .from = note_tag_bytes(from),
                                      .to = to == nullptr ? nullptr : note_tag_bytes(to)};
    (void)ui_text_format(&request, out, capacity);
}

static void management_notice(const struct folio_window *_Nonnull self, char *_Nonnull out,
                              size_t capacity)
{
    out[0] = '\0';
    if (self->command_failure != FOLIO_STATE_READY)
    {
        (void)bounded_text_copy(
            out, capacity,
            folio_state_failure_line(self->command_failure, folio_state_language(self->state)));
        return;
    }
    if (self->tag_management_view != TAG_MANAGEMENT_PLAN)
        return;
    struct tag_management_summary summary = folio_state_tag_management_summary(self->state);
    if (summary.notice != FOLIO_STATE_READY && summary.notice != FOLIO_STATE_CANCELLED)
    {
        (void)bounded_text_copy(
            out, capacity,
            folio_state_failure_line(summary.notice, folio_state_language(self->state)));
        return;
    }
    if (summary.phase != TAG_MANAGEMENT_RESULT)
        return;
    size_t pending = summary.pending + summary.total - summary.prepared;
    struct ui_text_request request = {.language = folio_state_language(self->state), .n = pending};
    if (pending != 0)
        request.id = UI_TEXT_TAG_MANAGE_PENDING;
    if (pending == 0)
        request.id =
            summary.notice == FOLIO_STATE_CANCELLED ? UI_TEXT_TAG_MANAGE_STOPPED : UI_TEXT_EMPTY;
    (void)ui_text_format(&request, out, capacity);
}

static void tag_footer_summary(const struct folio_window *_Nonnull self, char *_Nonnull out,
                               size_t capacity)
{
    if (management_surface(self))
    {
        management_status(self, out, capacity);
        return;
    }
    const struct note_tags *_Nullable selected = surface_tags(self);
    struct ui_text_request request = {.id = UI_TEXT_TAG_SELECTED,
                                      .language = folio_state_language(self->state),
                                      .n = selected == nullptr ? 0 : note_tags_count(selected)};
    (void)ui_text_format(&request, out, capacity);
}

static enum ui_text tag_footer_hint(const struct folio_window *_Nonnull self)
{
    if (management_surface(self))
        return self->management_merging ? UI_TEXT_TAG_MANAGE_MERGE : UI_TEXT_TAG_MANAGE_SCOPE;
    return self->command_surface == COMMAND_SURFACE_TAG_EDIT ? UI_TEXT_TAG_EDIT_HINT
                                                             : UI_TEXT_TAG_FILTER_HINT;
}

static void tag_footer_notice(const struct folio_window *_Nonnull self, char *_Nonnull out,
                              size_t capacity)
{
    if (management_surface(self))
        management_notice(self, out, capacity);
    if (!management_surface(self))
        tag_notice_text(self, out, capacity);
}

static void draw_tag_footer(const struct folio_window *_Nonnull self, HDC device)
{
    RECT line = tag_footer_rect(self);
    enum folio_language language = folio_state_language(self->state);
    char text[ui_text_unit_limit];
    tag_footer_summary(self, text, sizeof text);
    SetTextColor(device, self->palette.current_text);
    line.top += wrapped_text(device, text, line, true);
    enum ui_text hint = tag_footer_hint(self);
    SetTextColor(device, self->palette.dialog_secondary);
    if (self->settings_metrics.tag_hint_visible)
        line.top += wrapped_text(device, ui_text_line(hint, language), line, true);
    tag_footer_notice(self, text, sizeof text);
    (void)wrapped_text(device, text, line, true);
    size_t first = tag_button_first(self);
    size_t count = tag_button_count(self);
    for (size_t slot = first; slot < first + count; ++slot)
        if (surface_button_visible(self, slot))
            draw_surface_button(self, device, slot);
}

static enum ui_text tag_input_label(const struct folio_window *_Nonnull self)
{
    if (!management_surface(self))
        return self->command_surface == COMMAND_SURFACE_TAG_EDIT ? UI_TEXT_TAG_EDIT_INPUT
                                                                 : UI_TEXT_TAG_FILTER_INPUT;
    switch (self->tag_management_view)
    {
    case TAG_MANAGEMENT_CHOOSE:
        return UI_TEXT_TAG_FILTER_INPUT;
    case TAG_MANAGEMENT_NAME:
        return UI_TEXT_TAG_MANAGE_NAME;
    case TAG_MANAGEMENT_PLAN:
        return UI_TEXT_TAG_MANAGE_CHANGE;
    }
    return UI_TEXT_EMPTY;
}

static void management_source_text(const struct folio_window *_Nonnull self, char *_Nonnull out,
                                   size_t capacity)
{
    struct ui_text_request request = {.id = UI_TEXT_TAG_MANAGE_SOURCE,
                                      .language = folio_state_language(self->state),
                                      .name = note_tag_bytes(self->management_source)};
    (void)ui_text_format(&request, out, capacity);
}

static void draw_management_detail(const struct folio_window *_Nonnull self, HDC device, RECT input)
{
    if (!management_surface(self))
        return;
    if (!management_input(self))
    {
        char intent[draw_unit_limit];
        management_intent_text(self, intent, sizeof intent);
        draw_utf8(device, intent, input);
    }
    if (self->tag_management_view == TAG_MANAGEMENT_NAME)
    {
        char source[draw_unit_limit];
        management_source_text(self, source, sizeof source);
        (void)wrapped_text(device, source, tag_rows_rect(self), true);
    }
}

static void draw_tag_dialog(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    bool edit = self->command_surface == COMMAND_SURFACE_TAG_EDIT;
    ui_part_draw_surface(device, &self->palette, bounds);
    draw_floating_header(self, device,
                         management_surface(self)
                             ? UI_TEXT_TAG_MANAGE_TITLE
                             : (edit ? UI_TEXT_TAG_EDIT_TITLE : UI_TEXT_TAG_FILTER_TITLE));
    RECT input = theme_input_rect(self);
    RECT label = {input.left, input.top - scale(20, GetDpiForWindow(self->handle)), input.right,
                  input.top};
    SetTextColor(device, self->palette.current_text);
    draw_utf8(device, ui_text_line(tag_input_label(self), folio_state_language(self->state)),
              label);
    draw_input_field(self, device, self->command_input, input);
    if (!management_surface(self) || self->tag_management_view != TAG_MANAGEMENT_NAME)
        draw_tag_rows(self, device);
    draw_management_detail(self, device, input);
    draw_tag_footer(self, device);
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
    /* #941: the destination picker scrolls and reveals its rows with the same helpers. */
    if (move_surface(self))
        return self->move_count;
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

static void draw_about_text(const struct folio_window *_Nonnull self, HDC device, size_t index)
{
    RECT row = about_text_rect(self, index);
    char text[draw_unit_limit];
    about_text_line(self, index, text);
    SetTextColor(device, about_texts[index].secondary ? self->palette.dialog_secondary
                                                      : self->palette.current_text);
    HGDIOBJ previous = SelectObject(device, about_text_font(self, index));
    if (about_texts[index].heading)
        draw_units(device, text, row, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    else
        (void)wrapped_text(device, text, row, true);
    SelectObject(device, previous);
}

/* 設定と同じく、固定footerと題はスクロールする内容に参加しない（ADR0097）。 */
static void draw_about_dialog(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    ui_part_draw_surface(device, &self->palette, bounds);
    draw_floating_header(self, device, UI_TEXT_ABOUT_TITLE);
    RECT content = settings_content_rect(self);
    int saved = SaveDC(device);
    IntersectClipRect(device, content.left, content.top, content.right, content.bottom);
    for (size_t at = 0; at < about_text_count; ++at)
        draw_about_text(self, device, at);
    for (size_t at = 0; at < about_link_count; ++at)
        draw_surface_button_in(self, device, about_links[at].button, about_link_rect(self, at));
    RestoreDC(device, saved);
    if (settings_has_notice(self))
    {
        SetTextColor(device, self->palette.current_text);
        (void)wrapped_text(
            device,
            folio_state_failure_line(settings_notice(self), folio_state_language(self->state)),
            about_notice_rect(self), true);
    }
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_BACK);
}

/* 設定の枠を共有する二つの副面。 */
static void draw_settings_branch(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    if (about_surface(self))
        draw_about_dialog(self, device, bounds);
    else if (move_surface(self))
        draw_move_dialog(self, device, bounds);
    else
        draw_theme_dialog(self, device, bounds);
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
    if (RectVisible(device, &title))
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
    case COMMAND_SURFACE_TAG_EDIT:
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
        draw_tag_dialog(self, device, bounds);
        return;
    case COMMAND_SURFACE_DISCOVERY:
        draw_discovery(self, device, bounds);
        return;
    case COMMAND_SURFACE_THEMES:
    case COMMAND_SURFACE_ABOUT:
    case COMMAND_SURFACE_MOVE:
        draw_settings_branch(self, device, bounds);
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

/* 操作帯の配置と更新判定が共有する外枠。更新範囲外なら字面の計測もしない。 */
static RECT action_band_rect(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT band = regions_for(self).pane;
    band.top += scale(base_caption_height, dpi);
    band.bottom = band.top + scale(base_action_height, dpi);
    return band;
}

static RECT tag_strip_rect(const struct folio_window *_Nonnull self)
{
    if (!tag_strip_visible(self))
        return (RECT){0};
    RECT band = action_band_rect(self);
    UINT dpi = GetDpiForWindow(self->handle);
    band.top = band.bottom;
    band.bottom = band.top + scale(base_tag_height, dpi);
    band.left += scale(base_pane_left, dpi);
    band.right -= scale(ui_part_edge, dpi);
    return band;
}

static void tag_strip_text(const struct folio_window *_Nonnull self, char *_Nonnull out,
                           size_t capacity)
{
    enum ui_text label =
        self->tag_strip_count == 0 ? UI_TEXT_TAG_STRIP_EMPTY : UI_TEXT_TAG_STRIP_COUNT;
    if (self->tag_strip_outcome != FOLIO_STATE_READY)
        label = UI_TEXT_TAG_STRIP_UNAVAILABLE;
    struct ui_text_request request = {
        .id = label, .language = folio_state_language(self->state), .n = self->tag_strip_count};
    if (ui_text_format(&request, out, capacity) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void draw_tag_strip(const struct folio_window *_Nonnull self, HDC device)
{
    struct ui_part_paint part = {
        .bounds = tag_strip_rect(self),
        .style = UI_PART_STYLE_COMMAND,
        .dpi = GetDpiForWindow(self->handle),
        .labeled = true,
        .hovered = self->hover_part == ACCESSIBLE_TAG_EDIT,
        .pressed = self->pressed_part == ACCESSIBLE_TAG_EDIT && self->pressed_inside,
        .focused = GetFocus() == self->handle && self->keyboard_target == ACCESSIBLE_TAG_EDIT &&
                   (self->focus_cues || self->focus_cues_always),
        .disabled = !part_enabled(self, ACCESSIBLE_TAG_EDIT)};
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    char text[ui_text_unit_limit];
    tag_strip_text(self, text, sizeof text);
    SetTextColor(device, ink.text);
    draw_utf8(device, text, ink.label);
}

/* 本文操作帯の A 様式の群（帯 36 = 4+28+4）。幅は現在の言語の字面を頭と同じ等幅の書体と字間で
 * 測る（#122）。字を出すと窓の縁 16 に収まらないときだけ、絵を持つ項目を検索・保存の順に
 * 絵だけへ縮め、消さない（ADR0060 の決定 1）。描画・当たり判定・UIA・メニューの位置が共有する。 */
static void action_layout(const struct folio_window *_Nonnull self, RECT *_Nonnull out,
                          bool *_Nonnull labeled)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT band = action_band_rect(self);
    RECT client;
    GetClientRect(self->handle, &client);
    int left = band.left + scale(base_pane_left, dpi);
    int height = scale(ui_part_height, dpi);
    int top = band.top + (band.bottom - band.top - height) / 2;
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

static bool paint_intersects(RECT bounds, RECT damage)
{
    RECT touched = {0};
    return IntersectRect(&touched, &bounds, &damage) != 0;
}

/* 本文の左の空きに、見えている論理行の番号を右寄せで描く（ADR 0026 の決定 1 / 3）。
 * y は note_pane が主窓の client 座標へ直して返す。
 * 帯に掛からない更新矩形では番号の一巡（EM_LINEINDEX / EM_POSFROMCHAR）ごと飛ばし、
 * 升目は帯でクリップして上の操作行へはみ出させない（ADR 0026 の補正 7）。 */
static void draw_gutter(const struct folio_window *_Nonnull self, HDC device, RECT damage)
{
    RECT band = gutter_rect(self);
    if (!gutter_visible(self) || !paint_intersects(band, damage))
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
            return self->image_line;
        case PANE_NOTICE_DISCOVERY_CHANGED:
            return UI_TEXT_DISCOVERY_CHANGED;
        case PANE_NOTICE_COMMAND_RESULT:
            line = UI_TEXT_COMMAND_CAPTION;
            break;
        case PANE_NOTICE_NOTE_UNEDITABLE:
            line = UI_TEXT_NOTE_OPENED_IN_VIEW;
            break;
        case PANE_NOTICE_SESSION_BLOCKED:
            line = self->session_reason;
            break;
        case PANE_NOTICE_OPEN_LINK:
            line = self->open_line;
            break;
        }
    return line;
}

static RECT pane_notice_rect(const struct folio_window *_Nonnull self, RECT client)
{
    UINT dpi = GetDpiForWindow(self->handle);
    return (RECT){scale(16, dpi), client.bottom - pane_notice_height(self),
                  client.right - scale(16, dpi), client.bottom};
}

static void help_hint(enum folio_language language, enum ui_text id, char *_Nonnull out)
{
    const char *palette = "";
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        if (folio_shortcut_route(binding) == FOLIO_SHORTCUT_CHARACTER &&
            folio_shortcut_command(binding) == FOLIO_COMMAND_HELP)
            palette = folio_shortcut_label(binding, language);
    }
    struct ui_text_request request = {.id = id,
                                      .language = language,
                                      .from =
                                          folio_shortcut_command_hint(FOLIO_COMMAND_HELP, language),
                                      .to = palette};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void status_note_text(const struct folio_window *_Nonnull self, enum ui_text id,
                             char *_Nonnull out)
{
    struct note_times_view times = folio_state_note_times(self->state);
    struct ui_text_request request = {.id = id,
                                      .language = folio_state_language(self->state),
                                      .from = times.updated,
                                      .to = times.created};
    switch (times.state)
    {
    case NOTE_TIMES_NONE:
        request.id = UI_TEXT_EMPTY;
        break;
    case NOTE_TIMES_UNSAVED:
        request.id = UI_TEXT_STATUS_UNSAVED;
        break;
    case NOTE_TIMES_UNAVAILABLE:
        request.id = UI_TEXT_STATUS_TIMES_UNAVAILABLE;
        break;
    case NOTE_TIMES_READY:
        break;
    }
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

/* The tooltip and UIA retain full information even when the painted row is narrower. */
static void status_text(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    enum ui_text notice = pane_notice_line(self);
    if (notice != UI_TEXT_EMPTY)
    {
        (void)bounded_text_copy(out, draw_unit_limit,
                                self->notice == PANE_NOTICE_COMMAND_RESULT
                                    ? self->result_line
                                    : ui_text_line(notice, language));
        return;
    }
    char note[draw_unit_limit];
    char keys[draw_unit_limit];
    status_note_text(self, UI_TEXT_STATUS_NOTE_TIMES, note);
    help_hint(language, UI_TEXT_STATUS_KEYS, keys);
    if (note[0] == '\0')
    {
        (void)bounded_text_copy(out, draw_unit_limit, keys);
        return;
    }
    struct ui_text_request request = {
        .id = UI_TEXT_STATUS_CONTENT, .language = language, .name = note, .from = keys};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void draw_status_information(const struct folio_window *_Nonnull self, HDC device,
                                    RECT bounds)
{
    char note[draw_unit_limit];
    char keys[draw_unit_limit];
    status_note_text(self, UI_TEXT_STATUS_NOTE_TIMES, note);
    help_hint(folio_state_language(self->state), UI_TEXT_STATUS_KEYS, keys);
    int gap = scale(24, GetDpiForWindow(self->handle));
    int width = bounds.right - bounds.left;
    int key_width = measure_utf8(device, keys);
    if (measure_utf8(device, note) + key_width + gap > width)
        status_note_text(self, UI_TEXT_STATUS_NOTE_UPDATED, note);
    if (measure_utf8(device, note) + key_width + gap > width)
    {
        help_hint(folio_state_language(self->state), UI_TEXT_STATUS_HELP, keys);
        key_width = measure_utf8(device, keys);
    }
    RECT key_rect = bounds;
    key_rect.left = bounds.right - key_width;
    if (key_rect.left < bounds.left)
        key_rect.left = bounds.left;
    draw_utf8(device, keys, key_rect);
    bounds.right = key_rect.left - gap;
    if (bounds.right > bounds.left)
        draw_utf8(device, note, bounds);
}

static void draw_status_notice(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    enum ui_text line = pane_notice_line(self);
    if (self->image_pending && self->notice == PANE_NOTICE_NONE)
    {
        RECT track = bounds;
        track.left = track.right - scale(80, GetDpiForWindow(self->handle));
        track.top = (bounds.top + bounds.bottom) / 2 - 2;
        track.bottom = track.top + 4;
        fill_settings_rect(device, track, self->palette.status_text);
        LONG width = (track.right - track.left) / 4;
        track.left += (LONG)(self->image_meter % 24) * (track.right - track.left - width) / 23;
        track.right = track.left + width;
        fill_settings_rect(device, track, self->palette.status_background);
        bounds.right -= scale(90, GetDpiForWindow(self->handle));
    }
    draw_utf8(device,
              self->notice == PANE_NOTICE_COMMAND_RESULT
                  ? self->result_line
                  : ui_text_line(line, folio_state_language(self->state)),
              bounds);
}

static void draw_pane_notice(const struct folio_window *_Nonnull self, HDC device, RECT client)
{
    RECT band = {0, client.bottom - pane_notice_height(self), client.right, client.bottom};
    fill_settings_rect(device, band, self->palette.status_background);
    RECT edge = band;
    edge.bottom = edge.top + 1;
    fill_settings_rect(device, edge,
                       self->palette.contrast == FOLIO_CONTRAST_HIGH ? self->palette.status_text
                                                                     : self->palette.dialog_border);
    SelectObject(device, self->mono_font);
    SetBkMode(device, TRANSPARENT);
    SetTextCharacterExtra(device, 0);
    SetTextColor(device, self->palette.status_text);
    RECT bounds = pane_notice_rect(self, client);
    if (pane_notice_line(self) == UI_TEXT_EMPTY)
        draw_status_information(self, device, bounds);
    else
        draw_status_notice(self, device, bounds);
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
    int top = pane.top + scale(note_tools_height(self) + base_pane_top, dpi);
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
    /* 「表示」は言葉の書体、「NENE FOLIO」は等幅（#1222）。 */
    const char *heading = ui_text_line(UI_TEXT_LAYOUT_GROUP, folio_state_language(self->state));
    SelectObject(device, word_font(self));
    int heading_width = measure_utf8(device, heading);
    if (logo.right - logo.left >= heading_width + scale(128, dpi))
    {
        RECT label = logo;
        label.left = label.right - heading_width;
        SetTextColor(device, self->palette.current_text);
        draw_units(device, heading, label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        logo.right = label.left - scale(16, dpi);
    }
    SelectObject(device, self->mono_font);
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

/* 境界は既存の本文余白の中に描く。静止線と操作中の線は同じ文字系色を使う。 */
static void draw_splitter(const struct folio_window *_Nonnull self, HDC device)
{
    RECT bounds = regions_for(self).splitter;
    if (IsRectEmpty(&bounds))
        return;
    bool focused = part_focus_shown(self, ACCESSIBLE_SPLITTER);
    bool active = self->pressed_part == ACCESSIBLE_SPLITTER ||
                  self->hover_part == ACCESSIBLE_SPLITTER || focused;
    UINT dpi = GetDpiForWindow(self->handle);
    int thickness = scale(active ? 2 : 1, dpi);
    int middle = (bounds.left + bounds.right) / 2;
    RECT line = {middle - thickness / 2, bounds.top, middle - thickness / 2 + thickness,
                 bounds.bottom};
    COLORREF color = focused ? self->palette.current_text : self->palette.header_text;
    if (!part_enabled(self, ACCESSIBLE_SPLITTER))
        color = self->palette.border;
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(device, &line, brush);
    DeleteObject(brush);
}

static void draw_minimap(const struct folio_window *_Nonnull self, HDC device, RECT damage)
{
    RECT bounds = minimap_rect(self);
    if (!paint_intersects(bounds, damage))
        return;
    struct note_minimap_paint paint;
    if (minimap_paint_for(self, &paint))
        note_minimap_draw(self->minimap, device, &self->palette, &paint);
}

/* #975 / ADR0114: the headings column, painted beside the body like the minimap. */
static void draw_outline(const struct folio_window *_Nonnull self, HDC device, RECT damage)
{
    RECT bounds = outline_rect(self);
    if (IsRectEmpty(&bounds) || !paint_intersects(bounds, damage))
        return;
    enum folio_language language = folio_state_language(self->state);
    char16_t title[ui_text_unit_limit];
    char16_t empty[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(ui_text_line(UI_TEXT_OUTLINE, language), title, ui_text_unit_limit,
                        &written) != UTF16_TEXT_FILL_READY ||
        utf16_text_fill(ui_text_line(UI_TEXT_OUTLINE_EMPTY, language), empty, ui_text_unit_limit,
                        &written) != UTF16_TEXT_FILL_READY)
        return;
    struct note_outline_paint paint = outline_paint_for(self, title, empty);
    note_outline_draw(self->outline, self->pane, device, &paint);
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
        RECT caption = caption_rect(self);
        if (paint_intersects(caption, damage))
        {
            draw_breadcrumb(self, device, caption);
            draw_mode_group(self, device);
        }
        if (paint_intersects(action_band_rect(self), damage))
            draw_actions(self, device);
        if (paint_intersects(tag_strip_rect(self), damage))
            draw_tag_strip(self, device);
    }
    SetTextCharacterExtra(device, 0);
    if (!document_open(self))
    {
        draw_guide(self, device);
    }
    draw_minimap(self, device, damage);
    draw_outline(self, device, damage);
    draw_gutter(self, device, damage);
    draw_splitter(self, device);
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
        if (paint_intersects(regions_for(self).header, damage))
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
    if (pane_inset_changed(self))
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
    clear_images(self);
    request_minimap(self, MINIMAP_UPDATE_CLEAR);
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
        /* 編集のあいだは知らせるだけだった外の変更を、閲覧へ戻ったここで読み直す。後へ回すと、
         * 呼び手（リンクで開く・検索の一致）が古い本文で選んだ箇所を消してしまう。 */
        if (outcome == FOLIO_STATE_READY)
            show_outside_change(self);
        /* NOTEでは可視pane、それ以外は従来の索引へ戻す（ADR0047）。 */
        SetFocus(surface_return_target(self, self->handle));
    }
    return outcome;
}

/* 編集中の本文を取り出して保存する。取り出せない理由も保存の失敗も 1 つの結果に写す。 */
static enum folio_state_outcome store_body_mode(struct folio_window *_Nonnull self,
                                                enum note_save_mode mode)
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
    if (outcome != FOLIO_STATE_READY)
        return outcome;
    switch (mode)
    {
    case NOTE_SAVE_EDITS:
        return folio_state_store_note(self->state, units, count);
    case NOTE_SAVE_CHECK_CURRENT:
        return folio_state_store_note_checked(self->state, units, count);
    }
    return FOLIO_STATE_NOTE_STORE_FAILED;
}

static enum folio_state_outcome store_body(struct folio_window *_Nonnull self)
{
    return store_body_mode(self, NOTE_SAVE_EDITS);
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
    [FOLIO_STATE_TAG_INVALID] = true,
    [FOLIO_STATE_TAG_LIMIT] = true,
    [FOLIO_STATE_TAGS_UNAVAILABLE] = true,
    [FOLIO_STATE_TAGS_STALE] = true,
    [FOLIO_STATE_NOTE_SOURCE_CHANGED] = true,
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
    [FOLIO_STATE_DISCOVERY_QUERY_LIMIT] = true,
    [FOLIO_STATE_DISCOVERY_QUERY_INVALID] = true,
    [FOLIO_STATE_FILTERED] = true,
    [FOLIO_STATE_ORDERED] = true,
    [FOLIO_STATE_MOVE_ROLLBACK_PENDING] = true,
    [FOLIO_STATE_PANE_UNAVAILABLE] = false,
    [FOLIO_STATE_PANE_LOAD_FAILED] = false,
    [FOLIO_STATE_PANE_RECOLOR_FAILED] = false,
    [FOLIO_STATE_WEB_LINK_REJECTED] = true,
    [FOLIO_STATE_WEB_LINK_UNAVAILABLE] = true,
    [FOLIO_STATE_CODE_COPY_REJECTED] = true,
    [FOLIO_STATE_SOURCE_UNMAPPED] = true,
    [FOLIO_STATE_LEDGER_CHANGED_OUTSIDE] = true,
    [FOLIO_STATE_LEDGER_REFRESHED] = true,
    [FOLIO_STATE_CLIPBOARD_BUSY] = true,
    [FOLIO_STATE_CLIPBOARD_UNAVAILABLE] = true,
    [FOLIO_STATE_EXPORT_TOO_LARGE] = true,
    [FOLIO_STATE_EXPORT_EXISTS] = true,
    [FOLIO_STATE_EXPORT_FAILED] = true,
    [FOLIO_STATE_EXPORT_EXTERNAL_IMAGES] = false,
    [FOLIO_STATE_EXPORT_IMAGE_UNREADABLE] = true,
    [FOLIO_STATE_TRANSFER_UNAVAILABLE] = true,

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

static int tag_buttons_height(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int count = (int)tag_button_count(self);
    int width = (bounds.right - bounds.left - scale(6 * (count - 1), dpi)) / count - scale(16, dpi);
    RECT label = {0, 0, width, 0};
    size_t first = tag_button_first(self);
    int height = scale(ui_part_height, dpi);
    for (size_t slot = first; slot < first + tag_button_count(self); ++slot)
    {
        int measured = wrapped_text(device,
                                    ui_text_line(surface_button_label(self, slot),
                                                 folio_state_language(self->state)),
                                    label, false) +
                       scale(8, dpi);
        if (measured > height)
            height = measured;
    }
    return height;
}

static bool prepare_tag_metrics(struct folio_window *_Nonnull self)
{
    HDC device = GetDC(self->command_layer);
    if (device == nullptr)
        return false;
    HGDIOBJ old = SelectObject(device, self->mono_font);
    UINT dpi = GetDpiForWindow(self->handle);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    self->settings_metrics.tag_button_height = tag_buttons_height(self, device, bounds);
    char text[draw_unit_limit];
    tag_footer_notice(self, text, sizeof text);
    int notice = text[0] == '\0' ? 0 : wrapped_text(device, text, bounds, false);
    enum ui_text hint = tag_footer_hint(self);
    int guidance =
        wrapped_text(device, ui_text_line(hint, folio_state_language(self->state)), bounds, false);
    tag_footer_summary(self, text, sizeof text);
    int summary = text[0] == '\0' ? 0 : wrapped_text(device, text, bounds, false);
    int fixed = self->settings_metrics.tag_button_height + notice + summary + scale(8, dpi);
    int available =
        bounds.bottom - theme_input_rect(self).bottom - scale(base_command_row_height + 16, dpi);
    self->settings_metrics.tag_hint_visible = available >= fixed + guidance;
    self->settings_metrics.tags_footer =
        fixed + (self->settings_metrics.tag_hint_visible ? guidance : 0);
    SelectObject(device, old);
    ReleaseDC(self->command_layer, device);
    return true;
}

static int settings_motion_help_height(const struct folio_window *_Nonnull self, HDC device,
                                       RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    bounds.left += scale(32, dpi);
    bounds.right -= scale(4, dpi);
    enum folio_language language = folio_state_language(self->state);
    int normal = wrapped_text(device, ui_text_line(UI_TEXT_MOTION_HELP, language), bounds, false);
    int unavailable =
        wrapped_text(device, ui_text_line(UI_TEXT_MOTION_STATIC, language), bounds, false);
    return (normal > unavailable ? normal : unavailable) + scale(4, dpi);
}

/* 行の高さと釦の列数は幅・DPI・書体・言語だけで決まるので、設定と同じ失効に乗せる。 */
static void about_surface_metrics(const struct folio_window *_Nonnull self, HDC device, RECT bounds,
                                  struct command_surface_metrics *_Nonnull metrics)
{
    for (size_t at = 0; at < about_text_count; ++at)
    {
        char text[draw_unit_limit];
        about_text_line(self, at, text);
        HGDIOBJ previous = SelectObject(device, about_text_font(self, at));
        metrics->about_text[at] = wrapped_text(device, text, bounds, false);
        SelectObject(device, previous);
    }
    int width = bounds.right - bounds.left;
    int gaps = scale(base_settings_item_gap, metrics->dpi) * ((int)about_link_count - 1);
    int room = (width - gaps) / (int)about_link_count - scale(ui_part_padding * 2, metrics->dpi);
    bool fits = width >= scale(360, metrics->dpi);
    for (size_t at = 0; at < about_link_count; ++at)
        fits = fits &&
               measure_utf8(device, ui_text_line(about_links[at].label, metrics->language)) <= room;
    metrics->about_single_row = fits;
    metrics->about_ready = true;
    metrics->about_footer = metrics->notice + scale(ui_part_height + 8, metrics->dpi);
}

/* 同じ幅・DPI・書体・言語なら測り直さない。アプリの情報の行は、その面で未計測なら測る。 */
static bool surface_metrics_current(const struct folio_window *_Nonnull self,
                                    const struct command_surface_metrics *_Nonnull wanted)
{
    const struct command_surface_metrics *held = &self->settings_metrics;
    bool same = held->width == wanted->width && held->dpi == wanted->dpi &&
                held->font == wanted->font && held->language == wanted->language;
    return same && (held->about_ready || !about_surface(self));
}

static bool prepare_surface_metrics(struct folio_window *_Nonnull self)
{
    if (command_keys(self))
        return prepare_help_metrics(self);
    if (tag_surface(self))
        return prepare_tag_metrics(self);
    if (!settings_surface(self))
        return true;
    RECT bounds = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    struct command_surface_metrics metrics = {.width = bounds.right - bounds.left,
                                              .dpi = GetDpiForWindow(self->handle),
                                              .font = self->mono_font,
                                              .language = folio_state_language(self->state)};
    if (surface_metrics_current(self, &metrics))
        return true;
    HDC device = GetDC(self->command_layer);
    if (device == nullptr)
        return false;
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SetTextCharacterExtra(device, scale(base_tracking, metrics.dpi));
    bool ready = band_outcomes_height(device, metrics.language, bounds, &metrics.notice);
    metrics.motion_help = settings_motion_help_height(self, device, bounds);
    metrics.settings_footer =
        metrics.notice + surface_footer_text_height(self, device, bounds, false);
    metrics.themes_footer = metrics.notice +
                            surface_footer_text_height(self, device, bounds, true) +
                            scale(ui_part_height + 8, metrics.dpi);
    if (about_surface(self))
        about_surface_metrics(self, device, bounds, &metrics);
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
    size_t at = bounded_text_copy(
        text, sizeof text, folio_state_failure_line(FOLIO_STATE_REPLACE_BAD_PATTERN, language));
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
    int first_top = replacing ? scale(24, dpi) : content.top;
    int second_top = first_top + control + scale(16, dpi);
    band_action_layout(layout, content.right, replacing ? second_top : first_top);
    if (replacing)
    {
        layout->label[0] = (RECT){content.left, scale(8, dpi), content.right, first_top};
        layout->close =
            (RECT){content.right - control, first_top, content.right, first_top + control};
    }
    layout->field[0] = (RECT){content.left, first_top,
                              (replacing ? layout->close.left : layout->action[0].left) - gap,
                              first_top + control};
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
    /* The permanent status bar owns the entire bottom inset (ADR0087). */
    reduce_band_spacing(&layout->pane_top, scale(8, dpi), &shortage);
    reduce_band_spacing(&trailing, scale(8, dpi), &shortage);
    if (layout->narrow && layout->mode == COMMAND_SURFACE_SEARCH)
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
    int available = pane.bottom - pane.top - scale(note_tools_height(self), dpi);
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
    return discovery_surface(self) || self->command_surface == COMMAND_SURFACE_EX ||
           self->command_surface == COMMAND_SURFACE_PALETTE ||
           self->command_surface == COMMAND_SURFACE_SEARCH ||
           self->command_surface == COMMAND_SURFACE_REPLACE || choice_surface(self) ||
           about_surface(self) || tag_surface(self);
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

/* result_line を本文下の1行へ出し、同じ文言を読み上げへ通知する。 */
static void show_result_line(struct folio_window *_Nonnull self)
{
    set_pane_notice(self, PANE_NOTICE_COMMAND_RESULT);
    arrange(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    char16_t units[draw_unit_limit];
    if (wide_units(self->result_line, units) > 0)
        accessibility_host_notify(self->accessibility,
                                  (struct accessible_item){.kind = ACCESSIBLE_NOTICE}, units);
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
    show_result_line(self);
}

/* コードブロックのコピーの結果（ADR0098）。成功も失敗も本文下の同じ1行と読み上げ通知で知らせ、
 * 記憶域の不足でも箱を出さない。受け口はコマンド面が閉じているときだけ開く（web_link_ready）。 */
static void report_code_copy(struct folio_window *_Nonnull self, size_t slot,
                             enum folio_state_outcome outcome)
{
    enum folio_language language = folio_state_language(self->state);
    char16_t done[draw_unit_limit];
    if (outcome == FOLIO_STATE_READY)
        (void)bounded_text_copy(self->result_line, sizeof self->result_line,
                                ui_text_line(UI_TEXT_CODE_COPIED, language));
    else
        outcome_line(self, outcome, self->result_line);
    /* 成功したときだけ、押したボタンを短い間だけ済みの表示にする。 */
    if (outcome == FOLIO_STATE_READY && self->pane != nullptr &&
        wide_units(ui_text_line(UI_TEXT_CODE_COPY_DONE, language), done) > 0)
        note_pane_code_copied(self->pane, slot, done);
    show_result_line(self);
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
 * advance なら現選択をcoreへ渡す。前方は開始の次（空選択は同位置）、後方は開始より前。
 * 入力中（インクリメンタル）は、いまの一致の頭（後方なら末尾）から数え直し、同じ場所で伸ばす。 */
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
    /* 物理pxのまま持つ。96 DPIの単位へ直して戻すと丸めで1px変わる（#936）。 */
    self->wide_width = normal.right - normal.left;
    self->wide_dpi = GetDpiForWindow(self->handle);
    return true;
}

/* 一覧だけの直前の幅を、いまのDPIの物理pxで返す。DPIが同じなら測った値そのまま。 */
static int wide_width_at(const struct folio_window *_Nonnull self, UINT dpi)
{
    if (self->wide_dpi == 0 || dpi == self->wide_dpi)
        return self->wide_width;
    return MulDiv(self->wide_width, (int)dpi, (int)self->wide_dpi);
}

/* 現位置/高さを保つ。画面外の場合だけ幅と共通頭の位置をwork areaへ収める。 */
static bool set_layout_width(const struct folio_window *_Nonnull self, int pixel_width)
{
    RECT bounds;
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    if (!GetWindowRect(self->handle, &bounds) ||
        !GetMonitorInfoW(MonitorFromWindow(self->handle, MONITOR_DEFAULTTONEAREST), &monitor))
    {
        return false;
    }
    int width = pixel_width;
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
        return set_layout_width(self,
                                scale(base_drawer_window_width, GetDpiForWindow(self->handle)));
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
    return set_layout_width(self, wide_width_at(self, GetDpiForWindow(self->handle)));
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
    cancel_part_press(self);
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

static void release_tag_surface(struct folio_window *_Nonnull self, enum command_surface_mode next)
{
    if (!tag_surface(self) || self->command_surface == next)
        return;
    tag_candidates_invalidate(self->tag_candidates);
    folio_state_close_tag_edit(self->state);
    folio_state_close_tag_catalog(self->state);
    folio_state_stop_tag_management(self->state);
    folio_state_close_tag_management(self->state);
    note_tag_destroy(self->management_source);
    self->management_source = nullptr;
    self->tag_management_view = TAG_MANAGEMENT_CHOOSE;
}

static void change_surface_mode(struct folio_window *_Nonnull self, enum command_surface_mode mode)
{
    if (discovery_surface(self) && mode != COMMAND_SURFACE_DISCOVERY)
        folio_state_close_discovery(self->state);
    self->command_surface = mode;
    SendMessageW(self->command_input, EM_LIMITTEXT,
                 mode == COMMAND_SURFACE_DISCOVERY ? discovery_input_capacity - 1
                                                   : command_input_capacity - 1,
                 0);
}

[[nodiscard]] static bool set_command_surface(struct folio_window *_Nonnull self,
                                              enum command_surface_mode mode)
{
    discovery_motion_settle(self);
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
    release_tag_surface(self, mode);
    change_surface_mode(self, mode);
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
    if (self->tag_candidates != nullptr)
        tag_candidates_invalidate(self->tag_candidates);
    self->command_initializing = previous;
}

static void open_command_surface(struct folio_window *_Nonnull self,
                                 enum command_surface_mode surface)
{
    cancel_part_press(self);
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
    if (management_running(self))
    {
        folio_state_stop_tag_management(self->state);
        management_refresh(self);
        return;
    }
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

/* アプリの情報は設定の枠を共有し、EDITを持たない（ADR0097）。開いた直後のfocusは戻るに置き、
 * Enterの打ち過ぎで外部のブラウザーを開かない。 */
static void show_about_surface(struct folio_window *_Nonnull self)
{
    if (!set_command_surface(self, COMMAND_SURFACE_ABOUT))
        return;
    clear_command_status(self);
    self->command_selection = about_link_count;
    self->command_first = 0;
    rearrange_keeping_line(self);
    focus_command_input(self);
    redraw_command_layer(self);
}

/* 副面の戻る/Esc。アプリの情報からは、入ってきた設定の行を選んだ状態へ戻す。 */
static void return_to_settings(struct folio_window *_Nonnull self)
{
    bool about = about_surface(self);
    show_settings_surface(self, "");
    if (!about || self->command_surface != COMMAND_SURFACE_SETTINGS)
        return;
    self->command_selection = settings_button_index(ACCESSIBLE_SURFACE_ABOUT);
    reveal_command_selection(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

/* UIは意図だけを渡す。失敗は面内の通知行へ、成功しても面は閉じない（ADR0097）。 */
static void open_about_link(struct folio_window *_Nonnull self, size_t index)
{
    self->command_selection = index;
    report_outcome(self, folio_state_open_product_link(self->state, about_links[index].link));
    reveal_command_selection(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static bool about_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    bool tab = key == VK_TAB;
    bool backwards =
        key == VK_LEFT || key == VK_UP || (tab && (pressed_shortcut_modifiers() & FSHIFT) != 0);
    if (!backwards && !tab && key != VK_RIGHT && key != VK_DOWN)
        return false;
    size_t count = about_link_count + 1;
    self->command_selection = (self->command_selection + (backwards ? count - 1 : 1)) % count;
    reveal_command_selection(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
    return true;
}

static void about_activate(struct folio_window *_Nonnull self)
{
    if (self->command_selection < about_link_count)
        open_about_link(self, self->command_selection);
    else
        return_to_settings(self);
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

static void stop_management_for_close(struct folio_window *_Nonnull self)
{
    if (!management_running(self))
        return;
    folio_state_stop_tag_management(self->state);
    management_refresh(self);
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
    stop_management_for_close(self);
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

/* #904: the list-only layout narrows the window, and the layout is not kept across starts. So
 * closing in it records the geometry the window returns to: the width it had before (or, when
 * it was maximized before, that placement), not the narrow one the next start could not use. */
static struct window_placement closing_placement(const struct folio_window *_Nonnull self)
{
    struct window_placement closing = window_restore_capture(self->handle);
    if (!closing.placed || folio_state_layout_mode(self->state) != FOLIO_LAYOUT_DRAWER ||
        self->wide_width <= 0)
        return closing;
    if (self->restore_maximized)
    {
        struct window_placement wide = window_restore_from(&self->wide_placement, closing.dpi);
        wide.maximized = true;
        return wide;
    }
    closing.width = wide_width_at(self, closing.dpi);
    return closing;
}

static void finish_quit(struct folio_window *_Nonnull self, enum folio_state_outcome outcome)
{
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    clear_session_reason(self);
    /* #891: the place this window had, for the next start. Failing to write it never keeps the
     * window open; the next start then uses the earlier record or the initial place. */
    struct window_placement closing = closing_placement(self);
    if (closing.placed)
        (void)storage_state_record_window(self->storage, &closing);
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
    stop_management_for_close(self);
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
    stop_management_for_close(self);
    finish_quit(self, command_quit(self));
}

static void execute_save_quit_command(struct folio_window *_Nonnull self,
                                      const char *_Nonnull argument)
{
    (void)argument;
    stop_management_for_close(self);
    finish_quit(self, store_body(self));
}

/* 「編集」の操作。外で変わったファイルは、編集の元にする前に読み直す（ADR0119 の決定 4:
 * 古い本文から編集を始めると、その保存は外の変更とぶつかる）。enter_edit のほかの呼び手
 * （タグの適用・「ここを編集」・検索の一致）は、いまの本文から作った位置を持って入るので、
 * そこでは本文を入れ替えない。 */
static enum folio_state_outcome edit_current_file(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW &&
        self->command_surface == COMMAND_SURFACE_CLOSED)
        show_outside_change(self);
    return enter_edit(self);
}

static void execute_mode_command(struct folio_window *_Nonnull self, enum pane_mode mode)
{
    show_pane_layout(self);
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    switch (mode)
    {
    case PANE_MODE_EDIT:
        outcome = edit_current_file(self);
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
    /* 途中で外の変更を読み込んだときの 1 行は、面を隠すと消えるので立て直す。 */
    bool told =
        self->notice == PANE_NOTICE_OPEN_LINK && self->open_line == UI_TEXT_OUTSIDE_RELOADED;
    if (!hide_command_surface(self))
        return;
    if (told)
        set_pane_notice(self, PANE_NOTICE_OPEN_LINK);
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

/* ドラッグ・鍵・UIA・取消の配置経路。本文と索引の状態は各既存ownerに残す。 */
static void apply_drawer_width(struct folio_window *_Nonnull self, int width)
{
    if (self->drawer_width == width)
        return;
    self->drawer_width = width;
    rearrange_keeping_line(self);
    accessibility_host_schedule(self->accessibility);
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

/* #891 / ADR0108: the choice is this machine's, written at once to the window file. A failed
 * write leaves the choice as it was and says so with the warning sound. */
static void apply_window_restore(struct folio_window *_Nonnull self)
{
    bool restore = !storage_state_window(self->storage).restore;
    if (storage_state_set_window_restore(self->storage, restore) != STORAGE_READY)
        MessageBeep(MB_ICONWARNING);
    clear_command_status(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static void apply_motion(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome =
        folio_state_set_motion(self->state, !folio_state_motion(self->state));
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    clear_command_status(self);
    discovery_motion_reset(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static void apply_minimap(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome =
        folio_state_set_minimap(self->state, !folio_state_minimap(self->state));
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    if (settings_surface(self))
        clear_command_status(self);
    else if (!hide_command_surface(self))
        return;
    cancel_part_press(self);
    rearrange_keeping_line(self);
    request_minimap(self, MINIMAP_UPDATE_SETTLE);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

/* 閲覧の本文を新しい RTF で流し直し、選択を戻す（ADR 0031 の決定 6 の補正 1）。
 * **実測の補正**: EM_EXSETSEL は EM_SCROLLCARET を送らなくても選択を見える位置へ寄せる。
 * 空の選択を戻すと先頭へ飛ぶので、選んでいる一致があるときだけ戻す（ADR 0023 の
 * ハイライトは保ち、選んでいないときは流し直しが保ったスクロール位置をそのまま残す）。 */
/* #1026: after the same note was poured again and the body is back at its line. */
static void restore_outline(struct folio_window *_Nonnull self)
{
    if (self->pane == nullptr)
        return;
    struct note_outline_paint paint = outline_place(self);
    /* The rows are new ones whether or not the column moved: always redrawn and announced. */
    (void)note_outline_release(self->outline, &paint, note_pane_heading_count(self->pane));
    InvalidateRect(self->handle, &paint.bounds, FALSE);
    accessibility_host_schedule(self->accessibility);
    track_outline(self); /* the body's place decides again if it is not where it was */
}

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
    note_outline_hold(self->outline);
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
    restore_outline(self);
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
    note_outline_hold(self->outline);
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
    {
        outcome = restream_pane(self);
    }
    if (outcome == FOLIO_STATE_READY && line > 0)
    {
        note_pane_scroll_to_line(self->pane, line);
    }
    restore_outline(self);
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
    discovery_motion_reset(self);
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
    refresh_help_fonts(self, GetDpiForWindow(self->handle));
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
    discovery_motion_reset(self);
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
    refresh_word_font(self, GetDpiForWindow(self->handle));
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

/* 破棄の前に、名前のあるノートをファイルから読み直す（ADR0119 の決定 4: 保存が外の変更で
 * 止まったときの出口）。*told は後で出す 1 行: 読んでいた本文と違うファイルを読み込んだとき
 * と、ファイルが無い・読めないので、これまでどおり最後に読んだか保存した本文へ戻すとき。
 * 無題は読むファイルが無く、何も言わない。 */
static enum folio_state_outcome reread_for_discard(struct folio_window *_Nonnull self,
                                                   enum ui_text *_Nonnull told)
{
    bool stale = false;
    enum folio_state_outcome outcome = folio_state_document_stale(self->state, &stale);
    if (outcome != FOLIO_STATE_READY)
        return outcome;
    outcome = folio_state_reload_document(self->state);
    if (outcome == FOLIO_STATE_READY && stale)
        *told = UI_TEXT_OUTSIDE_RELOADED;
    if (outcome == FOLIO_STATE_NOTE_UNREADABLE)
        *told = UI_TEXT_DISCARD_KEPT;
    bool kept = outcome == FOLIO_STATE_NOTE_UNREADABLE || outcome == FOLIO_STATE_NOTHING_SELECTED;
    return kept ? FOLIO_STATE_READY : outcome;
}

/* `:e!`（ADR 0016 の補正 4、ADR0119 の決定 4）。前提は application が判定し、戻す本文はノート切替と
 * 同じ show_note の経路で流し込む。保存は試みない。戻す本文は、ファイルから読み直したもの。 */
static void execute_discard_command(struct folio_window *_Nonnull self,
                                    const char *_Nonnull argument)
{
    (void)argument;
    enum ui_text told = UI_TEXT_EMPTY;
    enum folio_state_outcome outcome = folio_state_discard_edits(self->state);
    /* 本文の欄が無ければ、状態を入れ替える前に止まる（入れ替えた後で流し込めないと、編集中の
     * 本文が残ったまま、次の保存が外の版と照らされなくなる）。 */
    if (outcome == FOLIO_STATE_READY && self->pane == nullptr)
        outcome = FOLIO_STATE_PANE_UNAVAILABLE;
    if (outcome == FOLIO_STATE_READY)
        outcome = reread_for_discard(self, &told);
    if (outcome == FOLIO_STATE_READY)
        outcome = show_note(self);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    if (!hide_command_surface(self))
        return;
    focus_pane(self);
    /* show_note も、面を隠すことも印を下げるので、その後に立てる。編集から始めた破棄が閲覧に
     * なっているのは、読み直した本文が編集で保てないとき。 */
    if (folio_state_pane_mode(self->state) == PANE_MODE_VIEW)
        set_pane_notice(self, PANE_NOTICE_NOTE_UNEDITABLE);
    else if (told != UI_TEXT_EMPTY)
        open_link_notice(self, told);
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
        /* a refused intent may have left a list read again (#1069, stage 5): draw it */
        redraw_drawer(self);
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
    (void)bounded_text_copy(removed_name, sizeof removed_name,
                            folio_state_note_name(self->state, target));
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
        /* a refused intent may have left a list read again (#1069, stage 5): draw it */
        redraw_drawer(self);
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

static void show_transfer_notice(struct folio_window *_Nonnull self,
                                 const struct ui_text_request *_Nonnull request)
{
    HWND focus = GetFocus();
    (void)begin_native_modal(self);
    failure_box_notice(modal_owner_current(self->modal_owner), self->state, request);
    if (!end_native_modal(self))
        native_modal_resource_failure(self);
    restore_valid_focus(self, focus);
}

static void import_at(struct folio_window *_Nonnull self, size_t category)
{
    if (category >= folio_state_category_count(self->state))
    {
        report_outcome(self, FOLIO_STATE_NO_SUCH_CATEGORY);
        return;
    }
    HWND focus = GetFocus();
    struct file_picker_request request = {.language = folio_state_language(self->state),
                                          .title = UI_TEXT_IMPORT_TITLE,
                                          .name = folio_state_category_name(self->state, category),
                                          .extension = "md"};
    struct file_picker *_Nullable picker = nullptr;
    enum file_picker_outcome picked = FILE_PICKER_UNAVAILABLE;
    if (begin_native_modal(self))
        picked = file_picker_open(modal_owner_current(self->modal_owner), &request, &picker);
    if (!end_native_modal(self))
        native_modal_resource_failure(self);
    restore_valid_focus(self, focus);
    if (picked == FILE_PICKER_UNAVAILABLE)
        report_outcome(self, FOLIO_STATE_TRANSFER_UNAVAILABLE);
    if (picked == FILE_PICKER_SELECTED && self->close_phase != CLOSE_PHASE_REQUESTED)
    {
        struct note_import_request batch = {.category = category,
                                            .paths = file_picker_paths(picker),
                                            .count = file_picker_count(picker)};
        struct note_import_result result = folio_state_import_notes(self->state, &batch);
        redraw_drawer(self);
        struct ui_text_request notice = {
            .id = UI_TEXT_IMPORT_RESULT,
            .language = folio_state_language(self->state),
            .n = result.added,
            .k = result.skipped,
            .offset = result.remaining,
            .from = folio_state_failure_line(result.outcome, folio_state_language(self->state))};
        show_transfer_notice(self, &notice);
    }
    file_picker_destroy(picker);
}

static void execute_import_command(struct folio_window *_Nonnull self,
                                   const char *_Nonnull argument)
{
    (void)argument;
    size_t category = 0;
    if (!category_command_target(self, &category))
    {
        report_outcome(self, FOLIO_STATE_CATEGORY_NOT_SELECTED);
        return;
    }
    if (hide_command_surface(self))
        import_at(self, category);
}

static const char *_Nonnull export_name(const struct folio_window *_Nonnull self,
                                        enum note_export_scope scope, size_t category)
{
    switch (scope)
    {
    case NOTE_EXPORT_CURRENT:
        return folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NAMED
                   ? folio_state_pane_title(self->state).note
                   : "Note";
    case NOTE_EXPORT_CATEGORY:
        return folio_state_category_name(self->state, category);
    case NOTE_EXPORT_ALL:
        return "NeNeFolio";
    }
    return "NeNeFolio";
}

static enum folio_state_outcome export_to(struct folio_window *_Nonnull self,
                                          struct note_export_request *_Nonnull request)
{
    if (request->scope == NOTE_EXPORT_CURRENT &&
        folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
    {
        enum folio_state_outcome taken = take_text(self, &request->units, &request->count);
        if (taken != FOLIO_STATE_READY)
            return taken;
    }
    return folio_state_export_notes(self->state, request);
}

static bool choose_export_images(struct folio_window *_Nonnull self,
                                 struct note_export_request *_Nonnull request)
{
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    const char *line =
        ui_text_line(UI_TEXT_IMAGES_EXPORT_EXTERNAL, folio_state_language(self->state));
    if (utf16_text_fill(line, units, ui_text_unit_limit, &written) != UTF16_TEXT_FILL_READY ||
        !begin_native_modal(self))
        return false;
    int choice = MessageBoxW(modal_owner_current(self->modal_owner), units, L"NeNe Folio",
                             MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2);
    if (!end_native_modal(self))
        native_modal_resource_failure(self);
    if (self->close_phase == CLOSE_PHASE_REQUESTED)
        return false;
    if (choice == IDYES)
        request->images = IMAGE_EXPORT_ALL_LOCAL;
    if (choice == IDNO)
        request->images = IMAGE_EXPORT_CONTAINED;
    return choice == IDYES || choice == IDNO;
}
static void report_export(struct folio_window *_Nonnull self,
                          struct note_export_request *_Nonnull request)
{
    enum folio_state_outcome outcome = export_to(self, request);
    if (outcome == FOLIO_STATE_EXPORT_EXTERNAL_IMAGES)
    {
        if (!choose_export_images(self, request))
            return;
        outcome = export_to(self, request);
    }
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    struct ui_text_request notice = {.id = UI_TEXT_EXPORT_DONE,
                                     .language = folio_state_language(self->state)};
    show_transfer_notice(self, &notice);
}

static void export_at(struct folio_window *_Nonnull self, enum note_export_scope scope,
                      size_t category)
{
    HWND focus = GetFocus();
    struct file_picker_request picker = {.language = folio_state_language(self->state),
                                         .title = scope == NOTE_EXPORT_CURRENT
                                                      ? UI_TEXT_EXPORT_NOTE_TITLE
                                                      : UI_TEXT_EXPORT_ZIP_TITLE,
                                         .name = export_name(self, scope, category),
                                         .extension = scope == NOTE_EXPORT_CURRENT ? "md" : "zip"};
    struct storage_path *_Nullable destination = nullptr;
    enum file_picker_outcome picked = FILE_PICKER_UNAVAILABLE;
    if (begin_native_modal(self))
        picked = file_picker_save(modal_owner_current(self->modal_owner), &picker, &destination);
    if (!end_native_modal(self))
        native_modal_resource_failure(self);
    restore_valid_focus(self, focus);
    if (picked == FILE_PICKER_UNAVAILABLE)
        report_outcome(self, FOLIO_STATE_TRANSFER_UNAVAILABLE);
    if (picked == FILE_PICKER_SELECTED && self->close_phase != CLOSE_PHASE_REQUESTED)
    {
        struct note_export_request request = {.scope = scope,
                                              .category = category,
                                              .destination = destination,
                                              .units = u"",
                                              .count = 0};
        report_export(self, &request);
    }
    storage_path_destroy(destination);
}

static void execute_export_note(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NONE)
    {
        report_outcome(self, FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    if (hide_command_surface(self))
        export_at(self, NOTE_EXPORT_CURRENT, 0);
}

static void execute_export_category(struct folio_window *_Nonnull self,
                                    const char *_Nonnull argument)
{
    (void)argument;
    size_t category = 0;
    if (!category_command_target(self, &category))
    {
        report_outcome(self, FOLIO_STATE_CATEGORY_NOT_SELECTED);
        return;
    }
    if (hide_command_surface(self))
        export_at(self, NOTE_EXPORT_CATEGORY, category);
}

static void execute_export_all(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (hide_command_surface(self))
        export_at(self, NOTE_EXPORT_ALL, 0);
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

static void settle_empty_drawer_focus(struct folio_window *_Nonnull self)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref ref = {0};
    if (GetFocus() == self->handle && self->keyboard_target == ACCESSIBLE_TREE &&
        !folio_state_cursor(self->state, &kind, &ref))
    {
        self->keyboard_target = selected_scope_kind(self);
        redraw_drawer(self);
    }
}

static void set_note_scope(struct folio_window *_Nonnull self, enum note_scope scope)
{
    folio_state_set_note_scope(self->state, scope);
    settle_empty_drawer_focus(self);
    redraw_drawer(self);
    accessibility_host_schedule(self->accessibility);
}

static void favorite_at(struct folio_window *_Nonnull self, struct note_ref target)
{
    /* #1069 / ADR0117: a list read again before the write is told in one line. */
    enum folio_state_outcome outcome =
        folio_state_ledger_outcome(self->state, folio_state_toggle_favorite(self->state, target));
    if (outcome != FOLIO_STATE_READY)
        report_outcome(self, outcome);
    settle_empty_drawer_focus(self);
    redraw_drawer(self);
    accessibility_host_schedule(self->accessibility);
}

static void execute_toggle_favorite(struct folio_window *_Nonnull self,
                                    const char *_Nonnull argument)
{
    (void)argument;
    struct note_ref target = {0};
    if (!folio_state_selection(self->state, &target))
    {
        report_outcome(self, FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    if (!hide_command_surface(self))
        return;
    favorite_at(self, target);
}

static void execute_toggle_favorites_only(struct folio_window *_Nonnull self,
                                          const char *_Nonnull argument)
{
    (void)argument;
    if (!hide_command_surface(self))
        return;
    enum note_scope scope = folio_state_note_scope(self->state);
    switch (scope)
    {
    case NOTE_SCOPE_ALL:
        scope = NOTE_SCOPE_FAVORITES;
        break;
    case NOTE_SCOPE_FAVORITES:
        scope = NOTE_SCOPE_ALL;
        break;
    }
    set_note_scope(self, scope);
}

static void execute_toggle_favorites_first(struct folio_window *_Nonnull self,
                                           const char *_Nonnull argument)
{
    (void)argument;
    enum note_order order = folio_state_note_order(self->state);
    switch (order)
    {
    case NOTE_ORDER_MANUAL:
        order = NOTE_ORDER_FAVORITES_FIRST;
        break;
    case NOTE_ORDER_FAVORITES_FIRST:
        order = NOTE_ORDER_MANUAL;
        break;
    }
    enum folio_state_outcome outcome = folio_state_set_note_order(self->state, order);
    if (outcome != FOLIO_STATE_READY)
    {
        report_outcome(self, outcome);
        return;
    }
    if (settings_surface(self))
        clear_command_status(self);
    else if (!hide_command_surface(self))
        return;
    redraw_drawer(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static void execute_toggle_minimap_command(struct folio_window *_Nonnull self,
                                           const char *_Nonnull argument)
{
    (void)argument;
    apply_minimap(self);
}

/* #975 / ADR0114: open or close the headings column. The state is the application's, for this
 * run only; the column itself appears where a note is being read and there is room. */
static void apply_outline(struct folio_window *_Nonnull self)
{
    if (!hide_command_surface(self))
        return;
    folio_state_set_outline(self->state, !folio_state_outline(self->state));
    cancel_part_press(self);
    (void)note_outline_set_hover(self->outline, note_outline_none);
    self->outline_wheel = 0;
    rearrange_keeping_line(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    accessibility_host_schedule(self->accessibility);
}

static void execute_toggle_outline_command(struct folio_window *_Nonnull self,
                                           const char *_Nonnull argument)
{
    (void)argument;
    apply_outline(self);
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

static void management_refresh(struct folio_window *_Nonnull self)
{
    if (self->tag_management_view == TAG_MANAGEMENT_PLAN)
        self->management_merging = folio_state_tag_management_summary(self->state).merging;
    invalidate_surface_items(self);
    self->command_selection = 0;
    self->command_first = 0;
    self->tag_focus = management_running(self) ? TAG_SURFACE_PRIMARY : TAG_SURFACE_ROWS;
    arrange_command_input(self);
    if (management_input(self))
        SetFocus(self->command_input);
    if (!management_input(self))
        SetFocus(self->command_layer);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
    enum tag_management_phase phase = folio_state_tag_management_phase(self->state);
    if (self->tag_management_view == TAG_MANAGEMENT_PLAN &&
        (phase == TAG_MANAGEMENT_PREVIEW || phase == TAG_MANAGEMENT_RESULT))
    {
        char text[draw_unit_limit];
        char16_t spoken[draw_unit_limit];
        management_status(self, text, sizeof text);
        (void)wide_units(text, spoken);
        accessibility_host_notify(self->accessibility,
                                  (struct accessible_item){.kind = ACCESSIBLE_WINDOW}, spoken);
    }
}

static enum folio_state_outcome management_input_tag(const struct folio_window *_Nonnull self,
                                                     struct note_tag *_Nullable *_Nonnull out)
{
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
        return FOLIO_STATE_OUT_OF_MEMORY;
    enum note_tag_outcome result =
        note_tag_create(utf8_text_bytes(query), utf8_text_length(query), out);
    utf8_text_destroy(query);
    switch (result)
    {
    case NOTE_TAG_READY:
        return FOLIO_STATE_READY;
    case NOTE_TAG_EMPTY:
    case NOTE_TAG_INVALID:
        return FOLIO_STATE_TAG_INVALID;
    case NOTE_TAG_TOO_LONG:
        return FOLIO_STATE_TAG_LIMIT;
    case NOTE_TAG_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_TAG_INVALID;
}

static void management_name_changed(struct folio_window *_Nonnull self)
{
    struct note_tag *_Nullable target = nullptr;
    enum folio_state_outcome result = management_input_tag(self, &target);
    self->management_merging =
        result == FOLIO_STATE_READY && !note_tag_equal(self->management_source, target) &&
        folio_state_tag_catalog_status(self->state) == TAG_CATALOG_AVAILABLE &&
        tag_catalog_contains(folio_state_tag_catalog(self->state), target);
    note_tag_destroy(target);
    clear_command_status(self);
    invalidate_surface_items(self);
    arrange_command_input(self);
    redraw_command_layer(self);
}

static void management_prepare(struct folio_window *_Nonnull self,
                               const struct note_tag *_Nonnull from,
                               const struct note_tag *_Nullable to)
{
    if (to != nullptr && folio_state_tag_catalog_status(self->state) == TAG_CATALOG_AVAILABLE)
        self->management_merging = !note_tag_equal(from, to) &&
                                   tag_catalog_contains(folio_state_tag_catalog(self->state), to);
    if (to == nullptr)
        self->management_merging = false;
    enum folio_state_outcome result = folio_state_prepare_tag_management(self->state, from, to);
    self->command_failure = result;
    if (result == FOLIO_STATE_READY)
    {
        self->tag_management_view = TAG_MANAGEMENT_PLAN;
        note_tag_destroy(self->management_source);
        self->management_source = nullptr;
    }
    management_refresh(self);
}

static void management_rename(struct folio_window *_Nonnull self)
{
    if (self->command_selection >= management_row_count(self))
        return;
    const struct note_tag *selected =
        tag_candidates_tag(self->tag_candidates, self->command_selection);
    struct note_tag *_Nullable source = nullptr;
    if (note_tag_create(note_tag_bytes(selected), note_tag_length(selected), &source) !=
        NOTE_TAG_READY)
    {
        self->command_failure = FOLIO_STATE_OUT_OF_MEMORY;
        management_refresh(self);
        return;
    }
    note_tag_destroy(self->management_source);
    self->management_source = source;
    self->tag_management_view = TAG_MANAGEMENT_NAME;
    self->management_merging = false;
    char16_t input[draw_unit_limit];
    (void)wide_units(note_tag_bytes(source), input);
    initialize_command_input(self, (const wchar_t *)input);
    management_refresh(self);
    SendMessageW(self->command_input, EM_SETSEL, 0, -1);
}

static void management_choose(struct folio_window *_Nonnull self)
{
    folio_state_close_tag_management(self->state);
    note_tag_destroy(self->management_source);
    self->management_source = nullptr;
    self->tag_management_view = TAG_MANAGEMENT_CHOOSE;
    self->management_merging = false;
    folio_state_request_tag_catalog(self->state);
    initialize_command_input(self, L"");
    tag_query_changed(self);
    management_refresh(self);
}

static void management_apply_action(struct folio_window *_Nonnull self)
{
    switch (folio_state_tag_management_phase(self->state))
    {
    case TAG_MANAGEMENT_CLOSED:
        close_command_surface(self);
        return;
    case TAG_MANAGEMENT_PREPARING:
    case TAG_MANAGEMENT_APPLYING:
        folio_state_stop_tag_management(self->state);
        break;
    case TAG_MANAGEMENT_PREVIEW:
        self->command_failure = folio_state_apply_tag_management(self->state);
        break;
    case TAG_MANAGEMENT_RESULT:
        if (management_retryable(self))
        {
            management_prepare(self, folio_state_tag_management_source(self->state),
                               folio_state_tag_management_target(self->state));
            return;
        }
        close_command_surface(self);
        return;
    }
    management_refresh(self);
}

static void management_named_prepare(struct folio_window *_Nonnull self)
{
    struct note_tag *_Nullable target = nullptr;
    enum folio_state_outcome result = management_input_tag(self, &target);
    if (result == FOLIO_STATE_READY)
        management_prepare(self, self->management_source, target);
    if (result != FOLIO_STATE_READY)
    {
        self->command_failure = result;
        management_refresh(self);
    }
    note_tag_destroy(target);
}

static void management_primary(struct folio_window *_Nonnull self)
{
    if (!management_button_enabled(self, ACCESSIBLE_SURFACE_TAG_PRIMARY))
        return;
    switch (self->tag_management_view)
    {
    case TAG_MANAGEMENT_CHOOSE:
        management_rename(self);
        return;
    case TAG_MANAGEMENT_NAME:
        management_named_prepare(self);
        return;
    case TAG_MANAGEMENT_PLAN:
        management_apply_action(self);
        return;
    }
}

static void management_secondary(struct folio_window *_Nonnull self)
{
    if (!management_button_enabled(self, ACCESSIBLE_SURFACE_TAG_SECONDARY))
        return;
    switch (self->tag_management_view)
    {
    case TAG_MANAGEMENT_CHOOSE:
        if (self->command_selection < management_row_count(self))
            management_prepare(
                self, tag_candidates_tag(self->tag_candidates, self->command_selection), nullptr);
        return;
    case TAG_MANAGEMENT_NAME:
        management_choose(self);
        return;
    case TAG_MANAGEMENT_PLAN:
        break;
    }
    enum tag_management_phase phase = folio_state_tag_management_phase(self->state);
    if (phase == TAG_MANAGEMENT_PREVIEW)
        management_choose(self);
    if (phase == TAG_MANAGEMENT_RESULT || phase == TAG_MANAGEMENT_CLOSED)
        close_command_surface(self);
}

static void tag_query_changed(struct folio_window *_Nonnull self)
{
    if (self->command_composing)
        return;
    if (management_surface(self) && self->tag_management_view == TAG_MANAGEMENT_NAME)
    {
        management_name_changed(self);
        return;
    }
    if (management_surface(self) && self->tag_management_view == TAG_MANAGEMENT_PLAN)
        return;
    invalidate_surface_items(self);
    self->command_selection = 0;
    self->command_first = 0;
    clear_command_status(self);
    if (prepare_tag_candidates(self) != S_OK)
        self->command_failure = FOLIO_STATE_OUT_OF_MEMORY;
    arrange_command_input(self);
    redraw_command_layer(self);
}

static enum folio_state_outcome tag_editor_source(const struct folio_window *_Nonnull self,
                                                  const char16_t *_Nonnull *_Nonnull units,
                                                  size_t *_Nonnull count)
{
    *units = u"";
    *count = 0;
    return folio_state_pane_mode(self->state) == PANE_MODE_EDIT ? take_text(self, units, count)
                                                                : FOLIO_STATE_READY;
}

static enum folio_state_outcome prepare_tag_surface(struct folio_window *_Nonnull self,
                                                    enum command_surface_mode mode)
{
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    if (mode == COMMAND_SURFACE_TAG_FILTER)
        outcome = save_before_filter(self);
    if (mode == COMMAND_SURFACE_TAG_MANAGE)
        outcome = save_before_catalog(self, NOTE_SAVE_CHECK_CURRENT);
    if (mode == COMMAND_SURFACE_TAG_EDIT)
    {
        const char16_t *_Nonnull units = u"";
        size_t count = 0;
        outcome = tag_editor_source(self, &units, &count);
        if (outcome == FOLIO_STATE_READY)
            outcome = folio_state_begin_tag_edit(self->state, units, count);
    }
    return outcome;
}

static void open_tag_surface(struct folio_window *_Nonnull self, enum command_surface_mode mode)
{
    if (self->pane != nullptr && note_pane_composing(self->pane))
        return;
    if (!hide_command_surface(self))
        return;
    enum folio_state_outcome outcome = prepare_tag_surface(self, mode);
    if (outcome != FOLIO_STATE_READY)
    {
        if (outcome != FOLIO_STATE_CANCELLED)
            report_outcome(self, outcome);
        return;
    }
    folio_state_request_tag_catalog(self->state);
    self->tag_origin_mode = folio_state_pane_mode(self->state);
    self->tag_focus = TAG_SURFACE_ROWS;
    open_command_surface(self, mode);
    tag_query_changed(self);
}

static void execute_edit_tags(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    open_tag_surface(self, COMMAND_SURFACE_TAG_EDIT);
}

static void execute_filter_tags(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    open_tag_surface(self, COMMAND_SURFACE_TAG_FILTER);
}

static void execute_manage_tags(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    self->tag_management_view = TAG_MANAGEMENT_CHOOSE;
    self->management_merging = false;
    open_tag_surface(self, COMMAND_SURFACE_TAG_MANAGE);
}

static void tag_action_finished(struct folio_window *_Nonnull self,
                                enum folio_state_outcome outcome)
{
    if (outcome != FOLIO_STATE_READY)
    {
        self->command_failure = outcome;
        arrange_command_input(self);
        redraw_command_layer(self);
        accessibility_host_schedule(self->accessibility);
        return;
    }
    size_t selected = self->command_selection;
    tag_query_changed(self);
    size_t count = tag_matches_count(self);
    self->command_selection = selected < count ? selected : (count == 0 ? 0 : count - 1);
    reveal_command_selection(self);
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
}

static enum folio_state_outcome tag_set_outcome(enum note_tags_outcome outcome)
{
    switch (outcome)
    {
    case NOTE_TAGS_READY:
        return FOLIO_STATE_READY;
    case NOTE_TAGS_TOO_MANY:
        return FOLIO_STATE_TAG_LIMIT;
    case NOTE_TAGS_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    return FOLIO_STATE_OUT_OF_MEMORY;
}

static enum folio_state_outcome toggle_filter_tag(struct folio_window *_Nonnull self,
                                                  const struct note_tag *_Nonnull tag)
{
    const struct note_tags *_Nullable current = folio_state_tag_filter(self->state);
    struct note_tags *_Nullable empty = nullptr;
    struct note_tags *_Nullable next = nullptr;
    if (current == nullptr)
    {
        if (note_tags_empty(&empty) != NOTE_TAGS_READY)
            return FOLIO_STATE_OUT_OF_MEMORY;
        current = empty;
    }
    enum note_tag_presence presence =
        note_tags_contains(current, tag) ? NOTE_TAG_ABSENT : NOTE_TAG_PRESENT;
    enum folio_state_outcome outcome =
        tag_set_outcome(note_tags_with(current, tag, presence, &next));
    if (outcome == FOLIO_STATE_READY)
        outcome =
            folio_state_set_tag_filter(self->state, next, folio_state_tag_match_mode(self->state));
    note_tags_destroy(next);
    note_tags_destroy(empty);
    return outcome;
}

static void tag_toggle_row(struct folio_window *_Nonnull self)
{
    if (management_surface(self))
    {
        redraw_command_layer(self);
        accessibility_host_schedule(self->accessibility);
        return;
    }
    if (self->command_composing || self->command_selection >= tag_matches_count(self))
        return;
    const struct note_tag *_Nonnull tag =
        tag_candidates_tag(self->tag_candidates, self->command_selection);
    enum folio_state_outcome outcome = self->command_surface == COMMAND_SURFACE_TAG_EDIT
                                           ? folio_state_toggle_edit_tag(self->state, tag)
                                           : toggle_filter_tag(self, tag);
    tag_action_finished(self, outcome);
}

static void tag_add_input(struct folio_window *_Nonnull self)
{
    if (self->command_composing || self->command_surface != COMMAND_SURFACE_TAG_EDIT)
        return;
    wchar_t units[command_input_capacity];
    int count = GetWindowTextW(self->command_input, units, (int)command_input_capacity);
    enum folio_state_outcome outcome = folio_state_enter_edit_tag(
        self->state, (const char16_t *)units, count > 0 ? (size_t)count : 0);
    if (outcome == FOLIO_STATE_READY)
        initialize_command_input(self, L"");
    tag_action_finished(self, outcome);
}

static enum folio_state_outcome prepare_tag_apply(struct folio_window *_Nonnull self,
                                                  struct replace_edit *_Nullable *_Nonnull edit)
{
    if (self->tag_origin_mode == PANE_MODE_VIEW && pane_unavailable(self))
    {
        enum folio_state_outcome outcome = enter_edit(self);
        if (outcome != FOLIO_STATE_READY)
            return outcome;
    }
    const char16_t *_Nonnull units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = tag_editor_source(self, &units, &count);
    if (outcome == FOLIO_STATE_READY)
        outcome = folio_state_apply_tag_edit(self->state, units, count, edit);
    return outcome;
}

static void tag_apply(struct folio_window *_Nonnull self)
{
    struct replace_edit *_Nullable edit = nullptr;
    enum folio_state_outcome outcome = prepare_tag_apply(self, &edit);
    if (outcome == FOLIO_STATE_READY && edit != nullptr)
        outcome = enter_edit(self);
    if (outcome != FOLIO_STATE_READY)
    {
        replace_edit_destroy(edit);
        tag_action_finished(self, outcome);
        return;
    }
    bool viewing = self->tag_origin_mode == PANE_MODE_VIEW;
    if (hide_command_surface(self) && edit != nullptr)
    {
        note_pane_replace(self->pane, replace_edit_span(edit), replace_edit_units(edit));
        if (viewing)
            outcome = flush_edit(self);
    }
    replace_edit_destroy(edit);
    if (outcome != FOLIO_STATE_READY)
        report_outcome(self, outcome);
    InvalidateRect(self->handle, nullptr, FALSE);
}

static void tag_filter_mode(struct folio_window *_Nonnull self, enum tag_match_mode mode)
{
    const struct note_tags *_Nullable tags = folio_state_tag_filter(self->state);
    struct note_tags *_Nullable empty = nullptr;
    if (tags == nullptr)
    {
        if (note_tags_empty(&empty) != NOTE_TAGS_READY)
        {
            tag_action_finished(self, FOLIO_STATE_OUT_OF_MEMORY);
            return;
        }
        tags = empty;
    }
    enum folio_state_outcome outcome = folio_state_set_tag_filter(self->state, tags, mode);
    note_tags_destroy(empty);
    tag_action_finished(self, outcome);
}

static void tag_clear_filters(struct folio_window *_Nonnull self)
{
    struct note_tags *_Nullable empty = nullptr;
    if (note_tags_empty(&empty) != NOTE_TAGS_READY)
    {
        tag_action_finished(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    enum folio_state_outcome outcome =
        folio_state_set_tag_filter(self->state, empty, TAG_MATCH_ALL);
    note_tags_destroy(empty);
    if (outcome == FOLIO_STATE_READY)
    {
        outcome = folio_state_set_index_filter(self->state, u"", 0);
        folio_state_set_note_scope(self->state, NOTE_SCOPE_ALL);
        SetWindowTextW(self->filter_input, L"");
    }
    tag_action_finished(self, outcome);
}

/* コマンドごとの行き先を 1 か所に置く（ADR 0027 の 2026-10-06 の補正）。全値を明示し、
 * 値を足して行を書き忘れると CNF-009 が落ちる（switch を離れて失う -Wswitch-enum の代わり）。
 * enum の外の値は作られないので添字の範囲検査は書かない。 */
static void (*_Nonnull const command_runs[])(struct folio_window *_Nonnull self,
                                             const char *_Nonnull argument) = {
    [FOLIO_COMMAND_IMPORT] = execute_import_command,
    [FOLIO_COMMAND_EXPORT_NOTE] = execute_export_note,
    [FOLIO_COMMAND_EXPORT_CATEGORY] = execute_export_category,
    [FOLIO_COMMAND_EXPORT_ALL] = execute_export_all,
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
    [FOLIO_COMMAND_TOGGLE_MINIMAP] = execute_toggle_minimap_command,
    [FOLIO_COMMAND_TOGGLE_OUTLINE] = execute_toggle_outline_command,
    [FOLIO_COMMAND_TOGGLE_FAVORITE] = execute_toggle_favorite,
    [FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY] = execute_toggle_favorites_only,
    [FOLIO_COMMAND_TOGGLE_FAVORITES_FIRST] = execute_toggle_favorites_first,
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
    [FOLIO_COMMAND_EDIT_TAGS] = execute_edit_tags,
    [FOLIO_COMMAND_FILTER_TAGS] = execute_filter_tags,
    [FOLIO_COMMAND_MANAGE_TAGS] = execute_manage_tags,
    [FOLIO_COMMAND_DISCOVER_NOTES] = execute_discovery,
    [FOLIO_COMMAND_MOVE_NOTE] = execute_move_note,
};

/* GUI・キー・Exで同じ操作と引数を実行する（ADR0020）。 */
static void execute_command(struct folio_window *_Nonnull self, enum folio_command command,
                            const char *_Nonnull argument)
{
    cancel_part_press(self);
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
    struct note_identity *_Nullable identity = nullptr;
    enum folio_state_outcome outcome = folio_state_capture_note(
        self->state, (struct note_ref){.category = category, .note = note}, &identity);
    if (outcome != FOLIO_STATE_READY)
        return outcome;
    set_pane_notice(self, PANE_NOTICE_NONE);
    struct name_prompt_result departure = save_edit(self);
    outcome = departure.outcome;
    struct note_ref target = {0};
    if (outcome == FOLIO_STATE_READY)
        outcome = folio_state_resolve_note(self->state, identity, &target);
    note_identity_destroy(identity);
    if (outcome == FOLIO_STATE_READY)
        outcome = opened(self, folio_state_select_note(self->state, target.category, target.note,
                                                       departure.untitled));
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
    case DRAWER_MENU_TOGGLE_FAVORITE:
        favorite_at(self, *target);
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
    case SETTINGS_ROW_STORAGE:
        show_storage_prompt(self);
        return;
    case SETTINGS_ROW_ABOUT:
        show_about_surface(self);
        return;
    case SETTINGS_ROW_THEME:
        show_theme_surface(self);
        return;
    case SETTINGS_ROW_LANGUAGE:
        apply_language(self, (enum folio_language)settings_rows[index].value);
        return;
    case SETTINGS_ROW_FAVORITES_FIRST:
        execute_toggle_favorites_first(self, "");
        return;
    case SETTINGS_ROW_MINIMAP:
        apply_minimap(self);
        return;
    case SETTINGS_ROW_MOTION:
        apply_motion(self);
        return;
    case SETTINGS_ROW_WINDOW:
        apply_window_restore(self);
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

static void click_tag_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT rows = tag_rows_rect(self);
    if (!PtInRect(&rows, point) || self->command_composing)
        return;
    size_t at =
        self->command_first + (size_t)((point.y - rows.top) / scale(base_command_row_height,
                                                                    GetDpiForWindow(self->handle)));
    if (at >= tag_matches_count(self))
        return;
    self->command_selection = at;
    self->tag_focus = TAG_SURFACE_ROWS;
    SetFocus(self->command_layer);
    tag_toggle_row(self);
}

static size_t tag_next_row(const struct folio_window *_Nonnull self, WPARAM key, size_t count)
{
    size_t step = key == VK_PRIOR || key == VK_NEXT ? command_visible_rows(self) : 1;
    size_t at = self->command_selection;
    if (key == VK_UP || key == VK_PRIOR)
        at = at < step ? 0 : at - step;
    else
        at = step >= count - at ? count - 1 : at + step;
    return at;
}

static bool tag_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_UP && key != VK_DOWN && key != VK_PRIOR && key != VK_NEXT)
        return false;
    size_t count = tag_matches_count(self);
    if (count == 0)
        return true;
    size_t at = tag_next_row(self, key, count);
    self->command_selection = at;
    self->tag_focus = TAG_SURFACE_ROWS;
    SetFocus(self->command_layer);
    reveal_command_selection(self);
    redraw_command_layer(self);
    return true;
}

static size_t tag_focus_slot(enum tag_surface_focus focus)
{
    switch (focus)
    {
    case TAG_SURFACE_ROWS:
        return SIZE_MAX;
    case TAG_SURFACE_ADD:
        return ACCESSIBLE_SURFACE_TAG_ADD;
    case TAG_SURFACE_APPLY:
        return ACCESSIBLE_SURFACE_TAG_APPLY;
    case TAG_SURFACE_CANCEL:
        return ACCESSIBLE_SURFACE_TAG_CANCEL;
    case TAG_SURFACE_ALL:
        return ACCESSIBLE_SURFACE_TAG_ALL;
    case TAG_SURFACE_ANY:
        return ACCESSIBLE_SURFACE_TAG_ANY;
    case TAG_SURFACE_CLEAR:
        return ACCESSIBLE_SURFACE_TAG_CLEAR;
    case TAG_SURFACE_CLOSE:
        return ACCESSIBLE_SURFACE_CLOSE;
    case TAG_SURFACE_PRIMARY:
        return ACCESSIBLE_SURFACE_TAG_PRIMARY;
    case TAG_SURFACE_SECONDARY:
        return ACCESSIBLE_SURFACE_TAG_SECONDARY;
    }
    return SIZE_MAX;
}

static size_t tag_focus_ordinal(const struct folio_window *_Nonnull self,
                                const enum tag_surface_focus *_Nonnull path)
{
    if (GetFocus() != self->command_layer)
        return 0;
    for (size_t at = 0; at < 5; ++at)
        if (self->tag_focus == path[at])
            return at + 1;
    return 0;
}

static bool management_focus_visible(const struct folio_window *_Nonnull self,
                                     enum tag_surface_focus focus)
{
    if (focus == TAG_SURFACE_ROWS)
        return management_row_count(self) != 0;
    size_t slot = tag_focus_slot(focus);
    return surface_button_visible(self, slot) && management_button_enabled(self, slot);
}

static size_t management_focus_ordinal(const struct folio_window *_Nonnull self,
                                       const enum tag_surface_focus *_Nonnull path)
{
    if (GetFocus() != self->command_layer)
        return 0;
    for (size_t at = 0; at < 4; ++at)
        if (self->tag_focus == path[at])
            return at + 1;
    return 0;
}

static bool management_tab(struct folio_window *_Nonnull self)
{
    const enum tag_surface_focus path[] = {TAG_SURFACE_ROWS, TAG_SURFACE_PRIMARY,
                                           TAG_SURFACE_SECONDARY, TAG_SURFACE_CLOSE};
    size_t current = management_focus_ordinal(self, path);
    size_t direction = (pressed_shortcut_modifiers() & FSHIFT) != 0 ? 4 : 1;
    for (size_t step = 0; step < 5; ++step)
    {
        current = (current + direction) % 5;
        if (current == 0 && management_input(self))
        {
            SetFocus(self->command_input);
            break;
        }
        if (current != 0 && management_focus_visible(self, path[current - 1]))
        {
            self->tag_focus = path[current - 1];
            SetFocus(self->command_layer);
            break;
        }
    }
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
    return true;
}

static bool tag_tab(struct folio_window *_Nonnull self)
{
    if (management_surface(self))
        return management_tab(self);
    const enum tag_surface_focus edit[] = {TAG_SURFACE_ROWS, TAG_SURFACE_ADD, TAG_SURFACE_APPLY,
                                           TAG_SURFACE_CANCEL, TAG_SURFACE_CLOSE};
    const enum tag_surface_focus filter[] = {TAG_SURFACE_ROWS, TAG_SURFACE_ALL, TAG_SURFACE_ANY,
                                             TAG_SURFACE_CLEAR, TAG_SURFACE_CLOSE};
    const enum tag_surface_focus *_Nonnull path =
        self->command_surface == COMMAND_SURFACE_TAG_EDIT ? edit : filter;
    size_t current = tag_focus_ordinal(self, path);
    bool backwards = (pressed_shortcut_modifiers() & FSHIFT) != 0;
    size_t next = (current + (backwards ? 5 : 1)) % 6;
    if (next == 1 && tag_matches_count(self) == 0)
        next = backwards ? 0 : 2;
    if (next == 0)
        SetFocus(self->command_input);
    else
    {
        self->tag_focus = path[next - 1];
        SetFocus(self->command_layer);
    }
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
    return true;
}

static bool management_return(struct folio_window *_Nonnull self)
{
    if (GetFocus() == self->command_input)
    {
        if (self->tag_management_view == TAG_MANAGEMENT_NAME)
            management_primary(self);
        if (self->tag_management_view == TAG_MANAGEMENT_CHOOSE && management_row_count(self) != 0)
        {
            self->tag_focus = TAG_SURFACE_ROWS;
            SetFocus(self->command_layer);
            redraw_command_layer(self);
        }
        return true;
    }
    if (self->tag_focus == TAG_SURFACE_ROWS && self->tag_management_view == TAG_MANAGEMENT_CHOOSE)
        management_rename(self);
    if (self->tag_focus != TAG_SURFACE_ROWS)
        (void)surface_button_action(self, tag_focus_slot(self->tag_focus));
    return true;
}

static bool tag_return(struct folio_window *_Nonnull self)
{
    if (management_surface(self))
        return management_return(self);
    if (GetFocus() == self->command_input)
    {
        if (self->command_surface == COMMAND_SURFACE_TAG_EDIT)
            tag_add_input(self);
        else
            (void)tag_navigate(self, VK_DOWN);
        return true;
    }
    if (self->tag_focus == TAG_SURFACE_ROWS)
        tag_toggle_row(self);
    else
        (void)surface_button_action(self, tag_focus_slot(self->tag_focus));
    return true;
}

static void click_command_surface(struct folio_window *_Nonnull self, POINT point)
{
    if (discovery_surface(self))
    {
        click_discovery(self, point);
        return;
    }
    if (tag_surface(self))
    {
        click_tag_surface(self, point);
        return;
    }
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        click_theme_surface(self, point);
        return;
    }
    if (move_surface(self))
    {
        click_move_surface(self, point);
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
        enum accessible_kind kind = main_parts[index];
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
    if (accessible_item_host((struct accessible_item){.kind = kind}) == ACCESSIBLE_DRAWER)
    {
        redraw_drawer(self);
        return;
    }
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
    bool choice = chip || accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP ||
                  accessible_controls[kind].parent == ACCESSIBLE_SCOPE_GROUP;
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
static void clear_part_press(struct folio_window *_Nonnull self)
{
    enum accessible_kind kind = self->pressed_part;
    if (kind == ACCESSIBLE_WINDOW)
    {
        return;
    }
    bool mouse = !self->pressed_by_key;
    if (kind == ACCESSIBLE_OUTLINE)
        (void)note_outline_set_pressed(self->outline, note_outline_none);
    self->pressed_part = ACCESSIBLE_WINDOW;
    self->pressed_inside = false;
    self->pressed_by_key = false;
    invalidate_part(self, kind);
    if (mouse && GetCapture() == self->handle)
    {
        ReleaseCapture();
    }
}

static void cancel_part_press(struct folio_window *_Nonnull self)
{
    bool resizing = self->pressed_part == ACCESSIBLE_SPLITTER;
    clear_part_press(self);
    if (resizing)
        apply_drawer_width(self, self->split_origin_width);
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

static void begin_drawer_resize(struct folio_window *_Nonnull self, POINT point)
{
    begin_part_press(self, ACCESSIBLE_SPLITTER, false);
    self->split_origin_width = self->drawer_width;
    self->split_pointer_offset = point.x - regions_for(self).drawer.right;
    SetCapture(self->handle);
    if (GetCapture() != self->handle)
        cancel_part_press(self);
}

static void drag_drawer_width(struct folio_window *_Nonnull self, int x)
{
    if (!part_enabled(self, ACCESSIBLE_SPLITTER) || !accessible_visible(self, ACCESSIBLE_SPLITTER))
    {
        cancel_part_press(self);
        return;
    }
    struct drawer_width_range range = drawer_width_for(self);
    int width =
        MulDiv(x - self->split_pointer_offset, base_dpi, (int)GetDpiForWindow(self->handle));
    width = width < range.minimum ? range.minimum : width;
    width = width > range.maximum ? range.maximum : width;
    apply_drawer_width(self, width);
}

static void scroll_minimap(struct folio_window *_Nonnull self, int first)
{
    if (!minimap_available(self))
        return;
    note_pane_scroll_pixels(self->pane, first);
    refresh_gutter(self, true);
}

static void begin_minimap_drag(struct folio_window *_Nonnull self, POINT point)
{
    struct note_minimap_paint paint;
    if (!minimap_available(self) || !minimap_paint_for(self, &paint))
        return;
    RECT track = note_minimap_track(&paint);
    if (!PtInRect(&track, point))
        return;
    RECT thumb = note_minimap_thumb(&paint);
    if (!PtInRect(&thumb, point))
    {
        scroll_minimap(self, note_minimap_jump(&paint, point.y));
        if (!minimap_paint_for(self, &paint))
            return;
        thumb = note_minimap_thumb(&paint);
    }
    begin_part_press(self, ACCESSIBLE_MINIMAP, false);
    self->minimap_pointer_offset = point.y - thumb.top;
    SetCapture(self->handle);
    if (GetCapture() != self->handle)
        clear_part_press(self);
}

static void drag_minimap(struct folio_window *_Nonnull self, int y)
{
    struct note_minimap_paint paint;
    if (!minimap_available(self) || !minimap_paint_for(self, &paint))
    {
        clear_part_press(self);
        return;
    }
    scroll_minimap(self, note_minimap_drag(&paint, y, self->minimap_pointer_offset));
}

/* #975: the column's row under a point in client coordinates, or none. */
static size_t outline_row_under(const struct folio_window *_Nonnull self, POINT point)
{
    if (!outline_available(self))
        return note_outline_none;
    struct note_outline_paint paint = outline_place(self);
    return note_outline_row_at(self->outline, &paint, point, note_pane_heading_count(self->pane));
}

/* Pressing a row of the headings column. The jump happens on release over the same row, like
 * a button (ADR0060 decision 4). */
static void press_outline(struct folio_window *_Nonnull self, POINT point)
{
    size_t row = outline_row_under(self, point);
    if (row == note_outline_none)
        return;
    begin_part_press(self, ACCESSIBLE_OUTLINE, false);
    self->outline_press = row;
    self->outline_press_revision = note_outline_revision(self->outline);
    (void)note_outline_set_pressed(self->outline, row);
    SetCapture(self->handle);
    if (GetCapture() != self->handle)
        clear_part_press(self);
}

/* While pressed, the row looks pressed only with the pointer over it. */
static void track_outline_press(struct folio_window *_Nonnull self, POINT point)
{
    bool inside = outline_row_under(self, point) == self->outline_press;
    self->pressed_inside = inside;
    if (note_outline_set_pressed(self->outline, inside ? self->outline_press : note_outline_none))
        invalidate_part(self, ACCESSIBLE_OUTLINE);
}

/* Released over the row it went down on: the same request UIA's Invoke sends. */
static void release_outline(struct folio_window *_Nonnull self, POINT point)
{
    size_t row = self->outline_press;
    bool same = outline_row_under(self, point) == row;
    cancel_part_press(self);
    if (!same)
        return;
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_OUTLINE_ITEM,
                                                  .slot = row,
                                                  .revision = self->outline_press_revision},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    (void)folio_window_accessible_action(self, &request);
    accessibility_host_schedule(self->accessibility);
}

static void hover_outline(struct folio_window *_Nonnull self, size_t row)
{
    if (note_outline_set_hover(self->outline, row))
        invalidate_part(self, ACCESSIBLE_OUTLINE);
}

static bool press_range(struct folio_window *_Nonnull self, enum accessible_kind kind, POINT point)
{
    if (kind == ACCESSIBLE_MINIMAP)
    {
        begin_minimap_drag(self, point);
        return true;
    }
    if (kind == ACCESSIBLE_OUTLINE)
    {
        press_outline(self, point);
        return true;
    }
    if (kind == ACCESSIBLE_SPLITTER && part_enabled(self, kind))
    {
        begin_drawer_resize(self, point);
        return true;
    }
    return false;
}

static void press_part(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (dismiss_menu_press(self, point))
        return;
    show_focus_cues(self, false);
    enum accessible_kind kind = part_at(self, point);
    if (press_range(self, kind, point))
        return;
    if (self->native_modal_depth == 0 && self->command_surface == COMMAND_SURFACE_CLOSED)
        pointer_focus(self, point);
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
    if (kind == ACCESSIBLE_MINIMAP)
    {
        drag_minimap(self, point.y);
        clear_part_press(self);
        return;
    }
    if (kind == ACCESSIBLE_SPLITTER)
    {
        drag_drawer_width(self, point.x);
        clear_part_press(self);
        return;
    }
    if (kind == ACCESSIBLE_OUTLINE)
    {
        release_outline(self, point);
        return;
    }
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

/* 共有pointerの現在位置を読まず、届いた通知から決めた押下/hoverを表示する。 */
static void refresh_pointer_cursor(const struct folio_window *_Nonnull self, POINT point)
{
    enum accessible_kind kind = part_at(self, point);
    if (kind == ACCESSIBLE_OUTLINE && outline_row_under(self, point) == note_outline_none)
        kind = ACCESSIBLE_WINDOW; /* the column's title and empty space are not actions */
    if (self->pressed_part == ACCESSIBLE_SPLITTER || self->pressed_part == ACCESSIBLE_MINIMAP)
        kind = self->pressed_part;
    if (self->pressed_part != ACCESSIBLE_WINDOW && !self->pressed_by_key &&
        kind != self->pressed_part)
        kind = ACCESSIBLE_WINDOW;
    enum ui_pointer_kind cursor = UI_POINTER_ARROW;
    if (kind != ACCESSIBLE_WINDOW && part_enabled(self, kind) && accessible_visible(self, kind))
        cursor = kind == ACCESSIBLE_SPLITTER ? UI_POINTER_RESIZE : UI_POINTER_ACTION;
    ui_pointer_show(cursor);
}

/* capture 中は押下の内外だけを写す（外へ出たら押していない見た目に戻す）。それ以外は hover。 */
/* The parts that follow the pointer while held: the minimap's thumb, the splitter and a row of
 * the headings column. True when one of them took the move. */
static bool track_held_part(struct folio_window *_Nonnull self, POINT point)
{
    if (self->pressed_part == ACCESSIBLE_MINIMAP)
        drag_minimap(self, point.y);
    else if (self->pressed_part == ACCESSIBLE_SPLITTER)
        drag_drawer_width(self, point.x);
    else if (self->pressed_part == ACCESSIBLE_OUTLINE)
        track_outline_press(self, point);
    else
        return false;
    refresh_pointer_cursor(self, point);
    return true;
}

static void track_pointer(struct folio_window *_Nonnull self, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (track_held_part(self, point))
        return;
    if (self->pressed_part != ACCESSIBLE_WINDOW && !self->pressed_by_key)
    {
        RECT bounds = accessible_main_rect(self, self->pressed_part);
        bool inside = PtInRect(&bounds, point) != 0;
        if (inside != self->pressed_inside)
        {
            self->pressed_inside = inside;
            invalidate_part(self, self->pressed_part);
        }
        refresh_pointer_cursor(self, point);
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
    hover_outline(self, open ? outline_row_under(self, point) : note_outline_none);
    refresh_pointer_cursor(self, point);
}

static void leave_pointer(struct folio_window *_Nonnull self)
{
    self->hover_tracking = false;
    set_hover(self, ACCESSIBLE_WINDOW);
    hover_outline(self, note_outline_none);
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

static bool web_link_ready(const struct folio_window *_Nonnull self)
{
    return self->pane != nullptr && folio_window_accessible_ready(self) &&
           self->command_surface == COMMAND_SURFACE_CLOSED &&
           folio_state_pane_mode(self->state) == PANE_MODE_VIEW &&
           note_pane_link_available(self->pane);
}

/* リンクの宛先を意図へ分ける唯一の場所（ADR0098）。クリックと Ctrl+Enter の両方がここを通る。
 * 内部の宛先（コードブロックのコピー）は検証済みの slot 番号へ変え、それ以外は従来のリンク。 */
/* #945: a footnote number's link moves inside the note. The body's number goes to the footnote
 * (its number, the `back` end); the footnote's number goes back to the first reference. */
static bool activate_footnote(struct folio_window *_Nonnull self, const char *_Nonnull bytes,
                              size_t length)
{
    size_t number = 0;
    struct note_pane *_Nullable pane = self->pane;
    if (pane == nullptr)
        return false;
    switch (footnote_target_parse(bytes, length, &number))
    {
    case FOOTNOTE_TARGET_NOTE:
        (void)note_pane_show_footnote(pane, true, number);
        return true;
    case FOOTNOTE_TARGET_REFERENCE:
        (void)note_pane_show_footnote(pane, false, number);
        return true;
    case FOOTNOTE_TARGET_OTHER:
        return false;
    }
    return false;
}

static void activate_link_target(struct folio_window *_Nonnull self, const char *_Nonnull bytes,
                                 size_t length)
{
    if (activate_footnote(self, bytes, length))
        return;
    size_t slot = 0;
    switch (code_copy_target_parse(bytes, length, &slot))
    {
    case CODE_COPY_TARGET_SLOT:
        report_code_copy(self, slot, folio_state_copy_code(self->state, slot));
        return;
    case CODE_COPY_TARGET_OTHER:
        report_outcome(self, folio_state_open_web_link(self->state, bytes, length));
        return;
    }
}

static void open_web_target(struct folio_window *_Nonnull self, enum note_pane_link_outcome outcome,
                            struct utf8_text *_Nullable target)
{
    switch (outcome)
    {
    case NOTE_PANE_LINK_READY:
        activate_link_target(self, utf8_text_bytes(target), utf8_text_length(target));
        utf8_text_destroy(target);
        return;
    case NOTE_PANE_LINK_ABSENT:
        return;
    case NOTE_PANE_LINK_UNAVAILABLE:
        report_outcome(self, FOLIO_STATE_WEB_LINK_UNAVAILABLE);
        return;
    case NOTE_PANE_LINK_OUT_OF_MEMORY:
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
}

static void open_web_selection(struct folio_window *_Nonnull self)
{
    if (!web_link_ready(self))
        return;
    struct utf8_text *_Nullable target = nullptr;
    enum note_pane_link_outcome outcome = note_pane_link_selection(self->pane, &target);
    open_web_target(self, outcome, target);
}

static LRESULT web_link_notice(struct folio_window *_Nonnull self, const ENLINK *_Nonnull notice)
{
    if (self->pane == nullptr || notice->nmhdr.hwndFrom != note_pane_handle(self->pane))
        return 1;
    if (notice->msg == WM_SETCURSOR)
    {
        bool action = web_link_ready(self) && GetCapture() != note_pane_handle(self->pane);
        ui_pointer_show(action ? UI_POINTER_ACTION : UI_POINTER_TEXT);
        return 1;
    }
    if (!web_link_ready(self))
        return 1;
    struct utf8_text *_Nullable target = nullptr;
    enum note_pane_link_outcome outcome = note_pane_link_click(self->pane, notice, &target);
    open_web_target(self, outcome, target);
    /* Native mouse selection proceeds; activation is consumed by the explicit intent. */
    return notice->msg == WM_LBUTTONUP ? 1 : 0;
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
    help_hint(language, UI_TEXT_TOOLTIP_HELP_KEYS, out);
}

static void splitter_tooltip(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    struct ui_text_request request = {.id = UI_TEXT_TOOLTIP_NAMED_KEY,
                                      .language = language,
                                      .name = ui_text_line(UI_TEXT_DRAWER_WIDTH, language),
                                      .from = ui_text_line(UI_TEXT_DRAWER_WIDTH_HELP, language)};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

static void note_action_tooltip(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                                char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    struct ui_text_request request = {.id = UI_TEXT_TOOLTIP_DESCRIPTION_KEY,
                                      .language = language,
                                      .name =
                                          ui_text_line(accessible_controls[kind].hint, language),
                                      .from = part_shortcut(kind, language)};
    if (ui_text_format(&request, out, draw_unit_limit) != UI_TEXT_FORMAT_READY)
        out[0] = '\0';
}

/* 通常の部品は字の有無に応じて名前/キー/補足を同じformatで組む。 */
static void labeled_part_tooltip(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind, char *_Nonnull out)
{
    enum folio_language language = folio_state_language(self->state);
    const char *key = part_shortcut(kind, language);
    bool labeled = part_labeled(self, kind);
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
        out[0] = '\0';
}

static void part_tooltip(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                         char *_Nonnull out)
{
    enum accessible_kind parent = accessible_controls[kind].parent;
    if (parent == ACCESSIBLE_MODE_GROUP || parent == ACCESSIBLE_ACTION_GROUP)
    {
        note_action_tooltip(self, kind, out);
        return;
    }
    if (kind == ACCESSIBLE_SPLITTER)
    {
        splitter_tooltip(self, out);
        return;
    }
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
    if (kind == ACCESSIBLE_HELP)
    {
        help_tooltip(folio_state_language(self->state), out);
        return;
    }
    labeled_part_tooltip(self, kind, out);
}

static LRESULT tooltip_display(struct folio_window *_Nonnull self, LPARAM lparam)
{
    NMTTDISPINFOW *_Nonnull notice = (NMTTDISPINFOW *)lparam;
    self->tooltip_text[0] = u'\0';
    if (notice->hdr.idFrom == ACCESSIBLE_NOTICE)
    {
        char text[draw_unit_limit];
        status_text(self, text);
        (void)wide_units(text, self->tooltip_text);
    }
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

static bool pane_paste(struct folio_window *_Nonnull self, const MSGFILTER *_Nonnull filter)
{
    /* TranslateMessage precedes DispatchMessage: the key's generated character is not a second
     * paste. */
    if (filter->msg == WM_CHAR && filter->wParam == 22)
        return true;
    if (filter->msg != WM_KEYDOWN)
        return false;
    BYTE modifiers = pressed_shortcut_modifiers();
    bool control_v = filter->wParam == 'V' && (modifiers == (FVIRTKEY | FCONTROL) ||
                                               modifiers == (FVIRTKEY | FCONTROL | FSHIFT));
    bool shift_insert = filter->wParam == VK_INSERT && modifiers == (FVIRTKEY | FSHIFT);
    if (!control_v && !shift_insert)
        return false;
    SendMessageW(note_pane_handle(self->pane), WM_PASTE, 0, 0);
    return true;
}

static LRESULT pane_filter(struct folio_window *_Nonnull self, const MSGFILTER *_Nonnull filter)
{
    if (filter->nmhdr.hwndFrom == note_pane_handle(self->pane) && pane_paste(self, filter))
        return 1;
    if (note_pane_composing(self->pane))
        return 0;
    if (filter->msg == WM_CHAR)
        return pane_character(self, filter->wParam);
    if (filter->msg == WM_KEYDOWN)
        return pane_key(self, filter->wParam);
    return 0;
}

/* RichEdit の鍵の通知。pasteも既存の終了/投入guardを通す。IME中のpasteはpaneで拒否する。 */
/* 編集表示で、RichEdit のドロップ先が受けたファイル（#1062）。0 を返して、RichEdit
 * には入れさせない。 */
static LRESULT files_notice(struct folio_window *_Nonnull self, const ENDROPFILES *_Nonnull dropped)
{
    files_dropped(self, (HDROP)dropped->hDrop, dropped->cp < 0 ? 0 : (size_t)dropped->cp);
    return 0;
}

static LRESULT on_notify(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const NMHDR *_Nonnull header = (const NMHDR *)lparam;
    if (header->code == EN_LINK)
        return web_link_notice(self, (const ENLINK *)lparam);
    if (header->code == EN_DROPFILES)
        return files_notice(self, (const ENDROPFILES *)lparam);
    if ((session_busy(self) || pane_streaming(self)) && header->code == EN_MSGFILTER)
    {
        return 1;
    }
    if (header->hwndFrom == self->tooltip && header->code == TTN_GETDISPINFOW)
    {
        return tooltip_display(self, lparam);
    }
    if (header->code != EN_MSGFILTER || self->pane == nullptr)
    {
        return 0;
    }
    return pane_filter(self, (const MSGFILTER *)lparam);
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

static bool minimap_wheel_target(const struct folio_window *_Nonnull self, LPARAM position)
{
    RECT bounds = minimap_rect(self);
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    return ScreenToClient(self->handle, &point) && PtInRect(&bounds, point);
}

static bool forward_minimap_wheel(const struct folio_window *_Nonnull self, WPARAM keys,
                                  LPARAM position)
{
    if (!minimap_wheel_target(self, position))
        return false;
    if (minimap_available(self))
    {
        note_pane_scroll_wheel(self->pane, GET_WHEEL_DELTA_WPARAM(keys));
        return true;
    }
    if (self->pane != nullptr)
        note_pane_cancel_wheel(self->pane);
    return true;
}

static bool outline_wheel_target(const struct folio_window *_Nonnull self, LPARAM position)
{
    RECT bounds = outline_rect(self);
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    return !IsRectEmpty(&bounds) && ScreenToClient(self->handle, &point) &&
           PtInRect(&bounds, point);
}

/* After the column moved under a still pointer, the hover is the row now there. */
static void rehover_outline(struct folio_window *_Nonnull self, LPARAM position)
{
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (ScreenToClient(self->handle, &point))
        (void)note_outline_set_hover(self->outline, outline_row_under(self, point));
}

/* #975: the wheel over the headings column moves the column by three rows a notch, keeping
 * what a fine wheel sends below a notch for the next message. The body stays where it is. */
static bool forward_outline_wheel(struct folio_window *_Nonnull self, WPARAM keys, LPARAM position)
{
    if (!outline_wheel_target(self, position))
        return false;
    if (self->pane != nullptr)
        note_pane_cancel_wheel(self->pane);
    if (!outline_available(self))
        return true;
    int delta = GET_WHEEL_DELTA_WPARAM(keys);
    if ((self->outline_wheel > 0 && delta < 0) || (self->outline_wheel < 0 && delta > 0))
        self->outline_wheel = 0;
    self->outline_wheel += delta * 3;
    int rows = -(self->outline_wheel / WHEEL_DELTA);
    self->outline_wheel %= WHEEL_DELTA;
    struct note_outline_paint paint = outline_place(self);
    if (rows != 0 &&
        note_outline_scroll(self->outline, &paint, rows, note_pane_heading_count(self->pane)))
    {
        rehover_outline(self, position);
        InvalidateRect(self->handle, &paint.bounds, FALSE);
        accessibility_host_schedule(self->accessibility);
    }
    return true;
}

/* 本文とminimapのwheelは同じ本文位置へ、ドロワー上は従来の索引へ渡す。
 * lparam はスクリーン座標なので、位置を読むために OS へ問い合わせない（ARC-007）。 */
static void forward_wheel(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (forward_outline_wheel(self, wparam, lparam) ||
        forward_minimap_wheel(self, wparam, lparam) || forward_body_wheel(self, wparam, lparam))
        return;
    if (self->pane != nullptr)
        note_pane_cancel_wheel(self->pane);
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
    if (self->keyboard_target == ACCESSIBLE_SPLITTER || self->keyboard_target == ACCESSIBLE_MINIMAP)
        return false;
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
static bool minimap_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->keyboard_target != ACCESSIBLE_MINIMAP || !minimap_available(self))
        return false;
    struct note_minimap_paint paint;
    if (!minimap_paint_for(self, &paint))
        return false;
    LONGLONG target = paint.view.first;
    switch (key)
    {
    case VK_UP:
        target -= paint.view.step;
        break;
    case VK_DOWN:
        target += paint.view.step;
        break;
    case VK_PRIOR:
        target -= paint.view.visible;
        break;
    case VK_NEXT:
        target += paint.view.visible;
        break;
    case VK_HOME:
        target = 0;
        break;
    case VK_END:
        target = note_pane_geometry_limit(paint.view);
        break;
    default:
        return false;
    }
    int limit = note_pane_geometry_limit(paint.view);
    target = target < 0 ? 0 : target;
    target = target > limit ? limit : target;
    scroll_minimap(self, (int)target);
    show_focus_cues(self, true);
    return true;
}

static bool splitter_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->pressed_part == ACCESSIBLE_SPLITTER)
    {
        cancel_part_press(self);
        if (key == VK_ESCAPE)
            return true;
    }
    if (self->keyboard_target != ACCESSIBLE_SPLITTER || !part_enabled(self, ACCESSIBLE_SPLITTER))
        return false;
    struct drawer_width_range range = drawer_width_for(self);
    int width = range.value;
    switch (key)
    {
    case VK_LEFT:
        width -= base_splitter_small_change;
        break;
    case VK_RIGHT:
        width += base_splitter_small_change;
        break;
    case VK_PRIOR:
        width -= base_splitter_large_change;
        break;
    case VK_NEXT:
        width += base_splitter_large_change;
        break;
    case VK_HOME:
        width = range.minimum;
        break;
    case VK_END:
        width = range.maximum;
        break;
    default:
        return false; /* Win32の鍵は開いた集合。 */
    }
    width = width < range.minimum ? range.minimum : width;
    width = width > range.maximum ? range.maximum : width;
    apply_drawer_width(self, width);
    show_focus_cues(self, true);
    return true;
}

/* #975: the row a key moves the column's cursor to, or none for a key the column does not use.
 * A page is the rows the column shows. */
static size_t outline_key_target(WPARAM key, size_t cursor, size_t count, size_t page)
{
    size_t last = count == 0 ? 0 : count - 1;
    size_t step = page == 0 ? 1 : page;
    size_t up = 0;   /* rows toward the first */
    size_t down = 0; /* rows toward the last */
    switch (key)
    {
    case VK_UP:
        up = 1;
        break;
    case VK_DOWN:
        down = 1;
        break;
    case VK_PRIOR:
        up = step;
        break;
    case VK_NEXT:
        down = step;
        break;
    case VK_HOME:
        up = SIZE_MAX;
        break;
    case VK_END:
        down = SIZE_MAX;
        break;
    default:
        return note_outline_none; /* Win32の鍵は開いた集合。 */
    }
    if (cursor > last)
        cursor = last;
    if (up > 0)
        return cursor > up ? cursor - up : 0;
    return last - cursor > down ? cursor + down : last;
}

/* Puts the column's keyboard cursor on a row and shows it. */
static void outline_set_cursor(struct folio_window *_Nonnull self, size_t row)
{
    struct note_outline_paint paint = outline_place(self);
    note_outline_set_cursor(self->outline, &paint, row, note_pane_heading_count(self->pane));
    InvalidateRect(self->handle, &paint.bounds, FALSE);
    accessibility_host_schedule(self->accessibility);
}

/* ↑↓・PageUp/Down・Home/End move the cursor between rows; Enter and Space take the shared
 * activation path (press_activation -> keyboard_activate). */
static bool outline_key(struct folio_window *_Nonnull self, WPARAM key)
{
    if (self->keyboard_target != ACCESSIBLE_OUTLINE || !outline_available(self))
        return false;
    struct note_outline_paint paint = outline_place(self);
    size_t count = note_pane_heading_count(self->pane);
    size_t target = outline_key_target(key, note_outline_cursor(self->outline), count,
                                       note_outline_rows(&paint));
    if (target == note_outline_none)
        return false;
    if (count > 0)
        outline_set_cursor(self, target);
    show_focus_cues(self, true);
    return true;
}

static bool range_key(struct folio_window *_Nonnull self, WPARAM key)
{
    return outline_key(self, key) || minimap_key(self, key) || splitter_key(self, key);
}

static void press_key(struct folio_window *_Nonnull self, WPARAM key, LPARAM lparam)
{
    if (range_key(self, key))
        return;
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

/* 欄の文字を読まずに済む Enter。パレットは選択中の候補、検索欄は次の一致。 */
static bool execute_selection_input(struct folio_window *_Nonnull self)
{
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        execute_palette_selection(self);
        return true;
    }
    /* 検索欄の Enter は欄を閉じず、覚えている向きで次の一致へ進む（ADR 0023 の決定 4）。 */
    if (self->command_surface == COMMAND_SURFACE_SEARCH)
    {
        repeat_search(self, false);
        return true;
    }
    return false;
}

static void execute_command_input(struct folio_window *_Nonnull self)
{
    if (execute_selection_input(self))
        return;
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
    case COMMAND_SURFACE_MOVE:
    case COMMAND_SURFACE_ABOUT:
    case COMMAND_SURFACE_TAG_EDIT:
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
    case COMMAND_SURFACE_DISCOVERY:
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
    if (key == VK_PRIOR)
        self->command_help_scroll = first == 0 ? 0 : help_page_at(self, first - 1);
    else
        self->command_help_scroll = next_help_page(self, first);
    redraw_command_layer(self);
}

static void update_command_composition(struct folio_window *_Nonnull self, UINT message)
{
    if (message == WM_IME_STARTCOMPOSITION)
    {
        self->command_composing = true;
        discovery_motion_settle(self);
    }
    if (message == WM_IME_ENDCOMPOSITION)
    {
        self->command_composing = false;
        discovery_motion_sync(self);
    }
}

/* Tabは設定では選択肢の巡回、パレットでは説明、置換では2つの欄の往復。 */
static bool command_tab(struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        return discovery_tab(self);
    if (tag_surface(self))
        return tag_tab(self);
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        toggle_command_keys(self);
        return true;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return settings_navigate(self, VK_TAB);
    }
    if (about_surface(self))
        return about_navigate(self, VK_TAB);
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
    if (discovery_surface(self))
        return discovery_return(self);
    if (tag_surface(self))
        return tag_return(self);
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
    if (move_surface(self))
    {
        adopt_move_at(self, self->command_selection);
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
    if (about_surface(self))
    {
        about_activate(self);
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
            settings_rows[index].kind == SETTINGS_ROW_MINIMAP ||
            settings_rows[index].kind == SETTINGS_ROW_FAVORITES_FIRST ||
            settings_rows[index].kind == SETTINGS_ROW_MOTION ||
            settings_rows[index].kind == SETTINGS_ROW_WINDOW ||
            settings_rows[index].kind == SETTINGS_ROW_THEME ||
            settings_rows[index].kind == SETTINGS_ROW_STORAGE ||
            settings_rows[index].kind == SETTINGS_ROW_ABOUT)
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
static bool navigate_palette_keys(struct folio_window *_Nonnull self, WPARAM key)
{
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

static bool command_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (discovery_surface(self))
        return discovery_navigate(self, key);
    if (tag_surface(self))
        return tag_navigate(self, key);
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        return theme_navigate(self, key);
    }
    if (move_surface(self))
        return move_navigate(self, key);
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
    {
        return settings_navigate(self, key);
    }
    if (about_surface(self))
        return about_navigate(self, key);
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
    {
        return history_navigate(self, key);
    }
    return navigate_palette_keys(self, key);
}

/* EDITを持たない設定とアプリの情報では、Spaceも採用の鍵になる。 */
static bool surface_space_activates(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_SETTINGS || about_surface(self);
}

static bool command_activation_key(const struct folio_window *_Nonnull self, WPARAM key)
{
    if (key == VK_RETURN)
        return true;
    if (key != VK_SPACE)
        return false;
    if (surface_space_activates(self))
        return true;
    if (GetFocus() != self->command_layer)
        return false;
    if (discovery_surface(self))
        return self->discovery_focus != DISCOVERY_FOCUS_ROWS;
    return tag_surface(self);
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
        if (self->command_surface == COMMAND_SURFACE_THEMES || about_surface(self))
        {
            return_to_settings(self);
            return true;
        }
        close_command_surface(self);
        return true;
    }
    if (command_activation_key(self, key))
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
    if (tag_surface(self) && (character == shortcut_character_for(FOLIO_COMMAND_SAVE) ||
                              character == shortcut_character_for(FOLIO_COMMAND_NEW)))
        return true;
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
    /* '\n' is Ctrl+Enter (#940): handled at the key, never typed into a one-line field. */
    return character == '\r' || character == '\n' || character == '\t' ||
           (character == ' ' && surface_space_activates(self));
}

static void scroll_settings_content(struct folio_window *_Nonnull self, int delta)
{
    int offset =
        (int)self->command_first +
        scale(base_settings_item_height, GetDpiForWindow(self->handle)) * (delta > 0 ? -1 : 1);
    size_t limit = settings_content_limit(self);
    self->command_first = offset > 0 ? (size_t)offset : 0;
    if (self->command_first > limit)
    {
        self->command_first = limit;
    }
    refresh_surface_tooltip_rects(self);
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

/* #941: by accumulated notches, as the theme picker, so a precision touchpad does not race. */
static void wheel_move_selection(struct folio_window *_Nonnull self, int delta)
{
    for (int steps = theme_wheel_steps(self, delta); steps != 0; steps += steps < 0 ? 1 : -1)
        (void)move_navigate(self, steps < 0 ? VK_UP : VK_DOWN);
}

static bool wheel_candidate_navigation(struct folio_window *_Nonnull self, WPARAM key)
{
    if (discovery_surface(self))
        return discovery_navigate(self, key);
    if (tag_surface(self))
        return tag_navigate(self, key);
    if (self->command_surface == COMMAND_SURFACE_HISTORY)
        return history_navigate(self, key);
    return false;
}

static bool command_wheel(struct folio_window *_Nonnull self, WPARAM wparam)
{
    bool listed = self->command_surface == COMMAND_SURFACE_PALETTE || framed_surface(self) ||
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
    if (wheel_candidate_navigation(self, key))
        return true;
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
    if (move_surface(self))
    {
        wheel_move_selection(self, delta);
        return true;
    }
    if (settings_surface(self))
    {
        scroll_settings_content(self, delta);
        return true;
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
    /* #940: Alt+Left / Alt+Right switch names/body from anywhere on the discovery surface. The
     * key is not consumed: the default handling must see that a key followed Alt, or releasing
     * Alt alone would start the window menu. */
    if (message == WM_SYSKEYDOWN && discovery_surface(self))
        (void)discovery_switch_mode(self, wparam);
    return false;
}

/* 未確定入力を抜けたあとに、開いている面ごとに 1 回だけ取り直す。 */
static void command_composition_settled(struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        discovery_query_changed(self);
    if (tag_surface(self))
        tag_query_changed(self);
    if (self->command_surface == COMMAND_SURFACE_THEMES)
    {
        invalidate_surface_items(self);
        theme_query_changed(self);
    }
    if (move_surface(self))
    {
        invalidate_surface_items(self);
        move_query_changed(self);
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

static bool command_input_consumed(struct folio_window *_Nonnull self, UINT message, WPARAM wparam)
{
    if (message == WM_PASTE && discovery_surface(self))
    {
        discovery_paste(self);
        return true;
    }
    return command_input_handled(self, message, wparam);
}

static void discovery_input_placeholder(struct folio_window *_Nonnull self, HWND window,
                                        UINT message)
{
    if (message != WM_PAINT || !discovery_surface(self))
        return;
    enum ui_text label = folio_state_discovery_summary(self->state).mode == DISCOVERY_MODE_BODY
                             ? UI_TEXT_DISCOVERY_BODY_INPUT
                             : UI_TEXT_DISCOVERY_INPUT;
    paint_input_placeholder(self, window, label, "");
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
    if (command_input_consumed(self, message, wparam))
    {
        return 0;
    }
    command_focus_changed(self, message);
    LRESULT result = CallWindowProcW(self->command_input_original, window, message, wparam, lparam);
    discovery_input_placeholder(self, window, message);
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

/* #855: the field's key at the right end of the placeholder, only when it fits after the label
 * (`label` is the label's measured extent) with a gap of half a line. An empty field shows no
 * clear button, so the key may use its margin; the right padding mirrors the left one. */
static void paint_placeholder_hint(HDC _Nonnull device, HWND window, RECT label,
                                   const char *_Nonnull hint)
{
    char16_t key[draw_unit_limit];
    int keys = wide_units(hint, key);
    if (keys == 0)
        return;
    RECT bounds;
    RECT client;
    SendMessageW(window, EM_GETRECT, 0, (LPARAM)&bounds);
    GetClientRect(window, &client);
    bounds.right = client.right - (bounds.left - client.left);
    RECT measured = bounds;
    DrawTextW(device, key, keys, &measured, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    LONG gap = (label.bottom - label.top) / 2;
    if ((label.right - label.left) + gap + (measured.right - measured.left) >
        bounds.right - bounds.left)
        return;
    DrawTextW(device, key, keys, &bounds, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
}

/* 空のあいだは用途を薄い文字で重ねる（EM_SETCUEBANNER は comctl32 を要るので使わない）。 */
static void paint_input_placeholder(const struct folio_window *_Nonnull self, HWND window,
                                    enum ui_text label, const char *_Nonnull hint)
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
    int count = wide_units(ui_text_line(label, folio_state_language(self->state)), units);
    DrawTextW(device, units, count, &bounds, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    RECT extent = bounds;
    DrawTextW(device, units, count, &extent, DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
    paint_placeholder_hint(device, window, extent, hint);
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
static bool filter_clear_at(const struct folio_window *_Nonnull self, HWND window, POINT point)
{
    RECT bounds = filter_clear_rect(self, window);
    return folio_state_filtering(self->state) && PtInRect(&bounds, point);
}

static bool filter_clear_pressed(struct folio_window *_Nonnull self, HWND window, LPARAM lparam)
{
    POINT point = {GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    if (!filter_clear_at(self, window, point))
        return false;
    SetFocus(window);
    if (GetFocus() == window)
    {
        SetWindowTextW(window, L"");
    }
    return true;
}

static void filter_pointer(const struct folio_window *_Nonnull self, HWND window, POINT point)
{
    enum ui_pointer_kind cursor = UI_POINTER_ARROW;
    /* EDIT自身の入力門と揃える。IME中も文字入力と消去は既定の入力経路へ通る。 */
    if (IsWindowEnabled(window) && IsWindowEnabled(self->handle) &&
        !session_blocks_message(self, WM_LBUTTONDOWN))
        cursor = GetCapture() != window && filter_clear_at(self, window, point) ? UI_POINTER_ACTION
                                                                                : UI_POINTER_TEXT;
    ui_pointer_show(cursor);
}

static bool filter_pointer_message(const struct folio_window *_Nonnull self, UINT message,
                                   WPARAM wparam, LPARAM lparam)
{
    HWND window = self->filter_input;
    if (message == WM_SETCURSOR && ui_pointer_client(window, wparam, lparam))
    {
        filter_pointer(self, window, ui_pointer_message_point(window));
        return true;
    }
    if (message == WM_MOUSEMOVE)
        filter_pointer(self, window, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
    return false;
}

/* 検索は保存済み本文だけを見る。名前付きEDITのfocus入口で共通保存を先に通す。 */
static enum folio_state_outcome save_before_catalog(struct folio_window *_Nonnull self,
                                                    enum note_save_mode mode)
{
    if (self->filter_saving || session_busy(self) || pane_streaming(self) || self->removal_running)
        return FOLIO_STATE_CANCELLED;
    if (folio_state_document_kind(self->state) != FOLIO_DOCUMENT_NAMED ||
        folio_state_pane_mode(self->state) != PANE_MODE_EDIT)
        return FOLIO_STATE_READY;
    if (self->pane != nullptr && note_pane_composing(self->pane))
    {
        focus_pane(self);
        return FOLIO_STATE_CANCELLED;
    }
    self->filter_saving = true;
    enum folio_state_outcome outcome = store_body_mode(self, mode);
    self->filter_saving = false;
    if (outcome != FOLIO_STATE_READY)
        focus_pane(self);
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    return outcome;
}

static enum folio_state_outcome save_before_filter(struct folio_window *_Nonnull self)
{
    return save_before_catalog(self, NOTE_SAVE_EDITS);
}

static void save_on_filter_focus(struct folio_window *_Nonnull self)
{
    enum folio_state_outcome outcome = save_before_filter(self);
    if (outcome != FOLIO_STATE_READY && outcome != FOLIO_STATE_CANCELLED)
    {
        report_outcome(self, outcome);
        focus_pane(self);
    }
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
    if (filter_pointer_message(self, message, wparam, lparam))
        return TRUE;
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
        paint_input_placeholder(self, window, UI_TEXT_PLACEHOLDER_FILTER,
                                folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_FILTER,
                                                           folio_state_language(self->state)));
        paint_filter_clear(self, window);
    }
    return result;
}

/* 準備開始/取消/完成/失敗は同じ表示更新へ合流する。本文の投入は行わない。 */
static void refresh_index_filter(struct folio_window *_Nonnull self, bool was_filtering)
{
    settle_empty_drawer_focus(self);
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
           ((choice_surface(self) || tag_surface(self) || discovery_surface(self)) &&
            self->command_composing);
}

static LRESULT command_query_changed(struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
    {
        discovery_query_changed(self);
        return 0;
    }
    if (self->command_surface == COMMAND_SURFACE_PALETTE || choice_surface(self))
    {
        invalidate_surface_items(self);
    }
    if (move_surface(self))
    {
        move_query_changed(self);
        return 0;
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

static LRESULT on_command(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if ((HWND)lparam == self->discovery_preview)
    {
        if (HIWORD(wparam) == EN_VSCROLL)
            redraw_command_layer(self);
        return 0;
    }
    if (!command_owns(self, (HWND)lparam) || HIWORD(wparam) != EN_CHANGE)
    {
        return 0;
    }
    if (command_query_deferred(self))
        return 0;
    if (tag_surface(self))
    {
        tag_query_changed(self);
        return 0;
    }
    return command_query_changed(self);
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
    return create_discovery_preview(self);
}

/* 見えていない部品の tool は空の矩形にして出さない。矩形は描画と同じ 1 本。 */
static RECT tooltip_rect(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    bool visible =
        part_enabled(self, kind) && accessible_visible(self, kind) && kind != ACCESSIBLE_OUTLINE;
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

/* ドロワーの固定操作のtool。名前はUIAと同じ表から引き、矩形はドロワーのclient
 * 座標。 文言を固定で渡すので、言語と配置が変わるたびに描き直しの経路から写し直す。 */
static const enum accessible_kind drawer_tools[] = {ACCESSIBLE_SCOPE_ALL,
                                                    ACCESSIBLE_SCOPE_FAVORITES,
                                                    ACCESSIBLE_TAG_FILTER, ACCESSIBLE_NEW_CATEGORY};

static TOOLINFOW drawer_tool(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                             char16_t *_Nonnull text)
{
    HWND drawer = self->drawer == nullptr ? nullptr : drawer_window_handle(self->drawer);
    TOOLINFOW tool = {.cbSize = tooltip_info_size,
                      .uFlags = TTF_SUBCLASS,
                      .hwnd = drawer,
                      .uId = kind,
                      .lpszText = (wchar_t *)text};
    text[0] = u'\0';
    (void)wide_units(
        ui_text_line(accessible_controls[kind].label, folio_state_language(self->state)), text);
    struct accessible_item item = {.kind = kind};
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

static bool create_drawer_tooltips(const struct folio_window *_Nonnull self)
{
    char16_t text[draw_unit_limit];
    for (size_t at = 0; at < sizeof drawer_tools / sizeof drawer_tools[0]; ++at)
    {
        TOOLINFOW tool = drawer_tool(self, drawer_tools[at], text);
        if (tool.hwnd == nullptr || !SendMessageW(self->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool))
            return false;
    }
    return true;
}

static void refresh_drawer_tooltips(const struct folio_window *_Nonnull self)
{
    char16_t text[draw_unit_limit];
    for (size_t at = 0; at < sizeof drawer_tools / sizeof drawer_tools[0]; ++at)
    {
        TOOLINFOW tool = drawer_tool(self, drawer_tools[at], text);
        if (tool.hwnd != nullptr)
        {
            SendMessageW(self->tooltip, TTM_NEWTOOLRECTW, 0, (LPARAM)&tool);
            SendMessageW(self->tooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&tool);
        }
    }
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

static TOOLINFOW status_tool(const struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    RECT bounds = self->command_surface == COMMAND_SURFACE_CLOSED && self->native_modal_depth == 0
                      ? pane_notice_rect(self, client)
                      : (RECT){0};
    return (TOOLINFOW){.cbSize = tooltip_info_size,
                       .uFlags = TTF_SUBCLASS,
                       .hwnd = self->handle,
                       .uId = ACCESSIBLE_NOTICE,
                       .rect = bounds,
                       .lpszText = LPSTR_TEXTCALLBACKW};
}

static void refresh_tooltip_rects(const struct folio_window *_Nonnull self)
{
    if (self->tooltip == nullptr)
    {
        return;
    }
    refresh_drawer_tooltips(self);
    SendMessageW(self->tooltip, TTM_POP, 0, 0);
    TOOLINFOW status = status_tool(self);
    SendMessageW(self->tooltip, TTM_NEWTOOLRECTW, 0, (LPARAM)&status);
    for (size_t index = 0; index < main_part_count(); ++index)
    {
        TOOLINFOW tool = {.cbSize = tooltip_info_size,
                          .hwnd = self->handle,
                          .uId = main_parts[index],
                          .rect = tooltip_rect(self, main_parts[index])};
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
                          .uId = main_parts[index],
                          .rect = tooltip_rect(self, main_parts[index]),
                          .lpszText = LPSTR_TEXTCALLBACKW};
        if (!SendMessageW(self->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&tool))
        {
            return false;
        }
    }
    if (!create_drawer_tooltips(self))
    {
        return false;
    }
    refresh_tooltip_style(self);
    activate_tooltip(self);
    TOOLINFOW status = status_tool(self);
    return SendMessageW(self->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&status) != 0 &&
           create_surface_tooltips(self);
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
           theme_candidates_create(&self->theme_candidates) == CANDIDATE_CREATED &&
           tag_candidates_create(&self->tag_candidates) == CANDIDATE_CREATED;
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

/* 大小のhandleはこの窓が所有し、両方取得してから公開して旧pairを解放する。 */
static bool refresh_window_icons(struct folio_window *_Nonnull self, UINT dpi)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    HICON large_icon =
        LoadImageW(instance, L"NENEFOLIO", IMAGE_ICON, GetSystemMetricsForDpi(SM_CXICON, dpi),
                   GetSystemMetricsForDpi(SM_CYICON, dpi), LR_DEFAULTCOLOR);
    if (large_icon == nullptr)
        return false;
    HICON small_icon =
        LoadImageW(instance, L"NENEFOLIO", IMAGE_ICON, GetSystemMetricsForDpi(SM_CXSMICON, dpi),
                   GetSystemMetricsForDpi(SM_CYSMICON, dpi), LR_DEFAULTCOLOR);
    if (small_icon == nullptr)
    {
        DestroyIcon(large_icon);
        return false;
    }
    SendMessageW(self->handle, WM_SETICON, ICON_BIG, (LPARAM)large_icon);
    SendMessageW(self->handle, WM_SETICON, ICON_SMALL, (LPARAM)small_icon);
    if (self->icon_large != nullptr)
        DestroyIcon(self->icon_large);
    if (self->icon_small != nullptr)
        DestroyIcon(self->icon_small);
    self->icon_large = large_icon;
    self->icon_small = small_icon;
    return true;
}

/* OS が勧める矩形へ移し、字の大きさを取り直す（FR-013）。 */
static void apply_dpi(struct folio_window *_Nonnull self, LPARAM lparam)
{
    discovery_motion_reset(self);
    cancel_part_press(self);
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

static HRGN _Nullable command_update_region(const struct folio_window *_Nonnull self)
{
    if (!discovery_surface(self))
        return nullptr;
    HRGN changed = CreateRectRgn(0, 0, 0, 0);
    if (changed != nullptr && GetUpdateRgn(self->command_layer, changed, FALSE) == ERROR)
    {
        DeleteObject(changed);
        return nullptr;
    }
    return changed;
}

static void apply_command_clip(const struct folio_window *_Nonnull self, HDC device,
                               HRGN _Nullable changed, RECT area)
{
    if (!discovery_surface(self))
        return;
    if (changed != nullptr)
        SelectClipRgn(device, changed);
    if (changed == nullptr)
        IntersectClipRect(device, area.left, area.top, area.right, area.bottom);
}

static void paint_command_layer(struct folio_window *_Nonnull self)
{
    HRGN changed = command_update_region(self);
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
        apply_command_clip(self, memory, changed, painting.rcPaint);
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
    if (changed != nullptr)
        DeleteObject(changed);
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
    if ((message == WM_KEYDOWN || message == WM_CHAR || message == WM_SYSKEYDOWN) &&
        command_input_handled(self, message, wparam))
    {
        return true;
    }
    return false;
}

static size_t surface_tab_at(const struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_surface != COMMAND_SURFACE_PALETTE && !discovery_surface(self))
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
    ui_pointer_show(surface_cursor(self, point));
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

static void surface_choose_tab(struct folio_window *_Nonnull self, size_t tab)
{
    if (discovery_surface(self))
        discovery_choose_mode(self, tab);
    if (self->command_surface == COMMAND_SURFACE_PALETTE && surface_current_tab(self) != tab)
        toggle_command_keys(self);
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
    if (run && slot >= surface_button_count)
        surface_choose_tab(self, slot - surface_button_count);
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
    if (message == WM_SETCURSOR && ui_pointer_client(window, wparam, lparam))
    {
        ui_pointer_show(surface_cursor(self, ui_pointer_message_point(window)));
        return TRUE;
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
    discovery_motion_settle(self);
    clear_part_press(self);
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
    if (self->word_font != nullptr)
    {
        DeleteObject(self->word_font);
        self->word_font = nullptr;
    }
    if (self->gutter_font != nullptr)
    {
        DeleteObject(self->gutter_font);
        self->gutter_font = nullptr;
    }
    command_help_fonts_destroy(&self->help_fonts);
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

static bool content_shortcut_allowed(const struct folio_window *_Nonnull self)
{
    if (framed_surface(self) || self->removal_running || !IsWindowEnabled(self->handle) ||
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

static bool note_shortcut_allowed(const struct folio_window *_Nonnull self)
{
    return content_shortcut_allowed(self) && document_open(self) && self->native_modal_depth == 0 &&
           !session_busy(self) && !pane_streaming(self) && self->close_phase == CLOSE_PHASE_IDLE;
}

static void toggle_mode_shortcut(struct folio_window *_Nonnull self)
{
    if (!note_shortcut_allowed(self))
        return;
    switch (folio_state_pane_mode(self->state))
    {
    case PANE_MODE_VIEW:
        execute_command(self, FOLIO_COMMAND_EDIT, "");
        return;
    case PANE_MODE_EDIT:
        execute_command(self, FOLIO_COMMAND_VIEW, "");
        return;
    }
}

static void operations_shortcut(struct folio_window *_Nonnull self)
{
    if (!note_shortcut_allowed(self) || !accessible_visible(self, ACCESSIBLE_MENU))
        return;
    show_focus_cues(self, true);
    show_part_menu(self, ACCESSIBLE_MENU);
}

/* Shift+F10 and the menu key. */
static void context_menu_shortcut(struct folio_window *_Nonnull self)
{
    /* #908: with the body's menu open the focus is on the menu, not the body. The key then
     * closes that menu; it must not swap it for the list's menu (rename, trash). */
    if (self->command_surface == COMMAND_SURFACE_MENU && text_menu_context(self->menu.context))
    {
        close_command_surface(self);
        return;
    }
    /* #883: in the note body the keys open its own menu at the caret. */
    if (self->pane != nullptr && GetFocus() == note_pane_handle(self->pane))
        SendMessageW(note_pane_handle(self->pane), WM_CONTEXTMENU,
                     (WPARAM)note_pane_handle(self->pane), -1);
    else if (self->drawer != nullptr)
        drawer_window_context_menu(self->drawer);
}

static void run_shortcut(struct folio_window *_Nonnull self,
                         const struct folio_shortcut *_Nonnull binding)
{
    switch (folio_shortcut_action(binding))
    {
    case FOLIO_SHORTCUT_ACTION_COMMAND:
    {
        enum folio_command command = folio_shortcut_command(binding);
        if ((folio_command_scope(command) != FOLIO_COMMAND_SCOPE_LAYOUT &&
             !view_toggle_command(command)) ||
            content_shortcut_allowed(self))
            execute_command(self, command, "");
        return;
    }
    case FOLIO_SHORTCUT_ACTION_TOGGLE_MODE:
        toggle_mode_shortcut(self);
        return;
    case FOLIO_SHORTCUT_ACTION_OPERATIONS:
        operations_shortcut(self);
        return;
    case FOLIO_SHORTCUT_ACTION_CONTEXT_MENU:
        context_menu_shortcut(self);
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
static bool pane_notification_source(const struct folio_window *_Nonnull self, WPARAM wparam,
                                     LPARAM lparam)
{
    HWND pane = self->pane == nullptr ? nullptr : note_pane_handle(self->pane);
    return pane != nullptr && (HWND)lparam == pane && LOWORD(wparam) == note_pane_control_id;
}

static bool pane_notification(struct folio_window *_Nonnull self, WPARAM wparam, LPARAM lparam)
{
    if (!pane_notification_source(self, wparam, lparam))
    {
        return false;
    }
    if (HIWORD(wparam) == EN_CHANGE)
    {
        /* 印と無効化だけ。EM_STREAMIN の最中に本文へ問い合わせない（決定 4）。 */
        note_pane_invalidate_lines(self->pane);
        self->body_touched = true;
        if (pane_streaming(self))
        {
            note_minimap_clear(self->minimap);
            return true; /* loaded_pane publishes the completed document once. */
        }
        request_minimap(self, MINIMAP_UPDATE_SETTLE);
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
    if (!framed_surface(self) || self->native_modal_depth != 0 || IsIconic(self->handle) ||
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
    discovery_motion_reset(self);
    cancel_part_press(self);
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
    discovery_motion_reset(self);
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
    {
        if (self->native_modal_depth == 0 && !pane_streaming(self) && !session_busy(self))
        {
            folio_state_refresh_note_times(self->state);
            RECT client;
            GetClientRect(self->handle, &client);
            RECT status = pane_notice_rect(self, client);
            InvalidateRect(self->handle, &status, FALSE);
            accessibility_host_schedule(self->accessibility);
        }
        return;
    }
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
            cancel_part_press(self);
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
        cancel_part_press(self);
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

/* #1071 / ADR0118: the line that says how a link was opened. */
static void open_link_notice(struct folio_window *_Nonnull self, enum ui_text line)
{
    self->open_line = line;
    if (self->notice == PANE_NOTICE_OPEN_LINK)
        InvalidateRect(self->handle, nullptr, FALSE);
    else
        set_pane_notice(self, PANE_NOTICE_OPEN_LINK);
    accessibility_host_schedule(self->accessibility);
}

/* A link does not open while something else holds the window: a surface with its own rows or
 * input (anything but the in-note search), a native dialog, a removal, a session ending, a
 * layout change, a load, or a pointer captured mid-drag (ADR0118 decision 3). Those may hold
 * places in the lists, and locating a note reads the lists again. */
static bool open_link_blocked(const struct folio_window *_Nonnull self)
{
    bool surface = self->command_surface != COMMAND_SURFACE_CLOSED &&
                   self->command_surface != COMMAND_SURFACE_SEARCH;
    bool held = surface || self->native_modal_depth > 0 || self->removal_running ||
                self->layout_changing || pane_streaming(self) || GetCapture() != nullptr;
    /* Inside another handler (a pump nested in it dispatched this), while the session or the
     * window is ending, or while an IME composition is open: the same refusals as the other
     * paths that save and reload on the user's behalf. */
    bool between = self->dispatch_depth > 1 || session_busy(self) ||
                   self->close_phase != CLOSE_PHASE_IDLE || session_composing(self);
    return held || between || !IsWindowEnabled(self->handle);
}

/* #1070 / ADR0119 decision 3: the open note's file is no longer what was read. The reading view
 * reads it again and stays where it was being read; an edit is only told, and neither its text
 * nor its Undo is touched (decision 4). A file that cannot be read says nothing here. */
static void show_outside_change(struct folio_window *_Nonnull self)
{
    bool stale = false;
    if (self->pane == nullptr || pane_unavailable(self) ||
        folio_state_document_stale(self->state, &stale) != FOLIO_STATE_READY || !stale)
        return;
    if (folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
    {
        /* Said once: a line that is still up is not raised (and read out) again. */
        if (self->notice != PANE_NOTICE_OPEN_LINK || self->open_line != UI_TEXT_OUTSIDE_CHANGED)
            open_link_notice(self, UI_TEXT_OUTSIDE_CHANGED);
        return;
    }
    size_t line = note_pane_first_visible_line(self->pane);
    if (folio_state_reload_document(self->state) != FOLIO_STATE_READY)
        return; /* unreadable since the look a moment ago: nothing was changed */
    note_outline_hold(self->outline);
    bool shown = render_pane(self) == FOLIO_STATE_READY;
    if (line > 0 && !pane_unavailable(self))
        note_pane_scroll_to_line(self->pane, line);
    restore_outline(self);
    if (shown)
        open_link_notice(self, UI_TEXT_OUTSIDE_RELOADED);
}

/* The data folder is read again only while the window holds nothing: the lists are replaced,
 * and whatever holds a place in them would then point at another note or category (#1069).
 * The same refusals as a link, and no surface at all: the in-note search holds places in the
 * body that reading the note again would move. No side effects: the message loop asks this
 * between an empty queue and its wait (ADR0068). */
static bool outside_refresh_ready(const struct folio_window *_Nonnull self)
{
    return self->outside_due && self->command_surface == COMMAND_SURFACE_CLOSED &&
           !self->filter_saving && !open_link_blocked(self) && folio_window_accessible_ready(self);
}

/* Reads the lists and the open note again. Nothing is written, and a list that cannot be read
 * is not told here: this runs on every return to the window, and the next intent that needs
 * the list tells it. */
static void refresh_outside(struct folio_window *_Nonnull self)
{
    self->outside_due = false;
    folio_window_begin_dispatch(self);
    bool changed = false;
    enum folio_state_outcome lists = folio_state_refresh_lists(self->state, &changed);
    if (changed)
    {
        redraw_drawer(self);
        InvalidateRect(self->handle, nullptr, FALSE);
        accessibility_host_schedule(self->accessibility);
    }
    if (lists != FOLIO_STATE_OUT_OF_MEMORY)
        show_outside_change(self);
    open_requested(self);
    folio_window_end_dispatch(self);
}

/* Selects the place the request names in the reading view, and says so when it is not the
 * place asked for. */
static void open_link_place(struct folio_window *_Nonnull self,
                            const struct open_request *_Nonnull request)
{
    struct open_request_lines lines = {0, 0};
    bool lined = open_request_lines(request, &lines);
    bool worded = open_request_text_length(request) > 0;
    if (!lined && !worded)
        return;
    struct note_byte_span span = {0, 0};
    enum open_place_kind kind = folio_state_open_place(self->state, request, &span);
    size_t total = 0;
    struct markdown_source_selection paragraphs = {0, 0};
    bool shown =
        kind != OPEN_PLACE_NONE && self->pane != nullptr &&
        note_pane_paragraph_total(self->pane, &total) &&
        folio_state_open_paragraphs(self->state, span, total, &paragraphs) == FOLIO_STATE_READY &&
        note_pane_select_paragraphs(self->pane, paragraphs);
    if (!shown)
        open_link_notice(self, UI_TEXT_OPEN_LINK_NO_PLACE);
    else if (kind == OPEN_PLACE_LINES && worded)
        open_link_notice(self, UI_TEXT_OPEN_LINK_LINES);
    else if (kind == OPEN_PLACE_TEXT_ELSEWHERE && lined)
        open_link_notice(self, UI_TEXT_OPEN_LINK_ELSEWHERE);
}

/* Opens the located note through the one switch (ADR0013 decision 4) and shows it in the
 * reading view. A note that is already open is not selected again, so its Undo stays. */
static enum folio_state_outcome open_link_note(struct folio_window *_Nonnull self,
                                               struct note_ref ref)
{
    /* In the reading view the note is read again even when it is the open one: whoever wrote
     * the link may have just changed the file. An open edit of the same note is not thrown
     * away for the file: it is saved as usual and shown. */
    bool editing =
        holds_document(self, ref) && folio_state_pane_mode(self->state) == PANE_MODE_EDIT;
    enum folio_state_outcome outcome =
        editing ? FOLIO_STATE_READY : switch_note(self, ref.category, ref.note);
    if (outcome == FOLIO_STATE_READY && folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
        outcome = flush_edit(self);
    return outcome;
}

/* The request is held by name; the places are looked up here and used at once (the lists are
 * read again by the lookup, so the drawer is drawn again whatever comes back). */
static void open_link_request(struct folio_window *_Nonnull self,
                              const struct open_request *_Nonnull request)
{
    struct note_ref ref = {0, 0};
    enum folio_state_outcome located = folio_state_locate_open_request(self->state, request, &ref);
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    if (located == FOLIO_STATE_NO_SUCH_CATEGORY || located == FOLIO_STATE_NO_SUCH_NOTE)
    {
        open_link_notice(self, located == FOLIO_STATE_NO_SUCH_NOTE ? UI_TEXT_OPEN_LINK_NO_NOTE
                                                                   : UI_TEXT_OPEN_LINK_NO_CATEGORY);
        return;
    }
    enum folio_state_outcome outcome =
        located == FOLIO_STATE_READY ? open_link_note(self, ref) : located;
    if (outcome != FOLIO_STATE_READY)
    {
        if (outcome != FOLIO_STATE_CANCELLED)
            report_outcome(self, outcome);
        return;
    }
    if (self->notice == PANE_NOTICE_OPEN_LINK)
        set_pane_notice(self, PANE_NOTICE_NONE); /* an earlier link's line; others stay */
    redraw_drawer(self);
    focus_pane(self); /* the pane is laid out and shown before the place is brought into view */
    open_link_place(self, request);
}

/* A dialog of this window keeps the activation; otherwise the window comes forward. */
static void come_forward(const struct folio_window *_Nonnull self)
{
    if (self->native_modal_depth > 0)
        return;
    if (IsIconic(self->handle))
        ShowWindow(self->handle, SW_RESTORE);
    SetForegroundWindow(self->handle);
}

/* #1070 / ADR0119 decision 6: a request file put under data/.requests is taken (and removed) and
 * opened by the same path as a link. Runs only in the quiet turn that reads the data folder
 * again, so nothing holds the window; a file that is no valid request is dropped silently. One
 * file a turn: the next is taken at the next quiet turn. */
static enum request_prompt_choice
show_request_prompt(struct folio_window *_Nonnull self,
                    const struct request_prompt_request *_Nonnull request)
{
    HWND _Nullable focus = GetFocus();
    if (!begin_native_modal(self))
    {
        (void)end_native_modal(self);
        restore_valid_focus(self, focus);
        return REQUEST_PROMPT_UNAVAILABLE;
    }
    enum request_prompt_choice choice =
        request_prompt_show(modal_owner_current(self->modal_owner), request);
    if (!end_native_modal(self))
        native_modal_resource_failure(self);
    restore_valid_focus(self, focus);
    /* 面の間に終了を頼まれていたら、何も選ばれなかったものとして扱う（名前入力面と同じ）。 */
    return self->close_phase == CLOSE_PHASE_REQUESTED ? REQUEST_PROMPT_DECLINE : choice;
}

/* 名前のあるノートを編集していて、保存していない変更がある（#1152）。無題は、離れるときの
 * いつもの面（保存／保存しない／キャンセル）が聞くので、ここでは聞かない。 */
static bool request_needs_answer(const struct folio_window *_Nonnull self)
{
    bool unsaved = false;
    return folio_state_pane_mode(self->state) == PANE_MODE_EDIT &&
           folio_state_document_kind(self->state) == FOLIO_DOCUMENT_NAMED &&
           current_unsaved(self, folio_state_store_revision(self->state), &unsaved) && unsaved;
}

/* 編集中の本文を、番号つきの名前の新しいノートとして残す。残せたら真（そのノートが、変更の無い
 * 状態で開いている）。 */
static bool keep_edit_aside(struct folio_window *_Nonnull self)
{
    const char16_t *units = u"";
    size_t count = 0;
    /* 別名保存と同じ前置き: 途中で止まった改名があれば、先にその面が出る。 */
    enum folio_state_outcome outcome = prepare_name_operation(self);
    if (outcome == FOLIO_STATE_READY)
        outcome = take_text(self, &units, &count);
    if (outcome == FOLIO_STATE_READY)
        outcome = folio_state_keep_aside(self->state, units, count);
    redraw_drawer(self);
    InvalidateRect(self->handle, nullptr, FALSE);
    if (outcome != FOLIO_STATE_READY && outcome != FOLIO_STATE_CANCELLED)
        report_outcome(self, outcome);
    return outcome == FOLIO_STATE_READY;
}

/* 残したノートの名前を 1 行で言う。 */
static void show_kept_line(struct folio_window *_Nonnull self, const char *_Nonnull kept)
{
    struct ui_text_request line = {
        .id = UI_TEXT_REQUEST_KEPT, .language = folio_state_language(self->state), .to = kept};
    if (ui_text_format(&line, self->result_line, sizeof self->result_line) == UI_TEXT_FORMAT_READY)
        show_result_line(self);
}

/* 編集中の本文を残してから、依頼のノートを開く。残した名前は、開いた後に言う: 開く側が言うこと
 * （ノートが無い・箇所が無い）があるときは、そちらを残す。 */
static void keep_and_open(struct folio_window *_Nonnull self,
                          const struct open_request *_Nonnull request)
{
    char kept[name_list_max_length + 1];
    if (!keep_edit_aside(self))
        return;
    /* 残した直後: いま開いているのが、残したノート（面に出した名前と違っていても、実際の名前）。 */
    (void)bounded_text_copy(kept, sizeof kept, folio_state_pane_title(self->state).note);
    open_link_request(self, request);
    if (self->notice != PANE_NOTICE_OPEN_LINK)
        show_kept_line(self, kept);
}

/* 残す名前を kept へ入れる。作れなければ（番号が尽きた・長すぎる・記憶不足）、知らせて偽。 */
static bool request_kept_name(struct folio_window *_Nonnull self,
                              struct text_buffer *_Nullable *_Nonnull kept)
{
    enum folio_state_outcome named = FOLIO_STATE_OUT_OF_MEMORY;
    if (text_buffer_create(kept) == TEXT_BUFFER_ACCEPTED && *kept != nullptr)
        named = folio_state_aside_name(self->state, *kept);
    if (named != FOLIO_STATE_READY)
        report_outcome(self, named); /* 面を出さず、依頼は開かない */
    return named == FOLIO_STATE_READY;
}

/* 編集中で保存していない変更があるときは、どうするかを面で選んでもらってから開く（#1152。hide の
 * 判断 2026-11-22）。リンク（FR-057）は利用者の操作なので、この道を通らない。 */
static void answer_request(struct folio_window *_Nonnull self,
                           const struct open_request *_Nonnull request)
{
    struct text_buffer *_Nullable kept = nullptr;
    if (!request_kept_name(self, &kept) || kept == nullptr)
    {
        text_buffer_destroy(kept);
        return;
    }
    /* 面の間にたまった依頼は、次の静かな回で、いちばん新しい 1 つだけを扱う。 */
    self->request_answered = true;
    self->outside_due = true;
    struct request_prompt_request prompt = {.state = self->state,
                                            .requested = note_name_stem(open_request_note(request)),
                                            .current = folio_state_pane_title(self->state).note,
                                            .kept = text_buffer_bytes(kept)};
    switch (show_request_prompt(self, &prompt))
    {
    case REQUEST_PROMPT_SAVE:
        open_link_request(self, request); /* いつもの道が、保存してから開く */
        break;
    case REQUEST_PROMPT_KEEP:
        keep_and_open(self, request);
        break;
    case REQUEST_PROMPT_DECLINE:
        break; /* 依頼のファイルは、取ったときに消えている */
    case REQUEST_PROMPT_UNAVAILABLE:
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY); /* 面を作れなかった。黙って捨てない */
        break;
    }
    text_buffer_destroy(kept);
}

static void open_requested(struct folio_window *_Nonnull self)
{
    struct open_request *_Nullable request = nullptr;
    bool pending = false;
    bool newest = self->request_answered;
    self->request_answered = false;
    enum folio_state_outcome taken =
        newest ? folio_state_take_last_open_request(self->state, &request, &pending)
               : folio_state_take_open_request(self->state, &request, &pending);
    /* Out of memory part-way: a request already taken (its file is gone) is still handled, and
     * whatever is left is looked at in another quiet turn. */
    self->outside_due = pending || (taken != FOLIO_STATE_READY && request != nullptr);
    if (request == nullptr)
        return;
    come_forward(self);
    if (request_needs_answer(self))
        answer_request(self, request);
    else
        open_link_request(self, request);
    open_request_destroy(request);
}

/* Validates and opens the link the window holds. Runs from the message loop's own dispatch. */
static void open_link_now(struct folio_window *_Nonnull self, const char *_Nonnull bytes,
                          size_t length)
{
    come_forward(self);
    struct open_request *_Nullable request = nullptr;
    enum open_request_outcome read = open_request_create(bytes, length, &request);
    if (read != OPEN_REQUEST_ACCEPTED || request == nullptr)
    {
        open_link_notice(self, UI_TEXT_OPEN_LINK_INVALID);
        return;
    }
    if (open_link_blocked(self))
        open_link_notice(self, UI_TEXT_OPEN_LINK_BUSY);
    else
        open_link_request(self, request);
    open_request_destroy(request);
}

static void open_queued_link(struct folio_window *_Nonnull self)
{
    char *_Nullable link = self->queued_link;
    size_t length = self->queued_length;
    self->queued_link = nullptr;
    self->queued_length = 0;
    if (link == nullptr)
        return;
    open_link_now(self, link, length);
    free(link);
}

void folio_window_open_link(struct folio_window *_Nonnull self, const char *_Nonnull bytes,
                            size_t length)
{
    if (length > folio_open_link_limit)
        return; /* no link is this long; nothing is kept */
    char *_Nullable copy = malloc(length + 1);
    if (copy == nullptr)
        return;
    memcpy(copy, bytes, length);
    copy[length] = '\0';
    free(self->queued_link);
    self->queued_link = copy;
    self->queued_length = length;
    (void)PostMessageW(self->handle, folio_message_open_link, 0, 0);
}

/* A second start hands its link over (folio_open_link.h). The sender is not trusted: the data
 * folder must be this window's, and the link is validated like any other. The link is only
 * kept here and the sender answered at once; a prompt or a save that opening may need must not
 * keep the sender waiting past its timeout (independent review of #1097). */
static LRESULT on_copy_data(struct folio_window *_Nonnull self, LPARAM lparam)
{
    const COPYDATASTRUCT *copy = (const COPYDATASTRUCT *)lparam;
    if (copy == nullptr || copy->dwData != folio_open_link_data || copy->lpData == nullptr ||
        copy->cbData == 0 || copy->cbData > folio_open_link_limit)
        return FALSE;
    const char *bytes = copy->lpData;
    size_t total = copy->cbData;
    size_t split = 0;
    while (split < total && bytes[split] != '\0')
        split += 1;
    const struct storage_path *own = storage_state_directory(self->storage);
    if (split == total || split != storage_path_length(own) ||
        memcmp(bytes, storage_path_text(own), split) != 0)
        return FALSE;
    folio_window_open_link(self, bytes + split + 1, total - split - 1);
    return TRUE;
}

static LRESULT on_child_request(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    switch (message)
    {
    case folio_message_open_web_link:
        open_web_selection(self);
        return 0;
    case WM_COPYDATA: /* a second start hands over its link (#1071) */
        return on_copy_data(self, lparam);
    case folio_message_open_link:
        open_queued_link(self);
        return 0;
    case folio_message_files_dropped:
        files_dropped(self, (HDROP)lparam, (size_t)wparam);
        return 0;
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
    case folio_message_close_pending:
        return self->close_phase == CLOSE_PHASE_REQUESTED;
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
    cancel_part_press(self);
    if (self->native_modal_depth > 0)
        modal_owner_capture_focus(self->modal_owner);
    LRESULT result = DefWindowProcW(self->handle, WM_SYSCOMMAND, wparam, lparam);
    if (self->handle != nullptr && self->native_modal_depth > 0 && !IsIconic(self->handle))
        modal_owner_restore_focus(self->modal_owner);
    return result;
}

static bool client_pointer_cursor(const struct folio_window *_Nonnull self, WPARAM wparam,
                                  LPARAM lparam)
{
    if (!ui_pointer_client(self->handle, wparam, lparam))
        return false;
    refresh_pointer_cursor(self, ui_pointer_message_point(self->handle));
    return true;
}

static enum ui_text media_phase_text(enum image_phase phase)
{
    switch (phase)
    {
    case IMAGE_PHASE_IDLE:
    case IMAGE_PHASE_READING:
    case IMAGE_PHASE_DECODING:
        return UI_TEXT_IMAGES_READING;
    case IMAGE_PHASE_RESIZING:
        return UI_TEXT_IMAGES_RESIZING;
    case IMAGE_PHASE_DIAGRAM_PARSING:
        return UI_TEXT_DIAGRAM_PARSING;
    case IMAGE_PHASE_DIAGRAM_LAYOUT:
        return UI_TEXT_DIAGRAM_LAYOUT;
    case IMAGE_PHASE_DIAGRAM_DRAWING:
        return UI_TEXT_DIAGRAM_DRAWING;
    }
    return UI_TEXT_EMPTY;
}
static void media_failure(struct folio_window *_Nonnull self, enum image_outcome outcome)
{
    const struct markdown_media *media = folio_state_media(self->state);
    if (media == nullptr || self->image_index >= markdown_media_count(media) ||
        markdown_media_kind(media, self->image_index) == MARKDOWN_MEDIA_IMAGE)
    {
        self->image_failed = true;
        return;
    }
    switch (outcome)
    {
    case IMAGE_READY:
    case IMAGE_PENDING:
    case IMAGE_CANCELLED:
    case IMAGE_SKIPPED:
        break;
    case IMAGE_FAILED:
    case IMAGE_OUT_OF_MEMORY:
        self->diagram_failure = UI_TEXT_DIAGRAM_FAILED;
        break;
    case IMAGE_UNSUPPORTED:
        self->diagram_failure = UI_TEXT_DIAGRAM_UNSUPPORTED;
        break;
    case IMAGE_TOO_LARGE:
        self->diagram_failure = UI_TEXT_DIAGRAM_TOO_LARGE;
        break;
    }
}
static void image_progress(struct folio_window *_Nonnull self)
{
    RECT client;
    GetClientRect(self->handle, &client);
    RECT previous = pane_notice_rect(self, client);
    enum ui_text line = UI_TEXT_EMPTY;
    if (self->image_advisory)
        line = UI_TEXT_IMAGES_ADVISORY;
    if (self->image_failed)
        line = UI_TEXT_IMAGES_FAILED;
    if (self->diagram_failure != UI_TEXT_EMPTY)
        line = self->diagram_failure;
    if (self->image_pending)
        line = media_phase_text(folio_state_image_phase(self->state));
    bool boundary = (self->image_line == UI_TEXT_EMPTY) != (line == UI_TEXT_EMPTY);
    self->image_line = line;
    ++self->image_meter;
    if (boundary)
        arrange_command_contents(self);
    RECT notice = pane_notice_rect(self, client);
    /* Completion removes the row; its previous pixels still need the ordinary paint path. */
    UnionRect(&notice, &previous, &notice);
    InvalidateRect(self->handle, &notice, FALSE);
}
static void receive_image(struct folio_window *_Nonnull self)
{
    struct image_bitmap *_Nullable bitmap = nullptr;
    enum image_outcome outcome = folio_state_poll_image(self->state, self->image_ticket, &bitmap);
    if (outcome == IMAGE_PENDING)
        return;
    if (outcome == IMAGE_READY)
        outcome = note_pane_insert_image(self->pane, self->image_index, bitmap);
    image_bitmap_destroy(bitmap);
    note_pane_finish_image(self->pane, self->image_index);
    self->image_pending = false;
    if (outcome != IMAGE_READY)
        media_failure(self, outcome);
    request_minimap(self, MINIMAP_UPDATE_READY);
}
static bool refresh_image_origin(struct folio_window *_Nonnull self)
{
    if (self->image_origin_revision == folio_state_image_origin_revision(self->state))
        return false;
    size_t start = 0;
    size_t end = 0;
    bool selected = note_pane_selection(self->pane, &start, &end);
    size_t line = note_pane_first_visible_line(self->pane);
    note_outline_hold(self->outline);
    enum folio_state_outcome outcome = render_pane(self);
    if (outcome == FOLIO_STATE_READY)
    {
        if (selected)
            note_pane_restore_selection(self->pane, (struct note_search_span){start, end});
        note_pane_scroll_to_line(self->pane, line);
    }
    restore_outline(self);
    report_outcome(self, outcome);
    return true;
}
static void continue_images(struct folio_window *_Nonnull self)
{
    if (folio_state_pane_mode(self->state) != PANE_MODE_VIEW || IsIconic(self->handle) ||
        !folio_window_accessible_ready(self) || self->command_surface != COMMAND_SURFACE_CLOSED ||
        refresh_image_origin(self))
        return;
    enum image_outcome resized = note_pane_resize_image(self->pane);
    if (resized == IMAGE_PENDING)
        request_minimap(self, MINIMAP_UPDATE_READY);
    if (resized != IMAGE_PENDING && resized != IMAGE_READY)
        self->image_failed = true;
    if (self->image_pending)
        receive_image(self);
    if (!self->image_pending && note_pane_next_image(self->pane, &self->image_index))
    {
        enum image_outcome outcome =
            folio_state_request_image(self->state, self->image_index, &self->image_ticket);
        self->image_pending = outcome == IMAGE_PENDING;
        if (!self->image_pending)
        {
            note_pane_finish_image(self->pane, self->image_index);
            media_failure(self, outcome);
        }
    }
    image_progress(self);
}
void folio_window_data_changed(struct folio_window *_Nonnull self)
{
    if (self->handle == nullptr || self->outside_settling)
        return; /* no window to time it, or already gathering: this notice joins the others */
    /* Without a timer the reading is asked for at once. */
    self->outside_settling = SetTimer(self->handle, outside_timer, outside_settle, nullptr) != 0;
    if (!self->outside_settling)
        self->outside_due = true;
}

bool folio_window_data_settling(const struct folio_window *_Nonnull self)
{
    return self->outside_settling;
}

/* The notices have settled: the data folder is read when the window is quiet. */
static void outside_settled(struct folio_window *_Nonnull self)
{
    KillTimer(self->handle, outside_timer);
    self->outside_settling = false;
    self->outside_due = true;
}

static LRESULT on_minimap_timer(struct folio_window *_Nonnull self, WPARAM timer, LPARAM lparam)
{
    if (timer == outside_timer)
    {
        outside_settled(self);
        return 0;
    }
    if (timer == discovery_motion_timer)
    {
        discovery_motion_tick(self);
        return 0;
    }
    if (timer == image_timer)
    {
        continue_images(self);
        return 0;
    }
    if (timer != minimap_timer)
        return DefWindowProcW(self->handle, WM_TIMER, timer, lparam);
    request_minimap(self, MINIMAP_UPDATE_READY);
    return 0;
}

/* Back from another application, which may have written the data folder. Nothing is read
 * here: the message loop reads when the window is quiet (ADR0119 decisions 1 and 2). The first
 * activation of a start follows no leaving, and the lists were read a moment before it. */
static void application_activation(struct folio_window *_Nonnull self, WPARAM wparam)
{
    if (wparam == FALSE)
        self->outside_away = true;
    else if (self->outside_away)
    {
        self->outside_away = false;
        self->outside_due = true;
    }
}

static LRESULT on_shell_message(struct folio_window *_Nonnull self, UINT message, WPARAM wparam,
                                LPARAM lparam)
{
    switch (message)
    {
    case WM_TIMER:
        return on_minimap_timer(self, wparam, lparam);
    case WM_SETCURSOR:
        if (client_pointer_cursor(self, wparam, lparam))
            return TRUE;
        return DefWindowProcW(self->handle, message, wparam, lparam);
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
    case WM_ACTIVATEAPP:
        application_activation(self, wparam);
        return DefWindowProcW(self->handle, message, wparam, lparam);
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
        discovery_motion_settle(self);
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
        (void)refresh_window_icons(self, HIWORD(wparam));
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
    discovery_motion_sync(self);
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
    case FOLIO_SHORTCUT_KEY_O:
        return 'O';
    case FOLIO_SHORTCUT_KEY_P:
        return 'P';
    case FOLIO_SHORTCUT_KEY_F:
        return 'F';
    case FOLIO_SHORTCUT_KEY_E:
        return 'E';
    case FOLIO_SHORTCUT_KEY_M:
        return 'M';
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

static bool create_main_window(struct folio_window *_Nonnull self, HINSTANCE instance)
{
    UINT dpi = GetDpiForSystem();
    DWORD style =
        WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN;
    /* #891 / ADR0108: the recorded place when the user carries it over and it can still be
     * used; otherwise the initial place and size. */
    struct window_placement saved = storage_state_window(self->storage);
    struct window_area place = {CW_USEDEFAULT, CW_USEDEFAULT, scale(base_width, dpi),
                                scale(base_height, dpi), dpi};
    struct window_fit_request request = {.saved = &saved,
                                         .minimum_width = base_min_width,
                                         .minimum_height = base_min_height,
                                         .header_height = base_common_header_height,
                                         .grip_width = base_window_grip_width};
    bool restored = window_restore_place(request, &place);
    HWND handle = CreateWindowExW(0, class_name, L"NeNe Folio", style, place.x, place.y,
                                  place.width, place.height, nullptr, nullptr, instance, self);
    if (handle == nullptr || !refresh_window_icons(self, GetDpiForWindow(handle)))
        return false;
    if (restored)
        window_restore_settle(handle, &saved);
    decorate(handle, self->palette);
    ShowWindow(handle,
               saved.restore && saved.placed && saved.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    return true;
}

enum folio_window_outcome folio_window_create(struct folio_state *_Nonnull state,
                                              struct storage_state *_Nonnull storage,
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
    self->next_idle_work = FOLIO_IDLE_WORK_MINIMAP;
    if (note_minimap_create(&self->minimap) != NOTE_MINIMAP_READY ||
        note_outline_create(&self->outline) != NOTE_OUTLINE_READY || !create_accelerators(self) ||
        !create_keyboard_accelerators(self))
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
    self->state = state;
    self->storage = storage;
    self->drawer_width = base_drawer_width;
    self->keyboard_target = ACCESSIBLE_TREE;
    if (FAILED(accessibility_host_create(self, &self->accessibility)))
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
    self->palette = folio_palette_from(folio_state_palette(state));
    /* 窓を作る前に GDI+ を起動する。枠なし窓で × が描けないと閉じる入口が見えなくなる
     * （ADR 0013）ので、起動できなければ窓を作らない（ADR 0033 の決定 4）。 */
    if (icon_paint_start(&self->gdiplus_token) != ICON_PAINT_STARTED)
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
    if (!create_main_window(self, instance))
    {
        folio_window_destroy(self);
        return FOLIO_WINDOW_NOT_CREATED;
    }
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

static BYTE pressed_shortcut_modifiers(void)
{
    BYTE modifiers = FVIRTKEY;
    if (GetKeyState(VK_CONTROL) < 0)
        modifiers |= FCONTROL;
    if (GetKeyState(VK_SHIFT) < 0)
        modifiers |= FSHIFT;
    if (GetKeyState(VK_MENU) < 0)
        modifiers |= FALT;
    return modifiers;
}

/* mode/menuは最初のkeydownだけ。繰返しbitはmessageから借り、押下状態を複製しない。 */
/* #908: the note body's own menus (editing, viewing, on a link). */
static bool text_menu_context(enum folio_menu_context context)
{
    switch (context)
    {
    case FOLIO_MENU_OPERATIONS:
    case FOLIO_MENU_LAYOUT:
    case FOLIO_MENU_CATEGORY:
    case FOLIO_MENU_NOTE:
    case FOLIO_MENU_EMPTY:
        return false;
    case FOLIO_MENU_TEXT_EDIT:
    case FOLIO_MENU_TEXT_VIEW:
    case FOLIO_MENU_TEXT_LINK:
        return true;
    }
    return false;
}

static bool repeated_note_shortcut(const MSG *_Nonnull message)
{
    if (message->message != WM_KEYDOWN || (message->lParam & ((LPARAM)1 << 30)) == 0)
        return false;
    BYTE modifiers = pressed_shortcut_modifiers();
    for (size_t at = 0; at < folio_shortcut_count(); ++at)
    {
        const struct folio_shortcut *binding = folio_shortcut_at(at);
        enum folio_shortcut_action action = folio_shortcut_action(binding);
        if ((action == FOLIO_SHORTCUT_ACTION_TOGGLE_MODE ||
             action == FOLIO_SHORTCUT_ACTION_OPERATIONS ||
             action == FOLIO_SHORTCUT_ACTION_CONTEXT_MENU ||
             view_toggle_command(folio_shortcut_command(binding))) &&
            message->wParam == shortcut_virtual_key(folio_shortcut_key(binding)) &&
            modifiers == shortcut_virtual_modifiers(folio_shortcut_modifiers(binding)))
            return true;
    }
    return false;
}

static bool translate_command_shortcut(const struct folio_window *_Nonnull window,
                                       const MSG *_Nonnull message)
{
    if (command_target(window, message->hwnd))
    {
        if (repeated_note_shortcut(message))
            return true;
        MSG translated = *message;
        if (TranslateAcceleratorW(window->handle, window->commands, &translated) != 0)
        {
            return true;
        }
    }
    return false;
}

/* captureはmouseだけなので、元の入力HWNDへ届く鍵/ホイールも同じ取消へ戻す。 */
static bool cancel_drawer_input(const struct folio_window *_Nonnull window,
                                const MSG *_Nonnull message)
{
    if ((window->pressed_part != ACCESSIBLE_SPLITTER &&
         window->pressed_part != ACCESSIBLE_MINIMAP) ||
        !command_target(window, message->hwnd))
        return false;
    if (message->message != WM_KEYDOWN && message->message != WM_MOUSEWHEEL)
        return false;
    SendMessageW(window->handle, WM_CANCELMODE, 0, 0);
    return message->message == WM_KEYDOWN && message->wParam == VK_ESCAPE;
}

/* Focus may still belong to a child while the pointer is over the minimap lane. */
static bool translate_minimap_wheel(const struct folio_window *_Nonnull window,
                                    const MSG *_Nonnull message)
{
    if (message->message != WM_MOUSEWHEEL ||
        !((minimap_available(window) && minimap_wheel_target(window, message->lParam)) ||
          (outline_available(window) && outline_wheel_target(window, message->lParam))))
        return false;
    if (message->hwnd != window->handle && !IsChild(window->handle, message->hwnd))
        return false;
    SendMessageW(window->handle, WM_MOUSEWHEEL, message->wParam, message->lParam);
    return true;
}

static bool translate_search_shortcut(const struct folio_window *_Nonnull window,
                                      const MSG *_Nonnull message)
{
    if (!search_target(window, message->hwnd))
        return false;
    MSG translated = *message;
    return TranslateAcceleratorW(window->handle, window->search_keys, &translated) != 0;
}

/* Consume paragraph-only shortcuts before TranslateMessage can generate a native edit command. */
static bool paragraph_shortcut(const struct folio_window *_Nonnull window,
                               const MSG *_Nonnull message)
{
    if (message->message != WM_KEYDOWN || window->pane == nullptr ||
        message->hwnd != note_pane_handle(window->pane) ||
        folio_state_pane_mode(window->state) != PANE_MODE_EDIT)
        return false;
    return pressed_shortcut_modifiers() == (FVIRTKEY | FCONTROL) &&
           (message->wParam == 'R' || message->wParam == '2' || message->wParam == '5');
}

bool folio_window_translate(const struct folio_window *_Nonnull window, const MSG *_Nonnull message)
{
    if (cancel_drawer_input(window, message))
        return true;
    if (session_blocks_queued_input(window, message))
    {
        return true;
    }
    if (translate_minimap_wheel(window, message))
        return true;
    if (translate_menu_system_key(window, message))
        return true;
    if (translate_keyboard(window, message))
    {
        return true;
    }
    if (translate_search_shortcut(window, message))
        return true;
    if (translate_command_shortcut(window, message))
        return true;
    if (window->pane == nullptr || !command_target(window, message->hwnd))
    {
        return false;
    }
    return note_pane_translate(window->pane, message, folio_state_pane_mode(window->state)) ||
           paragraph_shortcut(window, message);
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
    free(window->queued_link);
    window->queued_link = nullptr;
    /* 取り込みの途中で窓が無くなるとき: 開いたままの一連を残さない（次の窓が BUSY で断られる）。 */
    if (window->adopting)
        folio_state_close_image_adoption(window->state);
    window->adopting = false;
    if (window->handle != nullptr)
    {
        discovery_motion_settle(window);
        clear_session_reason(window);
        DestroyWindow(window->handle);
    }
    if (window->icon_large != nullptr)
        DestroyIcon(window->icon_large);
    if (window->icon_small != nullptr)
        DestroyIcon(window->icon_small);
    release_accelerators(window);
    modal_owner_destroy(window->modal_owner);
    name_prompt_metrics_destroy(window->native_metrics);
    note_pane_destroy(window->pane);
    note_minimap_destroy(window->minimap);
    note_outline_destroy(window->outline);
    drawer_window_destroy(window->drawer);
    if (window->accessibility != nullptr)
    {
        accessibility_host_detach(window->accessibility);
        accessibility_host_release(window->accessibility);
    }
    command_candidates_destroy(window->command_candidates);
    theme_candidates_destroy(window->theme_candidates);
    free(window->move_rows);
    tag_candidates_destroy(window->tag_candidates);
    note_tag_destroy(window->management_source);
    if (window->mono_font != nullptr)
    {
        DeleteObject(window->mono_font);
    }
    if (window->word_font != nullptr)
    {
        DeleteObject(window->word_font);
    }
    if (window->gutter_font != nullptr)
        DeleteObject(window->gutter_font);
    if (window->command_brush != nullptr)
    {
        DeleteObject(window->command_brush);
    }
    /* 子窓の WM_NCDESTROY まで済んだあとに終える（ADR 0033 の決定 4）。
     * 生成の途中で失敗した経路では token が 0 のままなので 1 対 1 が保たれる。 */
    icon_paint_stop(window->gdiplus_token);
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
    case COMMAND_SURFACE_MOVE:
        return UI_TEXT_MOVE_TITLE;
    case COMMAND_SURFACE_ABOUT:
        return UI_TEXT_ABOUT_TITLE;
    case COMMAND_SURFACE_DISCOVERY:
        return UI_TEXT_COMMAND_DISCOVER_NOTES;
    case COMMAND_SURFACE_TAG_EDIT:
        return UI_TEXT_TAG_EDIT_TITLE;
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
        return self->command_surface == COMMAND_SURFACE_TAG_MANAGE ? UI_TEXT_TAG_MANAGE_TITLE
                                                                   : UI_TEXT_TAG_FILTER_TITLE;
    case COMMAND_SURFACE_MENU:
    case COMMAND_SURFACE_HISTORY:
        return UI_TEXT_COMMAND_HISTORY;
    }
    return UI_TEXT_EMPTY;
}

static HRESULT surface_row_count(const struct folio_window *_Nonnull self, size_t *_Nonnull out)
{
    *out = 0;
    switch (self->command_surface)
    {
    case COMMAND_SURFACE_CLOSED:
    case COMMAND_SURFACE_EX:
    case COMMAND_SURFACE_SEARCH:
    case COMMAND_SURFACE_REPLACE:
        return S_OK;
    case COMMAND_SURFACE_MENU:
        *out = folio_menu_action_count(self->menu.context);
        return S_OK;
    case COMMAND_SURFACE_SETTINGS:
        *out = settings_row_count();
        return S_OK;
    case COMMAND_SURFACE_HISTORY:
        *out = folio_state_history_count(self->state);
        return S_OK;
    case COMMAND_SURFACE_ABOUT:
        *out = about_text_count;
        return S_OK;
    case COMMAND_SURFACE_TAG_EDIT:
    case COMMAND_SURFACE_TAG_FILTER:
    case COMMAND_SURFACE_TAG_MANAGE:
        *out = tag_matches_count(self);
        return S_OK;
    case COMMAND_SURFACE_DISCOVERY:
        *out = folio_state_discovery_summary(self->state).shown;
        return S_OK;
    case COMMAND_SURFACE_THEMES:
    {
        HRESULT result = prepare_theme_candidates(self);
        if (result == S_OK)
            *out = theme_matches_count(self);
        return result;
    }
    case COMMAND_SURFACE_MOVE:
        *out = self->move_count;
        return S_OK;
    case COMMAND_SURFACE_PALETTE:
    {
        if (command_keys(self))
            return S_OK;
        HRESULT result = prepare_palette_candidates(self);
        if (result == S_OK)
            *out = palette_matches_count(self);
        return result;
    }
    }
    return S_OK;
}

static bool surface_has_list(const struct folio_window *_Nonnull self)
{
    return (self->command_surface == COMMAND_SURFACE_PALETTE && !command_keys(self)) ||
           framed_surface(self) || self->command_surface == COMMAND_SURFACE_HISTORY ||
           self->command_surface == COMMAND_SURFACE_MENU;
}

static enum accessible_kind surface_row_kind(const struct folio_window *_Nonnull self, size_t at)
{
    if (management_surface(self))
        return self->tag_management_view == TAG_MANAGEMENT_CHOOSE ? ACCESSIBLE_SURFACE_CHOICE
                                                                  : ACCESSIBLE_SURFACE_TEXT;
    if (tag_surface(self))
        return ACCESSIBLE_SURFACE_TOGGLE;
    if (about_surface(self))
        return ACCESSIBLE_SURFACE_TEXT;
    if (self->command_surface != COMMAND_SURFACE_SETTINGS)
    {
        return ACCESSIBLE_SURFACE_CHOICE;
    }
    switch (settings_rows[at].kind)
    {
    case SETTINGS_ROW_HEADING:
        return ACCESSIBLE_SURFACE_TEXT;
    case SETTINGS_ROW_THEME:
    case SETTINGS_ROW_STORAGE:
    case SETTINGS_ROW_ABOUT:
        return ACCESSIBLE_SURFACE_BUTTON;
    case SETTINGS_ROW_LANGUAGE:
        return ACCESSIBLE_SURFACE_LANGUAGE;
    case SETTINGS_ROW_FAVORITES_FIRST:
    case SETTINGS_ROW_MINIMAP:
    case SETTINGS_ROW_MOTION:
    case SETTINGS_ROW_WINDOW:
    case SETTINGS_ROW_NUMBER:
        return ACCESSIBLE_SURFACE_TOGGLE;
    }
    return ACCESSIBLE_SURFACE_TEXT;
}

static bool surface_button_visible(const struct folio_window *_Nonnull self, size_t slot)
{
    switch ((enum accessible_surface_button)slot)
    {

    case ACCESSIBLE_SURFACE_DISCOVERY_OPEN:
    case ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE:
    case ACCESSIBLE_SURFACE_DISCOVERY_CLEAR:
    case ACCESSIBLE_SURFACE_DISCOVERY_STAR:
    case ACCESSIBLE_SURFACE_DISCOVERY_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_EDIT:
        return discovery_button_visible(self, slot);

    case ACCESSIBLE_SURFACE_CLOSE:
        return framed_surface(self) || command_band(self) ||
               self->command_surface == COMMAND_SURFACE_HISTORY ||
               self->command_surface == COMMAND_SURFACE_PALETTE;
    case ACCESSIBLE_SURFACE_BACK:
        return self->command_surface == COMMAND_SURFACE_THEMES || about_surface(self);
    case ACCESSIBLE_SURFACE_ABOUT_REPOSITORY:
    case ACCESSIBLE_SURFACE_ABOUT_RELEASES:
    case ACCESSIBLE_SURFACE_ABOUT_STORE:
    case ACCESSIBLE_SURFACE_ABOUT_ISSUES:
        return about_surface(self);
    case ACCESSIBLE_SURFACE_PREVIOUS:
    case ACCESSIBLE_SURFACE_NEXT:
        return self->command_surface == COMMAND_SURFACE_SEARCH || command_keys(self);
    case ACCESSIBLE_SURFACE_SHOW_HELP:
    case ACCESSIBLE_SURFACE_THEME:
    case ACCESSIBLE_SURFACE_STORAGE:
    case ACCESSIBLE_SURFACE_ABOUT:
        return false;
    case ACCESSIBLE_SURFACE_REPLACE_ONE:
    case ACCESSIBLE_SURFACE_REPLACE_ALL:
        return self->command_surface == COMMAND_SURFACE_REPLACE;
    case ACCESSIBLE_SURFACE_TAG_ADD:
    case ACCESSIBLE_SURFACE_TAG_APPLY:
    case ACCESSIBLE_SURFACE_TAG_CANCEL:
        return self->command_surface == COMMAND_SURFACE_TAG_EDIT;
    case ACCESSIBLE_SURFACE_TAG_ALL:
    case ACCESSIBLE_SURFACE_TAG_ANY:
    case ACCESSIBLE_SURFACE_TAG_CLEAR:
        return self->command_surface == COMMAND_SURFACE_TAG_FILTER;
    case ACCESSIBLE_SURFACE_TAG_PRIMARY:
        return management_surface(self);
    case ACCESSIBLE_SURFACE_TAG_SECONDARY:
        return management_surface(self) && !management_running(self) &&
               (folio_state_tag_management_phase(self->state) != TAG_MANAGEMENT_RESULT ||
                management_retryable(self));
    }
    return false;
}

static HRESULT surface_root_child(const struct folio_window *_Nonnull self, size_t index,
                                  struct accessible_item *_Nonnull out)
{
    const enum accessible_kind kinds[] = {ACCESSIBLE_SURFACE_TABS, ACCESSIBLE_SURFACE_LIST,
                                          ACCESSIBLE_SURFACE_STATUS};
    if (index < sizeof kinds / sizeof kinds[0])
    {
        bool visible[] = {self->command_surface == COMMAND_SURFACE_PALETTE ||
                              discovery_surface(self),
                          surface_has_list(self), true};
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
    size_t slot = kind == ACCESSIBLE_SURFACE_BUTTON ? settings_rows[index].button : index;
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
        return (self->command_surface == COMMAND_SURFACE_PALETTE || discovery_surface(self)) &&
                       index < 2
                   ? S_OK
                   : S_FALSE;
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
        settings_button_item(item))
        return ACCESSIBLE_SURFACE_LIST;
    return ACCESSIBLE_SURFACE;
}

static size_t surface_selection_ordinal(struct accessible_item item)
{
    if (surface_parent(item) == ACCESSIBLE_SURFACE_LIST)
        return item.kind == ACCESSIBLE_SURFACE_BUTTON ? settings_button_index(item.slot)
                                                      : item.slot;
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
        *out = self->command_surface == COMMAND_SURFACE_PALETTE || discovery_surface(self) ? 2 : 0;
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
    if (move_surface(self))
    {
        char text[draw_unit_limit];
        if (!move_row_text(self, slot, text, sizeof text))
            return false;
        (void)wide_units(text, out->name);
        return true;
    }
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
    if (discovery_surface(self))
    {
        char text[draw_unit_limit];
        discovery_text_row(self->state, slot, text, sizeof text);
        (void)wide_units(text, out->name);
        return S_OK;
    }
    if (self->command_surface == COMMAND_SURFACE_MENU)
    {
        menu_accessible(self, slot, out);
        return S_OK;
    }

    if (self->command_surface == COMMAND_SURFACE_HISTORY)
        return surface_history_name(self, slot, out->name);
    HRESULT result = S_OK;
    if (self->command_surface == COMMAND_SURFACE_THEMES)
        result = prepare_theme_candidates(self);
    else if (!move_surface(self))
        result = prepare_palette_candidates(self);
    if (FAILED(result))
        return result;
    return surface_query_name(self, slot, out) ? S_OK : accessibility_node_unavailable;
}

static enum ui_text standard_surface_button_label(const struct folio_window *_Nonnull self,
                                                  size_t slot)
{
    size_t index = settings_button_index(slot);
    if (index < settings_row_count())
        return settings_rows[index].label;
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
                                   UI_TEXT_ACTION_REPLACE_ALL};
    return labels[slot];
}

static enum ui_text surface_button_label(const struct folio_window *_Nonnull self, size_t slot)
{
    if (slot >= ACCESSIBLE_SURFACE_DISCOVERY_OPEN)
        return discovery_button_label(self, slot);
    if (slot == ACCESSIBLE_SURFACE_CLOSE && management_running(self))
        return UI_TEXT_TAG_MANAGE_STOP;
    if (slot >= ACCESSIBLE_SURFACE_TAG_PRIMARY)
        return management_button_label(self, slot);
    if (slot >= ACCESSIBLE_SURFACE_TAG_ADD)
    {
        const enum ui_text tags[] = {UI_TEXT_TAG_ADD, UI_TEXT_TAG_APPLY, UI_TEXT_TAG_CANCEL,
                                     UI_TEXT_TAG_ALL, UI_TEXT_TAG_ANY,   UI_TEXT_TAG_CLEAR};
        return tags[slot - ACCESSIBLE_SURFACE_TAG_ADD];
    }
    if (about_link_index(slot) < about_link_count)
        return about_links[about_link_index(slot)].label;
    return standard_surface_button_label(self, slot);
}

static void surface_empty_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    /* #941: the picker names the note it moves; it is moved without being opened. */
    if (move_surface(self))
    {
        const struct ui_text_request source = {
            .id = UI_TEXT_MOVE_SOURCE,
            .language = folio_state_language(self->state),
            .from = folio_state_category_name(self->state, self->move_source.category),
            .name = folio_state_note_name(self->state, self->move_source)};
        if (ui_text_format(&source, out, replace_status_capacity) != UI_TEXT_FORMAT_READY)
            out[0] = '\0';
        return;
    }
    if (!surface_has_list(self))
        return;
    size_t count = 0;
    if (SUCCEEDED(surface_row_count(self, &count)) && count == 0)
        (void)bounded_text_copy(
            out, replace_status_capacity,
            ui_text_line(UI_TEXT_ACCESSIBLE_NO_RESULTS, folio_state_language(self->state)));
}

static void management_surface_status(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    char line[draw_unit_limit];
    if (self->tag_management_view == TAG_MANAGEMENT_PLAN)
        management_intent_text(self, out, replace_status_capacity);
    if (self->tag_management_view == TAG_MANAGEMENT_NAME)
        management_source_text(self, out, replace_status_capacity);
    if (out[0] != '\0')
        (void)bounded_text_append(out, replace_status_capacity, "\n");
    (void)bounded_text_append(
        out, replace_status_capacity,
        ui_text_line(tag_footer_hint(self), folio_state_language(self->state)));
    management_status(self, line, sizeof line);
    if (out[0] != '\0')
        (void)bounded_text_append(out, replace_status_capacity, "\n");
    (void)bounded_text_append(out, replace_status_capacity, line);
    management_notice(self, line, sizeof line);
    if (line[0] != '\0')
    {
        (void)bounded_text_append(out, replace_status_capacity, "\n");
        (void)bounded_text_append(out, replace_status_capacity, line);
    }
}

static void surface_status_text(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    out[0] = '\0';
    if (discovery_surface(self))
    {
        discovery_status_text(self, out);
        return;
    }
    if (management_surface(self))
    {
        management_surface_status(self, out);
        return;
    }
    if (self->command_failure != FOLIO_STATE_READY)
    {
        failure_status(self, self->command_failure, out);
        return;
    }
    if (tag_surface(self))
    {
        tag_catalog_line(self, out, replace_status_capacity);
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
        (void)bounded_text_copy(
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
        return surface_tab_label(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_TABS && discovery_surface(self))
        return UI_TEXT_DISCOVERY_MODE_LABEL;
    if (item.kind == ACCESSIBLE_SURFACE_TEXT && self->command_surface == COMMAND_SURFACE_PALETTE)
        return command_candidates_group_label(item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON)
        return surface_button_label(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE_LANGUAGE || item.kind == ACCESSIBLE_SURFACE_TOGGLE ||
        item.kind == ACCESSIBLE_SURFACE_TEXT)
        return settings_rows[item.slot].label;
    return surface_title(self);
}

static bool surface_tag_name(const struct folio_window *_Nonnull self, struct accessible_item item,
                             struct accessible_view *_Nonnull out)
{
    if (discovery_surface(self) && item.kind == ACCESSIBLE_SURFACE_BUTTON &&
        item.slot == ACCESSIBLE_SURFACE_DISCOVERY_TAG)
    {
        char text[draw_unit_limit];
        discovery_text_tag(self->state, self->discovery_tag, text, sizeof text);
        (void)wide_units(text, out->name);
        return true;
    }
    if (tag_surface(self) &&
        (item.kind == ACCESSIBLE_SURFACE_TOGGLE ||
         (management_surface(self) &&
          (item.kind == ACCESSIBLE_SURFACE_CHOICE || item.kind == ACCESSIBLE_SURFACE_TEXT))))
    {
        char text[draw_unit_limit];
        tag_row_text(self, item.slot, text, sizeof text);
        (void)wide_units(text, out->name);
        return true;
    }
    return false;
}

/* 文字行は描画と同じ1行、リンクは表示名に加えて開く先のURLをHelpTextへ出す（ADR0097）。 */
static bool surface_about_name(const struct folio_window *_Nonnull self,
                               struct accessible_item item, struct accessible_view *_Nonnull out)
{
    if (!about_surface(self))
        return false;
    if (item.kind == ACCESSIBLE_SURFACE_TEXT)
    {
        char text[draw_unit_limit];
        about_text_line(self, item.slot, text);
        (void)wide_units(text, out->name);
        return true;
    }
    size_t link = about_link_index(item.slot);
    if (item.kind != ACCESSIBLE_SURFACE_BUTTON || link >= about_link_count)
        return false;
    (void)wide_units(ui_text_line(about_links[link].label, folio_state_language(self->state)),
                     out->name);
    (void)wide_units(product_link_uri(about_links[link].link), out->help);
    return true;
}

static HRESULT surface_name(const struct folio_window *_Nonnull self, struct accessible_item item,
                            struct accessible_view *_Nonnull out)
{
    if (surface_tag_name(self, item, out) || surface_about_name(self, item, out))
        return S_OK;
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
    if (self->command_surface == COMMAND_SURFACE_SETTINGS &&
        item.kind == ACCESSIBLE_SURFACE_TOGGLE &&
        settings_rows[item.slot].kind == SETTINGS_ROW_MOTION)
        (void)wide_units(ui_text_line(settings_motion_help(self), language), out->help);
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

/* スクロールする内容のうち、内容矩形に見えている部分。hitとUIAが使う。 */
static RECT settings_visible_rect(const struct folio_window *_Nonnull self, RECT row)
{
    RECT content = settings_content_rect(self);
    RECT visible = {0};
    IntersectRect(&visible, &row, &content);
    return visible;
}

static RECT accessible_surface_button_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    switch ((enum accessible_surface_button)slot)
    {
    case ACCESSIBLE_SURFACE_DISCOVERY_OPEN:
    case ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE:
    case ACCESSIBLE_SURFACE_DISCOVERY_CLEAR:
    case ACCESSIBLE_SURFACE_DISCOVERY_STAR:
    case ACCESSIBLE_SURFACE_DISCOVERY_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_EDIT:
        return discovery_button_rect(self, slot);

    case ACCESSIBLE_SURFACE_CLOSE:
        return command_band(self) ? search_close_rect(self) : settings_close_rect(self);
    case ACCESSIBLE_SURFACE_TAG_ADD:
    case ACCESSIBLE_SURFACE_TAG_APPLY:
    case ACCESSIBLE_SURFACE_TAG_CANCEL:
    case ACCESSIBLE_SURFACE_TAG_ALL:
    case ACCESSIBLE_SURFACE_TAG_ANY:
    case ACCESSIBLE_SURFACE_TAG_CLEAR:
    case ACCESSIBLE_SURFACE_TAG_PRIMARY:
    case ACCESSIBLE_SURFACE_TAG_SECONDARY:
        return tag_button_rect(self, slot);
    case ACCESSIBLE_SURFACE_BACK:
        return theme_back_rect(self);
    case ACCESSIBLE_SURFACE_THEME:
    case ACCESSIBLE_SURFACE_STORAGE:
    case ACCESSIBLE_SURFACE_ABOUT:
        return settings_visible_rect(self, settings_item_rect(self, settings_button_index(slot)));
    case ACCESSIBLE_SURFACE_ABOUT_REPOSITORY:
    case ACCESSIBLE_SURFACE_ABOUT_RELEASES:
    case ACCESSIBLE_SURFACE_ABOUT_STORE:
    case ACCESSIBLE_SURFACE_ABOUT_ISSUES:
        return settings_visible_rect(self, about_link_rect(self, about_link_index(slot)));
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
    if (discovery_surface(self))
        return discovery_row_rect(self, slot);
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
        return command_keys(self) ? (RECT){0} : surface_palette_rect(self, slot, false);
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return menu_row_rect(self, slot);

    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
        return settings_visible_rect(self, settings_item_rect(self, slot));
    if (about_surface(self))
        return settings_visible_rect(self, about_text_rect(self, slot));
    if (slot < self->command_first || slot - self->command_first >= command_visible_rows(self))
        return (RECT){0};
    RECT bounds = command_rows_rect(self);
    int height = scale(choice_surface(self) ? base_theme_row_height : base_command_row_height,
                       GetDpiForWindow(self->handle));
    bounds.top += (int)(slot - self->command_first) * height;
    bounds.bottom = bounds.top + height;
    return bounds;
}

static RECT surface_help_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    if (!command_keys(self) || slot >= (size_t)command_help_rows(self))
        return (RECT){0};
    return visible_help_rect(self, slot);
}

static RECT surface_status_rect(const struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        return discovery_geometry(self).status;
    if (tag_surface(self))
        return tag_footer_rect(self);
    if (command_band(self))
        return self->band.result;
    if (choice_surface(self))
        return theme_footer_rect(self);
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
        return settings_footer_rect(self);
    if (about_surface(self))
        return about_notice_rect(self);
    RECT bounds = {0};
    GetClientRect(self->command_layer, &bounds);
    int height = scale(base_command_status_height, GetDpiForWindow(self->handle));
    bounds.top = bounds.bottom - height;
    return bounds;
}

static RECT surface_list_bounds(const struct folio_window *_Nonnull self)
{
    return self->command_surface == COMMAND_SURFACE_SETTINGS || about_surface(self)
               ? settings_content_rect(self)
               : command_rows_rect(self);
}

static bool surface_row_actionable(const struct folio_window *_Nonnull self, size_t at)
{
    if (about_surface(self) ||
        (management_surface(self) && self->tag_management_view == TAG_MANAGEMENT_PLAN))
        return false;
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
        return settings_row_choice(at);
    if (self->command_surface == COMMAND_SURFACE_MENU)
        return menu_availability(self, at) == FOLIO_STATE_READY;
    if (move_surface(self))
        return !move_row_current(self, at);
    return true;
}

/* 描画/UIAと同じ可視行矩形。見出しと無効menuは操作対象に含めない。 */
static bool surface_row_action_at(const struct folio_window *_Nonnull self, POINT point)
{
    if (!surface_has_list(self) || self->command_composing)
        return false;
    RECT rows = surface_list_bounds(self);
    size_t count = 0;
    if (!PtInRect(&rows, point) || surface_row_count(self, &count) != S_OK)
        return false;
    for (size_t at = 0; at < count; ++at)
    {
        RECT row = surface_row_rect(self, at);
        if (!PtInRect(&row, point))
            continue;
        return surface_row_actionable(self, at);
    }
    return false;
}

static enum ui_pointer_kind surface_cursor(const struct folio_window *_Nonnull self, POINT point)
{
    if (!folio_window_accessible_ready(self) || !IsWindowEnabled(self->command_layer))
        return UI_POINTER_ARROW;
    size_t slot = surface_part_at(self, point);
    if (self->surface_pressed < surface_pointer_count && slot != self->surface_pressed)
        return UI_POINTER_ARROW;
    return slot < surface_pointer_count || surface_row_action_at(self, point) ? UI_POINTER_ACTION
                                                                              : UI_POINTER_ARROW;
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
    out->checklist = tag_surface(self) && !(management_surface(self) &&
                                            self->tag_management_view == TAG_MANAGEMENT_CHOOSE);
    out->selection_required = count != 0 && !out->checklist && !about_surface(self);
    out->scroll = (int)self->command_first;
    if (self->command_surface == COMMAND_SURFACE_SETTINGS || about_surface(self))
    {
        RECT content = settings_content_rect(self);
        out->viewport = content.bottom - content.top;
        out->scroll_limit = (int)settings_content_limit(self);
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
        out->selected = surface_current_tab(self) == item.slot;
    if (item.kind == ACCESSIBLE_SURFACE_LANGUAGE || item.kind == ACCESSIBLE_SURFACE_TOGGLE)
        out->selected = tag_surface(self) ? tag_row_selected(self, item.slot)
                                          : settings_row_current(self, item.slot);
}

static bool tag_surface_row(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    return kind == ACCESSIBLE_SURFACE_TOGGLE ||
           (management_surface(self) &&
            (kind == ACCESSIBLE_SURFACE_CHOICE || kind == ACCESSIBLE_SURFACE_TEXT));
}

static void tag_mode_status(const struct folio_window *_Nonnull self, struct accessible_item item,
                            struct accessible_view *_Nonnull out)
{
    bool mode =
        item.kind == ACCESSIBLE_SURFACE_BUTTON &&
        (item.slot == ACCESSIBLE_SURFACE_TAG_ALL || item.slot == ACCESSIBLE_SURFACE_TAG_ANY);
    if (mode && (item.slot == ACCESSIBLE_SURFACE_TAG_ALL) ==
                    (folio_state_tag_match_mode(self->state) == TAG_MATCH_ALL))
    {
        size_t written = 0;
        (void)utf16_text_fill(
            ui_text_line(UI_TEXT_ACCESSIBLE_CURRENT, folio_state_language(self->state)),
            out->status, sizeof out->status / sizeof out->status[0], &written);
    }
}

static void tag_control_view(const struct folio_window *_Nonnull self, struct accessible_item item,
                             struct accessible_view *_Nonnull out)
{
    bool row = tag_surface_row(self, item.kind);
    out->focusable = row || item.kind == ACCESSIBLE_SURFACE_BUTTON;
    bool target = row ? self->tag_focus == TAG_SURFACE_ROWS && self->command_selection == item.slot
                      : tag_focus_slot(self->tag_focus) == item.slot;
    out->focused = out->focusable && GetFocus() == self->command_layer && target;
    out->enabled = out->enabled && !self->command_composing;
    if (management_surface(self) && item.kind == ACCESSIBLE_SURFACE_BUTTON)
        out->enabled = out->enabled && management_button_enabled(self, item.slot);
    tag_mode_status(self, item, out);
}

static bool surface_status_live(const struct folio_window *_Nonnull self)
{
    if (!discovery_surface(self))
        return !management_running(self);
    enum discovery_phase phase = folio_state_discovery_summary(self->state).phase;
    return phase == DISCOVERY_READY || phase == DISCOVERY_FAILED;
}

/* focusを受けるのはリンク4つと戻るだけ。文字行と×は鍵の巡回に入らない。 */
static void about_control_view(const struct folio_window *_Nonnull self,
                               struct accessible_item item, struct accessible_view *_Nonnull out)
{
    out->focusable =
        item.kind == ACCESSIBLE_SURFACE_BUTTON &&
        (item.slot == ACCESSIBLE_SURFACE_BACK || about_link_index(item.slot) < about_link_count);
    out->focused =
        out->focusable && GetFocus() == self->command_layer && about_focus_slot(self) == item.slot;
}

static void surface_control_view(const struct folio_window *_Nonnull self,
                                 struct accessible_item item, struct accessible_view *_Nonnull out)
{
    out->enabled =
        folio_window_accessible_ready(self) && self->command_surface != COMMAND_SURFACE_CLOSED;
    out->dialog = item.kind == ACCESSIBLE_SURFACE && framed_surface(self);
    out->live = item.kind == ACCESSIBLE_SURFACE_STATUS && surface_status_live(self);
    if (item.kind == ACCESSIBLE_SURFACE && command_keys(self))
    {
        out->focusable = true;
        out->focused = GetFocus() == self->command_layer;
        return;
    }
    surface_selection_view(self, item, out);
    if (discovery_surface(self))
    {
        discovery_control_view(self, item, out);
        return;
    }
    if (tag_surface(self))
    {
        tag_control_view(self, item, out);
        return;
    }
    if (about_surface(self))
    {
        about_control_view(self, item, out);
        return;
    }
    out->focusable = surface_row_focusable(item.kind) || settings_button_item(item);
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

static size_t settings_selected_language(const struct folio_window *_Nonnull self)
{
    for (size_t index = 0; index < settings_row_count(); ++index)
        if (settings_rows[index].kind == SETTINGS_ROW_LANGUAGE && settings_row_current(self, index))
            return index;
    return self->command_selection;
}

static HRESULT surface_selected_row(const struct folio_window *_Nonnull self,
                                    struct accessible_item *_Nonnull out)
{
    if (about_surface(self) ||
        (tag_surface(self) &&
         !(management_surface(self) && self->tag_management_view == TAG_MANAGEMENT_CHOOSE)))
        return S_FALSE;
    size_t at = self->command_selection;
    if (self->command_surface == COMMAND_SURFACE_PALETTE)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_CHOICE, at);
        return surface_valid(self, *out) == S_OK ? S_OK : S_FALSE;
    }
    if (self->command_surface == COMMAND_SURFACE_SETTINGS)
        at = settings_selected_language(self);
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
        *out = surface_item(self, ACCESSIBLE_SURFACE_TAB, surface_current_tab(self));
        return S_OK;
    }
    if (item->kind != ACCESSIBLE_SURFACE_LIST || !surface_has_list(self))
        return S_FALSE;
    return surface_selected_row(self, out);
}

static size_t surface_focused_button(const struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        return discovery_focus_slot(self->discovery_focus);
    if (tag_surface(self))
        return tag_focus_slot(self->tag_focus);
    if (about_surface(self))
        return about_focus_slot(self);
    return SIZE_MAX;
}

static HRESULT surface_focus_content(const struct folio_window *_Nonnull self,
                                     struct accessible_item *_Nonnull out)
{
    if (command_keys(self))
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE, 0);
        return S_OK;
    }
    size_t slot = surface_focused_button(self);
    if (slot != SIZE_MAX)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_BUTTON, slot);
        return surface_valid(self, *out) == S_OK ? S_OK : S_FALSE;
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

static HRESULT surface_focus(const struct folio_window *_Nonnull self,
                             struct accessible_item *_Nonnull out)
{
    if (GetFocus() != self->command_layer || !folio_window_accessible_ready(self) ||
        self->command_surface == COMMAND_SURFACE_CLOSED)
        return S_FALSE;
    if (discovery_surface(self) && self->discovery_focus == DISCOVERY_FOCUS_MODE)
    {
        *out = surface_item(self, ACCESSIBLE_SURFACE_TAB,
                            self->discovery_mode_focus == DISCOVERY_MODE_BODY ? 1 : 0);
        return S_OK;
    }
    return surface_focus_content(self, out);
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

static void surface_tag_button_action(struct folio_window *_Nonnull self, size_t slot)
{
    if (slot == ACCESSIBLE_SURFACE_TAG_ADD)
        tag_add_input(self);
    if (slot == ACCESSIBLE_SURFACE_TAG_PRIMARY)
        management_primary(self);
    if (slot == ACCESSIBLE_SURFACE_TAG_SECONDARY)
        management_secondary(self);
    if (slot == ACCESSIBLE_SURFACE_TAG_APPLY)
        tag_apply(self);
    if (slot == ACCESSIBLE_SURFACE_TAG_ALL)
        tag_filter_mode(self, TAG_MATCH_ALL);
    if (slot == ACCESSIBLE_SURFACE_TAG_ANY)
        tag_filter_mode(self, TAG_MATCH_ANY);
    if (slot == ACCESSIBLE_SURFACE_TAG_CLEAR)
        tag_clear_filters(self);
}

/* 設定の入口行と、その副面の戻る・公式リンク。 */
static void surface_settings_button_action(struct folio_window *_Nonnull self, size_t slot)
{
    size_t link = about_link_index(slot);
    if (slot == ACCESSIBLE_SURFACE_BACK)
    {
        return_to_settings(self);
        return;
    }
    if (link < about_link_count)
    {
        open_about_link(self, link);
        return;
    }
    self->command_selection = settings_button_index(slot);
    adopt_settings_row(self);
}

static HRESULT surface_button_action(struct folio_window *_Nonnull self, size_t slot)
{
    if (discovery_surface(self) && !discovery_button_enabled(self, slot))
        return accessibility_node_disabled;
    if (management_surface(self) && !management_button_enabled(self, slot))
        return accessibility_node_disabled;
    if (tag_surface(self) && self->command_composing)
        return accessibility_node_disabled;
    switch ((enum accessible_surface_button)slot)
    {
    case ACCESSIBLE_SURFACE_DISCOVERY_OPEN:
    case ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE:
    case ACCESSIBLE_SURFACE_DISCOVERY_CLEAR:
    case ACCESSIBLE_SURFACE_DISCOVERY_STAR:
    case ACCESSIBLE_SURFACE_DISCOVERY_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG:
    case ACCESSIBLE_SURFACE_DISCOVERY_EDIT:
        discovery_action(self, slot);
        break;

    case ACCESSIBLE_SURFACE_TAG_ADD:
    case ACCESSIBLE_SURFACE_TAG_PRIMARY:
    case ACCESSIBLE_SURFACE_TAG_SECONDARY:
    case ACCESSIBLE_SURFACE_TAG_APPLY:
    case ACCESSIBLE_SURFACE_TAG_ALL:
    case ACCESSIBLE_SURFACE_TAG_ANY:
    case ACCESSIBLE_SURFACE_TAG_CLEAR:
        surface_tag_button_action(self, slot);
        break;
    case ACCESSIBLE_SURFACE_TAG_CANCEL:
    case ACCESSIBLE_SURFACE_CLOSE:
        close_command_surface(self);
        break;
    case ACCESSIBLE_SURFACE_BACK:
    case ACCESSIBLE_SURFACE_THEME:
    case ACCESSIBLE_SURFACE_STORAGE:
    case ACCESSIBLE_SURFACE_ABOUT:
    case ACCESSIBLE_SURFACE_ABOUT_REPOSITORY:
    case ACCESSIBLE_SURFACE_ABOUT_RELEASES:
    case ACCESSIBLE_SURFACE_ABOUT_STORE:
    case ACCESSIBLE_SURFACE_ABOUT_ISSUES:
        surface_settings_button_action(self, slot);
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
    if (about_surface(self))
    {
        if (item.kind == ACCESSIBLE_SURFACE_TEXT)
            reveal_content_rect(self, about_text_rect(self, item.slot), about_scroll_limit(self));
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

static HRESULT tag_row_action(struct folio_window *_Nonnull self,
                              const struct accessible_request *_Nonnull request)
{
    self->command_selection = request->item.slot;
    self->tag_focus = TAG_SURFACE_ROWS;
    reveal_command_selection(self);
    if (request->action == ACCESSIBLE_ACTION_FOCUS)
        return focus_surface_row(self);
    if (management_surface(self))
    {
        if (self->tag_management_view != TAG_MANAGEMENT_CHOOSE)
            return accessibility_node_invalid;
        if (request->action == ACCESSIBLE_ACTION_INVOKE)
            management_rename(self);
        redraw_command_layer(self);
        return S_OK;
    }
    tag_toggle_row(self);
    return S_OK;
}

static HRESULT surface_row_action(struct folio_window *_Nonnull self,
                                  const struct accessible_request *_Nonnull request)
{
    if (!surface_row_actionable(self, request->item.slot))
        return accessibility_node_disabled;

    struct accessible_item item = request->item;
    if (discovery_surface(self))
    {
        discovery_select(self, item.slot);
        return S_OK;
    }
    if (tag_surface(self))
        return tag_row_action(self, request);
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

static HRESULT discovery_tab_action(struct folio_window *_Nonnull self,
                                    const struct accessible_request *_Nonnull request)
{
    if (self->command_composing)
        return accessibility_node_disabled;
    if (request->action != ACCESSIBLE_ACTION_SELECT && request->action != ACCESSIBLE_ACTION_FOCUS)
        return accessibility_node_invalid;
    if (request->action == ACCESSIBLE_ACTION_SELECT)
        discovery_choose_mode(self, request->item.slot);
    self->discovery_mode_focus =
        request->item.slot == 0 ? DISCOVERY_MODE_NAMES : DISCOVERY_MODE_BODY;
    discovery_set_focus(self, DISCOVERY_FOCUS_MODE);
    return S_OK;
}

static HRESULT surface_tab_action(struct folio_window *_Nonnull self,
                                  const struct accessible_request *_Nonnull request)
{
    if (discovery_surface(self))
        return discovery_tab_action(self, request);
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

static HRESULT tag_button_focus(struct folio_window *_Nonnull self, size_t slot)
{
    const enum tag_surface_focus values[] = {
        TAG_SURFACE_CLOSE,  TAG_SURFACE_ADD,     TAG_SURFACE_APPLY,
        TAG_SURFACE_CANCEL, TAG_SURFACE_ALL,     TAG_SURFACE_ANY,
        TAG_SURFACE_CLEAR,  TAG_SURFACE_PRIMARY, TAG_SURFACE_SECONDARY};
    for (size_t at = 0; at < sizeof values / sizeof values[0]; ++at)
        if (tag_focus_slot(values[at]) == slot)
        {
            self->tag_focus = values[at];
            return focus_surface_row(self);
        }
    return accessibility_node_invalid;
}

static bool tag_button_focus_requested(const struct folio_window *_Nonnull self,
                                       const struct accessible_request *_Nonnull request)
{
    return tag_surface(self) && request->item.kind == ACCESSIBLE_SURFACE_BUTTON &&
           request->action == ACCESSIBLE_ACTION_FOCUS;
}

static bool management_row_focus_requested(const struct folio_window *_Nonnull self,
                                           const struct accessible_request *_Nonnull request)
{
    return management_surface(self) && request->item.kind == ACCESSIBLE_SURFACE_TEXT &&
           request->action == ACCESSIBLE_ACTION_FOCUS;
}

static bool about_button_focus_requested(const struct folio_window *_Nonnull self,
                                         const struct accessible_request *_Nonnull request)
{
    return about_surface(self) && request->item.kind == ACCESSIBLE_SURFACE_BUTTON &&
           request->action == ACCESSIBLE_ACTION_FOCUS;
}

static HRESULT about_button_focus(struct folio_window *_Nonnull self, size_t slot)
{
    size_t link = about_link_index(slot);
    if (link >= about_link_count && slot != ACCESSIBLE_SURFACE_BACK)
        return accessibility_node_invalid;
    self->command_selection = link;
    reveal_command_selection(self);
    return focus_surface_row(self);
}

static HRESULT surface_list_action(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request)
{
    struct accessible_item item = request->item;
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
    bool row = surface_row_focusable(item.kind) || settings_button_item(item);
    if (!row)
        return accessibility_node_invalid;
    return surface_row_action(self, request);
}

static HRESULT surface_action(struct folio_window *_Nonnull self,
                              const struct accessible_request *_Nonnull request)
{
    struct accessible_item item = request->item;
    if (management_row_focus_requested(self, request))
        return tag_row_action(self, request);
    if (discovery_surface(self) && item.kind == ACCESSIBLE_SURFACE_BUTTON &&
        request->action == ACCESSIBLE_ACTION_FOCUS)
        return discovery_button_focus(self, item.slot);
    if (tag_button_focus_requested(self, request))
        return tag_button_focus(self, item.slot);
    if (about_button_focus_requested(self, request))
        return about_button_focus(self, item.slot);
    if (item.kind == ACCESSIBLE_SURFACE)
        return surface_root_action(self, request);
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        return surface_tab_action(self, request);
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON && request->action == ACCESSIBLE_ACTION_INVOKE)
        return surface_button_action(self, item.slot);
    return surface_list_action(self, request);
}

static bool accessible_pane_control(enum accessible_kind kind)
{
    enum accessible_kind parent = accessible_controls[kind].parent;
    return kind == ACCESSIBLE_PANE_TOOLS || parent == ACCESSIBLE_PANE_TOOLS ||
           parent == ACCESSIBLE_MODE_GROUP || parent == ACCESSIBLE_ACTION_GROUP ||
           kind == ACCESSIBLE_BODY || kind == ACCESSIBLE_MINIMAP || kind == ACCESSIBLE_OUTLINE;
}

/* 空状態では操作帯・閲覧/編集・本文を描かず、hit と UIA にも出さない（ADR0060 の決定 13）。 */
static bool pane_control_visible(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_MINIMAP)
        return minimap_visible(self);
    if (kind == ACCESSIBLE_OUTLINE)
        return outline_visible(self);
    if (kind == ACCESSIBLE_TAG_EDIT && !tag_strip_visible(self))
        return false;
    if (kind == ACCESSIBLE_BREADCRUMB && !folio_state_pane_title(self->state).any)
    {
        return false;
    }
    return body_shown(self);
}

static bool notice_visible(const struct folio_window *_Nonnull self)
{
    return self->handle != nullptr;
}

/* 同じheader表示slotに属する静的参照。可視判定は単一geometryに任せる。 */
static bool layout_control(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_LAYOUT_CHOOSER || kind == ACCESSIBLE_LAYOUT_GROUP ||
           accessible_controls[kind].parent == ACCESSIBLE_LAYOUT_GROUP;
}

static bool accessible_visible(const struct folio_window *_Nonnull self, enum accessible_kind kind)
{
    if (kind == ACCESSIBLE_SPLITTER)
    {
        RECT bounds = regions_for(self).splitter;
        return !IsRectEmpty(&bounds);
    }
    if (layout_control(kind))
        return layout_metrics_for(self).chooser == (kind == ACCESSIBLE_LAYOUT_CHOOSER);
    if (kind == ACCESSIBLE_NOTICE)
        return notice_visible(self);
    if (kind == ACCESSIBLE_COMMAND_INPUT || kind == ACCESSIBLE_REPLACE_PATTERN ||
        kind == ACCESSIBLE_REPLACE_TEXT || kind == ACCESSIBLE_DISCOVERY_PREVIEW)
        return IsWindowVisible(accessible_input_window(self, kind));
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
        bounds.bottom = regions_for(self).pane.top +
                        scale(note_tools_height(self), GetDpiForWindow(self->handle));
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
    const struct
    {
        enum accessible_kind kind;
        RECT (*_Nonnull bounds)(const struct folio_window *_Nonnull);
    } parts[] = {{ACCESSIBLE_TAG_EDIT, tag_strip_rect},
                 {ACCESSIBLE_OUTLINE, outline_rect},
                 {ACCESSIBLE_BREADCRUMB, breadcrumb_rect},
                 {ACCESSIBLE_GUIDE, guide_rect}};
    for (size_t at = 0; at < sizeof parts / sizeof parts[0]; ++at)
    {
        if (parts[at].kind == kind)
            return parts[at].bounds(self);
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

/* RangeValueを持つ2つのドラッグ領域。 */
static RECT accessible_range_rect(const struct folio_window *_Nonnull self,
                                  enum accessible_kind kind)
{
    return kind == ACCESSIBLE_MINIMAP ? minimap_rect(self) : regions_for(self).splitter;
}

static RECT accessible_main_rect(const struct folio_window *_Nonnull self,
                                 enum accessible_kind kind)
{
    switch (accessible_controls[kind].region)
    {
    case ACCESSIBLE_MAIN_NONE:
        return (RECT){0};
    case ACCESSIBLE_MAIN_RANGE:
        return accessible_range_rect(self, kind);
    case ACCESSIBLE_MAIN_HEADER:
        return accessible_header_rect(self, kind);
    case ACCESSIBLE_MAIN_GROUP:
        return accessible_main_group_rect(self, kind);
    case ACCESSIBLE_MAIN_PANE:
        return accessible_pane_rect(self, kind);
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
    if (kind == ACCESSIBLE_DISCOVERY_PREVIEW)
        return self->discovery_preview;
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
        kind == ACCESSIBLE_REPLACE_TEXT || kind == ACCESSIBLE_DISCOVERY_PREVIEW ||
        kind == ACCESSIBLE_FILTER)
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

static enum ui_text surface_input_label(const struct folio_window *_Nonnull self)
{
    if (discovery_surface(self))
        return folio_state_discovery_summary(self->state).mode == DISCOVERY_MODE_BODY
                   ? UI_TEXT_DISCOVERY_BODY_INPUT
                   : UI_TEXT_DISCOVERY_INPUT;
    if (self->command_surface == COMMAND_SURFACE_THEMES)
        return UI_TEXT_THEME_FILTER;
    if (move_surface(self))
        return UI_TEXT_MOVE_FILTER;
    if (tag_surface(self))
        return tag_input_label(self);
    return surface_title(self);
}

static const char *_Nonnull accessible_label_line(const struct folio_window *_Nonnull self,
                                                  enum accessible_kind kind)
{
    enum ui_text label =
        kind == ACCESSIBLE_COMMAND_INPUT ? surface_input_label(self) : part_label(self, kind);
    if (kind == ACCESSIBLE_NOTICE)
        label = pane_notice_line(self);
    if (kind == ACCESSIBLE_NOTICE && self->notice == PANE_NOTICE_COMMAND_RESULT)
        return self->result_line;
    return ui_text_line(label, folio_state_language(self->state));
}

static bool tag_entry_name(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                           char16_t *_Nonnull out)
{
    if (kind != ACCESSIBLE_TAG_EDIT && kind != ACCESSIBLE_TAG_FILTER)
        return false;
    char text[ui_text_unit_limit];
    if (kind == ACCESSIBLE_TAG_EDIT)
        tag_strip_text(self, text, sizeof text);
    else
        tag_filter_text_format(self->state, text, sizeof text);
    (void)wide_units(text, out);
    return true;
}

static void accessible_name(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                            char16_t *_Nonnull out)
{
    if (discovery_accessible_name(self, kind, out))
        return;
    if (tag_entry_name(self, kind, out))
        return;
    enum folio_language language = folio_state_language(self->state);
    const char *line = accessible_label_line(self, kind);
    struct pane_title_view title = folio_state_pane_title(self->state);
    char body[1024];
    if (kind == ACCESSIBLE_NOTICE)
    {
        status_text(self, body);
        line = body;
    }
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
    joined[0] = '\0';
    size_t at = 0;
    for (size_t index = 1; index < 4 && at + strlen(lines[index]) + 2 <= sizeof joined; ++index)
    {
        (void)bounded_text_append(joined, sizeof joined, lines[index]);
        at = bounded_text_append(joined, sizeof joined, index < 3 ? "\n" : "");
    }
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

static void accessible_minimap_details(const struct folio_window *_Nonnull self,
                                       struct accessible_view *_Nonnull out)
{
    struct note_minimap_paint paint;
    if (minimap_paint_for(self, &paint))
    {
        out->range_value = paint.view.first;
        out->range_maximum = note_pane_geometry_limit(paint.view);
        out->range_small_change = paint.view.step;
        out->range_large_change = paint.view.visible;
    }
    (void)wide_units(ui_text_line(UI_TEXT_MINIMAP_HELP, folio_state_language(self->state)),
                     out->help);
}

static void accessible_details(const struct folio_window *_Nonnull self, enum accessible_kind kind,
                               struct accessible_view *_Nonnull out)
{
    if (kind == ACCESSIBLE_MINIMAP)
        accessible_minimap_details(self, out);
    if (kind == ACCESSIBLE_SPLITTER)
    {
        struct drawer_width_range range = drawer_width_for(self);
        out->range_value = range.value;
        out->range_minimum = range.minimum;
        out->range_maximum = range.maximum;
        out->range_small_change = base_splitter_small_change;
        out->range_large_change = base_splitter_large_change;
        (void)wide_units(ui_text_line(UI_TEXT_DRAWER_WIDTH_HELP, folio_state_language(self->state)),
                         out->help);
    }
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
    if (kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT)
        return folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_TOGGLE_MODE, language);
    if (kind == ACCESSIBLE_MENU)
        return folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_OPERATIONS, language);
    if (kind == ACCESSIBLE_FILTER)
        return folio_shortcut_action_hint(FOLIO_SHORTCUT_ACTION_FILTER, language);
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

static void accessible_native_bounds(struct folio_window *_Nonnull self, enum accessible_kind kind,
                                     struct accessible_view *_Nonnull out)
{
    HWND window = folio_window_accessible_window(self, kind);
    if (window != nullptr)
    {
        GetWindowRect(window, &out->bounds);
        out->enabled = out->enabled && IsWindowEnabled(window);
    }
}

static void accessible_main_bounds(struct folio_window *_Nonnull self, enum accessible_kind kind,
                                   struct accessible_view *_Nonnull out)
{
    if ((layout_control(kind) || kind == ACCESSIBLE_SPLITTER || kind == ACCESSIBLE_MINIMAP ||
         kind == ACCESSIBLE_OUTLINE) &&
        !accessible_visible(self, kind))
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
        kind == ACCESSIBLE_REPLACE_PATTERN || kind == ACCESSIBLE_REPLACE_TEXT ||
        kind == ACCESSIBLE_DISCOVERY_PREVIEW)
    {
        accessible_native_bounds(self, kind, out);
        return;
    }
    if (kind == ACCESSIBLE_NOTICE)
    {
        RECT client;
        GetClientRect(self->handle, &client);
        out->bounds = pane_notice_rect(self, client);
        out->live = pane_notice_line(self) != UI_TEXT_EMPTY;
    }
    else
        out->bounds = accessible_main_rect(self, kind);
    MapWindowPoints(self->handle, nullptr, (POINT *)&out->bounds, 2);
}

static void accessible_selection_view(struct folio_window *_Nonnull self, enum accessible_kind kind,
                                      struct accessible_view *_Nonnull out)
{
    out->selected = part_selected(self, kind);
    if (kind == ACCESSIBLE_LAYOUT_GROUP || kind == ACCESSIBLE_MODE_GROUP ||
        kind == ACCESSIBLE_TREE || kind == ACCESSIBLE_SCOPE_GROUP)
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
        kind == ACCESSIBLE_REPLACE_TEXT || kind == ACCESSIBLE_DISCOVERY_PREVIEW)
        out->enabled = folio_window_accessible_ready(self);
    out->focusable = part_focusable(self, kind);
}

/* #975: one row of the headings column: the heading's text, level and place in the column.
 * A row of an earlier load, or of a column no longer shown, does not answer. */
static HRESULT outline_item_read(struct folio_window *_Nonnull self, struct accessible_item item,
                                 struct accessible_view *_Nonnull out)
{
    struct note_pane_heading heading = {0};
    if (!outline_visible(self) || note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY ||
        item.revision != note_outline_revision(self->outline) ||
        !note_pane_heading(self->pane, item.slot, &heading))
        return accessibility_node_unavailable;
    struct note_pane_heading_label label = {0};
    if (note_pane_heading_text(self->pane, item.slot, &label) > 0)
        memcpy(out->name, label.units, sizeof label.units);
    out->level = (int)heading.level;
    out->position = (int)item.slot + 1;
    out->set_size = (int)note_pane_heading_count(self->pane);
    out->selected = note_outline_current(self->outline) == item.slot;
    out->enabled = part_enabled(self, ACCESSIBLE_OUTLINE);
    out->focusable = part_focusable(self, ACCESSIBLE_OUTLINE);
    struct accessible_item focus = {.kind = ACCESSIBLE_WINDOW};
    out->focused = folio_window_accessible_focus(self, ACCESSIBLE_WINDOW, &focus) == S_OK &&
                   accessible_item_equal(focus, item);
    struct note_outline_paint paint = outline_place(self);
    out->bounds = note_outline_row_rect(self->outline, &paint, item.slot);
    out->offscreen =
        IsRectEmpty(&out->bounds) || !IsWindowVisible(self->handle) || IsIconic(self->handle);
    if (!IsRectEmpty(&out->bounds))
        MapWindowPoints(self->handle, nullptr, (POINT *)&out->bounds, 2);
    return S_OK;
}

static struct accessible_item outline_item(const struct folio_window *_Nonnull self, size_t row)
{
    return (struct accessible_item){.kind = ACCESSIBLE_OUTLINE_ITEM,
                                    .slot = row,
                                    .revision = note_outline_revision(self->outline)};
}

uint64_t folio_window_accessible_outline_revision(const struct folio_window *_Nonnull self)
{
    if (self->outline == nullptr || self->handle == nullptr)
        return 0;
    return note_outline_revision(self->outline) * 2 + (outline_visible(self) ? 1 : 0);
}

/* The items whose views come from elsewhere than the static table: the surface's and the
 * headings column's rows. */
static bool accessible_read_elsewhere(struct folio_window *_Nonnull self,
                                      struct accessible_item item,
                                      struct accessible_view *_Nonnull out,
                                      HRESULT *_Nonnull result)
{
    if (accessible_item_surface(item))
        *result = surface_read(self, item, out);
    else if (item.kind == ACCESSIBLE_OUTLINE_ITEM)
        *result = outline_item_read(self, item, out);
    else
        return false;
    return true;
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
    HRESULT elsewhere = S_OK;
    if (accessible_read_elsewhere(self, *item, out, &elsewhere))
        return elsewhere;
    accessible_name(self, item->kind, out->name);
    accessible_accelerator(self, item->kind, out->accelerator);
    accessible_details(self, item->kind, out);
    accessible_availability(self, item->kind, out);
    accessible_selection_view(self, item->kind, out);
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
    return S_OK;
}

/* The heading being read is the column's selection; before the first heading there is none. */
static HRESULT outline_selected(const struct folio_window *_Nonnull self,
                                struct accessible_item *_Nonnull out)
{
    size_t current = note_outline_current(self->outline);
    if (!outline_visible(self) || current == note_outline_none)
        return S_FALSE;
    *out = outline_item(self, current);
    return S_OK;
}

static bool accessible_selected_elsewhere(struct folio_window *_Nonnull self,
                                          const struct accessible_item *_Nonnull item,
                                          struct accessible_item *_Nonnull out,
                                          HRESULT *_Nonnull result)
{
    if (accessible_item_surface(*item))
        *result = surface_selected(self, item, out);
    else if (item->kind == ACCESSIBLE_OUTLINE)
        *result = outline_selected(self, out);
    else
        return false;
    return true;
}

HRESULT folio_window_accessible_selected(struct folio_window *_Nonnull self,
                                         const struct accessible_item *_Nonnull item,
                                         struct accessible_item *_Nonnull out)
{
    HRESULT elsewhere = S_OK;
    if (accessible_selected_elsewhere(self, item, out, &elsewhere))
        return elsewhere;
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
    if (item->kind == ACCESSIBLE_SCOPE_GROUP)
    {
        *out = (struct accessible_item){.kind = selected_scope_kind(self)};
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

/* The item the keyboard is on: the index's cursor row, the headings column's cursor row, or the
 * part itself. */
static struct accessible_item keyboard_item(const struct folio_window *_Nonnull self)
{
    if (self->keyboard_target == ACCESSIBLE_TREE)
        return accessible_cursor(self);
    if (self->keyboard_target == ACCESSIBLE_OUTLINE && outline_visible(self) &&
        note_pane_heading_count(self->pane) > 0)
        return outline_item(self, note_outline_cursor(self->outline));
    return (struct accessible_item){.kind = self->keyboard_target};
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
    *out = keyboard_item(self);
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
           kind != ACCESSIBLE_DISCOVERY_PREVIEW && !accessible_item_row(item) &&
           kind != ACCESSIBLE_FILTER && kind != ACCESSIBLE_BODY && !accessible_item_root(item) &&
           kind != ACCESSIBLE_OUTLINE_ITEM;
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

/* The column's rows: the list's first and last children, and each row's neighbours. */
static bool outline_navigation(const struct accessible_item *_Nonnull item,
                               enum accessible_navigation direction)
{
    return item->kind == ACCESSIBLE_OUTLINE_ITEM ||
           (item->kind == ACCESSIBLE_OUTLINE && accessible_edge_direction(direction));
}

/* The row a step reaches: the list's first or last row, or a row's previous or next. None at
 * either end and for a row's own children (it has none). */
static size_t outline_neighbour(const struct accessible_item *_Nonnull item,
                                enum accessible_navigation direction, size_t count)
{
    bool forward = accessible_forward_direction(direction);
    bool edge = accessible_edge_direction(direction);
    if (count == 0 || (item->kind == ACCESSIBLE_OUTLINE_ITEM && edge))
        return note_outline_none;
    if (item->kind == ACCESSIBLE_OUTLINE)
        return forward ? 0 : count - 1;
    if (forward ? item->slot + 1 >= count : item->slot == 0)
        return note_outline_none;
    return forward ? item->slot + 1 : item->slot - 1;
}

static HRESULT outline_navigate(const struct folio_window *_Nonnull self,
                                const struct accessible_item *_Nonnull item,
                                enum accessible_navigation direction,
                                struct accessible_item *_Nonnull out)
{
    size_t count = outline_visible(self) ? note_pane_heading_count(self->pane) : 0;
    if (direction == ACCESSIBLE_NAVIGATION_PARENT)
    {
        *out = (struct accessible_item){.kind = ACCESSIBLE_OUTLINE};
        return S_OK;
    }
    size_t row = outline_neighbour(item, direction, count);
    if (row == note_outline_none)
        return S_FALSE;
    *out = outline_item(self, row);
    return S_OK;
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
    if (outline_navigation(item, direction))
        return outline_navigate(self, item, direction, out);
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
    return kind == ACCESSIBLE_FILTER || kind == ACCESSIBLE_BODY ||
           kind == ACCESSIBLE_COMMAND_INPUT || kind == ACCESSIBLE_REPLACE_PATTERN ||
           kind == ACCESSIBLE_REPLACE_TEXT || kind == ACCESSIBLE_DISCOVERY_PREVIEW;
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

/* The column, or its row, under a point in screen coordinates. */
static bool outline_item_at(struct folio_window *_Nonnull self, POINT screen,
                            struct accessible_item *_Nonnull out)
{
    POINT point = screen;
    RECT bounds = outline_rect(self);
    if (IsRectEmpty(&bounds) || !ScreenToClient(self->handle, &point) || !PtInRect(&bounds, point))
        return false;
    struct note_outline_paint paint = outline_place(self);
    size_t row =
        note_outline_row_at(self->outline, &paint, point, note_pane_heading_count(self->pane));
    *out = row == note_outline_none ? (struct accessible_item){.kind = ACCESSIBLE_OUTLINE}
                                    : outline_item(self, row);
    return true;
}

static HRESULT accessible_control_at(struct folio_window *_Nonnull self, POINT point,
                                     struct accessible_item *_Nonnull out)
{
    if (outline_item_at(self, point, out))
        return S_OK;
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

/* #975: the headings column is one keyboard stop. Entering it puts the cursor on the heading
 * being read (the first row before any); a row UIA focuses puts the cursor on that row. */
static enum accessible_kind focus_target(struct folio_window *_Nonnull self,
                                         struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_OUTLINE_ITEM)
    {
        outline_set_cursor(self, item.slot);
        return ACCESSIBLE_OUTLINE;
    }
    size_t current = note_outline_current(self->outline);
    if (item.kind == ACCESSIBLE_OUTLINE && self->keyboard_target != ACCESSIBLE_OUTLINE)
        outline_set_cursor(self, current == note_outline_none ? 0 : current);
    return item.kind;
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
        self->keyboard_target = focus_target(self, item);
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
                    {ACCESSIBLE_FIND, FOLIO_COMMAND_FIND},
                    {ACCESSIBLE_TAG_EDIT, FOLIO_COMMAND_EDIT_TAGS},
                    {ACCESSIBLE_TAG_FILTER, FOLIO_COMMAND_FILTER_TAGS}};
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

static HRESULT accessible_command_action(struct folio_window *_Nonnull self,
                                         const struct accessible_request *_Nonnull request)
{
    enum folio_command command = FOLIO_COMMAND_HELP;
    if (!accessible_command(request->item.kind, &command))
        return accessibility_node_invalid;
    execute_command(self, command, "");
    if (request->action == ACCESSIBLE_ACTION_SELECT)
    {
        struct accessible_view view = {0};
        HRESULT result = folio_window_accessible_read(self, &request->item, &view);
        return SUCCEEDED(result) && view.selected ? S_OK : accessibility_node_invalid;
    }
    return S_OK;
}

/* #975: a row of the headings column brings its heading to the top of the body. The pointer,
 * UIA's Invoke and Select (and later the keyboard) all end here. */
static HRESULT outline_activate(struct folio_window *_Nonnull self, struct accessible_item item)
{
    if (!outline_available(self) || item.revision != note_outline_revision(self->outline) ||
        !note_pane_show_heading(self->pane, item.slot))
        return accessibility_node_invalid;
    note_outline_choose(self->outline, self->pane, item.slot);
    refresh_gutter(self, true);
    return S_OK;
}

static HRESULT accessible_activate(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request)
{
    if (request->item.kind == ACCESSIBLE_OUTLINE_ITEM)
        return outline_activate(self, request->item);
    if (accessible_controls[request->item.kind].parent == ACCESSIBLE_SCOPE_GROUP)
    {
        set_note_scope(self, request->item.kind == ACCESSIBLE_SCOPE_ALL ? NOTE_SCOPE_ALL
                                                                        : NOTE_SCOPE_FAVORITES);
        return S_OK;
    }
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
    return accessible_command_action(self, request);
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

static HRESULT accessible_set_range(struct folio_window *_Nonnull self,
                                    const struct accessible_request *_Nonnull request)
{
    if (request->item.kind == ACCESSIBLE_MINIMAP)
    {
        struct note_minimap_paint paint;
        if (!minimap_available(self) || !minimap_paint_for(self, &paint) ||
            request->range_value < 0 || request->range_value > note_pane_geometry_limit(paint.view))
            return E_INVALIDARG;
        scroll_minimap(self, request->range_value);
        return S_OK;
    }
    struct drawer_width_range range = drawer_width_for(self);
    if (request->item.kind != ACCESSIBLE_SPLITTER || request->range_value < range.minimum ||
        request->range_value > range.maximum)
        return E_INVALIDARG;
    apply_drawer_width(self, request->range_value);
    return S_OK;
}

static HRESULT accessible_favorite(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request)
{
    if (request->item.kind != ACCESSIBLE_NOTE)
        return accessibility_node_invalid;
    /* The star went through when the list was only read again: UIA is told S_OK, the line
     * is shown as for the pointer (#1069). */
    enum folio_state_outcome outcome = folio_state_ledger_outcome(
        self->state, folio_state_toggle_favorite(self->state, request->item.ref));
    bool reread = outcome == FOLIO_STATE_LEDGER_REFRESHED;
    if (reread)
        report_outcome(self, outcome);
    HRESULT result = reread ? S_OK : accessible_outcome(self, outcome);
    settle_empty_drawer_focus(self);
    redraw_drawer(self);
    accessibility_host_schedule(self->accessibility);
    return result;
}

static HRESULT accessible_dispatch(struct folio_window *_Nonnull self,
                                   const struct accessible_request *_Nonnull request, bool selected)
{
    cancel_part_press(self);
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
    case ACCESSIBLE_ACTION_TOGGLE_FAVORITE:
        return accessible_favorite(self, request);
    case ACCESSIBLE_ACTION_SET_RANGE:
        return accessible_set_range(self, request);
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
    if (kind == ACCESSIBLE_FILTER || kind == ACCESSIBLE_SPLITTER ||
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
        if (item.kind == ACCESSIBLE_TREE)
            return false;
    }
    HRESULT result = accessible_set_focus(self, item);
    accessibility_host_schedule(self->accessibility);
    return SUCCEEDED(result);
}

/* 選択群は Tab で 1 回だけ止まる。群の中の項目は群の先頭の項目を代表にする（ADR0060 の決定 5）。 */
static enum accessible_kind keyboard_slot(enum accessible_kind kind)
{
    if (accessible_controls[kind].parent == ACCESSIBLE_SCOPE_GROUP)
        return ACCESSIBLE_SCOPE_ALL;
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
    if (kind == ACCESSIBLE_SCOPE_ALL)
        return selected_scope_kind(self);
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
        (parent != ACCESSIBLE_LAYOUT_GROUP && parent != ACCESSIBLE_MODE_GROUP &&
         parent != ACCESSIBLE_SCOPE_GROUP))
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
        ACCESSIBLE_NEW_NOTE,   ACCESSIBLE_LAYOUT_DRAWER, ACCESSIBLE_HELP,
        ACCESSIBLE_SETTINGS,   ACCESSIBLE_MINIMIZE,      ACCESSIBLE_MAXIMIZE,
        ACCESSIBLE_CLOSE,      ACCESSIBLE_FILTER,        ACCESSIBLE_SCOPE_ALL,
        ACCESSIBLE_TAG_FILTER, ACCESSIBLE_NEW_CATEGORY,  ACCESSIBLE_TREE,
        ACCESSIBLE_SPLITTER,   ACCESSIBLE_VIEW,          ACCESSIBLE_SAVE,
        ACCESSIBLE_FIND,       ACCESSIBLE_MENU,          ACCESSIBLE_TAG_EDIT,
        ACCESSIBLE_OUTLINE,    ACCESSIBLE_MINIMAP};
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
    if (self->keyboard_target != ACCESSIBLE_TREE && self->keyboard_target != ACCESSIBLE_OUTLINE)
    {
        activate_part(self, self->keyboard_target);
        return;
    }
    struct accessible_item item = keyboard_item(self);
    if (item.kind == ACCESSIBLE_CATEGORY || item.kind == ACCESSIBLE_TREE ||
        item.kind == ACCESSIBLE_OUTLINE)
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

static void settle_tag_strip(struct folio_window *_Nonnull self, uint64_t revision)
{
    if (!unsaved_needs_compare(self, revision))
        return;
    const char16_t *_Nonnull units = u"";
    size_t count = 0;
    enum folio_state_outcome outcome = tag_editor_source(self, &units, &count);
    size_t tags = 0;
    if (outcome == FOLIO_STATE_READY)
        outcome = folio_state_note_tag_count(self->state, units, count, &tags);
    if (outcome == self->tag_strip_outcome && tags == self->tag_strip_count)
        return;
    self->tag_strip_outcome = outcome;
    self->tag_strip_count = tags;
    if (tag_strip_visible(self))
    {
        RECT bounds = tag_strip_rect(self);
        InvalidateRect(self->handle, &bounds, FALSE);
    }
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
    if (saved)
    {
        RECT client;
        GetClientRect(self->handle, &client);
        RECT status = pane_notice_rect(self, client);
        InvalidateRect(self->handle, &status, FALSE);
    }
    settle_tag_strip(self, revision);
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
static bool minimap_idle_ready(const struct folio_window *_Nonnull self)
{
    return note_minimap_pending(self->minimap) && minimap_available(self) &&
           !IsIconic(self->handle);
}

/* #1062 / ADR0120: 本文の欄へ落とされた画像の取り込み。取り込みそのもの（どれを写し、何を書くか）
 * は application の一連で、ここは、始める・入力待ちの回ごとに 1 件進める・落とした位置へ入れる・
 * 1 行を出す、だけを行う。走っているあいだは主窓への入力を止める（落とした位置を動かさない）。 */
static void show_drop_line(struct folio_window *_Nonnull self, struct image_adoption_notice notice,
                           bool announce)
{
    struct ui_text_request request = {.id = notice.id,
                                      .language = folio_state_language(self->state),
                                      .k = notice.k,
                                      .n = notice.n,
                                      .offset = notice.offset};
    if (notice.id == UI_TEXT_EMPTY ||
        ui_text_format(&request, self->result_line, sizeof self->result_line) !=
            UI_TEXT_FORMAT_READY)
        return;
    if (announce)
        show_result_line(self);
    else
        InvalidateRect(self->handle, nullptr, FALSE);
}

static bool drop_accepted_now(const struct folio_window *_Nonnull self)
{
    /* リンクで開く依頼と同じ断り（面・OSの面・終了の途中・変換の途中・ほかの処理の途中・主窓が
     * 無効）に、検索の面も足す。 */
    return self->pane != nullptr && !open_link_blocked(self) &&
           self->command_surface == COMMAND_SURFACE_CLOSED;
}

static void begin_drop(struct folio_window *_Nonnull self, size_t position, size_t lost)
{
    self->adopting = true;
    self->drop_position = position;
    self->drop_lost = lost;
    self->adopting_enabled = IsWindowEnabled(self->handle) != 0;
    EnableWindow(self->handle, FALSE);
    struct image_adoption_summary summary = folio_state_image_adoption_summary(self->state);
    if (summary.pending > 1)
        show_drop_line(self, image_adoption_notice_progress(&summary), true);
}

/* パスにできないものだけが落とされた: 取り込みは始まらないが、読めなかったことは言う。 */
static void report_lost_drop(struct folio_window *_Nonnull self, size_t lost)
{
    struct image_adoption_summary summary = {.dropped = lost, .unreadable = lost, .finished = true};
    if (lost != 0)
        show_drop_line(self, image_adoption_notice_finished(&summary, IMAGE_ADOPTION_LANDED), true);
}

static void files_dropped(struct folio_window *_Nonnull self, HDROP _Nullable drop, size_t position)
{
    struct dropped_files *_Nullable files = nullptr;
    if (!drop_accepted_now(self))
        return;
    if (drop ==
        nullptr) /* #1195: 落とされたが、並びを読めなかった（folio_message_files_dropped） */
    {
        report_lost_drop(self, 1);
        return;
    }
    if (!dropped_files_create(drop, image_adoption_limit, &files) || files == nullptr)
    {
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        return;
    }
    size_t lost = dropped_files_lost(files);
    enum image_adoption_start start = folio_state_begin_image_adoption(
        self->state, dropped_files_paths(files), dropped_files_count(files));
    dropped_files_destroy(files);
    switch (start)
    {
    case IMAGE_ADOPTION_STARTED:
        begin_drop(self, position, lost);
        break;
    case IMAGE_ADOPTION_NOTHING:
        report_lost_drop(self, lost);
        break;
    case IMAGE_ADOPTION_OUT_OF_MEMORY:
        report_outcome(self, FOLIO_STATE_OUT_OF_MEMORY);
        break;
    case IMAGE_ADOPTION_VIEWING:
    case IMAGE_ADOPTION_UNTITLED:
    case IMAGE_ADOPTION_NO_NOTE:
    case IMAGE_ADOPTION_BUSY:
        show_drop_line(self, image_adoption_notice_refused(start), true);
        break;
    }
}

/* ためた Markdown を、落とした位置へ入れる（Undo は 1 回）。 */
static enum image_adoption_landing write_dropped(struct folio_window *_Nonnull self)
{
    const char *_Nonnull text = folio_state_image_adoption_text(self->state);
    struct utf16_text *_Nullable units = nullptr;
    if (!folio_state_image_adoption_lands(self->state))
        return IMAGE_ADOPTION_DISPLACED;
    if (text[0] == '\0')
        return IMAGE_ADOPTION_LANDED;
    if (self->pane == nullptr ||
        utf16_text_create(text, strlen(text), &units) != UTF16_TEXT_CONVERTED || units == nullptr)
        return IMAGE_ADOPTION_UNWRITTEN;
    bool inserted = note_pane_insert(self->pane, self->drop_position, utf16_text_units(units));
    utf16_text_destroy(units);
    return inserted ? IMAGE_ADOPTION_LANDED : IMAGE_ADOPTION_UNWRITTEN;
}

static void finish_drop(struct folio_window *_Nonnull self)
{
    struct image_adoption_summary summary = folio_state_image_adoption_summary(self->state);
    summary.dropped += self->drop_lost;
    summary.unreadable += self->drop_lost;
    /* 入力を戻してから入れる: 入れたことの知らせ（EN_CHANGE）を、止めているあいだの入力として
     * 捨てない。ここまで入力待ちの回の中なので、あいだに利用者の入力は入らない。 */
    self->adopting = false;
    EnableWindow(self->handle, self->adopting_enabled);
    enum image_adoption_landing landing = write_dropped(self);
    folio_state_close_image_adoption(self->state);
    if (self->adopting_enabled && self->pane != nullptr)
        restore_valid_focus(self, note_pane_handle(self->pane));
    show_drop_line(self, image_adoption_notice_finished(&summary, landing), true);
    (void)settle_save_state(self);
}

static void continue_drop(struct folio_window *_Nonnull self)
{
    folio_window_begin_dispatch(self);
    switch (folio_state_continue_image_adoption(self->state))
    {
    case IMAGE_ADOPTION_PROGRESSED:
    {
        struct image_adoption_summary summary = folio_state_image_adoption_summary(self->state);
        show_drop_line(self, image_adoption_notice_progress(&summary), false);
        break;
    }
    case IMAGE_ADOPTION_FINISHED:
    case IMAGE_ADOPTION_IDLE:
        finish_drop(self);
        break;
    }
    folio_window_end_dispatch(self);
}

bool folio_window_idle_work_ready(const struct folio_window *_Nonnull self)
{
    if (self->adopting)
        return true;
    if (outside_refresh_ready(self))
        return true;
    if (discovery_surface(self))
        return folio_state_discovery_work_pending(self->state) && !self->command_composing &&
               folio_window_accessible_ready(self);
    return (management_running(self) || folio_state_index_work_pending(self->state) ||
            minimap_idle_ready(self)) &&
           !self->filter_saving &&
           (self->command_surface == COMMAND_SURFACE_CLOSED || tag_surface(self)) &&
           folio_window_accessible_ready(self);
}

static enum folio_idle_work ready_idle_work(const struct folio_window *_Nonnull self)
{
    if (!minimap_idle_ready(self))
        return FOLIO_IDLE_WORK_INDEX;
    if (!folio_state_index_work_pending(self->state))
        return FOLIO_IDLE_WORK_MINIMAP;
    return self->next_idle_work;
}

static void continue_index_filter(struct folio_window *_Nonnull self)
{
    folio_window_begin_dispatch(self);
    bool was_filtering = folio_state_filtering(self->state);
    enum tag_catalog_status before = folio_state_tag_catalog_status(self->state);
    enum folio_state_outcome outcome = folio_state_continue_index_filter(self->state);
    if (tag_surface(self) && before != folio_state_tag_catalog_status(self->state))
        tag_query_changed(self);
    if (tag_surface(self) && outcome != FOLIO_STATE_READY)
        tag_action_finished(self, outcome);
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

static void continue_tag_management(struct folio_window *_Nonnull self)
{
    folio_window_begin_dispatch(self);
    enum tag_management_phase before = folio_state_tag_management_phase(self->state);
    enum tag_management_step step = folio_state_continue_tag_management(self->state);
    if (step == TAG_MANAGEMENT_CURRENT_CHANGED)
    {
        enum folio_state_outcome reflected = reflect_note(self);
        if (reflected != FOLIO_STATE_READY)
        {
            folio_state_stop_tag_management(self->state);
            self->command_failure = reflected;
        }
    }
    /* Consume publication revisions without a per-file "saved" announcement. */
    (void)settle_save_state(self);
    if (before != folio_state_tag_management_phase(self->state))
    {
        management_refresh(self);
        redraw_drawer(self);
    }
    if (before == folio_state_tag_management_phase(self->state))
    {
        arrange_command_input(self);
        redraw_command_layer(self);
        accessibility_host_schedule(self->accessibility);
    }
    present_pending_surface(self->command_layer);
    folio_window_end_dispatch(self);
}

/* 読込件数以外が同じなら、配置・結果・preview・通知は前回のrefreshのまま使える（#837）。 */
static bool discovery_progress_only(const struct discovery_summary *_Nonnull before,
                                    const struct discovery_summary *_Nonnull after)
{
    return before->phase == after->phase && before->mode == after->mode &&
           before->preview == after->preview && before->notice == after->notice &&
           before->total == after->total && before->shown == after->shown &&
           before->selected == after->selected && before->unreadable == after->unreadable &&
           before->unsupported == after->unsupported && before->revision == after->revision &&
           before->results_revision == after->results_revision;
}

/* 表示する件数が変わるidleだけ、状態行を描き直してUIA更新を予約する（#844）。
 * 一致計算中のようにsummaryが同じidleと、表示の間隔に当たらない件数では描画を起こさない。
 * 演出のidle再開は全refreshと同じく毎回同期する。 */
static void discovery_progress_refresh(struct folio_window *_Nonnull self,
                                       const struct discovery_summary *_Nonnull before,
                                       const struct discovery_summary *_Nonnull after)
{
    bool advanced = before->source_checked != after->source_checked;
    bool resized = before->source_total != after->source_total;
    bool shown = discovery_text_progress_shown(after->source_checked, after->source_total);
    if (resized || (advanced && shown))
    {
        RECT status = discovery_geometry(self).status;
        InvalidateRect(self->command_layer, &status, FALSE);
        accessibility_host_schedule(self->accessibility);
    }
    discovery_motion_sync(self);
}

static void continue_discovery(struct folio_window *_Nonnull self)
{
    folio_window_begin_dispatch(self);
    struct discovery_summary before = folio_state_discovery_summary(self->state);
    enum folio_state_outcome outcome = folio_state_continue_discovery(self->state);
    struct discovery_summary after = folio_state_discovery_summary(self->state);
    self->discovery_motion_wait = false;
    if (outcome != FOLIO_STATE_READY)
        self->command_failure = outcome;
    if (outcome == FOLIO_STATE_READY && discovery_progress_only(&before, &after))
        discovery_progress_refresh(self, &before, &after);
    else
        discovery_refresh(self);
    present_pending_surface(self->command_layer);
    folio_window_end_dispatch(self);
}

void folio_window_continue_idle_work(struct folio_window *_Nonnull self)
{
    if (self->adopting)
    {
        continue_drop(self);
        return;
    }
    if (outside_refresh_ready(self))
        refresh_outside(self); /* once per return; the turn's own work follows as before */
    if (!folio_window_idle_work_ready(self))
        return;
    if (discovery_surface(self))
    {
        continue_discovery(self);
        return;
    }
    if (management_running(self))
    {
        continue_tag_management(self);
        return;
    }
    switch (ready_idle_work(self))
    {
    case FOLIO_IDLE_WORK_MINIMAP:
        self->next_idle_work = FOLIO_IDLE_WORK_INDEX;
        if (note_minimap_step(self->minimap, self->pane))
            refresh_gutter(self, false);
        break;
    case FOLIO_IDLE_WORK_INDEX:
        self->next_idle_work = FOLIO_IDLE_WORK_MINIMAP;
        continue_index_filter(self);
        break;
    }
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
    case folio_message_pointer_blocked:
        return !folio_window_accessible_ready(self);
    case folio_message_focus_cues:
        return self->focus_cues || self->focus_cues_always;
    case folio_message_filter_focused:
        return self->filter_input != nullptr && GetFocus() == self->filter_input;
    case folio_message_part_pressed:
        return self->pressed_part == (enum accessible_kind)wparam && self->pressed_inside;
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

/* #1012: the source items also need the reading view to agree with the source map, and 「ここを
 * 編集」 a note that can be edited. Reading the pane's text costs a copy of the whole note, so it
 * is done once when the menu opens, not on every paint and hover (review); running the item
 * checks again with the live text. */
static void measure_source_items(struct folio_window *_Nonnull self)
{
    struct markdown_source_selection selection = {0};
    size_t total = 0;
    size_t native = 0;
    bool body_menu =
        self->menu.context == FOLIO_MENU_TEXT_VIEW || self->menu.context == FOLIO_MENU_TEXT_LINK;
    bool read = body_menu && self->pane != nullptr &&
                note_pane_paragraph_selection(self->pane, &selection, &total);
    self->menu_source_copy = read && folio_state_source_mapped(self->state, total);
    self->menu_source_edit =
        self->menu_source_copy &&
        folio_state_source_caret(self->state, selection.first, total, &native) == FOLIO_STATE_READY;
}

static bool source_available(const struct folio_window *_Nonnull self, enum folio_text_edit edit)
{
    return edit == FOLIO_TEXT_EDIT_COPY_MARKDOWN ? self->menu_source_copy : self->menu_source_edit;
}

static enum folio_state_outcome text_availability(const struct folio_window *_Nonnull self,
                                                  enum folio_text_edit edit)
{
    bool source = edit == FOLIO_TEXT_EDIT_EDIT_HERE || edit == FOLIO_TEXT_EDIT_COPY_MARKDOWN;
    bool available = self->pane != nullptr && note_pane_text_edit_available(self->pane, edit) &&
                     (!source || source_available(self, edit));
    return available ? FOLIO_STATE_READY : FOLIO_STATE_CANCELLED;
}

static enum folio_state_outcome menu_availability(const struct folio_window *_Nonnull self,
                                                  size_t index)
{
    if (index >= folio_menu_action_count(self->menu.context))
        return FOLIO_STATE_CANCELLED;
    const struct folio_menu_action *action = menu_action(self, index);
    if (folio_menu_action_kind(action) == FOLIO_MENU_ACTION_RECOLOR_CATEGORY)
        return folio_state_recolor_availability(self->state, self->menu.target.category);
    if (folio_menu_action_kind(action) == FOLIO_MENU_ACTION_TEXT)
        return text_availability(self, folio_menu_action_text_edit(action));
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
    /* #883: a menu can open over the note's text, where the card outline alone is too faint to
     * show where the surface ends; its edge uses the readable secondary ink. */
    SetDCBrushColor(device, self->palette.dialog_secondary);
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

/* #941: the destination picker. The rows are the categories whose name contains the query,
 * in the list's order; the note's own category is shown but cannot be chosen. */
static bool move_rows_prepare(struct folio_window *_Nonnull self)
{
    self->move_count = 0;
    size_t count = folio_state_category_count(self->state);
    if (count > self->move_capacity)
    {
        if (count > SIZE_MAX / sizeof *self->move_rows)
            return false;
        size_t *_Nullable grown = realloc(self->move_rows, count * sizeof *grown);
        if (grown == nullptr)
            return false;
        self->move_rows = grown;
        self->move_capacity = count;
    }
    struct utf8_text *_Nullable query = nullptr;
    if (command_query(self, &query) != UTF8_TEXT_CONVERTED)
        return false;
    for (size_t at = 0; at < count; ++at)
        if (ascii_fold_contains(folio_state_category_name(self->state, at), utf8_text_bytes(query),
                                utf8_text_length(query)))
            self->move_rows[self->move_count++] = at;
    utf8_text_destroy(query);
    return true;
}

static bool move_row_current(const struct folio_window *_Nonnull self, size_t index)
{
    return index < self->move_count && self->move_rows != nullptr &&
           self->move_rows[index] == self->move_source.category;
}

/* The first row that can be chosen, so Enter right after opening or typing moves the note. */
static void move_select_first(struct folio_window *_Nonnull self)
{
    self->command_selection = 0;
    self->command_first = 0;
    while (self->command_selection + 1 < self->move_count &&
           move_row_current(self, self->command_selection))
        ++self->command_selection;
}

static bool move_row_text(const struct folio_window *_Nonnull self, size_t index,
                          char *_Nonnull out, size_t capacity)
{
    if (index >= self->move_count || self->move_rows == nullptr)
        return false;
    const char *name = folio_state_category_name(self->state, self->move_rows[index]);
    const struct ui_text_request request = {
        .id = UI_TEXT_MOVE_CURRENT, .language = folio_state_language(self->state), .name = name};
    if (move_row_current(self, index))
        return ui_text_format(&request, out, capacity) == UI_TEXT_FORMAT_READY;
    return bounded_text_copy(out, capacity, name) == strlen(name);
}

static void move_refresh(struct folio_window *_Nonnull self)
{
    if (!move_rows_prepare(self))
        self->command_failure = FOLIO_STATE_OUT_OF_MEMORY;
    move_select_first(self);
}

static void move_query_changed(struct folio_window *_Nonnull self)
{
    if (self->command_composing)
        return;
    clear_command_status(self);
    move_refresh(self);
    arrange_command_input(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static bool move_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_UP && key != VK_DOWN && key != VK_PRIOR && key != VK_NEXT)
        return false;
    if (self->move_count == 0)
        return true;
    size_t page = command_visible_rows(self);
    size_t step = key == VK_PRIOR || key == VK_NEXT ? page : 1;
    self->command_failure = FOLIO_STATE_READY;
    move_theme_selection(self, key, self->move_count, step > 0 ? step : 1);
    reveal_command_selection(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
    return true;
}

/* The outcomes that refuse before anything changed: the note is where it was, so the picker
 * can stay open with the reason and the same source. Any other failure may come after the note
 * has moved (a ledger that could not be written back), and the source would be stale. */
static bool move_refused(enum folio_state_outcome outcome)
{
    return outcome == FOLIO_STATE_FILTERED || outcome == FOLIO_STATE_ORDERED ||
           outcome == FOLIO_STATE_NAME_TAKEN || outcome == FOLIO_STATE_NO_SUCH_CATEGORY ||
           outcome == FOLIO_STATE_NO_SUCH_NOTE;
}

/* The move itself is the application's one path (the drag uses it too). A refusal stays on the
 * surface as its one line; any other failure closes the picker and is reported as usual. */
static void adopt_move_at(struct folio_window *_Nonnull self, size_t index)
{
    if (self->command_composing || index >= self->move_count || self->move_rows == nullptr ||
        move_row_current(self, index))
        return;
    enum folio_state_outcome moved =
        folio_state_move_note_to_category(self->state, self->move_source, self->move_rows[index]);
    if (move_refused(moved))
    {
        self->command_failure = moved;
        redraw_command_layer(self);
        accessibility_host_schedule(self->accessibility);
        return;
    }
    close_command_surface(self);
    redraw_drawer(self);
    /* The pane's header shows the category of the open note. */
    InvalidateRect(self->handle, nullptr, FALSE);
    if (moved != FOLIO_STATE_READY)
        report_outcome(self, moved);
}

static void click_move_surface(struct folio_window *_Nonnull self, POINT point)
{
    RECT close = settings_close_rect(self);
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
        if (index < self->move_count)
            self->command_selection = index;
        adopt_move_at(self, index);
    }
    if (move_surface(self))
    {
        focus_command_input(self);
        redraw_command_layer(self);
    }
}

static void draw_move_row(const struct folio_window *_Nonnull self, HDC device, RECT box,
                          size_t index)
{
    char text[draw_unit_limit];
    if (!move_row_text(self, index, text, sizeof text))
        return;
    UINT dpi = GetDpiForWindow(self->handle);
    bool current = move_row_current(self, index);
    struct ui_part_paint part = {.bounds = box,
                                 .dpi = dpi,
                                 .style = UI_PART_STYLE_COMMAND,
                                 .focus_inside = true,
                                 .selected = current,
                                 .focused = index == self->command_selection};
    ui_part_draw(device, &self->palette, &part);
    if (current)
        icon_paint_fill(device, settings_mark_rect(self, box), ICON_PAINT_SELECTION,
                        self->palette.current_text);
    RECT label = {box.left + scale(32, dpi), box.top + scale(2, dpi), box.right - scale(6, dpi),
                  box.bottom};
    SetTextColor(device, current ? self->palette.dialog_secondary : self->palette.current_text);
    draw_units(device, text, label, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

static void draw_move_rows(const struct folio_window *_Nonnull self, HDC device)
{
    RECT bounds = theme_rows_rect(self);
    int height = scale(base_theme_row_height, GetDpiForWindow(self->handle));
    size_t visible = command_visible_rows(self);
    for (size_t offset = 0; offset < visible && self->command_first + offset < self->move_count;
         ++offset)
    {
        RECT row = {bounds.left, bounds.top + (int)offset * height, bounds.right,
                    bounds.top + ((int)offset + 1) * height};
        draw_move_row(self, device, row, self->command_first + offset);
    }
    if (self->move_count == 0)
    {
        SetTextColor(device, self->palette.current_text);
        draw_units(device, ui_text_line(UI_TEXT_MOVE_EMPTY, folio_state_language(self->state)),
                   bounds, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    }
}

static void draw_move_footer(const struct folio_window *_Nonnull self, HDC device)
{
    RECT line = theme_footer_rect(self);
    enum folio_language language = folio_state_language(self->state);
    char text[ui_text_unit_limit];
    struct ui_text_request request = {.id = UI_TEXT_MOVE_COUNT,
                                      .language = language,
                                      .k = self->move_count,
                                      .n = folio_state_category_count(self->state)};
    /* The note is moved without being opened, so the surface names it. */
    struct ui_text_request source = {
        .id = UI_TEXT_MOVE_SOURCE,
        .language = language,
        .from = folio_state_category_name(self->state, self->move_source.category),
        .name = folio_state_note_name(self->state, self->move_source)};
    SetTextColor(device, self->palette.current_text);
    /* One line, cut with an ellipsis: long names must not push the reason out of the footer. */
    RECT named = line;
    named.bottom = named.top + scale(20, GetDpiForWindow(self->handle));
    if (ui_text_format(&source, text, sizeof text) == UI_TEXT_FORMAT_READY)
        draw_units(device, text, named, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
    line.top = named.bottom;
    if (ui_text_format(&request, text, sizeof text) == UI_TEXT_FORMAT_READY)
        line.top += wrapped_text(device, text, line, true);
    /* A refusal takes the place of the key guidance. Only this surface's own failure: a pending
     * settings notice is not about the move. */
    if (self->command_failure != FOLIO_STATE_READY)
    {
        (void)wrapped_text(device, folio_state_failure_line(self->command_failure, language), line,
                           true);
        return;
    }
    SetTextColor(device, self->palette.dialog_secondary);
    (void)wrapped_text(device, ui_text_line(UI_TEXT_MOVE_GUIDANCE, language), line, true);
}

static void draw_move_dialog(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    enum folio_language language = folio_state_language(self->state);
    UINT dpi = GetDpiForWindow(self->handle);
    ui_part_draw_surface(device, &self->palette, bounds);
    RECT title = surface_frame_rect(self, UI_PART_SURFACE_TITLE_TEXT);
    SetTextColor(device, self->palette.current_text);
    draw_units(device, ui_text_line(UI_TEXT_MOVE_TITLE, language), title,
               DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_surface_button(self, device, ACCESSIBLE_SURFACE_CLOSE);
    RECT input = theme_input_rect(self);
    RECT label = {input.left, input.top - scale(20, dpi), input.right, input.top};
    draw_units(device, ui_text_line(UI_TEXT_MOVE_FILTER, language), label,
               DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    draw_input_field(self, device, self->command_input, command_input_rect(self));
    draw_move_rows(self, device);
    draw_move_footer(self, device);
}

static void show_move_surface(struct folio_window *_Nonnull self, struct note_ref source)
{
    if (self->removal_running || session_busy(self) || pane_streaming(self))
        return;
    if (self->command_surface != COMMAND_SURFACE_CLOSED && !hide_command_surface(self))
        return;
    self->move_source = source;
    open_command_surface(self, COMMAND_SURFACE_MOVE);
    initialize_command_input(self, L"");
    move_refresh(self);
    rearrange_keeping_line(self);
    focus_command_input(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

/* The command from the palette: the open, named note. */
static void execute_move_note(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    struct note_ref held = {.category = 0, .note = 0};
    enum folio_document_kind kind = folio_state_document_kind(self->state);
    if (kind != FOLIO_DOCUMENT_NAMED || !folio_state_selection(self->state, &held))
    {
        report_outcome(self, kind == FOLIO_DOCUMENT_UNTITLED ? FOLIO_STATE_NAME_REQUIRED
                                                             : FOLIO_STATE_NOTHING_SELECTED);
        return;
    }
    show_move_surface(self, held);
}

static void show_menu_surface(struct folio_window *_Nonnull self,
                              const struct menu_request *_Nonnull request)
{
    if (self->removal_running || session_busy(self) || pane_streaming(self))
        return;
    if (self->command_surface != COMMAND_SURFACE_CLOSED && !hide_command_surface(self))
        return;
    self->menu = *request;
    measure_source_items(self);
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

static void menu_run_note(struct folio_window *_Nonnull self, enum folio_command command,
                          struct note_ref target)
{
    if (command == FOLIO_COMMAND_RENAME)
        rename_from_drawer(self, target);
    if (command == FOLIO_COMMAND_TOGGLE_FAVORITE)
        favorite_at(self, target);
    if (command == FOLIO_COMMAND_TRASH_NOTE)
        trash_note_at(self, target);
    /* #941: the row's note moves without being opened, as a drag moves it. */
    if (command == FOLIO_COMMAND_MOVE_NOTE)
        show_move_surface(self, target);
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
    menu_run_note(self, command, request->target);
    if (command == FOLIO_COMMAND_NEW)
        new_note_at(self, request->target.category);
    if (command == FOLIO_COMMAND_RENAME_CATEGORY)
        rename_category_at(self, request->target.category, "");
    if (command == FOLIO_COMMAND_DELETE_CATEGORY)
        delete_category_at(self, request->target.category);
    if (command == FOLIO_COMMAND_NEW_CATEGORY)
        execute_new_category_command(self, "");
    if (command == FOLIO_COMMAND_IMPORT)
        import_at(self, request->target.category);
    if (command == FOLIO_COMMAND_EXPORT_CATEGORY)
        export_at(self, NOTE_EXPORT_CATEGORY, request->target.category);
    self->command_context = previous;
    self->reporting_command = previous_command;
}

/* #900: the link's own actions go through the link path and the clipboard port; the rest are
 * the body's edits. */
static void copy_web_selection(struct folio_window *_Nonnull self)
{
    struct utf8_text *_Nullable target = nullptr;
    if (self->pane == nullptr ||
        note_pane_link_selection(self->pane, &target) != NOTE_PANE_LINK_READY)
        return;
    enum folio_state_outcome outcome =
        folio_state_copy_web_link(self->state, utf8_text_bytes(target), utf8_text_length(target));
    utf8_text_destroy(target);
    if (outcome != FOLIO_STATE_READY)
        report_outcome(self, outcome);
}

/* #1012 / ADR0116: 「ここを編集」 opens the editing view with the caret at the clicked paragraph's
 * source, the same order as opening a body match (enter, then select). */
static void edit_here(struct folio_window *_Nonnull self)
{
    struct markdown_source_selection at = {0};
    size_t total = 0;
    size_t native = 0;
    if (!note_pane_paragraph_selection(self->pane, &at, &total))
        return;
    enum folio_state_outcome outcome =
        folio_state_source_caret(self->state, at.first, total, &native);
    if (outcome == FOLIO_STATE_READY)
        outcome = enter_edit(self);
    if (outcome == FOLIO_STATE_READY)
    {
        note_pane_select(self->pane, native, native);
        focus_pane(self);
    }
    else if (outcome != FOLIO_STATE_CANCELLED)
        report_outcome(self, outcome);
}

static void copy_markdown(struct folio_window *_Nonnull self)
{
    struct markdown_source_selection selection = {0};
    size_t total = 0;
    if (!note_pane_paragraph_selection(self->pane, &selection, &total))
        return;
    enum folio_state_outcome outcome = folio_state_copy_markdown(self->state, selection, total);
    if (outcome != FOLIO_STATE_READY)
        report_outcome(self, outcome);
}

static void menu_run_text(struct folio_window *_Nonnull self, enum folio_text_edit edit)
{
    if (self->pane == nullptr)
        return;
    if (edit == FOLIO_TEXT_EDIT_OPEN_LINK)
        open_web_selection(self);
    else if (edit == FOLIO_TEXT_EDIT_COPY_LINK)
        copy_web_selection(self);
    else if (edit == FOLIO_TEXT_EDIT_EDIT_HERE)
        edit_here(self);
    else if (edit == FOLIO_TEXT_EDIT_COPY_MARKDOWN)
        copy_markdown(self);
    else
        note_pane_text_edit(self->pane, edit);
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
    case FOLIO_MENU_ACTION_TEXT:
        menu_run_text(self, folio_menu_action_text_edit(action));
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
    ui_pointer_show(surface_cursor(self, point));
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
    if (folio_menu_action_kind(action) == FOLIO_MENU_ACTION_TEXT)
    {
        menu_hint(self, index, caption);
        (void)wide_units(caption, out->accelerator);
    }
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
    ui_pointer_after(window, message);
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
    ui_pointer_after(window, message);
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
    ui_pointer_after(window, message);
    return result;
}

/* 最外の同期入力dispatchが戻るまで終了要求を実行しない（ADR0064）。 */
static LRESULT CALLBACK window_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct folio_window *self = self_of(window);
    if (self == nullptr)
        return window_message(window, message, wparam, lparam);
    bool stream_change = message == WM_COMMAND && HIWORD(wparam) == EN_CHANGE &&
                         pane_notification_source(self, wparam, lparam) && pane_streaming(self);
    self->dispatch_depth += 1;
    LRESULT result = window_message(window, message, wparam, lparam);
    dispatch_finished(self);
    /* loaded_pane invalidates the finished surface, whose paint refreshes the final pointer. */
    if (!stream_change)
        ui_pointer_after(window, message);
    return result;
}

static size_t discovery_tags_count(const struct folio_window *_Nonnull self)
{
    const struct note_tags *_Nullable tags = folio_state_discovery_tags(self->state);
    return tags == nullptr ? 0 : note_tags_count(tags);
}

static void discovery_condition_geometry(const struct folio_window *_Nonnull self,
                                         struct discovery_layout *_Nonnull layout, RECT area)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int gap = scale(4, dpi);
    layout->clear = (RECT){area.right - scale(148, dpi), area.top, area.right, area.bottom};
    area.right = layout->clear.left - gap;
    if (folio_state_discovery_scope(self->state) == NOTE_SCOPE_FAVORITES)
    {
        layout->star = (RECT){area.left, area.top, area.left + scale(148, dpi), area.bottom};
        area.left = layout->star.right + gap;
    }
    if (discovery_tags_count(self) == 0)
        return;
    if (discovery_tags_count(self) > 1)
    {
        layout->previous_tag = (RECT){area.left, area.top, area.left + scale(28, dpi), area.bottom};
        layout->next_tag = (RECT){area.right - scale(28, dpi), area.top, area.right, area.bottom};
        area.left = layout->previous_tag.right + gap;
        area.right = layout->next_tag.left - gap;
    }
    layout->tag = area;
}

static void discovery_body_geometry(const struct folio_window *_Nonnull self,
                                    struct discovery_layout *_Nonnull layout, RECT body)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT preview = body;
    layout->rows = body;
    if (layout->wide)
    {
        layout->rows.right = body.left + (body.right - body.left) * 2 / 5;
        preview.left = layout->rows.right + scale(12, dpi);
    }
    if (!layout->wide && self->discovery_show_preview)
        layout->rows = (RECT){0};
    if (!layout->wide && !self->discovery_show_preview)
        return;
    layout->preview_title = preview;
    layout->preview_title.bottom = preview.top + scale(20, dpi);
    layout->preview_status = preview;
    layout->preview_status.top = layout->preview_title.bottom;
    layout->preview_status.bottom = layout->preview_status.top + scale(20, dpi);
    if (layout->compact)
    {
        layout->preview_status = layout->preview_title;
        layout->preview_status.left = preview.right - scale(180, dpi);
        layout->preview_title.right = layout->preview_status.left - scale(8, dpi);
    }
    layout->preview = preview;
    layout->preview.top = layout->preview_status.bottom;
    if (layout->preview.bottom < layout->preview.top)
        layout->preview.bottom = layout->preview.top;
}

/* #940: the bottom row holds "show source" (left), "open in the editing view" and the default
 * "open" (right). When the surface is narrower than the three at full width (the list-only
 * window), each keeps its share of the row, so none covers another. */
static void discovery_action_geometry(struct discovery_layout *_Nonnull layout, RECT area, UINT dpi)
{
    int gap = scale(8, dpi);
    int room = area.right - area.left - gap * 2;
    int full = scale(120 + 176 + 160, dpi);
    int fit = room < full ? room : full;
    if (fit < 0)
        fit = 0;
    int top = area.bottom - scale(28, dpi);
    int open = MulDiv(fit, 160, 456);
    int edit = MulDiv(fit, 176, 456);
    int toggle = MulDiv(fit, 120, 456);
    layout->open = (RECT){area.right - open, top, area.right, area.bottom};
    layout->edit =
        (RECT){layout->open.left - gap - edit, top, layout->open.left - gap, area.bottom};
    layout->toggle = (RECT){area.left, top, area.left + toggle, area.bottom};
}

static struct discovery_layout discovery_geometry(const struct folio_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT area = surface_frame_rect(self, UI_PART_SURFACE_CONTENT);
    struct discovery_layout layout = {.wide = area.right - area.left >= scale(720, dpi),
                                      .compact = area.bottom - area.top < scale(260, dpi)};
    layout.input = (RECT){area.left, area.top, area.right, area.top + scale(28, dpi)};
    /* #940: the two choices show their keys (Alt+Left / Alt+Right). */
    layout.body = (RECT){area.right - scale(104, dpi), area.top, area.right, layout.input.bottom};
    layout.names = layout.body;
    OffsetRect(&layout.names, -scale(104, dpi), 0);
    layout.input.right = layout.names.left - scale(8, dpi);
    layout.conditions = (RECT){area.left, layout.input.bottom + scale(4, dpi), area.right,
                               layout.input.bottom + scale(24, dpi)};
    int next = layout.conditions.bottom;
    if (folio_state_discovery_scope(self->state) == NOTE_SCOPE_FAVORITES ||
        discovery_tags_count(self) != 0)
    {
        RECT controls = {area.left, next, area.right, next + scale(28, dpi)};
        discovery_condition_geometry(self, &layout, controls);
        next = controls.bottom + scale(4, dpi);
    }
    layout.status =
        (RECT){area.left, next, area.right, next + scale(layout.compact ? 20 : 32, dpi)};
    discovery_action_geometry(&layout, area, dpi);
    RECT body = {area.left, layout.status.bottom + scale(4, dpi), area.right,
                 layout.open.top - scale(4, dpi)};
    if (body.bottom < body.top)
        body.bottom = body.top;
    discovery_body_geometry(self, &layout, body);
    return layout;
}

static RECT discovery_row_rect(const struct folio_window *_Nonnull self, size_t index)
{
    RECT rows = discovery_geometry(self).rows;
    if (index < self->command_first || IsRectEmpty(&rows))
        return (RECT){0};
    int height = scale(44, GetDpiForWindow(self->handle));
    size_t offset = index - self->command_first;
    if (offset >= (size_t)((rows.bottom - rows.top) / height))
        return (RECT){0};
    rows.top += (int)offset * height;
    rows.bottom = rows.top + height;
    return rows;
}

static int discovery_gutter_width(const struct folio_window *_Nonnull self)
{
    const struct note_excerpt *_Nullable excerpt = folio_state_discovery_excerpt(self->state);
    size_t last = excerpt == nullptr
                      ? 1
                      : note_excerpt_first_line(excerpt) + note_excerpt_line_count(excerpt) - 1;
    wchar_t number[24];
    int length = _snwprintf_s(number, sizeof number / sizeof number[0], _TRUNCATE, L"%zu", last);
    int width = scale(32, GetDpiForWindow(self->handle));
    HDC device = GetDC(self->discovery_preview);
    if (device == nullptr)
        return width;
    HGDIOBJ previous = SelectObject(device, self->mono_font);
    SIZE size = {0};
    if (length > 0 && GetTextExtentPoint32W(device, number, length, &size))
    {
        int measured = size.cx + scale(8, GetDpiForWindow(self->handle));
        if (measured > width)
            width = measured;
    }
    SelectObject(device, previous);
    ReleaseDC(self->discovery_preview, device);
    return width;
}

static bool discovery_preview_has_match(const struct folio_window *_Nonnull self)
{
    DWORD first = 0, last = 0;
    SendMessageW(self->discovery_preview, EM_GETSEL, (WPARAM)&first, (LPARAM)&last);
    return first < last;
}

static void arrange_discovery_preview(struct folio_window *_Nonnull self)
{
    if (self->discovery_preview == nullptr)
        return;
    RECT area = discovery_surface(self) ? discovery_geometry(self).preview : (RECT){0};
    bool shown = !IsRectEmpty(&area) && self->discovery_preview_ready;
    if (shown)
    {
        area.left += discovery_gutter_width(self);
        RECT before = {0};
        GetWindowRect(self->discovery_preview, &before);
        MoveWindow(self->discovery_preview, area.left, area.top, area.right - area.left,
                   area.bottom - area.top, TRUE);
        /* 幅が変わると折り返しが変わるので、一致を見える位置へ寄せ直す（#933）。 */
        if (before.right - before.left != area.right - area.left)
            self->discovery_preview_reveal |= discovery_preview_has_match(self);
    }
    ShowWindow(self->discovery_preview, shown ? SW_SHOW : SW_HIDE);
    if (shown && self->discovery_preview_reveal)
    {
        SendMessageW(self->discovery_preview, EM_SCROLLCARET, 0, 0);
        self->discovery_preview_reveal = false;
    }
}

static void discovery_index_lines(struct folio_window *_Nonnull self,
                                  const struct utf16_text *_Nonnull text)
{
    const char16_t *units = utf16_text_units(text);
    size_t length = utf16_text_length(text);
    self->discovery_line_starts[0] = 0;
    self->discovery_line_count = 1;
    /* WM_SETTEXTは最初のNULで止まるので、番号もそこまでの行にだけ付ける。 */
    for (size_t at = 0;
         at < length && units[at] != u'\0' && self->discovery_line_count < note_excerpt_line_limit;
         ++at)
        if (units[at] == u'\n')
            self->discovery_line_starts[self->discovery_line_count++] = at + 1;
}

/* Native EDIT receives display line endings; the immutable excerpt keeps exact source bytes. */
static enum folio_state_outcome load_discovery_preview(struct folio_window *_Nonnull self)
{
    self->discovery_preview_ready = false;
    self->discovery_preview_reveal = false;
    self->discovery_line_count = 0;
    SetWindowTextW(self->discovery_preview, L"");
    const struct note_excerpt *_Nullable excerpt = folio_state_discovery_excerpt(self->state);
    if (excerpt == nullptr)
    {
        SetWindowTextW(self->discovery_preview, L"");
        return FOLIO_STATE_READY;
    }
    char display[note_excerpt_byte_limit + note_excerpt_line_limit * 2 + 1];
    size_t length = 0;
    size_t count = note_excerpt_line_count(excerpt);
    for (size_t line = 0; line < count; ++line)
    {
        struct note_excerpt_line span = note_excerpt_line(excerpt, line);
        memcpy(display + length, note_excerpt_bytes(excerpt) + span.offset, span.length);
        length += span.length;
        if (line + 1 < count)
        {
            display[length++] = '\r';
            display[length++] = '\n';
        }
    }
    display[length] = '\0';
    struct utf16_text *_Nullable converted = nullptr;
    if (utf16_text_create(display, length, &converted) != UTF16_TEXT_CONVERTED)
        return FOLIO_STATE_OUT_OF_MEMORY;
    bool set = SendMessageW(self->discovery_preview, WM_SETTEXT, 0,
                            (LPARAM)utf16_text_units(converted)) != FALSE;
    self->discovery_preview_ready = set;
    discovery_index_lines(self, converted);
    utf16_text_destroy(converted);
    struct note_search_span selection = note_excerpt_selection(excerpt);
    SendMessageW(self->discovery_preview, EM_SETSEL, (WPARAM)selection.start,
                 (LPARAM)selection.end);
    self->discovery_preview_reveal = set && selection.start < selection.end;
    return set ? FOLIO_STATE_READY : FOLIO_STATE_PANE_LOAD_FAILED;
}

static bool create_discovery_preview(struct folio_window *_Nonnull self)
{
    self->discovery_preview = CreateWindowExW(
        0, edit_class, L"",
        WS_CHILD | ES_MULTILINE | ES_READONLY | ES_NOHIDESEL | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 0,
        0, self->command_layer, (HMENU)(INT_PTR)discovery_preview_control_id,
        GetModuleHandleW(nullptr), nullptr);
    if (self->discovery_preview == nullptr)
        return false;
    SetWindowLongPtrW(self->discovery_preview, GWLP_USERDATA, (LONG_PTR)self);
    self->discovery_preview_original = (WNDPROC)SetWindowLongPtrW(
        self->discovery_preview, GWLP_WNDPROC, (LONG_PTR)discovery_preview_procedure);
    SendMessageW(self->discovery_preview, WM_SETFONT, (WPARAM)self->mono_font, TRUE);
    return self->discovery_preview_original != nullptr;
}

static bool discovery_preview_key(struct folio_window *_Nonnull self, UINT message, WPARAM key)
{
    if (self->command_composing)
        return false;
    if (message == WM_SYSKEYDOWN)
    {
        (void)discovery_switch_mode(self, key);
        return false; /* seen by the default handling too, as in the query */
    }
    if (message == WM_KEYDOWN)
    {
        if (key == VK_TAB || key == VK_ESCAPE || key == VK_RETURN)
            return command_key_down(self, key);
        return shortcut_keydown(self, key);
    }
    return message == WM_CHAR && command_character(self, key);
}

static LRESULT CALLBACK discovery_preview_procedure(HWND window, UINT message, WPARAM wparam,
                                                    LPARAM lparam)
{
    struct folio_window *_Nullable self = self_of(window);
    if (self == nullptr || self->discovery_preview_original == nullptr)
        return DefWindowProcW(window, message, wparam, lparam);
    if (session_blocks_message(self, message) || load_blocks_input(self, message))
        return 0;
    update_command_composition(self, message);
    if (discovery_preview_key(self, message, wparam))
        return 0;
    command_focus_changed(self, message);
    LRESULT result =
        CallWindowProcW(self->discovery_preview_original, window, message, wparam, lparam);
    if (message == WM_VSCROLL || message == WM_MOUSEWHEEL || message == WM_KEYDOWN)
        redraw_command_layer(self);
    return result;
}

static void discovery_refresh(struct folio_window *_Nonnull self)
{
    struct discovery_summary summary = folio_state_discovery_summary(self->state);
    if (self->discovery_results_revision != summary.results_revision)
    {
        self->discovery_results_revision = summary.results_revision;
        invalidate_surface_items(self);
    }
    if (self->discovery_revision != summary.revision)
    {
        self->discovery_revision = summary.revision;
        self->command_selection = summary.selected;
        enum folio_state_outcome loaded = load_discovery_preview(self);
        if (loaded != FOLIO_STATE_READY)
            self->command_failure = loaded;
    }
    size_t count = discovery_tags_count(self);
    if (self->discovery_tag >= count)
        self->discovery_tag = count == 0 ? 0 : count - 1;
    arrange_command_input(self);
    discovery_motion_sync(self);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static void discovery_query_changed(struct folio_window *_Nonnull self)
{
    if (self->command_composing || self->command_initializing)
        return;
    wchar_t units[discovery_input_capacity];
    int count = GetWindowTextW(self->command_input, units, (int)discovery_input_capacity);
    self->command_failure =
        folio_state_query_discovery(self->state, units, count > 0 ? (size_t)count : 0);
    self->command_first = 0;
    self->discovery_show_preview = false;
    discovery_refresh(self);
}

/* Copy only enough native input to prove the application limit; never retain clipboard memory. */
static bool discovery_paste_text(HWND owner, wchar_t *_Nonnull out, size_t *_Nonnull count)
{
    if (!OpenClipboard(owner))
        return false;
    HGLOBAL memory = GetClipboardData(CF_UNICODETEXT);
    const wchar_t *_Nullable text = memory == nullptr ? nullptr : GlobalLock(memory);
    bool available = text != nullptr;
    *count = 0;
    if (available)
    {
        size_t bound = GlobalSize(memory) / sizeof *text;
        while (*count < bound && *count < discovery_input_capacity - 1 && text[*count] != L'\0')
        {
            out[*count] = text[*count];
            ++*count;
        }
        GlobalUnlock(memory);
    }
    out[*count] = L'\0';
    CloseClipboard();
    return available;
}

static void discovery_query_append(wchar_t *_Nonnull out, size_t *_Nonnull used,
                                   const wchar_t *_Nonnull text, size_t count)
{
    size_t room = discovery_input_capacity - 1 - *used;
    size_t take = count < room ? count : room;
    memcpy(out + *used, text, take * sizeof *text);
    *used += take;
}

static enum folio_state_outcome discovery_paste_query(struct folio_window *_Nonnull self,
                                                      const wchar_t *_Nonnull paste, size_t count)
{
    wchar_t current[discovery_input_capacity];
    int length = GetWindowTextW(self->command_input, current, (int)discovery_input_capacity);
    DWORD first = 0, last = 0;
    SendMessageW(self->command_input, EM_GETSEL, (WPARAM)&first, (LPARAM)&last);
    size_t size = length > 0 ? (size_t)length : 0;
    size_t start = first < size ? first : size;
    size_t end = last < size ? last : size;
    wchar_t combined[discovery_input_capacity];
    size_t used = 0;
    discovery_query_append(combined, &used, current, start);
    discovery_query_append(combined, &used, paste, count);
    discovery_query_append(combined, &used, current + end, size - end);
    return folio_state_query_discovery(self->state, combined, used);
}

static void discovery_paste(struct folio_window *_Nonnull self)
{
    if (self->command_composing)
        return;
    wchar_t paste[discovery_input_capacity];
    size_t count = 0;
    if (!discovery_paste_text(self->command_input, paste, &count))
        return;
    self->discovery_show_preview = false;
    self->command_first = 0;
    self->command_failure = discovery_paste_query(self, paste, count);
    if (self->command_failure != FOLIO_STATE_READY)
    {
        discovery_refresh(self);
        return;
    }
    self->command_initializing = true;
    SendMessageW(self->command_input, EM_REPLACESEL, TRUE, (LPARAM)paste);
    self->command_initializing = false;
    discovery_query_changed(self);
}

static void discovery_choose_mode(struct folio_window *_Nonnull self, size_t slot)
{
    if (self->command_composing)
        return;
    enum discovery_mode mode = slot == 0 ? DISCOVERY_MODE_NAMES : DISCOVERY_MODE_BODY;
    self->command_failure = folio_state_set_discovery_mode(self->state, mode);
    self->discovery_mode_focus = mode;
    self->discovery_show_preview = false;
    self->command_first = 0;
    discovery_set_focus(self, DISCOVERY_FOCUS_MODE);
    discovery_refresh(self);
}

static void execute_discovery(struct folio_window *_Nonnull self, const char *_Nonnull argument)
{
    (void)argument;
    if (self->pane != nullptr && note_pane_composing(self->pane))
        return;
    if (!hide_command_surface(self))
        return;
    enum folio_state_outcome opened = save_before_catalog(self, NOTE_SAVE_EDITS);
    if (opened == FOLIO_STATE_READY)
        opened = folio_state_open_discovery(self->state);
    if (opened != FOLIO_STATE_READY)
    {
        report_outcome(self, opened);
        return;
    }
    self->discovery_show_preview = false;
    self->discovery_tag = 0;
    self->discovery_focus = DISCOVERY_FOCUS_ROWS;
    self->discovery_mode_focus = DISCOVERY_MODE_NAMES;
    self->discovery_revision = UINT64_MAX;
    self->discovery_results_revision = UINT64_MAX;
    open_command_surface(self, COMMAND_SURFACE_DISCOVERY);
    discovery_refresh(self);
    discovery_motion_reset(self);
    if (discovery_motion_allowed(self))
        self->discovery_open_frames = discovery_open_frame_count;
    discovery_motion_invalidate(self);
    discovery_motion_sync(self);
}

static bool discovery_holds_target(const struct folio_window *_Nonnull self,
                                   const struct discovery_target *_Nonnull target)
{
    struct note_ref ref = {0};
    return folio_state_resolve_note(self->state, discovery_target_identity(target), &ref) ==
               FOLIO_STATE_READY &&
           holds_document(self, ref);
}

static void discovery_open_failure(struct folio_window *_Nonnull self,
                                   enum folio_state_outcome outcome, bool adopted)
{
    if (adopted || pane_unavailable(self))
    {
        close_command_surface(self);
        report_outcome(self, outcome);
        focus_pane(self);
        return;
    }
    self->command_failure = outcome;
    discovery_refresh(self);
}

static enum folio_state_outcome
discovery_apply_match(struct folio_window *_Nonnull self,
                      const struct discovery_target *_Nonnull target)
{
    if (self->pane == nullptr || note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY)
        return FOLIO_STATE_PANE_LOAD_FAILED;
    struct note_search_span selection = {0};
    switch (folio_state_check_discovery_target(self->state, target, &selection))
    {
    case DISCOVERY_TARGET_CURRENT:
        set_pane_notice(self, PANE_NOTICE_NONE);
        note_pane_select(self->pane, selection.start, selection.end);
        return FOLIO_STATE_READY;
    case DISCOVERY_TARGET_CHANGED:
        set_pane_notice(self, PANE_NOTICE_DISCOVERY_CHANGED);
        return FOLIO_STATE_READY;
    case DISCOVERY_TARGET_UNAVAILABLE:
        return FOLIO_STATE_CANCELLED;
    }
}

static void discovery_open_body(struct folio_window *_Nonnull self, struct note_ref ref)
{
    struct discovery_target *target = nullptr;
    enum folio_state_outcome outcome = folio_state_prepare_discovery_target(self->state, &target);
    if (outcome != FOLIO_STATE_READY)
    {
        discovery_open_failure(self, outcome, false);
        return;
    }
    bool same = holds_document(self, ref);
    if (!same)
        outcome = switch_note(self, ref.category, ref.note);
    bool adopted = !same && discovery_holds_target(self, target);
    bool protected_view =
        adopted && folio_state_select_notice(self->state) == FOLIO_STATE_NOTE_UNEDITABLE;
    if (outcome == FOLIO_STATE_READY && !protected_view)
        outcome = enter_edit(self);
    if (outcome == FOLIO_STATE_READY && !protected_view)
        outcome = discovery_apply_match(self, target);
    discovery_target_destroy(target);
    if (outcome != FOLIO_STATE_READY)
    {
        discovery_open_failure(self, outcome, adopted);
        return;
    }
    enum pane_notice notice = self->notice;
    close_command_surface(self);
    set_pane_notice(self, notice);
    focus_pane(self);
}

/* #940: the display mode the confirmed note opens in. Ctrl+Enter asks for the editing view (a
 * protected note stays in the reading view); Enter on a body match asks for the reading view;
 * Enter on a name keeps the current mode. */
static enum folio_state_outcome discovery_open_mode(struct folio_window *_Nonnull self, bool edit,
                                                    bool body)
{
    if (edit && folio_state_select_notice(self->state) != FOLIO_STATE_NOTE_UNEDITABLE)
        return enter_edit(self);
    if (!edit && body && folio_state_pane_mode(self->state) == PANE_MODE_EDIT)
        return flush_edit(self);
    return FOLIO_STATE_READY;
}

/* #940: the reading view shows rendered text, so the match is found there by the note's own
 * search: the term becomes the in-note search term (n / N go on from it) and the first match
 * from the top is selected. A term that only matches markup leaves the note at its top. */
static void discovery_reveal(struct folio_window *_Nonnull self, const wchar_t *_Nonnull query,
                             size_t count)
{
    if (self->pane == nullptr || folio_state_pane_mode(self->state) != PANE_MODE_VIEW ||
        note_pane_readiness(self->pane) != NOTE_PANE_READINESS_READY ||
        folio_state_set_search_term(self->state, (const char16_t *)query, count) !=
            FOLIO_STATE_READY)
        return;
    folio_state_set_search_direction(self->state, SEARCH_DIRECTION_FORWARD);
    note_pane_select(self->pane, 0, 0);
    update_search(self, SEARCH_DIRECTION_FORWARD, false);
}

/* The note opens through the shared switch; `body` then finds the match in the reading view. */
static void discovery_open_note(struct folio_window *_Nonnull self, struct note_ref ref, bool edit,
                                bool body)
{
    wchar_t query[discovery_input_capacity];
    int count = GetWindowTextW(self->command_input, query, (int)discovery_input_capacity);
    bool same = holds_document(self, ref);
    enum folio_state_outcome outcome =
        same ? FOLIO_STATE_READY : switch_note(self, ref.category, ref.note);
    bool adopted = !same && outcome == FOLIO_STATE_READY;
    if (outcome == FOLIO_STATE_READY)
        outcome = discovery_open_mode(self, edit, body);
    if (outcome != FOLIO_STATE_READY)
    {
        discovery_open_failure(self, outcome, adopted);
        return;
    }
    enum pane_notice notice = self->notice;
    close_command_surface(self);
    set_pane_notice(self, notice);
    if (body)
        discovery_reveal(self, query, count > 0 ? (size_t)count : 0);
    focus_pane(self);
}

/* Enter, or Ctrl+Enter / the editing button when `edit`. */
static void discovery_open_selected(struct folio_window *_Nonnull self, bool edit)
{
    struct discovery_summary summary = folio_state_discovery_summary(self->state);
    if (self->command_composing || summary.phase != DISCOVERY_READY || summary.shown == 0)
        return;
    struct note_ref ref = folio_state_discovery_row(self->state, summary.selected).ref;
    bool body = summary.mode == DISCOVERY_MODE_BODY;
    if (body && edit)
        discovery_open_body(self, ref);
    else
        discovery_open_note(self, ref, edit, body);
}

static size_t discovery_focus_slot(enum discovery_focus focus)
{
    switch (focus)
    {
    case DISCOVERY_FOCUS_INPUT:
    case DISCOVERY_FOCUS_MODE:
    case DISCOVERY_FOCUS_PREVIEW:
    case DISCOVERY_FOCUS_ROWS:
        return SIZE_MAX;
    case DISCOVERY_FOCUS_STAR:
        return ACCESSIBLE_SURFACE_DISCOVERY_STAR;
    case DISCOVERY_FOCUS_PREVIOUS_TAG:
        return ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG;
    case DISCOVERY_FOCUS_TAG:
        return ACCESSIBLE_SURFACE_DISCOVERY_TAG;
    case DISCOVERY_FOCUS_NEXT_TAG:
        return ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG;
    case DISCOVERY_FOCUS_CLEAR:
        return ACCESSIBLE_SURFACE_DISCOVERY_CLEAR;
    case DISCOVERY_FOCUS_TOGGLE:
        return ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE;
    case DISCOVERY_FOCUS_EDIT:
        return ACCESSIBLE_SURFACE_DISCOVERY_EDIT;
    case DISCOVERY_FOCUS_OPEN:
        return ACCESSIBLE_SURFACE_DISCOVERY_OPEN;
    case DISCOVERY_FOCUS_CLOSE:
        return ACCESSIBLE_SURFACE_CLOSE;
    }
    return SIZE_MAX;
}

static RECT discovery_button_rect(const struct folio_window *_Nonnull self, size_t slot)
{
    struct discovery_layout layout = discovery_geometry(self);
    const RECT boxes[] = {layout.open, layout.toggle,       layout.clear,    layout.star,
                          layout.tag,  layout.previous_tag, layout.next_tag, layout.edit};
    return boxes[slot - ACCESSIBLE_SURFACE_DISCOVERY_OPEN];
}

static bool discovery_button_visible(const struct folio_window *_Nonnull self, size_t slot)
{
    if (!discovery_surface(self))
        return false;
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE && discovery_geometry(self).wide)
        return false;
    RECT bounds = discovery_button_rect(self, slot);
    return !IsRectEmpty(&bounds);
}

static bool discovery_button_enabled(const struct folio_window *_Nonnull self, size_t slot)
{
    if (self->command_composing)
        return false;
    if (slot != ACCESSIBLE_SURFACE_DISCOVERY_OPEN && slot != ACCESSIBLE_SURFACE_DISCOVERY_EDIT)
        return true;
    struct discovery_summary summary = folio_state_discovery_summary(self->state);
    return summary.phase == DISCOVERY_READY && summary.shown != 0;
}

static enum ui_text discovery_button_label(const struct folio_window *_Nonnull self, size_t slot)
{
    enum ui_text open = folio_state_discovery_summary(self->state).mode == DISCOVERY_MODE_BODY
                            ? UI_TEXT_DISCOVERY_BODY_OPEN
                            : UI_TEXT_DISCOVERY_OPEN;
    const enum ui_text labels[] = {open,
                                   self->discovery_show_preview ? UI_TEXT_DISCOVERY_RESULTS
                                                                : UI_TEXT_DISCOVERY_PREVIEW,
                                   UI_TEXT_DISCOVERY_CLEAR,
                                   UI_TEXT_DISCOVERY_STAR_CLEAR,
                                   UI_TEXT_DISCOVERY_TAG_REMOVE,
                                   UI_TEXT_DISCOVERY_PREVIOUS_TAG,
                                   UI_TEXT_DISCOVERY_NEXT_TAG,
                                   UI_TEXT_DISCOVERY_EDIT_OPEN};
    return labels[slot - ACCESSIBLE_SURFACE_DISCOVERY_OPEN];
}

/* 入力欄から↑↓で選んだときはフォーカスを入力欄に残し、続けて打てるようにする（#933）。 */
static void discovery_pick(struct folio_window *_Nonnull self, size_t index, bool typing)
{
    if (self->command_composing)
        return;
    size_t before = self->command_selection;
    self->command_failure = folio_state_select_discovery(self->state, index);
    self->discovery_focus = typing ? DISCOVERY_FOCUS_INPUT : DISCOVERY_FOCUS_ROWS;
    discovery_refresh(self);
    reveal_command_selection(self);
    SetFocus(typing ? self->command_input : self->command_layer);
    if (self->command_failure == FOLIO_STATE_READY && before != self->command_selection)
        discovery_motion_select(self);
}

static void discovery_select(struct folio_window *_Nonnull self, size_t index)
{
    discovery_pick(self, index, false);
}

/* 入力欄のPageUp/PageDownは原文previewを縦に送る。previewが出ていなければ候補のページ送り（#933）。
 */
static bool discovery_scroll_preview(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_PRIOR && key != VK_NEXT)
        return false;
    if (self->discovery_preview == nullptr || !IsWindowVisible(self->discovery_preview))
        return false;
    SendMessageW(self->discovery_preview, EM_SCROLL, key == VK_PRIOR ? SB_PAGEUP : SB_PAGEDOWN, 0);
    redraw_command_layer(self);
    return true;
}

static bool discovery_mode_arrow(struct folio_window *_Nonnull self, WPARAM key)
{
    if (key != VK_LEFT && key != VK_RIGHT)
        return false;
    self->discovery_mode_focus = self->discovery_mode_focus == DISCOVERY_MODE_NAMES
                                     ? DISCOVERY_MODE_BODY
                                     : DISCOVERY_MODE_NAMES;
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
    return true;
}

/* #940: Left chooses names and Right chooses body, keeping the focus where it is. */
static bool discovery_switch_mode(struct folio_window *_Nonnull self, WPARAM key)
{
    if ((key != VK_LEFT && key != VK_RIGHT) || self->command_composing)
        return false;
    enum discovery_mode wanted = key == VK_RIGHT ? DISCOVERY_MODE_BODY : DISCOVERY_MODE_NAMES;
    if (folio_state_discovery_summary(self->state).mode == wanted)
        return true;
    HWND focus = GetFocus();
    enum discovery_focus kept = self->discovery_focus;
    discovery_choose_mode(self, key == VK_RIGHT ? 1 : 0);
    self->discovery_focus = kept;
    SetFocus(focus);
    /* The new mode starts without results or source: a focus that it hid or disabled (the
     * source, a row, an open action) goes back to the query. */
    if (!discovery_focus_visible(self, discovery_current_focus(self)))
        discovery_set_focus(self, DISCOVERY_FOCUS_INPUT);
    redraw_command_layer(self);
    return true;
}

/* #940: in an empty query the plain arrows have no caret to move, so they switch the mode. With
 * text they stay caret movement; Alt+Left / Alt+Right switch at any time. */
static bool discovery_empty_arrow(struct folio_window *_Nonnull self, WPARAM key)
{
    return GetWindowTextLengthW(self->command_input) == 0 && discovery_switch_mode(self, key);
}

static bool discovery_navigate(struct folio_window *_Nonnull self, WPARAM key)
{
    if (GetFocus() == self->command_layer && self->discovery_focus == DISCOVERY_FOCUS_MODE)
        return discovery_mode_arrow(self, key);
    bool typing = GetFocus() == self->command_input;
    if (typing && discovery_scroll_preview(self, key))
        return true;
    if (typing && discovery_empty_arrow(self, key))
        return true;
    if (key != VK_UP && key != VK_DOWN && key != VK_PRIOR && key != VK_NEXT)
        return false;
    struct discovery_summary summary = folio_state_discovery_summary(self->state);
    if (summary.phase == DISCOVERY_READY && summary.shown != 0)
        discovery_pick(self, tag_next_row(self, key, summary.shown), typing);
    return true;
}

static void discovery_cycle_tag(struct folio_window *_Nonnull self, size_t slot)
{
    size_t count = discovery_tags_count(self);
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG && count != 0)
        self->discovery_tag = (self->discovery_tag + count - 1) % count;
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG && count != 0)
        self->discovery_tag = (self->discovery_tag + 1) % count;
}

static void discovery_action(struct folio_window *_Nonnull self, size_t slot)
{
    if (!discovery_button_enabled(self, slot))
        return;
    self->command_failure = FOLIO_STATE_READY;
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_OPEN || slot == ACCESSIBLE_SURFACE_DISCOVERY_EDIT)
    {
        discovery_open_selected(self, slot == ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
        return;
    }
    if (slot == ACCESSIBLE_SURFACE_CLOSE)
    {
        close_command_surface(self);
        return;
    }
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE)
        self->discovery_show_preview = !self->discovery_show_preview;
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_CLEAR)
        folio_state_clear_discovery_conditions(self->state);
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_STAR)
        folio_state_clear_discovery_scope(self->state);
    if (slot == ACCESSIBLE_SURFACE_DISCOVERY_TAG)
        self->command_failure = folio_state_remove_discovery_tag(self->state, self->discovery_tag);
    discovery_cycle_tag(self, slot);
    self->discovery_focus = DISCOVERY_FOCUS_INPUT;
    SetFocus(self->command_input);
    discovery_refresh(self);
}

static bool discovery_return(struct folio_window *_Nonnull self)
{
    if (GetFocus() == self->command_layer && self->discovery_focus == DISCOVERY_FOCUS_MODE)
    {
        discovery_choose_mode(self, self->discovery_mode_focus == DISCOVERY_MODE_BODY ? 1 : 0);
        return true;
    }
    /* #940: Ctrl+Enter opens in the editing view, also on the focused default action. */
    bool edit = (pressed_shortcut_modifiers() & FCONTROL) != 0;
    bool layer = GetFocus() == self->command_layer;
    if (!layer || self->discovery_focus == DISCOVERY_FOCUS_ROWS ||
        (edit && self->discovery_focus == DISCOVERY_FOCUS_OPEN))
        discovery_open_selected(self, edit);
    else
        discovery_action(self, discovery_focus_slot(self->discovery_focus));
    return true;
}

static enum discovery_focus discovery_current_focus(const struct folio_window *_Nonnull self)
{
    HWND focus = GetFocus();
    if (focus == self->command_input)
        return DISCOVERY_FOCUS_INPUT;
    if (focus == self->discovery_preview)
        return DISCOVERY_FOCUS_PREVIEW;
    return self->discovery_focus;
}

static bool discovery_focus_visible(const struct folio_window *_Nonnull self,
                                    enum discovery_focus focus)
{
    if (focus == DISCOVERY_FOCUS_INPUT || focus == DISCOVERY_FOCUS_MODE)
        return true;
    if (focus == DISCOVERY_FOCUS_PREVIEW)
        return IsWindowVisible(self->discovery_preview) != 0;
    if (focus == DISCOVERY_FOCUS_ROWS)
    {
        RECT rows = discovery_geometry(self).rows;
        return folio_state_discovery_summary(self->state).shown != 0 && !IsRectEmpty(&rows);
    }
    size_t slot = discovery_focus_slot(focus);
    return surface_button_visible(self, slot) && discovery_button_enabled(self, slot);
}

static void discovery_set_focus(struct folio_window *_Nonnull self, enum discovery_focus focus)
{
    self->discovery_focus = focus;
    HWND target = self->command_layer;
    if (focus == DISCOVERY_FOCUS_INPUT)
        target = self->command_input;
    if (focus == DISCOVERY_FOCUS_PREVIEW)
        target = self->discovery_preview;
    SetFocus(target);
    redraw_command_layer(self);
    accessibility_host_schedule(self->accessibility);
}

static bool discovery_tab(struct folio_window *_Nonnull self)
{
    const enum discovery_focus path[] = {
        DISCOVERY_FOCUS_INPUT,        DISCOVERY_FOCUS_MODE, DISCOVERY_FOCUS_STAR,
        DISCOVERY_FOCUS_PREVIOUS_TAG, DISCOVERY_FOCUS_TAG,  DISCOVERY_FOCUS_NEXT_TAG,
        DISCOVERY_FOCUS_CLEAR,        DISCOVERY_FOCUS_ROWS, DISCOVERY_FOCUS_PREVIEW,
        DISCOVERY_FOCUS_TOGGLE,       DISCOVERY_FOCUS_EDIT, DISCOVERY_FOCUS_OPEN,
        DISCOVERY_FOCUS_CLOSE};
    size_t count = sizeof path / sizeof path[0];
    size_t current = 0;
    enum discovery_focus focus = discovery_current_focus(self);
    for (size_t at = 0; at < count; ++at)
        if (path[at] == focus)
            current = at;
    size_t direction = (pressed_shortcut_modifiers() & FSHIFT) != 0 ? count - 1 : 1;
    for (size_t step = 0; step < count; ++step)
    {
        current = (current + direction) % count;
        if (discovery_focus_visible(self, path[current]))
        {
            if (path[current] == DISCOVERY_FOCUS_MODE)
                self->discovery_mode_focus = folio_state_discovery_summary(self->state).mode;
            discovery_set_focus(self, path[current]);
            break;
        }
    }
    return true;
}

static RECT discovery_busy_band(const struct folio_window *_Nonnull self)
{
    RECT band = discovery_geometry(self).input;
    UINT dpi = GetDpiForWindow(self->handle);
    band.top = band.bottom + scale(1, dpi);
    band.bottom = band.top + scale(2, dpi);
    return band;
}

static RECT discovery_selection_band(const struct folio_window *_Nonnull self)
{
    RECT band = discovery_row_rect(self, self->command_selection);
    if (!IsRectEmpty(&band))
        band.top = band.bottom - ui_part_focus_width(GetDpiForWindow(self->handle));
    return band;
}

static void discovery_motion_edges(const struct folio_window *_Nonnull self)
{
    RECT area;
    GetClientRect(self->command_layer, &area);
    int edge = scale(2, GetDpiForWindow(self->handle));
    const RECT sides[] = {{area.left, area.top, area.right, area.top + edge},
                          {area.left, area.bottom - edge, area.right, area.bottom},
                          {area.left, area.top, area.left + edge, area.bottom},
                          {area.right - edge, area.top, area.right, area.bottom}};
    for (size_t at = 0; at < sizeof sides / sizeof sides[0]; ++at)
        InvalidateRect(self->command_layer, &sides[at], FALSE);
}

static void discovery_motion_invalidate(const struct folio_window *_Nonnull self)
{
    if (self->command_layer == nullptr)
        return;
    if (self->discovery_open_frames != 0)
        discovery_motion_edges(self);
    if (self->discovery_selection_frames != 0)
        InvalidateRect(self->command_layer, &self->discovery_motion_row, FALSE);
    if (self->discovery_busy)
    {
        RECT band = discovery_busy_band(self);
        InvalidateRect(self->command_layer, &band, FALSE);
    }
}

static void discovery_motion_disarm(struct folio_window *_Nonnull self)
{
    if (self->discovery_timer != 0)
        KillTimer(self->handle, self->discovery_timer);
    self->discovery_timer = 0;
}

static void discovery_motion_settle(struct folio_window *_Nonnull self)
{
    discovery_motion_invalidate(self);
    discovery_motion_disarm(self);
    self->discovery_open_frames = 0;
    self->discovery_selection_frames = 0;
    self->discovery_busy = false;
    self->discovery_busy_tick = 0;
    self->discovery_busy_delay = 0;
    self->discovery_motion_wait = false;
}

static void discovery_motion_reset(struct folio_window *_Nonnull self)
{
    discovery_motion_settle(self);
    self->discovery_motion_failed = false;
}

static bool discovery_motion_allowed(const struct folio_window *_Nonnull self)
{
    return self->handle != nullptr && self->command_layer != nullptr && discovery_surface(self) &&
           IsWindowVisible(self->handle) && IsWindowVisible(self->command_layer) &&
           !IsIconic(self->handle) && !self->command_composing && !self->root_composing &&
           self->command_failure == FOLIO_STATE_READY &&
           folio_state_discovery_summary(self->state).phase != DISCOVERY_FAILED &&
           !self->appearance_pending && !self->discovery_motion_failed &&
           folio_window_accessible_ready(self) && folio_state_motion_enabled(self->state);
}

static bool discovery_motion_pending(const struct folio_window *_Nonnull self)
{
    switch (folio_state_discovery_summary(self->state).phase)
    {
    case DISCOVERY_PREPARING:
    case DISCOVERY_MATCHING:
        return folio_window_idle_work_ready(self);
    case DISCOVERY_CLOSED:
    case DISCOVERY_INPUT:
    case DISCOVERY_READY:
    case DISCOVERY_FAILED:
        return false;
    }
    return false;
}

static void discovery_motion_busy(struct folio_window *_Nonnull self)
{
    bool busy = discovery_motion_pending(self);
    if (busy == self->discovery_busy)
        return;
    RECT band = discovery_busy_band(self);
    InvalidateRect(self->command_layer, &band, FALSE);
    self->discovery_busy = busy;
    self->discovery_busy_tick = 0;
    self->discovery_busy_delay = busy ? 2 : 0;
}

static void discovery_motion_validate_row(struct folio_window *_Nonnull self)
{
    if (self->discovery_selection_frames == 0)
        return;
    RECT band = discovery_selection_band(self);
    uint64_t revision = folio_state_discovery_summary(self->state).results_revision;
    if (revision == self->discovery_motion_revision &&
        EqualRect(&band, &self->discovery_motion_row))
        return;
    InvalidateRect(self->command_layer, &self->discovery_motion_row, FALSE);
    self->discovery_selection_frames = 0;
}

static void discovery_motion_sync(struct folio_window *_Nonnull self)
{
    if (!discovery_motion_allowed(self))
    {
        discovery_motion_settle(self);
        return;
    }
    discovery_motion_busy(self);
    discovery_motion_validate_row(self);
    if (!folio_state_discovery_work_pending(self->state))
        self->discovery_motion_wait = false;
    if (self->discovery_open_frames == 0 && self->discovery_selection_frames == 0 &&
        !self->discovery_busy)
    {
        discovery_motion_disarm(self);
        return;
    }
    if (self->discovery_timer != 0 || self->discovery_motion_wait)
        return;
    self->discovery_timer = SetTimer(self->handle, discovery_motion_timer, 16, nullptr);
    if (self->discovery_timer == 0)
    {
        discovery_motion_settle(self);
        self->discovery_motion_failed = true;
    }
}

static void discovery_motion_tick(struct folio_window *_Nonnull self)
{
    if (self->discovery_timer == 0)
        return; /* An already queued tick must not resurrect a discarded animation. */
    if (!discovery_motion_allowed(self))
    {
        discovery_motion_settle(self);
        return;
    }
    if (folio_state_discovery_work_pending(self->state))
    {
        discovery_motion_disarm(self);
        self->discovery_motion_wait = true;
    }
    discovery_motion_invalidate(self);
    if (self->discovery_open_frames != 0)
        --self->discovery_open_frames;
    if (self->discovery_selection_frames != 0)
        --self->discovery_selection_frames;
    if (self->discovery_busy_delay != 0)
        --self->discovery_busy_delay;
    else if (self->discovery_busy)
        self->discovery_busy_tick = (unsigned char)((self->discovery_busy_tick + 1U) % 64U);
    discovery_motion_sync(self);
}

static void discovery_motion_select(struct folio_window *_Nonnull self)
{
    discovery_motion_validate_row(self);
    if (self->discovery_selection_frames != 0)
        InvalidateRect(self->command_layer, &self->discovery_motion_row, FALSE);
    self->discovery_selection_frames = 0;
    RECT band = discovery_selection_band(self);
    if (discovery_motion_allowed(self) && !IsRectEmpty(&band))
    {
        self->discovery_motion_row = band;
        self->discovery_motion_revision =
            folio_state_discovery_summary(self->state).results_revision;
        self->discovery_selection_frames = discovery_selection_frame_count;
        InvalidateRect(self->command_layer, &band, FALSE);
    }
    discovery_motion_sync(self);
}

static int discovery_motion_clamp(int value, int extent)
{
    if (value < 0)
        return 0;
    return value > extent ? extent : value;
}

static void draw_discovery_motion_frame(const struct folio_window *_Nonnull self, HDC device,
                                        RECT bounds)
{
    if (self->discovery_open_frames == 0)
        return;
    /* The eased steps of the first design, each followed by the point halfway to the next. */
    const int fractions[] = {24,  52,  80,  106, 132, 154, 176, 194,
                             212, 224, 236, 243, 250, 253, 256, 256};
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    int step = discovery_open_frame_count - self->discovery_open_frames;
    int distance = (width + height) * fractions[step] / 256;
    int edge = scale(2, GetDpiForWindow(self->handle));
    const RECT sides[] = {
        {bounds.left, bounds.top, bounds.left + discovery_motion_clamp(distance, width),
         bounds.top + edge},
        {bounds.left, bounds.top, bounds.left + edge,
         bounds.top + discovery_motion_clamp(distance, height)},
        {bounds.right - edge, bounds.top, bounds.right,
         bounds.top + discovery_motion_clamp(distance - width, height)},
        {bounds.left, bounds.bottom - edge,
         bounds.left + discovery_motion_clamp(distance - height, width), bounds.bottom}};
    for (size_t at = 0; at < sizeof sides / sizeof sides[0]; ++at)
        fill_settings_rect(device, sides[at], self->palette.current_text);
}

static void draw_discovery_motion(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    draw_discovery_motion_frame(self, device, bounds);
    if (self->discovery_selection_frames != 0)
    {
        RECT head = self->discovery_motion_row;
        int width = head.right - head.left;
        int length = scale(24, GetDpiForWindow(self->handle));
        length = length > width ? width : length;
        head.left += (width - length) *
                     (discovery_selection_frame_count - self->discovery_selection_frames) /
                     (discovery_selection_frame_count - 1);
        head.right = head.left + length;
        fill_settings_rect(device, head, self->palette.current_text);
    }
    if (self->discovery_busy && self->discovery_busy_delay == 0)
    {
        RECT band = discovery_busy_band(self);
        int width = band.right - band.left;
        int length = width / 5;
        band.left += (width - length) * self->discovery_busy_tick / 63;
        band.right = band.left + length;
        fill_settings_rect(device, band, self->palette.current_text);
    }
}

static void draw_discovery_row(const struct folio_window *_Nonnull self, HDC device, size_t at)
{
    RECT row = discovery_row_rect(self, at);
    if (IsRectEmpty(&row) || !RectVisible(device, &row))
        return;
    struct ui_part_paint part = {.bounds = row,
                                 .style = UI_PART_STYLE_CHOICE,
                                 .dpi = GetDpiForWindow(self->handle),
                                 .labeled = true,
                                 .selected = at == self->command_selection,
                                 .focused = at == self->command_selection &&
                                            GetFocus() == self->command_layer &&
                                            self->discovery_focus == DISCOVERY_FOCUS_ROWS &&
                                            (self->focus_cues || self->focus_cues_always)};
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    SetTextColor(device, ink.text);
    InflateRect(&row, -scale(8, part.dpi), 0);
    row.bottom = row.top + scale(22, part.dpi);
    char text[draw_unit_limit];
    discovery_text_name(self->state, at, text, sizeof text);
    draw_utf8(device, text, row);
    OffsetRect(&row, 0, scale(22, part.dpi));
    discovery_text_match(self->state, at, text, sizeof text);
    draw_utf8(device, text, row);
}

static void draw_discovery_gutter(const struct folio_window *_Nonnull self, HDC device, RECT area)
{
    const struct note_excerpt *_Nullable excerpt = folio_state_discovery_excerpt(self->state);
    if (!self->discovery_preview_ready || excerpt == nullptr || IsRectEmpty(&area))
        return;
    RECT text_area = {0};
    SendMessageW(self->discovery_preview, EM_GETRECT, 0, (LPARAM)&text_area);
    MapWindowPoints(self->discovery_preview, self->command_layer, (POINT *)&text_area, 2);
    area.top = text_area.top;
    area.bottom = text_area.bottom;
    int saved = SaveDC(device);
    SelectObject(device, self->mono_font);
    area.right = area.left + discovery_gutter_width(self) - scale(2, GetDpiForWindow(self->handle));
    IntersectClipRect(device, area.left, area.top, area.right, area.bottom);
    SetTextColor(device, self->palette.gutter_text);
    size_t first = (size_t)SendMessageW(self->discovery_preview, EM_GETFIRSTVISIBLELINE, 0, 0);
    TEXTMETRICW metrics = {0};
    GetTextMetricsW(device, &metrics);
    /* 折り返した行には番号を付けず、論理行の1行目の高さにだけ置く（#933）。 */
    for (size_t at = 0; at < self->discovery_line_count; ++at)
    {
        size_t visual = (size_t)SendMessageW(self->discovery_preview, EM_LINEFROMCHAR,
                                             (WPARAM)self->discovery_line_starts[at], 0);
        if (visual < first)
            continue;
        RECT line = area;
        line.top += (int)(visual - first) * metrics.tmHeight;
        if (line.top >= area.bottom)
            break;
        line.bottom = line.top + metrics.tmHeight;
        char number[24];
        (void)snprintf(number, sizeof number, "%zu", note_excerpt_first_line(excerpt) + at);
        draw_units(device, number, line, DT_RIGHT | DT_SINGLELINE | DT_NOPREFIX);
    }
    RestoreDC(device, saved);
}

static void draw_discovery_preview(const struct folio_window *_Nonnull self, HDC device,
                                   const struct discovery_layout *_Nonnull layout)
{
    if (IsRectEmpty(&layout->preview_title))
        return;
    char text[draw_unit_limit];
    SetTextColor(device, self->palette.current_text);
    if (RectVisible(device, &layout->preview_title))
    {
        discovery_text_title(self->state, text, sizeof text);
        draw_utf8(device, text, layout->preview_title);
    }
    if (RectVisible(device, &layout->preview_status))
    {
        discovery_text_preview(self->state, text, sizeof text);
        SetTextColor(device, self->palette.dialog_secondary);
        draw_utf8(device, text, layout->preview_status);
    }
    if (RectVisible(device, &layout->preview))
    {
        draw_input_field(self, device, self->discovery_preview, layout->preview);
        draw_discovery_gutter(self, device, layout->preview);
    }
}

static void draw_discovery_status(const struct folio_window *_Nonnull self, HDC device,
                                  const struct discovery_layout *_Nonnull layout)
{
    char text[draw_unit_limit];
    if (RectVisible(device, &layout->conditions))
    {
        SetTextColor(device, self->palette.dialog_secondary);
        discovery_text_conditions(self->state, text, sizeof text);
        draw_utf8(device, text, layout->conditions);
    }
    if (!RectVisible(device, &layout->status))
        return;
    discovery_text_status(self->state, text, sizeof text);
    if (self->command_failure != FOLIO_STATE_READY)
        (void)bounded_text_copy(
            text, sizeof text,
            folio_state_failure_line(self->command_failure, folio_state_language(self->state)));
    SetTextColor(device, self->palette.current_text);
    if (layout->compact)
        draw_utf8(device, text, layout->status);
    if (!layout->compact)
        (void)wrapped_text(device, text, layout->status, true);
}

static void draw_discovery(const struct folio_window *_Nonnull self, HDC device, RECT bounds)
{
    ui_part_draw_surface(device, &self->palette, bounds);
    draw_command_guidance(self, device);
    draw_floating_header(self, device, UI_TEXT_COMMAND_DISCOVER_NOTES);
    struct discovery_layout layout = discovery_geometry(self);
    if (RectVisible(device, &layout.input))
        draw_input_field(self, device, self->command_input, layout.input);
    draw_discovery_status(self, device, &layout);
    size_t count = folio_state_discovery_summary(self->state).shown;
    for (size_t at = self->command_first; at < count; ++at)
        draw_discovery_row(self, device, at);
    draw_discovery_preview(self, device, &layout);
    for (size_t slot = ACCESSIBLE_SURFACE_DISCOVERY_OPEN; slot < surface_button_count; ++slot)
        if (surface_button_visible(self, slot))
            draw_surface_button(self, device, slot);
    draw_discovery_motion(self, device, bounds);
}

static void click_discovery(struct folio_window *_Nonnull self, POINT point)
{
    if (self->command_composing)
        return;
    size_t count = folio_state_discovery_summary(self->state).shown;
    for (size_t at = self->command_first; at < count; ++at)
    {
        RECT row = discovery_row_rect(self, at);
        if (PtInRect(&row, point))
        {
            discovery_select(self, at);
            return;
        }
    }
}

static bool discovery_control_focused(const struct folio_window *_Nonnull self,
                                      struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_SURFACE_TAB)
        return surface_tab_focused(self, item.slot);
    if (GetFocus() != self->command_layer)
        return false;
    if (item.kind == ACCESSIBLE_SURFACE_CHOICE)
        return self->discovery_focus == DISCOVERY_FOCUS_ROWS &&
               self->command_selection == item.slot;
    return discovery_focus_slot(self->discovery_focus) == item.slot;
}

/* The rows and the two open actions carry the hint that names their keys. */
static bool discovery_hinted(struct accessible_item item)
{
    if (item.kind == ACCESSIBLE_SURFACE_CHOICE)
        return true;
    return item.kind == ACCESSIBLE_SURFACE_BUTTON &&
           (item.slot == ACCESSIBLE_SURFACE_DISCOVERY_OPEN ||
            item.slot == ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
}

static void discovery_control_view(const struct folio_window *_Nonnull self,
                                   struct accessible_item item,
                                   struct accessible_view *_Nonnull out)
{
    bool row = item.kind == ACCESSIBLE_SURFACE_CHOICE;
    out->focusable =
        row || item.kind == ACCESSIBLE_SURFACE_BUTTON || item.kind == ACCESSIBLE_SURFACE_TAB;
    out->focused = out->focusable && discovery_control_focused(self, item);
    out->enabled = out->enabled && !self->command_composing;
    if (item.kind == ACCESSIBLE_SURFACE_BUTTON)
        out->enabled = out->enabled && discovery_button_enabled(self, item.slot);
    if (discovery_hinted(item))
        (void)wide_units(
            ui_text_line(folio_state_discovery_summary(self->state).mode == DISCOVERY_MODE_BODY
                             ? UI_TEXT_DISCOVERY_BODY_HINT
                             : UI_TEXT_DISCOVERY_HINT,
                         folio_state_language(self->state)),
            out->help);
}

static HRESULT discovery_button_focus(struct folio_window *_Nonnull self, size_t slot)
{
    const enum discovery_focus values[] = {
        DISCOVERY_FOCUS_STAR,     DISCOVERY_FOCUS_PREVIOUS_TAG, DISCOVERY_FOCUS_TAG,
        DISCOVERY_FOCUS_NEXT_TAG, DISCOVERY_FOCUS_CLEAR,        DISCOVERY_FOCUS_TOGGLE,
        DISCOVERY_FOCUS_OPEN,     DISCOVERY_FOCUS_CLOSE,        DISCOVERY_FOCUS_EDIT};
    for (size_t at = 0; at < sizeof values / sizeof values[0]; ++at)
        if (discovery_focus_slot(values[at]) == slot)
        {
            discovery_set_focus(self, values[at]);
            return GetFocus() == self->command_layer ? S_OK : accessibility_node_invalid;
        }
    return accessibility_node_invalid;
}

static void discovery_status_text(const struct folio_window *_Nonnull self, char *_Nonnull out)
{
    char line[draw_unit_limit];
    discovery_text_conditions(self->state, out, replace_status_capacity);
    (void)bounded_text_append(out, replace_status_capacity, "\n");
    discovery_text_status(self->state, line, sizeof line);
    (void)bounded_text_append(out, replace_status_capacity, line);
    if (self->command_failure != FOLIO_STATE_READY)
    {
        (void)bounded_text_append(out, replace_status_capacity, "\n");
        (void)bounded_text_append(
            out, replace_status_capacity,
            folio_state_failure_line(self->command_failure, folio_state_language(self->state)));
    }
}

static bool discovery_accessible_name(const struct folio_window *_Nonnull self,
                                      enum accessible_kind kind, char16_t *_Nonnull out)
{
    if (kind != ACCESSIBLE_DISCOVERY_PREVIEW)
        return false;
    char text[draw_unit_limit];
    char status[256];
    discovery_text_title(self->state, text, sizeof text);
    discovery_text_preview(self->state, status, sizeof status);
    (void)bounded_text_append(text, sizeof text, "\n");
    (void)bounded_text_append(text, sizeof text, status);
    (void)wide_units(text, out);
    return true;
}
