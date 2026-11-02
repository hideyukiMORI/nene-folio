#include "note_pane_sample.h"
#include "note_pane_geometry.h"
#include "note_pane_table.h"
#include <limits.h>
#include <richedit.h>

static bool font_style(ITextRange *_Nonnull range, struct note_pane_sample *_Nonnull out)
{
    ITextFont *_Nullable font = nullptr;
    if (FAILED(range->lpVtbl->GetFont(range, &font)) || font == nullptr)
        return false;
    LONG foreground = 0;
    LONG background = 0;
    LONG bold = tomFalse;
    bool ready = SUCCEEDED(font->lpVtbl->GetSize(font, &out->points)) &&
                 SUCCEEDED(font->lpVtbl->GetForeColor(font, &foreground)) &&
                 SUCCEEDED(font->lpVtbl->GetBackColor(font, &background)) &&
                 SUCCEEDED(font->lpVtbl->GetBold(font, &bold));
    font->lpVtbl->Release(font);
    out->foreground = (COLORREF)foreground;
    out->background = (COLORREF)background;
    out->bold = bold == tomTrue;
    return ready && out->points > 0 && out->points <= 1638;
}

static bool paragraph_style(ITextRange *_Nonnull range, struct note_pane_sample *_Nonnull out)
{
    ITextPara *_Nullable paragraph = nullptr;
    if (FAILED(range->lpVtbl->GetPara(range, &paragraph)) || paragraph == nullptr)
        return false;
    float first = 0;
    bool ready = SUCCEEDED(paragraph->lpVtbl->GetLeftIndent(paragraph, &out->indent)) &&
                 SUCCEEDED(paragraph->lpVtbl->GetFirstLineIndent(paragraph, &first)) &&
                 SUCCEEDED(paragraph->lpVtbl->GetSpaceBefore(paragraph, &out->space_before));
    paragraph->lpVtbl->Release(paragraph);
    if (!ready || !(out->indent >= 0 && out->indent <= 32767))
        return false;
    if (first >= 0 || out->cells > 0)
        return true;
    LONG start = 0;
    LONG paragraph_start = 0;
    LONG moved = 0;
    if (FAILED(range->lpVtbl->GetStart(range, &start)) ||
        FAILED(range->lpVtbl->StartOf(range, tomParagraph, tomMove, &moved)) ||
        FAILED(range->lpVtbl->GetStart(range, &paragraph_start)))
        return false;
    out->marker = start == paragraph_start;
    return true;
}

static bool native_cell_count(HWND handle, ITextRange *_Nonnull range, LONG at, int *_Nonnull cells)
{
    BSTR _Nullable unit = nullptr;
    HRESULT read = range->lpVtbl->GetText(range, &unit);
    if (FAILED(read))
        return false;
    bool table = unit != nullptr && SysStringLen(unit) == 1 && unit[0] == 0xFFF9;
    SysFreeString(unit);
    if (!table)
        return true;
    enum note_pane_table_scan scanned = note_pane_table_columns(handle, at, cells);
    switch (scanned)
    {
    case NOTE_PANE_TABLE_NONE:
    case NOTE_PANE_TABLE_PRESENT:
        return true;
    case NOTE_PANE_TABLE_FAILED:
        return false;
    }
}

bool note_pane_sample_read(HWND handle, ITextRange *_Nonnull range, int line,
                           struct note_pane_sample *_Nonnull out)
{
    if (line < 0)
        return false;
    LRESULT at = SendMessageW(handle, EM_LINEINDEX, (WPARAM)line, 0);
    if (at < 0 || at >= LONG_MAX)
        return false;
    LRESULT length = SendMessageW(handle, EM_LINELENGTH, (WPARAM)at, 0);
    if (length < 0 || FAILED(range->lpVtbl->SetRange(range, (LONG)at, (LONG)at + 1)))
        return false;
    *out = (struct note_pane_sample){
        .columns = length > note_pane_sample_columns ? note_pane_sample_columns : (int)length};
    return note_pane_geometry_bounds(handle, range, &out->bounds) && font_style(range, out) &&
           native_cell_count(handle, range, (LONG)at, &out->cells) && paragraph_style(range, out);
}
