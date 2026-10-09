#include "name_prompt.h"
#include "category_name.h"
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
    WNDPROC _Nullable original;
    HFONT _Nullable font;
    /* 開く瞬間の palette を写した塗り。面は追随しない（ADR 0035 の決定 5）。 */
    struct dialog_theme *_Nullable theme;
    UINT dpi;
    /* 一覧が閉じたコンボより高いぶん、下の欄と面を下げる画素（ADR 0035 の補正 18）。 */
    int lowered;
    int hint_extra;
    int failure_extra;
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
    position(prompt, window, bounds, prompt->lowered);
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
            SendMessageW(prompt->category, LB_ADDSTRING, 0, (LPARAM)utf16_text_units(wide));
        utf16_text_destroy(wide);
        if (added == LB_ERR || added == LB_ERRSPACE)
        {
            return false;
        }
    }
    SendMessageW(prompt->category, LB_SETCURSEL, folio_state_document_category(prompt->state), 0);
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
    case NAME_PROMPT_NEW_CATEGORY:
        return UI_TEXT_PROMPT_TITLE_NEW_CATEGORY;
    case NAME_PROMPT_RENAME_CATEGORY:
        return UI_TEXT_PROMPT_TITLE_RENAME_CATEGORY;
    case NAME_PROMPT_RETRY_RENAME:
        return UI_TEXT_PROMPT_TITLE_RENAME;
    }
    return UI_TEXT_PROMPT_TITLE_FIRST_SAVE;
}

static enum ui_text prompt_name_label(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_RENAME:
        return UI_TEXT_PROMPT_LABEL_NAME;
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RENAME_CATEGORY:
        return UI_TEXT_PROMPT_LABEL_CATEGORY_NAME;
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

/* 保存先カテゴリの一覧の見える行の数（ADR 0035 の補正 18）。 */
constexpr int category_rows = 4;
/* 閉じたコンボが占めていた縦の枠（96 DPI の単位。名前欄と同じ 28）。一覧はこれより高いぶん
 * 下の欄と面を下げる。 */
constexpr int category_slot_height = 28;
/* 一覧を持たない面で詰める縦の幅（96 DPI の単位）。カテゴリの見出しの上端 82 から、一覧の下の
 * 案内の上端 144 まで。案内・失敗・釦・面の高さがこの分だけ上がる（ADR 0039 の決定 10）。 */
constexpr int category_block_height = 62;

/* 保存先カテゴリの見出しと一覧を置く種別か。置かない種別は一覧の窓を作らない
 * （ADR 0039 の決定 10）。 */
static bool has_category_list(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_RENAME:
        return true;
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RENAME_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return false;
    }
    return true;
}

/* 一覧の 1 行の高さ（画素）。WM_MEASUREITEM と面の配置の両方がこの 1 本から取る。 */
static int category_row_height(const struct name_prompt *_Nonnull prompt)
{
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
    {
        return scaled(prompt, 22);
    }
    HGDIOBJ old_font = SelectObject(device, prompt->font);
    TEXTMETRICW metrics;
    int height = GetTextMetricsW(device, &metrics) ? (int)metrics.tmHeight + scaled(prompt, 4)
                                                   : scaled(prompt, 22);
    SelectObject(device, old_font);
    ReleaseDC(prompt->dialog, device);
    return height;
}

/* EDITはこのstyleを作成時に保持する。今回STARTから固定面へ進む種別も先に付けておく。 */
static DWORD name_selection_style(enum name_prompt_kind kind)
{
    switch (kind)
    {
    case NAME_PROMPT_RENAME:
    case NAME_PROMPT_RENAME_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return ES_NOHIDESEL;
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_NEW_CATEGORY:
        return 0;
    }
    return 0;
}

