/* #506: Native formatting is sampled without editing or moving the real document. */
#include "caret_command.h"
#include "markdown_rtf.h"
#include "note_minimap.h"
#include "note_pane.h"
#include "note_text.h"
#include "theme_palette.h"
#include "utf8_text.h"
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
    struct theme_palette theme = *folio_theme_palette(FOLIO_THEME_LIGHT);
    theme.rtf = palette;
    switch (mode)
    {
    case PANE_MODE_VIEW:
        made = markdown_rtf_create(
            text, &theme, (struct markdown_rtf_locale){"Yu Gothic UI", FOLIO_LANGUAGE_JA}, &rtf);
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
    require(
        wcscmp(text,
               L"c\x2003HYPERLINK "
               L"\"nenefolio-code:"
               L"0\"\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0\xA0\r\nint "
               L"value = 42; // comment\r\n\r\n"
               L"const char *s = \"{日本語}\\path\";") == 0,
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
    require(wcscmp(actual, L"Before\r\nPyThOn\x2003HYPERLINK "
                           L"\"nenefolio-code:"
                           L"0\"\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0"
                           L"\xA0\r\nreturn 42"
                           L"\r\nmystery\r\nHYPERLINK "
                           L"\"nenefolio-code:"
                           L"1\"\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0"
                           L"\xA0\r\nplain\r\nTail") == 0,
            "labels include unknown and empty fences, omit absent info and extra attributes; "
            "blocks with code carry the copy action and the empty fence does not");
    FINDTEXTEXW find = {{0, -1}, L"return 42", {0, 0}};
    require(SendMessageW(handle, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&find) >= 0,
            "code after label remains searchable");
    const char16_t *visible = nullptr;
    size_t length = 0;
    require(note_pane_text(pane, &visible, &length) == NOTE_PANE_TEXT_TAKEN && length > 24 &&
                visible[7] == u'P' && visible[13] == u'\x2003' && visible[14] == u'\r' &&
                visible[15] == u'r',
            "search projection includes the visible label, omits the copy action and keeps the "
            "correct body offset");
    /* The search position of the code (15) is not its native position: the action is skipped. */
    note_pane_select(pane, 15, 24);
    CHARRANGE selected = {0};
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&selected);
    SendMessageW(handle, EM_GETSELTEXT, 0, (LPARAM)actual);
    require(wcscmp(actual, L"return 42") == 0 && selected.cpMin == find.chrgText.cpMin &&
                selected.cpMax == find.chrgText.cpMax,
            "body selection does not acquire label or action text");
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

static LONG nth_copy_label(HWND handle, size_t nth)
{
    FINDTEXTEXW find = {{0, -1}, L"コピー", {0, 0}};
    for (size_t at = 0; at <= nth; ++at)
    {
        require(SendMessageW(handle, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find) >= 0,
                "the copy word occurrence exists in the native document");
        find.chrg.cpMin = find.chrgText.cpMax;
    }
    return find.chrgText.cpMin;
}

static void expect_copy_target(struct note_pane *_Nonnull pane, LONG position,
                               const char *_Nullable expected)
{
    SendMessageW(note_pane_handle(pane), EM_SETSEL, (WPARAM)position, position);
    struct utf8_text *_Nullable target = nullptr;
    enum note_pane_link_outcome outcome = note_pane_link_selection(pane, &target);
    if (expected == nullptr)
    {
        require(outcome == NOTE_PANE_LINK_ABSENT && target == nullptr,
                "prose is not an actionable link");
        return;
    }
    require(outcome == NOTE_PANE_LINK_READY && target != nullptr &&
                strcmp(utf8_text_bytes(target), expected) == 0,
            "the caret on a copy action resolves to its block's internal target");
    utf8_text_destroy(target);
}

static size_t count_copy_words(const char16_t *_Nonnull text, size_t length, size_t *_Nonnull last)
{
    static const char16_t word[] = u"コピー";
    size_t count = 0;
    for (size_t at = 0; at + 3 <= length; ++at)
        if (memcmp(text + at, word, 3 * sizeof word[0]) == 0)
        {
            ++count;
            *last = at;
        }
    return count;
}

/* #839 round 2: the action is a filled chip at the right edge of the header row. */
static void code_copy_chip(struct note_pane *_Nonnull pane, LONG first)
{
    HWND handle = note_pane_handle(pane);
    CHARRANGE label = {first, first + 3};
    SendMessageW(handle, EM_EXSETSEL, 0, (LPARAM)&label);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    const struct theme_palette *theme = folio_theme_palette(FOLIO_THEME_LIGHT);
    printf("copy label effects=%lx mask=%lx height=%ld color=%06lx\n", format.dwEffects,
           format.dwMask, format.yHeight, format.crTextColor);
    require((format.dwEffects & CFE_LINK) != 0 && format.yHeight == 180 &&
                format.crTextColor == RGB(theme->rtf.code_comment.red,
                                          theme->rtf.code_comment.green,
                                          theme->rtf.code_comment.blue) &&
                (format.dwEffects & CFE_AUTOBACKCOLOR) == 0 &&
                format.crBackColor == RGB(theme->rtf.code_background.red,
                                          theme->rtf.code_background.green,
                                          theme->rtf.code_background.blue),
            "the action is a native link in the label's size and muted colour on the code surface");
    /* The link run is the whole chip: it reserves the painted button with no-break spaces. */
    CHARRANGE chip = {first - 12, first + 7};
    SendMessageW(handle, EM_EXSETSEL, 0, (LPARAM)&chip);
    wchar_t padded[32];
    SendMessageW(handle, EM_GETSELTEXT, 0, (LPARAM)padded);
    require(wcscmp(padded,
                   L"\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0\xA0") == 0,
            "the chip pads the word inside the link");
    expect_copy_target(pane, first - 12, "nenefolio-code:0");
    expect_copy_target(pane, first + 6, "nenefolio-code:0");
    PARAFORMAT2 paragraph = {.cbSize = sizeof paragraph};
    SendMessageW(handle, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph);
    require((paragraph.dwMask & PFM_ALIGNMENT) != 0 && paragraph.wAlignment == PFA_RIGHT &&
                paragraph.dxRightIndent == 0 && paragraph.dySpaceBefore == 40 &&
                paragraph.dySpaceAfter == 60 && paragraph.bLineSpacingRule == 4 &&
                paragraph.dyLineSpacing == 260,
            "the header row is right aligned, flush with the block's right edge and compact");
}

/* #839: the copy action is a native link, absent from the search projection and from EDIT. */
static void code_copy_action(struct note_pane *_Nonnull pane)
{
    const char content[] =
        "コピー 前文\n\n```c\nint a;\n```\n\n> ```\n> 引用\n> ```\n\n終わり コピー";
    load(pane, PANE_MODE_VIEW, content);
    HWND handle = note_pane_handle(pane);
    wchar_t actual[512];
    GetWindowTextW(handle, actual, 512);
    require(
        wcscmp(actual,
               L"コピー 前文\r\nc\x2003HYPERLINK "
               L"\"nenefolio-code:"
               L"0\"\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0\xA0\r\nint "
               L"a;\r\n"
               L"HYPERLINK "
               L"\"nenefolio-code:"
               L"1\"\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0\xA0\r\n引用"
               L"\r\n終わり コピー") == 0,
        "every block with code shows one copy action after its optional label");
    LONG first = nth_copy_label(handle, 1);
    LONG second = nth_copy_label(handle, 2);
    expect_copy_target(pane, first, "nenefolio-code:0");
    expect_copy_target(pane, first + 2, "nenefolio-code:0");
    expect_copy_target(pane, second + 1, "nenefolio-code:1");
    expect_copy_target(pane, nth_copy_label(handle, 0), nullptr);
    expect_copy_target(pane, nth_copy_label(handle, 3) + 1, nullptr);
    code_copy_chip(pane, first);
    const char16_t *visible = nullptr;
    size_t length = 0;
    size_t last = 0;
    require(note_pane_text(pane, &visible, &length) == NOTE_PANE_TEXT_TAKEN &&
                count_copy_words(visible, length, &last) == 2,
            "the search projection keeps the two prose words and drops both actions");
    note_pane_select(pane, last, last + 3);
    CHARRANGE selected = {0};
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin == nth_copy_label(handle, 3) && selected.cpMax == selected.cpMin + 3,
            "a search position after the actions maps to the real prose word");
    size_t start = 0;
    size_t end = 0;
    require(note_pane_selection(pane, &start, &end) && start == last && end == last + 3,
            "the native selection maps back to the same search position");
    load(pane, PANE_MODE_EDIT, content);
    GetWindowTextW(handle, actual, 512);
    require(wcscmp(actual,
                   L"コピー 前文\r\n\r\n```c\r\nint a;\r\n```\r\n\r\n> ```\r\n> 引用\r\n> ```"
                   L"\r\n\r\n終わり コピー") == 0,
            "editing shows the Markdown source with nothing added");
    struct utf8_text *_Nullable target = nullptr;
    require(note_pane_link_selection(pane, &target) == NOTE_PANE_LINK_ABSENT,
            "editing has no copy action");
}

