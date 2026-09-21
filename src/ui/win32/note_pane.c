#include "note_pane.h"

#include "caret_command.h"
#include "folio_message.h"
#include "line_index.h"
#include "rtf_stream.h"

#include <richedit.h>
#include <stdlib.h>
#include <string.h>
#include <tom.h>

struct note_pane
{
    HMODULE _Nullable library; /* Msftedit.dll。窓より長く生かす */
    HWND _Nullable handle;
    WNDPROC _Nullable original;
    bool composing;
    HACCEL _Nullable navigation;
    ITextSelection *_Nullable selection;
    char16_t *_Nullable taken; /* 最後に取り出した本文（終端付き） */
    size_t capacity;           /* taken のバイト数 */
    size_t used;               /* 取り出したバイト数（終端を含まない） */
    bool overflowed;           /* 取り出しに足りなかった */
    /* 番号専用のバッファ。保存と検索が借りる taken を描画が無効化しない（ADR 0026 の決定 3） */
    char16_t *_Nullable numbered;
    size_t numbered_capacity;
    struct line_index *_Nullable lines; /* 本文の派生物の写し。所有者はここ */
    bool lines_stale;                   /* 次に番号を描くときに作り直す */
    /* EM_STREAMIN を挟むあいだだけ真。途中の本文から表を作らせない（ADR 0026 の補正 5） */
    bool streaming;
};

static const wchar_t library_name[] = L"Msftedit.dll";
static const wchar_t class_name[] = L"RICHEDIT50W";
static const wchar_t editor_face[] = L"Yu Gothic UI";
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