static bool inputs(struct name_prompt *_Nonnull prompt)
{
    /* 塗れない OS の縁を外し、面が札の色の 1px の枠を描く（ADR 0035 の決定 3）。
     * 保存先カテゴリは常時開いた owner-draw の一覧で、矢印の釦もドロップダウンも持たない
     * （ADR 0035 の補正 17）。 */
    bool listed = has_category_list(prompt->kind);
    prompt->name = control(prompt, L"EDIT", L"",
                           WS_TABSTOP | ES_AUTOHSCROLL | name_selection_style(prompt->kind));
    if (listed)
    {
        prompt->category = control(prompt, L"LISTBOX", L"",
                                   WS_TABSTOP | LBS_OWNERDRAWFIXED | LBS_HASSTRINGS | LBS_NOTIFY |
                                       LBS_NOINTEGRALHEIGHT | WS_VSCROLL);
    }
    prompt->failure = control(prompt, L"STATIC", L"", SS_LEFT);
    if (prompt->name == nullptr || (listed && prompt->category == nullptr) ||
        prompt->failure == nullptr)
    {
        return false;
    }
    position(prompt, prompt->name, (RECT){20, 46, 380, 74}, 0);
    if (listed)
    {
        MoveWindow(prompt->category, scaled(prompt, 20), scaled(prompt, 108), scaled(prompt, 360),
                   category_rows * category_row_height(prompt), TRUE);
    }
    position(prompt, prompt->failure, (RECT){20, 166, 380, 226}, prompt->lowered);
    SendMessageW(prompt->name, EM_SETLIMITTEXT, 255, 0);
    SetWindowLongPtrW(prompt->name, GWLP_USERDATA, (LONG_PTR)prompt);
    prompt->original =
        (WNDPROC)SetWindowLongPtrW(prompt->name, GWLP_WNDPROC, (LONG_PTR)input_procedure);
    return prompt->original != nullptr && (!listed || fill_categories(prompt));
}

/* 一覧より下の欄と面をずらす画素。一覧を持つ種別は閉じたコンボより高いぶん下げ（補正 18）、
 * 持たない種別はカテゴリの見出しと一覧の枠のぶん上げる（ADR 0039 の決定 10）。 */
static int lowered_by(const struct name_prompt *_Nonnull prompt)
{
    if (!has_category_list(prompt->kind))
    {
        return -scaled(prompt, category_block_height);
    }
    return category_rows * category_row_height(prompt) - scaled(prompt, category_slot_height);
}

static void size_dialog(const struct name_prompt *_Nonnull prompt)
{
    RECT bounds = {0, 0, scaled(prompt, 400),
                   scaled(prompt, 270) + prompt->lowered + prompt->hint_extra +
                       prompt->failure_extra};
    DWORD style = (DWORD)GetWindowLongPtrW(prompt->dialog, GWL_STYLE);
    AdjustWindowRectExForDpi(&bounds, style, FALSE, 0, prompt->dpi);
    RECT owner;
    GetWindowRect(GetParent(prompt->dialog), &owner);
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    MoveWindow(prompt->dialog, (owner.left + owner.right - width) / 2,
               (owner.top + owner.bottom - height) / 2, width, height, FALSE);
}

/* STATICと同じfont/折り返し幅で必要な高さだけ広げる。名前や理由を切らない。 */
static int text_extra(const struct name_prompt *_Nonnull prompt, HWND window, int minimum)
{
    char16_t text[pending_reason_capacity];
    GetWindowTextW(window, text, pending_reason_capacity);
    HDC device = GetDC(prompt->dialog);
    if (device == nullptr)
    {
        return 0;
    }
    HGDIOBJ old_font = SelectObject(device, prompt->font);
    RECT bounds = {0, 0, scaled(prompt, 360), 0};
    DrawTextW(device, text, -1, &bounds, DT_LEFT | DT_WORDBREAK | DT_EXPANDTABS | DT_CALCRECT);
    SelectObject(device, old_font);
    ReleaseDC(prompt->dialog, device);
    int base = scaled(prompt, minimum);
    return bounds.bottom > base ? bounds.bottom - base : 0;
}

static void reflow_prompt(struct name_prompt *_Nonnull prompt)
{
    enum folio_language language = prompt_language(prompt);
    show_line(prompt->dialog, prompt_title(displayed_kind(prompt)), language);
    show_line(prompt->hint, prompt_hint(prompt), language);
    prompt->hint_extra = text_extra(prompt, prompt->hint, 22);
    prompt->failure_extra = text_extra(prompt, prompt->failure, 60);
    MoveWindow(prompt->hint, scaled(prompt, 20), scaled(prompt, 144) + prompt->lowered,
               scaled(prompt, 360), scaled(prompt, 22) + prompt->hint_extra, TRUE);
    MoveWindow(prompt->failure, scaled(prompt, 20),
               scaled(prompt, 166) + prompt->lowered + prompt->hint_extra, scaled(prompt, 360),
               scaled(prompt, 60) + prompt->failure_extra, TRUE);
    int lowered = prompt->lowered + prompt->hint_extra + prompt->failure_extra;
    position(prompt, prompt->accept, (RECT){192, 230, 280, 258}, lowered);
    position(prompt, prompt->cancel, (RECT){288, 230, 380, 258}, lowered);
    size_dialog(prompt);
}

