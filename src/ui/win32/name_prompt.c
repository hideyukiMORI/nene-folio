#include "name_prompt.h"
#include "accessibility_native.h"
#include "category_name.h"
#include "dialog_theme.h"
#include "folio_command.h"
#include "folio_message.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "icon_paint.h"
#include "modal_owner.h"
#include "name_prompt_metrics.h"
#include "note_name.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <string.h>

struct name_prompt
{
    struct folio_state *_Nonnull state;
    enum name_prompt_kind kind;
    size_t target;
    enum rename_kind pending_kind;
    const char16_t *_Nonnull units;
    size_t count;
    HWND _Nullable dialog;
    HWND _Nullable name;
    HWND _Nullable category;
    HWND _Nullable failure;
    HWND _Nullable hint;
    HWND _Nullable accept;
    HWND _Nullable cancel;
    HWND _Nullable discard;
    HWND _Nullable close;
    HWND _Nullable name_label;
    HWND _Nullable category_label;
    WNDPROC _Nullable original;
    HFONT _Nullable font;
    struct name_prompt_metrics *_Nonnull metrics;
    /* 初期paletteの塗り。OS色通知で実効色へ追随する（ADR0057）。 */
    struct dialog_theme *_Nullable theme;
    UINT dpi;
    RECT name_bounds;
    RECT category_bounds;
    RECT result_bounds;
    RECT name_label_bounds;
    RECT category_label_bounds;
    int body_height;
    int rows;
    int button_width;
    bool stacked;
    bool result_scroll;
    bool composing;
    /* 記録を公開した後は名前を固定し、同じ改名の再開だけを受ける（ADR 0022 の決定 7）。 */
    bool pending;
    enum folio_state_outcome outcome;
    enum folio_untitled_policy untitled;
};

static const struct
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WORD title;
} template = {.dialog = {.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN,
                         .cx = 250,
                         .cy = 180}};

/* 「旧名 → 新名」と「失敗の 1 行 ＋ 説明」を組む領域（UTF-8 のバイト数）。
 * ノート名は 255 バイトまでなので、矢印を挟んでも 600 に収まる（ADR 0030 の決定 3）。 */
constexpr size_t pending_line_capacity = 600;
constexpr size_t pending_reason_capacity = 512;

static int scaled(const struct name_prompt *_Nonnull prompt, int value)
{
    return MulDiv(value, (int)prompt->dpi, 96);
}

/* 表の 1 行を確保せずに UTF-16 へ写す（ADR 0030 の決定 5）。写せなければ空にする
 * （表の値が上限に収まることは単体が全 ID を回して固定する）。 */
static void wide_line(enum ui_text id, enum folio_language language, char16_t *_Nonnull out)
{
    size_t written = 0;
    if (utf16_text_fill(ui_text_line(id, language), out, ui_text_unit_limit, &written) !=
        UTF16_TEXT_FILL_READY)
    {
        out[0] = u'\0';
    }
}

static enum folio_language prompt_language(const struct name_prompt *_Nonnull prompt)
{
    return folio_state_language(prompt->state);
}

static HWND _Nullable control(struct name_prompt *_Nonnull prompt, const wchar_t *_Nonnull kind,
                              const wchar_t *_Nonnull text, DWORD style)
{
    HWND window = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0,
                                  prompt->dialog, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (window != nullptr)
    {
        SendMessageW(window, WM_SETFONT, (WPARAM)prompt->font, TRUE);
    }
    if (window != nullptr && wcscmp(kind, L"BUTTON") == 0 &&
        dialog_theme_track_button(window) != DIALOG_THEME_CONTROL_READY)
    {
        DestroyWindow(window);
        return nullptr;
    }
    return window;
}

/* bounds は 96 DPI の単位、lowered はその下へ足す画素（一覧より下の欄だけが渡す）。 */
static void position(const struct name_prompt *_Nonnull prompt, HWND window, RECT bounds,
                     int lowered)
{
    MoveWindow(window, scaled(prompt, bounds.left), scaled(prompt, bounds.top) + lowered,
               scaled(prompt, bounds.right - bounds.left),
               scaled(prompt, bounds.bottom - bounds.top), TRUE);
}

static HWND _Nullable label(struct name_prompt *_Nonnull prompt, enum ui_text id, RECT bounds,
                            int lowered)
{
    char16_t units[ui_text_unit_limit];
    wide_line(id, prompt_language(prompt), units);
    HWND window = control(prompt, L"STATIC", units, SS_LEFT);
    if (window == nullptr)
    {
        return nullptr;
    }
    position(prompt, window, bounds, lowered);
    return window;
}

constexpr int discard_identity = 3;

static bool button(struct name_prompt *_Nonnull prompt, enum ui_text id, int identity, RECT bounds)
{
    char16_t units[ui_text_unit_limit];
    wide_line(id, prompt_language(prompt), units);
    /* 押し釦は WM_CTLCOLORBTN では塗れないので owner-draw にする（ADR 0035 の決定 3）。
     * 既定かどうかは ODS_DEFAULT が立たないので、描くときに id で面が渡す。 */
    HWND window = control(prompt, L"BUTTON", units, WS_TABSTOP | BS_OWNERDRAW);
    if (window == nullptr)
    {
        return false;
    }
    SetWindowLongPtrW(window, GWLP_ID, identity);
    position(prompt, window, bounds, 0);
    if (identity == IDOK)
    {
        prompt->accept = window;
    }
    if (identity == IDCANCEL)
    {
        prompt->cancel = window;
    }
    if (identity == discard_identity)
    {
        prompt->discard = window;
    }
    return true;
}

