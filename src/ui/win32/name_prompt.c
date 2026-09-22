#include "name_prompt.h"
#include "dialog_theme.h"
#include "folio_palette.h"
#include "folio_state.h"
#include "note_name.h"
#include "ui_face.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <string.h>

struct name_prompt
{
    struct folio_state *_Nonnull state;
    enum name_prompt_kind kind;
    const char16_t *_Nonnull units;
    size_t count;
    HWND _Nullable dialog;
    HWND _Nullable name;
    HWND _Nullable category;
    HWND _Nullable failure;
    HWND _Nullable accept;
    HWND _Nullable cancel;
    WNDPROC _Nullable original;
    HFONT _Nullable font;
    /* 開く瞬間の palette を写した塗り。面は追随しない（ADR 0035 の決定 5）。 */
    const struct folio_palette *_Nonnull palette;
    struct dialog_theme *_Nullable theme;
    UINT dpi;
    bool composing;
    /* 記録を公開した後は名前を固定し、同じ改名の再開だけを受ける（ADR 0022 の決定 7）。 */
    bool pending;
    enum folio_state_outcome outcome;
};

static const struct
{
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WORD title;
} template = {.dialog = {.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_CENTER,
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
    return window;
}

static void position(const struct name_prompt *_Nonnull prompt, HWND window, RECT bounds)
{
    MoveWindow(window, scaled(prompt, bounds.left), scaled(prompt, bounds.top),
               scaled(prompt, bounds.right - bounds.left),
               scaled(prompt, bounds.bottom - bounds.top), TRUE);
}

static bool label(struct name_prompt *_Nonnull prompt, enum ui_text id, RECT bounds)
{
    char16_t units[ui_text_unit_limit];
    wide_line(id, prompt_language(prompt), units);
    HWND window = control(prompt, L"STATIC", units, SS_LEFT);
    if (window == nullptr)
    {
        return false;
    }
    position(prompt, window, bounds);
    return true;
}

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
    position(prompt, window, bounds);
    if (identity == IDOK)
    {
        prompt->accept = window;
    }
    else
    {
        prompt->cancel = window;
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
            SendMessageW(prompt->category, CB_ADDSTRING, 0, (LPARAM)utf16_text_units(wide));
        utf16_text_destroy(wide);
        if (added == CB_ERR || added == CB_ERRSPACE)
        {
            return false;
        }
    }
    SendMessageW(prompt->category, CB_SETCURSEL, folio_state_document_category(prompt->state), 0);
    if (prompt->kind == NAME_PROMPT_RENAME)
    {
        /* 改名はカテゴリを変えない（ADR 0022 の決定 1）。表示だけ残して選べなくする。 */
        EnableWindow(prompt->category, FALSE);
    }
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
    EnableWindow(prompt->name, FALSE);
    show_line(prompt->failure, UI_TEXT_PROMPT_PENDING_EXPLANATION, language);
    return true;
}

/* 改名は文書の実名を選択状態で出す。初回・別名保存は空のまま（決定 1）。 */
static bool fill_name(struct name_prompt *_Nonnull prompt)
{
    if (prompt->kind != NAME_PROMPT_RENAME)
    {
        return true;
    }
    struct rename_view pending = {.from = "", .to = ""};
    if (folio_state_rename_pending(prompt->state, &pending))
    {
        return fill_pending(prompt, pending);
    }
    if (!set_name_text(prompt, folio_state_document_name(prompt->state)))
    {
        return false;
    }
    SendMessageW(prompt->name, EM_SETSEL, 0, -1);
    return true;
}

static enum ui_text prompt_title(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
        return UI_TEXT_PROMPT_TITLE_FIRST_SAVE;
    case NAME_PROMPT_SAVE_AS:
        return UI_TEXT_PROMPT_TITLE_SAVE_AS;
    case NAME_PROMPT_RENAME:
        return UI_TEXT_PROMPT_TITLE_RENAME;
    }
    return UI_TEXT_PROMPT_TITLE_FIRST_SAVE;
}

static enum ui_text prompt_hint(const struct name_prompt *_Nonnull prompt)
{
    if (prompt->pending)
    {
        return UI_TEXT_PROMPT_HINT_PENDING;
    }
    switch (prompt->kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
        return UI_TEXT_PROMPT_HINT_FIRST_SAVE;
    case NAME_PROMPT_SAVE_AS:
        return UI_TEXT_PROMPT_HINT_SAVE_AS;
    case NAME_PROMPT_RENAME:
        return UI_TEXT_PROMPT_HINT_RENAME;
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
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        return UI_TEXT_PROMPT_ACCEPT_SAVE;
    case NAME_PROMPT_RENAME:
        return UI_TEXT_PROMPT_ACCEPT_RENAME;
    }
    return UI_TEXT_PROMPT_ACCEPT_SAVE;
}

