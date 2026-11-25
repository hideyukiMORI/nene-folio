#include "drawer_window.h"
#include "accessibility_host.h"
#include "accessibility_node.h"
#include "accessible_request.h"
#include "accessible_view.h"

#include "chip_flow.h"
#include "drawer_filter_band.h"
#include "drawer_header_button.h"
#include "drawer_header_hit.h"
#include "drawer_layout.h"
#include "drawer_menu_item.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "icon_paint.h"
#include "menu_request.h"
#include "note_ref.h"
#include "note_tag.h"
#include "note_tags.h"
#include "tag_match_mode.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_pointer.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"

#include <commctrl.h>
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
    int accessible_band; /* 配置を作ったときの固定帯の高さ（札の行で動く・#1199） */
    /* 最後に描いた札の行の配置。変わったら主窓へ知らせる（高さが同じでも札は動く） */
    struct drawer_filter_band painted_band;
    uint64_t painted_revision; /* そのときのタグの条件の改訂。同じ幅の別のタグでも知らせる */
    /* 「× 解除」を押した点。解除が消えると同じ場所に「＋」の当たりが来るので、ポインタが
     * 動くまで、その点での次の押下を受けない（星の連続押下と同じ守り・ADR0091）。 */
    bool header_blocked;
    POINT header_block_point;
    struct folio_palette palette;
    HFONT _Nullable category_font;
    HFONT _Nullable note_font;
    HFONT _Nullable mono_font;
    /* 見出しの行の数の単位と、絞り込みの札の字（言語ごとの face・細字 11） */
    HFONT _Nullable unit_font;
    HFONT _Nullable chip_lit_font; /* 点いた札の字（同じ face・太字 11） */
    bool favorite_pressed;
    struct note_ref favorite_target;
    uint64_t favorite_revision;
    RECT favorite_bounds;
    bool favorite_blocked;
    POINT favorite_block_point;
    bool note_hover;
    struct note_ref hover_target;
    uint64_t hover_revision;
    RECT hover_bounds;
    bool pressed; /* 左ボタンを押した行を覚えているか */
    enum drawer_header_button header_pressed;
    /* 押した札・載っている札がタグの札のときの番号と、押したときのタグの条件の改訂（#1247）。
     * 離すまでに条件が変わっていたら、別のタグを外さない。 */
    size_t header_pressed_slot;
    uint64_t header_pressed_revision;
    bool header_inside; /* 押したボタンの矩形の内にポインタがある（外なら押していない見た目） */
    enum drawer_header_button header_hover;
    size_t header_hover_slot;
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
/* 1 回の描画で UTF-16 へ写せる単位数（ADR 0030 の決定 5）。表の 1 行もカテゴリ名・ノート名
 * （255 バイト）もここに収まる。収まらなければ何も描かない。 */
constexpr size_t draw_unit_limit = 1024;

/* 96 DPI での寸法（デザイン「案2 堅」）。描くときに DPI で拡大する（FR-013）。 */
constexpr int base_dpi = 96;
constexpr int base_header_indent = 16;
/* 常設の「すべてのノートを検索」の欄が占める帯（ADR 0024 の決定 6）。欄そのものは主窓の子で、
 * ここは行を置かない余白として数える。 */
constexpr int base_filter_height = 36;
/* 絞り込みの札の行（ADR0122）。札の高さ・余白は ui_part の F 様式。 */
constexpr int base_chip_top = 2;      /* 検索の欄の帯の下から、札の上端まで */
constexpr int base_chip_gap = 5;      /* 札の間と、札の行の間 */
constexpr int base_chip_bottom = 4;   /* 札の行の下に空ける間 */
constexpr int base_clear_height = 18; /* 「× 解除」の行 */
constexpr int base_clear_bottom = 2;  /* 解除の行の下に空ける間 */
constexpr int base_match_padding = 2; /* 2 択の字の左右の余白（#1249） */
constexpr int base_match_gap = 8;     /* 2 択の間と、2 択と解除の間 */
constexpr int chip_rows = 2;          /* 札を並べてよい行の数 */
/* タグの札の幅の上限と下限（#1247）。長い名前は … で切る。上限は、札が「+N」・開く札と同じ行に
 * 入る幅までさらに狭める（狭めないと、長い名前の札 1 つで 2 行に入らず、全部が畳まれる）。 */
constexpr int base_tag_chip_limit = 132;
constexpr int base_tag_chip_floor = 48;
/* 札の字を 1 行に組み立てる大きさ（「# 」＋タグ名 ＋ 終端で足りる）。 */
constexpr size_t chip_text_limit = 512;
constexpr int base_category_band_height = 36;
constexpr int base_filter_inset = 16; /* 欄の左右の余白（窓の縁 16・#349） */
constexpr int base_filter_margin = 4; /* 欄の上下に空ける間 */
constexpr int base_new_note_gap = 8;  /* 検索の欄と、右の「新しいノート」の印の間（#1291） */
/* 「一致 / 総数」の 1 行が要る大きさ（20 桁 2 つ ＋ 区切り ＋ 終端で足りる）。 */
constexpr size_t filter_count_capacity = 48;
constexpr int base_unit_gap = 6; /* 見出しの行の、数の単位と数の間 */
constexpr int base_row_height = 30;
constexpr int base_category_height = 34;
constexpr int base_category_gap = 6;
constexpr int base_category_indent = 16;
constexpr int base_name_offset = 26; /* 番号の左端から名前の左端まで */
constexpr int base_right_inset = 16;
/* カテゴリ名とノートの題の右端（右の縁から）。カーソルの角（幅−52〜幅−46）の左で切る
 * （ADR0091 の決定 1・ADR0122）。 */
constexpr int base_text_right = 58;
constexpr int base_bottom_padding = 16; /* 最後の行の下に空ける余白（左右の余白と同じ） */
constexpr int base_fade_height = 24;    /* あふれを示すフェードの高さ（ADR 0009 の決定 7） */
constexpr int base_mark_size = 6;
constexpr int base_mark_gap = 6; /* 右の列（星）とカーソルの角の間に空ける幅 */
constexpr int base_favorite_glyph = 16;
constexpr int base_favorite_target = 28;
/* 右の列の箱。星の当たりの中心と、カーソルの角の x を決める（ADR0091）。 */
constexpr int base_toggle_box = 24;
/* 畳む印（三角）の箱。カテゴリ行の左の余白（字下げ 16）に収め、番号の左端で終わる（#1199）。 */
constexpr int base_fold_box = 16;
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
    release_font(&self->unit_font);
    release_font(&self->chip_lit_font);
}

/* 文字の書体は言語ごとの face（ADR 0032 の決定 4）。番号の等幅は ui_face_mono（#1217）。
 * DPI が変わったときと言語を採り直したときの両方がここを通る（第 2 の経路を作らない）。 */
static void refresh_fonts(struct drawer_window *_Nonnull self, UINT dpi)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(folio_state_language(self->state), face);
    release_fonts(self);
    self->category_font = create_font(dpi, base_category_font, FW_BOLD, face);
    self->note_font = create_font(dpi, base_note_font, FW_NORMAL, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, base_mono_font);
    self->mono_font = create_font(dpi, mono_size, FW_NORMAL, mono);
    self->unit_font = create_font(dpi, base_mono_font, FW_NORMAL, face);
    self->chip_lit_font = create_font(dpi, base_mono_font, FW_BOLD, face);
}

/* 固定の検索欄・札の行・カテゴリ帯。行配置/clip/hit/scrollはこの高さを共用する。札の行は
 * 状態（条件が効いているか・札が何行か）で高さが動く（#1199）。 */
static int band_height(const struct drawer_window *_Nonnull self);

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

/* 「新しいノート」の印（#1291・ADR0122 の決定 1）。検索の欄の右、欄の帯の上下の中央。C 様式の
 * 28 角で、右端は窓の縁 16。検索の欄は、この印と間のぶんだけ狭くなる。 */
