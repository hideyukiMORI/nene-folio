#ifndef NENEFOLIO_NOTE_PANE_SAMPLE_H
#define NENEFOLIO_NOTE_PANE_SAMPLE_H
#include <tom.h>
#include <windows.h>

/* Transient native appearance, never text or a retained COM object. */
struct note_pane_sample
{
    RECT bounds;
    int columns;
    float points;
    float indent;
    float space_before;
    COLORREF foreground;
    COLORREF background;
    int cells;
    bool bold;
    bool marker;
};

constexpr size_t note_pane_sample_work = 16;
constexpr int note_pane_sample_columns = 80;
[[nodiscard]] bool note_pane_sample_read(HWND _Nonnull handle, ITextRange *_Nonnull range, int line,
                                         struct note_pane_sample *_Nonnull out);
#endif