static LONG native_find(HWND _Nonnull handle, const wchar_t *_Nonnull word)
{
    FINDTEXTEXW find = {{0, -1}, word, {0, 0}};
    require(SendMessageW(handle, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find) >= 0,
            "the word exists in the native document");
    return find.chrgText.cpMin;
}

static void expect_link(struct note_pane *_Nonnull pane, LONG position,
                        const char *_Nullable expected)
{
    SendMessageW(note_pane_handle(pane), EM_SETSEL, (WPARAM)position, position);
    struct utf8_text *_Nullable target = nullptr;
    enum note_pane_link_outcome outcome = note_pane_link_selection(pane, &target);
    if (outcome == NOTE_PANE_LINK_READY)
        printf("bare link at %ld: %s\n", position, utf8_text_bytes(target));
    if (expected == nullptr)
        require(outcome == NOTE_PANE_LINK_ABSENT && target == nullptr,
                "text next to a bare link is not a link");
    else
        require(outcome == NOTE_PANE_LINK_READY && target != nullptr &&
                    strcmp(utf8_text_bytes(target), expected) == 0,
                "the caret on a bare link resolves to its validated URI");
    utf8_text_destroy(target);
}

static bool near_colour(const unsigned char *_Nonnull pixel, struct rgb_color colour)
{
    return abs(pixel[2] - colour.red) + abs(pixel[1] - colour.green) + abs(pixel[0] - colour.blue) <
           60;
}