static RECT new_note_rect(const struct drawer_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT client;
    GetClientRect(self->handle, &client);
    int box = scale(ui_part_height, dpi);
    int right = client.right - scale(base_filter_inset, dpi);
    int top = (scale(base_filter_height, dpi) - box) / 2;
    return (RECT){right - box, top, right, top + box};
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
    bounds.right = new_note_rect(drawer).left - scale(base_new_note_gap, dpi);
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
        /* ノートの題の左端は、カテゴリ名の左端と同じ線（#1199）。別々に丸めた 2 つの和に
         * するので、どの DPI でも draw_category の名前の左端と 1px もずれない。 */
        .note_indent = scale(base_category_indent, dpi) + scale(base_name_offset, dpi),
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

/* いま選んである書体での、1 行の幅。 */
static int utf8_width(HDC device, const char *_Nonnull text)
{
    char16_t units[draw_unit_limit];
    int count = wide_units(text, units);
    RECT bounds = {0, 0, 0, 0};
    if (count != 0)
    {
        DrawTextW(device, units, count, &bounds, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    }
    return bounds.right;
}

/* 数だけの 1 行（「17」「2 / 17」）。ふだんはノートの総数、**絞り込みが効いているあいだは
 * 「一致 / 総数」**（ADR 0024 の 2026-09-22 の補正 7）。数は application の一致集合から取り、
 * UI は数えない。作れなければ 0 を返す。 */
static int count_units(const struct drawer_window *_Nonnull self, char16_t *_Nonnull out)
{
    size_t total = folio_state_note_count(self->state);
    if (!folio_state_filtering(self->state))
    {
        return format_count(total, (wchar_t *)out);
    }
    char line[filter_count_capacity];
    struct ui_text_request request = {.id = UI_TEXT_STATUS_FILTER_COUNT,
                                      .language = folio_state_language(self->state),
                                      .k = folio_state_index_filter_count(self->state),
                                      .n = total};
    if (ui_text_format(&request, line, filter_count_capacity) != UI_TEXT_FORMAT_READY)
    {
        return 0;
    }
    return wide_units(line, out);
}

/* 頭の帯の右の数。数は等幅の書体で右に寄せ、その左に、何の数かを言う単位（「ノート」）を
 * 言語ごとの書体で置く。単位が幅に入らないときは、数だけを描く（ADR0122）。呼び出し側は
 * 等幅の書体を選んでおく。 */
static void draw_header_count(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    enum folio_language language = folio_state_language(self->state);
    if (folio_state_index_filter_pending(self->state))
    {
        draw_utf8(device, ui_text_line(UI_TEXT_STATUS_FILTER_PREPARING, language), bounds,
                  DT_RIGHT);
        return;
    }
    char16_t number[draw_unit_limit];
    int count = count_units(self, number);
    if (count == 0)
    {
        return;
    }
    RECT measured = {0, 0, 0, 0};
    DrawTextW(device, (const wchar_t *)number, count, &measured,
              DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    const char *unit = ui_text_line(UI_TEXT_STATUS_NOTE_UNIT, language);
    RECT label = bounds;
    label.right -= measured.right + scale(base_unit_gap, GetDpiForWindow(self->handle));
    SelectObject(device, self->unit_font);
    if (utf8_width(device, unit) <= label.right - label.left)
    {
        draw_utf8(device, unit, label, DT_RIGHT);
    }
    SelectObject(device, self->mono_font);
    DrawTextW(device, (const wchar_t *)number, count, &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_NOPREFIX);
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

/* タグの札は番号も見る。ほかの部品の slot は 0。 */
static bool chip_focused(const struct drawer_window *_Nonnull self, enum accessible_kind kind,
                         size_t slot)
{
    struct accessible_item focus = {.kind = ACCESSIBLE_WINDOW};
    return accessibility_host_focus(self->accessibility, ACCESSIBLE_DRAWER, &focus) == S_OK &&
           focus.kind == kind && (kind != ACCESSIBLE_FILTER_TAG || focus.slot == slot) &&
           SendMessageW(GetParent(self->handle), folio_message_focus_cues, 0, 0) != 0;
}

static bool control_focused(const struct drawer_window *_Nonnull self, enum accessible_kind kind)
{
    return chip_focused(self, kind, 0);
}

static enum accessible_kind header_kind(enum drawer_header_button button);

static bool favorites_lit(const struct drawer_window *_Nonnull self)
{
    return folio_state_note_scope(self->state) == NOTE_SCOPE_FAVORITES;
}

static size_t tag_condition_count(const struct drawer_window *_Nonnull self)
{
    const struct note_tags *_Nullable tags = folio_state_tag_filter(self->state);
    return tags == nullptr ? 0 : note_tags_count(tags);
}

/* 札の字の幅。点いた札は太字で測る（描く書体と同じ）。 */
static int chip_text_width(const struct drawer_window *_Nonnull self, HDC device,
                           const char *_Nonnull text, bool lit)
{
    SelectObject(device, lit ? self->chip_lit_font : self->unit_font);
    return utf8_width(device, text);
}

/* お気に入りの札の幅: 余白・星・字・余白。点いた札は太字の分だけ広い。 */
static int favorites_chip_width(const struct drawer_window *_Nonnull self, HDC device, UINT dpi)
{
    int icon = scale(ui_part_chip_icon, dpi) + scale(ui_part_chip_icon_gap, dpi);
    const char *text = ui_text_line(UI_TEXT_TITLE_FAVORITES, folio_state_language(self->state));
    return 2 * scale(ui_part_chip_padding, dpi) + icon +
           chip_text_width(self, device, text, favorites_lit(self));
}

/* タグの面を開く札の字「# タグ」。どのタグで絞っているかは、タグごとの札が示す（#1247）。
 * 条件の中身（すべて含む／いずれか）は、読み上げの名前とタグの面が持つ。 */
static const char *_Nonnull tags_chip_text(const struct drawer_window *_Nonnull self)
{
    return ui_text_line(UI_TEXT_FILTER_TAGS, folio_state_language(self->state));
}

/* 開く札の幅: 余白・字・▾・余白。 */
static int tags_chip_width(const struct drawer_window *_Nonnull self, HDC device, UINT dpi)
{
    return 2 * scale(ui_part_chip_padding, dpi) +
           chip_text_width(self, device, tags_chip_text(self), false) +
           scale(ui_part_chip_icon_gap, dpi) + scale(ui_part_chip_icon, dpi);
}

/* タグの札の字「# 名前」。番号が条件の外なら空。 */
static void tag_chip_text(const struct drawer_window *_Nonnull self, size_t slot,
                          char *_Nonnull out)
{
    const struct note_tags *_Nullable tags = folio_state_tag_filter(self->state);
    out[0] = '\0';
    if (tags == nullptr || slot >= note_tags_count(tags))
    {
        return;
    }
    struct ui_text_request request = {.id = UI_TEXT_FILTER_TAG_LABEL,
                                      .language = folio_state_language(self->state),
                                      .name = note_tag_bytes(note_tags_at(tags, slot))};
    if (ui_text_format(&request, out, chip_text_limit) != UI_TEXT_FORMAT_READY)
    {
        out[0] = '\0';
    }
}

/* タグの札の幅: 余白・字（太字）・×・余白。上限は呼び出し側が掛ける。 */
static int tag_chip_width(const struct drawer_window *_Nonnull self, HDC device, UINT dpi,
                          size_t slot)
{
    char text[chip_text_limit];
    tag_chip_text(self, slot, text);
    return 2 * scale(ui_part_chip_padding, dpi) + chip_text_width(self, device, text, true) +
           scale(ui_part_chip_icon_gap, dpi) + scale(ui_part_chip_icon, dpi);
}

/* タグの札の幅の上限: 行の幅から「+N」と開く札と、その間 2 つを引いた残り（上限と下限の間）。 */
static int tag_chip_limit(const struct chip_flow_input *_Nonnull input, UINT dpi)
{
    int room = input->available - input->opener_width - input->more_width - 2 * input->gap;
    int limit = scale(base_tag_chip_limit, dpi);
    int floor = scale(base_tag_chip_floor, dpi);
    limit = room < limit ? room : limit;
    return limit > floor ? limit : floor;
}

/* 畳んだタグの札の字「+N」。 */
static void more_chip_text(const struct drawer_window *_Nonnull self, size_t hidden,
                           char *_Nonnull out)
{
    struct ui_text_request request = {
        .id = UI_TEXT_FILTER_MORE, .language = folio_state_language(self->state), .n = hidden};
    if (ui_text_format(&request, out, chip_text_limit) != UI_TEXT_FORMAT_READY)
    {
        out[0] = '\0';
    }
}

/* 「+N」の札の幅。畳む数は並べてみるまで決まらないので、いちばん広い数（条件のタグの数）で
 * 測る。桁が同じなら幅は変わらない。 */
static int more_chip_width(const struct drawer_window *_Nonnull self, HDC device, UINT dpi)
{
    char text[chip_text_limit];
    more_chip_text(self, tag_condition_count(self), text);
    return 2 * scale(ui_part_chip_padding, dpi) + chip_text_width(self, device, text, true);
}

/* 2 択の 1 つの字。 */
static const char *_Nonnull match_text(const struct drawer_window *_Nonnull self,
                                       enum tag_match_mode mode)
{
    enum ui_text line = UI_TEXT_FILTER_MATCH_ALL;
    switch (mode)
    {
    case TAG_MATCH_ALL:
        break;
    case TAG_MATCH_ANY:
        line = UI_TEXT_FILTER_MATCH_ANY;
        break;
    }
    return ui_text_line(line, folio_state_language(self->state));
}

/* 2 択の 1 つの釦。 */
static enum drawer_header_button match_button(enum tag_match_mode mode)
{
    switch (mode)
    {
    case TAG_MATCH_ALL:
        return DRAWER_HEADER_MATCH_ALL;
    case TAG_MATCH_ANY:
        return DRAWER_HEADER_MATCH_ANY;
    }
    return DRAWER_HEADER_MATCH_ALL;
}

/* 2 択の 1 つの幅。選んでいる方は太字なので、どちらも太字で測る（選び替えても幅が動かない）。 */
static int match_width(const struct drawer_window *_Nonnull self, HDC device, UINT dpi,
                       enum tag_match_mode mode)
{
    return 2 * scale(base_match_padding, dpi) +
           chip_text_width(self, device, match_text(self, mode), true);
}

/* 検索の範囲の 2 択の 1 つの字（#1292）。 */
static const char *_Nonnull scope_text(const struct drawer_window *_Nonnull self,
                                       enum search_scope scope)
{
    enum ui_text line = UI_TEXT_SEARCH_SCOPE_ALL;
    switch (scope)
    {
    case SEARCH_SCOPE_ALL:
        break;
    case SEARCH_SCOPE_FILTERED:
        line = UI_TEXT_SEARCH_SCOPE_FILTERED;
        break;
    }
    return ui_text_line(line, folio_state_language(self->state));
}

static enum drawer_header_button scope_button(enum search_scope scope)
{
    switch (scope)
    {
    case SEARCH_SCOPE_ALL:
        return DRAWER_HEADER_SCOPE_ALL;
    case SEARCH_SCOPE_FILTERED:
        return DRAWER_HEADER_SCOPE_FILTERED;
    }
    return DRAWER_HEADER_SCOPE_ALL;
}

/* 検索の範囲の行の幅: 見出し「範囲」（細字）・すべてのノート・絞り込みの中（タグの条件の
 * 2 択と同じく、どちらも太字で測る）の順。 */
static void scope_widths(const struct drawer_window *_Nonnull self, HDC device, UINT dpi,
                         int *_Nonnull out)
{
    const char *label = ui_text_line(UI_TEXT_SEARCH_SCOPE_LABEL, folio_state_language(self->state));
    int padding = 2 * scale(base_match_padding, dpi);
    out[0] = chip_text_width(self, device, label, false);
    out[1] = padding + chip_text_width(self, device, scope_text(self, SEARCH_SCOPE_ALL), true);
    out[2] = padding + chip_text_width(self, device, scope_text(self, SEARCH_SCOPE_FILTERED), true);
}

static int clear_width(const struct drawer_window *_Nonnull self, HDC device, UINT dpi)
{
    const char *text = ui_text_line(UI_TEXT_FILTER_CLEAR, folio_state_language(self->state));
    return 2 * scale(ui_part_chip_padding, dpi) + chip_text_width(self, device, text, false);
}

/* 札の行の左端と、使える幅。 */
static RECT chip_row(const struct drawer_window *_Nonnull self, UINT dpi)
{
    RECT client = {0, 0, 0, 0};
    GetClientRect(self->handle, &client);
    int inset = scale(base_filter_inset, dpi);
    int right = client.right > 2 * inset ? client.right - inset : inset;
    int top = scale(base_filter_height, dpi) + scale(base_chip_top, dpi);
    return (RECT){inset, top, right, top};
}

static RECT chip_rect(RECT row, struct chip_flow_place place, int height)
{
    return (RECT){row.left + place.left, row.top + place.top, row.left + place.left + place.width,
                  row.top + place.top + height};
}

/* 札の行の下の行: 左に「すべて含む／いずれか」（タグの条件が 2 つ以上のときだけ）、右端に
 * 「× 解除」。条件が効いていないときは、行ごと置かない。2 択と解除が 1 行に入らない幅では、
 * 解除を次の行へ送る（ADR0122 の決定 4）。widths は、すべて含む・いずれか・解除の順。 */
static void place_status(const struct drawer_window *_Nonnull self,
                         struct drawer_filter_band *_Nonnull band, const int *_Nonnull widths,
                         UINT dpi)
{
    if (!folio_state_conditions_active(self->state))
    {
        return;
    }
    int height = scale(base_clear_height, dpi);
    int top = band->bottom;
    if (tag_condition_count(self) >= 2)
    {
        int gap = scale(base_match_gap, dpi);
        int left = band->group.left;
        band->match_all = (RECT){left, top, left + widths[0], top + height};
        left = band->match_all.right + gap;
        band->match_any = (RECT){left, top, left + widths[1], top + height};
        if (band->match_any.right + gap + widths[2] > band->group.right)
        {
            top += height + scale(base_clear_bottom, dpi);
        }
    }
    int left = band->group.right - widths[2];
    band->clear = (RECT){left > band->group.left ? left : band->group.left, top, band->group.right,
                         top + height};
    band->bottom = band->clear.bottom + scale(base_clear_bottom, dpi);
}

/* 検索の範囲の行（#1292・ADR0122 の決定 6）: 条件の行の下に、見出し「探す範囲」と 2 択
 * 「すべてのノート／絞り込みの中」。条件と検索語の両方があるあいだだけ置く。見出しと 2 択が
 * 1 行に入らない幅では、見出しを置かずに 2 択を左から詰める。widths は scope_widths の順。 */
static void place_scope(const struct drawer_window *_Nonnull self,
                        struct drawer_filter_band *_Nonnull band, const int *_Nonnull widths,
                        UINT dpi)
{
    if (!folio_state_search_scope_offered(self->state))
    {
        return;
    }
    int height = scale(base_clear_height, dpi);
    int gap = scale(base_match_gap, dpi);
    int top = band->bottom;
    int left = band->group.left;
    if (left + widths[0] + gap + widths[1] + gap + widths[2] <= band->group.right)
    {
        band->scope_label = (RECT){left, top, left + widths[0], top + height};
        left = band->scope_label.right + gap;
    }
    band->scope_all = (RECT){left, top, left + widths[1], top + height};
    left = band->scope_all.right + gap;
    band->scope_filtered = (RECT){left, top, left + widths[2], top + height};
    band->bottom = top + height + scale(base_clear_bottom, dpi);
}

/* 絞り込みの札の行の配置。字の幅を測って、中核の chip_flow_arrange に並べさせる（ADR0122）。 */
static struct drawer_filter_band filter_band(const struct drawer_window *_Nonnull self)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT row = chip_row(self, dpi);
    int height = scale(ui_part_chip_height, dpi);
    HDC device = GetDC(self->handle);
    int saved = SaveDC(device);
    size_t tags = tag_condition_count(self);
    int widths[1 + note_tags_max_count] = {favorites_chip_width(self, device, dpi)};
    struct chip_flow_input input = {.widths = widths,
                                    .count = 1 + tags,
                                    .opener_width = tags_chip_width(self, device, dpi),
                                    .more_width = more_chip_width(self, device, dpi),
                                    .available = row.right - row.left,
                                    .height = height,
                                    .gap = scale(base_chip_gap, dpi),
                                    .row_gap = scale(base_chip_gap, dpi),
                                    .rows = chip_rows};
    int limit = tag_chip_limit(&input, dpi);
    for (size_t slot = 0; slot < tags && slot < note_tags_max_count; ++slot)
    {
        int width = tag_chip_width(self, device, dpi, slot);
        widths[1 + slot] = width < limit ? width : limit;
    }
    const int status[3] = {match_width(self, device, dpi, TAG_MATCH_ALL),
                           match_width(self, device, dpi, TAG_MATCH_ANY),
                           clear_width(self, device, dpi)};
    int scope[3] = {0, 0, 0};
    scope_widths(self, device, dpi, scope);
    RestoreDC(device, saved);
    ReleaseDC(self->handle, device);
    struct chip_flow_place places[1 + note_tags_max_count] = {{0, 0, 0}};
    struct chip_flow_result flow = chip_flow_arrange(&input, places);
    struct drawer_filter_band band = {.favorites = chip_rect(row, places[0], height),
                                      .shown = flow.shown - 1,
                                      .hidden = flow.hidden,
                                      .tags = chip_rect(row, flow.opener, height)};
    for (size_t slot = 0; slot < band.shown; ++slot)
    {
        band.chips[slot] = chip_rect(row, places[1 + slot], height);
    }
    if (band.hidden > 0)
    {
        band.more = chip_rect(row, flow.more, height);
    }
    band.group = (RECT){row.left, row.top, row.right,
                        row.top + flow.rows * height + (flow.rows - 1) * input.row_gap};
    band.bottom = band.group.bottom + scale(base_chip_bottom, dpi);
    place_status(self, &band, status, dpi);
    place_scope(self, &band, scope, dpi);
    return band;
}

/* 2 択の 2 つ（タグの条件・検索の範囲）と、検索の範囲の見出しが同じ所にあるか。 */
static bool same_choices(const struct drawer_filter_band *_Nonnull left,
                         const struct drawer_filter_band *_Nonnull right)
{
    return EqualRect(&left->match_all, &right->match_all) &&
           EqualRect(&left->match_any, &right->match_any) &&
           EqualRect(&left->scope_label, &right->scope_label) &&
           EqualRect(&left->scope_all, &right->scope_all) &&
           EqualRect(&left->scope_filtered, &right->scope_filtered);
}

static bool same_band(const struct drawer_filter_band *_Nonnull left,
                      const struct drawer_filter_band *_Nonnull right)
{
    if (left->shown != right->shown || left->hidden != right->hidden ||
        !EqualRect(&left->more, &right->more))
    {
        return false;
    }
    for (size_t slot = 0; slot < left->shown; ++slot)
    {
        if (!EqualRect(&left->chips[slot], &right->chips[slot]))
        {
            return false;
        }
    }
    if (!same_choices(left, right))
    {
        return false;
    }
    return left->bottom == right->bottom && EqualRect(&left->favorites, &right->favorites) &&
           EqualRect(&left->tags, &right->tags) && EqualRect(&left->clear, &right->clear);
}

static int band_height(const struct drawer_window *_Nonnull self)
{
    return filter_band(self).bottom +
           scale(base_category_band_height, GetDpiForWindow(self->handle));
}

/* 札 1 つの状態。hover・押下・focus・無効は、帯のほかの釦と同じ元から取る。 */
static struct ui_part_paint chip_part(const struct drawer_window *_Nonnull self,
                                      struct drawer_header_hit chip, RECT bounds)
{
    HWND parent = GetParent(self->handle);
    enum accessible_kind kind = header_kind(chip.button);
    bool pressed = self->header_pressed == chip.button && self->header_pressed_slot == chip.slot;
    bool hovered = self->header_hover == chip.button && self->header_hover_slot == chip.slot;
    return (struct ui_part_paint){
        .bounds = bounds,
        .style = UI_PART_STYLE_CHIP,
        .dpi = GetDpiForWindow(self->handle),
        .labeled = true,
        .hovered = hovered && !pressed,
        .pressed = (pressed && self->header_inside) ||
                   SendMessageW(parent, folio_message_part_pressed, kind, (LPARAM)chip.slot) != 0,
        .focused = chip_focused(self, kind, chip.slot),
        .disabled = SendMessageW(parent, folio_message_pointer_blocked, 0, 0) != 0};
}

/* お気に入りの札。点いているときは、塗った星・太字・字の色の枠（塗らない。タグの札と同じく
 * ✓ は置かない・ADR0122 の決定 2）。 */
static void draw_favorites_chip(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    bool lit = favorites_lit(self);
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){DRAWER_HEADER_FAVORITES, 0}, bounds);
    part.selected = lit;
    part.has_icon = true;
    part.icon = lit ? ICON_PAINT_STAR : ICON_PAINT_STAR_OUTLINE;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    RECT label = ink.label;
    label.right = bounds.right - scale(ui_part_chip_padding, dpi);
    SelectObject(device, lit ? self->chip_lit_font : self->unit_font);
    SetTextColor(device, ink.text);
    draw_utf8(device, ui_text_line(UI_TEXT_TITLE_FAVORITES, folio_state_language(self->state)),
              label, DT_END_ELLIPSIS);
}

/* 札の右端に置く絵（開く札の ▾、タグの札の ×）の箱。字は、その左（絵と字の間を空ける）まで。 */
static RECT chip_trailing_rect(const struct drawer_window *_Nonnull self, RECT bounds)
{
    UINT dpi = GetDpiForWindow(self->handle);
    int size = scale(ui_part_chip_icon, dpi);
    int right = bounds.right - scale(ui_part_chip_padding, dpi);
    int top = (bounds.top + bounds.bottom - size) / 2;
    return (RECT){right - size, top, right, top + size};
}

/* 右端に絵を持つ札の字を描く。 */
static void draw_chip_label(const struct drawer_window *_Nonnull self, HDC device, RECT label,
                            const char *_Nonnull text)
{
    label.right -= scale(ui_part_chip_icon_gap, GetDpiForWindow(self->handle));
    draw_utf8(device, text, label, DT_END_ELLIPSIS);
}

/* タグごとの札「# 名前 ×」（#1247）。いつも点いた見た目（条件として効いている）。札のどこを
 * 押しても外れるので、× は絵として描くだけ（ADR0122）。 */
static void draw_tag_chip(const struct drawer_window *_Nonnull self, HDC device, size_t slot,
                          RECT bounds)
{
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){DRAWER_HEADER_TAG, slot}, bounds);
    part.selected = true;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    RECT mark = chip_trailing_rect(self, bounds);
    icon_paint_fill(device, mark, ICON_PAINT_CLOSE, ink.text);
    char text[chip_text_limit];
    tag_chip_text(self, slot, text);
    SelectObject(device, self->chip_lit_font);
    SetTextColor(device, ink.text);
    draw_chip_label(self, device,
                    (RECT){ink.label.left, ink.label.top, mark.left, ink.label.bottom}, text);
}

