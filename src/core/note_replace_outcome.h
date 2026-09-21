/* note_replace の結果（C-005 / ADR 0028 の決定 4(c) / 6）。 */
#ifndef NENEFOLIO_NOTE_REPLACE_OUTCOME_H
#define NENEFOLIO_NOTE_REPLACE_OUTCOME_H

enum note_replace_outcome : unsigned char
{
    NOTE_REPLACE_READY,
    /* 選び方に合う一致が 1 つも無い。本文は変えない */
    NOTE_REPLACE_NOT_FOUND,
    /* anchor の start が end より後ろ。公開契約の違反なので黙って直さず拒む */
    NOTE_REPLACE_BAD_SPAN,
    /* 組み立てた本文が note_replace_limit を超える。確保の前に断るので何も変えない */
    NOTE_REPLACE_TOO_LARGE,
    NOTE_REPLACE_OUT_OF_MEMORY
};

#endif
