#include "note_pane_links.h"
#include "note_pane_link_gesture.h"
#include "utf8_text.h"
#include "web_uri.h"
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windowsx.h>

struct note_pane_links
{
    struct hidden_run
    {
        size_t start;
        size_t end;
    } *_Nullable hidden;
    size_t count;
    size_t capacity;
    enum note_pane_link_gesture gesture;
    POINT down;
    CHARRANGE target;
};

static const IID range2_id = {
    0xC241F5E2, 0x7206, 0x11D8, {0xA2, 0xC7, 0x00, 0xA0, 0xD1, 0xD6, 0xC6, 0xB3}};

static bool append_hidden(struct note_pane_links *_Nonnull links, LONG start, LONG end)
{
    if (links->count == links->capacity)
    {
        size_t capacity = links->capacity == 0 ? 16 : links->capacity * 2;
        if (capacity < links->capacity || capacity > SIZE_MAX / sizeof *links->hidden)
            return false;
        struct hidden_run *_Nullable prepared = realloc(links->hidden, capacity * sizeof *prepared);
        if (prepared == nullptr)
            return false;
        links->hidden = prepared;
        links->capacity = capacity;
    }
    links->hidden[links->count++] = (struct hidden_run){(size_t)start, (size_t)end};
    return true;
}

static enum note_pane_link_outcome collect_format(struct note_pane_links *_Nonnull links,
                                                  ITextRange *_Nonnull range, LONG start, LONG end)
{
    ITextFont *_Nullable font = nullptr;
    if (FAILED(range->lpVtbl->GetFont(range, &font)) || font == nullptr)
        return NOTE_PANE_LINK_UNAVAILABLE;
    LONG hidden = tomUndefined;
    HRESULT read = font->lpVtbl->GetHidden(font, &hidden);
    font->lpVtbl->Release(font);
    if (FAILED(read) || hidden == tomUndefined)
        return NOTE_PANE_LINK_UNAVAILABLE;
    if (hidden == tomTrue && !append_hidden(links, start, end))
        return NOTE_PANE_LINK_OUT_OF_MEMORY;
    return NOTE_PANE_LINK_READY;
}

static enum note_pane_link_outcome collect_runs(struct note_pane_links *_Nonnull links,
                                                ITextRange *_Nonnull range)
{
    LONG length = 0;
    if (FAILED(range->lpVtbl->GetStoryLength(range, &length)))
        return NOTE_PANE_LINK_UNAVAILABLE;
    LONG start = 0;
    while (start < length)
    {
        LONG moved = 0;
        LONG end = 0;
        if (FAILED(range->lpVtbl->SetRange(range, start, start)) ||
            FAILED(range->lpVtbl->MoveEnd(range, tomCharFormat, 1, &moved)) ||
            FAILED(range->lpVtbl->GetEnd(range, &end)) || end <= start)
            return NOTE_PANE_LINK_UNAVAILABLE;
        enum note_pane_link_outcome outcome = collect_format(links, range, start, end);
        if (outcome != NOTE_PANE_LINK_READY)
            return outcome;
        start = end;
    }
    return NOTE_PANE_LINK_READY;
}

enum note_pane_link_outcome note_pane_links_create(ITextDocument *_Nonnull document,
                                                   struct note_pane_links *_Nullable *_Nonnull out)
{
    struct note_pane_links *_Nullable links = calloc(1, sizeof *links);
    if (links == nullptr)
        return NOTE_PANE_LINK_OUT_OF_MEMORY;
    ITextRange *_Nullable range = nullptr;
    enum note_pane_link_outcome outcome = NOTE_PANE_LINK_UNAVAILABLE;
    if (SUCCEEDED(document->lpVtbl->Range(document, 0, 0, &range)) && range != nullptr)
    {
        outcome = collect_runs(links, range);
        range->lpVtbl->Release(range);
    }
    if (outcome != NOTE_PANE_LINK_READY)
    {
        note_pane_links_destroy(links);
        return outcome;
    }
    *out = links;
    return NOTE_PANE_LINK_READY;
}

void note_pane_links_destroy(struct note_pane_links *_Nullable links)
{
    if (links != nullptr)
        free(links->hidden);
    free(links);
}

size_t note_pane_links_visible(const struct note_pane_links *_Nullable links, size_t native)
{
    if (links == nullptr)
        return native;
    size_t removed = 0;
    for (size_t index = 0; index < links->count; ++index)
    {
        const struct hidden_run *run = &links->hidden[index];
        if (native <= run->start)
            break;
        removed += (native < run->end ? native : run->end) - run->start;
    }
    return native - removed;
}

