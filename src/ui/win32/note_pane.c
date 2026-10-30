#include "note_pane.h"

#include "caret_command.h"
#include "folio_message.h"
#include "line_index.h"
#include "note_pane_table.h"
#include "pane_mode.h"
#include "pane_zoom_command.h"
#include "rtf_stream.h"

#include <limits.h>
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
    LONG table_width; /* Successful native width in twips; reset on every document load. */
    bool table_layout_active;
    HWND _Nullable scroll_timer;
    double scroll_remaining; /* nativeの実位置とは別の、1画面以内の未消化量。 */
    double scroll_fraction;
    UINT scroll_left;
    DWORD scroll_tick;
    bool scroll_tick_known;
    bool scroll_projecting;
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

static bool layout_tables(struct note_pane *_Nonnull pane, enum pane_mode mode);
static bool finish_recolor(struct note_pane *_Nonnull pane, ITextDocument *_Nonnull document);
static bool display_line_top(const struct note_pane *_Nonnull pane, LRESULT display,
                             size_t *_Nonnull position, LONG *_Nonnull top);
static void cancel_scroll(struct note_pane *_Nonnull pane);

static const wchar_t library_name[] = L"Msftedit.dll";
static const wchar_t class_name[] = L"RICHEDIT50W";
static const wchar_t scroll_class[] = L"NeNeFolioPaneScroll";
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
constexpr UINT scroll_duration = 240;
constexpr UINT scroll_interval = 16;
constexpr UINT_PTR scroll_timer_id = 1;

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

static bool navigation_command(struct note_pane *_Nonnull pane, WPARAM command)
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
        cancel_scroll(pane);
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

static void finish_scroll(struct note_pane *_Nonnull pane)
{
    if (pane->scroll_timer != nullptr)
        KillTimer(pane->scroll_timer, scroll_timer_id);
    pane->scroll_left = 0;
    pane->scroll_remaining = 0;
    pane->scroll_tick_known = false;
}

static void cancel_scroll(struct note_pane *_Nonnull pane)
{
    finish_scroll(pane);
    pane->scroll_fraction = 0;
}

/* EM_SETSCROLLPOSは本文/選択を変更せず、nativeの端で要求を丸める。 */
static bool project_scroll(struct note_pane *_Nonnull pane, LONG pixels)
{
    POINT before = {0};
    SendMessageW(pane->handle, EM_GETSCROLLPOS, 0, (LPARAM)&before);
    LONGLONG wanted = (LONGLONG)before.y + pixels;
    wanted = wanted < 0 ? 0 : wanted;
    wanted = wanted > LONG_MAX ? LONG_MAX : wanted;
    POINT target = {.x = before.x, .y = (LONG)wanted};
    pane->scroll_projecting = true;
    SendMessageW(pane->handle, EM_SETSCROLLPOS, 0, (LPARAM)&target);
    pane->scroll_projecting = false;
    POINT after = {0};
    SendMessageW(pane->handle, EM_GETSCROLLPOS, 0, (LPARAM)&after);
    SendMessageW(GetParent(pane->handle), folio_message_pane_scrolled, 0, 0);
    return after.y != before.y && after.y == target.y && wanted == (LONGLONG)before.y + pixels;
}

/* 残り時間に対する二次曲線。tickの遅れをframe数に置き換えて引き延ばさない。 */
static void advance_scroll(struct note_pane *_Nonnull pane, DWORD elapsed)
{
    elapsed = elapsed > pane->scroll_left ? pane->scroll_left : elapsed;
    double ratio = (double)(pane->scroll_left - elapsed) / pane->scroll_left;
    double remaining = pane->scroll_remaining * ratio * ratio;
    double movement = pane->scroll_remaining - remaining + pane->scroll_fraction;
    LONG pixels = (LONG)movement;
    pane->scroll_fraction = movement - pixels;
    pane->scroll_remaining = remaining;
    pane->scroll_left -= elapsed;
    if (pixels != 0 && !project_scroll(pane, pixels))
        cancel_scroll(pane);
    if (pane->scroll_left == 0)
        finish_scroll(pane);
}

