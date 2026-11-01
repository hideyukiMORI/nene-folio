#include "note_minimap.h"
#include <limits.h>
#include <richedit.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t sample_capacity = 256;
constexpr size_t sample_work = 16;
constexpr int sample_columns = 80;

struct note_minimap
{
    int widths[sample_capacity];
    int scratch[sample_capacity];
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

static bool sample_line(struct note_minimap *_Nonnull map, HWND pane)
{
    size_t line = map->count <= 1 ? 0 : map->cursor * (size_t)(map->lines - 1) / (map->count - 1);
    LRESULT at = SendMessageW(pane, EM_LINEINDEX, line, 0);
    if (at < 0)
        return false;
    LRESULT length = SendMessageW(pane, EM_LINELENGTH, (WPARAM)at, 0);
    if (length < 0)
        return false;
    map->scratch[map->cursor++] = length > sample_columns ? sample_columns : (int)length;
    return true;
}

bool note_minimap_step(struct note_minimap *_Nonnull map, HWND pane)
{
    if (!map->pending)
        return false;
    if (map->count == 0 && !begin_samples(map, pane))
    {
        note_minimap_clear(map);
        return true;
    }
    for (size_t work = 0; work < sample_work && map->cursor < map->count; ++work)
    {
        if (!sample_line(map, pane))
        {
            note_minimap_clear(map);
            return true;
        }
    }
    if (map->cursor != map->count)
        return false;
    memcpy(map->widths, map->scratch, map->count * sizeof map->widths[0]);
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

static void draw_lines(const struct note_minimap *_Nonnull map, HDC device,
                       const struct folio_palette *_Nonnull palette,
                       const struct note_minimap_paint *_Nonnull paint)
{
    RECT track = note_minimap_track(paint);
    RECT thumb = note_minimap_thumb(paint);
    int height = track.bottom - track.top;
    int pad = MulDiv(4, (int)paint->dpi, 96);
    int width = track.right - track.left - pad * 2;
    for (size_t at = 0; at < map->shown && height > 0 && width > 0; ++at)
    {
        if (map->widths[at] == 0)
            continue;
        int y = track.top + (int)((LONGLONG)at * height / (LONGLONG)map->shown);
        int length = 1 + (width - 1) * map->widths[at] / sample_columns;
        RECT line = {track.left + pad, y + 1, track.left + pad + length, y + 2};
        if (line.bottom > track.bottom)
            line.bottom = track.bottom;
        fill(device, line,
             y >= thumb.top && y < thumb.bottom ? palette->selected_text : palette->gutter_text);
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
