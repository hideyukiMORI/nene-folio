/* #520: real RichEdit image replacement on an isolated desktop. */
#include "markdown_media.h"
#include "markdown_rtf.h"
#include "note_minimap.h"
#include "note_pane.h"
#include "note_text.h"
#include "utf8_text.h"
#include <commdlg.h>
#include <limits.h>
#include <richole.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tom.h>

static unsigned checks;
static void require(bool value, const char *_Nonnull label)
{
    ++checks;
    if (!value)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        exit(EXIT_FAILURE);
    }
}
static void load(struct note_pane *_Nonnull pane, const char *_Nonnull source, enum pane_mode mode)
{
    struct note_text *_Nullable text = nullptr;
    struct markdown_rtf *_Nullable rtf = nullptr;
    require(note_text_create(source, strlen(source), &text) == NOTE_TEXT_ACCEPTED, "source");
    enum markdown_rtf_outcome result =
        mode == PANE_MODE_VIEW
            ? markdown_rtf_create(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Yu Gothic UI", &rtf)
            : markdown_rtf_plain(text, rtf_palette_for(FOLIO_THEME_LIGHT), "Yu Gothic UI", &rtf);
    require(result == MARKDOWN_RTF_CONVERTED, "canonical render");
    require(note_pane_load(pane, markdown_rtf_text(rtf), markdown_rtf_length(rtf), mode) ==
                NOTE_PANE_LOAD_READY,
            "native load");
    if (mode == PANE_MODE_VIEW)
    {
        enum image_outcome prepared =
            note_pane_prepare_images(pane, markdown_media_count(markdown_rtf_media(rtf)));
        if (prepared != IMAGE_READY)
        {
            wchar_t native[2048];
            GETTEXTEX query = {.cb = sizeof native, .flags = GT_RAWTEXT, .codepage = 1200};
            LRESULT count =
                SendMessageW(note_pane_handle(pane), EM_GETTEXTEX, (WPARAM)&query, (LPARAM)native);
            fprintf(stderr, "image preparation=%d raw:", (int)prepared);
            for (LRESULT index = 0; index < count; ++index)
                fprintf(stderr, " %04x", (unsigned)native[index]);
            fputc('\n', stderr);
        }
        require(prepared == IMAGE_READY, "identify native image markers");
    }
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}
static struct image_bitmap *_Nonnull bitmap(void)
{
    size_t length = 1200 * 600 * 4;
    unsigned char *_Nullable pixels = malloc(length);
    require(pixels != nullptr, "pixels");
    for (size_t index = 0; index < length; index += 4)
    {
        pixels[index] = 0x40;
        pixels[index + 1] = 0x90;
        pixels[index + 2] = 0xE0;
        pixels[index + 3] = 0xFF;
    }
    struct image_bitmap *_Nullable result = nullptr;
    require(image_bitmap_create(pixels, (struct image_size){1200, 600}, &result) == IMAGE_READY,
            "display bitmap");
    free(pixels);
    return result;
}
static LONG find(HWND _Nonnull window, const wchar_t *_Nonnull needle)
{
    FINDTEXTEXW query = {.chrg = {0, -1}, .lpstrText = needle};
    require(SendMessageW(window, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&query) >= 0,
            "native searchable text");
    return query.chrgText.cpMin;
}
static LRESULT raw(HWND _Nonnull window, wchar_t *_Nonnull bytes)
{
    GETTEXTEX query = {.cb = 8192 * sizeof(wchar_t), .flags = GT_RAWTEXT, .codepage = 1200};
    return SendMessageW(window, EM_GETTEXTEX, (WPARAM)&query, (LPARAM)bytes);
}
static LONG line_height(HWND _Nonnull window, LONG position)
{
    LRESULT line = SendMessageW(window, EM_EXLINEFROMCHAR, 0, position);
    LRESULT first = SendMessageW(window, EM_LINEINDEX, (WPARAM)line, 0);
    LRESULT next = SendMessageW(window, EM_LINEINDEX, (WPARAM)(line + 1), 0);
    POINTL a = {0}, b = {0};
    SendMessageW(window, EM_POSFROMCHAR, (WPARAM)&a, first);
    SendMessageW(window, EM_POSFROMCHAR, (WPARAM)&b, next);
    return b.y - a.y;
}
static LONG image_position(HWND _Nonnull window)
{
    wchar_t text[8192];
    raw(window, text);
    const wchar_t *marker = L"<nenefolio:image:0>";
    wchar_t *_Nullable found = wcsstr(text, marker);
    wchar_t *_Nullable last = nullptr;
    while (found != nullptr)
    {
        last = found;
        found = wcsstr(found + 1, marker);
    }
    require(last != nullptr, "image marker in raw text");
    return (LONG)(last - text + (ptrdiff_t)wcslen(marker));
}
static void resizing(struct note_pane *_Nonnull pane)
{
    HWND window = note_pane_handle(pane);
    wchar_t before[8192], after[8192];
    LRESULT length = raw(window, before);
    LONG alt = image_position(window);
    LONG height = line_height(window, alt);
    fprintf(stderr, "fitted image line height=%ld\n", height);
    require(height > 100 && height < 350, "image rendered at fitted aspect ratio");
    LONG link = find(window, L"after");
    SendMessageW(window, EM_SETSEL, (WPARAM)link, link + 1);
    struct utf8_text *_Nullable target = nullptr;
    require(note_pane_link_selection(pane, &target) == NOTE_PANE_LINK_READY &&
                strcmp(utf8_text_bytes(target), "https://example.com/two") == 0,
            "link target retained");
    utf8_text_destroy(target);
    MoveWindow(window, 0, 0, 300, 600, FALSE);
    require(note_pane_reflow(pane, PANE_MODE_VIEW) == NOTE_PANE_LOAD_READY &&
                note_pane_resize_image(pane) == IMAGE_PENDING,
            "width change schedules decoded image reflow");
    LONG smaller = line_height(window, image_position(window));
    require(smaller > 50 && smaller < height, "narrower view reduces image size");
    require(note_pane_resize_image(pane) == IMAGE_READY, "reflow queue drains");
    require(raw(window, after) == length &&
                memcmp(before, after, (size_t)length * sizeof(wchar_t)) == 0,
            "reflow preserves native text");
}
static void minimap(struct note_pane *_Nonnull pane)
{
    struct note_minimap *_Nullable map = nullptr;
    require(note_minimap_create(&map) == NOTE_MINIMAP_READY, "map");
    note_minimap_begin(map);
    for (size_t steps = 0; note_minimap_pending(map) && steps < 32; ++steps)
    {
        bool finished = note_minimap_step(map, pane);
        require(finished == !note_minimap_pending(map), "image map progresses");
    }
    require(!note_minimap_pending(map), "image map completes");
    note_minimap_destroy(map);
}
static void original_edit(struct note_pane *_Nonnull pane, const char *_Nonnull source)
{
    HWND window = note_pane_handle(pane);
    size_t index = 0;
    wchar_t after[8192];
    load(pane, source, PANE_MODE_EDIT);
    require(!note_pane_next_image(pane, &index), "edit drops image slots");
    wchar_t expected[8192];
    int converted = MultiByteToWideChar(CP_UTF8, 0, source, -1, expected, 8192);
    require(converted > 0, "source unicode");
    for (int at = 0; at < converted; ++at)
        if (expected[at] == L'\n')
            expected[at] = L'\r';
    raw(window, after);
    if (wcscmp(after, expected) != 0)
    {
        fwprintf(stderr, L"edit expected=%ls\nactual=%ls\n", expected, after);
    }
    require(wcscmp(after, expected) == 0, "edit reload is original Markdown");
    MoveWindow(window, 0, 0, 600, 600, FALSE);
}
static void preservation(struct note_pane *_Nonnull pane, const struct image_bitmap *_Nonnull image)
{
    const char source[] = "literal \\<nenefolio:image:0\\> literal-marker\n\n"
                          "[before](https://example.com/one)\n\n![alt](images/a.png)\n\n"
                          "[after](https://example.com/two)\n\nTail\n";
    load(pane, source, PANE_MODE_VIEW);
    HWND window = note_pane_handle(pane);
    size_t index = 99;
    require(note_pane_next_image(pane, &index) && index == 0,
            "visible literal marker is not an image slot");
    wchar_t before[8192], after[8192];
    LRESULT length = raw(window, before);
    CHARRANGE selected = {2, 7}, actual;
    SendMessageW(window, EM_EXSETSEL, 0, (LPARAM)&selected);
    LRESULT dirty = SendMessageW(window, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(window, EM_CANUNDO, 0, 0);
    require(note_pane_insert_image(pane, 0, image) == IMAGE_READY, "replace native placeholder");
    note_pane_finish_image(pane, 0);
    require(!note_pane_next_image(pane, &index), "completed slot is not re-requested");
    SendMessageW(window, EM_EXGETSEL, 0, (LPARAM)&actual);
    require(actual.cpMin == selected.cpMin && actual.cpMax == selected.cpMax,
            "selection preserved");
    require(SendMessageW(window, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(window, EM_CANUNDO, 0, 0) == undo,
            "dirty and Undo unchanged");
    require(raw(window, after) == length &&
                memcmp(before, after, (size_t)length * sizeof(wchar_t)) == 0,
            "raw text and hidden fields unchanged");
    require((GetWindowLongPtrW(window, GWL_STYLE) & ES_READONLY) != 0, "readonly restored");
    resizing(pane);
    minimap(pane);
    original_edit(pane, source);
}
static void inspect_table(HWND _Nonnull window)
{
    wchar_t raw_units[8192];
    LRESULT raw_count = raw(window, raw_units);
    fprintf(stderr, "table raw:");
    for (LRESULT at = 0; at < raw_count; ++at)
        fprintf(stderr, " %04x", (unsigned)raw_units[at]);
    fputc('\n', stderr);
    static const IID document_id = {
        0x8CC497C0, 0xA1DF, 0x11CE, {0x80, 0x98, 0x00, 0xAA, 0x00, 0x47, 0xBE, 0x5D}};
    static const IID range_id = {
        0xC241F5E2, 0x7206, 0x11D8, {0xA2, 0xC7, 0x00, 0xA0, 0xD1, 0xD6, 0xC6, 0xB3}};
    IRichEditOle *_Nullable objects = nullptr;
    require(SendMessageW(window, EM_GETOLEINTERFACE, 0, (LPARAM)&objects) != 0 &&
                objects != nullptr,
            "OLE interface");
    void *_Nullable result = nullptr;
    require(SUCCEEDED(objects->lpVtbl->QueryInterface(objects, &document_id, &result)),
            "text document");
    ITextDocument *document = result;
    ITextRange *_Nullable range = nullptr;
    LONG position = image_position(window);
    require(SUCCEEDED(document->lpVtbl->Range(document, position, position + 1, &range)),
            "image range");
    require(SUCCEEDED(range->lpVtbl->QueryInterface(range, &range_id, &result)),
            "image extended range");
    ITextRange2 *extended = result;
    ITextRow *_Nullable row = nullptr;
    HRESULT hr = extended->lpVtbl->GetRow(extended, &row);
    LONG count = -1, width = -1, margin = -1, start = -1, end = -1;
    if (SUCCEEDED(hr) && row != nullptr)
    {
        HRESULT c = row->lpVtbl->GetCellCount(row, &count);
        HRESULT w = row->lpVtbl->GetCellWidth(row, &width);
        HRESULT m = row->lpVtbl->GetCellMargin(row, &margin);
        fprintf(stderr, "table row hr=%lx count=%ld/%lx width=%ld/%lx margin=%ld/%lx\n",
                (unsigned long)hr, count, (unsigned long)c, width, (unsigned long)w, margin,
                (unsigned long)m);
        row->lpVtbl->Release(row);
    }
    extended->lpVtbl->GetStart(extended, &start);
    extended->lpVtbl->GetEnd(extended, &end);
    fprintf(stderr, "image range before=%ld after=%ld,%ld rowhr=%lx\n", position, start, end,
            (unsigned long)hr);
    extended->lpVtbl->Release(extended);
    range->lpVtbl->Release(range);
    document->lpVtbl->Release(document);
    objects->lpVtbl->Release(objects);
}
static void table_image(struct note_pane *_Nonnull pane, const struct image_bitmap *_Nonnull image)
{
    load(pane, "| Image | Name |\n| --- | --- |\n| ![cell](images/a.png) | text |\n\nEnd\n",
         PANE_MODE_VIEW);
    HWND window = note_pane_handle(pane);
    inspect_table(window);
    wchar_t before[8192], after[8192];
    LRESULT length = raw(window, before);
    require(note_pane_insert_image(pane, 0, image) == IMAGE_READY, "image inside native table");
    require(raw(window, after) == length &&
                memcmp(before, after, (size_t)length * sizeof(wchar_t)) == 0,
            "table raw text retained");
    inspect_table(window);
    LONG height = line_height(note_pane_handle(pane), image_position(note_pane_handle(pane)));
    printf("native table image height=%ld\n", height);
    require(height > 40 && height < 190, "image fits half-width table cell");
    RECT bounds;
    require(note_pane_image_bounds(pane, 0, &bounds) && bounds.right - bounds.left < 400 &&
                bounds.bottom - bounds.top > 40,
            "table image native geometry follows cell width");
    minimap(pane);
}
static void settle_wheel(void)
{
    HANDLE timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    require(timer != nullptr, "wheel completion timer");
    LARGE_INTEGER due = {.QuadPart = -4000000};
    require(SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE), "relative wheel interval");
    DWORD result = WAIT_FAILED;
    for (unsigned batch = 0; batch < 1000; ++batch)
    {
        result = MsgWaitForMultipleObjects(1, &timer, FALSE, INFINITE, QS_ALLINPUT);
        if (result != WAIT_OBJECT_0 + 1)
            break;
        MSG message;
        for (unsigned at = 0; at < 1000 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++at)
            DispatchMessageW(&message);
    }
    CloseHandle(timer);
    require(result == WAIT_OBJECT_0, "wheel settled within bounded message batches");
}

static LONG scroll_y(HWND window)
{
    POINT point = {0};
    SendMessageW(window, EM_GETSCROLLPOS, 0, (LPARAM)&point);
    return point.y;
}

static void wheel_geometry(struct note_pane *_Nonnull pane,
                           const struct image_bitmap *_Nonnull image)
{
    HWND window = note_pane_handle(pane);
    MoveWindow(window, 0, 0, 600, 240, FALSE);
    ShowWindow(GetParent(window), SW_SHOWNOACTIVATE);
    char source[8192] = "before\n\n![image](images/a.png)\n\nafter\n\n";
    for (size_t at = 0; at < 80; ++at)
        strcat_s(source, sizeof source, "body\n\n");
    load(pane, source, PANE_MODE_VIEW);
    require(note_pane_insert_image(pane, 0, image) == IMAGE_READY, "wheel image fixture");
    struct note_pane_geometry view;
    require(note_pane_viewport(pane, &view), "native document geometry");
    RECT bounds;
    require(note_pane_image_bounds(pane, 0, &bounds), "native image bounds");
    printf("geometry height=%d visible=%d step=%d image=%ld,%ld..%ld,%ld\n", view.height,
           view.visible, view.step, bounds.left, bounds.top, bounds.right, bounds.bottom);
    require(bounds.right - bounds.left > 500 && bounds.bottom - bounds.top > 250,
            "image rectangle includes native width and height");
    int height = view.height;
    UINT lines = 0;
    require(SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0), "OS wheel setting");
    LONGLONG expected = lines == WHEEL_PAGESCROLL ? view.visible : (LONGLONG)view.step * lines;
    expected = expected > view.visible ? view.visible : expected;
    for (int start = 0; start <= 400; start += 100)
    {
        note_pane_scroll_pixels(pane, start);
        note_pane_scroll_wheel(pane, -WHEEL_DELTA);
        settle_wheel();
        LONG distance = scroll_y(window) - start;
        printf("wheel start=%d distance=%ld expected=%lld\n", start, distance, expected);
        require(llabs(distance - expected) <= 1, "same wheel amount before inside and after image");
        require(note_pane_viewport(pane, &view) && view.height == height,
                "document extent does not change while scrolling");
    }
    note_pane_scroll_pixels(pane, INT_MAX);
    require(scroll_y(window) == note_pane_geometry_limit(view), "bottom does not scroll to blank");
    note_pane_scroll_wheel(pane, -WHEEL_DELTA);
    settle_wheel();
    require(scroll_y(window) == note_pane_geometry_limit(view), "wheel stops at real bottom");
    note_pane_scroll_pixels(pane, -1);
    require(scroll_y(window) == 0, "negative target clamps to top");
}

static void image_map_geometry(struct note_pane *_Nonnull pane)
{
    struct note_minimap *_Nullable map = nullptr;
    require(note_minimap_create(&map) == NOTE_MINIMAP_READY, "image map");
    note_minimap_begin(map);
    for (size_t at = 0; at < 32 && note_minimap_pending(map); ++at)
    {
        bool finished = note_minimap_step(map, pane);
        require(finished == !note_minimap_pending(map), "bounded image map progress");
    }
    require(!note_minimap_pending(map), "image and text map complete");
    struct note_minimap_paint paint = {.bounds = {0, 0, 100, 400}, .dpi = 96};
    require(note_pane_viewport(pane, &paint.view), "map geometry");
    RECT track = note_minimap_track(&paint);
    RECT thumb = note_minimap_thumb(&paint);
    require(note_minimap_drag(&paint, track.bottom, 0) == note_pane_geometry_limit(paint.view),
            "drag reaches document bottom");
    paint.view.first = note_pane_geometry_limit(paint.view);
    thumb = note_minimap_thumb(&paint);
    require(thumb.bottom == track.bottom &&
                note_minimap_drag(&paint, thumb.top, 0) == paint.view.first,
            "bottom thumb and drag share pixel coordinate");
    paint.view.first /= 2;
    thumb = note_minimap_thumb(&paint);
    int mapped = note_minimap_drag(&paint, thumb.top, 0);
    int tolerance = 1 + paint.view.height / (track.bottom - track.top - (thumb.bottom - thumb.top));
    require(abs(mapped - paint.view.first) <= tolerance, "middle thumb maps to same document area");
    struct folio_palette palette = {.pane = RGB(255, 255, 255),
                                    .dialog_control = RGB(12, 34, 56),
                                    .gutter_text = RGB(44, 55, 66)};
    HDC device = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateBitmap(100, 400, 1, 32, nullptr);
    require(device != nullptr && bitmap != nullptr, "map bitmap");
    HGDIOBJ previous = SelectObject(device, bitmap);
    note_minimap_draw(map, device, &palette, &paint);
    RECT native;
    require(note_pane_image_bounds(pane, 0, &native), "mapped image bounds");
    int y = track.top +
            MulDiv(native.top + native.bottom, track.bottom - track.top, 2 * paint.view.height);
    require(GetPixel(device, 25, y) == palette.dialog_control,
            "minimap paints image block at native document height");
    SelectObject(device, previous);
    DeleteObject(bitmap);
    DeleteDC(device);
    note_minimap_destroy(map);
}

static void image_anchor(struct note_pane *_Nonnull pane, const struct image_bitmap *_Nonnull image)
{
    HWND window = note_pane_handle(pane);
    char source[4096] = "![image](images/a.png)\n\n";
    for (size_t at = 0; at < 80; ++at)
        strcat_s(source, sizeof source, "body\n\n");
    load(pane, source, PANE_MODE_VIEW);
    LONG body = find(window, L"body");
    CHARRANGE selected = {body + 1, body + 3}, actual;
    SendMessageW(window, EM_EXSETSEL, 0, (LPARAM)&selected);
    note_pane_scroll_pixels(pane, 250);
    LRESULT first = SendMessageW(window, EM_GETFIRSTVISIBLELINE, 0, 0);
    LRESULT anchor = SendMessageW(window, EM_LINEINDEX, (WPARAM)first, 0);
    POINTL before = {0}, after = {0};
    SendMessageW(window, EM_POSFROMCHAR, (WPARAM)&before, anchor);
    require(note_pane_insert_image(pane, 0, image) == IMAGE_READY, "image above viewport arrives");
    SendMessageW(window, EM_POSFROMCHAR, (WPARAM)&after, anchor);
    require(before.y == after.y, "asynchronous image keeps the visible text anchor");
    wchar_t text[8192], changed[8192];
    LRESULT length = raw(window, text);
    LRESULT dirty = SendMessageW(window, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(window, EM_CANUNDO, 0, 0);
    note_pane_scroll_pixels(pane, 100);
    note_pane_scroll_wheel(pane, -WHEEL_DELTA);
    require(note_pane_insert_image(pane, 0, image) == IMAGE_READY,
            "image replacement while wheel is active");
    settle_wheel();
    UINT lines = 0;
    require(SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0), "wheel lines");
    require(lines == 0 || scroll_y(window) > 100, "image arrival retains active wheel movement");
    SendMessageW(window, EM_EXGETSEL, 0, (LPARAM)&actual);
    require(actual.cpMin == selected.cpMin && actual.cpMax == selected.cpMax,
            "image and pixel scroll retain selection");
    require(raw(window, changed) == length &&
                memcmp(text, changed, (size_t)length * sizeof(wchar_t)) == 0 &&
                SendMessageW(window, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(window, EM_CANUNDO, 0, 0) == undo,
            "pixel scroll and image arrival preserve text dirty and Undo");
}

static void image_only(struct note_pane *_Nonnull pane, const struct image_bitmap *_Nonnull image)
{
    HWND window = note_pane_handle(pane);
    MoveWindow(window, 0, 0, 600, 120, FALSE);
    load(pane, "![](images/a.png)", PANE_MODE_VIEW);
    require(note_pane_insert_image(pane, 0, image) == IMAGE_READY, "single image document");
    struct note_pane_geometry view;
    require(note_pane_viewport(pane, &view) && view.height > view.visible,
            "one image line has scrollable document height");
    require(SendMessageW(window, EM_GETLINECOUNT, 0, 0) == 1, "single display line fixture");
    note_pane_scroll_pixels(pane, INT_MAX);
    require(scroll_y(window) == note_pane_geometry_limit(view),
            "single image can scroll to bottom");
    int height = view.height;
    require(SendMessageW(window, EM_SETZOOM, 2, 1), "native zoom");
    require(note_pane_viewport(pane, &view) && view.height > height, "zoom changes native extent");
    require(SendMessageW(window, EM_SETZOOM, 1, 1), "zoom reset");
    MoveWindow(window, 0, 0, 300, 120, FALSE);
    require(note_pane_reflow(pane, PANE_MODE_VIEW) == NOTE_PANE_LOAD_READY &&
                note_pane_resize_image(pane) == IMAGE_PENDING,
            "single image resize");
    require(note_pane_viewport(pane, &view) && view.height < height &&
                scroll_y(window) <= note_pane_geometry_limit(view),
            "resize recalculates height and clamps native position");
}

static void geometry_boundaries(struct note_pane *_Nonnull pane,
                                const struct image_bitmap *_Nonnull image)
{
    HWND window = note_pane_handle(pane);
    MoveWindow(window, 0, 0, 600, 240, FALSE);
    char source[12000] = "";
    for (size_t at = 0; at < 20; ++at)
        strcat_s(source, sizeof source, "![](images/a.png)\n\n");
    for (size_t at = 0; at < 3000; ++at)
        strcat_s(source, sizeof source, "x\n\n");
    load(pane, source, PANE_MODE_VIEW);
    require(note_pane_insert_image(pane, 19, image) == IMAGE_READY,
            "offscreen image uses same native geometry");
    require(note_pane_image_count(pane) == 20, "all image slots are represented");
    struct note_pane_geometry before, after;
    require(note_pane_viewport(pane, &before) && before.height > 70000,
            "long document exceeds sixteen bit coordinates");
    RECT bounds;
    require(!note_pane_image_bounds(pane, 20, &bounds), "image index boundary");
    note_pane_scroll_pixels(pane, 70000);
    require(scroll_y(window) == 70000 && note_pane_viewport(pane, &after) &&
                after.height == before.height && after.first == 70000,
            "long document position and extent remain exact");
    note_pane_scroll_pixels(pane, INT_MAX);
    require(scroll_y(window) == note_pane_geometry_limit(before), "long document bottom clamp");
    struct note_minimap *_Nullable map = nullptr;
    require(note_minimap_create(&map) == NOTE_MINIMAP_READY, "bounded image map");
    note_minimap_begin(map);
    unsigned batches = 0;
    while (note_minimap_pending(map) && batches < 32)
    {
        bool finished = note_minimap_step(map, pane);
        require(finished == !note_minimap_pending(map), "large image map progress");
        ++batches;
    }
    require(!note_minimap_pending(map) && batches == 18,
            "256 text samples and 20 images use separate sixteen item batches");
    note_minimap_destroy(map);
}

static void mixed_media(struct note_pane *_Nonnull pane, const struct image_bitmap *_Nonnull image)
{
    const char source[] = "Before\n\n```mermaid\nflowchart LR\nA --> B\n```\n\n"
                          "![image](images/a.png)\n\n"
                          "```mermaid\nsequenceDiagram\nA->>B: hello\n```\n\nAfter\n";
    MoveWindow(note_pane_handle(pane), 0, 0, 600, 600, FALSE);
    load(pane, source, PANE_MODE_VIEW);
    HWND window = note_pane_handle(pane);
    require(note_pane_image_count(pane) == 3, "diagrams and images share ordered display slots");
    wchar_t before[8192], after[8192];
    LRESULT length = raw(window, before);
    CHARRANGE selected = {1, 4}, actual;
    SendMessageW(window, EM_EXSETSEL, 0, (LPARAM)&selected);
    LRESULT dirty = SendMessageW(window, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(window, EM_CANUNDO, 0, 0);
    for (size_t index = 0; index < 3; ++index)
    {
        require(note_pane_insert_image(pane, index, image) == IMAGE_READY,
                "mixed native insertion");
        note_pane_finish_image(pane, index);
    }
    SendMessageW(window, EM_EXGETSEL, 0, (LPARAM)&actual);
    require(actual.cpMin == selected.cpMin && actual.cpMax == selected.cpMax &&
                SendMessageW(window, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(window, EM_CANUNDO, 0, 0) == undo,
            "mixed insertion preserves selection dirty and Undo");
    require(raw(window, after) == length &&
                memcmp(before, after, (size_t)length * sizeof(wchar_t)) == 0,
            "mixed insertion preserves all native text and hidden markers");
    size_t index = 99;
    require(!note_pane_next_image(pane, &index), "all mixed slots completed");
    minimap(pane);
    original_edit(pane, source);
}

int main(void)
{
    HDESK desktop = CreateDesktopW(L"NeNeFolioImages520", nullptr, nullptr, 0,
                                   DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_READOBJECTS |
                                       DESKTOP_WRITEOBJECTS,
                                   nullptr);
    require(desktop != nullptr && SetThreadDesktop(desktop), "isolated desktop");
    require(SUCCEEDED(OleInitialize(nullptr)), "OLE");
    HWND parent = CreateWindowExW(0, L"STATIC", L"image fixture", WS_OVERLAPPEDWINDOW, 0, 0, 800,
                                  700, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    require(parent != nullptr, "parent");
    struct note_pane *_Nullable pane = nullptr;
    require(note_pane_create(parent, RGB(255, 255, 255), RGB(0, 0, 0), &pane) == NOTE_PANE_CREATED,
            "pane");
    MoveWindow(note_pane_handle(pane), 0, 0, 600, 600, FALSE);
    struct image_bitmap *image = bitmap();
    mixed_media(pane, image);
    preservation(pane, image);
    table_image(pane, image);
    wheel_geometry(pane, image);
    image_map_geometry(pane);
    image_anchor(pane, image);
    image_only(pane, image);
    geometry_boundaries(pane, image);
    image_bitmap_destroy(image);
    note_pane_destroy(pane);
    DestroyWindow(parent);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("image display: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
