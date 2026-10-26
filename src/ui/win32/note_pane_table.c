#include "note_pane_table.h"
#include <limits.h>
#include <richedit.h>

static enum note_pane_table_scan query_row(HWND _Nonnull handle, LONG start,
                                           TABLECELLPARMS *_Nonnull cells,
                                           TABLEROWPARMS *_Nonnull row)
{
    *row = (TABLEROWPARMS){
        .cbRow = sizeof *row, .cbCell = sizeof *cells, .cCell = 63, .cRow = 1, .cpStartRow = start};
    HRESULT result = (HRESULT)SendMessageW(handle, EM_GETTABLEPARMS, (WPARAM)row, (LPARAM)cells);
    if (result == E_INVALIDARG)
        return NOTE_PANE_TABLE_NONE; /* Literal Unicode markers are not native rows. */
    if (result != S_OK)
        return NOTE_PANE_TABLE_FAILED;
    return row->bTableLevel != 0 && row->cCell > 0 && row->cCell <= 63 ? NOTE_PANE_TABLE_PRESENT
                                                                       : NOTE_PANE_TABLE_NONE;
}

enum note_pane_table_scan note_pane_table_scan(HWND _Nonnull handle, const char16_t *_Nonnull units,
                                               size_t length)
{
    for (size_t index = 0; index < length; ++index)
    {
        if (units[index] != 0xFFF9 || index > LONG_MAX)
            continue;
        TABLECELLPARMS cells[63] = {0};
        TABLEROWPARMS row = {0};
        enum note_pane_table_scan scanned = query_row(handle, (LONG)index, cells, &row);
        if (scanned != NOTE_PANE_TABLE_NONE)
            return scanned;
    }
    return NOTE_PANE_TABLE_NONE;
}

static bool same_widths(const TABLECELLPARMS *_Nonnull expected,
                        const TABLECELLPARMS *_Nonnull actual, size_t count)
{
    for (size_t index = 0; index < count; ++index)
        if (actual[index].dxWidth != expected[index].dxWidth)
            return false;
    return true;
}

static bool layout_row(HWND _Nonnull handle, LONG start, LONG width)
{
    TABLEROWPARMS row = {0};
    TABLECELLPARMS cells[63] = {0};
    enum note_pane_table_scan scanned = query_row(handle, start, cells, &row);
    if (scanned == NOTE_PANE_TABLE_NONE)
        return true;
    if (scanned == NOTE_PANE_TABLE_FAILED)
        return false;
    size_t count = row.cCell;
    for (size_t index = 0; index < count; ++index)
        cells[index].dxWidth = width / (LONG)count + (index == count - 1 ? width % (LONG)count : 0);
    row.cRow = 1;
    row.iCell = 0;
    if ((HRESULT)SendMessageW(handle, EM_SETTABLEPARMS, (WPARAM)&row, (LPARAM)cells) != S_OK)
        return false;
    TABLECELLPARMS actual[63] = {0};
    if ((HRESULT)SendMessageW(handle, EM_GETTABLEPARMS, (WPARAM)&row, (LPARAM)actual) != S_OK)
        return false;
    return same_widths(cells, actual, count);
}

enum note_pane_load_outcome note_pane_table_layout(HWND _Nonnull handle,
                                                   const char16_t *_Nonnull units, size_t length,
                                                   LONG width)
{
    for (size_t index = 0; index < length; ++index)
    {
        if (units[index] != 0xFFF9)
            continue;
        if (index > LONG_MAX || !layout_row(handle, (LONG)index, width))
            return NOTE_PANE_LOAD_FAILED;
    }
    return NOTE_PANE_LOAD_READY;
}
