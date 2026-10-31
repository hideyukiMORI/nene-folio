#include "storage_prompt.h"
#include "accessibility_native.h"
#include "dialog_theme.h"
#include "folder_picker.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "icon_paint.h"
#include "modal_owner.h"
#include "storage_path.h"
#include "storage_state.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_text.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <string.h>

constexpr int browse_identity = 1201;
constexpr int default_identity = 1202;
constexpr int current_identity = 1203;
constexpr int input_identity = 1204;
constexpr INT_PTR unavailable_result = -2;
constexpr size_t path_unit_capacity = storage_path_byte_limit + 1;

struct storage_prompt
{
    const struct storage_prompt_request *_Nonnull request;
    struct dialog_theme *_Nullable theme;
    HWND _Nullable dialog;
    HWND _Nullable hint;
    HWND _Nullable current_label;
    HWND _Nullable current;
    HWND _Nullable input_label;
    HWND _Nullable input;
    HWND _Nullable browse;
    HWND _Nullable reset;
    HWND _Nullable status;
    HWND _Nullable accept;
    HWND _Nullable cancel;
    HWND _Nullable close;
    HFONT _Nullable font;
    WNDPROC _Nullable input_original;
    WNDPROC _Nullable current_original;
    RECT current_bounds;
    RECT input_bounds;
    UINT dpi;
    bool setup;
    bool composing;
};

static const struct
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WORD title;
} template = {.dialog = {.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                         .cx = 240,
                         .cy = 168}};

static int scaled(const struct storage_prompt *_Nonnull prompt, int value)
{
    return MulDiv(value, (int)prompt->dpi, 96);
}

static bool wide_line(const struct storage_prompt *_Nonnull prompt, enum ui_text id,
                      char16_t *_Nonnull out)
{
    size_t written = 0;
    return utf16_text_fill(ui_text_line(id, prompt->request->language), out, ui_text_unit_limit,
                           &written) == UTF16_TEXT_FILL_READY;
}

static HWND _Nullable control(struct storage_prompt *_Nonnull prompt, const wchar_t *_Nonnull kind,
                              enum ui_text text, DWORD style)
{
    char16_t units[ui_text_unit_limit];
    if (!wide_line(prompt, text, units))
        return nullptr;
    HWND window = CreateWindowExW(0, kind, units, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                                  prompt->dialog, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window != nullptr)
        SendMessageW(window, WM_SETFONT, (WPARAM)prompt->font, FALSE);
    return window;
}

static HWND _Nullable button(struct storage_prompt *_Nonnull prompt, enum ui_text text, int id)
{
    HWND window = control(prompt, L"BUTTON", text, WS_TABSTOP | BS_OWNERDRAW);
    if (window == nullptr)
        return nullptr;
    SetWindowLongPtrW(window, GWLP_ID, id);
    if (dialog_theme_track_button(window) != DIALOG_THEME_CONTROL_READY)
    {
        DestroyWindow(window);
        return nullptr;
    }
    return window;
}

static bool set_path(HWND window, const char *_Nonnull path)
{
    char16_t units[path_unit_capacity];
    size_t written = 0;
    return utf16_text_fill(path, units, path_unit_capacity, &written) == UTF16_TEXT_FILL_READY &&
           SetWindowTextW(window, units);
}

static SIZE text_size(const struct storage_prompt *_Nonnull prompt, HWND window, int width)
{
    wchar_t text[ui_text_unit_limit];
    int count = GetWindowTextW(window, text, (int)ui_text_unit_limit);
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
        return (SIZE){scaled(prompt, 160), scaled(prompt, 48)};
    HGDIOBJ previous = SelectObject(device, prompt->font);
    RECT bounds = {0, 0, width, 0};
    DrawTextW(device, text, count, &bounds,
              DT_CALCRECT | DT_NOPREFIX | (width > 0 ? DT_WORDBREAK : DT_SINGLELINE));
    SelectObject(device, previous);
    ReleaseDC(prompt->dialog, device);
    return (SIZE){bounds.right, bounds.bottom};
}

