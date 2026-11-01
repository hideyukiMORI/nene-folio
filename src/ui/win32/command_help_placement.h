#ifndef NENEFOLIO_COMMAND_HELP_PLACEMENT_H
#define NENEFOLIO_COMMAND_HELP_PLACEMENT_H

#include <windows.h>

/* 行の原点からの計測済み矩形。描画とページ判定はこの値を読む。 */
struct command_help_placement
{
    RECT heading;
    RECT key;
    RECT description;
    int height;
};

#endif
