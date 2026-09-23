#include "failure_box.h"

#include "dialog_theme.h"
#include "failure_box_outcome.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "icon_paint.h"
#include "icon_paint_kind.h"
#include "ui_face.h"
#include "ui_text.h"
#include "utf16_text.h"

#include <string.h>

/* 寸法は 96 DPI の論理値で、開く瞬間の DPI で拡げる（ADR 0035 の決定 4）。 */
constexpr int box_margin = 20;
constexpr int box_gap = 16;
constexpr int box_line_limit = 480;
constexpr int box_button_width = 88;
constexpr int box_button_height = 28;
constexpr int box_font_height = 14;
/* 警告の印の箱は他の印と同じ 24px の正方形（ADR 0035 の補正 14）。 */
constexpr int box_mark_size = 24;
/* WM_INITDIALOG で面を組み立てられなかったことを EndDialog で返す値。
 * IDOK（閉じた）とも DialogBoxIndirectParamW の −1（面を作れなかった）とも重ならない。 */
constexpr INT_PTR box_not_built = -2;

struct failure_box
{
    const char16_t *_Nonnull units;
    enum folio_language language;
    /* palette は開く瞬間の描画テーマから写し、面は追随しない（ADR 0035 の決定 5）。 */
    enum folio_theme theme_choice;
    struct dialog_theme *_Nullable theme;
    HFONT _Nullable font;
    HWND _Nullable dialog;
    UINT dpi;
    /* 警告の印の箱（client 座標）と色。色は開く瞬間の palette の chip_background（補正 13）。 */
    RECT mark;
    COLORREF mark_color;
};

static const struct
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WORD title;
} template = {
    .dialog = {.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME, .cx = 200, .cy = 80}};

static int scaled(const struct failure_box *_Nonnull box, int value)
{
    return MulDiv(value, (int)box->dpi, 96);
}

/* 子を作って書体を渡す。置き場所は作った後に MoveWindow で決める（name_prompt と同じ）。 */
static HWND _Nullable child(struct failure_box *_Nonnull box, const wchar_t *_Nonnull kind,
                            const wchar_t *_Nonnull text, DWORD style)
{
    HWND window = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                                  box->dialog, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window != nullptr)
    {
        SendMessageW(window, WM_SETFONT, (WPARAM)box->font, TRUE);
    }
    return window;
}