/* 畳んだタグの「+N」。押すとタグの面が開く。 */
static void draw_more_chip(const struct drawer_window *_Nonnull self, HDC device, size_t hidden,
                           RECT bounds)
{
    if (IsRectEmpty(&bounds))
    {
        return;
    }
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){DRAWER_HEADER_MORE, 0}, bounds);
    part.selected = true;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    RECT label = ink.label;
    label.right = bounds.right - scale(ui_part_chip_padding, GetDpiForWindow(self->handle));
    char text[chip_text_limit];
    more_chip_text(self, hidden, text);
    SelectObject(device, self->chip_lit_font);
    SetTextColor(device, ink.text);
    /* 幅は、いちばん広い数で取ってある。畳んだ数の桁が少ないときに片寄らないよう、中央に描く。 */
    draw_utf8(device, text, label, DT_CENTER);
}

/* タグの面を開く札。 */
static void draw_tags_chip(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){DRAWER_HEADER_TAG_FILTER, 0}, bounds);
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    RECT mark = chip_trailing_rect(self, bounds);
    icon_paint_fill(device, mark, ICON_PAINT_FOLD_COLLAPSE, ink.text); /* 面が開く印の ▾ */
    SelectObject(device, self->unit_font);
    SetTextColor(device, ink.text);
    draw_chip_label(self, device,
                    (RECT){ink.label.left, ink.label.top, mark.left, ink.label.bottom},
                    tags_chip_text(self));
}

