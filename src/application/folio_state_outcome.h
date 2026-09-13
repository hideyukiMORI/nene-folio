/* folio_state の生成と派生値の結果（C-005）。合成ルートはこれを起動できなかった理由 1 行に写す。 */
#ifndef NENEFOLIO_FOLIO_STATE_OUTCOME_H
#define NENEFOLIO_FOLIO_STATE_OUTCOME_H

enum folio_state_outcome : unsigned char
{
    FOLIO_STATE_READY,
    FOLIO_STATE_DATA_UNREADABLE,   /* data/ の走査や台帳の読み込みが失敗した */
    FOLIO_STATE_LEDGER_MALFORMED,  /* 台帳が版 1 の形ではない。既定値へは落とさない（FR-015） */
    FOLIO_STATE_STORE_FAILED,      /* 台帳を書き戻せなかった。表示上の状態は変えていない */
    FOLIO_STATE_NO_SUCH_CATEGORY,  /* 索引に無いカテゴリへの意図 */
    FOLIO_STATE_NO_SUCH_NOTE,      /* 索引に無いノートへの意図 */
    FOLIO_STATE_NOTE_UNREADABLE,   /* ノートが無い・読めない・UTF-8 ではない。表示は変えていない */
    FOLIO_STATE_NOTHING_SELECTED,  /* ノートを選ばずに編集へ入ろうとした */
    FOLIO_STATE_NOT_EDITING,       /* 閲覧中に保存の意図が来た */
    FOLIO_STATE_NOTE_MALFORMED,    /* 編集中の本文が UTF-16 として壊れている。保存していない */
    FOLIO_STATE_NOTE_STORE_FAILED, /* md を書き戻せなかった。モードも読んだ本文も変えていない */
    FOLIO_STATE_HISTORY_FAILED,    /* 履歴を書けなかったので md も書いていない（ADR 0012） */
    FOLIO_STATE_UNSAVED_CHANGES,   /* :q は未保存の変更を破棄せず、終了を拒否した（ADR 0016） */
    FOLIO_STATE_NAME_TAKEN,   /* 移動先に同じ名前のノートがある。ファイルも索引も変えていない */
    FOLIO_STATE_LEDGER_STALE, /* md は移したが index.json を書けなかった。次回の起動で揃う */
    FOLIO_STATE_OUT_OF_MEMORY,
    FOLIO_STATE_NAME_REQUIRED,
    FOLIO_STATE_INVALID_NAME,
    FOLIO_STATE_ALREADY_NAMED,
    FOLIO_STATE_CANCELLED
};

#endif