static void position(HWND _Nonnull window, RECT bounds)
{
    MoveWindow(window, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

/* 本文の矩形（物理画素）。幅は min(480 − 印 − 隙間, 1 行で測った幅) に下限（釦 ＋ 余白 2 つ）、
 * 高さはその幅で折り返して測る（ADR 0035 の決定 4・補正 14）。line は 1 行の高さ。 */
static bool measure_body(const struct failure_box *_Nonnull box, SIZE *_Nonnull out,
                         int *_Nonnull line_height)
{
    HDC device = GetDC(box->dialog);
    if (device == nullptr)
    {
        return false;
    }
    HGDIOBJ old_font = SelectObject(device, box->font);
    RECT line = {0, 0, 0, 0};
    DrawTextW(device, box->units, -1, &line, DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX);
    int floor = scaled(box, box_button_width + 2 * box_margin);
    int limit = scaled(box, box_line_limit - box_mark_size - box_gap);
    int measured = (int)(line.right - line.left);
    int width = measured < limit ? measured : limit;
    width = width < floor ? floor : width;
    RECT wrapped = {0, 0, width, 0};
    DrawTextW(device, box->units, -1, &wrapped, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    SelectObject(device, old_font);
    ReleaseDC(box->dialog, device);
    /* 切れない長い語は DT_CALCRECT が右へ拡げるので、その幅を採る。 */
    int spread = (int)(wrapped.right - wrapped.left);
    out->cx = spread > width ? spread : width;
    out->cy = wrapped.bottom - wrapped.top;
    *line_height = line.bottom - line.top;
    return true;
}

/* owner の中央に置く。owner が無ければ窓のある画面の作業領域の中央。 */
static RECT center_of(HWND _Nonnull dialog)
{
    RECT area = {0, 0, 0, 0};
    HWND owner = GetWindow(dialog, GW_OWNER);
    if (owner != nullptr && GetWindowRect(owner, &area))
    {
        return area;
    }
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    if (GetMonitorInfoW(MonitorFromWindow(dialog, MONITOR_DEFAULTTOPRIMARY), &monitor))
    {
        area = monitor.rcWork;
    }
    return area;
}

static void place(const struct failure_box *_Nonnull box, SIZE client)
{
    RECT bounds = {0, 0, client.cx, client.cy};
    DWORD style = (DWORD)GetWindowLongPtrW(box->dialog, GWL_STYLE);
    AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, box->dpi);
    RECT area = center_of(box->dialog);
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    MoveWindow(box->dialog, (area.left + area.right - width) / 2,
               (area.top + area.bottom - height) / 2, width, height, FALSE);
}

static bool make_font(struct failure_box *_Nonnull box)
{
    /* 面はモーダルなので、言語は開く瞬間に決まる（ADR 0032 の決定 4）。 */
    wchar_t face[LF_FACESIZE];
    ui_face_for(box->language, face);
    box->font = CreateFontW(-scaled(box, box_font_height), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                            CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    return box->font != nullptr;
}

/* 本文と釦を作り、面を本文に合わせる。釦の字は ui_text の表から写す（C-018）。 */
static bool lay_out(struct failure_box *_Nonnull box)
{
    SIZE body;
    int line_height = 0;
    if (!measure_body(box, &body, &line_height))
    {
        return false;
    }
    int margin = scaled(box, box_margin);
    int gap = scaled(box, box_gap);
    int mark = scaled(box, box_mark_size);
    int button_width = scaled(box, box_button_width);
    int button_height = scaled(box, box_button_height);
    /* 印は本文の左に隙間を空けて置き、本文の 1 行目の高さの中央に揃える（補正 14）。 */
    int mark_top = margin + (line_height - mark) / 2;
    box->mark = (RECT){margin, mark_top, margin + mark, mark_top + mark};
    int text_left = margin + mark + gap;
    int content_bottom = margin + body.cy;
    content_bottom = content_bottom < box->mark.bottom ? box->mark.bottom : content_bottom;
    SIZE client = {text_left + body.cx + margin, content_bottom + gap + button_height + margin};
    place(box, client);
    HWND text = child(box, L"STATIC", box->units, SS_LEFT | SS_NOPREFIX);
    if (text == nullptr)
    {
        return false;
    }
    position(text, (RECT){text_left, margin, text_left + body.cx, margin + body.cy});
    char16_t label[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(ui_text_line(UI_TEXT_PROMPT_OK, box->language), label, ui_text_unit_limit,
                        &written) != UTF16_TEXT_FILL_READY)
    {
        return false;
    }
    int right = client.cx - margin;
    int bottom = client.cy - margin;
    /* 押し釦は WM_CTLCOLORBTN では塗れないので owner-draw にする（ADR 0035 の決定 3・4）。 */
    HWND accept = child(box, L"BUTTON", label, WS_TABSTOP | BS_OWNERDRAW);
    if (accept == nullptr)
    {
        return false;
    }
    position(accept, (RECT){right - button_width, bottom - button_height, right, bottom});
    SetWindowLongPtrW(accept, GWLP_ID, IDOK);
    SetFocus(accept);
    return true;
}

static bool initialize(struct failure_box *_Nonnull box, HWND dialog)
{
    box->dialog = dialog;
    box->dpi = GetDpiForWindow(dialog);
    /* 子を作る前に塗りを用意する。子の WM_CTLCOLOR* は作る途中から来る（ADR 0035 の決定 3）。 */
    struct folio_palette palette = folio_palette_for(box->theme_choice);
    box->mark_color = palette.chip_background;
    switch (dialog_theme_create(&palette, &box->theme))
    {
    case DIALOG_THEME_READY:
        break;
    case DIALOG_THEME_NO_MEMORY:
    case DIALOG_THEME_NO_BRUSH:
        return false;
    }
    dialog_theme_decorate(box->theme, dialog);
    SetWindowTextW(dialog, L"NeNe Folio");
    if (!make_font(box) || !lay_out(box))
    {
        return false;
    }
    /* 組み上がって見える直前に 1 回だけ鳴らす。鳴らなくても箱の働きは変わらないので戻り値は
     * 見ない（ADR 0035 の補正 12）。 */
    MessageBeep(MB_ICONWARNING);
    return true;
}

/* 塗りのメッセージを dialog_theme へ渡す。扱ったものは 0 でない値、扱わなければ FALSE。 */
static INT_PTR paint_message(struct failure_box *_Nonnull box, UINT message, WPARAM wparam,
                             LPARAM lparam)
{
    if (box->theme == nullptr)
    {
        return FALSE;
    }
    if (message == WM_CTLCOLORDLG)
    {
        return (INT_PTR)dialog_theme_color(box->theme, DIALOG_THEME_DIALOG, (HDC)wparam);
    }
    if (message == WM_CTLCOLORSTATIC)
    {
        return (INT_PTR)dialog_theme_color(box->theme, DIALOG_THEME_LABEL, (HDC)wparam);
    }
    if (message == WM_DRAWITEM)
    {
        /* 釦は 1 本で、それが既定（ADR 0035 の決定 4）。 */
        dialog_theme_draw_button(box->theme, (const DRAWITEMSTRUCT *)lparam,
                                 DIALOG_THEME_BUTTON_PRIMARY);
        return TRUE;
    }
    if (message == WM_PAINT)
    {
        /* 地は WM_CTLCOLORDLG のブラシで消えているので、印だけを描く（補正 13・14）。 */
        PAINTSTRUCT paint;
        HDC device = BeginPaint(box->dialog, &paint);
        if (device != nullptr)
        {
            icon_paint_fill(device, box->mark, ICON_PAINT_WARNING, box->mark_color);
        }
        EndPaint(box->dialog, &paint);
        return TRUE;
    }
    if (message == WM_DPICHANGED)
    {
        const RECT *_Nonnull suggested = (const RECT *)lparam;
        SetWindowPos(box->dialog, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return TRUE;
    }
    return FALSE;
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_INITDIALOG)
    {
        struct failure_box *_Nonnull box = (struct failure_box *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)box);
        /* 塗れない・作れない箱は閉じて、呼び出し側が OS の箱へ退避する（補正 3）。 */
        if (!initialize(box, dialog))
        {
            EndDialog(dialog, box_not_built);
        }
        return FALSE;
    }
    struct failure_box *_Nullable box = (struct failure_box *)GetWindowLongPtrW(dialog, DWLP_USER);
    if (box == nullptr)
    {
        return FALSE;
    }
    INT_PTR painted = paint_message(box, message, wparam, lparam);
    if (painted != FALSE)
    {
        return painted;
    }
    /* Enter・OK は IDOK、Esc・× は IDCANCEL で来る。どれも閉じるだけ（決定 4）。 */
    if (message == WM_CLOSE ||
        (message == WM_COMMAND && (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL)))
    {
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    return FALSE;
}

/* 自前のモーダルを出す。塗り・書体・DC・子窓のどれかを作れない（box_not_built）か、
 * 面そのものを作れない（−1）なら FALLBACK。閉じ方の区別は使わない（決定 4）。 */
[[nodiscard]] static enum failure_box_outcome show_themed(HWND _Nullable owner,
                                                          struct failure_box *_Nonnull box)
{
    INT_PTR ended = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                            procedure, (LPARAM)box);
    return ended == -1 || ended == box_not_built ? FAILURE_BOX_FALLBACK : FAILURE_BOX_SHOWN;
}

void failure_box_show(HWND _Nullable owner, enum folio_state_outcome outcome,
                      const struct folio_state *_Nonnull state)
{
    if (outcome == FOLIO_STATE_CANCELLED)
    {
        return;
    }
    enum folio_language language = folio_state_language(state);
    const char *_Nonnull line = folio_state_failure_line(outcome, language);
    struct utf16_text *_Nullable text = nullptr;
    if (utf16_text_create(line, strlen(line), &text) != UTF16_TEXT_CONVERTED)
    {
        return;
    }
    struct failure_box box = {.units = utf16_text_units(text),
                              .language = language,
                              .theme_choice = folio_state_theme(state)};
    enum failure_box_outcome shown = show_themed(owner, &box);
    /* ブラシと書体は面が返った直後に捨てる（ADR 0035 の決定 2）。 */
    dialog_theme_destroy(box.theme);
    if (box.font != nullptr)
    {
        DeleteObject(box.font);
    }
    switch (shown)
    {
    case FAILURE_BOX_SHOWN:
        break;
    case FAILURE_BOX_FALLBACK:
        /* 正典の箱と同じく警告の音と印を付ける（ADR 0035 の補正 12）。題は製品名で翻訳しない。 */
        MessageBoxW(owner, box.units, L"NeNe Folio", MB_OK | MB_ICONWARNING);
        break;
    }
    utf16_text_destroy(text);
}