static void CALLBACK scroll_frame(HWND window, UINT message, UINT_PTR timer, DWORD tick)
{
    (void)message;
    struct note_pane *_Nullable pane = (struct note_pane *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (pane == nullptr || timer != scroll_timer_id || pane->scroll_left == 0)
        return;
    if (!zoom_available(pane) || IsIconic(GetAncestor(pane->handle, GA_ROOT)))
    {
        cancel_scroll(pane);
        return;
    }
    DWORD elapsed = pane->scroll_tick_known ? tick - pane->scroll_tick : scroll_interval;
    pane->scroll_tick = tick;
    pane->scroll_tick_known = true;
    advance_scroll(pane, elapsed);
}

/* 専用のmessage-only窓にtimerを置き、RichEditの非公開timer IDと衝突させない。 */
static bool prepare_scroll(struct note_pane *_Nonnull pane)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW kind = {0};
    if (!GetClassInfoW(instance, scroll_class, &kind))
    {
        kind = (WNDCLASSW){
            .lpfnWndProc = DefWindowProcW, .hInstance = instance, .lpszClassName = scroll_class};
        if (RegisterClassW(&kind) == 0)
            return false;
    }
    pane->scroll_timer = CreateWindowExW(0, scroll_class, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                         instance, nullptr);
    if (pane->scroll_timer == nullptr)
        return false;
    SetWindowLongPtrW(pane->scroll_timer, GWLP_USERDATA, (LONG_PTR)pane);
    return true;
}

static LONG scroll_page(const struct note_pane *_Nonnull pane)
{
    RECT bounds = {0};
    GetClientRect(pane->handle, &bounds);
    return bounds.bottom > bounds.top ? bounds.bottom - bounds.top : 0;
}

static LRESULT scroll_target(LRESULT first, LRESULT count, UINT lines, int delta)
{
    LRESULT span = (LRESULT)lines < count ? (LRESULT)lines : count;
    LRESULT target = delta < 0 ? first + span : first - span;
    target = target < 0 ? 0 : target;
    target = target >= count ? count - 1 : target;
    if (target == first)
        target = first > 0 ? first - 1 : first + 1;
    return target;
}

static LONG scroll_lines(const struct note_pane *_Nonnull pane, UINT lines, int delta, LONG page)
{
    LRESULT first = SendMessageW(pane->handle, EM_GETFIRSTVISIBLELINE, 0, 0);
    LRESULT count = SendMessageW(pane->handle, EM_GETLINECOUNT, 0, 0);
    if (first < 0 || count <= 1)
        return 0;
    LRESULT target = scroll_target(first, count, lines, delta);
    size_t position = 0;
    LONG here = 0;
    LONG there = 0;
    if (!display_line_top(pane, first, &position, &here) ||
        !display_line_top(pane, target, &position, &there))
        return 0;
    LONGLONG distance = (LONGLONG)there - here;
    distance = distance < 0 ? -distance : distance;
    return distance > page ? page : (LONG)distance;
}

static LONG scroll_unit(const struct note_pane *_Nonnull pane, int delta, LONG page)
{
    UINT lines = 0;
    if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0))
    {
        MessageBeep(MB_ICONWARNING);
        return 0;
    }
    if (lines == 0)
        return 0;
    if (lines == WHEEL_PAGESCROLL)
        return page;
    return scroll_lines(pane, lines, delta, page);
}

static void scroll_wheel(struct note_pane *_Nonnull pane, int delta)
{
    LONG page = scroll_page(pane);
    if (delta == 0 || page == 0)
        return;
    double movement = -(double)delta * scroll_unit(pane, delta, page) / WHEEL_DELTA;
    if (movement == 0)
        return;
    if (movement * (pane->scroll_remaining + pane->scroll_fraction) < 0)
        cancel_scroll(pane);
    bool starting = pane->scroll_left == 0;
    double remaining = pane->scroll_remaining + movement;
    remaining = remaining < -page ? -page : remaining;
    pane->scroll_remaining = remaining > page ? page : remaining;
    pane->scroll_left = scroll_duration;
    if (starting &&
        SetTimer(pane->scroll_timer, scroll_timer_id, scroll_interval, scroll_frame) == 0)
    {
        cancel_scroll(pane);
        MessageBeep(MB_ICONWARNING);
    }
}