static LRESULT CALLBACK input_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct name_prompt *_Nullable prompt =
        (struct name_prompt *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (prompt == nullptr || prompt->original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    if (message == WM_IME_STARTCOMPOSITION)
    {
        prompt->composing = true;
    }
    if (message == WM_IME_ENDCOMPOSITION)
    {
        prompt->composing = false;
        InvalidateRect(prompt->dialog, nullptr, FALSE);
    }
    return CallWindowProcW(prompt->original, window, message, wparam, lparam);
}

static bool fill_categories(struct name_prompt *_Nonnull prompt)
{
    size_t count = folio_state_category_count(prompt->state);
    for (size_t index = 0; index < count; ++index)
    {
        const char *_Nonnull name = folio_state_category_name(prompt->state, index);
        struct utf16_text *_Nullable wide = nullptr;
        if (utf16_text_create(name, strlen(name), &wide) != UTF16_TEXT_CONVERTED)
        {
            return false;
        }
        LRESULT added =
            SendMessageW(prompt->category, LB_ADDSTRING, 0, (LPARAM)utf16_text_units(wide));
        utf16_text_destroy(wide);
        if (added == LB_ERR || added == LB_ERRSPACE)
        {
            return false;
        }
    }
    SendMessageW(prompt->category, LB_SETCURSEL, folio_state_document_category(prompt->state), 0);
    return true;
}

/* 名前欄に出す 1 行。組み立てた「旧名 → 新名」も実名もこの上限に収まる（ADR 0030 の決定 5）。 */
static bool set_name_text(struct name_prompt *_Nonnull prompt, const char *_Nonnull text)
{
    char16_t units[pending_line_capacity];
    size_t written = 0;
    if (utf16_text_fill(text, units, pending_line_capacity, &written) != UTF16_TEXT_FILL_READY)
    {
        return false;
    }
    SetWindowTextW(prompt->name, units);
    return true;
}

/* 表の 1 行をそのまま欄へ出す。写せなければ何も出さない（ADR 0030 の決定 5）。 */
static void show_line(HWND _Nullable window, enum ui_text id, enum folio_language language)
{
    char16_t units[ui_text_unit_limit];
    wide_line(id, language, units);
    SetWindowTextW(window, units);
}

/* 未完了の意図があるあいだは、名前もカテゴリも変えられない固定状態で開く（決定 7）。
 * 出すのは旧名と新名で、押せるのは「再試行」と「閉じる」だけ。閉じても意図は残る。 */
static bool fill_pending(struct name_prompt *_Nonnull prompt, struct rename_view pending)
{
    enum folio_language language = prompt_language(prompt);
    char line[pending_line_capacity];
    struct ui_text_request request = {.id = UI_TEXT_PROMPT_PENDING_RENAME,
                                      .language = language,
                                      .from = pending.from,
                                      .to = pending.to};
    if (ui_text_format(&request, line, pending_line_capacity) != UI_TEXT_FORMAT_READY)
    {
        return false;
    }
    if (!set_name_text(prompt, line))
    {
        return false;
    }
    prompt->pending = true;
    prompt->pending_kind = pending.kind;
    SendMessageW(prompt->name, EM_SETREADONLY, TRUE, 0);
    char16_t to[pending_line_capacity];
    size_t to_units = 0;
    if (utf16_text_fill(pending.to, to, pending_line_capacity, &to_units) != UTF16_TEXT_FILL_READY)
    {
        return false;
    }
    LRESULT end = GetWindowTextLengthW(prompt->name);
    SendMessageW(prompt->name, EM_SETSEL, (WPARAM)(end - (LRESULT)to_units), end);
    SendMessageW(prompt->name, EM_SCROLLCARET, 0, 0);
    show_line(prompt->failure, UI_TEXT_PROMPT_PENDING_EXPLANATION, language);
    return true;
}

/* 改名は文書の実名を選択状態で出す。初回・別名保存は空のまま（決定 1）。 */
static bool fill_name(struct name_prompt *_Nonnull prompt)
{
    if (prompt->kind == NAME_PROMPT_RETRY_RENAME)
    {
        struct rename_view pending = {.from = "", .to = ""};
        return folio_state_rename_pending(prompt->state, &pending) && fill_pending(prompt, pending);
    }
    const char *_Nonnull name = "";
    switch (prompt->kind)
    {
    case NAME_PROMPT_RENAME:
        name = folio_state_document_name(prompt->state);
        break;
    case NAME_PROMPT_RENAME_CATEGORY:
        name = folio_state_category_name(prompt->state, prompt->target);
        break;
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return true;
    }
    if (!set_name_text(prompt, name))
    {
        return false;
    }
    SendMessageW(prompt->name, EM_SETSEL, 0, -1);
    return true;
}

/* 面の種別は変えず、復旧の題と見出しだけ実際の意図から引く（ADR0041）。 */
static enum name_prompt_kind displayed_kind(const struct name_prompt *_Nonnull prompt)
{
    if (!prompt->pending)
    {
        return prompt->kind;
    }
    switch (prompt->pending_kind)
    {
    case RENAME_KIND_NOTE:
        return NAME_PROMPT_RENAME;
    case RENAME_KIND_CATEGORY:
        return NAME_PROMPT_RENAME_CATEGORY;
    }
    return prompt->kind;
}

static const char *_Nonnull prompt_title(enum name_prompt_kind kind, enum folio_language language)
{
    switch (kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
        return ui_text_line(UI_TEXT_PROMPT_TITLE_UNTITLED, language);
    case NAME_PROMPT_FIRST_SAVE:
        return folio_command_label(FOLIO_COMMAND_SAVE, language);
    case NAME_PROMPT_SAVE_AS:
        return folio_command_label(FOLIO_COMMAND_SAVE_AS, language);
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_RETRY_RENAME:
        return folio_command_label(FOLIO_COMMAND_RENAME, language);
    case NAME_PROMPT_NEW_CATEGORY:
        return folio_command_label(FOLIO_COMMAND_NEW_CATEGORY, language);
    case NAME_PROMPT_RENAME_CATEGORY:
        return folio_command_label(FOLIO_COMMAND_RENAME_CATEGORY, language);
    }
    return "";
}

static enum ui_text prompt_name_label(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_RENAME:
        return UI_TEXT_PROMPT_LABEL_NAME;
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RENAME_CATEGORY:
        return UI_TEXT_PROMPT_LABEL_NAME;
    case NAME_PROMPT_RETRY_RENAME:
        return UI_TEXT_PROMPT_LABEL_NAME;
    }
    return UI_TEXT_PROMPT_LABEL_NAME;
}

static enum ui_text prompt_hint(const struct name_prompt *_Nonnull prompt)
{
    if (prompt->kind == NAME_PROMPT_RETRY_RENAME)
    {
        return UI_TEXT_PROMPT_HINT_RETRY_RENAME;
    }
    if (prompt->pending)
    {
        return UI_TEXT_PROMPT_HINT_PENDING;
    }
    switch (prompt->kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
        return UI_TEXT_PROMPT_HINT_LEAVE;
    case NAME_PROMPT_FIRST_SAVE:
        return UI_TEXT_PROMPT_HINT_FIRST_SAVE;
    case NAME_PROMPT_SAVE_AS:
        return UI_TEXT_PROMPT_HINT_SAVE_AS;
    case NAME_PROMPT_RENAME:
        return UI_TEXT_PROMPT_HINT_RENAME;
    case NAME_PROMPT_NEW_CATEGORY:
        return UI_TEXT_PROMPT_HINT_NEW_CATEGORY;
    case NAME_PROMPT_RENAME_CATEGORY:
        return UI_TEXT_PROMPT_HINT_RENAME_CATEGORY;
    case NAME_PROMPT_RETRY_RENAME:
        return UI_TEXT_PROMPT_HINT_RETRY_RENAME;
    }
    return UI_TEXT_PROMPT_HINT_FIRST_SAVE;
}

static enum ui_text prompt_accept(const struct name_prompt *_Nonnull prompt)
{
    if (prompt->pending)
    {
        return UI_TEXT_PROMPT_ACCEPT_RETRY;
    }
    switch (prompt->kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        return UI_TEXT_PROMPT_ACCEPT_SAVE;
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_RENAME_CATEGORY:
        return UI_TEXT_PROMPT_ACCEPT_RENAME;
    case NAME_PROMPT_NEW_CATEGORY:
        return UI_TEXT_PROMPT_ACCEPT_CREATE;
    case NAME_PROMPT_RETRY_RENAME:
        return UI_TEXT_PROMPT_ACCEPT_RETRY;
    }
    return UI_TEXT_PROMPT_ACCEPT_SAVE;
}

static enum ui_text prompt_close(const struct name_prompt *_Nonnull prompt)
{
    return prompt->pending ? UI_TEXT_PROMPT_CLOSE_PENDING : UI_TEXT_PROMPT_CLOSE_CANCEL;
}

static bool has_category_list(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        return true;
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RENAME_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return false;
    }
    return false;
}

