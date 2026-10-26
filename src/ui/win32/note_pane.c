#include "note_pane.h"

#include "caret_command.h"
#include "folio_message.h"
#include "line_index.h"
#include "pane_mode.h"
#include "pane_zoom_command.h"
#include "rtf_stream.h"

#include <richedit.h>
#include <stdlib.h>
#include <string.h>
#include <tom.h>
#include <windowsx.h>

struct note_pane
{
    HMODULE _Nullable library; /* Msftedit.dll。窓より長く生かす */
    HWND _Nullable handle;
    WNDPROC _Nullable original;
    COLORREF background; /* 実効paletteから受けた描画色。無効時も同じ地を使う（ADR0066）。 */
    bool composing;
    HACCEL _Nullable navigation;
    HACCEL _Nullable zoom_keys;
    int zoom_percent; /* 表示倍率の唯一の所有値。RTF投入がnative倍率を戻すので再適用する。 */
    int zoom_wheel_delta;
    bool wheel_dispatching; /* nativeの親伝播が同じ本文へ戻っても二度渡さない。 */
    ITextSelection *_Nullable selection;
    char16_t *_Nullable taken; /* 最後に取り出した本文（終端付き） */
    size_t capacity;           /* taken のバイト数 */
    /* 番号専用のバッファ。保存と検索が借りる taken を描画が無効化しない（ADR 0026 の決定 3） */
    char16_t *_Nullable numbered;
    size_t numbered_capacity;
    struct line_index *_Nullable lines; /* 本文の派生物の写し。所有者はここ */
    bool lines_stale;                   /* 次に番号を描くときに作り直す */
    enum note_pane_readiness readiness;
    bool recolor_undo_pending; /* 自分が停止したUndo。再開成功まで新たな停止をしない。 */
    bool recolor_input_locked; /* Undo停止中の入力だけを止め、本文のREADYは保持する。 */
};

static const wchar_t library_name[] = L"Msftedit.dll";
static const wchar_t class_name[] = L"RICHEDIT50W";
/* TOMのGUIDはSDKのtom.hとMicrosoftのUse TOM GUIDsに従う（ADR 0019）。 */
static const IID text_document_id = {
    0x8CC497C0, 0xA1DF, 0x11CE, {0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D}};

static const struct
{
    WORD key;
    enum caret_command command;
} navigation_bindings[] = {{'H', CARET_COMMAND_LEFT},
                           {'J', CARET_COMMAND_DOWN},
                           {'K', CARET_COMMAND_UP},
                           {'L', CARET_COMMAND_RIGHT}};

static const struct
{
    WORD key;
    BYTE modifiers;
    enum pane_zoom_command command;
} zoom_bindings[] = {{VK_OEM_PLUS, FCONTROL, PANE_ZOOM_COMMAND_IN},
                     {VK_OEM_PLUS, FCONTROL | FSHIFT, PANE_ZOOM_COMMAND_IN},
                     {VK_ADD, FCONTROL, PANE_ZOOM_COMMAND_IN},
                     {VK_OEM_MINUS, FCONTROL, PANE_ZOOM_COMMAND_OUT},
                     {VK_SUBTRACT, FCONTROL, PANE_ZOOM_COMMAND_OUT},
                     {'0', FCONTROL, PANE_ZOOM_COMMAND_RESET},
                     {VK_NUMPAD0, FCONTROL, PANE_ZOOM_COMMAND_RESET}};
static_assert((unsigned short)CARET_COMMAND_RIGHT < (unsigned short)PANE_ZOOM_COMMAND_IN,
              "pane accelerator ids do not overlap");

constexpr int zoom_normal = 100;
constexpr int zoom_minimum = 50;
constexpr int zoom_maximum = 300;
constexpr int zoom_step = 10;

constexpr LONG editor_height = 220; /* 11pt（twips）。RTF 側の \fs22 と同じ */
constexpr UINT unicode_codepage = 1200;
/* 編集モードで打てる文字数の上限（既定は 32767）。EM_STREAMIN は流し込んだ長さまでしか広げないが、
 * 先に広げた上限は縮めない（2026-09-09 実測）。 */
constexpr LPARAM editor_limit = 0x7FFFFFFF;

/* EM_STREAMIN のコールバック。cookie は rtf_stream への DWORD_PTR。 */
static DWORD CALLBACK stream_in(DWORD_PTR cookie, LPBYTE buffer, LONG wanted, LONG *_Nonnull read)
{
    struct rtf_stream *_Nonnull stream = (struct rtf_stream *)cookie;
    size_t count = stream->remaining;
    if (wanted > 0 && count > (size_t)wanted)
    {
        count = (size_t)wanted;
    }
    memcpy(buffer, stream->bytes, count);
    stream->bytes += count;
    stream->remaining -= count;
    *read = (LONG)count;
    return 0;
}

/* 取り出し用の領域を bytes まで広げる。保存用と番号用が同じ形で別の領域を持つ。 */
static bool reserve_into(char16_t *_Nullable *_Nonnull slot, size_t *_Nonnull capacity,
                         size_t bytes)
{
    if (*slot != nullptr && bytes <= *capacity)
    {
        return true;
    }
    char16_t *_Nullable grown = realloc(*slot, bytes);
    if (grown == nullptr)
    {
        return false;
    }
    *slot = grown;
    *capacity = bytes;
    return true;
}

