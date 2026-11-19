#include "note_outline.h"
#include "note_pane.h"
#include <stdlib.h>

struct note_outline
{
    size_t first;       /* the first row shown in the column */
    size_t cursor;      /* the keyboard's row */
    size_t hover;       /* the row under the pointer, or none */
    size_t pressed;     /* the row being pressed, or none */
    size_t current;     /* the heading being read, as last observed, or none */
    size_t rows;        /* how many rows the column showed when last fitted (#1018) */
    size_t held_first;  /* the column's place before the same note is poured again (#1026) */
    size_t held_cursor; /* and the keyboard's row */
    uint64_t revision;
};

/* Logical pixels at 96 DPI. */
constexpr int base_title_height = 28;
constexpr int base_row_height = 24;
constexpr int base_text_left = 12;
constexpr int base_text_right = 8;
constexpr int base_level_step = 12;
constexpr int base_mark_width = 3;

static int scaled(int value, UINT dpi)
{
    return MulDiv(value, (int)dpi, 96);
}

enum note_outline_outcome note_outline_create(struct note_outline *_Nullable *_Nonnull out)
{
    struct note_outline *_Nullable outline = calloc(1, sizeof *outline);
    if (outline == nullptr)
        return NOTE_OUTLINE_OUT_OF_MEMORY;
    outline->hover = outline->pressed = outline->current = note_outline_none;
    outline->revision = 1;
    *out = outline;
    return NOTE_OUTLINE_READY;
}

void note_outline_destroy(struct note_outline *_Nullable outline)
{
    free(outline);
}

void note_outline_reset(struct note_outline *_Nonnull outline)
{
    outline->first = outline->cursor = 0;
    outline->hover = outline->pressed = outline->current = note_outline_none;
    ++outline->revision;
}

uint64_t note_outline_revision(const struct note_outline *_Nonnull outline)
{
    return outline->revision;
}

size_t note_outline_current(const struct note_outline *_Nonnull outline)
{
    return outline->current;
}

size_t note_outline_rows(const struct note_outline_paint *_Nonnull paint)
{
    int space = paint->bounds.bottom - paint->bounds.top - scaled(base_title_height, paint->dpi);
    int height = scaled(base_row_height, paint->dpi);
    return space <= 0 || height <= 0 ? 0 : (size_t)(space / height);
}

/* The last value `first` may take so that the rows end at the column's end. */
static size_t last_first(const struct note_outline_paint *_Nonnull paint, size_t count)
{
    size_t rows = note_outline_rows(paint);
    return count > rows ? count - rows : 0;
}

/* Scrolls the column the least that shows the row. True when `first` changed. */
static bool reveal(struct note_outline *_Nonnull outline,
                   const struct note_outline_paint *_Nonnull paint, size_t index)
{
    size_t rows = note_outline_rows(paint);
    size_t first = outline->first;
    if (index < first)
        first = index;
    else if (rows != 0 && index >= first + rows)
        first = index - rows + 1;
    bool changed = first != outline->first;
    outline->first = first;
    return changed;
}

bool note_outline_track(struct note_outline *_Nonnull outline, struct note_pane *_Nonnull pane,
                        const struct note_outline_paint *_Nonnull paint)
{
    size_t current = note_outline_none;
    if (!note_pane_heading_current(pane, &current))
        current = note_outline_none;
    if (current == outline->current)
        return false;
    outline->current = current;
    if (current != note_outline_none)
        (void)reveal(outline, paint, current);
    return true;
}

RECT note_outline_row_rect(const struct note_outline *_Nonnull outline,
                           const struct note_outline_paint *_Nonnull paint, size_t index)
{
    if (index < outline->first || index - outline->first >= note_outline_rows(paint))
        return (RECT){0};
    int height = scaled(base_row_height, paint->dpi);
    int top = paint->bounds.top + scaled(base_title_height, paint->dpi) +
              (int)(index - outline->first) * height;
    return (RECT){paint->bounds.left + 1, top, paint->bounds.right, top + height};
}

