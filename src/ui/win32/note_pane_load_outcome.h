/* RTF投入の結果。正常な空本文もREADYである（ADR0054）。 */
#ifndef NENEFOLIO_NOTE_PANE_LOAD_OUTCOME_H
#define NENEFOLIO_NOTE_PANE_LOAD_OUTCOME_H

enum note_pane_load_outcome : unsigned char
{
    NOTE_PANE_LOAD_READY,
    NOTE_PANE_LOAD_UNAVAILABLE,
    NOTE_PANE_LOAD_FAILED,
    NOTE_PANE_LOAD_BUSY
};

#endif
