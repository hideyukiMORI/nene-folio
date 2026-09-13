#include "note_pane.h"

#include "caret_command.h"
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

/* 取り出し用の領域を bytes まで広げる。 */
static bool reserve(struct note_pane *_Nonnull pane, size_t bytes)
{
    if (pane->taken != nullptr && bytes <= pane->capacity)
    {
        return true;
    }
    char16_t *_Nullable grown = realloc(pane->taken, bytes);
    if (grown == nullptr)
    {
        return false;
    }
    pane->taken = grown;
    pane->capacity = bytes;
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
    return CallWindowProcW(pane->original, window, message, wparam, lparam);
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
    /* スクロールバーは出さない（FR-012 と同じ流儀）。ホイールで動く。 */
    DWORD style =
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL;
    pane->handle = CreateWindowExW(0, class_name, L"", style, 0, 0, 0, 0, parent, nullptr,
                                   GetModuleHandleW(nullptr), nullptr);
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
    /* 鍵の通知を親の WM_NOTIFY へ上げる（Ctrl+S と Esc）。 */
    SendMessageW(pane->handle, EM_SETEVENTMASK, 0, ENM_KEYEVENTS);
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
    SendMessageW(pane->handle, EM_SETREADONLY, TRUE, 0);
    struct rtf_stream stream = {.bytes = rtf, .remaining = length};
    EDITSTREAM editing = {.dwCookie = (DWORD_PTR)&stream, .dwError = 0, .pfnCallback = stream_in};
    SendMessageW(pane->handle, EM_STREAMIN, SF_RTF, (LPARAM)&editing);
}

void note_pane_edit(struct note_pane *_Nonnull pane, const char16_t *_Nonnull units, size_t count)
{
    if (pane->handle == nullptr)
    {
        return;
    }
    SendMessageW(pane->handle, EM_EXLIMITTEXT, 0, editor_limit);
    SendMessageW(pane->handle, EM_SETREADONLY, FALSE, 0);
    /* BOM は付けない（付けると本文の U+FEFF になる・2026-09-09 実測）。 */
    struct rtf_stream stream = {.bytes = (const char *)units, .remaining = count * sizeof *units};
    EDITSTREAM editing = {.dwCookie = (DWORD_PTR)&stream, .dwError = 0, .pfnCallback = stream_in};
    SendMessageW(pane->handle, EM_STREAMIN, SF_TEXT | SF_UNICODE, (LPARAM)&editing);
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
    free(pane->taken);
    free(pane);
}
