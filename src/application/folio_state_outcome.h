/* folio_state の生成と派生値の結果（C-005）。合成ルートはこれを起動できなかった理由 1 行に写す。 */
#ifndef NENEFOLIO_FOLIO_STATE_OUTCOME_H
#define NENEFOLIO_FOLIO_STATE_OUTCOME_H

enum folio_state_outcome : unsigned char
{
    FOLIO_STATE_READY,
    FOLIO_STATE_DATA_UNREADABLE,  /* data/ の走査や台帳の読み込みが失敗した */
    FOLIO_STATE_LEDGER_MALFORMED, /* 台帳が版 1 の形ではない。既定値へは落とさない（FR-015） */
    FOLIO_STATE_OUT_OF_MEMORY
};

#endif
