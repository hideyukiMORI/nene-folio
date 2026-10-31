#ifndef NENEFOLIO_DRAWER_WIDTH_RANGE_H
#define NENEFOLIO_DRAWER_WIDTH_RANGE_H

/* 一回のclient/DPIから導くlogical pxの範囲と実幅。選択幅はfolio_windowだけが持つ。 */
struct drawer_width_range
{
    int minimum;
    int maximum;
    int value;
};

#endif
