/* #506: Native formatting is sampled without editing or moving the real document. */
#include "markdown_rtf.h"
#include "note_minimap.h"
#include "note_pane.h"
#include "note_text.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
static const char source[] =
    "# Heading one\n\n## Heading two\n\n### Heading three\n\n#### Heading four\n\n"
    "Plain body text\n\n---\n\n- First bullet\n  - Nested bullet\n- [ ] Task\n\n"
    "1. Ordered\n2. Ordered second\n\n| Name | Value |\n| --- | --- |\n"
    "| alpha | 100 |\n| beta | 200 |\n\nEnd\n";

static void require(bool condition, const char *_Nonnull label)
{
    ++checks;
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        exit(EXIT_FAILURE);
    }
}

static void load(struct note_pane *_Nonnull pane, enum pane_mode mode, const char *_Nonnull content)
{
    struct note_text *_Nullable text = nullptr;
    struct markdown_rtf *_Nullable rtf = nullptr;
    require(note_text_create(content, strlen(content), &text) == NOTE_TEXT_ACCEPTED, "source");
    struct rtf_palette palette = rtf_palette_for(FOLIO_THEME_LIGHT);
    enum markdown_rtf_outcome made = MARKDOWN_RTF_OUT_OF_MEMORY;
    switch (mode)
    {
    case PANE_MODE_VIEW:
        made = markdown_rtf_create(text, palette, "Yu Gothic UI", &rtf);
        break;
    case PANE_MODE_EDIT:
        made = markdown_rtf_plain(text, palette, "Yu Gothic UI", &rtf);
        break;
    }
    require(made == MARKDOWN_RTF_CONVERTED, "canonical RTF");
    require(note_pane_load(pane, markdown_rtf_text(rtf), markdown_rtf_length(rtf), mode) ==
                NOTE_PANE_LOAD_READY,
            "native document");
    markdown_rtf_destroy(rtf);
    note_text_destroy(text);
}

struct sample_tally
{
    unsigned headings;
    unsigned rules;
    unsigned markers;
    unsigned tables;
};

static void inspect_sample(const struct note_pane_sample *_Nonnull sample,
                           struct sample_tally *_Nonnull tally)
{
    const float sizes[] = {22, 17, 14, 11};
    if (sample->space_before > 0 && sample->columns > 0 && tally->headings < 4)
    {
        require(sample->points == sizes[tally->headings], "h1-h4 native size");
        ++tally->headings;
    }
    if (sample->columns == 0 && sample->points <= 2 && sample->background <= 0xFFFFFF)
        ++tally->rules;
    if (sample->marker)
    {
        require(sample->indent >= 22, "list native indent");
        ++tally->markers;
    }
    if (sample->cells > 0)
    {
        require(sample->cells == 2, "native table column count");
        ++tally->tables;
    }
}

static void view_structure(struct note_pane *_Nonnull pane)
{
    int count = (int)SendMessageW(note_pane_handle(pane), EM_GETLINECOUNT, 0, 0);
    struct sample_tally tally = {0};
    for (int line = 0; line < count; ++line)
    {
        struct note_pane_sample sample;
        require(note_pane_samples(pane, &line, 1, &sample), "native sample");
        inspect_sample(&sample, &tally);
    }
    printf("native headings=%u rules=%u markers=%u tables=%u\n", tally.headings, tally.rules,
           tally.markers, tally.tables);
    require(tally.headings == 4 && tally.rules == 1 && tally.markers == 5 && tally.tables == 3,
            "all requested native structures are represented");
}

