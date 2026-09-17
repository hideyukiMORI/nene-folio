/* folio_settings の結果（C-005 / ARC-010 / ADR 0025 の決定 3）。
 * 「無い」は永続化ポートの PERSISTENCE_ABSENT が答えるので、ここには無い。 */
#ifndef NENEFOLIO_FOLIO_SETTINGS_OUTCOME_H
#define NENEFOLIO_FOLIO_SETTINGS_OUTCOME_H

enum folio_settings_outcome : unsigned char
{
    FOLIO_SETTINGS_READY,
    /* 版 1 の形ではない（未知・欠落・重複したキー、型の違う値）。既定値へは落とさない */
    FOLIO_SETTINGS_MALFORMED,
    /* 版が 1 ではない。新しい版を古い exe が潰さないよう、上書きもしない（決定 2 / 6） */
    FOLIO_SETTINGS_UNSUPPORTED_VERSION,
    FOLIO_SETTINGS_OUT_OF_MEMORY
};

#endif
