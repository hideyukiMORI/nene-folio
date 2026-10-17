/* 再着色の失敗は本文の投入失敗と区別する。READYな本文を無効化しない（ADR0057）。 */
#ifndef NENEFOLIO_NOTE_PANE_RECOLOR_OUTCOME_H
#define NENEFOLIO_NOTE_PANE_RECOLOR_OUTCOME_H

enum note_pane_recolor_outcome : unsigned char
{
    NOTE_PANE_RECOLOR_READY,
    NOTE_PANE_RECOLOR_FAILED
};

#endif
