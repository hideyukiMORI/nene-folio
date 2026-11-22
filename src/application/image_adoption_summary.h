#ifndef NENEFOLIO_IMAGE_ADOPTION_SUMMARY_H
#define NENEFOLIO_IMAGE_ADOPTION_SUMMARY_H
#include <stddef.h>
/* #1062 / ADR0120: 取り込みの数。dropped = beyond + （扱う分）。扱う分のうち、扱い終えたものは
 * taken + not_image + too_large + unreadable + not_stored + out_of_memory で、残りが pending。 */
struct image_adoption_summary
{
    size_t dropped;       /* 落とされたファイルの数 */
    size_t beyond;        /* 1 回の上限（image_adoption_limit）を超えていて、扱わない数 */
    size_t pending;       /* まだ扱っていない数。打ち切った後は、扱わずに終えた数 */
    size_t taken;         /* 取り込んだ（写した・同じ中身がもうあった）。Markdown を作った数 */
    size_t not_image;     /* PNG でも JPEG でもない */
    size_t too_large;     /* 1 枚の上限を超える */
    size_t unreadable;    /* 開けない・読み切れない */
    size_t not_stored;    /* images/ へ書けない */
    size_t out_of_memory; /* 記憶が足りない */
    bool finished;        /* もう進めるものが無い */
};
#endif
