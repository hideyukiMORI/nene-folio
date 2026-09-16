#include "name_prompt.h"
#include "folio_state.h"
#include "note_name.h"
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
    WNDPROC _Nullable original;
    HFONT _Nullable font;
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

static int scaled(const struct name_prompt *_Nonnull prompt, int value)
{
    return MulDiv(value, (int)prompt->dpi, 96);
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

static bool label(struct name_prompt *_Nonnull prompt, const wchar_t *_Nonnull text, RECT bounds)
{
    HWND window = control(prompt, L"STATIC", text, SS_LEFT);
    if (window == nullptr)
    {
        return false;
    }
    position(prompt, window, bounds);
    return true;
}

static bool button(struct name_prompt *_Nonnull prompt, const wchar_t *_Nonnull text, int identity,
                   RECT bounds)
{
    HWND window = control(prompt, L"BUTTON", text,
                          WS_TABSTOP | (identity == IDOK ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON));
    if (window == nullptr)
    {
        return false;
    }
    SetWindowLongPtrW(window, GWLP_ID, identity);
    position(prompt, window, bounds);
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

/* 改名は元の名前を選択状態で出す。初回・別名保存は空のまま（決定 1）。 */
static bool fill_name(struct name_prompt *_Nonnull prompt)
{
    if (prompt->kind != NAME_PROMPT_RENAME)
    {
        return true;
    }
    const char *_Nonnull current = folio_state_pane_title(prompt->state).note;
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(current, strlen(current), &wide) != UTF16_TEXT_CONVERTED)
    {
        return false;
    }
    SetWindowTextW(prompt->name, utf16_text_units(wide));
    utf16_text_destroy(wide);
    SendMessageW(prompt->name, EM_SETSEL, 0, -1);
    return true;
}

static const wchar_t *_Nonnull prompt_title(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
        return L"名前をつけて保存";
    case NAME_PROMPT_SAVE_AS:
        return L"別名で保存";
    case NAME_PROMPT_RENAME:
        return L"名前を変更";
    }
    return L"名前をつけて保存";
}

static const wchar_t *_Nonnull prompt_hint(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
        return L"名前の末尾に .md を補います。";
    case NAME_PROMPT_SAVE_AS:
        return L"元の保存内容を保ち、別の .md を作ります。";
    case NAME_PROMPT_RENAME:
        return L"md と履歴を新しい名前へ移します。";
    }
    return L"名前の末尾に .md を補います。";
}

static const wchar_t *_Nonnull prompt_accept(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        return L"保存";
    case NAME_PROMPT_RENAME:
        return L"変更";
    }
    return L"保存";
}

static bool inputs(struct name_prompt *_Nonnull prompt)
{
    prompt->name = control(prompt, L"EDIT", L"", WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL);
    prompt->category =
        control(prompt, L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL);
    prompt->failure = control(prompt, L"STATIC", L"", SS_LEFT);
    if (prompt->name == nullptr || prompt->category == nullptr || prompt->failure == nullptr)
    {
        return false;
    }
    position(prompt, prompt->name, (RECT){20, 46, 380, 74});
    position(prompt, prompt->category, (RECT){20, 108, 380, 280});
    position(prompt, prompt->failure, (RECT){20, 170, 380, 218});
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
    prompt->font = CreateFontW(-scaled(prompt, 14), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Yu Gothic UI");
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
    SetWindowTextW(dialog, prompt_title(prompt->kind));
    return inputs(prompt) && fill_name(prompt) &&
           label(prompt, L"ノートの名前", (RECT){20, 20, 380, 42}) &&
           label(prompt, L"保存先カテゴリ", (RECT){20, 82, 380, 104}) &&
           label(prompt, prompt_hint(prompt->kind), (RECT){20, 144, 380, 166}) &&
           button(prompt, prompt_accept(prompt->kind), IDOK, (RECT){192, 230, 280, 258}) &&
           button(prompt, L"キャンセル", IDCANCEL, (RECT){288, 230, 380, 258});
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

/* 記録を公開した改名は、名前を固定して同じ意図の再開だけを受ける（ADR 0022 の決定 7）。
 * 「キャンセル」は意図の取り消しではないので、そのことを面の中で言う。 */
static void hold_pending(struct name_prompt *_Nonnull prompt)
{
    prompt->pending = true;
    EnableWindow(prompt->name, FALSE);
    SetWindowTextW(prompt->failure,
                   L"名前の変更が途中で止まりました。「変更」でやり直してください。"
                   L"キャンセルや閉じるでは取り消せません。");
}

/* 新しいmdを公開できたときだけ閉じる。LEDGER_STALE は公開後の台帳の失敗。
 * LEDGER_UNSYNCED は何も作れていないので、NAME_TAKEN と同じく入力を残して理由を見せる。 */
static void submit(struct name_prompt *_Nonnull prompt)
{
    enum folio_state_outcome saved = save(prompt);
    if (saved == FOLIO_STATE_READY || saved == FOLIO_STATE_LEDGER_STALE)
    {
        prompt->outcome = saved;
        EndDialog(prompt->dialog, IDOK);
        return;
    }
    if (saved == FOLIO_STATE_RENAME_PENDING)
    {
        prompt->outcome = saved;
        hold_pending(prompt);
        SetFocus(prompt->dialog);
        return;
    }
    const char *_Nonnull reason = folio_state_failure_line(saved);
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(reason, strlen(reason), &wide) == UTF16_TEXT_CONVERTED)
    {
        SetWindowTextW(prompt->failure, utf16_text_units(wide));
        utf16_text_destroy(wide);
    }
    else
    {
        SetWindowTextW(prompt->failure,
                       L"エラー表示の記憶域が不足しています。入力は残っています。");
    }
    SetFocus(prompt->name);
}

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_INITDIALOG)
    {
        struct name_prompt *_Nonnull prompt = (struct name_prompt *)lparam;
        SetWindowLongPtrW(dialog, DWLP_USER, lparam);
        if (!initialize(prompt, dialog))
        {
            prompt->outcome = FOLIO_STATE_OUT_OF_MEMORY;
            EndDialog(dialog, IDCANCEL);
        }
        SetFocus(prompt->name);
        return FALSE;
    }
    struct name_prompt *_Nullable prompt =
        (struct name_prompt *)GetWindowLongPtrW(dialog, DWLP_USER);
    if (prompt == nullptr || prompt->composing)
    {
        return FALSE;
    }
    if (message == WM_COMMAND && LOWORD(wparam) == IDOK)
    {
        submit(prompt);
        return TRUE;
    }
    if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL))
    {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

enum folio_state_outcome name_prompt_show(HWND _Nonnull owner,
                                          const struct name_prompt_request *_Nonnull request)
{
    struct name_prompt prompt = {.state = request->state,
                                 .kind = request->kind,
                                 .units = request->units,
                                 .count = request->count,
                                 .outcome = FOLIO_STATE_CANCELLED};
    INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), &template.dialog, owner,
                                             procedure, (LPARAM)&prompt);
    if (prompt.font != nullptr)
    {
        DeleteObject(prompt.font);
    }
    return result == -1 ? FOLIO_STATE_OUT_OF_MEMORY : prompt.outcome;
}
