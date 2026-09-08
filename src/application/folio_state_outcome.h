/* folio_state の生成と派生値の結果（C-005）。合成ルートはこれを起動できなかった理由 1 行に写す。 */
#ifndef NENEFOLIO_FOLIO_STATE_OUTCOME_H
#define NENEFOLIO_FOLIO_STATE_OUTCOME_H

enum folio_state_outcome : unsigned char
{
    FOLIO_STATE_READY,
    FOLIO_STATE_DATA_UNREADABLE,  /* data/ の走査や台帳の読み込みが失敗した */
    FOLIO_STATE_LEDGER_MALFORMED, /* 台帳が版 1 の形ではない。既定値へは落とさない（FR-015） */
    FOLIO_STATE_STORE_FAILED,     /* 台帳を書き戻せなかった。表示上の状態は変えていない */
    FOLIO_STATE_NO_SUCH_CATEGORY, /* 索引に無いカテゴリへの意図 */
    FOLIO_STATE_NO_SUCH_NOTE,     /* 索引に無いノートへの意図 */
    FOLIO_STATE_NOTE_UNREADABLE,  /* ノートが無い・読めない・UTF-8 ではない。表示は変えていない */
    FOLIO_STATE_OUT_OF_MEMORY
};

#endif