static int category_row_height(const struct name_prompt *_Nonnull prompt)
{
    return scaled(prompt, 32);
}

static DWORD name_selection_style(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_RENAME_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return ES_NOHIDESEL;
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_NEW_CATEGORY:
        return 0;
    }
    return 0;
}

static RECT name_box(const struct name_prompt *_Nonnull prompt)
{
    return prompt->name_bounds;
}

static bool readonly_category(struct name_prompt *_Nonnull prompt)
{
    struct ui_text_request request = {
        .id = UI_TEXT_PROMPT_CATEGORY_READONLY,
        .language = prompt_language(prompt),
        .name =
            folio_state_category_name(prompt->state, folio_state_document_category(prompt->state))};
    char text[pending_line_capacity];
    char16_t units[pending_line_capacity];
    size_t written = 0;
    if (ui_text_format(&request, text, sizeof text) != UI_TEXT_FORMAT_READY ||
        utf16_text_fill(text, units, pending_line_capacity, &written) != UTF16_TEXT_FILL_READY)
    {
        return false;
    }
    SetWindowTextW(prompt->category, units);
    return true;
}

static bool inputs(struct name_prompt *_Nonnull prompt)
{
    prompt->name = control(prompt, L"EDIT", L"",
                           WS_TABSTOP | ES_AUTOHSCROLL | name_selection_style(prompt->kind));
    if (has_category_list(prompt->kind))
    {
        prompt->category = control(prompt, L"LISTBOX", L"",
                                   WS_TABSTOP | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY |
                                       LBS_NOINTEGRALHEIGHT | WS_VSCROLL);
        if (prompt->category == nullptr || !fill_categories(prompt))
            return false;
    }
    if (prompt->kind == NAME_PROMPT_RENAME)
    {
        prompt->category = control(prompt, L"EDIT", L"", WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL);
        if (prompt->category == nullptr || !readonly_category(prompt))
            return false;
    }
    prompt->failure =
        control(prompt, L"EDIT", L"", ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL);
    if (prompt->name == nullptr || prompt->failure == nullptr)
        return false;
    SendMessageW(prompt->name, EM_SETLIMITTEXT, 255, 0);
    SetWindowLongPtrW(prompt->name, GWLP_USERDATA, (LONG_PTR)prompt);
    prompt->original =
        (WNDPROC)SetWindowLongPtrW(prompt->name, GWLP_WNDPROC, (LONG_PTR)input_procedure);
    return prompt->original != nullptr;
}

static enum name_prompt_metrics_outcome
wrapped_height(HDC _Nonnull device, const char *_Nonnull text, int width, int *_Nonnull height)
{
    if (width <= 0)
        return NAME_PROMPT_METRICS_FAILED;
    if (text[0] == '\0')
    {
        *height = 0;
        return NAME_PROMPT_METRICS_READY;
    }
    char16_t units[pending_reason_capacity];
    size_t written = 0;
    if (utf16_text_fill(text, units, pending_reason_capacity, &written) != UTF16_TEXT_FILL_READY)
        return NAME_PROMPT_METRICS_FAILED;
    RECT bounds = {0, 0, width, 0};
    int drawn = DrawTextW(device, units, -1, &bounds,
                          DT_WORDBREAK | DT_CALCRECT | DT_NOPREFIX | DT_EXPANDTABS);
    if (drawn <= 0 || bounds.bottom <= 0)
        return NAME_PROMPT_METRICS_FAILED;
    *height = bounds.bottom;
    return NAME_PROMPT_METRICS_READY;
}

static int maximum_result_height(const struct name_prompt *_Nonnull prompt, int width)
{
    int maximum = 0;
    const struct name_prompt_metrics_request request = {.dialog = prompt->dialog,
                                                        .font = prompt->font,
                                                        .width = width,
                                                        .dpi = prompt->dpi,
                                                        .language = prompt_language(prompt)};
    if (name_prompt_metrics_failure_height(prompt->metrics, &request, &maximum) !=
        NAME_PROMPT_METRICS_READY)
        return 0;
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
    {
        name_prompt_metrics_invalidate(prompt->metrics);
        return 0;
    }
    HGDIOBJ previous = SelectObject(device, prompt->font);
    if (previous == nullptr || previous == HGDI_ERROR)
    {
        ReleaseDC(prompt->dialog, device);
        name_prompt_metrics_invalidate(prompt->metrics);
        return 0;
    }
    const enum ui_text ids[] = {prompt_hint(prompt), UI_TEXT_PROMPT_NAME_EMPTY,
                                UI_TEXT_PROMPT_PENDING_EXPLANATION};
    for (size_t at = 0; at < sizeof ids / sizeof ids[0]; ++at)
    {
        int height = 0;
        if (wrapped_height(device, ui_text_line(ids[at], prompt_language(prompt)), width,
                           &height) != NAME_PROMPT_METRICS_READY)
        {
            SelectObject(device, previous);
            ReleaseDC(prompt->dialog, device);
            name_prompt_metrics_invalidate(prompt->metrics);
            return 0;
        }
        maximum = height > maximum ? height : maximum;
    }
    SelectObject(device, previous);
    ReleaseDC(prompt->dialog, device);
    return maximum;
}

static int measure_button_width(const struct name_prompt *_Nonnull prompt)
{
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
        return 0;
    HGDIOBJ previous = SelectObject(device, prompt->font);
    HWND buttons[] = {prompt->accept, prompt->discard, prompt->cancel};
    int width = scaled(prompt, 88);
    for (size_t at = 0; at < sizeof buttons / sizeof buttons[0]; ++at)
    {
        if (buttons[at] == nullptr)
            continue;
        wchar_t text[ui_text_unit_limit];
        GetWindowTextW(buttons[at], text, (int)ui_text_unit_limit);
        RECT line = {0};
        DrawTextW(device, text, -1, &line, DT_SINGLELINE | DT_CALCRECT);
        int candidate = ui_part_command_width(line.right, false, prompt->dpi);
        width = candidate > width ? candidate : width;
    }
    SelectObject(device, previous);
    ReleaseDC(prompt->dialog, device);
    return width;
}

static bool stacked_labels(const struct name_prompt *_Nonnull prompt, int width)
{
    if (width < scaled(prompt, 480))
        return true;
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
        return true;
    HGDIOBJ previous = SelectObject(device, prompt->font);
    const enum ui_text ids[] = {UI_TEXT_PROMPT_LABEL_NAME, UI_TEXT_PROMPT_LABEL_CATEGORY};
    bool stacked = false;
    for (size_t at = 0; at < sizeof ids / sizeof ids[0]; ++at)
    {
        wchar_t text[ui_text_unit_limit];
        wide_line(ids[at], prompt_language(prompt), text);
        SIZE measured = {0};
        GetTextExtentPoint32W(device, text, (int)wcslen(text), &measured);
        stacked = stacked || measured.cx > scaled(prompt, 80);
    }
    SelectObject(device, previous);
    ReleaseDC(prompt->dialog, device);
    return stacked;
}

