/* OS終了通知の同意と、同期保存中の取消をUIだけで管理する（ADR0053）。 */
#ifndef NENEFOLIO_SESSION_PHASE_H
#define NENEFOLIO_SESSION_PHASE_H

enum session_phase : unsigned char
{
    SESSION_PHASE_IDLE,
    SESSION_PHASE_CHECKING,
    SESSION_PHASE_CANCELLED,
    SESSION_PHASE_APPROVED
};

#endif