static void status_scroll(const struct storage_prompt *_Nonnull prompt)
{
    RECT bounds;
    GetClientRect(prompt->status, &bounds);
    int width = bounds.right - GetSystemMetrics(SM_CXVSCROLL);
    SIZE text = text_size(prompt, prompt->status, width > 0 ? width : 1);
    ShowScrollBar(prompt->status, SB_VERT, text.cy > bounds.bottom);
}

static void show_reason(struct storage_prompt *_Nonnull prompt, const char *_Nonnull reason)
{
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(reason, units, ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY)
        SetWindowTextW(prompt->status, units);
    status_scroll(prompt);
    NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, prompt->status, OBJID_CLIENT, CHILDID_SELF);
}

static void show_failure(struct storage_prompt *_Nonnull prompt, enum storage_outcome result)
{
    show_reason(prompt, storage_state_failure_line(result, prompt->request->language));
}

static bool make_font(struct storage_prompt *_Nonnull prompt)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(prompt->request->language, face);
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

static bool select_all(HWND window, UINT message, WPARAM key)
{
    if (message == WM_KEYDOWN && key == 'A' && GetKeyState(VK_CONTROL) < 0)
    {
        SendMessageW(window, EM_SETSEL, 0, -1);
        return true;
    }
    return message == WM_CHAR && key == 1;
}

static LRESULT CALLBACK input_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct storage_prompt *_Nullable prompt =
        (struct storage_prompt *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (prompt == nullptr)
        return DefWindowProcW(window, message, wparam, lparam);
    WNDPROC original = window == prompt->input ? prompt->input_original : prompt->current_original;
    if (original == nullptr)
        return DefWindowProcW(window, message, wparam, lparam);
    if (window == prompt->input && message == WM_IME_STARTCOMPOSITION)
        prompt->composing = true;
    if (window == prompt->input && message == WM_IME_ENDCOMPOSITION)
        prompt->composing = false;
    if (select_all(window, message, wparam))
        return 0;
    return CallWindowProcW(original, window, message, wparam, lparam);
}

