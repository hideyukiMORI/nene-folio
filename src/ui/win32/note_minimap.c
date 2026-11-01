#include "note_minimap.h"
#include "note_pane.h"
#include <limits.h>
#include <richedit.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t sample_capacity = 256;

struct note_minimap
{
    struct note_pane_sample samples[sample_capacity];
    struct note_pane_sample scratch[sample_capacity];
    size_t shown;
    size_t count;
    size_t cursor;
    int lines;
    bool pending;
};

enum note_minimap_outcome note_minimap_create(struct note_minimap *_Nullable *_Nonnull out)
{
    struct note_minimap *_Nullable map = calloc(1, sizeof *map);
    if (map == nullptr)
        return NOTE_MINIMAP_OUT_OF_MEMORY;
    *out = map;
    return NOTE_MINIMAP_READY;
}

void note_minimap_destroy(struct note_minimap *_Nullable map)
{
    free(map);
}

void note_minimap_invalidate(struct note_minimap *_Nonnull map)
{
    map->pending = false;
    map->count = 0;
    map->cursor = 0;
}

void note_minimap_clear(struct note_minimap *_Nonnull map)
{
    note_minimap_invalidate(map);
    map->shown = 0;
}

void note_minimap_begin(struct note_minimap *_Nonnull map)
{
    note_minimap_invalidate(map);
    map->pending = true;
}

bool note_minimap_pending(const struct note_minimap *_Nonnull map)
{
    return map->pending;
}

static bool begin_samples(struct note_minimap *_Nonnull map, HWND pane)
{
    LRESULT lines = SendMessageW(pane, EM_GETLINECOUNT, 0, 0);
    if (lines < 1 || lines > INT_MAX)
        return false;
    map->lines = (int)lines;
    map->count = (size_t)lines < sample_capacity ? (size_t)lines : sample_capacity;
    return true;
}

static bool sample_batch(struct note_minimap *_Nonnull map, struct note_pane *_Nonnull pane)
{
    int lines[note_pane_sample_work] = {0};
    size_t count = map->count - map->cursor;
    count = count > note_pane_sample_work ? note_pane_sample_work : count;
    for (size_t at = 0; at < count; ++at)
        lines[at] = map->count <= 1
                        ? 0
                        : (int)((map->cursor + at) * (size_t)(map->lines - 1) / (map->count - 1));
    if (!note_pane_samples(pane, lines, count, &map->scratch[map->cursor]))
        return false;
    map->cursor += count;
    return true;
}

bool note_minimap_step(struct note_minimap *_Nonnull map, struct note_pane *_Nonnull pane)
{
    if (!map->pending)
        return false;
    HWND handle = note_pane_handle(pane);
    if (handle == nullptr || (map->count == 0 && !begin_samples(map, handle)))
    {
        note_minimap_clear(map);
        return true;
    }
    if (!sample_batch(map, pane))
    {
        note_minimap_clear(map);
        return true;
    }
    if (map->cursor != map->count)
        return false;
    memcpy(map->samples, map->scratch, map->count * sizeof map->samples[0]);
    map->shown = map->count;
    map->pending = false;
    return true;
}

bool note_minimap_viewport(HWND pane, struct note_minimap_view *_Nonnull out)
{
    LRESULT lines = SendMessageW(pane, EM_GETLINECOUNT, 0, 0);
    LRESULT first = SendMessageW(pane, EM_GETFIRSTVISIBLELINE, 0, 0);
    if (lines < 1 || lines > INT_MAX || first < 0 || first >= lines)
        return false;
    RECT rect = {0};
    SendMessageW(pane, EM_GETRECT, 0, (LPARAM)&rect);
    if (IsRectEmpty(&rect))
        return false;
    POINTL point = {.x = rect.right - 1, .y = rect.bottom - 1};
    LRESULT at = SendMessageW(pane, EM_CHARFROMPOS, 0, (LPARAM)&point);
    if (at < 0)
        return false;
    LRESULT last = SendMessageW(pane, EM_EXLINEFROMCHAR, 0, at);
    if (last < first || last >= lines)
        return false;
    *out = (struct note_minimap_view){
        .lines = (int)lines, .first = (int)first, .visible = (int)(last - first + 1)};
    return true;
}

int note_minimap_limit(struct note_minimap_view view)
{
    return view.lines > 0 ? view.lines - 1 : 0;
}

RECT note_minimap_track(const struct note_minimap_paint *_Nonnull paint)
{
    RECT track = paint->bounds;
    int pad = MulDiv(4, (int)paint->dpi, 96);
    InflateRect(&track, -pad, -pad);
    int maximum = track.bottom - track.top;
    int row = MulDiv(3, (int)paint->dpi, 96);
    LONGLONG wanted = (LONGLONG)paint->view.lines * row;
    int height = wanted < maximum ? (int)wanted : maximum;
    int minimum = MulDiv(18, (int)paint->dpi, 96);
    if (height < minimum)
        height = minimum < maximum ? minimum : maximum;
    track.bottom = track.top + height;
    return track;
}

RECT note_minimap_thumb(const struct note_minimap_paint *_Nonnull paint)
{
    RECT thumb = note_minimap_track(paint);
    int height = thumb.bottom - thumb.top;
    if (height <= 0 || paint->view.lines <= 0)
        return thumb;
    int length = (int)((LONGLONG)height * paint->view.visible / paint->view.lines);
    int minimum = MulDiv(18, (int)paint->dpi, 96);
    length = length < minimum ? minimum : length;
    length = length > height ? height : length;
    int position = (int)((LONGLONG)height * paint->view.first / paint->view.lines);
    int travel = height - length;
    thumb.top += position < travel ? position : travel;
    thumb.bottom = thumb.top + length;
    return thumb;
}

