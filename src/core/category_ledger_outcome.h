/* category_ledger の生成・解釈・書き出しの結果（C-005 / ARC-009）。 */
#ifndef NENEFOLIO_CATEGORY_LEDGER_OUTCOME_H
#define NENEFOLIO_CATEGORY_LEDGER_OUTCOME_H

enum category_ledger_outcome : unsigned char
{
    CATEGORY_LEDGER_ACCEPTED,
    CATEGORY_LEDGER_MALFORMED,           /* JSON として、または版 1 の形として読めない */
    CATEGORY_LEDGER_UNSUPPORTED_VERSION, /* version が 1 ではない */
    CATEGORY_LEDGER_OUT_OF_MEMORY
};

#endif
