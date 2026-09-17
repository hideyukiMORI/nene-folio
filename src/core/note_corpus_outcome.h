/* 本文の写しの置き場の結果（C-005 / ADR 0024 の決定 1）。 */
#ifndef NENEFOLIO_NOTE_CORPUS_OUTCOME_H
#define NENEFOLIO_NOTE_CORPUS_OUTCOME_H

enum note_corpus_outcome : unsigned char
{
    NOTE_CORPUS_ACCEPTED,
    NOTE_CORPUS_OUT_OF_MEMORY
};

#endif
