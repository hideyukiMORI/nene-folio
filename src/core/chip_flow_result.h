/* 札を並べた結果。全メンバーが独立に妥当（C-003）。 */
#ifndef NENEFOLIO_CHIP_FLOW_RESULT_H
#define NENEFOLIO_CHIP_FLOW_RESULT_H

#include "chip_flow_place.h"

#include <stddef.h>

struct chip_flow_result
{
    size_t shown;                  /* 先頭から置いた札の数（places のうち有効な数） */
    size_t hidden;                 /* 畳んだ札の数。0 なら「+N」の札は無い */
    int rows;                      /* 使った行の数 */
    struct chip_flow_place more;   /* 「+N」の札。hidden が 0 のときは使わない */
    struct chip_flow_place opener; /* 「開く札」 */
};

#endif