/* 別操作が所有する位置変更や入力へ、以前のホイールの余韻を持ち越さない。 */
static void interrupt_scroll(struct note_pane *_Nonnull pane, UINT message)
{
    switch (message)
    {
    case EM_SETSCROLLPOS:
        if (!pane->scroll_projecting)
            cancel_scroll(pane);
        return;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_CHAR:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_IME_STARTCOMPOSITION:
    case WM_SIZE:
    case WM_SHOWWINDOW:
    case WM_ENABLE:
    case WM_KILLFOCUS:
    case WM_DESTROY:
    case WM_SETTEXT:
    case WM_SETFONT:
    case WM_VSCROLL:
    case WM_HSCROLL:
    case EM_STREAMIN:
    case EM_SETTEXTEX:
    case EM_REPLACESEL:
    case EM_UNDO:
    case EM_REDO:
    case EM_SETZOOM:
    case EM_SCROLLCARET:
    case EM_LINESCROLL:
    case EM_SETCHARFORMAT:
    case EM_SETPARAFORMAT:
    case EM_SETRECT:
    case EM_SETRECTNP:
    case EM_SETREADONLY:
        cancel_scroll(pane);
        return;
    default:
        return; /* Win32 messages are an open set. */
    }
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
    cancel_scroll(pane);
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

/* 本文外は主窓の同じ振分けへ戻す。通常wheelもCtrl倍率もnativeへ二重に渡さない。 */
static bool pane_wheel(struct note_pane *_Nonnull pane, UINT message, WPARAM keys, LPARAM position)
{
    if (message != WM_MOUSEWHEEL)
        return false;
    if (!zoom_available(pane))
    {
        cancel_scroll(pane);
        pane->zoom_wheel_delta = 0;
        return true;
    }
    RECT bounds;
    GetWindowRect(pane->handle, &bounds);
    POINT point = {GET_X_LPARAM(position), GET_Y_LPARAM(position)};
    if (!PtInRect(&bounds, point))
    {
        cancel_scroll(pane);
        pane->zoom_wheel_delta = 0;
        SendMessageW(GetParent(pane->handle), message, keys, position);
        return true;
    }
    if ((GET_KEYSTATE_WPARAM(keys) & MK_CONTROL) == 0)
    {
        pane->zoom_wheel_delta = 0;
        scroll_wheel(pane, GET_WHEEL_DELTA_WPARAM(keys));
        return true;
    }
    cancel_scroll(pane);
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
    return pane_wheel(pane, message, keys, parameter);
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

static LRESULT CALLBACK pane_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct note_pane *_Nullable pane = (struct note_pane *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (pane == nullptr || pane->original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    interrupt_scroll(pane, message);
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
    return CallWindowProcW(pane->original, pane->handle, message, wparam, lparam);
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

static bool extend_background(const struct note_pane *_Nonnull pane)
{
    LRESULT styles =
        SendMessageW(pane->handle, EM_SETEDITSTYLE, SES_EXTENDBACKCOLOR, SES_EXTENDBACKCOLOR);
    return (styles & SES_EXTENDBACKCOLOR) != 0;
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
    /* nativeのスクロール能力を空/短文でも保ち、バーの外観だけを隠す。
     * ES_NOHIDESEL は、検索欄に鍵があるあいだも一致の選択を見せるため（ADR 0023 の決定 4）。 */
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_MULTILINE | ES_READONLY |
                  ES_AUTOVSCROLL | ES_NOHIDESEL | WS_VSCROLL | ES_DISABLENOSCROLL;
    pane->handle =
        CreateWindowExW(0, class_name, L"", style, 0, 0, 0, 0, parent,
                        (HMENU)(INT_PTR)note_pane_control_id, GetModuleHandleW(nullptr), nullptr);
    if (pane->handle == nullptr || !subclass_pane(pane) || !prepare_navigation(pane) ||
        !extend_background(pane) || !prepare_scroll(pane))
    {
        note_pane_destroy(pane);
        return NOTE_PANE_NOT_CREATED;
    }
    SendMessageW(pane->handle, EM_SHOWSCROLLBAR, SB_VERT, FALSE);
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
    pane->table_width = 0;
    LRESULT modified = SendMessageW(pane->handle, EM_GETMODIFY, 0, 0);
    SendMessageW(pane->handle, EM_EXLIMITTEXT, 0, editor_limit);
    if (SendMessageW(pane->handle, EM_SETREADONLY, TRUE, 0) == 0 ||
        !stream_rtf(pane, rtf, length) || !apply_zoom(pane, pane->zoom_percent) ||
        !layout_tables(pane, mode))
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

static LONG table_width(struct note_pane *_Nonnull pane)
{
    RECT client;
    GetClientRect(pane->handle, &client);
    RECT format = {0};
    SendMessageW(pane->handle, EM_GETRECT, 0, (LPARAM)&format);
    LONG pixels = client.right - client.left - format.left - (client.right - format.right);
    if (pixels <= 0)
        return 0;
    LONG width =
        MulDiv(MulDiv(pixels, 1440, (int)GetDpiForWindow(pane->handle)), 100, pane->zoom_percent);
    return width > 31679 ? 31679 : width;
}

static bool format_tables(struct note_pane *_Nonnull pane, LONG width, size_t count)
{
    ITextDocument *_Nullable document = text_document(pane);
    if (document == nullptr)
        return false;
    if (!resume_recolor(pane, document, PANE_MODE_VIEW) ||
        FAILED(document->lpVtbl->Undo(document, tomSuspend, nullptr)))
    {
        document->lpVtbl->Release(document);
        return false;
    }
    LRESULT modified = SendMessageW(pane->handle, EM_GETMODIFY, 0, 0);
    bool placed =
        note_pane_table_layout(pane->handle, pane->numbered, count, width) == NOTE_PANE_LOAD_READY;
    bool resumed = finish_recolor(pane, document);
    document->lpVtbl->Release(document);
    SendMessageW(pane->handle, EM_SETMODIFY, (WPARAM)(modified != 0), 0);
    return placed && resumed;
}

static bool layout_tables(struct note_pane *_Nonnull pane, enum pane_mode mode)
{
    if (mode == PANE_MODE_EDIT || pane->table_layout_active)
        return true;
    LONG width = table_width(pane);
    if (width <= 0 || width == pane->table_width)
        return true;
    size_t count = 0;
    pane->table_layout_active = true;
    bool taken =
        take_text(pane, &pane->numbered, &pane->numbered_capacity, &count) == NOTE_PANE_TEXT_TAKEN;
    bool placed = taken;
    if (taken)
    {
        enum note_pane_table_scan scanned =
            note_pane_table_scan(pane->handle, pane->numbered, count);
        placed = scanned != NOTE_PANE_TABLE_FAILED;
        if (scanned == NOTE_PANE_TABLE_PRESENT)
            placed = format_tables(pane, width, count);
    }
    pane->table_layout_active = false;
    if (placed)
        pane->table_width = width;
    return placed;
}

enum note_pane_load_outcome note_pane_reflow(struct note_pane *_Nonnull pane, enum pane_mode mode)
{
    if (pane->handle == nullptr)
        return NOTE_PANE_LOAD_UNAVAILABLE;
    if (pane->readiness == NOTE_PANE_READINESS_STREAMING || pane->table_layout_active)
        return NOTE_PANE_LOAD_BUSY;
    if (pane->readiness != NOTE_PANE_READINESS_READY)
        return NOTE_PANE_LOAD_FAILED;
    if (layout_tables(pane, mode))
        return NOTE_PANE_LOAD_READY;
    return failed_load(pane, SendMessageW(pane->handle, EM_GETMODIFY, 0, 0));
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
    cancel_scroll(pane);
    if (pane->scroll_timer != nullptr)
    {
        SetWindowLongPtrW(pane->scroll_timer, GWLP_USERDATA, 0);
        DestroyWindow(pane->scroll_timer);
        pane->scroll_timer = nullptr;
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
