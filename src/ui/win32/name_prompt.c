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
    HWND _Nullable accept;
    HWND _Nullable cancel;
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

static bool set_name_text(struct name_prompt *_Nonnull prompt, const char *_Nonnull text)
{
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(text, strlen(text), &wide) != UTF16_TEXT_CONVERTED)
    {
        return false;
    }
    SetWindowTextW(prompt->name, utf16_text_units(wide));
    utf16_text_destroy(wide);
    return true;
}

/* 記録を公開した後に面の中で言うこと。閉じても意図は消えない（決定 7）。 */
static const wchar_t pending_explanation[] =
    L"「再試行」で同じ名前変更を続けます。「閉じる」は取り消しではありません。";

/* 未完了の意図があるあいだは、名前もカテゴリも変えられない固定状態で開く（決定 7）。
 * 出すのは旧名と新名で、押せるのは「再試行」と「閉じる」だけ。閉じても意図は残る。 */
static bool fill_pending(struct name_prompt *_Nonnull prompt, struct rename_view pending)
{
    static const char arrow[] = " → ";
    constexpr size_t arrow_length = sizeof arrow - 1;
    char line[600];
    size_t from_length = strlen(pending.from);
    size_t to_length = strlen(pending.to);
    if (from_length + arrow_length + to_length + 1 > sizeof line)
    {
        return false;
    }
    memcpy(line, pending.from, from_length);
    memcpy(line + from_length, arrow, arrow_length);
    memcpy(line + from_length + arrow_length, pending.to, to_length + 1);
    if (!set_name_text(prompt, line))
    {
        return false;
    }
    prompt->pending = true;
    EnableWindow(prompt->name, FALSE);
    SetWindowTextW(prompt->failure, pending_explanation);
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

static const wchar_t *_Nonnull prompt_hint(const struct name_prompt *_Nonnull prompt)
{
    if (prompt->pending)
    {
        return L"この名前変更を最後まで終えるまで、ほかの操作へ進めません。";
    }
    switch (prompt->kind)
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

static const wchar_t *_Nonnull prompt_accept(const struct name_prompt *_Nonnull prompt)
{
    if (prompt->pending)
    {
        return L"再試行";
    }
    switch (prompt->kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
        return L"保存";
    case NAME_PROMPT_RENAME:
        return L"変更";
    }
    return L"保存";
}

static const wchar_t *_Nonnull prompt_close(const struct name_prompt *_Nonnull prompt)
{
    return prompt->pending ? L"閉じる" : L"キャンセル";
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
    const char *_Nonnull reason = folio_state_failure_line(outcome, FOLIO_LANGUAGE_JA);
    struct utf16_text *_Nullable wide = nullptr;
    if (utf16_text_create(reason, strlen(reason), &wide) != UTF16_TEXT_CONVERTED)
    {
        SetWindowTextW(prompt->failure, pending_explanation);
        return;
    }
    constexpr size_t capacity = 512;
    constexpr size_t explanation = sizeof pending_explanation / sizeof pending_explanation[0] - 1;
    wchar_t line[capacity];
    size_t reason_length = utf16_text_length(wide);
    if (reason_length + explanation + 2 > capacity)
    {
        utf16_text_destroy(wide);
        SetWindowTextW(prompt->failure, pending_explanation);
        return;
    }
    memcpy(line, utf16_text_units(wide), reason_length * sizeof *line);
    utf16_text_destroy(wide);
    line[reason_length] = L' ';
    memcpy(line + reason_length + 1, pending_explanation, (explanation + 1) * sizeof *line);
    SetWindowTextW(prompt->failure, line);
}

/* 記録を公開した改名は、名前を固定して同じ意図の再開だけを受ける（ADR 0022 の決定 7）。
 * 「閉じる」は意図の取り消しではないので、そのことを面の中で言う。 */
static void hold_pending(struct name_prompt *_Nonnull prompt, enum folio_state_outcome outcome)
{
    struct rename_view pending = {.from = "", .to = ""};
    if (!prompt->pending && folio_state_rename_pending(prompt->state, &pending))
    {
        (void)fill_pending(prompt, pending);
        SetWindowTextW(prompt->accept, prompt_accept(prompt));
        SetWindowTextW(prompt->cancel, prompt_close(prompt));
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
    const char *_Nonnull reason =
        folio_state_failure_line(saved, folio_state_language(prompt->state));
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

static INT_PTR CALLBACK procedure(HWND dialog, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_INITDIALOG)
    {
        begin_dialog((struct name_prompt *)lparam, dialog);
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
