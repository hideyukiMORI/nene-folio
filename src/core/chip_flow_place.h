/* 並べた札 1 つの場所（画素。行の左上が原点）。全メンバーが独立に妥当（C-003）。 */
#ifndef NENEFOLIO_CHIP_FLOW_PLACE_H
#define NENEFOLIO_CHIP_FLOW_PLACE_H

struct chip_flow_place
{
    int left;
    int top;
    int width; /* 行の幅より広い札は、行の幅に切ってある */
};

#endif