/* 「すべて含む／いずれか」の 1 つ。枠を持たない札で、選んでいる方は字の色・太字・下辺の線
 * （塗らない。線は ui_part が描く・ADR0060 の 2026-11-24 の補正）。無いときは描かない。 */
static void draw_match(const struct drawer_window *_Nonnull self, HDC device,
                       enum tag_match_mode mode, RECT bounds)
{
    if (IsRectEmpty(&bounds))
    {
        return;
    }
    bool selected = folio_state_tag_match_mode(self->state) == mode;
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){match_button(mode), 0}, bounds);
    part.grouped = true;
    part.selected = selected;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    SelectObject(device, selected ? self->chip_lit_font : self->unit_font);
    SetTextColor(device, ink.text);
    draw_utf8(device, match_text(self, mode), bounds, DT_CENTER);
}

/* 検索の範囲の 2 択の 1 つ（#1292）。見た目は、タグの条件の 2 択と同じ（枠を持たない札・
 * 選んでいる方は字の色・太字・下辺の線）。無いときは描かない。 */
static void draw_scope(const struct drawer_window *_Nonnull self, HDC device,
                       enum search_scope scope, RECT bounds)
{
    if (IsRectEmpty(&bounds))
    {
        return;
    }
    bool selected = folio_state_search_scope(self->state) == scope;
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){scope_button(scope), 0}, bounds);
    part.grouped = true;
    part.selected = selected;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    SelectObject(device, selected ? self->chip_lit_font : self->unit_font);
    SetTextColor(device, ink.text);
    draw_utf8(device, scope_text(self, scope), bounds, DT_CENTER);
}

/* 検索の範囲の行の見出し「範囲」（読み上げの群の名前は「探す範囲」）。字だけで、押せない。見出しの数の単位と同じ書体と色。
 * 2 択と 1 行に入らない幅では、矩形が空で、描かない。 */
static void draw_scope_label(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    if (IsRectEmpty(&bounds))
    {
        return;
    }
    SelectObject(device, self->unit_font);
    SetTextColor(device, self->palette.note_text);
    draw_utf8(device, ui_text_line(UI_TEXT_SEARCH_SCOPE_LABEL, folio_state_language(self->state)),
              bounds, 0);
}

/* 「× 解除」。枠を持たない札（字だけ）。条件が効いていないときは描かない。 */
static void draw_clear(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    if (IsRectEmpty(&bounds))
    {
        return;
    }
    struct ui_part_paint part =
        chip_part(self, (struct drawer_header_hit){DRAWER_HEADER_CLEAR, 0}, bounds);
    part.grouped = true;
    struct ui_part_ink ink = ui_part_draw(device, &self->palette, &part);
    SelectObject(device, self->unit_font);
    SetTextColor(device, ink.text);
    draw_utf8(device, ui_text_line(UI_TEXT_FILTER_CLEAR, folio_state_language(self->state)),
              ink.label, DT_END_ELLIPSIS);
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
    const char *title = ui_text_line(UI_TEXT_TITLE_CATEGORIES, folio_state_language(self->state));
    draw_utf8(device, title, label, 0);
    /* 数に使える幅は、見出しの字の右から「＋」の左まで（どちらも 8 空ける）。見出しの字の
     * 幅で決めるので、短い見出しの言語では単位つきの数が入りやすい（ADR0122）。 */
    int title_right = label.left + utf8_width(device, title);
    RECT count = {(title_right < label.right ? title_right : label.right) + scale(8, dpi),
                  label.top, button.left - scale(8, dpi), label.bottom};
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
        .hovered = self->header_hover == DRAWER_HEADER_NEW_CATEGORY && !pressed,
        .pressed = pressed && self->header_inside,
        .focused = control_focused(self, ACCESSIBLE_NEW_CATEGORY),
    };
    ui_part_draw(device, &self->palette, &part);
}

/* 「新しいノート」の印は C 様式。「新しいカテゴリ」の＋と同じ 1 本を通る（#1291）。 */
static void draw_new_note(const struct drawer_window *_Nonnull self, HDC device)
{
    bool pressed = self->header_pressed == DRAWER_HEADER_NEW_NOTE;
    struct ui_part_paint part = {
        .bounds = new_note_rect(self),
        .style = UI_PART_STYLE_ICON,
        .dpi = GetDpiForWindow(self->handle),
        .has_icon = true,
        .icon = ICON_PAINT_NEW_NOTE,
        .hovered = self->header_hover == DRAWER_HEADER_NEW_NOTE && !pressed,
        .pressed = pressed && self->header_inside,
        .focused = control_focused(self, ACCESSIBLE_NEW_NOTE),
    };
    ui_part_draw(device, &self->palette, &part);
}

/* 固定帯。共通のブランド/窓操作の頭は親が所有する（ADR0047）。 */
static void draw_header(const struct drawer_window *_Nonnull self, HDC device)
{
    SetTextCharacterExtra(device, 0);
    draw_filter_frame(self, device);
    draw_new_note(self, device);
    struct drawer_filter_band band = filter_band(self);
    draw_favorites_chip(self, device, band.favorites);
    for (size_t slot = 0; slot < band.shown; ++slot)
    {
        draw_tag_chip(self, device, slot, band.chips[slot]);
    }
    draw_more_chip(self, device, band.hidden, band.more);
    draw_tags_chip(self, device, band.tags);
    draw_match(self, device, TAG_MATCH_ALL, band.match_all);
    draw_match(self, device, TAG_MATCH_ANY, band.match_any);
    draw_clear(self, device, band.clear);
    draw_scope_label(self, device, band.scope_label);
    draw_scope(self, device, SEARCH_SCOPE_ALL, band.scope_all);
    draw_scope(self, device, SEARCH_SCOPE_FILTERED, band.scope_filtered);
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

/* 右の列（星の当たりの中心を決める 24 の箱と間）の幅。カーソルの角はこのぶんだけ左に置いて、
 * 星との重なりを避ける（ADR 0015 の決定 7 の補正）。畳む印は行の左へ移った（#1199）。 */
static int toggle_room(UINT dpi)
{
    return scale(base_toggle_box, dpi) + scale(base_mark_gap, dpi);
}

/* カテゴリ行: 左の余白に畳む印（▾／▸）、番号（カテゴリ色・等幅）、名前（太字・字間広め）。
 * 右の列は星だけに使う（#1199）。カーソルの角は、ノートの行と同じ x に置く（ADR 0015 の決定 7）。
 */
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
                 width - scale(base_text_right, dpi), row.top + row.height};
    SelectObject(device, self->category_font);
    SetTextColor(device, self->palette.category_text);
    SetTextCharacterExtra(device, scale(base_tracking, dpi));
    draw_utf8(device, row.text, name, DT_END_ELLIPSIS);
    SetTextCharacterExtra(device, 0);
    int box = scale(base_fold_box, dpi);
    int top = row.top + (row.height - box) / 2;
    RECT fold = {row.indent - box, top, row.indent, top + box};
    icon_paint_fill(device, fold, row.expanded ? ICON_PAINT_FOLD_COLLAPSE : ICON_PAINT_FOLD_EXPAND,
                    self->palette.header_text);
    if (row.cursor)
    {
        draw_cursor_mark(self, device, row,
                         width - scale(base_right_inset, dpi) - toggle_room(dpi));
    }
}

