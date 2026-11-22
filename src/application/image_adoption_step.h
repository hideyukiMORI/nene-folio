#ifndef NENEFOLIO_IMAGE_ADOPTION_STEP_H
#define NENEFOLIO_IMAGE_ADOPTION_STEP_H
/* #1062 / ADR0120: 取り込みを 1 回進めた結果。1 回に扱うファイルは 1 つまで。 */
enum image_adoption_step : unsigned char
{
    IMAGE_ADOPTION_IDLE,       /* 進めるものが無い（始めていない・もう終わっている） */
    IMAGE_ADOPTION_PROGRESSED, /* 1 つ扱った。まだ残りがある */
    IMAGE_ADOPTION_FINISHED    /* この回で終わった（最後の 1 つを扱った・途中で打ち切った） */
};
#endif
