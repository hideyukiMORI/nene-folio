/* note_ledger の生成・解釈・書き出しの結果（C-005 / ARC-009）。 */
#ifndef NENEFOLIO_NOTE_LEDGER_OUTCOME_H
#define NENEFOLIO_NOTE_LEDGER_OUTCOME_H

enum note_ledger_outcome : unsigned char
{
    NOTE_LEDGER_ACCEPTED,
    NOTE_LEDGER_MALFORMED,           /* JSON として、または版 1 の形として読めない */
    NOTE_LEDGER_UNSUPPORTED_VERSION, /* version が 1 ではない */
    NOTE_LEDGER_OUT_OF_MEMORY
};

#endif