static int plan_category(struct name_prompt *_Nonnull prompt, int width, int top)
{
    int edge = scaled(prompt, 16);
    int label = prompt->stacked ? scaled(prompt, 24) : 0;
    int left = prompt->stacked ? edge : scaled(prompt, 104);
    prompt->category_label_bounds =
        (RECT){edge, top, prompt->stacked ? width - edge : left - scaled(prompt, 8),
               top + (prompt->stacked ? label : scaled(prompt, 32))};
    int height = prompt->rows > 0 ? prompt->rows * category_row_height(prompt) +
                                        ui_part_focus_width(prompt->dpi) * 2
                                  : scaled(prompt, 32);
    prompt->category_bounds = (RECT){left, top + label, width - edge, top + label + height};
    return prompt->category_bounds.bottom;
}

static int plan_fields(struct name_prompt *_Nonnull prompt, int width)
{
    int edge = scaled(prompt, 16);
    int top = scaled(prompt, 60);
    int label = prompt->stacked ? scaled(prompt, 24) : 0;
    int left = prompt->stacked ? edge : scaled(prompt, 104);
    prompt->name_label_bounds =
        (RECT){edge, top, prompt->stacked ? width - edge : left - scaled(prompt, 8),
               top + (prompt->stacked ? label : scaled(prompt, 32))};
    prompt->name_bounds = (RECT){left, top + label, width - edge, top + label + scaled(prompt, 32)};
    top = prompt->name_bounds.bottom;
    if (prompt->category != nullptr)
        top = plan_category(prompt, width, top + scaled(prompt, 8));
    return top + scaled(prompt, 8);
}

static bool plan_dialog(struct name_prompt *_Nonnull prompt, HWND anchor, SIZE *_Nonnull size)
{
    HWND owner = modal_owner_root(prompt->dialog);
    RECT parent = {0, 0, scaled(prompt, 560), scaled(prompt, 360)};
    if (owner != nullptr)
        GetClientRect(owner, &parent);
    int maximum = parent.bottom - scaled(prompt, ui_part_surface_title_height + 32);
    RECT allowed = dialog_theme_popup_bounds(anchor, (SIZE){scaled(prompt, 480), maximum});
    int width = allowed.right - allowed.left;
    int limit = allowed.bottom - allowed.top;
    prompt->stacked = stacked_labels(prompt, width);
    prompt->rows = has_category_list(prompt->kind) ? 4 : 0;
    prompt->button_width = measure_button_width(prompt);
    prompt->body_height = maximum_result_height(prompt, width - scaled(prompt, 64));
    int top = plan_fields(prompt, width);
    int footer = scaled(prompt, 60);
    if (top + prompt->body_height + footer > limit && prompt->rows == 4)
    {
        prompt->rows = 2;
        top = plan_fields(prompt, width);
    }
    int room = limit - top - footer;
    if (room <= 0 || prompt->button_width <= 0 || prompt->body_height <= 0)
        return false;
    prompt->result_scroll = prompt->body_height > room;
    prompt->body_height = prompt->body_height < room ? prompt->body_height : room;
    prompt->result_bounds =
        (RECT){scaled(prompt, 16), top, width - scaled(prompt, 16), top + prompt->body_height};
    *size = (SIZE){width, top + prompt->body_height + footer};
    return true;
}

static void place_control(HWND window, RECT bounds)
{
    MoveWindow(window, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, TRUE);
}

static void place_buttons(const struct name_prompt *_Nonnull prompt, RECT client)
{
    int right = client.right - scaled(prompt, 16);
    int bottom = client.bottom - scaled(prompt, 16);
    int width = prompt->button_width;
    int gap = scaled(prompt, 8);
    place_control(prompt->cancel,
                  (RECT){right - width, bottom - scaled(prompt, 28), right, bottom});
    right -= width + gap;
    if (prompt->discard != nullptr)
    {
        place_control(prompt->discard,
                      (RECT){right - width, bottom - scaled(prompt, 28), right, bottom});
        right -= width + gap;
    }
    place_control(prompt->accept,
                  (RECT){right - width, bottom - scaled(prompt, 28), right, bottom});
    place_control(prompt->close, ui_part_surface_rect(client, prompt->dpi, UI_PART_SURFACE_CLOSE));
}

static void reflow_category(const struct name_prompt *_Nonnull prompt)
{
    if (prompt->category != nullptr)
    {
        RECT field = ui_part_field_edit(
            prompt->category_bounds,
            prompt->rows > 0 ? 0 : ui_part_field_text_height(prompt->category), prompt->dpi);
        place_control(prompt->category, field);
        place_control(prompt->category_label, prompt->category_label_bounds);
        if (prompt->rows == 0)
            ui_part_field_margins(prompt->category, prompt->dpi, 0);
    }
}

static void reflow_result(const struct name_prompt *_Nonnull prompt)
{
    bool failed = GetWindowTextLengthW(prompt->failure) > 0;
    RECT result = prompt->result_bounds;
    place_control(prompt->hint, result);
    if (!prompt->pending)
        result.left += scaled(prompt, 32);
    place_control(prompt->failure,
                  prompt->pending ? ui_part_field_edit(result, 0, prompt->dpi) : result);
    ShowWindow(prompt->hint, failed ? SW_HIDE : SW_SHOW);
    ShowWindow(prompt->failure, failed ? SW_SHOW : SW_HIDE);
    LONG_PTR style = GetWindowLongPtrW(prompt->failure, GWL_STYLE);
    SetWindowLongPtrW(prompt->failure, GWL_STYLE,
                      prompt->pending ? style | WS_TABSTOP : style & ~(LONG_PTR)WS_TABSTOP);
    /* 通常結果もmonitor縮退で予約高が不足した時は全文をscrollで読める。 */
    SendMessageW(prompt->failure, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, 0);
    ShowScrollBar(prompt->failure, SB_VERT, prompt->pending || prompt->result_scroll);
}

static void reflow_prompt(struct name_prompt *_Nonnull prompt)
{
    enum folio_language language = prompt_language(prompt);
    char16_t title[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(prompt_title(displayed_kind(prompt), language), title, ui_text_unit_limit,
                        &written) == UTF16_TEXT_FILL_READY)
        SetWindowTextW(prompt->dialog, title);
    show_line(prompt->hint, prompt_hint(prompt), language);
    RECT field = ui_part_field_edit(prompt->name_bounds, ui_part_field_text_height(prompt->name),
                                    prompt->dpi);
    place_control(prompt->name, field);
    ui_part_field_margins(prompt->name, prompt->dpi, 0);
    place_control(prompt->name_label, prompt->name_label_bounds);
    reflow_category(prompt);
    reflow_result(prompt);
    RECT client;
    GetClientRect(prompt->dialog, &client);
    place_buttons(prompt, client);
    RedrawWindow(prompt->dialog, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN);
}

static bool name_inputs(const struct name_prompt *_Nonnull prompt)
{
    char16_t name[ui_text_unit_limit];
    wide_line(prompt_name_label(displayed_kind(prompt)), prompt_language(prompt), name);
    if (FAILED(accessibility_native_name(prompt->name, name)))
    {
        return false;
    }
    if (prompt->category == nullptr)
    {
        return true;
    }
    wide_line(UI_TEXT_PROMPT_LABEL_CATEGORY, prompt_language(prompt), name);
    return SUCCEEDED(accessibility_native_name(prompt->category, name));
}

static bool refresh_font(struct name_prompt *_Nonnull prompt)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(prompt_language(prompt), face);
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