static HRESULT move_caret(ITextSelection *_Nonnull selection, enum caret_command command)
{
    switch (command)
    {
    case CARET_COMMAND_LEFT:
        return selection->lpVtbl->MoveLeft(selection, tomCharacter, 1, tomMove, nullptr);
    case CARET_COMMAND_DOWN:
        return selection->lpVtbl->MoveDown(selection, tomLine, 1, tomMove, nullptr);
    case CARET_COMMAND_UP:
        return selection->lpVtbl->MoveUp(selection, tomLine, 1, tomMove, nullptr);
    case CARET_COMMAND_RIGHT:
        return selection->lpVtbl->MoveRight(selection, tomCharacter, 1, tomMove, nullptr);
    }
    return E_INVALIDARG;
}

static bool navigation_command(const struct note_pane *_Nonnull pane, WPARAM command)
{
    if (HIWORD(command) != 1 || pane->selection == nullptr)
    {
        return false;
    }
    for (size_t index = 0; index < sizeof navigation_bindings / sizeof navigation_bindings[0];
         ++index)
    {
        if (LOWORD(command) != navigation_bindings[index].command)
        {
            continue;
        }
        if (FAILED(move_caret(pane->selection, navigation_bindings[index].command)))
        {
            MessageBeep(MB_ICONWARNING);
        }
        return true;
    }
    return false;
}

/* nativeの投影はここだけで変更し、成功した値だけを所有する。投入中にも同じ経路を使う。 */
static bool apply_zoom(struct note_pane *_Nonnull pane, int percent)
{
    if (SendMessageW(pane->handle, EM_SETZOOM, (WPARAM)percent, zoom_normal) == 0)
        return false;
    pane->zoom_percent = percent;
    return true;
}

static bool zoom_available(const struct note_pane *_Nonnull pane)
{
    return pane->handle != nullptr && !pane->composing &&
           pane->readiness == NOTE_PANE_READINESS_READY && IsWindowEnabled(pane->handle) &&
           IsWindowVisible(pane->handle);
}