/* ノート行: 選択中なら面を敷き、カーソルの行なら右端にカテゴリ色の角を置く
 * （面は選択・角はカーソル・ADR 0015 の決定 8）。 */
/* 字形と当たりを分け、右の列の中心（幅−28）へ置く（ADR0091。折畳印は左へ移った・ADR0122）。 */
static RECT favorite_rect(const struct drawer_window *_Nonnull self, struct drawer_row row)
{
    UINT dpi = GetDpiForWindow(self->handle);
    RECT client;
    GetClientRect(self->handle, &client);
    int center = client.right - scale(base_right_inset + base_toggle_box / 2, dpi);
    int left = center - scale(base_favorite_target, dpi) / 2;
    return (RECT){left, row.top, left + scale(base_favorite_target, dpi), row.top + row.height};
}

static bool same_note(struct drawer_row row, struct note_ref ref)
{
    return row.category == ref.category && row.note == ref.note;
}

static bool hovered_note(const struct drawer_window *_Nonnull self, struct drawer_row row)
{
    RECT bounds = favorite_rect(self, row);
    return self->note_hover && self->hover_revision == folio_state_index_revision(self->state) &&
           same_note(row, self->hover_target) && EqualRect(&bounds, &self->hover_bounds);
}

static bool favorite_shown(const struct drawer_window *_Nonnull self, struct drawer_row row)
{
    uint64_t revision = folio_state_index_revision(self->state);
    struct note_ref ref = {row.category, row.note};
    return folio_state_note_favorite(self->state, ref) || (row.cursor && index_focused(self)) ||
           hovered_note(self, row) ||
           (self->favorite_pressed && self->favorite_revision == revision &&
            same_note(row, self->favorite_target));
}

static void draw_favorite(const struct drawer_window *_Nonnull self, HDC device,
                          struct drawer_row row)
{
    if (!favorite_shown(self, row))
        return;
    RECT target = favorite_rect(self, row);
    int size = scale(base_favorite_glyph, GetDpiForWindow(self->handle));
    int left = target.left + (target.right - target.left - size) / 2;
    int top = row.top + (row.height - size) / 2;
    RECT star = {left, top, left + size, top + size};
    bool favorite =
        folio_state_note_favorite(self->state, (struct note_ref){row.category, row.note});
    icon_paint_fill(device, star, favorite ? ICON_PAINT_STAR : ICON_PAINT_STAR_OUTLINE,
                    row.selected ? self->palette.selected_text : self->palette.note_text);
}

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
        draw_cursor_mark(self, device, row, width - inset - toggle_room(dpi));
    }
    draw_favorite(self, device, row);
    RECT bounds = {row.indent, row.top, width - scale(base_text_right, dpi), row.top + row.height};
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
                      const struct drawer_layout *_Nonnull layout, RECT clip)
{
    size_t count = drawer_layout_row_count(layout);
    for (size_t index = 0; index < count; ++index)
    {
        struct drawer_row row = drawer_layout_row(layout, index);
        if (row.top >= clip.bottom)
            break;
        if (row.top + row.height <= clip.top)
            continue;
        switch (row.kind)
        {
        case DRAWER_ROW_CATEGORY:
            draw_category(self, device, row, clip.right);
            break;
        case DRAWER_ROW_NOTE:
            draw_note(self, device, row, clip.right);
            break;
        }
    }
    draw_drop_line(self, device, layout, clip.right);
}

/* 一覧が空のときの説明。お気に入りの文は、一覧にお気に入りの範囲が掛かっているあいだだけ
 * （検索語がすべてのノートから探しているあいだは、札が点いていても出さない・#1292）。 */
static enum ui_text empty_message(const struct drawer_window *_Nonnull self)
{
    if (folio_state_listed_scope(self->state) != NOTE_SCOPE_FAVORITES ||
        folio_state_index_filter_pending(self->state))
        return UI_TEXT_EMPTY;
    return folio_state_favorite_count(self->state) == 0 ? UI_TEXT_EMPTY_FAVORITES
                                                        : UI_TEXT_EMPTY_FAVORITES_FILTER;
}