static bool make_labels_and_buttons(struct name_prompt *_Nonnull prompt)
{
    prompt->hint = label(prompt, prompt_hint(prompt), (RECT){0}, 0);
    prompt->name_label = label(prompt, UI_TEXT_PROMPT_LABEL_NAME, (RECT){0}, 0);
    if (prompt->category != nullptr)
        prompt->category_label = label(prompt, UI_TEXT_PROMPT_LABEL_CATEGORY, (RECT){0}, 0);
    if (!button(prompt, prompt_accept(prompt), IDOK, (RECT){0}))
        return false;
    if (prompt->kind == NAME_PROMPT_LEAVE_UNTITLED &&
        !button(prompt, UI_TEXT_PROMPT_DISCARD, discard_identity, (RECT){0}))
        return false;
    if (!button(prompt, prompt_close(prompt), IDCANCEL, (RECT){0}) ||
        dialog_theme_create_close(prompt->dialog, prompt->font, prompt_language(prompt),
                                  &prompt->close) != DIALOG_THEME_CONTROL_READY)
        return false;
    return prompt->hint != nullptr && prompt->name_label != nullptr &&
           (prompt->category == nullptr || prompt->category_label != nullptr);
}

static bool initialize(struct name_prompt *_Nonnull prompt, HWND dialog)
{
    prompt->dialog = dialog;
    prompt->dpi = GetDpiForWindow(dialog);
    dialog_theme_decorate(prompt->theme, dialog);
    if (!refresh_font(prompt) || !inputs(prompt) || !fill_name(prompt) || !name_inputs(prompt))
        return false;
    if (!make_labels_and_buttons(prompt))
        return false;
    SIZE size;
    if (!plan_dialog(prompt, modal_owner_root(dialog), &size))
        return false;
    RECT bounds = dialog_theme_popup_bounds(modal_owner_root(dialog), size);
    MoveWindow(dialog, bounds.left, bounds.top, bounds.right - bounds.left,
               bounds.bottom - bounds.top, FALSE);
    return true;
}

/* ノートの種別は同じ名前型を使う（ADR 0022 の決定 1）。".md" はここで剥がれる。 */
static enum folio_state_outcome note_name_from(const struct utf8_text *_Nonnull narrow,
                                               struct note_name *_Nullable *_Nonnull name)
{
    enum note_name_outcome accepted =
        note_name_create(utf8_text_bytes(narrow), utf8_text_length(narrow), name);
    if (accepted != NOTE_NAME_ACCEPTED)
    {
        return accepted == NOTE_NAME_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_INVALID_NAME;
    }
    return FOLIO_STATE_READY;
}

static enum folio_state_outcome store_note(struct name_prompt *_Nonnull prompt,
                                           const struct utf8_text *_Nonnull narrow)
{
    struct note_name *_Nullable name = nullptr;
    enum folio_state_outcome result = note_name_from(narrow, &name);
    if (result != FOLIO_STATE_READY)
    {
        return result;
    }
    LRESULT category = SendMessageW(prompt->category, LB_GETCURSEL, 0, 0);
    struct note_destination destination = {.category = (size_t)category, .name = name};
    result = folio_state_store_new(prompt->state, &destination, prompt->units, prompt->count);
    note_name_destroy(name);
    return result;
}

static enum folio_state_outcome rename_note(struct name_prompt *_Nonnull prompt,
                                            const struct utf8_text *_Nonnull narrow)
{
    struct note_name *_Nullable name = nullptr;
    enum folio_state_outcome result = note_name_from(narrow, &name);
    if (result != FOLIO_STATE_READY)
    {
        return result;
    }
    result = folio_state_rename_note(prompt->state, name, prompt->units, prompt->count);
    note_name_destroy(name);
    return result;
}

