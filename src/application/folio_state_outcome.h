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
    FOLIO_STATE_NAME_TAKEN, /* 移動先に同じ名前のノートがある。ファイルも索引も変えていない */
    /* 今回 md を移した・公開したが index.json を書けなかった。次回の起動で揃う */
    FOLIO_STATE_LEDGER_STALE,
    /* 前回書けなかった index.json の修復を先に試みたが失敗した。
     * 今回の意図は何も実行しておらず、ファイルも索引も表示も変えていない。 */
    FOLIO_STATE_LEDGER_UNSYNCED,
    /* 改名の記録は公開したが、履歴・md・index.json のどこかで止まっている（ADR 0022 の決定 7）。
     * 今回の意図は何も実行していない。同じ改名を再試行するまで他の操作へ進めない。 */
    FOLIO_STATE_RENAME_PENDING,
    /* data/ に書けないので改名できない。記録は公開しておらず、意図も残っていない（決定 3 の補正）
     */
    FOLIO_STATE_RENAME_UNLOCKED,
    /* ローカル NTFS でない・照会できない・シンボリックリンク／junction。
     * 何も動かしていない（決定 4 と 2026-09-17 の補正） */
    FOLIO_STATE_RENAME_UNSUPPORTED,
    /* 元の md や履歴の識別子を取れないので、再開できる記録を作れない（決定 4） */
    FOLIO_STATE_RENAME_IDENTITY_FAILED,
    /* 改名の記録を公開できなかった。data/ には何も残っていない（決定 5） */
    FOLIO_STATE_RENAME_JOURNAL_FAILED,
    /* 改名の記録が版 1 の形ではない。既定値へは落とさず、記録も消さない（決定 5） */
    FOLIO_STATE_RENAME_JOURNAL_BROKEN,
    /* 改名の記録と data/ の実体が合わない。意図は保持したまま、手当てするまで進めない（決定 5） */
    FOLIO_STATE_RENAME_HALTED,
    /* 検索の語が壊れている。語も絞り込みも前のまま（ADR 0023 の決定 3・ADR 0024 の決定 2） */
    FOLIO_STATE_SEARCH_MALFORMED,
    /* 絞り込み中に並び替え・開閉が来た。台帳もファイルも表示も変えていない（ADR 0024 の決定 4） */
    FOLIO_STATE_FILTERED,
    /* 右ペインが表示している平文を取り出せない。探していないし、表示も選択も変えていない */
    FOLIO_STATE_PANE_UNAVAILABLE,
    FOLIO_STATE_OUT_OF_MEMORY,
    FOLIO_STATE_NAME_REQUIRED,
    FOLIO_STATE_INVALID_NAME,
    FOLIO_STATE_ALREADY_NAMED,
    FOLIO_STATE_CANCELLED
};

#endif