static void preservation(struct note_pane *_Nonnull pane)
{
    HWND handle = note_pane_handle(pane);
    SendMessageW(handle, EM_SETSEL, 0, 0);
    SendMessageW(handle, EM_REPLACESEL, TRUE, (LPARAM)L"unsaved ");
    CHARRANGE selected = {2, 6}, after = {0};
    SendMessageW(handle, EM_EXSETSEL, 0, (LPARAM)&selected);
    LRESULT dirty = SendMessageW(handle, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(handle, EM_CANUNDO, 0, 0);
    POINT before = {0}, scroll = {0};
    SendMessageW(handle, EM_GETSCROLLPOS, 0, (LPARAM)&before);
    wchar_t text[1024], actual[1024];
    int length = GetWindowTextW(handle, text, 1024);
    struct note_minimap *_Nullable map = nullptr;
    require(note_minimap_create(&map) == NOTE_MINIMAP_READY, "map");
    note_minimap_begin(map);
    unsigned slices = 0;
    while (note_minimap_pending(map) && slices < 16)
    {
        bool finished = note_minimap_step(map, pane);
        require(finished == !note_minimap_pending(map), "finite build result");
        ++slices;
    }
    require(!note_minimap_pending(map), "bounded native build");
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&after);
    SendMessageW(handle, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    require(selected.cpMin == after.cpMin && selected.cpMax == after.cpMax, "selection retained");
    require(before.x == scroll.x && before.y == scroll.y, "scroll retained");
    require(dirty && undo && SendMessageW(handle, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(handle, EM_CANUNDO, 0, 0) == undo,
            "dirty and Undo retained");
    require(GetWindowTextW(handle, actual, 1024) == length &&
                memcmp(text, actual, ((size_t)length + 1) * sizeof text[0]) == 0,
            "text retained");
    note_minimap_destroy(map);
}

static void wrapped_list(struct note_pane *_Nonnull pane)
{
    const char content[] =
        "- A long list item that wraps onto several native display lines. "
        "Its continuation must retain the indent without repeating the marker.\n";
    MoveWindow(note_pane_handle(pane), 0, 0, 160, 600, FALSE);
    load(pane, PANE_MODE_VIEW, content);
    int count = (int)SendMessageW(note_pane_handle(pane), EM_GETLINECOUNT, 0, 0);
    require(count > 1, "wrapped list fixture");
    unsigned markers = 0;
    for (int line = 0; line < count; ++line)
    {
        struct note_pane_sample sample;
        require(note_pane_samples(pane, &line, 1, &sample), "wrapped native sample");
        if (sample.marker)
            ++markers;
    }
    require(markers == 1, "wrapped continuation does not repeat list marker");
    MoveWindow(note_pane_handle(pane), 0, 0, 600, 600, FALSE);
}

static void boundaries(struct note_pane *_Nonnull pane)
{
    int lines[17] = {0};
    struct note_pane_sample samples[17];
    require(!note_pane_samples(pane, lines, 17, samples), "oversized batch rejected");
    lines[0] = -1;
    require(!note_pane_samples(pane, lines, 1, samples), "negative display line rejected");
    lines[0] = INT_MAX;
    require(!note_pane_samples(pane, lines, 1, samples), "absent display line rejected");
    lines[0] = 0;
    require(note_pane_samples(pane, lines, 16, samples), "maximum batch accepted");
    for (size_t at = 0; at < 16; ++at)
        require(samples[at].points == 11 && !samples[at].marker && samples[at].cells == 0,
                "editing uses plain native appearance");
}

int main(void)
{
    HDESK desktop = CreateDesktopW(L"NeNeFolioMinimap506", nullptr, nullptr, 0,
                                   DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_READOBJECTS |
                                       DESKTOP_WRITEOBJECTS,
                                   nullptr);
    require(desktop != nullptr && SetThreadDesktop(desktop), "isolated desktop");
    require(SUCCEEDED(OleInitialize(nullptr)), "OLE");
    HWND parent = CreateWindowExW(0, L"STATIC", L"synthetic", WS_OVERLAPPEDWINDOW, 0, 0, 800, 700,
                                  nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    require(parent != nullptr, "parent");
    struct note_pane *_Nullable pane = nullptr;
    require(note_pane_create(parent, RGB(255, 255, 255), RGB(0, 0, 0), &pane) == NOTE_PANE_CREATED,
            "pane");
    MoveWindow(note_pane_handle(pane), 0, 0, 600, 600, FALSE);
    load(pane, PANE_MODE_VIEW, source);
    view_structure(pane);
    wrapped_list(pane);
    load(pane, PANE_MODE_EDIT, source);
    boundaries(pane);
    preservation(pane);
    note_pane_destroy(pane);
    DestroyWindow(parent);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("minimap structure: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
