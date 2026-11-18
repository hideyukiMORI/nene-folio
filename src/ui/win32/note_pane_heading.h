#ifndef NENEFOLIO_NOTE_PANE_HEADING_H
#define NENEFOLIO_NOTE_PANE_HEADING_H
#include <stddef.h>
#include <windows.h>
/* #975 / ADR0114: one heading of the viewed note: its level (1-6) and where its paragraph
 * starts in the native text. */
struct note_pane_heading
{
    size_t level;
    LONG position;
};
#endif