/* カテゴリの種別はカテゴリ名の型を使う。".md" は剥がさない（ADR 0039 の決定 10）。 */
static enum folio_state_outcome create_category(struct name_prompt *_Nonnull prompt,
                                                const struct utf8_text *_Nonnull narrow)
{
    struct category_name *_Nullable name = nullptr;
    switch (category_name_create(utf8_text_bytes(narrow), utf8_text_length(narrow), &name))
    {
    case CATEGORY_NAME_ACCEPTED:
        break;
    case CATEGORY_NAME_INVALID:
        return FOLIO_STATE_CATEGORY_NAME_INVALID;
    case CATEGORY_NAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome result = folio_state_create_category(prompt->state, name);
    category_name_destroy(name);
    return result;
}

static enum folio_state_outcome rename_category(struct name_prompt *_Nonnull prompt,
                                                const struct utf8_text *_Nonnull narrow)
{
    struct category_name *_Nullable name = nullptr;
    switch (category_name_create(utf8_text_bytes(narrow), utf8_text_length(narrow), &name))
    {
    case CATEGORY_NAME_ACCEPTED:
        break;
    case CATEGORY_NAME_INVALID:
        return FOLIO_STATE_CATEGORY_NAME_INVALID;
    case CATEGORY_NAME_OUT_OF_MEMORY:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct category_rename_target target = {.category = prompt->target, .name = name};
    enum folio_state_outcome result =
        folio_state_rename_category(prompt->state, &target, prompt->units, prompt->count);
    category_name_destroy(name);
    return result;
}

/* 名前欄の字を UTF-8 にし、種別ごとの名前の型と意図へ渡す。 */
static enum folio_state_outcome save(struct name_prompt *_Nonnull prompt)
{
    wchar_t buffer[256];
    int count = GetWindowTextW(prompt->name, buffer, 256);
    struct utf8_text *_Nullable narrow = nullptr;
    if (utf8_text_create(buffer, (size_t)count, &narrow) != UTF8_TEXT_CONVERTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    enum folio_state_outcome result = FOLIO_STATE_CANCELLED;
    switch (prompt->kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        result = store_note(prompt, narrow);
        break;
    case NAME_PROMPT_RENAME:
        result = rename_note(prompt, narrow);
        break;
    case NAME_PROMPT_NEW_CATEGORY:
        result = create_category(prompt, narrow);
        break;
    case NAME_PROMPT_RENAME_CATEGORY:
        result = rename_category(prompt, narrow);
        break;
    case NAME_PROMPT_RETRY_RENAME:
        result = FOLIO_STATE_RENAME_PENDING;
        break;
    }
    utf8_text_destroy(narrow);
    return result;
}

/* 保持している意図をそのまま再開する。名前欄は固定なので読まない（ADR 0022 の決定 7）。 */
static enum folio_state_outcome retry_pending(struct name_prompt *_Nonnull prompt)
{
    return folio_state_retry_rename(prompt->state);
}

/* 止まった理由の 1 行と、閉じても取り消しにならないことを同じ欄で見せる。 */
static void show_pending_reason(struct name_prompt *_Nonnull prompt,
                                enum folio_state_outcome outcome)
{
    enum folio_language language = prompt_language(prompt);
    if (outcome == FOLIO_STATE_RENAME_PENDING)
    {
        show_line(prompt->failure, UI_TEXT_PROMPT_PENDING_EXPLANATION, language);
        return;
    }
    const char *_Nonnull reason = folio_state_failure_line(outcome, language);
    const char *_Nonnull explanation = ui_text_line(UI_TEXT_PROMPT_PENDING_EXPLANATION, language);
    size_t reason_length = strlen(reason);
    size_t explanation_length = strlen(explanation);
    char line[pending_reason_capacity];
    char16_t units[pending_reason_capacity];
    size_t written = 0;
    if (reason_length + explanation_length + 2 <= pending_reason_capacity)
    {
        memcpy(line, reason, reason_length);
        line[reason_length] = ' ';
        memcpy(line + reason_length + 1, explanation, explanation_length + 1);
        if (utf16_text_fill(line, units, pending_reason_capacity, &written) ==
            UTF16_TEXT_FILL_READY)
        {
            SetWindowTextW(prompt->failure, units);
            return;
        }
    }
    /* 収まらないときは説明だけを出す（現行と同じ）。 */
    show_line(prompt->failure, UI_TEXT_PROMPT_PENDING_EXPLANATION, language);
}

/* 記録を公開した改名は、名前を固定して同じ意図の再開だけを受ける（ADR 0022 の決定 7）。
 * 「閉じる」は意図の取り消しではないので、そのことを面の中で言う。 */
static bool hold_pending(struct name_prompt *_Nonnull prompt, enum folio_state_outcome outcome)
{
    struct rename_view pending = {.from = "", .to = ""};
    if (!folio_state_rename_pending(prompt->state, &pending))
    {
        return false;
    }
    if (!fill_pending(prompt, pending))
    {
        prompt->outcome = FOLIO_STATE_OUT_OF_MEMORY;
        return false;
    }
    show_line(prompt->accept, prompt_accept(prompt), prompt_language(prompt));
    show_line(prompt->cancel, prompt_close(prompt), prompt_language(prompt));
    show_pending_reason(prompt, outcome);
    reflow_prompt(prompt);
    return true;
}

/* 面を閉じる結果か。ノートは新しい md を公開できたときで、LEDGER_STALE は公開後の台帳の失敗。
 * カテゴリはフォルダを作れたときで、CATEGORY_LEDGER_STALE は作った後の台帳の失敗
 * （ADR 0039 の補正 1）。 */
static bool closes_on(enum name_prompt_kind kind, enum folio_state_outcome outcome)
{
    switch (kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        return outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_LEDGER_STALE;
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_RENAME_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return outcome == FOLIO_STATE_READY;
    case NAME_PROMPT_NEW_CATEGORY:
        return outcome == FOLIO_STATE_READY || outcome == FOLIO_STATE_CATEGORY_LEDGER_STALE;
    }
    return false;
}

/* 今回STARTを持つNOTE/CATEGORYだけが同じ面の固定retryへ進む（ADR0041）。 */
static bool holds_pending(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return false;
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_RENAME_CATEGORY:
        return true;
    }
    return false;
}

/* 閉じない結果では入力を残して理由を見せる。ノートの LEDGER_UNSYNCED は何も作れていないので、
 * NAME_TAKEN と同じく残る。 */
static void show_failure(struct name_prompt *_Nonnull prompt, enum folio_state_outcome outcome)
{
    const char *_Nonnull reason = folio_state_failure_line(outcome, prompt_language(prompt));
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(reason, units, ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY)
    {
        SetWindowTextW(prompt->failure, units);
    }
    reflow_prompt(prompt);
    SetFocus(prompt->name);
}

/* retryを越してviewを借りない。採用後OOMでplanが消えていたら面を終了して呼出し元へ返す。 */
static void submit_retry(struct name_prompt *_Nonnull prompt)
{
    prompt->outcome = retry_pending(prompt);
    if (prompt->outcome == FOLIO_STATE_READY || !hold_pending(prompt, prompt->outcome))
    {
        EndDialog(prompt->dialog, IDOK);
        return;
    }
    SetFocus(prompt->accept);
}

static bool owns_started_pending(const struct name_prompt *_Nonnull prompt)
{
    struct rename_view pending = {.from = "", .to = ""};
    if (!folio_state_rename_pending(prompt->state, &pending))
    {
        return false;
    }
    switch (prompt->kind)
    {
    case NAME_PROMPT_RENAME:
        return pending.kind == RENAME_KIND_NOTE;
    case NAME_PROMPT_RENAME_CATEGORY:
        return pending.kind == RENAME_KIND_CATEGORY;
    case NAME_PROMPT_LEAVE_UNTITLED:
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return false;
    }
    return false;
}

static void submit_new(struct name_prompt *_Nonnull prompt)
{
    enum folio_state_outcome saved = save(prompt);
    /* Publication can succeed before cache/index repair fails; this is no longer a discardable
     * draft. */
    if (prompt->kind == NAME_PROMPT_LEAVE_UNTITLED &&
        folio_state_document_kind(prompt->state) != FOLIO_DOCUMENT_UNTITLED)
    {
        prompt->outcome = saved;
        EndDialog(prompt->dialog, IDOK);
        return;
    }
    if (closes_on(prompt->kind, saved))
    {
        prompt->outcome = saved;
        EndDialog(prompt->dialog, IDOK);
        return;
    }
    if (holds_pending(prompt->kind) && owns_started_pending(prompt))
    {
        prompt->outcome = saved;
        if (!hold_pending(prompt, saved))
        {
            EndDialog(prompt->dialog, IDOK);
            return;
        }
        SetFocus(prompt->accept);
        return;
    }
    show_failure(prompt, saved);
}

static void submit(struct name_prompt *_Nonnull prompt)
{
    if (prompt->pending)
    {
        submit_retry(prompt);
        return;
    }
    if (GetWindowTextLengthW(prompt->name) == 0)
    {
        show_line(prompt->failure, UI_TEXT_PROMPT_NAME_EMPTY, prompt_language(prompt));
        reflow_prompt(prompt);
        SetFocus(prompt->name);
        return;
    }
    struct rename_view prior = {.from = "", .to = ""};
    if (prompt->kind != NAME_PROMPT_NEW_CATEGORY &&
        folio_state_rename_pending(prompt->state, &prior))
    {
        /* 面を開いた後の先行意図は入力を上書きせず、このsubmitでは保存しない。 */
        show_failure(prompt, FOLIO_STATE_RENAME_PENDING);
        return;
    }
    submit_new(prompt);
}

static void begin_dialog(struct name_prompt *_Nonnull prompt, HWND dialog)
{
    SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)prompt);
    if (!initialize(prompt, dialog))
    {
        prompt->outcome = FOLIO_STATE_OUT_OF_MEMORY;
        EndDialog(dialog, IDCANCEL);
        return;
    }
    reflow_prompt(prompt);
    /* 固定状態は読取専用。初期focusは再試行で、欄の全文は選択/横移動で閲覧できる。 */
    SetFocus(prompt->pending ? prompt->accept : prompt->name);
}

/* WM_CTLCOLOR* の相手を役割に変える。無効な EDIT は WM_CTLCOLORSTATIC で来るが、中の面なので
 * FIELD（ADR 0035 の決定 2・3）。保存先カテゴリの一覧は無効でも WM_CTLCOLORLISTBOX で来るので
 * LIST（地は pane・補正 17）。 */
static enum dialog_theme_surface surface_for(const struct name_prompt *_Nonnull prompt,
                                             UINT message, HWND child)
{
    if (message == WM_CTLCOLORDLG)
    {
        return DIALOG_THEME_DIALOG;
    }
    if (message == WM_CTLCOLORLISTBOX)
    {
        return DIALOG_THEME_LIST;
    }
    if (message == WM_CTLCOLOREDIT || child == prompt->name)
    {
        return DIALOG_THEME_FIELD;
    }
    if (child == prompt->category || (child == prompt->failure && prompt->pending))
        return DIALOG_THEME_FIELD;
    if (child == prompt->hint)
        return DIALOG_THEME_SECONDARY;
    return DIALOG_THEME_LABEL;
}