static void draw_empty(const struct drawer_window *_Nonnull self, HDC device, RECT bounds)
{
    char16_t units[draw_unit_limit];
    int count =
        wide_units(ui_text_line(empty_message(self), folio_state_language(self->state)), units);
    int inset = scale(base_filter_inset, GetDpiForWindow(self->handle));
    bounds.left += inset;
    bounds.right -= inset;
    bounds.top += inset;
    SelectObject(device, self->note_font);
    SetTextColor(device, self->palette.note_text);
    DrawTextW(device, units, count, &bounds, DT_WORDBREAK | DT_NOPREFIX);
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
    draw_rows(self, device, layout, (RECT){0, header, client.right, client.bottom});
    if (drawer_layout_row_count(layout) == 0)
        draw_empty(self, device, (RECT){0, header, client.right, client.bottom});
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
    /* 札の行の配置が変わったら、主窓に説明（tooltip）の矩形と focus を合わせ直させる（#1199）。
     * 高さが同じでも、札が点くと幅が変わって隣の札が動く。 */
    struct drawer_filter_band band = filter_band(self);
    uint64_t revision = folio_state_tag_filter_revision(self->state);
    if (!same_band(&band, &self->painted_band) || revision != self->painted_revision)
    {
        self->painted_band = band;
        self->painted_revision = revision;
        PostMessageW(GetParent(self->handle), folio_message_drawer_band, 0, 0);
    }
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
static bool row_action_available(const struct drawer_window *_Nonnull self,
                                 enum drawer_row_kind kind)
{
    switch (kind)
    {
    case DRAWER_ROW_CATEGORY:
        return !folio_state_filtering(self->state);
    case DRAWER_ROW_NOTE:
        return true;
    }
    return false;
}

static void act_on_row(struct drawer_window *_Nonnull self, struct drawer_row row)
{
    if (!row_action_available(self, row.kind))
        return;
    enum folio_state_outcome outcome = FOLIO_STATE_READY;
    switch (row.kind)
    {
    case DRAWER_ROW_CATEGORY:
        /* #1069 / ADR0117 stage 5: the categories may have been read again before the write.
         * A toggle that went through then says so in a line; the list is drawn again whatever
         * the outcome, since a refused toggle may have left a fresh list too. */
        outcome = folio_state_ledger_outcome(
            self->state, folio_state_toggle_category(self->state, row.category));
        InvalidateRect(self->handle, nullptr, FALSE);
        /* the pane's head shows the category's number and colour, which a fresh list can move */
        InvalidateRect(GetParent(self->handle), nullptr, FALSE);
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
    SendMessageW(GetParent(self->handle), folio_message_report_outcome, (WPARAM)outcome, 0);
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
    /* #1069 / ADR0117: the list may have been read again before the write. A drop that went
     * through then says so in a line, and the list is drawn again whatever the outcome: a
     * refused drop may have left a fresh list too. */
    outcome = folio_state_ledger_outcome(self->state, outcome);
    if (outcome != FOLIO_STATE_READY)
        SendMessageW(GetParent(self->handle), folio_message_report_outcome, (WPARAM)outcome, 0);
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
    if (!SendMessageW(GetParent(self->handle), folio_message_choose_color, 0, (LPARAM)&choice))
    {
        /* キャンセルは何もしない（決定 4）。 */
        return;
    }
    enum folio_state_outcome outcome = folio_state_ledger_outcome(
        self->state,
        folio_state_recolor_category(self->state, row.category, to_rgb_color(choice.rgbResult)));
    InvalidateRect(self->handle, nullptr, FALSE); /* a fresh list may have been read (#1069) */
    if (outcome != FOLIO_STATE_READY)
    {
        SendMessageW(GetParent(self->handle), folio_message_report_outcome, (WPARAM)outcome, 0);
        InvalidateRect(GetParent(self->handle), nullptr, FALSE);
        return;
    }
    /* 右ペインの頭の色も台帳から引く。本文は触らない（決定 6）。 */
    InvalidateRect(GetParent(self->handle), nullptr, FALSE);
}

/* 右クリックは対象とanchorだけを主窓へ渡す。文書・cursor・focusを変更しない。 */
static void show_menu(struct drawer_window *_Nonnull self, LPARAM position)
{
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (self->pressed || self->header_pressed != DRAWER_HEADER_NONE || point.y < band_height(self))
        return;
    struct drawer_layout *_Nullable layout = nullptr;
    if (!current_layout(self, &layout))
        return;
    size_t index = 0;
    bool found = drawer_layout_hit(layout, point.y, &index);
    struct drawer_row row = found ? drawer_layout_row(layout, index) : (struct drawer_row){0};
    drawer_layout_destroy(layout);
    MapWindowPoints(self->handle, GetParent(self->handle), &point, 1);
    struct menu_request request = {.context = FOLIO_MENU_EMPTY,
                                   .target = {row.category, row.note},
                                   .anchor = {point.x, point.y, point.x, point.y}};
    if (found)
        request.context = row.kind == DRAWER_ROW_CATEGORY ? FOLIO_MENU_CATEGORY : FOLIO_MENU_NOTE;
    SendMessageW(GetParent(self->handle), folio_message_open_menu, 0, (LPARAM)&request);
}

void drawer_window_choose_color(struct drawer_window *_Nonnull self, size_t category)
{
    struct drawer_layout *_Nullable layout = nullptr;
    if (!current_layout(self, &layout))
        return;
    for (size_t at = 0; at < drawer_layout_row_count(layout); ++at)
    {
        struct drawer_row row = drawer_layout_row(layout, at);
        if (row.kind == DRAWER_ROW_CATEGORY && row.category == category)
        {
            drawer_layout_destroy(layout);
            choose_color(self, row);
            return;
        }
    }
    drawer_layout_destroy(layout);
}

void drawer_window_context_menu(struct drawer_window *_Nonnull self)
{
    struct menu_request request = {.context = FOLIO_MENU_EMPTY, .keyboard = true};
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref target = {0};
    if (folio_state_cursor(self->state, &kind, &target))
    {
        struct accessible_item item = {.kind = kind == FOLIO_CURSOR_CATEGORY ? ACCESSIBLE_CATEGORY
                                                                             : ACCESSIBLE_NOTE,
                                       .ref = target,
                                       .revision = folio_state_row_revision(self->state)};
        struct accessible_view view = {0};
        if (SUCCEEDED(drawer_window_accessible_read(self, &item, &view)) && !view.offscreen)
        {
            request.context = kind == FOLIO_CURSOR_CATEGORY ? FOLIO_MENU_CATEGORY : FOLIO_MENU_NOTE;
            request.target = target;
            request.anchor = view.bounds;
            MapWindowPoints(nullptr, GetParent(self->handle), (POINT *)&request.anchor, 2);
        }
    }
    if (request.context == FOLIO_MENU_EMPTY)
    {
        POINT point = {16, band_height(self)};
        MapWindowPoints(self->handle, GetParent(self->handle), &point, 1);
        request.anchor = (RECT){point.x, point.y, point.x, point.y};
    }
    SendMessageW(GetParent(self->handle), folio_message_open_menu, 0, (LPARAM)&request);
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
    case DRAWER_HEADER_NEW_NOTE:
        return new_note_rect(self);
    case DRAWER_HEADER_FAVORITES:
        return filter_band(self).favorites;
    case DRAWER_HEADER_TAG:
        return (RECT){0, 0, 0, 0}; /* 番号が要る。header_hit_rect が答える */
    case DRAWER_HEADER_MORE:
        return filter_band(self).more;
    case DRAWER_HEADER_TAG_FILTER:
        return filter_band(self).tags;
    case DRAWER_HEADER_MATCH_ALL:
        return filter_band(self).match_all;
    case DRAWER_HEADER_MATCH_ANY:
        return filter_band(self).match_any;
    case DRAWER_HEADER_SCOPE_ALL:
        return filter_band(self).scope_all;
    case DRAWER_HEADER_SCOPE_FILTERED:
        return filter_band(self).scope_filtered;
    case DRAWER_HEADER_CLEAR:
        return filter_band(self).clear;
    case DRAWER_HEADER_NEW_CATEGORY:
        return new_category_rect(self);
    }
    return (RECT){0, 0, 0, 0};
}

static enum accessible_kind header_kind(enum drawer_header_button button)
{
    switch (button)
    {
    case DRAWER_HEADER_NONE:
        return ACCESSIBLE_DRAWER;
    case DRAWER_HEADER_NEW_NOTE:
        return ACCESSIBLE_NEW_NOTE;
    case DRAWER_HEADER_FAVORITES:
        return ACCESSIBLE_FILTER_FAVORITES;
    case DRAWER_HEADER_TAG:
        return ACCESSIBLE_FILTER_TAG;
    case DRAWER_HEADER_MORE:
        return ACCESSIBLE_FILTER_MORE;
    case DRAWER_HEADER_TAG_FILTER:
        return ACCESSIBLE_TAG_FILTER;
    case DRAWER_HEADER_MATCH_ALL:
        return ACCESSIBLE_FILTER_MATCH_ALL;
    case DRAWER_HEADER_MATCH_ANY:
        return ACCESSIBLE_FILTER_MATCH_ANY;
    case DRAWER_HEADER_SCOPE_ALL:
        return ACCESSIBLE_SEARCH_SCOPE_ALL;
    case DRAWER_HEADER_SCOPE_FILTERED:
        return ACCESSIBLE_SEARCH_SCOPE_FILTERED;
    case DRAWER_HEADER_CLEAR:
        return ACCESSIBLE_FILTER_CLEAR;
    case DRAWER_HEADER_NEW_CATEGORY:
        return ACCESSIBLE_NEW_CATEGORY;
    }
    return ACCESSIBLE_DRAWER;
}

/* 当たりの矩形。タグの札は番号で引く。置かれていない番号は空。 */
static RECT header_hit_rect(const struct drawer_window *_Nonnull self, struct drawer_header_hit hit)
{
    if (hit.button != DRAWER_HEADER_TAG)
    {
        return header_button_rect(self, hit.button);
    }
    struct drawer_filter_band band = filter_band(self);
    return hit.slot < band.shown ? band.chips[hit.slot] : (RECT){0, 0, 0, 0};
}

static struct drawer_header_hit header_hit_at(const struct drawer_window *_Nonnull self,
                                              POINT point)
{
    /* 配置は 1 回だけ作る（作るたびに札の字の幅を測る）。 */
    struct drawer_filter_band band = filter_band(self);
    for (size_t slot = 0; slot < band.shown; ++slot)
    {
        if (PtInRect(&band.chips[slot], point))
            return (struct drawer_header_hit){DRAWER_HEADER_TAG, slot};
    }
    const enum drawer_header_button buttons[] = {
        DRAWER_HEADER_FAVORITES, DRAWER_HEADER_MORE,          DRAWER_HEADER_TAG_FILTER,
        DRAWER_HEADER_MATCH_ALL, DRAWER_HEADER_MATCH_ANY,     DRAWER_HEADER_CLEAR,
        DRAWER_HEADER_SCOPE_ALL, DRAWER_HEADER_SCOPE_FILTERED};
    const RECT bounds[] = {band.favorites, band.more,  band.tags,      band.match_all,
                           band.match_any, band.clear, band.scope_all, band.scope_filtered};
    for (size_t at = 0; at < sizeof buttons / sizeof buttons[0]; ++at)
    {
        if (PtInRect(&bounds[at], point))
            return (struct drawer_header_hit){buttons[at], 0};
    }
    RECT fresh = new_note_rect(self);
    if (PtInRect(&fresh, point))
        return (struct drawer_header_hit){DRAWER_HEADER_NEW_NOTE, 0};
    RECT add = new_category_rect(self);
    return (struct drawer_header_hit){
        PtInRect(&add, point) ? DRAWER_HEADER_NEW_CATEGORY : DRAWER_HEADER_NONE, 0};
}

static enum drawer_header_button header_at(const struct drawer_window *_Nonnull self, POINT point)
{
    return header_hit_at(self, point).button;
}

static bool same_hit(struct drawer_header_hit left, struct drawer_header_hit right)
{
    return left.button == right.button && left.slot == right.slot;
}

/* 解除を押した点から動いていないあいだは、帯の押下を受けない。 */
static bool header_press_blocked(struct drawer_window *_Nonnull self, POINT point)
{
    if (self->header_blocked && point.x == self->header_block_point.x &&
        point.y == self->header_block_point.y)
    {
        return true;
    }
    self->header_blocked = false;
    return false;
}

static bool press_header(struct drawer_window *_Nonnull self, POINT point)
{
    if (header_press_blocked(self, point))
    {
        return true; /* 押下を消費する。行へも渡さない */
    }
    struct drawer_header_hit pressed = header_hit_at(self, point);
    if (pressed.button == DRAWER_HEADER_NONE)
    {
        return false;
    }
    if (pressed.button == match_button(folio_state_tag_match_mode(self->state)))
    {
        /* 選んでいる方は、押しても何も起きない。押下の見た目にもしない（ADR0060 の決定 4）。
         * focus だけ置く。 */
        SendMessageW(GetParent(self->handle), folio_message_index_focus,
                     (WPARAM)header_kind(pressed.button), 0);
        return true;
    }
    RECT button = header_hit_rect(self, pressed);
    self->header_pressed = pressed.button;
    self->header_pressed_slot = pressed.slot;
    self->header_pressed_revision = folio_state_tag_filter_revision(self->state);
    self->header_inside = true;
    SendMessageW(GetParent(self->handle), folio_message_index_focus,
                 (WPARAM)header_kind(pressed.button), (LPARAM)pressed.slot);
    SetCapture(self->handle);
    InvalidateRect(self->handle, &button, FALSE);
    return true;
}

static HRESULT accessible_layout(struct drawer_window *_Nonnull self);

static bool note_at(struct drawer_window *_Nonnull self, POINT point,
                    struct drawer_row *_Nonnull out)
{
    RECT client;
    GetClientRect(self->handle, &client);
    if (!PtInRect(&client, point) || point.y < band_height(self) || FAILED(accessible_layout(self)))
        return false;
    size_t index = 0;
    bool found = drawer_layout_hit(self->accessible_layout, point.y, &index);
    struct drawer_row row =
        found ? drawer_layout_row(self->accessible_layout, index) : (struct drawer_row){0};
    if (!found || row.kind != DRAWER_ROW_NOTE)
        return false;
    enum folio_cursor_kind kind = FOLIO_CURSOR_CATEGORY;
    struct note_ref cursor = {0};
    row.cursor = folio_state_cursor(self->state, &kind, &cursor) && kind == FOLIO_CURSOR_NOTE &&
                 same_note(row, cursor);
    *out = row;
    return true;
}

static bool favorite_hit(struct drawer_window *_Nonnull self, POINT point,
                         struct drawer_row *_Nonnull out)
{
    if (!note_at(self, point, out) || !favorite_shown(self, *out))
        return false;
    RECT bounds = favorite_rect(self, *out);
    return PtInRect(&bounds, point) != 0;
}

static bool press_favorite(struct drawer_window *_Nonnull self, POINT point)
{
    struct drawer_row row = {0};
    if (!note_at(self, point, &row))
        return false;
    RECT bounds = favorite_rect(self, row);
    if (!PtInRect(&bounds, point))
        return false;
    if (self->favorite_blocked)
        return true;
    if (!favorite_shown(self, row))
        return false;
    self->favorite_pressed = true;
    self->favorite_target = (struct note_ref){row.category, row.note};
    self->favorite_revision = folio_state_index_revision(self->state);
    self->favorite_bounds = bounds;
    SetCapture(self->handle);
    InvalidateRect(self->handle, &bounds, FALSE);
    return true;
}

static void release_favorite(struct drawer_window *_Nonnull self, POINT point)
{
    struct note_ref pressed = self->favorite_target;
    uint64_t revision = self->favorite_revision;
    struct drawer_row row = {0};
    bool hit = favorite_hit(self, point, &row);
    RECT bounds = favorite_rect(self, row);
    bool activate = hit && revision == folio_state_index_revision(self->state) &&
                    same_note(row, pressed) && EqualRect(&bounds, &self->favorite_bounds);
    self->favorite_pressed = false;
    ReleaseCapture();
    if (!activate)
        return;
    self->favorite_blocked = true;
    self->favorite_block_point = point;
    self->note_hover = false;
    SendMessageW(GetParent(self->handle), folio_message_drawer_menu, DRAWER_MENU_TOGGLE_FAVORITE,
                 (LPARAM)&pressed);
}

static void press(struct drawer_window *_Nonnull self, POINT point)
{
    if (self->pressed || self->favorite_pressed || self->header_pressed != DRAWER_HEADER_NONE)
    {
        return;
    }
    if (press_favorite(self, point) || press_header(self, point))
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
    if (point.x != self->header_block_point.x || point.y != self->header_block_point.y)
    {
        /* 押した点から動いたら、続けて押すのを止める守りを外す（戻ってきて押せる・#1247）。 */
        self->header_blocked = false;
    }
    struct drawer_header_hit over = header_hit_at(self, point);
    struct drawer_header_hit pressed = {self->header_pressed, self->header_pressed_slot};
    struct drawer_header_hit hover = {self->header_hover, self->header_hover_slot};
    if (pressed.button != DRAWER_HEADER_NONE)
    {
        bool inside = same_hit(over, pressed);
        if (inside != self->header_inside)
        {
            self->header_inside = inside;
            RECT button = header_hit_rect(self, pressed);
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
    if (!same_hit(over, hover))
    {
        RECT previous = header_hit_rect(self, hover);
        RECT current = header_hit_rect(self, over);
        self->header_hover = over.button;
        self->header_hover_slot = over.slot;
        InvalidateRect(self->handle, &previous, FALSE);
        InvalidateRect(self->handle, &current, FALSE);
    }
}

static void clear_note_hover(struct drawer_window *_Nonnull self)
{
    if (self->note_hover)
        InvalidateRect(self->handle, &self->hover_bounds, FALSE);
    self->note_hover = false;
}

static void leave_header(struct drawer_window *_Nonnull self)
{
    self->header_tracking = false;
    clear_note_hover(self);
    if (self->header_hover != DRAWER_HEADER_NONE)
    {
        RECT bounds = header_hit_rect(
            self, (struct drawer_header_hit){self->header_hover, self->header_hover_slot});
        self->header_hover = DRAWER_HEADER_NONE;
        self->header_hover_slot = 0;
        InvalidateRect(self->handle, &bounds, FALSE);
    }
}

/* Tablet PC署名のpen/touch合成moveはhoverにしない。実座標の変化だけで連続押下の抑止を解く。 */
static void track_note(struct drawer_window *_Nonnull self, POINT point)
{
    if (point.x != self->favorite_block_point.x || point.y != self->favorite_block_point.y)
        self->favorite_blocked = false;
    struct drawer_row row = {0};
    bool pen = ((ULONG_PTR)GetMessageExtraInfo() & 0xFFFFFF00u) == 0xFF515700u;
    if (pen || self->favorite_blocked || self->pressed || !note_at(self, point, &row))
    {
        clear_note_hover(self);
        return;
    }
    if (hovered_note(self, row))
        return;
    clear_note_hover(self);
    self->note_hover = true;
    self->hover_target = (struct note_ref){row.category, row.note};
    self->hover_revision = folio_state_index_revision(self->state);
    self->hover_bounds = favorite_rect(self, row);
    InvalidateRect(self->handle, &self->hover_bounds, FALSE);
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

/* 札・開く札・解除は、キーボード・読み上げと同じ 1 本の実行の道を通す。タグの札は、押した
 * ときの番号と改訂で指す（離すまでに条件が変わっていたら、何も外れない）。 */
static void invoke_header(struct drawer_window *_Nonnull self, enum drawer_header_button button)
{
    struct accessible_request request = {.item = {.kind = header_kind(button)},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    if (button == DRAWER_HEADER_MATCH_ALL || button == DRAWER_HEADER_MATCH_ANY ||
        button == DRAWER_HEADER_SCOPE_ALL || button == DRAWER_HEADER_SCOPE_FILTERED)
    {
        request.action = ACCESSIBLE_ACTION_SELECT; /* 2 択は選ぶ（選んでいる方は何もしない） */
    }
    if (button == DRAWER_HEADER_TAG)
    {
        request.item.slot = self->header_pressed_slot;
        request.item.revision = self->header_pressed_revision;
    }
    (void)accessibility_host_action(self->accessibility, &request);
}

static void run_header(struct drawer_window *_Nonnull self, enum drawer_header_button button)
{
    struct note_ref target = {0};
    switch (button)
    {
    case DRAWER_HEADER_NONE:
        return;
    case DRAWER_HEADER_NEW_NOTE: /* キーボード・読み上げと同じ「新しいノート」の道（#1291） */
    case DRAWER_HEADER_FAVORITES:
    case DRAWER_HEADER_TAG:
    case DRAWER_HEADER_MORE:
    case DRAWER_HEADER_TAG_FILTER:
    case DRAWER_HEADER_MATCH_ALL:
    case DRAWER_HEADER_MATCH_ANY:
    case DRAWER_HEADER_SCOPE_ALL:
    case DRAWER_HEADER_SCOPE_FILTERED:
    case DRAWER_HEADER_CLEAR:
        invoke_header(self, button);
        return;
    case DRAWER_HEADER_NEW_CATEGORY:
        SendMessageW(GetParent(self->handle), folio_message_drawer_menu, DRAWER_MENU_NEW_CATEGORY,
                     (LPARAM)&target);
        return;
    }
}

static void release(struct drawer_window *_Nonnull self, POINT point)
{
    if (self->favorite_pressed)
    {
        release_favorite(self, point);
        return;
    }
    enum drawer_header_button pressed = self->header_pressed;
    if (pressed == DRAWER_HEADER_NONE)
    {
        release_row(self);
        return;
    }
    self->header_pressed = DRAWER_HEADER_NONE;
    RECT button =
        header_hit_rect(self, (struct drawer_header_hit){pressed, self->header_pressed_slot});
    ReleaseCapture();
    InvalidateRect(self->handle, &button, FALSE);
    if (PtInRect(&button, point))
    {
        self->header_hover = DRAWER_HEADER_NONE;
        /* タグの札を外すと、隣の札（最後の 1 つなら開く札）が同じ場所へ来る。解除と同じ守り。 */
        self->header_blocked = pressed == DRAWER_HEADER_CLEAR || pressed == DRAWER_HEADER_TAG;
        self->header_block_point = point;
        run_header(self, pressed);
    }
}

static void cancel_favorite_press(struct drawer_window *_Nonnull self)
{
    clear_note_hover(self);
    if (self->favorite_pressed)
    {
        self->favorite_pressed = false;
        ReleaseCapture();
    }
}

/* スクロールの意図を出し、結果を写す（ADR 0009 の決定 8）。
 * ドラッグ中は行が動いたので、同じポインタ位置で落とし先を引き直す（決定 6）。 */
static void scroll_by(struct drawer_window *_Nonnull self, int delta)
{
    cancel_favorite_press(self);
    enum folio_state_outcome outcome =
        folio_state_scroll_drawer(self->state, metrics_for(self), delta);
    if (outcome != FOLIO_STATE_READY)
    {
        SendMessageW(GetParent(self->handle), folio_message_report_outcome, (WPARAM)outcome, 0);
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
    self->favorite_pressed = false;
    self->note_hover = false;
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

static enum ui_pointer_kind drawer_cursor(struct drawer_window *_Nonnull self, POINT point);

static LRESULT drawer_pointer_message(struct drawer_window *_Nonnull self, UINT message,
                                      WPARAM wparam, LPARAM lparam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        press(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        return 0;
    case WM_MOUSEMOVE:
        track_note(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        track_header(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)});
        drag(self, GET_Y_LPARAM(lparam));
        ui_pointer_show(drawer_cursor(self, (POINT){GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)}));
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
    default:
        /* Win32のメッセージは開いた集合。 */
        return DefWindowProcW(self->handle, message, wparam, lparam);
    }
}

/* 出すたびに文を組む説明（「新しいノート」の作る先・#1291）は、主窓が答える。主窓へ回すのは
 * その問い合わせ（TTN_GETDISPINFOW）だけで、ほかの知らせは既定の処理へ渡す。 */
static LRESULT tooltip_notice(const struct drawer_window *_Nonnull self, WPARAM wparam,
                              LPARAM lparam)
{
    const NMHDR *_Nonnull header = (const NMHDR *)lparam;
    if (header->code != TTN_GETDISPINFOW)
    {
        return DefWindowProcW(self->handle, WM_NOTIFY, wparam, lparam);
    }
    return SendMessageW(GetParent(self->handle), WM_NOTIFY, wparam, lparam);
}

static LRESULT drawer_message(struct drawer_window *_Nonnull self, UINT message, WPARAM wparam,
                              LPARAM lparam)
{
    switch (message)
    {
    case WM_SETCURSOR:
        if (!ui_pointer_client(self->handle, wparam, lparam))
            return DefWindowProcW(self->handle, message, wparam, lparam);
        ui_pointer_show(drawer_cursor(self, ui_pointer_message_point(self->handle)));
        return TRUE;
    case WM_NCHITTEST:
        if (SendMessageW(GetParent(self->handle), folio_message_resize_through, 0, lparam) != 0)
        {
            return HTTRANSPARENT;
        }
        return DefWindowProcW(self->handle, message, wparam, lparam);
    case WM_PAINT:
        paint(self);
        return 0;
    case WM_SIZE:
        /* 幅依存の検索枠・右寄せ操作・fadeを古い位置に残さない（#475）。 */
        cancel(self);
        if (GetCapture() == self->handle)
            ReleaseCapture();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NOTIFY:
        return tooltip_notice(self, wparam, lparam);
    case WM_DPICHANGED_AFTERPARENT:
        cancel(self);
        if (GetCapture() == self->handle)
            ReleaseCapture();
        refresh_fonts(self, GetDpiForWindow(self->handle));
        InvalidateRect(self->handle, nullptr, FALSE);
        return 0;
    case WM_DESTROY:
        release_fonts(self);
        self->handle = nullptr;
        return 0;
    default:
        /* Win32 のメッセージは開いた集合（C-017）。 */
        return drawer_pointer_message(self, message, wparam, lparam);
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
    LRESULT result = drawer_message(self, message, wparam, lparam);
    ui_pointer_after(window, message);
    return result;
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
    cancel_favorite_press(drawer);
    enum folio_state_outcome outcome =
        folio_state_reveal_cursor(drawer->state, metrics_for(drawer));
    if (outcome != FOLIO_STATE_READY)
    {
        SendMessageW(GetParent(drawer->handle), folio_message_report_outcome, (WPARAM)outcome, 0);
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
    int band = band_height(self);
    if (self->accessible_layout == nullptr || revision == UINT64_MAX ||
        self->accessible_revision != revision || self->accessible_dpi != dpi ||
        self->accessible_height != client.bottom || self->accessible_band != band)
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
        self->accessible_band = band;
    }
    drawer_layout_scroll(self->accessible_layout, folio_state_drawer_scroll(self->state));
    return S_OK;
}

static enum ui_pointer_kind drawer_cursor(struct drawer_window *_Nonnull self, POINT point)
{
    RECT client;
    GetClientRect(self->handle, &client);
    if (!IsWindowEnabled(self->handle) || !IsWindowEnabled(GetAncestor(self->handle, GA_ROOT)) ||
        !PtInRect(&client, point) || (self->pressed && self->moved) ||
        SendMessageW(GetParent(self->handle), folio_message_pointer_blocked, 0, 0) != 0)
        return UI_POINTER_ARROW;
    if (header_at(self, point) != DRAWER_HEADER_NONE)
        return UI_POINTER_ACTION;
    if (self->header_pressed != DRAWER_HEADER_NONE || point.y < band_height(self) ||
        FAILED(accessible_layout(self)))
        return UI_POINTER_ARROW;
    size_t at = 0;
    if (!drawer_layout_hit(self->accessible_layout, point.y, &at))
        return UI_POINTER_ARROW;
    struct drawer_row row = drawer_layout_row(self->accessible_layout, at);
    return row_action_available(self, row.kind) ? UI_POINTER_ACTION : UI_POINTER_ARROW;
}

/* 台帳番号は表示順とは限らない。カテゴリ内を実際の表示行で照合する（ADR0090）。 */
static bool accessible_row_index(const struct drawer_window *_Nonnull self,
                                 const struct accessible_item *_Nonnull item, size_t *_Nonnull out)
{
    size_t count = drawer_layout_row_count(self->accessible_layout);
    for (size_t index = 0; index < count; ++index)
    {
        struct drawer_row row = drawer_layout_row(self->accessible_layout, index);
        if (row.category > item->ref.category)
            break;
        bool category = row.kind == DRAWER_ROW_CATEGORY;
        if (row.category == item->ref.category && category == (item->kind == ACCESSIBLE_CATEGORY) &&
            (category || row.note == item->ref.note))
        {
            *out = index;
            return true;
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
    if (row.kind == DRAWER_ROW_NOTE)
    {
        out->favorite =
            folio_state_note_favorite(self->state, (struct note_ref){row.category, row.note});
        const char *status =
            ui_text_line(out->favorite ? UI_TEXT_NOTE_FAVORITE_ON : UI_TEXT_NOTE_FAVORITE_OFF,
                         folio_state_language(self->state));
        size_t written = 0;
        (void)utf16_text_fill(status, out->status, sizeof out->status / sizeof out->status[0],
                              &written);
    }
    out->focusable = true;
    struct note_ref selected;
    out->selected = row.kind == DRAWER_ROW_NOTE && folio_state_selection(self->state, &selected) &&
                    selected.category == row.category && selected.note == row.note;
}

/* 絞り込みの帯の部品の矩形を、1 回作った配置から引く。帯の部品でなければ偽。 */
static bool filter_part_rect(const struct drawer_window *_Nonnull self, enum accessible_kind kind,
                             RECT *_Nonnull out)
{
    const enum accessible_kind kinds[] = {
        ACCESSIBLE_FILTER_GROUP,      ACCESSIBLE_FILTER_FAVORITES,
        ACCESSIBLE_TAG_FILTER,        ACCESSIBLE_FILTER_MORE,
        ACCESSIBLE_FILTER_MATCH_ALL,  ACCESSIBLE_FILTER_MATCH_ANY,
        ACCESSIBLE_FILTER_CLEAR,      ACCESSIBLE_FILTER_MATCH_GROUP,
        ACCESSIBLE_SEARCH_SCOPE_ALL,  ACCESSIBLE_SEARCH_SCOPE_FILTERED,
        ACCESSIBLE_SEARCH_SCOPE_GROUP};
    size_t at = 0;
    while (at < sizeof kinds / sizeof kinds[0] && kinds[at] != kind)
    {
        ++at;
    }
    if (at == sizeof kinds / sizeof kinds[0])
    {
        return false; /* 帯の部品でなければ、配置を作らない（作るたびに字の幅を測る） */
    }
    struct drawer_filter_band band = filter_band(self);
    const RECT rects[] = {
        band.group,
        band.favorites,
        band.tags,
        band.more,
        band.match_all,
        band.match_any,
        band.clear,
        {band.match_all.left, band.match_all.top, band.match_any.right, band.match_any.bottom},
        band.scope_all,
        band.scope_filtered,
        /* 群は、見出し（あれば）から「絞り込みの中」の右端まで */
        {IsRectEmpty(&band.scope_label) ? band.scope_all.left : band.scope_label.left,
         band.scope_all.top, band.scope_filtered.right, band.scope_filtered.bottom}};
    *out = rects[at];
    return true;
}

static RECT accessible_fixed_rect(const struct drawer_window *_Nonnull self,
                                  enum accessible_kind kind)
{
    RECT part = {0, 0, 0, 0};
    if (filter_part_rect(self, kind, &part))
    {
        return part;
    }
    if (kind == ACCESSIBLE_NEW_CATEGORY)
    {
        return new_category_rect(self);
    }
    if (kind == ACCESSIBLE_NEW_NOTE)
    {
        return new_note_rect(self);
    }
    if (kind == ACCESSIBLE_INDEX_COUNT)
    {
        /* 見出しの字の左端から、数の右端（「＋」の左 8）まで。カテゴリの段の高さ。 */
        UINT dpi = GetDpiForWindow(self->handle);
        return (RECT){scale(base_header_indent, dpi),
                      band_height(self) - scale(base_category_band_height, dpi),
                      new_category_rect(self).left - scale(8, dpi), band_height(self)};
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
    if (drawer_layout_row_count(self->accessible_layout) == 0)
        (void)wide_units(ui_text_line(empty_message(self), folio_state_language(self->state)),
                         out->help);
}

/* タグの札の矩形。前の条件の札と、置かれていない番号（畳んだ分）は答えない（#1247）。 */
static bool accessible_tag_rect(const struct drawer_window *_Nonnull self,
                                const struct accessible_item *_Nonnull item, RECT *_Nonnull out)
{
    if (item->revision != folio_state_tag_filter_revision(self->state) ||
        item->slot >= tag_condition_count(self))
    {
        return false; /* 配置を作る（字の幅を測る）前に分かる */
    }
    struct drawer_filter_band band = filter_band(self);
    if (item->slot >= band.shown)
    {
        return false;
    }
    *out = band.chips[item->slot];
    return true;
}

size_t drawer_window_filter_tags(const struct drawer_window *_Nonnull self, size_t *_Nonnull hidden)
{
    struct drawer_filter_band band = filter_band(self);
    *hidden = band.hidden;
    return band.shown;
}

/* 要素の矩形（行は行の中身も）を書く。前の改訂の行・札と、置かれていない札は答えない。 */
static bool accessible_item_place(struct drawer_window *_Nonnull self,
                                  const struct accessible_item *_Nonnull item,
                                  struct accessible_view *_Nonnull out)
{
    if (item->kind == ACCESSIBLE_FILTER_TAG)
    {
        return accessible_tag_rect(self, item, &out->bounds);
    }
    if (!accessible_item_row(*item))
    {
        out->bounds = accessible_fixed_rect(self, item->kind);
        return true;
    }
    size_t index = 0;
    if (item->revision == UINT64_MAX || item->revision != folio_state_row_revision(self->state) ||
        !accessible_row_index(self, item, &index))
    {
        return false;
    }
    accessible_row_view(self, drawer_layout_row(self->accessible_layout, index), out);
    return true;
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
    if (!accessible_item_place(self, item, out))
    {
        return accessibility_node_unavailable;
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
    struct drawer_header_hit hit = header_hit_at(self, point);
    *out = (struct accessible_item){.kind = ACCESSIBLE_DRAWER};
    if (hit.button != DRAWER_HEADER_NONE)
    {
        out->kind = header_kind(hit.button);
        if (hit.button == DRAWER_HEADER_TAG)
        {
            out->slot = hit.slot;
            out->revision = folio_state_tag_filter_revision(self->state);
        }
        return S_OK;
    }
    RECT bounds = filter_rect(self);
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
    cancel_favorite_press(self);
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