static bool name_input(struct storage_prompt *_Nonnull prompt, HWND window, enum ui_text name,
                       int identity)
{
    char16_t label[ui_text_unit_limit];
    SetWindowLongPtrW(window, GWLP_ID, identity);
    SetWindowLongPtrW(window, GWLP_USERDATA, (LONG_PTR)prompt);
    WNDPROC original = (WNDPROC)SetWindowLongPtrW(window, GWLP_WNDPROC, (LONG_PTR)input_procedure);
    if (window == prompt->input)
        prompt->input_original = original;
    else
        prompt->current_original = original;
    SendMessageW(window, EM_SETLIMITTEXT, path_unit_capacity - 1, 0);
    SendMessageW(window, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    return original != nullptr && wide_line(prompt, name, label) &&
           SUCCEEDED(accessibility_native_name(window, label));
}

static bool make_inputs(struct storage_prompt *_Nonnull prompt)
{
    enum ui_text current_name = storage_state_phase(prompt->request->storage) == STORAGE_FIRST_USE
                                    ? UI_TEXT_STORAGE_DEFAULT
                                    : UI_TEXT_STORAGE_CURRENT;
    enum ui_text input_name = prompt->setup ? UI_TEXT_STORAGE_TITLE : UI_TEXT_STORAGE_NEXT;
    prompt->current_label = control(prompt, L"STATIC", current_name, SS_NOPREFIX);
    prompt->current =
        control(prompt, L"EDIT", UI_TEXT_EMPTY, WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL);
    prompt->input_label = control(prompt, L"STATIC", input_name, SS_NOPREFIX);
    prompt->input = control(prompt, L"EDIT", UI_TEXT_EMPTY, WS_TABSTOP | ES_AUTOHSCROLL);
    return prompt->current_label != nullptr && prompt->current != nullptr &&
           prompt->input_label != nullptr && prompt->input != nullptr &&
           name_input(prompt, prompt->current, current_name, current_identity) &&
           name_input(prompt, prompt->input, input_name, input_identity) &&
           set_path(prompt->current,
                    storage_path_text(storage_state_directory(prompt->request->storage))) &&
           set_path(prompt->input,
                    storage_path_text(storage_state_next_directory(prompt->request->storage)));
}

static bool make_controls(struct storage_prompt *_Nonnull prompt)
{
    prompt->hint = control(
        prompt, L"STATIC",
        prompt->setup ? UI_TEXT_STORAGE_FIRST_HINT : UI_TEXT_STORAGE_RUNTIME_HINT, SS_NOPREFIX);
    if (prompt->hint == nullptr || !make_inputs(prompt))
        return false;
    prompt->browse = button(prompt, UI_TEXT_STORAGE_BROWSE, browse_identity);
    prompt->reset = button(prompt, UI_TEXT_STORAGE_DEFAULT, default_identity);
    prompt->status = control(prompt, L"EDIT", UI_TEXT_EMPTY,
                             ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | WS_TABSTOP);
    prompt->accept =
        button(prompt, prompt->setup ? UI_TEXT_STORAGE_FIRST_ACCEPT : UI_TEXT_STORAGE_ACCEPT, IDOK);
    prompt->cancel = button(prompt, UI_TEXT_PROMPT_CLOSE_CANCEL, IDCANCEL);
    if (prompt->browse == nullptr || prompt->reset == nullptr || prompt->status == nullptr ||
        prompt->accept == nullptr || prompt->cancel == nullptr)
        return false;
    return dialog_theme_create_close(prompt->dialog, prompt->font, prompt->request->language,
                                     &prompt->close) == DIALOG_THEME_CONTROL_READY;
}

static void position(HWND window, RECT bounds)
{
    MoveWindow(window, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

static void place_field(const struct storage_prompt *_Nonnull prompt, HWND window, RECT bounds)
{
    int inset = ui_part_focus_width(prompt->dpi) + scaled(prompt, 4);
    InflateRect(&bounds, -inset, -scaled(prompt, 6));
    position(window, bounds);
}

static void place_controls(struct storage_prompt *_Nonnull prompt, SIZE size)
{
    int edge = scaled(prompt, 16);
    int gap = scaled(prompt, size.cy >= scaled(prompt, 320) ? 8 : 4);
    int row = scaled(prompt, 32);
    int label = text_size(prompt, prompt->current_label, 0).cx;
    int next_label = text_size(prompt, prompt->input_label, 0).cx;
    label = (label > next_label ? label : next_label) + gap;
    label = label < size.cx / 2 ? label : size.cx / 2;
    int right = size.cx - edge;
    int top = scaled(prompt, 44);
    int hint = text_size(prompt, prompt->hint, right - edge).cy;
    position(prompt->hint, (RECT){edge, top, right, top + hint});
    top += hint + gap;
    position(prompt->current_label,
             (RECT){edge, top + scaled(prompt, 6), edge + label - gap, top + row});
    prompt->current_bounds = (RECT){edge + label, top, right, top + row};
    place_field(prompt, prompt->current, prompt->current_bounds);
    top += row + gap;
    position(prompt->input_label,
             (RECT){edge, top + scaled(prompt, 6), edge + label - gap, top + row});
    prompt->input_bounds = (RECT){edge + label, top, right, top + row};
    place_field(prompt, prompt->input, prompt->input_bounds);
    top += row + gap;
    int half = (right - edge - gap) / 2;
    position(prompt->browse, (RECT){edge, top, edge + half, top + row});
    position(prompt->reset, (RECT){edge + half + gap, top, right, top + row});
    top += row + gap;
    int footer = size.cy - edge - row;
    position(prompt->status, (RECT){edge, top, right, footer - gap});
    int cancel_width = text_size(prompt, prompt->cancel, 0).cx + scaled(prompt, 24);
    int accept_width = text_size(prompt, prompt->accept, 0).cx + scaled(prompt, 24);
    position(prompt->cancel, (RECT){right - cancel_width, footer, right, footer + row});
    position(prompt->accept, (RECT){right - cancel_width - gap - accept_width, footer,
                                    right - cancel_width - gap, footer + row});
    position(prompt->close, ui_part_surface_rect((RECT){0, 0, size.cx, size.cy}, prompt->dpi,
                                                 UI_PART_SURFACE_CLOSE));
}

static bool lay_out(struct storage_prompt *_Nonnull prompt, HWND anchor)
{
    RECT bounds =
        dialog_theme_popup_bounds(anchor, (SIZE){scaled(prompt, 480), scaled(prompt, 336)});
    SIZE size = {bounds.right - bounds.left, bounds.bottom - bounds.top};
    if (size.cx < scaled(prompt, 300) || size.cy < scaled(prompt, 264))
        return false;
    position(prompt->dialog, bounds);
    place_controls(prompt, size);
    status_scroll(prompt);
    InvalidateRect(prompt->dialog, nullptr, TRUE);
    return true;
}

static bool initialize(struct storage_prompt *_Nonnull prompt, HWND dialog)
{
    prompt->dialog = dialog;
    prompt->dpi = GetDpiForWindow(dialog);
    dialog_theme_decorate(prompt->theme, dialog);
    char16_t title[ui_text_unit_limit];
    if (!wide_line(prompt, UI_TEXT_STORAGE_TITLE, title) || !SetWindowTextW(dialog, title) ||
        !make_font(prompt) || !make_controls(prompt) ||
        !lay_out(prompt, GetWindow(dialog, GW_OWNER)))
        return false;
    enum storage_outcome notice = storage_state_notice(prompt->request->storage);
    if (notice != STORAGE_READY)
        show_failure(prompt, notice);
    else if (storage_state_pending(prompt->request->storage))
        show_reason(prompt, ui_text_line(UI_TEXT_STORAGE_PENDING, prompt->request->language));
    SendMessageW(dialog, DM_SETDEFID, IDOK, 0);
    modal_owner_positioned(dialog);
    SetFocus(prompt->input);
    SendMessageW(prompt->input, EM_SETSEL, 0, -1);
    return true;
}

static enum storage_outcome read_input(const struct storage_prompt *_Nonnull prompt,
                                       struct utf8_text *_Nullable *_Nonnull out)
{
    int length = GetWindowTextLengthW(prompt->input);
    if (length <= 0 || (size_t)length >= path_unit_capacity)
        return STORAGE_INVALID_PATH;
    char16_t units[path_unit_capacity];
    int read = GetWindowTextW(prompt->input, units, (int)path_unit_capacity);
    if (read != length)
        return STORAGE_INVALID_PATH;
    enum utf8_text_outcome result = utf8_text_create(units, (size_t)read, out);
    if (result == UTF8_TEXT_CONVERTED)
        return STORAGE_READY;
    return result == UTF8_TEXT_OUT_OF_MEMORY ? STORAGE_OUT_OF_MEMORY : STORAGE_INVALID_PATH;
}

static void submit(struct storage_prompt *_Nonnull prompt)
{
    struct utf8_text *_Nullable text = nullptr;
    enum storage_outcome result = read_input(prompt, &text);
    if (result == STORAGE_READY)
        result = storage_state_choose(prompt->request->storage, utf8_text_bytes(text),
                                      utf8_text_length(text));
    utf8_text_destroy(text);
    if (result == STORAGE_READY)
    {
        EndDialog(prompt->dialog, IDOK);
        return;
    }
    show_failure(prompt, result);
    SetFocus(prompt->input);
}

static bool owner_closing(const struct storage_prompt *_Nonnull prompt)
{
    HWND root = modal_owner_root(prompt->dialog);
    return root != nullptr && root != prompt->dialog &&
           SendMessageW(root, folio_message_close_pending, 0, 0) != 0;
}

static void browse(struct storage_prompt *_Nonnull prompt)
{
    struct utf8_text *_Nullable input = nullptr;
    enum storage_outcome read = read_input(prompt, &input);
    const char *_Nonnull initial =
        read == STORAGE_READY
            ? utf8_text_bytes(input)
            : storage_path_text(storage_state_next_directory(prompt->request->storage));
    struct utf8_text *_Nullable selected = nullptr;
    enum folder_picker_outcome result =
        folder_picker_show(prompt->dialog, initial, prompt->request->language, &selected);
    utf8_text_destroy(input);
    if (owner_closing(prompt))
    {
        utf8_text_destroy(selected);
        EndDialog(prompt->dialog, IDCANCEL);
        return;
    }
    switch (result)
    {
    case FOLDER_PICKER_SELECTED:
        if (!set_path(prompt->input, utf8_text_bytes(selected)))
            show_failure(prompt, STORAGE_INVALID_PATH);
        break;
    case FOLDER_PICKER_CANCELLED:
        break;
    case FOLDER_PICKER_UNAVAILABLE:
        show_reason(prompt, ui_text_line(UI_TEXT_STORAGE_PICKER_FAILED, prompt->request->language));
        break;
    }
    utf8_text_destroy(selected);
    SetFocus(prompt->input);
}

static INT_PTR command(struct storage_prompt *_Nonnull prompt, WPARAM wparam)
{
    WORD id = LOWORD(wparam);
    if (id == input_identity || id == current_identity)
    {
        InvalidateRect(prompt->dialog, nullptr, FALSE);
        return FALSE;
    }
    if (prompt->composing)
        return TRUE;
    switch (id)
    {
    case IDOK:
        submit(prompt);
        return TRUE;
    case IDCANCEL:
    case IDCLOSE:
        EndDialog(prompt->dialog, IDCANCEL);
        return TRUE;
    case browse_identity:
        browse(prompt);
        return TRUE;
    case default_identity:
        if (!set_path(prompt->input,
                      storage_path_text(storage_state_default_directory(prompt->request->storage))))
            show_failure(prompt, STORAGE_INVALID_PATH);
        SetFocus(prompt->input);
        return TRUE;
    default:
        return FALSE;
    }
}

static void paint_contents(const struct storage_prompt *_Nonnull prompt, HDC device)
{
    dialog_theme_draw_title(prompt->theme, prompt->dialog, device, prompt->font);
    HWND focus = GetFocus();
    int width = ui_part_focus_width(prompt->dpi);
    dialog_theme_field(prompt->theme, device, &prompt->current_bounds,
                       focus == prompt->current ? width : 0);
    dialog_theme_field(prompt->theme, device, &prompt->input_bounds,
                       focus == prompt->input ? width : 0);
}

static void paint_prompt(const struct storage_prompt *_Nonnull prompt)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(prompt->dialog, &paint);
    if (device != nullptr)
        paint_contents(prompt, device);
    EndPaint(prompt->dialog, &paint);
}

static void reflow(struct storage_prompt *_Nonnull prompt, const RECT *_Nullable suggested)
{
    if (suggested != nullptr)
        SetWindowPos(prompt->dialog, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    prompt->dpi = GetDpiForWindow(prompt->dialog);
    if (!make_font(prompt) || !lay_out(prompt, prompt->dialog))
        EndDialog(prompt->dialog, unavailable_result);
    RedrawWindow(prompt->dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static enum dialog_theme_button button_kind(UINT identity)
{
    if (identity == IDCLOSE)
        return DIALOG_THEME_BUTTON_CLOSE;
    return identity == IDOK ? DIALOG_THEME_BUTTON_PRIMARY : DIALOG_THEME_BUTTON_SECONDARY;
}

static INT_PTR paint_message(struct storage_prompt *_Nonnull prompt, UINT message, WPARAM wparam,
                             LPARAM lparam)
{
    switch (message)
    {
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        if (prompt->request->folio != nullptr)
            dialog_theme_follow(prompt->dialog, prompt->request->folio, &prompt->theme);
        return TRUE;
    case WM_CTLCOLORDLG:
        return (INT_PTR)dialog_theme_color(prompt->theme, DIALOG_THEME_DIALOG, (HDC)wparam);
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        return (INT_PTR)dialog_theme_color(
            prompt->theme,
            (HWND)lparam == prompt->input || (HWND)lparam == prompt->current ? DIALOG_THEME_FIELD
                                                                             : DIALOG_THEME_LABEL,
            (HDC)wparam);
    case WM_DRAWITEM:
    {
        const DRAWITEMSTRUCT *_Nonnull item = (const DRAWITEMSTRUCT *)lparam;
        dialog_theme_draw_button(prompt->theme, item, button_kind(item->CtlID));
        return TRUE;
    }
    case WM_PAINT:
        paint_prompt(prompt);
        return TRUE;
    case WM_PRINTCLIENT:
        paint_contents(prompt, (HDC)wparam);
        return TRUE;
    case WM_DPICHANGED:
        reflow(prompt, (const RECT *)lparam);
        return TRUE;
    case folio_message_modal_reflow:
        reflow(prompt, nullptr);
        return TRUE;
    default:
        return FALSE;
    }
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (dialog_theme_native_message(dialog, message, wparam, lparam))
        return TRUE;
    if (message == WM_INITDIALOG)
    {
        struct storage_prompt *_Nonnull prompt = (struct storage_prompt *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)prompt);
        if (!initialize(prompt, dialog))
            EndDialog(dialog, unavailable_result);
        return FALSE;
    }
    struct storage_prompt *_Nullable prompt =
        (struct storage_prompt *)GetWindowLongPtrW(dialog, DWLP_USER);
    if (prompt == nullptr)
        return FALSE;
    if (message == folio_message_modal_cancel || message == WM_CLOSE)
    {
        SetWindowLongPtrW(dialog, DWLP_MSGRESULT, 1);
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    if (message == WM_COMMAND)
        return command(prompt, wparam);
    return paint_message(prompt, message, wparam, lparam);
}

/* modal loopが完全に返ってからfont/themeを破棄する。入力を保持するのはnative欄だけ。 */
static void destroy_resources(struct storage_prompt *_Nonnull prompt, ULONG_PTR token)
{
    if (prompt->font != nullptr)
        DeleteObject(prompt->font);
    dialog_theme_destroy(prompt->theme);
    icon_paint_stop(token);
    CoUninitialize();
}

enum storage_prompt_outcome
storage_prompt_show(HWND _Nullable owner, const struct storage_prompt_request *_Nonnull request,
                    const struct folio_palette *_Nonnull palette)
{
    struct storage_prompt prompt = {
        .request = request, .setup = storage_state_phase(request->storage) != STORAGE_ACTIVE};
    ULONG_PTR token = 0;
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return STORAGE_PROMPT_UNAVAILABLE;
    if (icon_paint_start(&token) != ICON_PAINT_STARTED)
    {
        destroy_resources(&prompt, token);
        return STORAGE_PROMPT_UNAVAILABLE;
    }
    if (dialog_theme_create(palette, &prompt.theme) != DIALOG_THEME_READY)
    {
        destroy_resources(&prompt, token);
        return STORAGE_PROMPT_UNAVAILABLE;
    }
    INT_PTR ended = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                            procedure, (LPARAM)&prompt);
    destroy_resources(&prompt, token);
    if (ended == IDOK)
        return STORAGE_PROMPT_ACCEPTED;
    return ended == IDCANCEL ? STORAGE_PROMPT_CANCELLED : STORAGE_PROMPT_UNAVAILABLE;
}
