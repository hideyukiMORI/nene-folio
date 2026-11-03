/* #506: Native formatting is sampled without editing or moving the real document. */
#include "markdown_rtf.h"
#include "note_minimap.h"
#include "note_pane.h"
#include "note_text.h"
#include <commdlg.h>
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

static void load_palette(struct note_pane *_Nonnull pane, enum pane_mode mode,
                         const char *_Nonnull content, struct rtf_palette palette)
{
    struct note_text *_Nullable text = nullptr;
    struct markdown_rtf *_Nullable rtf = nullptr;
    require(note_text_create(content, strlen(content), &text) == NOTE_TEXT_ACCEPTED, "source");
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

static void load(struct note_pane *_Nonnull pane, enum pane_mode mode, const char *_Nonnull content)
{
    load_palette(pane, mode, content, rtf_palette_for(FOLIO_THEME_LIGHT));
}

struct sample_tally
{
    unsigned headings;
    unsigned rules;
    unsigned markers;
    unsigned tables;
    unsigned code;
    unsigned empty_code;
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

static void inspect_code(const struct note_pane_sample *_Nonnull sample,
                         struct sample_tally *_Nonnull tally)
{
    if (sample->background > 0xFFFFFF)
        return;
    if (sample->points <= 2)
    {
        ++tally->rules;
        return;
    }
    ++tally->code;
    if (sample->columns == 0)
        ++tally->empty_code;
}

static void code_blocks(struct note_pane *_Nonnull pane)
{
    const char content[] =
        "Plain\n\n`inline` after\n\n```c\nint a = 1;\n\nreturn a;\n```\n\n---\n\nTail\n";
    load(pane, PANE_MODE_VIEW, content);
    int count = (int)SendMessageW(note_pane_handle(pane), EM_GETLINECOUNT, 0, 0);
    struct sample_tally tally = {0};
    for (int line = 0; line < count; ++line)
    {
        struct note_pane_sample sample;
        require(note_pane_samples(pane, &line, 1, &sample), "code native sample");
        inspect_code(&sample, &tally);
    }
    printf("native code=%u empty=%u rules=%u\n", tally.code, tally.empty_code, tally.rules);
    require(tally.code == 4 && tally.empty_code == 1 && tally.rules == 1,
            "code background includes its language label and empty line only");
    int inline_line = 1;
    struct note_pane_sample sample;
    require(note_pane_samples(pane, &inline_line, 1, &sample) && sample.points == 10 &&
                sample.background > 0xFFFFFF,
            "inline code is not a shaded block");
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

static void token_color(HWND handle, const wchar_t *_Nonnull word, struct rgb_color color)
{
    FINDTEXTEXW find = {{0, -1}, word, {0, 0}};
    require(SendMessageW(handle, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&find) >= 0,
            "native syntax text is searchable");
    SendMessageW(handle, EM_EXSETSEL, 0, (LPARAM)&find.chrgText);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    require((format.dwMask & CFM_COLOR) != 0 &&
                format.crTextColor == RGB(color.red, color.green, color.blue),
            "RichEdit uses the canonical syntax color");
}

static void syntax_preserved(struct note_pane *_Nonnull pane)
{
    HWND handle = note_pane_handle(pane);
    CHARRANGE before = {0}, after = {0};
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&before);
    LRESULT dirty = SendMessageW(handle, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(handle, EM_CANUNDO, 0, 0);
    wchar_t text[512], actual[512];
    int length = GetWindowTextW(handle, text, 512);
    struct note_minimap *_Nullable map = nullptr;
    require(note_minimap_create(&map) == NOTE_MINIMAP_READY, "syntax map");
    note_minimap_begin(map);
    unsigned slices = 0;
    while (note_minimap_pending(map) && slices++ < 16)
    {
        bool finished = note_minimap_step(map, pane);
        require(finished == !note_minimap_pending(map), "syntax map completion");
    }
    require(!note_minimap_pending(map), "syntax map bounded");
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&after);
    require(before.cpMin == after.cpMin && before.cpMax == after.cpMax,
            "syntax sampling preserves selection");
    require(SendMessageW(handle, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(handle, EM_CANUNDO, 0, 0) == undo,
            "syntax sampling preserves dirty and Undo");
    require(GetWindowTextW(handle, actual, 512) == length &&
                memcmp(text, actual, ((size_t)length + 1) * sizeof text[0]) == 0,
            "syntax sampling preserves displayed text");
    note_minimap_destroy(map);
}

static void syntax_palette(struct note_pane *_Nonnull pane, const char *_Nonnull content,
                           struct rtf_palette palette)
{
    load_palette(pane, PANE_MODE_VIEW, content, palette);
    HWND handle = note_pane_handle(pane);
    wchar_t text[512];
    GetWindowTextW(handle, text, 512);
    require(wcscmp(text,
                   L"c\r\nint value = 42; // comment\r\n\r\nconst char *s = \"{日本語}\\path\";") ==
                0,
            "syntax preserves braces, slash, Unicode, spaces and empty line");
    token_color(handle, L"int", palette.code_keyword);
    token_color(handle, L"c", palette.code_comment);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    require((format.dwMask & (CFM_SIZE | CFM_BACKCOLOR)) == (CFM_SIZE | CFM_BACKCOLOR) &&
                format.yHeight == 180 &&
                format.crBackColor == RGB(palette.code_background.red,
                                          palette.code_background.green,
                                          palette.code_background.blue),
            "label uses nine points and the same code background in every palette");
    require((format.dwMask & CFM_FACE) != 0 && wcscmp(format.szFaceName, L"Consolas") != 0,
            "label uses the proportional body face");
    token_color(handle, L"value", palette.code_text);
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    require((format.dwMask & CFM_FACE) != 0 && wcscmp(format.szFaceName, L"Consolas") == 0,
            "code body restores Consolas after the label");
    token_color(handle, L"42", palette.code_number);
    token_color(handle, L"// comment", palette.code_comment);
    token_color(handle, L"\"{日本語}\\path\"", palette.code_string);
    syntax_preserved(pane);
}

static void code_syntax(struct note_pane *_Nonnull pane)
{
    const char content[] = "```c\nint value = 42; // comment\n\n"
                           "const char *s = \"{日本語}\\path\";\n```\n";
    syntax_palette(pane, content, rtf_palette_for(FOLIO_THEME_LIGHT));
    syntax_palette(pane, content, rtf_palette_for(FOLIO_THEME_DARK));
    struct rtf_palette contrast = rtf_palette_for(FOLIO_THEME_LIGHT);
    contrast.code_text = contrast.code_keyword = contrast.code_string = contrast.code_comment =
        contrast.code_number = (struct rgb_color){255, 255, 0};
    contrast.code_background = (struct rgb_color){0, 0, 0};
    syntax_palette(pane, content, contrast);
    load(pane, PANE_MODE_EDIT, content);
    wchar_t text[512];
    GetWindowTextW(note_pane_handle(pane), text, 512);
    require(wcsstr(text, L"```c\r\nint value = 42;") != nullptr &&
                wcsstr(text, L";\r\n```") != nullptr,
            "editing returns to source fences");
    token_color(note_pane_handle(pane), L"int", rtf_palette_for(FOLIO_THEME_LIGHT).text);
    preservation(pane);
}

static void code_label_boundaries(struct note_pane *_Nonnull pane)
{
    const char content[] = "Before\n\n``` \tPyThOn ignored\nreturn 42\n```\n\n"
                           "```mystery\n```\n\n```\nplain\n```\n\nTail";
    load(pane, PANE_MODE_VIEW, content);
    HWND handle = note_pane_handle(pane);
    wchar_t actual[512];
    GetWindowTextW(handle, actual, 512);
    require(wcscmp(actual, L"Before\r\nPyThOn\r\nreturn 42\r\nmystery\r\nplain\r\nTail") == 0,
            "labels include unknown and empty fences, omit absent info and extra attributes");
    FINDTEXTEXW find = {{0, -1}, L"return 42", {0, 0}};
    require(SendMessageW(handle, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&find) >= 0,
            "code after label remains searchable");
    note_pane_select(pane, (size_t)find.chrgText.cpMin, (size_t)find.chrgText.cpMax);
    SendMessageW(handle, EM_GETSELTEXT, 0, (LPARAM)actual);
    require(wcscmp(actual, L"return 42") == 0, "body selection does not acquire label text");
    const char16_t *visible = nullptr;
    size_t length = 0;
    require(note_pane_text(pane, &visible, &length) == NOTE_PANE_TEXT_TAKEN && length > 15 &&
                visible[7] == u'P' && visible[14] == u'r',
            "search projection includes the visible label and correct body offset");
    token_color(handle, L"PyThOn", rtf_palette_for(FOLIO_THEME_LIGHT).code_comment);
    CHARFORMAT2W label_format = {.cbSize = sizeof label_format};
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&label_format);
    token_color(handle, L"Tail", rtf_palette_for(FOLIO_THEME_LIGHT).text);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    require(wcscmp(format.szFaceName, label_format.szFaceName) == 0,
            "label and following prose share the body font");
    require(format.yHeight == 220 && (format.dwEffects & CFE_AUTOBACKCOLOR) != 0,
            "label font and code background do not leak to following prose");
    syntax_preserved(pane);
}

static void code_label_wrap(struct note_pane *_Nonnull pane)
{
    const char content[] = "~~~VeryLongUnknownLanguageNameThatMustWrapWithoutLosingAnyCharacters"
                           " extra\npayload\n~~~\nTail";
    load(pane, PANE_MODE_VIEW, content);
    HWND handle = note_pane_handle(pane);
    wchar_t before[512], after[512];
    GetWindowTextW(handle, before, 512);
    MoveWindow(handle, 0, 0, 160, 600, FALSE);
    ShowWindow(GetParent(handle), SW_SHOWNOACTIVATE);
    RECT bounds;
    GetWindowRect(handle, &bounds);
    LPARAM pointer = MAKELPARAM(bounds.left + 8, bounds.top + 8);
    SendMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(MK_CONTROL, 5 * WHEEL_DELTA), pointer);
    require(note_pane_zoom_percent(pane) == 150, "label uses canonical zoom path");
    FINDTEXTEXW find = {{0, -1}, L"payload", {0, 0}};
    require(SendMessageW(handle, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&find) >= 0 &&
                SendMessageW(handle, EM_EXLINEFROMCHAR, 0, find.chrgText.cpMin) > 1,
            "long language label wraps above its code at narrow enlarged width");
    GetWindowTextW(handle, after, 512);
    require(wcscmp(before, after) == 0, "wrapping and zoom do not truncate label or body");
    SendMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(MK_CONTROL, (WORD)(-5 * WHEEL_DELTA)), pointer);
    require(note_pane_zoom_percent(pane) == 100, "zoom restored");
    MoveWindow(handle, 0, 0, 600, 600, FALSE);
    ShowWindow(GetParent(handle), SW_HIDE);
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
    code_blocks(pane);
    code_syntax(pane);
    code_label_boundaries(pane);
    code_label_wrap(pane);
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