static bool initialize(struct name_prompt *_Nonnull prompt, HWND dialog)
{
    prompt->dialog = dialog;
    prompt->dpi = GetDpiForWindow(dialog);
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
    prompt->lowered = lowered_by(prompt);
    size_dialog(prompt);
    if (!inputs(prompt) || !fill_name(prompt))
    {
        return false;
    }
    prompt->hint = label(prompt, prompt_hint(prompt), (RECT){20, 144, 380, 166}, prompt->lowered);
    return prompt->hint != nullptr &&
           label(prompt, prompt_name_label(displayed_kind(prompt)), (RECT){20, 20, 380, 42}, 0) &&
           (!has_category_list(prompt->kind) ||
            label(prompt, UI_TEXT_PROMPT_LABEL_CATEGORY, (RECT){20, 82, 380, 104}, 0)) &&
           button(prompt, prompt_accept(prompt), IDOK, (RECT){192, 230, 280, 258}) &&
           button(prompt, prompt_close(prompt), IDCANCEL, (RECT){288, 230, 380, 258});
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
    case NAME_PROMPT_FIRST_SAVE:
    case NAME_PROMPT_SAVE_AS:
    case NAME_PROMPT_NEW_CATEGORY:
    case NAME_PROMPT_RETRY_RENAME:
        return false;
    }
    return false;
}

static void submit(struct name_prompt *_Nonnull prompt)
{
    if (prompt->pending)
    {
        submit_retry(prompt);
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
    enum folio_state_outcome saved = save(prompt);
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
    dialog_theme_draw_button(prompt->theme, item,
                             item->CtlID == IDOK ? DIALOG_THEME_BUTTON_PRIMARY
                                                 : DIALOG_THEME_BUTTON_SECONDARY);
}

/* 一覧の行の高さ。WM_MEASUREITEM は inputs() の作る途中で来るので書体から測る。 */
static void measure_category(const struct name_prompt *_Nonnull prompt,
                             MEASUREITEMSTRUCT *_Nonnull item)
{
    item->itemHeight = (UINT)category_row_height(prompt);
}

static void frame_around(const struct name_prompt *_Nonnull prompt, HDC device, HWND window)
{
    RECT bounds;
    GetWindowRect(window, &bounds);
    MapWindowPoints(nullptr, prompt->dialog, (POINT *)&bounds, 2);
    InflateRect(&bounds, 1, 1);
    dialog_theme_frame(prompt->theme, device, &bounds);
}

/* WS_BORDER を外した名前欄と一覧の外側に札の色の 1px の枠を描く（draw_filter_frame と同じ流儀・
 * ADR 0035 の補正 17）。 */
static void paint_frame(const struct name_prompt *_Nonnull prompt)
{
    PAINTSTRUCT paint;
    HDC device = BeginPaint(prompt->dialog, &paint);
    if (device == nullptr)
    {
        return;
    }
    frame_around(prompt, device, prompt->name);
    if (prompt->category != nullptr)
    {
        frame_around(prompt, device, prompt->category);
    }
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

/* 保存先カテゴリの行のダブルクリックは「保存」と同じ経路へ読む（ADR 0035 の補正 19）。 */
static bool reads_as_accept(const struct name_prompt *_Nonnull prompt, WPARAM wparam, LPARAM lparam)
{
    return LOWORD(wparam) == IDOK ||
           (prompt->category != nullptr && (HWND)lparam == prompt->category &&
            HIWORD(wparam) == LBN_DBLCLK);
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
    if (message == WM_COMMAND && reads_as_accept(prompt, wparam, lparam))
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
                                 .target = request->target,
                                 .units = request->units,
                                 .count = request->count,
                                 .outcome = FOLIO_STATE_CANCELLED};
    /* 面を作る前に palette を写す。子の WM_CTLCOLOR* は作る途中から来て、
     * 開いている間に主窓の palette が変わっても面は追随しない（ADR 0035 の決定 3・5）。 */
    switch (dialog_theme_create(palette, &prompt.theme))
    {
    case DIALOG_THEME_READY:
        break;
    case DIALOG_THEME_NO_MEMORY:
    case DIALOG_THEME_NO_BRUSH:
        return FOLIO_STATE_OUT_OF_MEMORY;
    }
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