size_t note_outline_row_at(const struct note_outline *_Nonnull outline,
                           const struct note_outline_paint *_Nonnull paint, POINT point,
                           size_t count)
{
    int top = paint->bounds.top + scaled(base_title_height, paint->dpi);
    int height = scaled(base_row_height, paint->dpi);
    if (!PtInRect(&paint->bounds, point) || point.y < top || height <= 0)
        return note_outline_none;
    size_t row = (size_t)((point.y - top) / height);
    if (row >= note_outline_rows(paint) || outline->first + row >= count)
        return note_outline_none;
    return outline->first + row;
}

bool note_outline_scroll(struct note_outline *_Nonnull outline,
                         const struct note_outline_paint *_Nonnull paint, int rows, size_t count)
{
    size_t limit = last_first(paint, count);
    size_t first = outline->first > limit ? limit : outline->first;
    size_t distance = (size_t)(rows < 0 ? -(long long)rows : rows);
    if (rows < 0)
        first = distance > first ? 0 : first - distance;
    else
        first = distance > limit - first ? limit : first + distance;
    bool changed = first != outline->first;
    outline->first = first;
    return changed;
}

/* #1018: when the column shows a different number of rows (the window was resized), the column
 * keeps no empty rows below its last heading and keeps the heading being read in view. With
 * the same number of rows, a column the wheel moved stays where it is. */
bool note_outline_fit(struct note_outline *_Nonnull outline,
                      const struct note_outline_paint *_Nonnull paint, size_t count)
{
    size_t rows = note_outline_rows(paint);
    if (rows == outline->rows)
        return false;
    outline->rows = rows;
    size_t before = outline->first;
    size_t limit = last_first(paint, count);
    if (outline->first > limit)
        outline->first = limit;
    if (outline->current != note_outline_none && outline->current < count)
        (void)reveal(outline, paint, outline->current);
    return outline->first != before;
}

/* #1026: pouring the same note again (new colours, a new face, a moved image folder) resets
 * the column like any load; hold before it and release after the body is back at its line,
 * and the column and the keyboard's row are where they were. The rows are still new ones. */
void note_outline_hold(struct note_outline *_Nonnull outline)
{
    outline->held_first = outline->first;
    outline->held_cursor = outline->cursor;
}

bool note_outline_release(struct note_outline *_Nonnull outline,
                          const struct note_outline_paint *_Nonnull paint, size_t count)
{
    size_t limit = last_first(paint, count);
    size_t first = outline->held_first > limit ? limit : outline->held_first;
    bool changed = first != outline->first;
    outline->first = first;
    outline->cursor = count == 0 || outline->held_cursor < count ? outline->held_cursor : count - 1;
    return changed;
}

size_t note_outline_cursor(const struct note_outline *_Nonnull outline)
{
    return outline->cursor;
}

void note_outline_set_cursor(struct note_outline *_Nonnull outline,
                             const struct note_outline_paint *_Nonnull paint, size_t index,
                             size_t count)
{
    if (count == 0)
        return;
    outline->cursor = index < count ? index : count - 1;
    bool moved = reveal(outline, paint, outline->cursor);
    (void)moved;
}

bool note_outline_set_hover(struct note_outline *_Nonnull outline, size_t index)
{
    bool changed = outline->hover != index;
    outline->hover = index;
    return changed;
}

bool note_outline_set_pressed(struct note_outline *_Nonnull outline, size_t index)
{
    bool changed = outline->pressed != index;
    outline->pressed = index;
    return changed;
}

size_t note_outline_pressed(const struct note_outline *_Nonnull outline)
{
    return outline->pressed;
}