static LONG native_position(const struct note_pane_links *_Nullable links, size_t visible,
                            bool after_hidden)
{
    size_t removed = 0;
    if (links != nullptr)
    {
        for (size_t index = 0; index < links->count; ++index)
        {
            const struct hidden_run *run = &links->hidden[index];
            size_t boundary = run->start - removed;
            if (visible < boundary || (visible == boundary && !after_hidden))
                break;
            removed += run->end - run->start;
        }
    }
    return visible > (size_t)LONG_MAX - removed ? LONG_MAX : (LONG)(visible + removed);
}

CHARRANGE note_pane_links_native(const struct note_pane_links *_Nullable links,
                                 struct note_search_span span)
{
    return (CHARRANGE){.cpMin = native_position(links, span.start, true),
                       .cpMax = native_position(links, span.end, span.start == span.end)};
}

size_t note_pane_links_compact(const struct note_pane_links *_Nonnull links,
                               char16_t *_Nonnull units, size_t length)
{
    size_t source = 0;
    size_t written = 0;
    for (size_t index = 0; index < links->count; ++index)
    {
        const struct hidden_run *run = &links->hidden[index];
        size_t end = run->start < length ? run->start : length;
        size_t count = end - source;
        memmove(units + written, units + source, count * sizeof *units);
        written += count;
        source = run->end < length ? run->end : length;
        if (source == length)
            break;
    }
    memmove(units + written, units + source, (length - source) * sizeof *units);
    written += length - source;
    units[written] = u'\0';
    return written;
}

static enum note_pane_link_outcome convert_url(BSTR _Nonnull url,
                                               struct utf8_text *_Nullable *_Nonnull out)
{
    size_t length = SysStringLen(url);
    size_t start = length >= 2 && url[0] == L'"' && url[length - 1] == L'"' ? 1 : 0;
    length -= start * 2;
    if (length > web_uri_byte_limit)
        return NOTE_PANE_LINK_UNAVAILABLE;
    switch (utf8_text_create(url + start, length, out))
    {
    case UTF8_TEXT_CONVERTED:
        return NOTE_PANE_LINK_READY;
    case UTF8_TEXT_INVALID_UTF16:
        return NOTE_PANE_LINK_UNAVAILABLE;
    case UTF8_TEXT_OUT_OF_MEMORY:
        return NOTE_PANE_LINK_OUT_OF_MEMORY;
    }
    return NOTE_PANE_LINK_UNAVAILABLE;
}

/* RichEdit converts a field whose label equals its URL to a native autoURL, even when
 * automatic scanning is disabled. The native link type and extent remain authoritative. */
static HRESULT automatic_url(ITextRange2 *_Nonnull range, BSTR _Nullable *_Nonnull out)
{
    ITextFont2 *_Nullable font = nullptr;
    HRESULT read = range->lpVtbl->GetFont2(range, &font);
    if (FAILED(read) || font == nullptr)
        return E_FAIL;
    LONG kind = tomNoLink;
    read = font->lpVtbl->GetLinkType(font, &kind);
    font->lpVtbl->Release(font);
    if (FAILED(read))
        return read;
    if (kind != tomAutoLinkURL && kind != tomClientLink)
        return S_FALSE;
    LONG changed = 0;
    read = range->lpVtbl->Expand(range, (LONG)tomLink, &changed);
    return FAILED(read) ? read : range->lpVtbl->GetText(range, out);
}

static enum note_pane_link_outcome read_target(ITextRange2 *_Nonnull range, CHARRANGE selection,
                                               struct utf8_text *_Nullable *_Nonnull out)
{
    BSTR _Nullable url = nullptr;
    HRESULT read = range->lpVtbl->GetURL(range, &url);
    if (read == S_FALSE)
        read = automatic_url(range, &url);
    if (read == S_FALSE)
        return NOTE_PANE_LINK_ABSENT;
    LONG start = 0;
    LONG end = 0;
    enum note_pane_link_outcome outcome = NOTE_PANE_LINK_UNAVAILABLE;
    if (SUCCEEDED(read) && url != nullptr && SUCCEEDED(range->lpVtbl->GetStart(range, &start)) &&
        SUCCEEDED(range->lpVtbl->GetEnd(range, &end)))
    {
        outcome = selection.cpMin >= start && selection.cpMin < end && selection.cpMax <= end
                      ? convert_url(url, out)
                      : NOTE_PANE_LINK_ABSENT;
    }
    SysFreeString(url);
    return outcome;
}

