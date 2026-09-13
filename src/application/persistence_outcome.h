/* 永続化ポートの結果（C-005 / ARC-010）。見つからない・読めない・形が違う、を区別する。 */
#ifndef NENEFOLIO_PERSISTENCE_OUTCOME_H
#define NENEFOLIO_PERSISTENCE_OUTCOME_H

enum persistence_outcome : unsigned char
{
    PERSISTENCE_LOADED,
    PERSISTENCE_ABSENT,     /* ファイルやディレクトリが無い。既定の空として扱ってよい */
    PERSISTENCE_UNREADABLE, /* あるのに読めない（権限・入出力・名前の変換） */
    PERSISTENCE_MALFORMED,  /* 読めたが版 1 の形ではない、または受け入れられない名前 */
    PERSISTENCE_STORED,     /* 書き戻した */
    PERSISTENCE_UNWRITABLE, /* 書き戻せない（権限・入出力・置き換えの失敗） */
    PERSISTENCE_OUT_OF_MEMORY,
    PERSISTENCE_NAME_TAKEN /* 新規公開先が既にある。既存ファイルは変更していない */
};

#endif