static enum ui_text prompt_close(const struct name_prompt *_Nonnull prompt)
{
    return prompt->pending ? UI_TEXT_PROMPT_CLOSE_PENDING : UI_TEXT_PROMPT_CLOSE_CANCEL;
}

static bool inputs(struct name_prompt *_Nonnull prompt)
{
    /* 塗れない OS の縁を外し、面が札の色の 1px の枠を描く（ADR 0035 の決定 3）。 */
    prompt->name = control(prompt, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL);
    prompt->category =
        control(prompt, L"COMBOBOX", L"",
                WS_TABSTOP | CBS_DROPDOWNLIST | CBS_OWNERDRAWFIXED | CBS_HASSTRINGS | WS_VSCROLL);
    prompt->failure = control(prompt, L"STATIC", L"", SS_LEFT);
    if (prompt->name == nullptr || prompt->category == nullptr || prompt->failure == nullptr)
    {
        return false;
    }
    position(prompt, prompt->name, (RECT){20, 46, 380, 74});
    position(prompt, prompt->category, (RECT){20, 108, 380, 280});
    position(prompt, prompt->failure, (RECT){20, 166, 380, 226});
    SendMessageW(prompt->name, EM_SETLIMITTEXT, 255, 0);
    SetWindowLongPtrW(prompt->name, GWLP_USERDATA, (LONG_PTR)prompt);
    prompt->original =
        (WNDPROC)SetWindowLongPtrW(prompt->name, GWLP_WNDPROC, (LONG_PTR)input_procedure);
    return prompt->original != nullptr && fill_categories(prompt);
}

