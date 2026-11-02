#include "note_pane_images.h"
#include "markdown_images.h"
#include "note_pane_table.h"
#include <commdlg.h>
#include <richedit.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct note_pane_images
{
    HWND _Nonnull window;
    LONG positions[markdown_images_reference_limit];
    LONG rows[markdown_images_display_limit];
    bool finished[markdown_images_reference_limit];
    struct image_bitmap *_Nullable bitmaps[markdown_images_display_limit];
    size_t resizing;
    size_t count;
    size_t bytes;
};
static const IID range2_id = {
    0xC241F5E2, 0x7206, 0x11D8, {0xA2, 0xC7, 0x00, 0xA0, 0xD1, 0xD6, 0xC6, 0xB3}};
static bool hidden_marker(ITextDocument *_Nonnull document, LONG start, LONG end)
{
    ITextRange *_Nullable range = nullptr;
    if (FAILED(document->lpVtbl->Range(document, start, end, &range)) || range == nullptr)
        return false;
    ITextFont *_Nullable font = nullptr;
    LONG hidden = tomFalse;
    bool ready = SUCCEEDED(range->lpVtbl->GetFont(range, &font));
    if (ready && font != nullptr)
    {
        ready = SUCCEEDED(font->lpVtbl->GetHidden(font, &hidden)) && hidden == tomTrue;
        font->lpVtbl->Release(font);
    }
    range->lpVtbl->Release(range);
    return ready;
}
static wchar_t *_Nullable raw_text(HWND _Nonnull window)
{
    GETTEXTLENGTHEX measuring = {.flags = GTL_NUMCHARS | GTL_PRECISE, .codepage = 1200};
    LRESULT length = SendMessageW(window, EM_GETTEXTLENGTHEX, (WPARAM)&measuring, 0);
    if (length < 0 || (uint64_t)length >= UINT32_MAX / sizeof(wchar_t))
        return nullptr;
    size_t bytes = ((size_t)length + 1) * sizeof(wchar_t);
    wchar_t *_Nullable text = malloc(bytes);
    if (text == nullptr)
        return nullptr;
    GETTEXTEX query = {.cb = (DWORD)bytes, .flags = GT_RAWTEXT, .codepage = 1200};
    LRESULT copied = SendMessageW(window, EM_GETTEXTEX, (WPARAM)&query, (LPARAM)text);
    text[copied > 0 ? (size_t)copied : 0] = L'\0';
    return text;
}
static bool locate(const wchar_t *_Nonnull text, ITextDocument *_Nonnull document, size_t index,
                   LONG *_Nonnull position)
{
    wchar_t marker[40];
    if (swprintf_s(marker, 40, L"<nenefolio:image:%zu>", index) < 0)
        return false;
    size_t length = wcslen(marker);
    const wchar_t *_Nullable found = wcsstr(text, marker);
    while (found != nullptr)
    {
        LONG start = (LONG)(found - text);
        LONG end = start + (LONG)length;
        if (found[length] == 0xFFFC && hidden_marker(document, start, end))
        {
            *position = end;
            return true;
        }
        found = wcsstr(found + length, marker);
    }
    return false;
}
static bool collect_positions(struct note_pane_images *_Nonnull images,
                              ITextDocument *_Nonnull document)
{
    wchar_t *_Nullable text = raw_text(images->window);
    if (text == nullptr)
        return false;
    bool ready = true;
    for (size_t index = 0; index < images->count && ready; ++index)
    {
        ready = locate(text, document, index, &images->positions[index]);
        if (ready)
            images->rows[index] =
                note_pane_table_image_row(images->window, text, (size_t)images->positions[index]);
    }
    free(text);
    return ready;
}
enum image_outcome note_pane_images_create(HWND _Nonnull window, ITextDocument *_Nonnull document,
                                           size_t count,
                                           struct note_pane_images *_Nullable *_Nonnull out)
{
    if (count > markdown_images_display_limit)
        return IMAGE_TOO_LARGE;
    struct note_pane_images *_Nullable images = calloc(1, sizeof *images);
    if (images == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    images->count = count;
    images->window = window;
    images->resizing = count;
    if (!collect_positions(images, document))
    {
        note_pane_images_destroy(images);
        return IMAGE_FAILED;
    }
    *out = images;
    return IMAGE_READY;
}
bool note_pane_images_next(const struct note_pane_images *_Nonnull images, HWND _Nonnull window,
                           size_t *_Nonnull index)
{
    RECT client;
    GetClientRect(window, &client);
    LONG margin = client.bottom - client.top;
    for (size_t at = 0; at < images->count; ++at)
    {
        if (images->finished[at])
            continue;
        POINTL point = {0};
        SendMessageW(window, EM_POSFROMCHAR, (WPARAM)&point, images->positions[at]);
        if (point.y >= -margin && point.y <= client.bottom + margin)
        {
            *index = at;
            return true;
        }
    }
    return false;
}
static IStream *_Nullable bitmap_stream(const struct image_bitmap *_Nonnull bitmap)
{
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, image_bitmap_length(bitmap));
    if (memory == nullptr)
        return nullptr;
    void *_Nullable bytes = GlobalLock(memory);
    if (bytes == nullptr)
    {
        GlobalFree(memory);
        return nullptr;
    }
    memcpy(bytes, image_bitmap_bytes(bitmap), image_bitmap_length(bitmap));
    GlobalUnlock(memory);
    IStream *_Nullable stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)))
        GlobalFree(memory);
    return stream;
}
static enum image_outcome insert_at(ITextRange2 *_Nonnull range,
                                    const struct image_bitmap *_Nonnull bitmap,
                                    HWND _Nonnull window, LONG row)
{
    IStream *_Nullable stream = bitmap_stream(bitmap);
    if (stream == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    struct image_size size = image_bitmap_size(bitmap);
    LONG width = (LONG)((uint64_t)size.width * 2540 / 96);
    LONG height = (LONG)((uint64_t)size.height * 2540 / 96);
    RECT area;
    SendMessageW(window, EM_GETRECT, 0, (LPARAM)&area);
    int numerator = 0;
    int denominator = 0;
    SendMessageW(window, EM_GETZOOM, (WPARAM)&numerator, (LPARAM)&denominator);
    LONG available = MulDiv(area.right - area.left, 2540, (int)GetDpiForWindow(window));
    if (numerator > 0 && denominator > 0)
        available = MulDiv(available, denominator, numerator);
    LONG twips = note_pane_table_image_width(window, row);
    if (twips > 0)
    {
        LONG cell = MulDiv(twips, 2540, 1440);
        if (cell < available)
            available = cell;
    }
    if (available > 0 && width > available)
    {
        height = MulDiv(height, available, width);
        width = available;
    }
    BSTR alt = SysAllocString(L"");
    HRESULT inserted =
        range->lpVtbl->InsertImage(range, width, height, 0, TA_BASELINE, alt, stream);
    SysFreeString(alt);
    stream->lpVtbl->Release(stream);
    return SUCCEEDED(inserted) ? IMAGE_READY : IMAGE_FAILED;
}
static enum image_outcome replace_at(struct note_pane_images *_Nonnull images,
                                     ITextDocument *_Nonnull document, size_t index,
                                     const struct image_bitmap *_Nonnull bitmap)
{
    LONG position = images->positions[index];
    ITextRange *_Nullable range = nullptr;

    if (FAILED(document->lpVtbl->Range(document, position, position + 1, &range)) ||
        range == nullptr)
    {
        return IMAGE_FAILED;
    }
    void *_Nullable extended = nullptr;
    HRESULT queried = range->lpVtbl->QueryInterface(range, &range2_id, &extended);
    range->lpVtbl->Release(range);
    if (FAILED(queried) || extended == nullptr)
    {
        return IMAGE_FAILED;
    }
    ITextRange2 *image = extended;
    enum image_outcome outcome = insert_at(image, bitmap, images->window, images->rows[index]);
    image->lpVtbl->Release(image);
    return outcome;
}
enum image_outcome note_pane_images_insert(struct note_pane_images *_Nonnull images,
                                           ITextDocument *_Nonnull document, size_t index,
                                           const struct image_bitmap *_Nonnull bitmap)
{
    if (index >= images->count)
        return IMAGE_FAILED;
    size_t previous =
        images->bitmaps[index] == nullptr ? 0 : image_bitmap_length(images->bitmaps[index]);
    if (images->bytes - previous + image_bitmap_length(bitmap) > 64 * 1024 * 1024)
        return IMAGE_TOO_LARGE;
    struct image_bitmap *_Nullable copy = nullptr;
    if (images->bitmaps[index] != bitmap && image_bitmap_clone(bitmap, &copy) != IMAGE_READY)
        return IMAGE_OUT_OF_MEMORY;
    enum image_outcome outcome = replace_at(images, document, index, bitmap);
    if (outcome == IMAGE_READY && copy != nullptr)
    {
        images->bytes = images->bytes - previous + image_bitmap_length(copy);
        image_bitmap_destroy(images->bitmaps[index]);
        images->bitmaps[index] = copy;
        copy = nullptr;
    }
    image_bitmap_destroy(copy);
    return outcome;
}
void note_pane_images_finish(struct note_pane_images *_Nonnull images, size_t index)
{
    if (index < images->count)
        images->finished[index] = true;
}
void note_pane_images_destroy(struct note_pane_images *_Nullable images)
{
    if (images != nullptr)
        for (size_t index = 0; index < images->count; ++index)
            image_bitmap_destroy(images->bitmaps[index]);
    free(images);
}
void note_pane_images_reflow(struct note_pane_images *_Nonnull images)
{
    images->resizing = 0;
}
const struct image_bitmap *_Nullable note_pane_images_resize_next(
    struct note_pane_images *_Nonnull images, size_t *_Nonnull index)
{
    while (images->resizing < images->count)
    {
        size_t at = images->resizing++;
        if (images->bitmaps[at] != nullptr)
        {
            *index = at;
            return images->bitmaps[at];
        }
    }
    return nullptr;
}
