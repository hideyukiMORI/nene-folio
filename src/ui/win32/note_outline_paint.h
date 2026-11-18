#ifndef NENEFOLIO_NOTE_OUTLINE_PAINT_H
#define NENEFOLIO_NOTE_OUTLINE_PAINT_H
#include "folio_palette.h"
#include <uchar.h>
#include <windows.h>
/* #975 / ADR0114: where and how the headings column is shown. One value serves drawing,
 * hit-testing and accessibility, so the three agree on every row's rectangle. */
struct note_outline_paint
{
    RECT bounds; /* the column in the owner's client coordinates */
    UINT dpi;
    HFONT _Nullable font;                         /* the rows' font; the device's own when absent */
    const struct folio_palette *_Nonnull palette; /* the look in use */
    const char16_t *_Nonnull title;               /* the column's name, terminated */
    const char16_t *_Nonnull empty;               /* what a note without headings shows */
    bool focused;                                 /* the keyboard is in the column */
};
#endif