/* Pixels of the text between two characters on one line: theme link colour and strongly blue
 * (a native autoURL). ClearType fringes on other glyphs are outside the rectangle. */
static size_t link_counts[2];

static void count_link_row(const unsigned char *_Nonnull row, LONG from, LONG to)
{
    struct rgb_color link = rtf_palette_for(FOLIO_THEME_LIGHT).link;
    for (LONG x = from; x < to; ++x)
    {
        const unsigned char *pixel = row + (size_t)x * 4;
        link_counts[0] += near_colour(pixel, link) ? 1 : 0;
        link_counts[1] += pixel[0] > pixel[2] + 100 && pixel[0] > pixel[1] + 40 ? 1 : 0;
    }
}

static void count_link_area(HWND _Nonnull handle, const unsigned char *_Nonnull pixels, LONG width,
                            CHARRANGE text)
{
    POINTL start = {0};
    POINTL end = {0};
    SendMessageW(handle, EM_POSFROMCHAR, (WPARAM)&start, text.cpMin);
    SendMessageW(handle, EM_POSFROMCHAR, (WPARAM)&end, text.cpMax);
    require(end.y == start.y && end.x > start.x, "the link text is on one line");
    link_counts[0] = link_counts[1] = 0;
    for (LONG y = start.y; y < start.y + 28; ++y)
        count_link_row(pixels + (size_t)y * (size_t)width * 4, start.x, end.x);
}

