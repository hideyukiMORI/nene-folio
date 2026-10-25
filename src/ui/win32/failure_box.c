#include "failure_box.h"
#include "folio_message.h"
#include "modal_owner.h"

#include "dialog_theme.h"
#include "failure_box_outcome.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"

constexpr INT_PTR box_not_built = -2;

struct failure_box
{
    const char16_t *_Nonnull units;
    enum folio_language language;
    const struct folio_state *_Nonnull state;
    struct dialog_theme *_Nullable theme;
    HFONT _Nullable font;
    HWND _Nullable dialog;
    HWND _Nullable body;
    HWND _Nullable accept;
    HWND _Nullable close;
    UINT dpi;
    RECT mark;
    bool ready;
    bool announced;
    bool io_failure;
};

static const struct
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WORD title;
} template = {
    .dialog = {.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, .cx = 200, .cy = 80}};

/* 入力/前提/形式判定は無音。既存resultが束ねる読込・公開操作単位で分類する。 */
static constexpr bool io_sounds[] = {
    [FOLIO_STATE_READY] = false,
    [FOLIO_STATE_DATA_UNREADABLE] = true,
    [FOLIO_STATE_LEDGER_MALFORMED] = false,
    [FOLIO_STATE_STORE_FAILED] = true,
    [FOLIO_STATE_NO_SUCH_CATEGORY] = false,
    [FOLIO_STATE_NO_SUCH_NOTE] = false,
    [FOLIO_STATE_NOTE_UNREADABLE] = true,
    [FOLIO_STATE_NOTHING_SELECTED] = false,
    [FOLIO_STATE_NOT_EDITING] = false,
    [FOLIO_STATE_NOTE_MALFORMED] = false,
    [FOLIO_STATE_NOTE_TOO_LARGE] = false,
    [FOLIO_STATE_NOTE_UNEDITABLE] = false,
    [FOLIO_STATE_NOTE_STORE_FAILED] = true,
    [FOLIO_STATE_HISTORY_FAILED] = true,
    [FOLIO_STATE_UNSAVED_CHANGES] = false,
    [FOLIO_STATE_NAME_TAKEN] = false,
    [FOLIO_STATE_NAME_UNCHANGED] = false,
    [FOLIO_STATE_LEDGER_STALE] = true,
    [FOLIO_STATE_LEDGER_UNSYNCED] = true,
    [FOLIO_STATE_RENAME_PENDING] = true,
    [FOLIO_STATE_RENAME_UNLOCKED] = true,
    [FOLIO_STATE_RENAME_UNSUPPORTED] = false,
    [FOLIO_STATE_RENAME_IDENTITY_FAILED] = true,
    [FOLIO_STATE_RENAME_JOURNAL_FAILED] = true,
    [FOLIO_STATE_RENAME_JOURNAL_BROKEN] = false,
    [FOLIO_STATE_RENAME_HALTED] = false,
    [FOLIO_STATE_SEARCH_MALFORMED] = false,
    [FOLIO_STATE_FILTERED] = false,
    [FOLIO_STATE_SETTINGS_UNREADABLE] = true,
    [FOLIO_STATE_SETTINGS_STORE_FAILED] = true,
    [FOLIO_STATE_APPEARANCE_UNAVAILABLE] = false,
    [FOLIO_STATE_PANE_UNAVAILABLE] = false,
    [FOLIO_STATE_PANE_LOAD_FAILED] = false,
    [FOLIO_STATE_PANE_RECOLOR_FAILED] = false,
    [FOLIO_STATE_REPLACE_NO_PATTERN] = false,
    [FOLIO_STATE_REPLACE_BAD_PATTERN] = false,
    [FOLIO_STATE_REPLACE_BAD_TEMPLATE] = false,
    [FOLIO_STATE_REPLACE_TIMED_OUT] = false,
    [FOLIO_STATE_REPLACE_TOO_COMPLEX] = false,
    [FOLIO_STATE_REPLACE_TOO_MANY] = false,
    [FOLIO_STATE_REPLACE_TOO_LARGE] = false,
    [FOLIO_STATE_REPLACE_STALE] = false,
    [FOLIO_STATE_REPLACE_BAD_SPAN] = false,
    [FOLIO_STATE_OUT_OF_MEMORY] = false,
    [FOLIO_STATE_NAME_REQUIRED] = false,
    [FOLIO_STATE_INVALID_NAME] = false,
    [FOLIO_STATE_ALREADY_NAMED] = false,
    [FOLIO_STATE_CANCELLED] = false,
    [FOLIO_STATE_FONT_BUNDLE_UNAVAILABLE] = false,
    [FOLIO_STATE_HISTORY_EMPTY] = false,
    [FOLIO_STATE_HISTORY_UNREADABLE] = true,
    [FOLIO_STATE_CATEGORY_NAME_INVALID] = false,
    [FOLIO_STATE_CATEGORY_NAME_TAKEN] = false,
    [FOLIO_STATE_CATEGORY_NOT_CREATED] = true,
    [FOLIO_STATE_CATEGORY_LEDGER_STALE] = true,
    [FOLIO_STATE_TRASH_UNAVAILABLE] = false,
    [FOLIO_STATE_TRASH_BUSY] = true,
    [FOLIO_STATE_TRASH_FAILED] = true,
    [FOLIO_STATE_CATEGORY_NOT_EMPTY] = false,
    [FOLIO_STATE_CATEGORY_HAS_FILES] = false,
    [FOLIO_STATE_CATEGORY_HISTORY_NOT_RECYCLED] = true,
    [FOLIO_STATE_CATEGORY_REMOVE_FAILED] = true,
    [FOLIO_STATE_CATEGORY_NOT_SELECTED] = false,
};

