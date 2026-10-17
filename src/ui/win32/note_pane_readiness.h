/* 外部表示境界の完成状態。文書や編集modeの写しではない（ADR0054）。 */
#ifndef NENEFOLIO_NOTE_PANE_READINESS_H
#define NENEFOLIO_NOTE_PANE_READINESS_H

enum note_pane_readiness : unsigned char
{
    NOTE_PANE_READINESS_UNAVAILABLE,
    NOTE_PANE_READINESS_STREAMING,
    NOTE_PANE_READINESS_READY
};

#endif
