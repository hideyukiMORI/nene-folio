/* RichEdit の本文を保存時と同じ形へ正規化したあとの、保存済み本文との差（ADR 0016）。 */
#ifndef NENEFOLIO_FOLIO_NOTE_CHANGE_H
#define NENEFOLIO_FOLIO_NOTE_CHANGE_H

enum folio_note_change : unsigned char
{
    FOLIO_NOTE_SAME,
    FOLIO_NOTE_CHANGED
};

#endif
