#include "note_pane_headings.h"
#include <stdint.h>
#include <stdlib.h>

struct note_pane_headings
{
    struct note_pane_heading *_Nullable items;
    size_t count;
    size_t capacity;
};

/* RichEdit's built-in paragraph styles: Heading 1 is -2, Heading 6 is -7 (tomStyleHeading1...). */
constexpr LONG style_heading_first = -2;
constexpr LONG style_heading_last = -7;

static bool append(struct note_pane_headings *_Nonnull headings, struct note_pane_heading heading)
{
    if (headings->count == headings->capacity)
    {
        size_t capacity = headings->capacity == 0 ? 16 : headings->capacity * 2;
        if (capacity < headings->capacity || capacity > SIZE_MAX / sizeof *headings->items)
            return false;
        struct note_pane_heading *_Nullable prepared =
            realloc(headings->items, capacity * sizeof *prepared);
        if (prepared == nullptr)
            return false;
        headings->items = prepared;
        headings->capacity = capacity;
    }
    headings->items[headings->count++] = heading;
    return true;
}

/* The style of the paragraph the collapsed range is in, or 0 when it cannot be read. */
static LONG paragraph_style(ITextRange *_Nonnull range)
{
    ITextPara *_Nullable para = nullptr;
    if (FAILED(range->lpVtbl->GetPara(range, &para)) || para == nullptr)
        return 0;
    LONG style = 0;
    HRESULT read = para->lpVtbl->GetStyle(para, &style);
    para->lpVtbl->Release(para);
    return SUCCEEDED(read) ? style : 0;
}

/* One paragraph: keep it when its style is a heading's. */
static bool collect(struct note_pane_headings *_Nonnull headings, ITextRange *_Nonnull range)
{
    LONG style = paragraph_style(range);
    if (style > style_heading_first || style < style_heading_last)
        return true;
    LONG start = 0;
    if (FAILED(range->lpVtbl->GetStart(range, &start)))
        return true;
    return append(headings,
                  (struct note_pane_heading){.level = (size_t)(style_heading_first - style + 1),
                                             .position = start});
}

/* The walk's last stop is the very end of the document, on its closing paragraph mark, which
 * has the format of the paragraph before it: after a heading that ends the note it would read
 * as a second, empty heading. It is not a paragraph of the note. */
static bool final_mark(ITextRange *_Nonnull range, LONG length)
{
    LONG start = 0;
    return SUCCEEDED(range->lpVtbl->GetStart(range, &start)) && start + 1 >= length;
}

static enum note_pane_link_outcome walk(struct note_pane_headings *_Nonnull headings,
                                        ITextRange *_Nonnull range)
{
    LONG length = 0;
    if (FAILED(range->lpVtbl->GetStoryLength(range, &length)))
        return NOTE_PANE_LINK_UNAVAILABLE;
    LONG moved = 1;
    while (moved == 1)
    {
        if (!final_mark(range, length) && !collect(headings, range))
            return NOTE_PANE_LINK_OUT_OF_MEMORY;
        moved = 0;
        if (FAILED(range->lpVtbl->Move(range, tomParagraph, 1, &moved)))
            return NOTE_PANE_LINK_UNAVAILABLE;
    }
    return NOTE_PANE_LINK_READY;
}

enum note_pane_link_outcome
note_pane_headings_create(ITextDocument *_Nonnull document,
                          struct note_pane_headings *_Nullable *_Nonnull out)
{
    struct note_pane_headings *_Nullable headings = calloc(1, sizeof *headings);
    if (headings == nullptr)
        return NOTE_PANE_LINK_OUT_OF_MEMORY;
    ITextRange *_Nullable range = nullptr;
    enum note_pane_link_outcome outcome = NOTE_PANE_LINK_UNAVAILABLE;
    if (SUCCEEDED(document->lpVtbl->Range(document, 0, 0, &range)) && range != nullptr)
    {
        outcome = walk(headings, range);
        range->lpVtbl->Release(range);
    }
    if (outcome != NOTE_PANE_LINK_READY)
    {
        note_pane_headings_destroy(headings);
        return outcome;
    }
    *out = headings;
    return NOTE_PANE_LINK_READY;
}

void note_pane_headings_destroy(struct note_pane_headings *_Nullable headings)
{
    if (headings != nullptr)
        free(headings->items);
    free(headings);
}

size_t note_pane_headings_count(const struct note_pane_headings *_Nullable headings)
{
    return headings == nullptr ? 0 : headings->count;
}

struct note_pane_heading note_pane_headings_at(const struct note_pane_headings *_Nonnull headings,
                                               size_t index)
{
    return headings->items[index];
}
