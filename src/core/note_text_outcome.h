/* note_text の生成結果（C-005）。 */
#ifndef NENEFOLIO_NOTE_TEXT_OUTCOME_H
#define NENEFOLIO_NOTE_TEXT_OUTCOME_H

enum note_text_outcome : unsigned char
{
    NOTE_TEXT_ACCEPTED,
    NOTE_TEXT_INVALID_UTF8,
    NOTE_TEXT_OUT_OF_MEMORY
};

#endif
