/* 事前確認は移動の成功ではない（ADR 0040 の 2026-10-08 の補正 7）。 */
#ifndef NENEFOLIO_RECYCLE_READINESS_H
#define NENEFOLIO_RECYCLE_READINESS_H

enum recycle_readiness : unsigned char
{
    RECYCLE_READY,
    RECYCLE_ABSENT,
    RECYCLE_UNAVAILABLE,
    RECYCLE_BUSY,
    RECYCLE_FAILED
};

#endif
