#include "request_prompt.h"
#include "folio_message.h"
#include "modal_owner.h"

#include "dialog_theme.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"

constexpr INT_PTR prompt_not_built = -2;
constexpr int keep_identity = 3; /* 名前入力面の「保存しない」と同じ、IDOK・IDCANCEL でない番号 */
constexpr size_t choice_count = 3;
constexpr size_t line_units = ui_text_unit_limit * 4;
/* 面が出た直後は、鍵を釦へ通さない。本文を打っている最中に面が出ると、打っていた字（空白・Enter・
 * S / K / N）が、そのまま答えになってしまうため。鍵が止まってからこの時間が過ぎたら、受ける。
 * マウスと Alt+字は、はじめから効く。 */
constexpr UINT_PTR grace_timer = 1;
constexpr UINT grace_millis = 700;

struct request_prompt
{
    const struct request_prompt_request *_Nonnull request;
    enum folio_language language;
    struct dialog_theme *_Nullable theme;
    HFONT _Nullable font;
    HWND _Nullable dialog;
    HWND _Nullable message;
    HWND _Nullable buttons[choice_count];
    WNDPROC _Nullable originals[choice_count]; /* 釦の元の手続き（鍵の見張りの先） */
    bool armed;                                /* 鍵を受けはじめた */
    HWND _Nullable hints[choice_count];
    HWND _Nullable close;
    UINT dpi;
};

static const struct
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WORD title;
} template = {.dialog = {.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                         .cx = 220,
                         .cy = 140}};

/* 縦に並ぶ 3 つ。添字は buttons / hints と同じ。 */
static const int identities[choice_count] = {IDOK, keep_identity, IDCANCEL};
static const enum ui_text labels[choice_count] = {
    UI_TEXT_REQUEST_PROMPT_SAVE, UI_TEXT_REQUEST_PROMPT_KEEP, UI_TEXT_REQUEST_PROMPT_DECLINE};
static const enum ui_text explanations[choice_count] = {UI_TEXT_REQUEST_PROMPT_SAVE_HINT,
                                                        UI_TEXT_REQUEST_PROMPT_KEEP_HINT,
                                                        UI_TEXT_REQUEST_PROMPT_DECLINE_HINT};

static int scaled(const struct request_prompt *_Nonnull prompt, int value)
{
    return MulDiv(value, (int)prompt->dpi, 96);
}