/* 一覧の行の字を取って描く。空の一覧のフォーカス（項目 -1）なら空の字で地だけ塗る。 */
static void draw_category(const struct name_prompt *_Nonnull prompt,
                          const DRAWITEMSTRUCT *_Nonnull item)
{
    wchar_t units[pending_line_capacity];
    units[0] = L'\0';
    LRESULT length = SendMessageW(item->hwndItem, LB_GETTEXTLEN, item->itemID, 0);
    if (item->itemID != (UINT)-1 && length >= 0 && length < (LRESULT)pending_line_capacity &&
        SendMessageW(item->hwndItem, LB_GETTEXT, item->itemID, (LPARAM)units) == LB_ERR)
    {
        units[0] = L'\0';
    }
    dialog_theme_draw_item(prompt->theme, item, units);
}

static void draw_owned(const struct name_prompt *_Nonnull prompt,
                       const DRAWITEMSTRUCT *_Nonnull item)
{
    if (item->CtlType == ODT_LISTBOX)
    {
        draw_category(prompt, item);
        return;
    }
    /* 既定は「保存」「変更」「再試行」の側（ADR 0035 の決定 3）。 */
    enum dialog_theme_button kind =
        item->CtlID == IDOK ? DIALOG_THEME_BUTTON_PRIMARY : DIALOG_THEME_BUTTON_SECONDARY;
    if (item->CtlID == IDCLOSE)
        kind = DIALOG_THEME_BUTTON_CLOSE;
    dialog_theme_draw_button(prompt->theme, item, kind);
}

/* 一覧の行の高さ。WM_MEASUREITEM は inputs() の作る途中で来るので書体から測る。 */
static void measure_category(const struct name_prompt *_Nonnull prompt,
                             MEASUREITEMSTRUCT *_Nonnull item)
{
    item->itemHeight = (UINT)category_row_height(prompt);
}

static RECT child_bounds(const struct name_prompt *_Nonnull prompt, HWND window)
{
    RECT bounds;
    GetWindowRect(window, &bounds);
    MapWindowPoints(nullptr, prompt->dialog, (POINT *)&bounds, 2);
    return bounds;
}

static RECT field_bounds(const struct name_prompt *_Nonnull prompt, HWND window)
{
    if (window == prompt->name)
    {
        return name_box(prompt);
    }
    return window == prompt->failure ? prompt->result_bounds : prompt->category_bounds;
}

static void frame_around(const struct name_prompt *_Nonnull prompt, HDC device, HWND window)
{
    RECT bounds = field_bounds(prompt, window);
    int focus = GetFocus() == window ? ui_part_focus_width(GetDpiForWindow(prompt->dialog)) : 0;
    /* WM_PRINTCLIENTも同じ経路を通るので子の矩形を除いて箱だけを塗る。 */
    RECT child = child_bounds(prompt, window);
    int saved = SaveDC(device);
    ExcludeClipRect(device, child.left, child.top, child.right, child.bottom);
    dialog_theme_field(prompt->theme, device, &bounds, focus);
    if (saved != 0)
    {
        RestoreDC(device, saved);
    }
}

/* focus の出入りで枠の太さが変わるので、枠だけを描き直す。一覧は外側の輪、名前欄は箱（#349）。 */
static void refresh_frame(const struct name_prompt *_Nonnull prompt, HWND window)
{
    RECT box = field_bounds(prompt, window);
    InvalidateRect(prompt->dialog, &box, FALSE);
}

static bool field_focus_changed(const struct name_prompt *_Nonnull prompt, WPARAM wparam,
                                LPARAM lparam)
{
    HWND window = (HWND)lparam;
    WORD code = HIWORD(wparam);
    bool name = window == prompt->name && (code == EN_SETFOCUS || code == EN_KILLFOCUS);
    bool category = prompt->category != nullptr && window == prompt->category &&
                    (code == LBN_SETFOCUS || code == LBN_KILLFOCUS || code == EN_SETFOCUS ||
                     code == EN_KILLFOCUS);
    bool reason = prompt->pending && window == prompt->failure &&
                  (code == EN_SETFOCUS || code == EN_KILLFOCUS);
    if (name || category || reason)
    {
        refresh_frame(prompt, window);
    }
    return name || category || reason;
}

/* WS_BORDER を外した名前欄（箱の内側）と一覧（外側）に入力欄の枠を描く（draw_filter_frame と同じ
 * ui_part の 1 本・ ADR 0035 の補正 17 を ADR0060 の決定 3 で置き換えた）。 */
static void paint_contents(const struct name_prompt *_Nonnull prompt, HDC device)
{
    dialog_theme_draw_title(prompt->theme, prompt->dialog, device, prompt->font);
    frame_around(prompt, device, prompt->name);
    if (prompt->category != nullptr)
        frame_around(prompt, device, prompt->category);
    if (prompt->pending)
        frame_around(prompt, device, prompt->failure);
    if (!prompt->pending && GetWindowTextLengthW(prompt->failure) > 0)
    {
        RECT mark = prompt->result_bounds;
        mark.right = mark.left + scaled(prompt, 24);
        mark.bottom = mark.top + scaled(prompt, 24);
        dialog_theme_draw_warning(prompt->theme, device, mark);
    }
}

static void paint_frame(const struct name_prompt *_Nonnull prompt)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(prompt->dialog, &paint);
    if (device == nullptr)
    {
        return;
    }
    paint_contents(prompt, device);
    EndPaint(prompt->dialog, &paint);
}