static void fill(HDC _Nonnull device, RECT rect, COLORREF color)
{
    if (IsRectEmpty(&rect))
        return;
    SetDCBrushColor(device, color);
    FillRect(device, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
}

static void frame(HDC _Nonnull device, RECT rect, COLORREF color)
{
    SetDCBrushColor(device, color);
    FrameRect(device, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
}

static void text(HDC _Nonnull device, const char16_t *_Nonnull units, RECT bounds, COLORREF color)
{
    SetTextColor(device, color);
    DrawTextW(device, (const wchar_t *)units, -1, &bounds,
              DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_END_ELLIPSIS);
}

/* The column's own surface: the pane's ground, a hairline on its body side, and its name. */
static void draw_surface(HDC _Nonnull device, const struct note_outline_paint *_Nonnull paint)
{
    fill(device, paint->bounds, paint->palette->pane);
    RECT line = paint->bounds;
    line.right = line.left + 1;
    fill(device, line, paint->palette->border);
    RECT title = paint->bounds;
    title.left += scaled(base_text_left, paint->dpi);
    title.right -= scaled(base_text_right, paint->dpi);
    title.bottom = title.top + scaled(base_title_height, paint->dpi);
    text(device, paint->title, title, paint->palette->header_text);
}

/* A note without headings says so where the first row would be. */
static void draw_empty(HDC _Nonnull device, const struct note_outline_paint *_Nonnull paint)
{
    RECT row = paint->bounds;
    row.left += scaled(base_text_left, paint->dpi);
    row.right -= scaled(base_text_right, paint->dpi);
    row.top += scaled(base_title_height, paint->dpi);
    row.bottom = row.top + scaled(base_row_height, paint->dpi);
    if (row.bottom <= paint->bounds.bottom)
        text(device, paint->empty, row, paint->palette->header_text);
}

/* The row's ground and edge: the heading being read is the selected surface with a bar on the
 * body side; the pressed row has the same surface; hover and keyboard focus are lines. */
static void draw_row_state(const struct note_outline *_Nonnull outline, HDC _Nonnull device,
                           const struct note_outline_paint *_Nonnull paint, size_t index)
{
    RECT row = note_outline_row_rect(outline, paint, index);
    if (index == outline->current || index == outline->pressed)
        fill(device, row, paint->palette->selected_background);
    if (index == outline->current)
    {
        RECT mark = row;
        mark.right = mark.left + scaled(base_mark_width, paint->dpi);
        fill(device, mark, paint->palette->current_text);
    }
    if (paint->focused && index == outline->cursor)
        frame(device, row,
              index == outline->current ? paint->palette->focus_selected_line
                                        : paint->palette->focus_item_line);
    else if (index == outline->hover)
        frame(device, row, paint->palette->hover_line);
}

static void draw_row(const struct note_outline *_Nonnull outline, struct note_pane *_Nonnull pane,
                     HDC _Nonnull device, const struct note_outline_paint *_Nonnull paint)
{
    for (size_t index = outline->first; index < note_pane_heading_count(pane); ++index)
    {
        RECT row = note_outline_row_rect(outline, paint, index);
        struct note_pane_heading heading = {0};
        struct note_pane_heading_label label;
        if (IsRectEmpty(&row) || !note_pane_heading(pane, index, &heading))
            return;
        size_t length = note_pane_heading_text(pane, index, &label);
        (void)length;
        draw_row_state(outline, device, paint, index);
        row.left += scaled(base_text_left + (int)(heading.level - 1) * base_level_step, paint->dpi);
        row.right -= scaled(base_text_right, paint->dpi);
        bool selected = index == outline->current || index == outline->pressed;
        text(device, label.units, row,
             selected ? paint->palette->selected_text : paint->palette->note_text);
    }
}

void note_outline_draw(const struct note_outline *_Nonnull outline, struct note_pane *_Nonnull pane,
                       HDC _Nonnull device, const struct note_outline_paint *_Nonnull paint)
{
    int saved = SaveDC(device);
    if (saved == 0)
        return;
    IntersectClipRect(device, paint->bounds.left, paint->bounds.top, paint->bounds.right,
                      paint->bounds.bottom);
    SetBkMode(device, TRANSPARENT);
    if (paint->font != nullptr)
        SelectObject(device, paint->font);
    draw_surface(device, paint);
    if (note_pane_heading_count(pane) == 0)
        draw_empty(device, paint);
    else
        draw_row(outline, pane, device, paint);
    if (paint->focused && note_pane_heading_count(pane) == 0)
    {
        RECT focus = paint->bounds;
        InflateRect(&focus, -1, -1);
        frame(device, focus, paint->palette->focus_line);
    }
    RestoreDC(device, saved);
}