/* #852 / ADR0100: a link that shows its own URL paints in the theme's link colour, not as a
 * native autoURL in the system's blue. The pane is printed, not read back from the screen. */
static void link_colour(HWND _Nonnull handle, CHARRANGE text)
{
    RECT client;
    GetClientRect(handle, &client);
    HDC screen = GetDC(handle);
    HDC dc = CreateCompatibleDC(screen);
    BITMAPINFO info = {.bmiHeader = {.biSize = sizeof info.bmiHeader,
                                     .biWidth = client.right,
                                     .biHeight = -client.bottom,
                                     .biPlanes = 1,
                                     .biBitCount = 32,
                                     .biCompression = BI_RGB}};
    void *_Nullable bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    require(bitmap != nullptr && bits != nullptr, "link colour bitmap");
    HGDIOBJ old = SelectObject(dc, bitmap);
    SendMessageW(handle, WM_PRINT, (WPARAM)dc, PRF_CLIENT | PRF_ERASEBKGND);
    count_link_area(handle, bits, client.right, text);
    size_t *counts = link_counts;
    printf("link colour pixels: theme %zu, blue %zu\n", counts[0], counts[1]);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(handle, screen);
    require(counts[0] > 100 && counts[1] * 10 < counts[0],
            "a link that shows its URL paints in the theme's link colour");
}

/* #850 / ADR0099: bare URLs are native links whose shown text is exactly the source text. */
static void bare_links(struct note_pane *_Nonnull pane)
{
    const char content[] = "詳細はhttps://example.com/a。と www.example.org/b. 終わり\n\n"
                           "`https://example.com/code` x";
    static const char16_t shown[] = u"詳細はhttps://example.com/a。と www.example.org/b. 終わり\r"
                                    u"https://example.com/code x";
    load(pane, PANE_MODE_VIEW, content);
    HWND handle = note_pane_handle(pane);
    const char16_t *visible = nullptr;
    size_t length = 0;
    require(note_pane_text(pane, &visible, &length) == NOTE_PANE_TEXT_TAKEN, "search projection");
    if (length != sizeof shown / sizeof shown[0] - 1 ||
        memcmp(visible, shown, length * sizeof shown[0]) != 0)
    {
        printf("projection:");
        for (size_t index = 0; index < length; ++index)
            printf(" %04x", (unsigned)visible[index]);
        printf("\n");
    }
    require(length == sizeof shown / sizeof shown[0] - 1 &&
                memcmp(visible, shown, length * sizeof shown[0]) == 0,
            "search sees the shown text without hidden URLs or boundaries");
    /* ADR0100: the hidden instruction carries an upper-case scheme; the application lowers it
     * again when it builds the web URI. */
    LONG first = native_find(handle, L"https://example.com/a");
    link_colour(handle, (CHARRANGE){first, first + 21});
    expect_link(pane, first + 1, "HTTPS://example.com/a");
    expect_link(pane, first + 20, "HTTPS://example.com/a");
    expect_link(pane, native_find(handle, L"。と"), nullptr);
    LONG second = native_find(handle, L"www.example.org/b");
    expect_link(pane, second + 1, "HTTP://www.example.org/b");
    expect_link(pane, second + 16, "HTTP://www.example.org/b");
    expect_link(pane, native_find(handle, L"終わり"), nullptr);
    expect_link(pane, native_find(handle, L"example.com/code") + 1, nullptr);
    load(pane, PANE_MODE_EDIT, content);
    wchar_t actual[512];
    GetWindowTextW(handle, actual, 512);
    require(wcscmp(actual, L"詳細はhttps://example.com/a。と www.example.org/b. 終わり\r\n\r\n"
                           L"`https://example.com/code` x") == 0,
            "editing shows the Markdown source with nothing added");
    struct utf8_text *_Nullable target = nullptr;
    SendMessageW(handle, EM_SETSEL, 5, 5);
    require(note_pane_link_selection(pane, &target) == NOTE_PANE_LINK_ABSENT,
            "editing has no bare link");
}