enum note_pane_link_outcome note_pane_links_target(ITextDocument *_Nonnull document,
                                                   CHARRANGE selection,
                                                   struct utf8_text *_Nullable *_Nonnull out)
{
    if (selection.cpMin < 0 || selection.cpMin == LONG_MAX || selection.cpMax < selection.cpMin)
        return NOTE_PANE_LINK_ABSENT;
    ITextRange *_Nullable range = nullptr;
    /* GetURL requires a character extent; a collapsed caret is not a link range. */
    if (FAILED(document->lpVtbl->Range(document, selection.cpMin, selection.cpMin + 1, &range)) ||
        range == nullptr)
        return NOTE_PANE_LINK_UNAVAILABLE;
    void *_Nullable queried = nullptr;
    HRESULT upgraded = range->lpVtbl->QueryInterface(range, &range2_id, &queried);
    range->lpVtbl->Release(range);
    if (FAILED(upgraded) || queried == nullptr)
        return NOTE_PANE_LINK_UNAVAILABLE;
    ITextRange2 *_Nonnull extended = queried;
    enum note_pane_link_outcome outcome = read_target(extended, selection, out);
    extended->lpVtbl->Release(extended);
    return outcome;
}

void note_pane_links_input(struct note_pane_links *_Nullable links, UINT message, LPARAM position)
{
    if (links == nullptr)
        return;
    switch (message)
    {
    case WM_MOUSEMOVE:
        if (abs(GET_X_LPARAM(position) - links->down.x) >= GetSystemMetrics(SM_CXDRAG) ||
            abs(GET_Y_LPARAM(position) - links->down.y) >= GetSystemMetrics(SM_CYDRAG))
            links->gesture = NOTE_PANE_LINK_IDLE;
        return;
    case WM_LBUTTONUP:
        if (links->gesture == NOTE_PANE_LINK_PRESSED)
            links->gesture = NOTE_PANE_LINK_RELEASING;
        return;
    case WM_CAPTURECHANGED:
        /* RichEdit releases capture inside WM_LBUTTONUP before issuing EN_LINK. */
        if (links->gesture != NOTE_PANE_LINK_RELEASING)
            links->gesture = NOTE_PANE_LINK_IDLE;
        return;
    case WM_SETCURSOR:
    case WM_PAINT:
        return;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_CANCELMODE:
    case WM_KILLFOCUS:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_MOUSEWHEEL:
    case WM_SIZE:
    case WM_ENABLE:
    case WM_IME_STARTCOMPOSITION:
        links->gesture = NOTE_PANE_LINK_IDLE;
        return;
    default:
        return; /* Open Win32 message set. */
    }
}

bool note_pane_links_clicked(struct note_pane_links *_Nonnull links, HWND _Nonnull window,
                             const ENLINK *_Nonnull notice)
{
    if (notice->msg == WM_LBUTTONDOWN)
    {
        links->gesture = (notice->wParam & (MK_CONTROL | MK_SHIFT | MK_RBUTTON | MK_MBUTTON)) == 0
                             ? NOTE_PANE_LINK_PRESSED
                             : NOTE_PANE_LINK_IDLE;
        links->target = notice->chrg;
        links->down = (POINT){GET_X_LPARAM(notice->lParam), GET_Y_LPARAM(notice->lParam)};
        return false;
    }
    if (notice->msg != WM_LBUTTONUP)
        return false;
    bool released = links->gesture == NOTE_PANE_LINK_RELEASING;
    links->gesture = NOTE_PANE_LINK_IDLE;
    CHARRANGE selected = {0};
    SendMessageW(window, EM_EXGETSEL, 0, (LPARAM)&selected);
    return released && notice->chrg.cpMin == links->target.cpMin &&
           notice->chrg.cpMax == links->target.cpMax && selected.cpMin == selected.cpMax &&
           (notice->wParam & (MK_CONTROL | MK_SHIFT | MK_RBUTTON | MK_MBUTTON)) == 0;
}

void note_pane_links_finish_input(struct note_pane_links *_Nullable links, UINT message)
{
    if (links != nullptr && message == WM_LBUTTONUP)
        links->gesture = NOTE_PANE_LINK_IDLE;
}
