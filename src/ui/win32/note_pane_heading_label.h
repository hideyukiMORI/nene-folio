#ifndef NENEFOLIO_NOTE_PANE_HEADING_LABEL_H
#define NENEFOLIO_NOTE_PANE_HEADING_LABEL_H
#include <uchar.h>
/* #975 / ADR0114: a heading's text as it is shown, for a row of the headings column: one line,
 * terminated, at most 127 units. Longer headings are cut; the row draws the ellipsis. */
constexpr int note_pane_heading_label_capacity = 128;
struct note_pane_heading_label
{
    char16_t units[note_pane_heading_label_capacity];
};
#endif
