/* ドロワーの 1 行の表示値。UI はこれを写すだけで判断しない（ARC-011）。
 * 座標は画素で、ドロワーの左上を原点とする。text は drawer_layout が所有し、layout と同じ寿命。 */
#ifndef NENEFOLIO_DRAWER_ROW_H
#define NENEFOLIO_DRAWER_ROW_H

#include "drawer_row_kind.h"
#include "rgb_color.h"

struct drawer_row
{
    enum drawer_row_kind kind;
    int top;    /* 行の上端の y 座標 */
    int height; /* 行の高さ */
    int indent; /* 文字の左端の x 座標 */
    const char *_Nonnull text;
    struct rgb_color color; /* 行が属するカテゴリの色 */
};

#endif