static void follow_dpi(struct name_prompt *_Nonnull prompt, const RECT *_Nonnull suggested)
{
    SetWindowPos(prompt->dialog, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    prompt->dpi = GetDpiForWindow(prompt->dialog);
    SIZE size;
    if (!refresh_font(prompt) || !plan_dialog(prompt, prompt->dialog, &size))
    {
        prompt->outcome = FOLIO_STATE_OUT_OF_MEMORY;
        EndDialog(prompt->dialog, IDCANCEL);
        return;
    }
    if (prompt->rows > 0)
        SendMessageW(prompt->category, LB_SETITEMHEIGHT, 0, category_row_height(prompt));
    RECT fitted = dialog_theme_popup_bounds(prompt->dialog, size);
    MoveWindow(prompt->dialog, fitted.left, fitted.top, fitted.right - fitted.left,
               fitted.bottom - fitted.top, FALSE);
    reflow_prompt(prompt);
}

/* ownerのresizeは既存font/同じ計測cacheで再flowする。 */
static void reflow_owner(struct name_prompt *_Nonnull prompt)
{
    SIZE size;
    if (!plan_dialog(prompt, prompt->dialog, &size))
    {
        prompt->outcome = FOLIO_STATE_OUT_OF_MEMORY;
        EndDialog(prompt->dialog, IDCANCEL);
        return;
    }
    RECT fitted = dialog_theme_popup_bounds(prompt->dialog, size);
    MoveWindow(prompt->dialog, fitted.left, fitted.top, fitted.right - fitted.left,
               fitted.bottom - fitted.top, FALSE);
    reflow_prompt(prompt);
}

/* 塗りのメッセージを dialog_theme へ渡す。IME の門より前で答えるので組成中も色が抜けない
 * （ADR 0035 の決定 3）。扱わないメッセージは FALSE で、扱ったものは 0 でない値を返す。 */
static INT_PTR paint_message(struct name_prompt *_Nonnull prompt, UINT message, WPARAM wparam,
                             LPARAM lparam)
{
    if (prompt->theme == nullptr || prompt->dialog == nullptr)
    {
        return FALSE;
    }
    switch (message)
    {
    case WM_SYSCOLORCHANGE:
    case WM_SETTINGCHANGE:
        dialog_theme_follow(prompt->dialog, prompt->state, &prompt->theme);
        return TRUE;
    case WM_CTLCOLORDLG:
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
        return (INT_PTR)dialog_theme_color(prompt->theme,
                                           surface_for(prompt, message, (HWND)lparam), (HDC)wparam);
    case WM_DRAWITEM:
        draw_owned(prompt, (const DRAWITEMSTRUCT *)lparam);
        return TRUE;
    case WM_MEASUREITEM:
        measure_category(prompt, (MEASUREITEMSTRUCT *)lparam);
        return TRUE;
    case WM_PAINT:
        paint_frame(prompt);
        return TRUE;
    case WM_PRINTCLIENT:
        paint_contents(prompt, (HDC)wparam);
        return TRUE;
    case WM_DPICHANGED:
        follow_dpi(prompt, (const RECT *)lparam);
        return TRUE;
    case folio_message_modal_reflow:
        reflow_owner(prompt);
        return TRUE;
    default:
        return FALSE;
    }
}

/* owner-draw の釦は既定釦の鍵を答えないので、取消しの釦にフォーカスがあるときの Enter も
 * IDOK で来る。そのときは取消しとして読む（ADR 0035 の決定 3・V5）。クリックは WM_COMMAND の前に
 * フォーカスを押した釦へ移すので、OK のクリックを読み替えることはない。 */
static bool reads_as_cancel(const struct name_prompt *_Nonnull prompt, WPARAM wparam)
{
    return LOWORD(wparam) == IDCANCEL || LOWORD(wparam) == IDCLOSE ||
           (LOWORD(wparam) == IDOK && HIWORD(wparam) == BN_CLICKED &&
            (GetFocus() == prompt->cancel || GetFocus() == prompt->close));
}

/* 保存先カテゴリの行のダブルクリックは「保存」と同じ経路へ読む（ADR 0035 の補正 19）。 */
static bool reads_as_discard(const struct name_prompt *_Nonnull prompt, WPARAM wparam)
{
    return prompt->discard != nullptr &&
           folio_state_document_kind(prompt->state) == FOLIO_DOCUMENT_UNTITLED &&
           (LOWORD(wparam) == discard_identity ||
            (LOWORD(wparam) == IDOK && HIWORD(wparam) == BN_CLICKED &&
             GetFocus() == prompt->discard));
}

static bool reads_as_accept(const struct name_prompt *_Nonnull prompt, WPARAM wparam, LPARAM lparam)
{
    return LOWORD(wparam) == IDOK ||
           (prompt->category != nullptr && (HWND)lparam == prompt->category &&
            HIWORD(wparam) == LBN_DBLCLK);
}

static INT_PTR prompt_command(struct name_prompt *_Nonnull prompt, WPARAM wparam, LPARAM lparam)
{
    if (reads_as_cancel(prompt, wparam))
    {
        EndDialog(prompt->dialog, IDCANCEL);
        return TRUE;
    }
    if (reads_as_discard(prompt, wparam))
    {
        prompt->outcome = FOLIO_STATE_READY;
        prompt->untitled = FOLIO_UNTITLED_DISCARD;
        EndDialog(prompt->dialog, discard_identity);
        return TRUE;
    }
    if (reads_as_accept(prompt, wparam, lparam))
    {
        submit(prompt);
        return TRUE;
    }
    return FALSE;
}

static INT_PTR cancel_from_owner(struct name_prompt *_Nonnull prompt)
{
    SetWindowLongPtrW(prompt->dialog, DWLP_MSGRESULT, prompt->composing ? 2 : 1);
    if (!prompt->composing)
        EndDialog(prompt->dialog, IDCANCEL);
    return TRUE;
}

static INT_PTR operation_message(struct name_prompt *_Nonnull prompt, UINT message, WPARAM wparam,
                                 LPARAM lparam)
{
    if (message == folio_message_modal_cancel)
        return cancel_from_owner(prompt);
    if (message == WM_COMMAND && field_focus_changed(prompt, wparam, lparam))
        return FALSE;
    if (prompt->composing && (message == WM_COMMAND || message == WM_CLOSE))
        return TRUE;
    if (message == WM_CLOSE)
    {
        EndDialog(prompt->dialog, IDCANCEL);
        return TRUE;
    }
    return message == WM_COMMAND ? prompt_command(prompt, wparam, lparam) : FALSE;
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (dialog_theme_native_message(dialog, message, wparam, lparam))
        return TRUE;
    if (message == WM_INITDIALOG)
    {
        begin_dialog((struct name_prompt *)lparam, dialog);
        return FALSE;
    }
    struct name_prompt *_Nullable prompt =
        (struct name_prompt *)GetWindowLongPtrW(dialog, DWLP_USER);
    if (prompt == nullptr)
    {
        return FALSE;
    }
    INT_PTR painted = paint_message(prompt, message, wparam, lparam);
    if (painted != FALSE)
    {
        return painted;
    }
    return operation_message(prompt, message, wparam, lparam);
}

struct name_prompt_result name_prompt_show(HWND _Nonnull owner,
                                           const struct name_prompt_request *_Nonnull request,
                                           const struct folio_palette *_Nonnull palette,
                                           struct name_prompt_metrics *_Nonnull metrics)
{
    struct name_prompt prompt = {.state = request->state,
                                 .kind = request->kind,
                                 .target = request->target,
                                 .units = request->units,
                                 .count = request->count,
                                 .metrics = metrics,
                                 .outcome = FOLIO_STATE_CANCELLED,
                                 .untitled = FOLIO_UNTITLED_PROTECT};
    /* 面を作る前に palette を写す。子の WM_CTLCOLOR* は作る途中から来て、
     * OS色通知で主窓の実効paletteへ追随する（ADR0057）。 */
    switch (dialog_theme_create(palette, &prompt.theme))
    {
    case DIALOG_THEME_READY:
        break;
    case DIALOG_THEME_NO_MEMORY:
    case DIALOG_THEME_NO_BRUSH:
        return (struct name_prompt_result){FOLIO_STATE_OUT_OF_MEMORY, FOLIO_UNTITLED_PROTECT};
    }
    INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                             procedure, (LPARAM)&prompt);
    /* ブラシと書体は面が返った直後に捨てる（ADR 0035 の決定 2）。 */
    dialog_theme_destroy(prompt.theme);
    if (prompt.font != nullptr)
    {
        DeleteObject(prompt.font);
    }
    return (struct name_prompt_result){result == -1 ? FOLIO_STATE_OUT_OF_MEMORY : prompt.outcome,
                                       prompt.untitled};
}
