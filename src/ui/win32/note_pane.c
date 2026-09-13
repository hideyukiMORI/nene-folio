#include "note_pane.h"

#include "rtf_stream.h"

#include <richedit.h>
#include <stdlib.h>
#include <string.h>

struct note_pane
{
    HMODULE _Nullable library; /* Msftedit.dll。窓より長く生かす */
    HWND _Nullable handle;
    char16_t *_Nullable taken; /* 最後に取り出した本文（終端付き） */
    size_t capacity;           /* taken のバイト数 */
    size_t used;               /* 取り出したバイト数（終端を含まない） */
    bool overflowed;           /* 取り出しに足りなかった */
};

static const wchar_t library_name[] = L"Msftedit.dll";
static const wchar_t class_name[] = L"RICHEDIT50W";
static const wchar_t editor_face[] = L"Yu Gothic UI";

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
    if (pane->handle == nullptr)
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
