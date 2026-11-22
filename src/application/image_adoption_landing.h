#ifndef NENEFOLIO_IMAGE_ADOPTION_LANDING_H
#define NENEFOLIO_IMAGE_ADOPTION_LANDING_H
/* #1062 / ADR0120: 取り込みの終わりに、ためた Markdown が本文へ入ったかどうか。窓が答え、
 * 1 行の文を選ぶ関数（image_adoption_notice）が読む。 */
enum image_adoption_landing : unsigned char
{
    IMAGE_ADOPTION_LANDED,    /* 入れた。入れるものが無かったときも */
    IMAGE_ADOPTION_DISPLACED, /* 落としたノートの本文ではなくなっていたので、入れなかった */
    IMAGE_ADOPTION_UNWRITTEN  /* 入れようとして、入らなかった（本文が入力を受けない・記憶不足） */
};
#endif