static bool io_failure(enum folio_state_outcome outcome)
{
    return io_sounds[outcome];
}

static int scaled(const struct failure_box *_Nonnull box, int value)
{
    return MulDiv(value, (int)box->dpi, 96);
}

static void position(HWND window, RECT bounds)
{
    MoveWindow(window, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

static bool make_font(struct failure_box *_Nonnull box)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(box->language, face);
    HFONT font = CreateFontW(-scaled(box, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    if (font == nullptr)
        return false;
    HFONT previous = box->font;
    box->font = font;
    for (HWND child = GetWindow(box->dialog, GW_CHILD); child != nullptr;
         child = GetWindow(child, GW_HWNDNEXT))
        SendMessageW(child, WM_SETFONT, (WPARAM)font, FALSE);
    if (previous != nullptr)
        DeleteObject(previous);
    return true;
}

static bool make_controls(struct failure_box *_Nonnull box)
{
    box->body = CreateWindowExW(
        0, L"EDIT", box->units,
        WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 0, 0, 0,
        0, box->dialog, nullptr, GetModuleHandleW(nullptr), nullptr);
    char16_t label[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(ui_text_line(UI_TEXT_TOOLTIP_CLOSE, box->language), label,
                        ui_text_unit_limit, &written) != UTF16_TEXT_FILL_READY)
        return false;
    box->accept = CreateWindowExW(
        0, L"BUTTON", label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW, 0, 0, 0, 0,
        box->dialog, (HMENU)(INT_PTR)IDOK, GetModuleHandleW(nullptr), nullptr);
    if (box->body == nullptr || box->accept == nullptr ||
        dialog_theme_track_button(box->accept) != DIALOG_THEME_CONTROL_READY ||
        dialog_theme_create_close(box->dialog, box->font, box->language, &box->close) !=
            DIALOG_THEME_CONTROL_READY)
        return false;
    SendMessageW(box->body, WM_SETFONT, (WPARAM)box->font, FALSE);
    SendMessageW(box->body, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    SendMessageW(box->accept, WM_SETFONT, (WPARAM)box->font, FALSE);
    return true;
}

static int measured_height(const struct failure_box *_Nonnull box, int width)
{
    HDC device = GetDC(box->dialog);
    if (device == nullptr)
        return 0;
    HGDIOBJ previous = SelectObject(device, box->font);
    RECT bounds = {0, 0, width, 0};
    DrawTextW(device, box->units, -1, &bounds,
              DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX | DT_EXPANDTABS);
    SelectObject(device, previous);
    ReleaseDC(box->dialog, device);
    return bounds.bottom;
}

static SIZE maximum_size(const struct failure_box *_Nonnull box, HWND anchor)
{
    HWND owner = modal_owner_root(box->dialog);
    RECT parent = {0, 0, scaled(box, 560), scaled(box, 360)};
    if (owner != nullptr)
        GetClientRect(owner, &parent);
    RECT bounds = dialog_theme_popup_bounds(
        anchor,
        (SIZE){scaled(box, 480), parent.bottom - scaled(box, ui_part_surface_title_height + 32)});
    return (SIZE){bounds.right - bounds.left, bounds.bottom - bounds.top};
}

static void place_controls(struct failure_box *_Nonnull box, SIZE size, int height)
{
    int edge = scaled(box, 16);
    int top = scaled(box, 60);
    box->mark = (RECT){edge, top, edge + scaled(box, 24), top + scaled(box, 24)};
    position(box->body, (RECT){scaled(box, 48), top, size.cx - edge, top + height});
    int bottom = size.cy - edge;
    position(box->accept, (RECT){size.cx - edge - scaled(box, 88), bottom - scaled(box, 28),
                                 size.cx - edge, bottom});
    position(box->close,
             ui_part_surface_rect((RECT){0, 0, size.cx, size.cy}, box->dpi, UI_PART_SURFACE_CLOSE));
}

static bool lay_out(struct failure_box *_Nonnull box, HWND anchor)
{
    SIZE size = maximum_size(box, anchor);
    int room = size.cy - scaled(box, 120);
    int wanted = measured_height(box, size.cx - scaled(box, 64));
    if (room <= 0 || wanted <= 0)
        return false;
    int height = wanted < room ? wanted : room;
    height = height < scaled(box, 24) ? scaled(box, 24) : height;
    size.cy = scaled(box, 120) + height;
    RECT bounds = dialog_theme_popup_bounds(anchor, size);
    position(box->dialog, bounds);
    bool scrolling = wanted > height;
    ShowScrollBar(box->body, SB_VERT, scrolling);
    LONG_PTR style = GetWindowLongPtrW(box->body, GWL_STYLE);
    SetWindowLongPtrW(box->body, GWL_STYLE,
                      scrolling ? style | WS_TABSTOP : style & ~(LONG_PTR)WS_TABSTOP);
    place_controls(box, size, height);
    return true;
}

static bool initialize(struct failure_box *_Nonnull box, HWND dialog)
{
    box->dialog = dialog;
    box->dpi = GetDpiForWindow(dialog);
    struct folio_palette palette = folio_palette_from(folio_state_palette(box->state));
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
    if (!make_font(box) || !make_controls(box) || !lay_out(box, modal_owner_root(dialog)))
        return false;
    box->ready = true;
    SetFocus(box->accept);
    return true;
}

static void announce(struct failure_box *_Nonnull box)
{
    if (!box->ready || box->announced)
        return;
    box->announced = true;
    NotifyWinEvent(EVENT_SYSTEM_ALERT, box->dialog, OBJID_WINDOW, CHILDID_SELF);
    if (box->io_failure)
        MessageBeep(MB_ICONWARNING);
}

static void paint_contents(const struct failure_box *_Nonnull box, HDC device)
{
    dialog_theme_draw_title(box->theme, box->dialog, device, box->font);
    dialog_theme_draw_warning(box->theme, device, box->mark);
}

static void paint_box(const struct failure_box *_Nonnull box)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(box->dialog, &paint);
    if (device != nullptr)
        paint_contents(box, device);
    EndPaint(box->dialog, &paint);
}

static void follow_dpi(struct failure_box *_Nonnull box, const RECT *_Nonnull suggested)
{
    SetWindowPos(box->dialog, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    box->dpi = GetDpiForWindow(box->dialog);
    if (!make_font(box) || !lay_out(box, box->dialog))
        EndDialog(box->dialog, box_not_built);
    RedrawWindow(box->dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static INT_PTR paint_message(struct failure_box *_Nonnull box, UINT message, WPARAM wparam,
                             LPARAM lparam)
{
    if (box->theme == nullptr)
        return FALSE;
    switch (message)
    {
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        dialog_theme_follow(box->dialog, box->state, &box->theme);
        return TRUE;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
        return (INT_PTR)dialog_theme_color(box->theme, DIALOG_THEME_LABEL, (HDC)wparam);
    case WM_DRAWITEM:
        dialog_theme_draw_button(box->theme, (const DRAWITEMSTRUCT *)lparam,
                                 ((const DRAWITEMSTRUCT *)lparam)->CtlID == IDCLOSE
                                     ? DIALOG_THEME_BUTTON_CLOSE
                                     : DIALOG_THEME_BUTTON_SECONDARY);
        return TRUE;
    case WM_PAINT:
        paint_box(box);
        return TRUE;
    case WM_PRINTCLIENT:
        paint_contents(box, (HDC)wparam);
        return TRUE;
    case WM_DPICHANGED:
        follow_dpi(box, (const RECT *)lparam);
        return TRUE;
    case folio_message_modal_reflow:
        if (!lay_out(box, box->dialog))
            EndDialog(box->dialog, box_not_built);
        return TRUE;
    default:
        return FALSE;
    }
}

static bool closes_box(UINT message, WPARAM wparam)
{
    return message == WM_CLOSE ||
           (message == WM_COMMAND &&
            (LOWORD(wparam) == IDOK || LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDCLOSE));
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (dialog_theme_native_message(dialog, message, wparam, lparam))
        return TRUE;
    if (message == WM_INITDIALOG)
    {
        struct failure_box *_Nonnull box = (struct failure_box *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)box);
        if (!initialize(box, dialog))
            EndDialog(dialog, box_not_built);
        return FALSE;
    }
    struct failure_box *_Nullable box = (struct failure_box *)GetWindowLongPtrW(dialog, DWLP_USER);
    if (box == nullptr)
        return FALSE;
    if (message == folio_message_modal_cancel)
    {
        SetWindowLongPtrW(dialog, DWLP_MSGRESULT, 1);
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (message == WM_SHOWWINDOW && wparam != 0)
        announce(box);
    INT_PTR painted = paint_message(box, message, wparam, lparam);
    if (painted != FALSE)
        return painted;
    if (closes_box(message, wparam))
    {
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    return FALSE;
}

[[nodiscard]] static enum failure_box_outcome show_themed(HWND _Nullable owner,
                                                          struct failure_box *_Nonnull box)
{
    INT_PTR ended = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                            procedure, (LPARAM)box);
    return ended == -1 || ended == box_not_built ? FAILURE_BOX_FALLBACK : FAILURE_BOX_SHOWN;
}

static const char *_Nonnull failure_line(enum folio_state_outcome outcome,
                                         const struct folio_state *_Nonnull state,
                                         const enum folio_command *_Nullable command,
                                         char *_Nonnull out)
{
    enum folio_language language = folio_state_language(state);
    const char *_Nonnull reason = folio_state_failure_line(outcome, language);
    if (command == nullptr)
        return reason;
    enum folio_command copied = *command;
    struct ui_text_request request = {.id = UI_TEXT_FAILURE_COMMAND_REASON,
                                      .language = language,
                                      .name = folio_command_label(copied, language),
                                      .from = reason};
    return ui_text_format(&request, out, ui_text_unit_limit * 8 + 2) == UI_TEXT_FORMAT_READY
               ? out
               : reason;
}

void failure_box_show(HWND _Nullable owner, enum folio_state_outcome outcome,
                      const struct folio_state *_Nonnull state,
                      const enum folio_command *_Nullable command)
{
    if (outcome == FOLIO_STATE_CANCELLED)
        return;
    enum folio_language language = folio_state_language(state);
    char message[ui_text_unit_limit * 8 + 2];
    const char *_Nonnull line = failure_line(outcome, state, command, message);
    char16_t units[ui_text_unit_limit * 2];
    size_t written = 0;
    if (utf16_text_fill(line, units, ui_text_unit_limit * 2, &written) != UTF16_TEXT_FILL_READY)
        return;
    struct failure_box box = {
        .units = units, .language = language, .state = state, .io_failure = io_failure(outcome)};
    enum failure_box_outcome shown = show_themed(owner, &box);
    dialog_theme_destroy(box.theme);
    if (box.font != nullptr)
        DeleteObject(box.font);
    switch (shown)
    {
    case FAILURE_BOX_SHOWN:
        break;
    case FAILURE_BOX_FALLBACK:
        /* 同じ元理由をOSへ渡す。既に通知した面がDPI更新で退避した場合も二重に鳴らさない。 */
        MessageBoxW(owner, box.units, L"NeNe Folio",
                    MB_OK | (box.io_failure && !box.announced ? MB_ICONWARNING : 0));
        break;
    }
}