static bool initialize(struct name_prompt *_Nonnull prompt, HWND dialog)
{
    prompt->dialog = dialog;
    prompt->dpi = GetDpiForWindow(dialog);
    /* 子を作る前に塗りを用意する。子の WM_CTLCOLOR* は作る途中から来る（ADR 0035 の決定 3）。 */
    if (!dialog_theme_create(prompt->palette, &prompt->theme))
    {
        return false;
    }
    dialog_theme_decorate(prompt->theme, dialog);
    /* 面はモーダルなので、言語は開く瞬間に決まる（ADR 0032 の決定 4）。 */
    wchar_t face[LF_FACESIZE];
    ui_face_for(prompt_language(prompt), face);
    prompt->font = CreateFontW(-scaled(prompt, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    if (prompt->font == nullptr)
    {
        return false;
    }
    RECT bounds = {0, 0, scaled(prompt, 400), scaled(prompt, 270)};
    DWORD style = (DWORD)GetWindowLongPtrW(dialog, GWL_STYLE);
    AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, prompt->dpi);
    RECT owner;
    GetWindowRect(GetParent(dialog), &owner);
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    MoveWindow(dialog, (owner.left + owner.right - width) / 2,
               (owner.top + owner.bottom - height) / 2, width, height, FALSE);
    show_line(dialog, prompt_title(prompt->kind), prompt_language(prompt));
    return inputs(prompt) && fill_name(prompt) &&
           label(prompt, UI_TEXT_PROMPT_LABEL_NAME, (RECT){20, 20, 380, 42}) &&
           label(prompt, UI_TEXT_PROMPT_LABEL_CATEGORY, (RECT){20, 82, 380, 104}) &&
           label(prompt, prompt_hint(prompt), (RECT){20, 144, 380, 166}) &&
           button(prompt, prompt_accept(prompt), IDOK, (RECT){192, 230, 280, 258}) &&
           button(prompt, prompt_close(prompt), IDCANCEL, (RECT){288, 230, 380, 258});
}

/* 同じ名前型を使い、種類ごとの意図へ渡す（ADR 0022 の決定 1）。 */
static enum folio_state_outcome apply_name(struct name_prompt *_Nonnull prompt,
                                           const struct note_name *_Nonnull name)
{
    switch (prompt->kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    {
        LRESULT category = SendMessageW(prompt->category, CB_GETCURSEL, 0, 0);
        struct note_destination destination = {.category = (size_t)category, .name = name};
        return folio_state_store_new(prompt->state, &destination, prompt->units, prompt->count);
    }
    case NAME_PROMPT_RENAME:
        return folio_state_rename_note(prompt->state, name, prompt->units, prompt->count);
    }
    return FOLIO_STATE_CANCELLED;
}

static enum folio_state_outcome save(struct name_prompt *_Nonnull prompt)
{
    wchar_t buffer[256];
    int count = GetWindowTextW(prompt->name, buffer, 256);
    struct utf8_text *_Nullable narrow = nullptr;
    if (utf8_text_create(buffer, (size_t)count, &narrow) != UTF8_TEXT_CONVERTED)
    {
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
    struct note_name *_Nullable name = nullptr;
    enum note_name_outcome accepted =
        note_name_create(utf8_text_bytes(narrow), utf8_text_length(narrow), &name);
    utf8_text_destroy(narrow);
    if (accepted != NOTE_NAME_ACCEPTED)
    {
        return accepted == NOTE_NAME_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_INVALID_NAME;
    }
    enum folio_state_outcome result = apply_name(prompt, name);
    note_name_destroy(name);
    return result;
}

/* 保持している意図をそのまま再開する。名前欄は固定なので読まない（ADR 0022 の決定 7）。 */
static enum folio_state_outcome retry_pending(struct name_prompt *_Nonnull prompt)
{
    struct rename_view pending = {.from = "", .to = ""};
    if (!folio_state_rename_pending(prompt->state, &pending))
    {
        return FOLIO_STATE_READY;
    }
    struct note_name *_Nullable name = nullptr;
    enum note_name_outcome accepted = note_name_create(pending.to, strlen(pending.to), &name);
    if (accepted != NOTE_NAME_ACCEPTED)
    {
        return accepted == NOTE_NAME_OUT_OF_MEMORY ? FOLIO_STATE_OUT_OF_MEMORY
                                                   : FOLIO_STATE_INVALID_NAME;
    }
    enum folio_state_outcome outcome =
        folio_state_rename_note(prompt->state, name, prompt->units, prompt->count);
    note_name_destroy(name);
    return outcome;
}

/* 止まった理由の 1 行と、閉じても取り消しにならないことを同じ欄で見せる。 */
static void show_pending_reason(struct name_prompt *_Nonnull prompt,
                                enum folio_state_outcome outcome)
{
    enum folio_language language = prompt_language(prompt);
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
static void hold_pending(struct name_prompt *_Nonnull prompt, enum folio_state_outcome outcome)
{
    struct rename_view pending = {.from = "", .to = ""};
    if (!prompt->pending && folio_state_rename_pending(prompt->state, &pending))
    {
        (void)fill_pending(prompt, pending);
        show_line(prompt->accept, prompt_accept(prompt), prompt_language(prompt));
        show_line(prompt->cancel, prompt_close(prompt), prompt_language(prompt));
    }
    show_pending_reason(prompt, outcome);
}

/* 新しいmdを公開できたときだけ閉じる。LEDGER_STALE は公開後の台帳の失敗。
 * LEDGER_UNSYNCED は何も作れていないので、NAME_TAKEN と同じく入力を残して理由を見せる。 */
static void submit(struct name_prompt *_Nonnull prompt)
{
    enum folio_state_outcome saved = prompt->pending ? retry_pending(prompt) : save(prompt);
    if (saved == FOLIO_STATE_READY || saved == FOLIO_STATE_LEDGER_STALE)
    {
        prompt->outcome = saved;
        EndDialog(prompt->dialog, IDOK);
        return;
    }
    if (saved == FOLIO_STATE_RENAME_PENDING || saved == FOLIO_STATE_RENAME_HALTED)
    {
        prompt->outcome = saved;
        hold_pending(prompt, saved);
        SetFocus(prompt->accept);
        return;
    }
    const char *_Nonnull reason = folio_state_failure_line(saved, prompt_language(prompt));
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    if (utf16_text_fill(reason, units, ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY)
    {
        SetWindowTextW(prompt->failure, units);
    }
    SetFocus(prompt->name);
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
    /* 固定状態では名前欄が無効なので、押せるほうへ鍵を渡す（ADR 0022 の決定 7）。 */
    SetFocus(prompt->pending ? prompt->accept : prompt->name);
}

/* WM_CTLCOLOR* の相手を役割に変える。無効な EDIT とコンボは WM_CTLCOLORSTATIC で来るが、
 * 中の面なので FIELD（ADR 0035 の決定 2・3）。 */
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
    if (message == WM_CTLCOLOREDIT || child == prompt->category || child == prompt->name)
    {
        return DIALOG_THEME_FIELD;
    }
    return DIALOG_THEME_LABEL;
}

/* コンボの項目の字を取って描く。閉じた面が空（項目 -1）なら空の字で面だけ塗る。 */
static void draw_category(const struct name_prompt *_Nonnull prompt,
                          const DRAWITEMSTRUCT *_Nonnull item)
{
    wchar_t units[pending_line_capacity];
    units[0] = L'\0';
    LRESULT length = SendMessageW(item->hwndItem, CB_GETLBTEXTLEN, item->itemID, 0);
    if (item->itemID != (UINT)-1 && length >= 0 && length < (LRESULT)pending_line_capacity &&
        SendMessageW(item->hwndItem, CB_GETLBTEXT, item->itemID, (LPARAM)units) == CB_ERR)
    {
        units[0] = L'\0';
    }
    dialog_theme_draw_item(prompt->theme, item, units);
}

static void draw_owned(const struct name_prompt *_Nonnull prompt,
                       const DRAWITEMSTRUCT *_Nonnull item)
{
    if (item->CtlType == ODT_COMBOBOX)
    {
        draw_category(prompt, item);
        return;
    }
    /* 既定は「保存」「変更」「再試行」の側（ADR 0035 の決定 3）。 */
    dialog_theme_draw_button(prompt->theme, item,
                             item->CtlID == IDOK ? DIALOG_THEME_BUTTON_PRIMARY
                                                 : DIALOG_THEME_BUTTON_SECONDARY);
}

/* コンボの行の高さ。WM_MEASUREITEM は inputs() の作る途中で来るので書体から測る。 */
static void measure_category(const struct name_prompt *_Nonnull prompt,
                             MEASUREITEMSTRUCT *_Nonnull item)
{
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
    {
        item->itemHeight = (UINT)scaled(prompt, 22);
        return;
    }
    HGDIOBJ old_font = SelectObject(device, prompt->font);
    TEXTMETRICW metrics;
    if (GetTextMetricsW(device, &metrics))
    {
        item->itemHeight = (UINT)(metrics.tmHeight + scaled(prompt, 4));
    }
    else
    {
        item->itemHeight = (UINT)scaled(prompt, 22);
    }
    SelectObject(device, old_font);
    ReleaseDC(prompt->dialog, device);
}

/* WS_BORDER を外した名前欄の外側に札の色の 1px の枠を描く（draw_filter_frame と同じ流儀）。 */
static void paint_frame(const struct name_prompt *_Nonnull prompt)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(prompt->dialog, &paint);
    if (device == nullptr)
    {
        return;
    }
    RECT bounds;
    GetWindowRect(prompt->name, &bounds);
    MapWindowPoints(nullptr, prompt->dialog, (POINT *)&bounds, 2);
    InflateRect(&bounds, 1, 1);
    SetDCBrushColor(device, prompt->palette->chip_background);
    FrameRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    EndPaint(prompt->dialog, &paint);
}

static void follow_dpi(const struct name_prompt *_Nonnull prompt, const RECT *_Nonnull suggested)
{
    SetWindowPos(prompt->dialog, nullptr, suggested->left, suggested->top,
                 suggested->right - suggested->left, suggested->bottom - suggested->top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
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
    case WM_DPICHANGED:
        follow_dpi(prompt, (const RECT *)lparam);
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
    return LOWORD(wparam) == IDCANCEL ||
           (LOWORD(wparam) == IDOK && HIWORD(wparam) == BN_CLICKED && GetFocus() == prompt->cancel);
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
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
    if (painted != FALSE || prompt->composing)
    {
        return painted;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && reads_as_cancel(prompt, wparam)))
    {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK)
    {
        submit(prompt);
        return TRUE;
    }
    return FALSE;
}

enum folio_state_outcome name_prompt_show(HWND _Nonnull owner,
                                          const struct name_prompt_request *_Nonnull request,
                                          const struct folio_palette *_Nonnull palette)
{
    struct name_prompt prompt = {.state = request->state,
                                 .kind = request->kind,
                                 .units = request->units,
                                 .count = request->count,
                                 .palette = palette,
                                 .outcome = FOLIO_STATE_CANCELLED};
    INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                             procedure, (LPARAM)&prompt);
    /* ブラシと書体は面が返った直後に捨てる（ADR 0035 の決定 2）。 */
    dialog_theme_destroy(prompt.theme);
    if (prompt.font != nullptr)
    {
        DeleteObject(prompt.font);
    }
    return result == -1 ? FOLIO_STATE_OUT_OF_MEMORY : prompt.outcome;
}
