/* note_pane の生成結果（C-005）。 */
#ifndef NENEFOLIO_NOTE_PANE_OUTCOME_H
#define NENEFOLIO_NOTE_PANE_OUTCOME_H

enum note_pane_outcome : unsigned char
{
    NOTE_PANE_CREATED,
    NOTE_PANE_NOT_CREATED, /* Msftedit.dll が読めない、または CreateWindowExW が失敗した */
    NOTE_PANE_OUT_OF_MEMORY
};

#endif