/* EM_STREAMOUT のコールバック。cookie は note_pane への DWORD_PTR。 */
static DWORD CALLBACK stream_out(DWORD_PTR cookie, LPBYTE buffer, LONG offered,
                                 LONG *_Nonnull written)
{
    struct note_pane *_Nonnull pane = (struct note_pane *)cookie;
    size_t count = offered > 0 ? (size_t)offered : 0;
    if (pane->taken == nullptr || pane->used + count + sizeof(char16_t) > pane->capacity)
    {
        pane->overflowed = true;
        return 1;
    }
    memcpy((char *)pane->taken + pane->used, buffer, count);
    pane->used += count;
    *written = (LONG)count;
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

static bool reserve(struct note_pane *_Nonnull pane, size_t bytes)
{
    return reserve_into(&pane->taken, &pane->capacity, bytes);
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

static LRESULT CALLBACK pane_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    struct note_pane *_Nullable pane = (struct note_pane *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (pane == nullptr || pane->original == nullptr)
    {
        return DefWindowProcW(window, message, wparam, lparam);
    }
    if (message == WM_COMMAND && lparam == 0 && navigation_command(pane, wparam))
    {
        return 0;
    }
    if (message == WM_IME_STARTCOMPOSITION)
    {
        pane->composing = true;
    }
    if (message == WM_IME_ENDCOMPOSITION)
    {
        pane->composing = false;
    }
    LRESULT answer = CallWindowProcW(pane->original, window, message, wparam, lparam);
    if (message == WM_MOUSEWHEEL)
    {
        /* ホイールは EN_VSCROLL を出さないので、動かしたあとで親へ知らせる（決定 4）。 */
        SendMessageW(GetParent(window), folio_message_pane_scrolled, 0, 0);
    }
    return answer;
}

static bool subclass_pane(struct note_pane *_Nonnull pane)
{
    SetWindowLongPtrW(pane->handle, GWLP_USERDATA, (LONG_PTR)pane);
    pane->original =
        (WNDPROC)SetWindowLongPtrW(pane->handle, GWLP_WNDPROC, (LONG_PTR)pane_procedure);
    return pane->original != nullptr;
}

static bool prepare_selection(struct note_pane *_Nonnull pane)
{
    IUnknown *_Nullable unknown = nullptr;
    SendMessageW(pane->handle, EM_GETOLEINTERFACE, 0, (LPARAM)&unknown);
    if (unknown == nullptr)
    {
        return false;
    }
    /* QueryInterfaceの出力はvoid**で受け、SDKの型へ直ちに写すWin32境界（C-006）。 */
    void *_Nullable result = nullptr;
    HRESULT queried = unknown->lpVtbl->QueryInterface(unknown, &text_document_id, &result);
    unknown->lpVtbl->Release(unknown);
    if (FAILED(queried) || result == nullptr)
    {
        return false;
    }
    ITextDocument *_Nonnull document = result;
    HRESULT selected = document->lpVtbl->GetSelection(document, &pane->selection);
    document->lpVtbl->Release(document);
    return SUCCEEDED(selected) && pane->selection != nullptr;
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
    return pane->navigation != nullptr && prepare_selection(pane);
}

enum note_pane_outcome note_pane_create(HWND _Nonnull parent, COLORREF background, COLORREF text,
                                        struct note_pane *_Nullable *_Nonnull out)
{
    struct note_pane *_Nullable pane = calloc(1, sizeof *pane);
    if (pane == nullptr)
    {
        return NOTE_PANE_OUT_OF_MEMORY;
    }
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
    SendMessageW(pane->handle, EM_SETBKGNDCOLOR, 0, (LPARAM)background);
    /* 平文の流し込みはこの既定書式で描かれる（RTF の流し込みは既定を壊さない・2026-09-09 実測）。
     */
    CHARFORMAT2W format = {.cbSize = sizeof format,
                           .dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR,
                           .yHeight = editor_height,
                           .crTextColor = text};
    memcpy(format.szFaceName, editor_face, sizeof editor_face);
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

bool note_pane_translate(const struct note_pane *_Nonnull pane, const MSG *_Nonnull message)
{
    if (pane->navigation == nullptr || pane->composing || message->hwnd != pane->handle)
    {
        return false;
    }
    MSG translated = *message;
    return TranslateAcceleratorW(pane->handle, pane->navigation, &translated) != 0;
}

void note_pane_render(struct note_pane *_Nonnull pane, const char *_Nonnull rtf, size_t length)
{
    if (pane->handle == nullptr)
    {
        return;
    }
    /* 差し替えの中で印を付ける（外の呼び出し元は 4 経路あり、呼び忘れを構造で防ぐ・決定 3）。 */
    pane->lines_stale = true;
    SendMessageW(pane->handle, EM_SETREADONLY, TRUE, 0);
    struct rtf_stream stream = {.bytes = rtf, .remaining = length};
    EDITSTREAM editing = {.dwCookie = (DWORD_PTR)&stream, .dwError = 0, .pfnCallback = stream_in};
    /* 流し込みの途中で EN_VSCROLL が届いても、そこから表を作って印を落とさない（補正 5）。
     * 印を落とすのは流し込みが終わったあとの、最初の描き直しである。 */
    pane->streaming = true;
    SendMessageW(pane->handle, EM_STREAMIN, SF_RTF, (LPARAM)&editing);
    pane->streaming = false;
}

void note_pane_edit(struct note_pane *_Nonnull pane, const char16_t *_Nonnull units, size_t count)
{
    if (pane->handle == nullptr)
    {
        return;
    }
    pane->lines_stale = true;
    SendMessageW(pane->handle, EM_EXLIMITTEXT, 0, editor_limit);
    SendMessageW(pane->handle, EM_SETREADONLY, FALSE, 0);
    /* BOM は付けない（付けると本文の U+FEFF になる・2026-09-09 実測）。 */
    struct rtf_stream stream = {.bytes = (const char *)units, .remaining = count * sizeof *units};
    EDITSTREAM editing = {.dwCookie = (DWORD_PTR)&stream, .dwError = 0, .pfnCallback = stream_in};
    pane->streaming = true;
    SendMessageW(pane->handle, EM_STREAMIN, SF_TEXT | SF_UNICODE, (LPARAM)&editing);
    pane->streaming = false;
    SendMessageW(pane->handle, EM_SETMODIFY, FALSE, 0);
}

enum note_pane_text_outcome note_pane_text(struct note_pane *_Nonnull pane,
                                           const char16_t *_Nonnull *_Nonnull units,
                                           size_t *_Nonnull count)
{
    if (pane->handle == nullptr)
    {
        return NOTE_PANE_TEXT_UNAVAILABLE;
    }
    /* 段落区切りは CRLF で返る。単位数の上限をそのまま領域にする（2026-09-09 実測）。 */
    GETTEXTLENGTHEX request = {.flags = GTL_NUMCHARS | GTL_USECRLF, .codepage = unicode_codepage};
    LRESULT length = SendMessageW(pane->handle, EM_GETTEXTLENGTHEX, (WPARAM)&request, 0);
    size_t limit = length > 0 ? (size_t)length : 0;
    if (!reserve(pane, (limit + 1) * sizeof(char16_t)))
    {
        return NOTE_PANE_TEXT_OUT_OF_MEMORY;
    }
    pane->used = 0;
    pane->overflowed = false;
    EDITSTREAM editing = {.dwCookie = (DWORD_PTR)pane, .dwError = 0, .pfnCallback = stream_out};
    SendMessageW(pane->handle, EM_STREAMOUT, SF_TEXT | SF_UNICODE, (LPARAM)&editing);
    if (pane->overflowed || pane->taken == nullptr)
    {
        return NOTE_PANE_TEXT_OUT_OF_MEMORY;
    }
    pane->taken[pane->used / sizeof(char16_t)] = u'\0';
    *units = pane->taken;
    *count = pane->used / sizeof(char16_t);
    return NOTE_PANE_TEXT_TAKEN;
}

/* 表示中の平文を slot へ取り出す。段落区切りを CR 1 つのまま受け取るので、EM_EXSETSEL の位置と
 * そのまま合う。GETTEXTEX の cb の単位（バイトか文字か）は版で揺れるので、どちらでも溢れない
 * 大きさを確保する（2026-09-17 の Win32 部品測定で実際の文字数を確かめている）。 */
static enum note_pane_text_outcome take_display(struct note_pane *_Nonnull pane,
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
                        .flags = GT_DEFAULT,
                        .codepage = unicode_codepage};
    LRESULT taken = SendMessageW(pane->handle, EM_GETTEXTEX, (WPARAM)&taking, (LPARAM)*slot);
    *count = taken > 0 ? (size_t)taken : 0;
    (*slot)[*count] = u'\0';
    return NOTE_PANE_TEXT_TAKEN;
}

enum note_pane_text_outcome note_pane_display_text(struct note_pane *_Nonnull pane,
                                                   const char16_t *_Nonnull *_Nonnull units,
                                                   size_t *_Nonnull count)
{
    if (pane->handle == nullptr)
    {
        return NOTE_PANE_TEXT_UNAVAILABLE;
    }
    size_t taken = 0;
    enum note_pane_text_outcome outcome = take_display(pane, &pane->taken, &pane->capacity, &taken);
    if (outcome != NOTE_PANE_TEXT_TAKEN)
    {
        return outcome;
    }
    pane->used = taken * sizeof(char16_t);
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
    if (pane->streaming)
    {
        /* 流し込みの途中の本文は原文ではない。表を作らず「古い」の印も残す（補正 5）。 */
        return false;
    }
    if (pane->lines != nullptr && !pane->lines_stale)
    {
        return true;
    }
    size_t count = 0;
    if (take_display(pane, &pane->numbered, &pane->numbered_capacity, &count) !=
        NOTE_PANE_TEXT_TAKEN)
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

static void collect_rows(struct note_pane *_Nonnull pane, struct gutter_row *_Nonnull rows,
                         size_t capacity, size_t *_Nonnull count)
{
    RECT bounds = {0, 0, 0, 0};
    GetClientRect(pane->handle, &bounds);
    POINT origin = {.x = 0, .y = 0};
    MapWindowPoints(pane->handle, GetParent(pane->handle), &origin, 1);
    LRESULT total = SendMessageW(pane->handle, EM_GETLINECOUNT, 0, 0);
    LONG opened = -1; /* 高さがまだ決まっていない行の y。無ければ -1 */
    for (LRESULT display = SendMessageW(pane->handle, EM_GETFIRSTVISIBLELINE, 0, 0);
         display < total; ++display)
    {
        size_t position = 0;
        LONG top = 0;
        if (!display_line_top(pane, display, &position, &top))
        {
            break;
        }
        close_row(rows, *count, &opened, top);
        if (top >= bounds.bottom || *count == capacity)
        {
            break;
        }
        size_t number = number_at(pane, position);
        if (number == 0)
        {
            continue;
        }
        rows[*count] = (struct gutter_row){
            .top = origin.y + top, .height = (int)(bounds.bottom - top), .number = number};
        *count += 1;
        opened = top;
    }
}

bool note_pane_visible_rows(struct note_pane *_Nonnull pane, struct gutter_row *_Nonnull rows,
                            size_t capacity, size_t *_Nonnull count)
{
    *count = 0;
    if (pane->handle == nullptr || !refresh_lines(pane))
    {
        return false;
    }
    collect_rows(pane, rows, capacity, count);
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
