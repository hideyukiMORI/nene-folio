/* 札を並べる行の入力（画素）。UI が字の幅を測って渡す値で、全メンバーが独立に妥当（C-003）。
 * 左の欄の絞り込みの札の行（ADR0122）が使う。 */
#ifndef NENEFOLIO_CHIP_FLOW_INPUT_H
#define NENEFOLIO_CHIP_FLOW_INPUT_H

#include <stddef.h>

struct chip_flow_input
{
    const int *_Nonnull widths; /* 並べる札の幅。0 番は必ず置く札、1 番からは畳める札 */
    size_t count;               /* widths の数 */
    int opener_width;           /* 最後に必ず置く「開く札」の幅 */
    int more_width;             /* 畳んだ札の代わりに置く「+N」の札の幅 */
    int available;              /* 行の幅 */
    int height;                 /* 札の高さ */
    int gap;                    /* 同じ行の札の間 */
    int row_gap;                /* 行の間 */
    int rows;                   /* 使ってよい行の数 */
};

#endif