static LONG height_at(HWND _Nonnull handle, LONG position)
{
    CHARRANGE one = {position, position + 1};
    SendMessageW(handle, EM_EXSETSEL, 0, (LPARAM)&one);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(handle, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    return format.yHeight;
}

static const char *const heading_decorations[] = {"**太字**",
                                                  "*斜体*",
                                                  "~~取消~~",
                                                  "***両方***",
                                                  "`code`",
                                                  "[リンク](https://example.com/)",
                                                  "<https://example.com/>",
                                                  "https://example.com/",
                                                  "&amp;"};

/* One heading level (6 is setext h1) with one decoration between plain text. */
static void heading_inline_size(struct note_pane *_Nonnull pane, size_t level, size_t at)
{
    static const char *const prefixes[] = {"# ", "## ", "### ", "#### ", "##### ", "###### "};
    static const wchar_t *const words[] = {L"太字",
                                           L"斜体",
                                           L"取消",
                                           L"両方",
                                           L"code",
                                           L"リンク",
                                           L"https://example.com/",
                                           L"https://example.com/",
                                           L"&"};
    char content[160];
    int written = level < 6
                      ? snprintf(content, sizeof content, "%s前%s後", prefixes[level],
                                 heading_decorations[at])
                      : snprintf(content, sizeof content, "前%s後\n===", heading_decorations[at]);
    require(written > 0, "heading sample");
    load(pane, PANE_MODE_VIEW, content);
    HWND handle = note_pane_handle(pane);
    LONG plain = height_at(handle, native_find(handle, L"前"));
    LONG decorated = height_at(handle, native_find(handle, words[at]));
    /* yHeight is in twips (10 per half-point); code keeps the body's 20:22 ratio. */
    LONG expected = at == 4 ? (plain / 10 * 10 + 5) / 11 * 10 : plain;
    if (decorated != expected)
        printf("heading-size h%zu %s plain=%ld decorated=%ld expected=%ld\n", level + 1,
               heading_decorations[at], plain, decorated, expected);
    require(decorated == expected, "an inline decoration keeps the heading's size");
}

/* #853: an inline decoration inside a heading keeps the heading's size. */
static void heading_inline_sizes(struct note_pane *_Nonnull pane)
{
    for (size_t level = 0; level < 7; ++level)
        for (size_t at = 0; at < sizeof heading_decorations / sizeof heading_decorations[0]; ++at)
            heading_inline_size(pane, level, at);
}

static void press_line_key(HWND _Nonnull handle, WPARAM key, bool shift)
{
    BYTE state[256];
    GetKeyboardState(state);
    state[VK_SHIFT] = shift ? 0x80 : 0;
    SetKeyboardState(state);
    SendMessageW(handle, WM_KEYDOWN, key, 1);
    SendMessageW(handle, WM_KEYUP, key, (LPARAM)0xC0000001);
    state[VK_SHIFT] = 0;
    SetKeyboardState(state);
}

/* Characters as caret positions count them: one per paragraph break, unlike WM_GETTEXTLENGTH. */
static LONG character_count(HWND _Nonnull handle)
{
    GETTEXTLENGTHEX query = {.flags = GTL_NUMCHARS | GTL_PRECISE, .codepage = 1200};
    return (LONG)SendMessageW(handle, EM_GETTEXTLENGTHEX, (WPARAM)&query, 0);
}

constexpr WPARAM with_shift = 0x10000;

/* One line move from a collapsed caret. 'K' and 'J' are the Ctrl+K / Ctrl+J commands; a key
 * with `with_shift` is pressed with Shift held. */
static void expect_edge(struct note_pane *_Nonnull pane, LONG caret, WPARAM pressed,
                        CHARRANGE expected)
{
    HWND handle = note_pane_handle(pane);
    WPARAM key = pressed & ~with_shift;
    bool shift = (pressed & with_shift) != 0;
    SendMessageW(handle, EM_SETSEL, (WPARAM)caret, caret);
    if (key == 'K' || key == 'J')
        SendMessageW(handle, WM_COMMAND,
                     MAKEWPARAM(key == 'K' ? CARET_COMMAND_UP : CARET_COMMAND_DOWN, 1), 0);
    else
        press_line_key(handle, key, shift);
    CHARRANGE actual = {0, 0};
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&actual);
    if (actual.cpMin != expected.cpMin || actual.cpMax != expected.cpMax)
        printf("edge key %u shift %d from %ld: %ld..%ld, wanted %ld..%ld\n", (unsigned)key, shift,
               caret, actual.cpMin, actual.cpMax, expected.cpMin, expected.cpMax);
    require(actual.cpMin == expected.cpMin && actual.cpMax == expected.cpMax,
            "a line move at the document's edge lands like Notepad");
}

