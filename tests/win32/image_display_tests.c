/* #520: real RichEdit image replacement on an isolated desktop. */
#include "markdown_images.h"
#include "markdown_rtf.h"
#include "note_minimap.h"
#include "note_pane.h"
#include "note_text.h"
#include "utf8_text.h"
#include <commdlg.h>
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
            note_pane_prepare_images(pane, markdown_images_count(markdown_rtf_images(rtf)));
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
    preservation(pane, image);
    table_image(pane, image);
    image_bitmap_destroy(image);
    note_pane_destroy(pane);
    DestroyWindow(parent);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("image display: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
