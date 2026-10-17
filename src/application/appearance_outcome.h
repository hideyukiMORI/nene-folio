/* 取得失敗はHC_OFFではない。失敗時は出力を変更しない（ADR0057）。 */
#ifndef NENEFOLIO_APPEARANCE_OUTCOME_H
#define NENEFOLIO_APPEARANCE_OUTCOME_H

enum appearance_outcome : unsigned char
{
    APPEARANCE_READY,
    APPEARANCE_UNAVAILABLE
};

#endif
