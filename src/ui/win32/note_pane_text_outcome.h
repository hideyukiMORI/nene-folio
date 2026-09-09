/* 編集中の本文を取り出した結果（C-005）。 */
#ifndef NENEFOLIO_NOTE_PANE_TEXT_OUTCOME_H
#define NENEFOLIO_NOTE_PANE_TEXT_OUTCOME_H

enum note_pane_text_outcome : unsigned char
{
    NOTE_PANE_TEXT_TAKEN,
    NOTE_PANE_TEXT_UNAVAILABLE, /* 控えのウィンドウが既に無い */
    NOTE_PANE_TEXT_OUT_OF_MEMORY
};

#endif