static void position(HWND _Nullable window, RECT bounds)
{
    MoveWindow(window, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

/* 文言の表の 1 行に、依頼の 3 つの名前を入れて UTF-16 にする。 */
static bool line(const struct request_prompt *_Nonnull prompt, enum ui_text id,
                 char16_t *_Nonnull out)
{
    char text[line_units];
    size_t written = 0;
    struct ui_text_request request = {.id = id,
                                      .language = prompt->language,
                                      .name = prompt->request->requested,
                                      .from = prompt->request->current,
                                      .to = prompt->request->kept};
    return ui_text_format(&request, text, sizeof text) == UI_TEXT_FORMAT_READY &&
           utf16_text_fill(text, out, line_units, &written) == UTF16_TEXT_FILL_READY;
}

static bool make_font(struct request_prompt *_Nonnull prompt)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(prompt->language, face);
    HFONT font = CreateFontW(-scaled(prompt, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    if (font == nullptr)
        return false;
    HFONT previous = prompt->font;
    prompt->font = font;
    for (HWND child = GetWindow(prompt->dialog, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, (WPARAM)font, FALSE);
    if (previous != nullptr)
        DeleteObject(previous);
    return true;
}

static HWND _Nullable label(const struct request_prompt *_Nonnull prompt, enum ui_text id)
{
    char16_t units[line_units];
    if (!line(prompt, id, units))
        return nullptr;
    return CreateWindowExW(0, L"STATIC", units,
                           WS_CHILD | WS_VISIBLE | SS_NOPREFIX | SS_EDITCONTROL, 0, 0, 0, 0,
                           prompt->dialog, nullptr, GetModuleHandleW(nullptr), nullptr);
}

/* 釦の元の手続き。この面の釦でなければ無い。 */
static WNDPROC _Nullable original_of(const struct request_prompt *_Nullable prompt, HWND button)
{
    for (size_t at = 0; prompt != nullptr && at < choice_count; ++at)
        if (prompt->buttons[at] == button)
            return prompt->originals[at];
    return nullptr;
}

static bool key_message(UINT message)
{
    return message == WM_KEYDOWN || message == WM_KEYUP || message == WM_CHAR;
}

/* 面が出た直後の鍵を、釦の手前で捨てる。捨てるたびに、待ちを最初から数え直す。 */
static LRESULT CALLBACK guarded_button(HWND button, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct request_prompt *_Nullable prompt =
        (struct request_prompt *)GetWindowLongPtrW(GetParent(button), DWLP_USER);
    WNDPROC original = original_of(prompt, button);
    if (prompt == nullptr || original == nullptr)
        return DefWindowProcW(button, message, wparam, lparam);
    if (prompt->armed || (message != WM_GETDLGCODE && !key_message(message)))
        return CallWindowProcW(original, button, message, wparam, lparam);
    if (message == WM_GETDLGCODE)
        return DLGC_WANTALLKEYS;
    SetTimer(prompt->dialog, grace_timer, grace_millis, nullptr);
    return 0;
}

static bool guard_button(struct request_prompt *_Nonnull prompt, size_t at)
{
    prompt->originals[at] =
        (WNDPROC)SetWindowLongPtrW(prompt->buttons[at], GWLP_WNDPROC, (LONG_PTR)guarded_button);
    return prompt->originals[at] != nullptr;
}

static bool make_choice(struct request_prompt *_Nonnull prompt, size_t at)
{
    char16_t units[line_units];
    if (!line(prompt, labels[at], units))
        return false;
    DWORD style = WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW | (at == 0 ? WS_GROUP : 0);
    prompt->buttons[at] =
        CreateWindowExW(0, L"BUTTON", units, style, 0, 0, 0, 0, prompt->dialog,
                        (HMENU)(INT_PTR)identities[at], GetModuleHandleW(nullptr), nullptr);
    prompt->hints[at] = label(prompt, explanations[at]);
    return prompt->buttons[at] != nullptr && prompt->hints[at] != nullptr &&
           dialog_theme_track_control(prompt->buttons[at], DIALOG_POINTER_BUTTON) ==
               DIALOG_THEME_CONTROL_READY &&
           guard_button(prompt, at);
}

static bool make_controls(struct request_prompt *_Nonnull prompt)
{
    prompt->message = label(prompt, UI_TEXT_REQUEST_PROMPT_MESSAGE);
    if (prompt->message == nullptr)
        return false;
    for (size_t at = 0; at < choice_count; ++at)
        if (!make_choice(prompt, at))
            return false;
    if (dialog_theme_create_close(prompt->dialog, prompt->font, prompt->language, &prompt->close) !=
        DIALOG_THEME_CONTROL_READY)
        return false;
    for (HWND child = GetWindow(prompt->dialog, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, (WPARAM)prompt->font, FALSE);
    return true;
}

/* 折り返した文の高さ。 */
static int text_height(const struct request_prompt *_Nonnull prompt, HWND _Nullable text, int width)
{
    wchar_t units[line_units];
    GetWindowTextW(text, units, (int)line_units);
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
        return 0;
    HGDIOBJ previous = SelectObject(device, prompt->font);
    RECT bounds = {0, 0, width, 0};
    DrawTextW(device, units, -1, &bounds,
              DT_CALCRECT | DT_WORDBREAK | DT_EDITCONTROL | DT_NOPREFIX);
    SelectObject(device, previous);
    ReleaseDC(prompt->dialog, device);
    return bounds.bottom;
}

/* 上から: 題の段、知らせ、（釦、その説明）× 3。幅は 440 までで、主窓の中に収める。 */
static bool lay_out(struct request_prompt *_Nonnull prompt, HWND _Nullable anchor)
{
    int edge = scaled(prompt, 16);
    RECT wanted =
        dialog_theme_popup_bounds(anchor, (SIZE){scaled(prompt, 440), scaled(prompt, 200)});
    int width = wanted.right - wanted.left;
    int inner = width - edge * 2;
    int top = scaled(prompt, ui_part_surface_title_height + 12);
    int height = text_height(prompt, prompt->message, inner);
    if (inner <= 0 || height <= 0)
        return false;
    position(prompt->message, (RECT){edge, top, edge + inner, top + height});
    top += height + scaled(prompt, 14);
    for (size_t at = 0; at < choice_count; ++at)
    {
        position(prompt->buttons[at], (RECT){edge, top, edge + inner, top + scaled(prompt, 30)});
        top += scaled(prompt, 30 + 4);
        height = text_height(prompt, prompt->hints[at], inner);
        position(prompt->hints[at], (RECT){edge, top, edge + inner, top + height});
        top += height + scaled(prompt, 12);
    }
    SIZE size = {width, top + scaled(prompt, 4)};
    position(prompt->dialog, dialog_theme_popup_bounds(anchor, size));
    position(prompt->close, ui_part_surface_rect((RECT){0, 0, size.cx, size.cy}, prompt->dpi,
                                                 UI_PART_SURFACE_CLOSE));
    return true;
}

static bool initialize(struct request_prompt *_Nonnull prompt, HWND dialog)
{
    char16_t title[line_units];
    prompt->dialog = dialog;
    prompt->dpi = GetDpiForWindow(dialog);
    struct folio_palette palette = folio_palette_from(folio_state_palette(prompt->request->state));
    switch (dialog_theme_create(&palette, &prompt->theme))
    {
    case DIALOG_THEME_READY:
        break;
    case DIALOG_THEME_NO_MEMORY:
    case DIALOG_THEME_NO_BRUSH:
        return false;
    }
    dialog_theme_decorate(prompt->theme, dialog);
    if (!line(prompt, UI_TEXT_REQUEST_PROMPT_TITLE, title))
        return false;
    SetWindowTextW(dialog, title);
    if (!make_font(prompt) || !make_controls(prompt) || !lay_out(prompt, modal_owner_root(dialog)))
        return false;
    SetFocus(prompt->buttons[0]);
    if (SetTimer(dialog, grace_timer, grace_millis, nullptr) == 0)
        prompt->armed = true; /* 待ちを数えられなければ、止めたままにしない */
    NotifyWinEvent(EVENT_SYSTEM_ALERT, dialog, OBJID_WINDOW, CHILDID_SELF);
    return true;
}

static void paint_prompt(const struct request_prompt *_Nonnull prompt)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(prompt->dialog, &paint);
    if (device != nullptr)
        dialog_theme_draw_title(prompt->theme, prompt->dialog, device, prompt->font);
    EndPaint(prompt->dialog, &paint);
}

static void follow_dpi(struct request_prompt *_Nonnull prompt, const RECT *_Nonnull suggested)
{
    SetWindowPos(prompt->dialog, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    prompt->dpi = GetDpiForWindow(prompt->dialog);
    if (!make_font(prompt) || !lay_out(prompt, prompt->dialog))
        EndDialog(prompt->dialog, prompt_not_built);
    RedrawWindow(prompt->dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

/* 説明の行は案内の色、知らせは字の色。 */
static INT_PTR label_color(const struct request_prompt *_Nonnull prompt, HDC device, HWND control)
{
    enum dialog_theme_surface surface =
        control == prompt->message ? DIALOG_THEME_LABEL : DIALOG_THEME_SECONDARY;
    return (INT_PTR)dialog_theme_color(prompt->theme, surface, device);
}

static void draw_button(const struct request_prompt *_Nonnull prompt,
                        const DRAWITEMSTRUCT *_Nonnull item)
{
    enum dialog_theme_button kind = DIALOG_THEME_BUTTON_SECONDARY;
    if (item->CtlID == IDCLOSE)
        kind = DIALOG_THEME_BUTTON_CLOSE;
    else if (item->CtlID == IDOK)
        kind = DIALOG_THEME_BUTTON_PRIMARY;
    dialog_theme_draw_button(prompt->theme, item, kind);
}

static INT_PTR paint_message(struct request_prompt *_Nonnull prompt, UINT message, WPARAM wparam,
                             LPARAM lparam)
{
    if (prompt->theme == nullptr)
        return FALSE;
    switch (message)
    {
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        dialog_theme_follow(prompt->dialog, prompt->request->state, &prompt->theme);
        return TRUE;
    case WM_CTLCOLORDLG:
        return (INT_PTR)dialog_theme_color(prompt->theme, DIALOG_THEME_DIALOG, (HDC)wparam);
    case WM_CTLCOLORSTATIC:
        return label_color(prompt, (HDC)wparam, (HWND)lparam);
    case WM_DRAWITEM:
        draw_button(prompt, (const DRAWITEMSTRUCT *)lparam);
        return TRUE;
    case WM_PAINT:
        paint_prompt(prompt);
        return TRUE;
    case WM_PRINTCLIENT:
        dialog_theme_draw_title(prompt->theme, prompt->dialog, (HDC)wparam, prompt->font);
        return TRUE;
    case WM_DPICHANGED:
        follow_dpi(prompt, (const RECT *)lparam);
        return TRUE;
    case folio_message_modal_reflow:
        if (!lay_out(prompt, prompt->dialog))
            EndDialog(prompt->dialog, prompt_not_built);
        return TRUE;
    default:
        return FALSE;
    }
}

/* owner-draw の釦は既定の釦にならないので、Enter はどの釦に鍵があっても IDOK で届く。鍵のある釦の
 * 番号へ読み替える（名前入力面と同じ）。×と Esc は「開かない」。 */
static int chosen(const struct request_prompt *_Nonnull prompt, UINT message, WPARAM wparam)
{
    if (message == WM_CLOSE)
        return IDCANCEL;
    if (message != WM_COMMAND)
        return 0;
    int identity = LOWORD(wparam);
    HWND focus = GetFocus();
    if (identity == IDCLOSE || focus == prompt->close)
        return IDCANCEL;
    if (identity != IDOK)
        return identity == keep_identity || identity == IDCANCEL ? identity : 0;
    if (focus == prompt->buttons[1])
        return keep_identity;
    return focus == prompt->buttons[2] ? IDCANCEL : IDOK;
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (dialog_theme_native_message(dialog, message, wparam, lparam))
        return TRUE;
    if (message == WM_INITDIALOG)
    {
        struct request_prompt *_Nonnull prompt = (struct request_prompt *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)prompt);
        if (!initialize(prompt, dialog))
            EndDialog(dialog, prompt_not_built);
        return FALSE;
    }
    struct request_prompt *_Nullable prompt =
        (struct request_prompt *)GetWindowLongPtrW(dialog, DWLP_USER);
    if (prompt == nullptr)
        return FALSE;
    if (message == folio_message_modal_cancel)
    {
        SetWindowLongPtrW(dialog, DWLP_MSGRESULT, 1);
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    if (message == WM_TIMER && wparam == grace_timer)
    {
        KillTimer(dialog, grace_timer);
        prompt->armed = true;
        return TRUE;
    }
    INT_PTR painted = paint_message(prompt, message, wparam, lparam);
    if (painted != FALSE)
        return painted;
    int identity = chosen(prompt, message, wparam);
    if (identity != 0)
        EndDialog(dialog, identity);
    return identity != 0;
}

enum request_prompt_choice
request_prompt_show(HWND _Nullable owner, const struct request_prompt_request *_Nonnull request)
{
    struct request_prompt prompt = {.request = request,
                                    .language = folio_state_language(request->state)};
    INT_PTR ended = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                            procedure, (LPARAM)&prompt);
    dialog_theme_destroy(prompt.theme);
    if (prompt.font != nullptr)
        DeleteObject(prompt.font);
    if (ended == IDOK)
        return REQUEST_PROMPT_SAVE;
    if (ended == keep_identity)
        return REQUEST_PROMPT_KEEP;
    return ended == IDCANCEL ? REQUEST_PROMPT_DECLINE : REQUEST_PROMPT_UNAVAILABLE;
}