static void edge_active_end(HWND _Nonnull handle, LONG last, LONG end);

/* Two paragraphs: the edges, Ctrl+K/J, Shift and a move between them. */
static void edge_arrows_mode(struct note_pane *_Nonnull pane, enum pane_mode mode)
{
    load(pane, mode, "first line\n\nsecond line");
    HWND handle = note_pane_handle(pane);
    LONG end = character_count(handle);
    LONG lines = (LONG)SendMessageW(handle, EM_GETLINECOUNT, 0, 0);
    LONG last = (LONG)SendMessageW(handle, EM_LINEINDEX, (WPARAM)(lines - 1), 0);
    expect_edge(pane, 4, VK_UP, (CHARRANGE){0, 0});
    expect_edge(pane, last + 3, VK_DOWN, (CHARRANGE){end, end});
    expect_edge(pane, 0, VK_UP, (CHARRANGE){0, 0});
    expect_edge(pane, end, VK_DOWN, (CHARRANGE){end, end});
    expect_edge(pane, 4, 'K', (CHARRANGE){0, 0});
    expect_edge(pane, last + 3, 'J', (CHARRANGE){end, end});
    expect_edge(pane, 4, VK_UP | with_shift, (CHARRANGE){0, 4});
    expect_edge(pane, last + 3, VK_DOWN | with_shift, (CHARRANGE){last + 3, end});
    SendMessageW(handle, EM_SETSEL, 4, 4);
    press_line_key(handle, VK_DOWN, false);
    CHARRANGE moved = {0, 0};
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&moved);
    require(moved.cpMin == moved.cpMax && moved.cpMin > 4 && moved.cpMin < end,
            "a line move away from the edges stays native");
    edge_active_end(handle, last, end);
}

/* Shift moves the active end: a selection whose active end is not on the edge row is native,
 * even when its other end is (anchor at the text's end, active on the first row, and the
 * reverse). The anchors are the text's ends so a native move can never land on them. */
static void edge_active_end(HWND _Nonnull handle, LONG last, LONG end)
{
    SendMessageW(handle, EM_SETSEL, (WPARAM)end, 4);
    press_line_key(handle, VK_DOWN, true);
    CHARRANGE moved = {0, 0};
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&moved);
    /* RichEdit itself may grow an anchor at the text's end over the final paragraph mark. */
    require(moved.cpMax >= end && moved.cpMin > 4 && moved.cpMin <= last + 3,
            "Shift+Down moves an active end on the first row by one native line");
    SendMessageW(handle, EM_SETSEL, 0, (LPARAM)(last + 3));
    press_line_key(handle, VK_UP, true);
    SendMessageW(handle, EM_EXGETSEL, 0, (LPARAM)&moved);
    require(moved.cpMin == 0 && moved.cpMax > 0 && moved.cpMax < last,
            "Shift+Up moves an active end on the last row by one native line");
}