static void zoom_to(struct note_pane *_Nonnull pane, int percent)
{
    percent = percent < zoom_minimum ? zoom_minimum : percent;
    percent = percent > zoom_maximum ? zoom_maximum : percent;
    if (!zoom_available(pane) || percent == pane->zoom_percent)
        return;
    size_t first = note_pane_first_visible_line(pane);
    if (!apply_zoom(pane, percent))
    {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    SendMessageW(GetParent(pane->handle), folio_message_pane_zoomed, (WPARAM)first, 0);
}

static void zoom_action(struct note_pane *_Nonnull pane, enum pane_zoom_command command)
{
    int percent = pane->zoom_percent;
    switch (command)
    {
    case PANE_ZOOM_COMMAND_IN:
        percent += zoom_step;
        break;
    case PANE_ZOOM_COMMAND_OUT:
        percent -= zoom_step;
        break;
    case PANE_ZOOM_COMMAND_RESET:
        percent = zoom_normal;
        break;
    }
    pane->zoom_wheel_delta = 0;
    zoom_to(pane, percent);
}

static bool zoom_command(struct note_pane *_Nonnull pane, WPARAM command)
{
    if (HIWORD(command) != 1)
        return false;
    for (size_t index = 0; index < sizeof zoom_bindings / sizeof zoom_bindings[0]; ++index)
    {
        if (LOWORD(command) == zoom_bindings[index].command)
        {
            zoom_action(pane, zoom_bindings[index].command);
            return true;
        }
    }
    return false;
}

/* ポインタが本文外なら主窓の振分けへ戻し、nativeの独立したCtrl+wheel zoomは呼ばない。 */
static bool zoom_wheel(struct note_pane *_Nonnull pane, UINT message, WPARAM keys, LPARAM position)
{
    if (message != WM_MOUSEWHEEL)
        return false;
    if ((GET_KEYSTATE_WPARAM(keys) & MK_CONTROL) == 0)
    {
        pane->zoom_wheel_delta = 0;
        return false;
    }
    if (!zoom_available(pane))
    {
        pane->zoom_wheel_delta = 0;
        return true;
    }
    RECT bounds;
    GetWindowRect(pane->handle, &bounds);
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (!PtInRect(&bounds, point))
    {
        pane->zoom_wheel_delta = 0;
        SendMessageW(GetParent(pane->handle), message, keys, position);
        return true;
    }
    pane->zoom_wheel_delta += GET_WHEEL_DELTA_WPARAM(keys);
    int steps = pane->zoom_wheel_delta / WHEEL_DELTA;
    pane->zoom_wheel_delta %= WHEEL_DELTA;
    zoom_to(pane, pane->zoom_percent + steps * zoom_step);
    return true;
}

static bool pane_input(struct note_pane *_Nonnull pane, UINT message, WPARAM keys, LPARAM parameter)
{
    if (message == WM_COMMAND && parameter == 0)
        return zoom_command(pane, keys) || navigation_command(pane, keys);
    return zoom_wheel(pane, message, keys, parameter);
}

static LRESULT enable_pane(struct note_pane *_Nonnull pane, WPARAM enabled, LPARAM parameter)
{
    pane->zoom_wheel_delta = 0;
    LRESULT result = CallWindowProcW(pane->original, pane->handle, WM_ENABLE, enabled, parameter);
    LONG_PTR style = GetWindowLongPtrW(pane->handle, GWL_EXSTYLE);
    style = IsWindowEnabled(pane->handle) ? style & ~WS_EX_TRANSPARENT : style | WS_EX_TRANSPARENT;
    SetWindowLongPtrW(pane->handle, GWL_EXSTYLE, style);
    InvalidateRect(pane->handle, nullptr, TRUE);
    return result;
}

static bool background_requested(const struct note_pane *_Nonnull pane, UINT message, LPARAM flags)
{
    if (IsWindowEnabled(pane->handle))
        return false;
    if (message == WM_ERASEBKGND)
        return true;
    if (message != WM_PRINTCLIENT || (flags & (PRF_CLIENT | PRF_ERASEBKGND)) == 0)
        return false;
    return (flags & PRF_CHECKVISIBLE) == 0 || IsWindowVisible(pane->handle);
}

/* 文字とWM_PAINTはnativeが所有する。disabled時の地だけを同じpaletteで用意する。 */
static bool paint_background(const struct note_pane *_Nonnull pane, UINT message, WPARAM wparam,
                             LPARAM flags)
{
    if (message == WM_PAINT && !IsWindowEnabled(pane->handle))
    {
        /* nativeは選択内の文字を部分paintで再使用する。地を消すときは同じ本文全体を更新する。 */
        if (GetUpdateRect(pane->handle, nullptr, FALSE))
            InvalidateRect(pane->handle, nullptr, TRUE);
    }
    if (!background_requested(pane, message, flags))
        return false;
    HDC device = (HDC)wparam;
    RECT bounds;
    GetClientRect(pane->handle, &bounds);
    COLORREF previous = SetDCBrushColor(device, pane->background);
    FillRect(device, &bounds, (HBRUSH)GetStockObject(DC_BRUSH));
    SetDCBrushColor(device, previous);
    return message == WM_ERASEBKGND;
}

static LRESULT native_message(struct note_pane *_Nonnull pane, UINT message, WPARAM wparam,
                              LPARAM lparam)
{
    if (message != WM_MOUSEWHEEL)
        return CallWindowProcW(pane->original, pane->handle, message, wparam, lparam);
    if (pane->wheel_dispatching)
        return 0;
    pane->wheel_dispatching = true;
    LRESULT answer = CallWindowProcW(pane->original, pane->handle, message, wparam, lparam);
    pane->wheel_dispatching = false;
    /* ホイールは EN_VSCROLL を出さないので、動かしたあとで親へ知らせる（決定 4）。 */
    SendMessageW(GetParent(pane->handle), folio_message_pane_scrolled, 0, 0);
    return answer;
}

static LRESULT CALLBACK pane_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct note_pane *_Nullable pane = (struct note_pane *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (pane == nullptr || pane->original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    if (message == WM_ENABLE)
        return enable_pane(pane, wparam, lparam);
    if (paint_background(pane, message, wparam, lparam))
        return 1;
    if (pane_input(pane, message, wparam, lparam))
    {
        return 0;
    }
    if (message == WM_IME_STARTCOMPOSITION)
    {
        pane->composing = true;
        pane->zoom_wheel_delta = 0;
    }
    if (message == WM_IME_ENDCOMPOSITION)
    {
        pane->composing = false;
    }
    return native_message(pane, message, wparam, lparam);
}

static bool subclass_pane(struct note_pane *_Nonnull pane)
{
    SetWindowLongPtrW(pane->handle, GWLP_USERDATA, (LONG_PTR)pane);
    pane->original =
        (WNDPROC)SetWindowLongPtrW(pane->handle, GWLP_WNDPROC, (LONG_PTR)pane_procedure);
    return pane->original != nullptr;
}

/* RichEdit から ITextDocument を 1 つ取る唯一の経路（ARC-001）。呼んだ側が Release する。
 * 取れなければ nullptr（選択の準備も再着色もその場合の振る舞いを自分で決める）。 */
static ITextDocument *_Nullable text_document(struct note_pane *_Nonnull pane)
{
    IUnknown *_Nullable unknown = nullptr;
    SendMessageW(pane->handle, EM_GETOLEINTERFACE, 0, (LPARAM)&unknown);
    if (unknown == nullptr)
    {
        return nullptr;
    }
    /* QueryInterfaceの出力はvoid**で受け、SDKの型へ直ちに写すWin32境界（C-006）。 */
    void *_Nullable result = nullptr;
    HRESULT queried = unknown->lpVtbl->QueryInterface(unknown, &text_document_id, &result);
    unknown->lpVtbl->Release(unknown);
    if (FAILED(queried))
    {
        return nullptr;
    }
    return result;
}

static bool resume_recolor(struct note_pane *_Nonnull pane, ITextDocument *_Nonnull document,
                           enum pane_mode mode)
{
    if (pane->recolor_undo_pending)
    {
        if (FAILED(document->lpVtbl->Undo(document, tomResume, nullptr)))
        {
            return false;
        }
        pane->recolor_undo_pending = false;
    }
    if (pane->recolor_input_locked)
    {
        bool writable = mode == PANE_MODE_EDIT && pane->readiness == NOTE_PANE_READINESS_READY;
        if (SendMessageW(pane->handle, EM_SETREADONLY, (WPARAM)!writable, 0) == 0)
        {
            return false;
        }
        EnableWindow(pane->handle, TRUE);
        pane->recolor_input_locked = false;
    }
    return true;
}

/* 次の本文投入も、未解決のUndo停止を捨てず先に回復する。失敗なら元の本文を維持する。 */
static bool ready_to_load(struct note_pane *_Nonnull pane, enum pane_mode mode)
{
    if (!pane->recolor_undo_pending && !pane->recolor_input_locked)
    {
        return true;
    }
    ITextDocument *_Nullable document = text_document(pane);
    if (document == nullptr)
    {
        return false;
    }
    bool ready = resume_recolor(pane, document, mode);
    document->lpVtbl->Release(document);
    return ready;
}

static bool prepare_selection(struct note_pane *_Nonnull pane)
{
    ITextDocument *_Nullable document = text_document(pane);
    if (document == nullptr)
    {
        return false;
    }
    HRESULT selected = document->lpVtbl->GetSelection(document, &pane->selection);
    document->lpVtbl->Release(document);
    return SUCCEEDED(selected) && pane->selection != nullptr;
}

static bool prepare_zoom(struct note_pane *_Nonnull pane)
{
    constexpr size_t count = sizeof zoom_bindings / sizeof zoom_bindings[0];
    ACCEL accelerators[count];
    for (size_t index = 0; index < count; ++index)
    {
        accelerators[index] = (ACCEL){.fVirt = (BYTE)(FVIRTKEY | zoom_bindings[index].modifiers),
                                      .key = zoom_bindings[index].key,
                                      .cmd = zoom_bindings[index].command};
    }
    pane->zoom_keys = CreateAcceleratorTableW(accelerators, (int)count);
    return pane->zoom_keys != nullptr;
}

static bool prepare_navigation(struct note_pane *_Nonnull pane)
{
    constexpr size_t count = sizeof navigation_bindings / sizeof navigation_bindings[0];
    ACCEL accelerators[count];
    for (size_t index = 0; index < count; ++index)
    {
        accelerators[index] = (ACCEL){.fVirt = FVIRTKEY | FCONTROL,
                                      .key = navigation_bindings[index].key,
                                      .cmd = navigation_bindings[index].command};
    }
    pane->navigation = CreateAcceleratorTableW(accelerators, (int)count);
    return pane->navigation != nullptr && prepare_selection(pane) && prepare_zoom(pane);
}

static void set_background(struct note_pane *_Nonnull pane, COLORREF background)
{
    pane->background = background;
    SendMessageW(pane->handle, EM_SETBKGNDCOLOR, 0, (LPARAM)background);
    InvalidateRect(pane->handle, nullptr, TRUE);
}

enum note_pane_outcome note_pane_create(HWND _Nonnull parent, COLORREF background, COLORREF text,
                                        struct note_pane *_Nullable *_Nonnull out)
{
    struct note_pane *_Nullable pane = calloc(1, sizeof *pane);
    if (pane == nullptr)
    {
        return NOTE_PANE_OUT_OF_MEMORY;
    }
    pane->zoom_percent = zoom_normal;
    pane->library = LoadLibraryW(library_name);
    if (pane->library == nullptr)
    {
        note_pane_destroy(pane);
        return NOTE_PANE_NOT_CREATED;
    }
    /* スクロールバーは出さない（FR-012 と同じ流儀）。ホイールで動く。
     * ES_NOHIDESEL は、検索欄に鍵があるあいだも一致の選択を見せるため（ADR 0023 の決定 4）。 */
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_MULTILINE | ES_READONLY |
                  ES_AUTOVSCROLL | ES_NOHIDESEL;
    pane->handle =
        CreateWindowExW(0, class_name, L"", style, 0, 0, 0, 0, parent,
                        (HMENU)(INT_PTR)note_pane_control_id, GetModuleHandleW(nullptr), nullptr);
    if (pane->handle == nullptr || !subclass_pane(pane) || !prepare_navigation(pane))
    {
        note_pane_destroy(pane);
        return NOTE_PANE_NOT_CREATED;
    }
    set_background(pane, background);
    /* 既定書式。閲覧も編集も RTF が face・大きさ・色を持ち込む（ADR 0006 の #245 補正）ので、
     * これは RTF が書式を持たない箇所の退避で、RTF の流し込みは既定を壊さない（2026-09-09 実測）。
     * **face はここでは決めない**（言語ごとの値の正本は core の ui_font で、
     * 主窓が作った直後と言語の切り替えで note_pane_reface が当てる・ADR 0032 の決定 4）。 */
    CHARFORMAT2W format = {.cbSize = sizeof format,
                           .dwMask = CFM_SIZE | CFM_COLOR,
                           .yHeight = editor_height,
                           .crTextColor = text};
    SendMessageW(pane->handle, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&format);
    /* 鍵の通知を親の WM_NOTIFY へ上げる（Ctrl+S と Esc）。ENM_KEYEVENTS は消さない。
     * スクロールと本文の増減は番号の帯の契機なので足す（ADR 0026 の決定 4）。 */
    SendMessageW(pane->handle, EM_SETEVENTMASK, 0, ENM_KEYEVENTS | ENM_SCROLL | ENM_CHANGE);
    *out = pane;
    return NOTE_PANE_CREATED;
}

HWND _Nullable note_pane_handle(const struct note_pane *_Nonnull pane)
{
    return pane->handle;
}

bool note_pane_composing(const struct note_pane *_Nonnull pane)
{
    return pane->composing;
}

bool note_pane_translate(const struct note_pane *_Nonnull pane, const MSG *_Nonnull message,
                         enum pane_mode mode)
{
    if (message->hwnd != pane->handle || !zoom_available(pane))
    {
        return false;
    }
    MSG translated = *message;
    if (TranslateAcceleratorW(pane->handle, pane->zoom_keys, &translated) != 0)
        return true;
    return mode == PANE_MODE_EDIT &&
           TranslateAcceleratorW(pane->handle, pane->navigation, &translated) != 0;
}

int note_pane_zoom_percent(const struct note_pane *_Nonnull pane)
{
    return pane->zoom_percent;
}

/* 閲覧と編集が共有する唯一の流し込み（ADR 0006 の #245 補正）。UTF-8 RTF で表示を置き換える。
 * 差し替えの中で印を付ける（外の呼び出し元は複数経路あり、呼び忘れを構造で防ぐ・決定 3）。 */
static bool stream_rtf(struct note_pane *_Nonnull pane, const char *_Nonnull rtf, size_t length)
{
    pane->lines_stale = true;
    struct rtf_stream stream = {.bytes = rtf, .remaining = length};
    EDITSTREAM editing = {.dwCookie = (DWORD_PTR)&stream, .dwError = 0, .pfnCallback = stream_in};
    /* 流し込みの途中で EN_VSCROLL が届いても、そこから表を作って印を落とさない（補正 5）。
     * 印を落とすのは流し込みが終わったあとの、最初の描き直しである。 */
    LRESULT count =
        SendMessageW(pane->handle, EM_STREAMIN, ((WPARAM)CP_UTF8 << 16) | SF_USECODEPAGE | SF_RTF,
                     (LPARAM)&editing);
    return count >= 0 && editing.dwError == 0 && stream.remaining == 0;
}

/* readonly化自体も失敗したら入力窓を止める。親の既存札から再試行できる。 */
static enum note_pane_load_outcome failed_load(struct note_pane *_Nonnull pane, LRESULT modified)
{
    if (SendMessageW(pane->handle, EM_SETREADONLY, TRUE, 0) == 0)
    {
        EnableWindow(pane->handle, FALSE);
    }
    SendMessageW(pane->handle, EM_SETMODIFY, (WPARAM)(modified != 0), 0);
    pane->readiness = NOTE_PANE_READINESS_UNAVAILABLE;
    return NOTE_PANE_LOAD_FAILED;
}

enum note_pane_load_outcome note_pane_load(struct note_pane *_Nonnull pane,
                                           const char *_Nonnull rtf, size_t length,
                                           enum pane_mode mode)
{
    if (pane->readiness == NOTE_PANE_READINESS_STREAMING)
    {
        return NOTE_PANE_LOAD_BUSY;
    }
    if (pane->handle == nullptr)
    {
        return NOTE_PANE_LOAD_UNAVAILABLE;
    }
    if (!ready_to_load(pane, mode))
    {
        /* applicationは既に次の対象を採用し得る。旧本文を次の保存対象へ渡してはいけない。 */
        return failed_load(pane, SendMessageW(pane->handle, EM_GETMODIFY, 0, 0));
    }
    pane->readiness = NOTE_PANE_READINESS_STREAMING;
    LRESULT modified = SendMessageW(pane->handle, EM_GETMODIFY, 0, 0);
    SendMessageW(pane->handle, EM_EXLIMITTEXT, 0, editor_limit);
    if (SendMessageW(pane->handle, EM_SETREADONLY, TRUE, 0) == 0 ||
        !stream_rtf(pane, rtf, length) || !apply_zoom(pane, pane->zoom_percent))
    {
        return failed_load(pane, modified);
    }
    switch (mode)
    {
    case PANE_MODE_EDIT:
        if (SendMessageW(pane->handle, EM_SETREADONLY, FALSE, 0) == 0)
        {
            return failed_load(pane, modified);
        }
        SendMessageW(pane->handle, EM_SETMODIFY, FALSE, 0);
        break;
    case PANE_MODE_VIEW:
        break;
    }
    EnableWindow(pane->handle, TRUE);
    pane->readiness = NOTE_PANE_READINESS_READY;
    return NOTE_PANE_LOAD_READY;
}

enum note_pane_readiness note_pane_readiness(const struct note_pane *_Nonnull pane)
{
    return pane->readiness;
}

/* 表示中の平文を slot へ取り出す。段落区切りを CR 1 つのまま受け取るので、EM_EXSETSEL の位置と
 * そのまま合う。GETTEXTEX の cb の単位（バイトか文字か）は版で揺れるので、どちらでも溢れない
 * 大きさを確保する（2026-09-17 の Win32 部品測定で実際の文字数を確かめている）。 */
static enum note_pane_text_outcome take_text(struct note_pane *_Nonnull pane,
                                             char16_t *_Nullable *_Nonnull slot,
                                             size_t *_Nonnull capacity, size_t *_Nonnull count)
{
    GETTEXTLENGTHEX request = {.flags = GTL_NUMCHARS, .codepage = unicode_codepage};
    LRESULT length = SendMessageW(pane->handle, EM_GETTEXTLENGTHEX, (WPARAM)&request, 0);
    size_t limit = length > 0 ? (size_t)length : 0;
    if (!reserve_into(slot, capacity, (limit + 1) * 2 * sizeof(char16_t)))
    {
        return NOTE_PANE_TEXT_OUT_OF_MEMORY;
    }
    GETTEXTEX taking = {.cb = (DWORD)((limit + 1) * sizeof(char16_t)),
                        .flags = GT_RAWTEXT,
                        .codepage = unicode_codepage};
    LRESULT taken = SendMessageW(pane->handle, EM_GETTEXTEX, (WPARAM)&taking, (LPARAM)*slot);
    *count = taken > 0 ? (size_t)taken : 0;
    (*slot)[*count] = u'\0';
    return NOTE_PANE_TEXT_TAKEN;
}

enum note_pane_text_outcome note_pane_text(struct note_pane *_Nonnull pane,
                                           const char16_t *_Nonnull *_Nonnull units,
                                           size_t *_Nonnull count)
{
    if (pane->handle == nullptr)
    {
        return NOTE_PANE_TEXT_UNAVAILABLE;
    }
    if (pane->readiness != NOTE_PANE_READINESS_READY)
    {
        return NOTE_PANE_TEXT_NOT_READY;
    }
    size_t taken = 0;
    enum note_pane_text_outcome outcome = take_text(pane, &pane->taken, &pane->capacity, &taken);
    if (outcome != NOTE_PANE_TEXT_TAKEN)
    {
        return outcome;
    }
    *units = pane->taken;
    *count = taken;
    return NOTE_PANE_TEXT_TAKEN;
}

void note_pane_invalidate_lines(struct note_pane *_Nonnull pane)
{
    pane->lines_stale = true;
}

/* 番号の表を、必要なときだけ作り直す（描画の合流が打鍵の連続をまとめる・決定 3）。 */
static bool refresh_lines(struct note_pane *_Nonnull pane)
{
    if (pane->readiness != NOTE_PANE_READINESS_READY)
    {
        /* 流し込みの途中の本文は原文ではない。表を作らず「古い」の印も残す（補正 5）。 */
        return false;
    }
    if (pane->lines != nullptr && !pane->lines_stale)
    {
        return true;
    }
    size_t count = 0;
    if (take_text(pane, &pane->numbered, &pane->numbered_capacity, &count) != NOTE_PANE_TEXT_TAKEN)
    {
        return false;
    }
    struct line_index *_Nullable built = nullptr;
    if (line_index_create(pane->numbered, count, &built) != LINE_INDEX_READY)
    {
        return false;
    }
    line_index_destroy(pane->lines);
    pane->lines = built;
    pane->lines_stale = false;
    return true;
}

/* 表示行の先頭の文字位置と、その y（RichEdit の client 座標）。行高は一定でないので 1 行ずつ問う。
 */
static bool display_line_top(const struct note_pane *_Nonnull pane, LRESULT display,
                             size_t *_Nonnull position, LONG *_Nonnull top)
{
    LRESULT at = SendMessageW(pane->handle, EM_LINEINDEX, (WPARAM)display, 0);
    if (at < 0)
    {
        return false;
    }
    POINTL point = {.x = 0, .y = 0};
    SendMessageW(pane->handle, EM_POSFROMCHAR, (WPARAM)&point, (LPARAM)at);
    *position = (size_t)at;
    *top = point.y;
    return true;
}

/* その位置が論理行の先頭なら番号を、継続行なら 0 を答える（継続行には番号を出さない）。 */
static size_t number_at(const struct note_pane *_Nonnull pane, size_t position)
{
    struct line_mark mark = {.number = 0, .first = false};
    if (pane->lines == nullptr || line_index_at(pane->lines, position, &mark) != LINE_INDEX_READY ||
        !mark.first)
    {
        return 0;
    }
    return mark.number;
}

/* 直前に置いた行の高さを、次の表示行の y で確定する。opened が負なら待っている行は無い。 */
static void close_row(struct gutter_row *_Nonnull rows, size_t count, LONG *_Nonnull opened,
                      LONG top)
{
    if (*opened < 0)
    {
        return;
    }
    rows[count - 1].height = (int)(top - *opened);
    *opened = -1;
}

/* 選択を動かさず、本文の実baselineを測る範囲。所有者は呼び出し側。 */
static ITextRange *_Nullable measuring_range(struct note_pane *_Nonnull pane)
{
    ITextDocument *_Nullable document = text_document(pane);
    if (document == nullptr)
    {
        return nullptr;
    }
    ITextRange *_Nullable range = nullptr;
    HRESULT made = document->lpVtbl->Range(document, 0, 0, &range);
    document->lpVtbl->Release(document);
    return SUCCEEDED(made) ? range : nullptr;
}

static bool row_baseline(ITextRange *_Nonnull range, size_t position, LONG *_Nonnull baseline)
{
    if (FAILED(range->lpVtbl->SetRange(range, (LONG)position, (LONG)position)))
    {
        return false;
    }
    LONG left = 0;
    return range->lpVtbl->GetPoint(range,
                                   tomStart | TA_BASELINE | tomClientCoord | tomAllowOffClient,
                                   &left, baseline) == S_OK;
}

static size_t collect_rows(struct note_pane *_Nonnull pane, ITextRange *_Nonnull range,
                           struct gutter_row *_Nonnull rows, size_t capacity)
{
    RECT bounds = {0, 0, 0, 0};
    GetClientRect(pane->handle, &bounds);
    POINT origin = {.x = 0, .y = 0};
    MapWindowPoints(pane->handle, GetParent(pane->handle), &origin, 1);
    LRESULT total = SendMessageW(pane->handle, EM_GETLINECOUNT, 0, 0);
    LONG opened = -1;
    size_t count = 0;
    for (LRESULT display = SendMessageW(pane->handle, EM_GETFIRSTVISIBLELINE, 0, 0);
         display < total; ++display)
    {
        size_t position = 0;
        LONG top = 0;
        if (!display_line_top(pane, display, &position, &top))
        {
            break;
        }
        close_row(rows, count, &opened, top);
        if (top >= bounds.bottom || count == capacity)
        {
            break;
        }
        size_t number = number_at(pane, position);
        if (number == 0)
        {
            continue;
        }
        LONG baseline = 0;
        if (!row_baseline(range, position, &baseline))
        {
            break;
        }
        rows[count] = (struct gutter_row){.top = origin.y + top,
                                          .height = (int)(bounds.bottom - top),
                                          .baseline = origin.y + baseline,
                                          .number = number};
        count += 1;
        opened = top;
    }
    return count;
}

bool note_pane_visible_rows(struct note_pane *_Nonnull pane, struct gutter_row *_Nonnull rows,
                            size_t capacity, size_t *_Nonnull count)
{
    *count = 0;
    if (pane->handle == nullptr || !refresh_lines(pane))
    {
        return false;
    }
    ITextRange *_Nullable range = measuring_range(pane);
    if (range == nullptr)
    {
        return false;
    }
    *count = collect_rows(pane, range, rows, capacity);
    range->lpVtbl->Release(range);
    return true;
}

size_t note_pane_line_digits(struct note_pane *_Nonnull pane)
{
    /* 作り直せなかった（確保失敗・流し込み中）ときも、古い表があればその桁数を答える。
     * 番号が描けないのに帯の幅だけ縮んで本文が動く、を作らない（ADR 0026 の補正 8）。 */
    bool rebuilt = pane->handle != nullptr && refresh_lines(pane);
    if (!rebuilt && pane->lines == nullptr)
    {
        return line_index_minimum_digits;
    }
    return line_index_digits(pane->lines);
}

size_t note_pane_first_visible_line(struct note_pane *_Nonnull pane)
{
    if (pane->handle == nullptr || !refresh_lines(pane))
    {
        return 0;
    }
    LRESULT display = SendMessageW(pane->handle, EM_GETFIRSTVISIBLELINE, 0, 0);
    size_t position = 0;
    LONG top = 0;
    if (!display_line_top(pane, display, &position, &top))
    {
        return 0;
    }
    struct line_mark mark = {.number = 0, .first = false};
    if (line_index_at(pane->lines, position, &mark) != LINE_INDEX_READY)
    {
        return 0;
    }
    return mark.number;
}

void note_pane_scroll_to_line(struct note_pane *_Nonnull pane, size_t number)
{
    size_t position = 0;
    if (pane->handle == nullptr || !refresh_lines(pane) ||
        line_index_start(pane->lines, number, &position) != LINE_INDEX_READY)
    {
        return;
    }
    LRESULT wanted = SendMessageW(pane->handle, EM_EXLINEFROMCHAR, 0, (LPARAM)position);
    LRESULT now = SendMessageW(pane->handle, EM_GETFIRSTVISIBLELINE, 0, 0);
    SendMessageW(pane->handle, EM_LINESCROLL, 0, (LPARAM)(wanted - now));
}

void note_pane_select(struct note_pane *_Nonnull pane, size_t start, size_t end)
{
    if (pane->handle == nullptr)
    {
        return;
    }
    CHARRANGE range = {.cpMin = (LONG)start, .cpMax = (LONG)end};
    SendMessageW(pane->handle, EM_EXSETSEL, 0, (LPARAM)&range);
    SendMessageW(pane->handle, EM_SCROLLCARET, 0, 0);
}

void note_pane_restore_selection(struct note_pane *_Nonnull pane, struct note_search_span span)
{
    if (pane->handle == nullptr)
    {
        return;
    }
    /* EM_SCROLLCARET を送らない。流し直しで保たれた位置を動かさない（ADR 0031 の決定 6）。 */
    CHARRANGE range = {.cpMin = (LONG)span.start, .cpMax = (LONG)span.end};
    SendMessageW(pane->handle, EM_EXSETSEL, 0, (LPARAM)&range);
}

/* 文字色だけを scope（SCF_ALL か SCF_DEFAULT）へ当てる。書体と大きさは触らない。 */
static bool apply_text_color(struct note_pane *_Nonnull pane, COLORREF text, WPARAM scope)
{
    CHARFORMAT2W format = {.cbSize = sizeof format, .dwMask = CFM_COLOR, .crTextColor = text};
    return SendMessageW(pane->handle, EM_SETCHARFORMAT, scope, (LPARAM)&format) != 0;
}

/* 閲覧は RTF が文字色を持っていて直後に流し直すので、SCF_ALL は捨てる書式を
 * 1 度塗るだけでちらつきの元になる。既定書式は RTF が書式を持たない箇所の退避なので
 * 閲覧でも更新が要る（ADR 0031 の補正節・D6。編集の流し込みも RTF になった・#245）。 */
static bool recolor_body(enum pane_mode mode)
{
    switch (mode)
    {
    case PANE_MODE_VIEW:
        return false;
    case PANE_MODE_EDIT:
        return true;
    }
    return true;
}

/* 既定書式を先に塗り、本文に失敗したら既定色も戻す。背景は両方の成功まで保持する。 */
static bool recolor_text(struct note_pane *_Nonnull pane, COLORREF text, enum pane_mode mode)
{
    CHARFORMAT2W previous = {.cbSize = sizeof previous};
    SendMessageW(pane->handle, EM_GETCHARFORMAT, SCF_DEFAULT, (LPARAM)&previous);
    if ((previous.dwMask & CFM_COLOR) == 0 || !apply_text_color(pane, text, SCF_DEFAULT))
    {
        return false;
    }
    if (recolor_body(mode) && !apply_text_color(pane, text, SCF_ALL))
    {
        /* 本文は編集の単色。復元が失敗しても成功扱いせず次の通知で再試行する。 */
        apply_text_color(pane, previous.crTextColor, SCF_ALL);
        apply_text_color(pane, previous.crTextColor, SCF_DEFAULT);
        return false;
    }
    return true;
}

static bool finish_recolor(struct note_pane *_Nonnull pane, ITextDocument *_Nonnull document)
{
    if (SUCCEEDED(document->lpVtbl->Undo(document, tomResume, nullptr)))
    {
        return true;
    }
    pane->recolor_undo_pending = true;
    pane->recolor_input_locked = true;
    if (SendMessageW(pane->handle, EM_SETREADONLY, TRUE, 0) == 0)
    {
        EnableWindow(pane->handle, FALSE);
    }
    return false;
}

enum note_pane_recolor_outcome note_pane_recolor(struct note_pane *_Nonnull pane,
                                                 COLORREF background, COLORREF text,
                                                 enum pane_mode mode)
{
    if (pane->handle == nullptr || pane->readiness != NOTE_PANE_READINESS_READY)
    {
        return NOTE_PANE_RECOLOR_FAILED;
    }
    ITextDocument *_Nullable document = text_document(pane);
    if (document == nullptr)
    {
        return NOTE_PANE_RECOLOR_FAILED;
    }
    if (!resume_recolor(pane, document, mode))
    {
        document->lpVtbl->Release(document);
        return NOTE_PANE_RECOLOR_FAILED;
    }
    LRESULT modified = SendMessageW(pane->handle, EM_GETMODIFY, 0, 0);
    /* 特殊Countの停止/再開は実MsfteditがS_FALSEも返す。負のHRESULTだけを失敗とする。 */
    if (FAILED(document->lpVtbl->Undo(document, tomSuspend, nullptr)))
    {
        document->lpVtbl->Release(document);
        return NOTE_PANE_RECOLOR_FAILED;
    }
    bool colored = recolor_text(pane, text, mode);
    bool resumed = finish_recolor(pane, document);
    document->lpVtbl->Release(document);
    SendMessageW(pane->handle, EM_SETMODIFY, (WPARAM)(modified != 0), 0);
    if (colored)
    {
        set_background(pane, background);
    }
    return colored && resumed ? NOTE_PANE_RECOLOR_READY : NOTE_PANE_RECOLOR_FAILED;
}

/* 既定書式の face だけを当て直す（ADR 0032 の決定 6）。色（note_pane_recolor）と同じ枠で、
 * 本文・選択・Undo の段数・変更印は変わらない（実測 (c2)）。SCF_ALL は当てない
 * （既定書式を継いでいる本文は既定書式の変更に追随する）。
 * 再折り返しが起きるので、呼び出し側が論理行を退避・復元する。 */
void note_pane_reface(struct note_pane *_Nonnull pane, const wchar_t *_Nonnull face)
{
    if (pane->handle == nullptr || pane->recolor_undo_pending || pane->recolor_input_locked)
    {
        return;
    }
    CHARFORMAT2W format = {.cbSize = sizeof format, .dwMask = CFM_FACE};
    size_t at = 0;
    while (face[at] != L'\0')
    {
        if (at + 1 >= LF_FACESIZE)
        {
            return; /* 収まらない face は当てない（szFaceName は終端を含む） */
        }
        format.szFaceName[at] = face[at];
        at += 1;
    }
    format.szFaceName[at] = L'\0';
    /* 本文の表は face で幅が変わるので、次に番号を描くときに作り直す（ADR 0026 の決定 3）。 */
    pane->lines_stale = true;
    ITextDocument *_Nullable document = text_document(pane);
    if (document == nullptr)
    {
        return; /* Undo を汚さない方を取る（note_pane_recolor と同じ判断） */
    }
    LRESULT modified = SendMessageW(pane->handle, EM_GETMODIFY, 0, 0);
    document->lpVtbl->Undo(document, tomSuspend, nullptr);
    SendMessageW(pane->handle, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&format);
    document->lpVtbl->Undo(document, tomResume, nullptr);
    document->lpVtbl->Release(document);
    SendMessageW(pane->handle, EM_SETMODIFY, (WPARAM)(modified != 0), 0);
}

void note_pane_replace(struct note_pane *_Nonnull pane, struct note_search_span span,
                       const char16_t *_Nonnull units)
{
    if (pane->handle == nullptr || pane->readiness != NOTE_PANE_READINESS_READY ||
        pane->recolor_undo_pending || pane->recolor_input_locked)
    {
        return;
    }
    CHARRANGE range = {.cpMin = (LONG)span.start, .cpMax = (LONG)span.end};
    SendMessageW(pane->handle, EM_EXSETSEL, 0, (LPARAM)&range);
    /* fCanUndo = TRUE で 1 回だけ。流し込みではないので streaming の印は付けない（決定 7）。 */
    SendMessageW(pane->handle, EM_REPLACESEL, TRUE, (LPARAM)units);
}

bool note_pane_selection(const struct note_pane *_Nonnull pane, size_t *_Nonnull start,
                         size_t *_Nonnull end)
{
    if (pane->handle == nullptr)
    {
        return false;
    }
    CHARRANGE range = {.cpMin = 0, .cpMax = 0};
    SendMessageW(pane->handle, EM_EXGETSEL, 0, (LPARAM)&range);
    *start = range.cpMin > 0 ? (size_t)range.cpMin : 0;
    *end = range.cpMax > 0 ? (size_t)range.cpMax : 0;
    return true;
}

void note_pane_destroy(struct note_pane *_Nullable pane)
{
    if (pane == nullptr)
    {
        return;
    }
    if (pane->selection != nullptr)
    {
        pane->selection->lpVtbl->Release(pane->selection);
    }
    if (pane->navigation != nullptr)
    {
        DestroyAcceleratorTable(pane->navigation);
    }
    if (pane->zoom_keys != nullptr)
        DestroyAcceleratorTable(pane->zoom_keys);
    if (pane->handle != nullptr && IsWindow(pane->handle))
    {
        DestroyWindow(pane->handle);
    }
    if (pane->library != nullptr)
    {
        FreeLibrary(pane->library);
    }
    line_index_destroy(pane->lines);
    free(pane->numbered);
    free(pane->taken);
    free(pane);
}