int note_minimap_drag(const struct note_minimap_paint *_Nonnull paint, int y, int offset)
{
    RECT track = note_minimap_track(paint);
    RECT thumb = note_minimap_thumb(paint);
    int travel = track.bottom - track.top - (thumb.bottom - thumb.top);
    int distance = y - offset - track.top;
    distance = distance < 0 ? 0 : distance;
    distance = distance > travel ? travel : distance;
    if (travel <= 0)
        return 0;
    if (distance == travel)
        return note_minimap_limit(paint->view);
    return (int)((LONGLONG)distance * paint->view.lines / (track.bottom - track.top));
}

int note_minimap_jump(const struct note_minimap_paint *_Nonnull paint, int y)
{
    RECT thumb = note_minimap_thumb(paint);
    return note_minimap_drag(paint, y, (thumb.bottom - thumb.top) / 2);
}

static void fill(HDC device, RECT rect, COLORREF color)
{
    if (IsRectEmpty(&rect))
        return;
    SetDCBrushColor(device, color);
    FillRect(device, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
}

static void frame(HDC device, RECT rect, COLORREF color)
{
    SetDCBrushColor(device, color);
    FrameRect(device, &rect, (HBRUSH)GetStockObject(DC_BRUSH));
}

static void draw_table(HDC device, RECT row, int cells, COLORREF color)
{
    frame(device, row, color);
    int width = row.right - row.left;
    /* Dense tables retain a grid without painting every column over the same pixel. */
    int visible = width / 4;
    cells = cells > visible ? visible : cells;
    for (int at = 1; at < cells; ++at)
    {
        int x = row.left + width * at / cells;
        fill(device, (RECT){x, row.top, x + 1, row.bottom}, color);
    }
}

static void draw_text(HDC device, const struct note_pane_sample *_Nonnull sample, RECT row,
                      COLORREF color)
{
    int thickness = sample->points >= 20 ? 3 : 1;
    if (sample->bold || sample->space_before > 0)
        thickness = thickness < 2 ? 2 : thickness;
    float extent = (float)sample->columns * sample->points / 11.0F;
    extent = extent > note_pane_sample_columns ? note_pane_sample_columns : extent;
    int length = 1 + (int)((float)(row.right - row.left - 1) * extent / note_pane_sample_columns);
    row.right = row.left + length;
    row.bottom = row.bottom > row.top + thickness ? row.top + thickness : row.bottom;
    fill(device, row, color);
    if (sample->marker)
        fill(device, (RECT){row.left - 3, row.top, row.left - 2, row.bottom}, color);
}

static void draw_sample(HDC device, const struct note_pane_sample *_Nonnull sample, RECT row,
                        COLORREF color)
{
    int width = row.right - row.left;
    int indent = (int)(sample->indent * (float)width / 440.0F);
    indent = indent > width * 3 / 4 ? width * 3 / 4 : indent;
    row.left += indent;
    if (sample->cells > 0)
    {
        draw_table(device, row, sample->cells, color);
        return;
    }
    if (sample->columns > 0)
    {
        draw_text(device, sample, row, color);
        return;
    }
    if (sample->points <= 2 && sample->background <= 0xFFFFFF)
    {
        row.bottom = row.top + 1;
        fill(device, row, color);
    }
}

static void draw_lines(const struct note_minimap *_Nonnull map, HDC device,
                       const struct folio_palette *_Nonnull palette,
                       const struct note_minimap_paint *_Nonnull paint)
{
    RECT track = note_minimap_track(paint);
    RECT thumb = note_minimap_thumb(paint);
    int height = track.bottom - track.top;
    int pad = MulDiv(4, (int)paint->dpi, 96);
    for (size_t at = 0; at < map->shown && height > 0; ++at)
    {
        int y = track.top + (int)((LONGLONG)at * height / (LONGLONG)map->shown);
        int bottom = track.top + (int)((LONGLONG)(at + 1) * height / (LONGLONG)map->shown);
        RECT row = {track.left + pad, y, track.right - pad, bottom};
        if (IsRectEmpty(&row))
            continue;
        const struct note_pane_sample *sample = &map->samples[at];
        COLORREF color =
            sample->foreground <= 0xFFFFFF && palette->contrast == FOLIO_CONTRAST_STANDARD
                ? sample->foreground
                : palette->gutter_text;
        if (y >= thumb.top && y < thumb.bottom)
            color = palette->selected_text;
        draw_sample(device, sample, row, color);
    }
}

void note_minimap_draw(const struct note_minimap *_Nonnull map, HDC device,
                       const struct folio_palette *_Nonnull palette,
                       const struct note_minimap_paint *_Nonnull paint)
{
    int saved = SaveDC(device);
    if (saved == 0)
        return;
    IntersectClipRect(device, paint->bounds.left, paint->bounds.top, paint->bounds.right,
                      paint->bounds.bottom);
    fill(device, paint->bounds, palette->pane);
    RECT thumb = note_minimap_thumb(paint);
    fill(device, thumb, palette->selected_background);
    draw_lines(map, device, palette, paint);
    frame(device, thumb, paint->active ? palette->focus_line : palette->hover_line);
    if (paint->focused)
    {
        RECT focus = paint->bounds;
        InflateRect(&focus, -1, -1);
        frame(device, focus, palette->focus_line);
    }
    RestoreDC(device, saved);
}