/* #854: Up on the first display line goes to the start, Down on the last one to the end. */
static void edge_arrows(struct note_pane *_Nonnull pane)
{
    edge_arrows_mode(pane, PANE_MODE_EDIT);
    edge_arrows_mode(pane, PANE_MODE_VIEW);
    /* A long wrapped paragraph: the edge is the first and last display row. */
    load(pane, PANE_MODE_EDIT,
         "word word word word word word word word word word word word word word word word word "
         "word word word word word word word word word word word word word word word word word");
    HWND handle = note_pane_handle(pane);
    require(SendMessageW(handle, EM_GETLINECOUNT, 0, 0) > 1, "the paragraph wraps");
    LONG end = character_count(handle);
    expect_edge(pane, 3, VK_UP, (CHARRANGE){0, 0});
    expect_edge(pane, end - 2, VK_DOWN, (CHARRANGE){end, end});
    load(pane, PANE_MODE_EDIT, "");
    expect_edge(pane, 0, VK_UP, (CHARRANGE){0, 0});
    expect_edge(pane, 0, VK_DOWN, (CHARRANGE){0, 0});
    load(pane, PANE_MODE_EDIT, "single");
    expect_edge(pane, 3, VK_UP, (CHARRANGE){0, 0});
    expect_edge(pane, 3, VK_DOWN, (CHARRANGE){6, 6});
}

/* #861 / ADR0101: a reference link opens its definition's URI; the definition is not shown. */
static void reference_links(struct note_pane *_Nonnull pane)
{
    const char content[] = "前[資料][doc]と[ref][]。\n\n[doc]: https://example.com/doc\n"
                           "[REF]: <https://example.com/ref> \"title\"\n\n後";
    static const char16_t shown[] = u"前資料とref。\r後";
    load(pane, PANE_MODE_VIEW, content);
    HWND handle = note_pane_handle(pane);
    const char16_t *visible = nullptr;
    size_t length = 0;
    require(note_pane_text(pane, &visible, &length) == NOTE_PANE_TEXT_TAKEN, "search projection");
    if (length != sizeof shown / sizeof shown[0] - 1 ||
        memcmp(visible, shown, length * sizeof shown[0]) != 0)
    {
        printf("reference projection:");
        for (size_t index = 0; index < length; ++index)
            printf(" %04x", (unsigned)visible[index]);
        printf("\n");
    }
    require(length == sizeof shown / sizeof shown[0] - 1 &&
                memcmp(visible, shown, length * sizeof shown[0]) == 0,
            "definitions are not shown and the labels read as text");
    expect_link(pane, native_find(handle, L"資料") + 1, "HTTPS://example.com/doc");
    /* "ref" also occurs in the hidden instruction; the label's last letter sits before the
     * hidden boundary blank that precedes "。". */
    expect_link(pane, native_find(handle, L"。") - 2, "HTTPS://example.com/ref");
    expect_link(pane, native_find(handle, L"前"), nullptr);
    load(pane, PANE_MODE_EDIT, content);
    wchar_t actual[512];
    GetWindowTextW(handle, actual, 512);
    require(wcsstr(actual, L"[doc]: https://example.com/doc") != nullptr,
            "editing shows the definitions in the Markdown source");
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
    expect_copy_target(pane, nth_copy_label(handle, 0) + 1, "nenefolio-code:0");
    require(SendMessageW(handle, EM_EXLINEFROMCHAR, 0, nth_copy_label(handle, 0)) <
                SendMessageW(handle, EM_EXLINEFROMCHAR, 0, find.chrgText.cpMin),
            "the copy action stays in the header above the code when wrapped and enlarged");
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
    code_copy_action(pane);
    bare_links(pane);
    heading_inline_sizes(pane);
    edge_arrows(pane);
    reference_links(pane);
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
