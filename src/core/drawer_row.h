/* ドロワーの 1 行の表示値。UI はこれを写すだけで判断しない（ARC-011）。
 * 座標は画素で、ドロワーの左上を原点とする。text は drawer_layout が所有し、layout と同じ寿命。 */
#ifndef NENEFOLIO_DRAWER_ROW_H
#define NENEFOLIO_DRAWER_ROW_H

#include "drawer_row_kind.h"
#include "rgb_color.h"

#include <stddef.h>

struct drawer_row
{
    enum drawer_row_kind kind;
    int top;    /* 行の上端の y 座標 */
    int height; /* 行の高さ */
    int indent; /* 文字（カテゴリ行は番号）の左端の x 座標 */
    const char *_Nonnull text;
    struct rgb_color color; /* 行が属するカテゴリの色 */
    size_t category;        /* 行が属するカテゴリの番号（台帳の順） */
    size_t note;            /* ノート行のときの索引台帳の番号。カテゴリ行では 0 */
    size_t ordinal;         /* カテゴリの 1 始まりの番号（表示用） */
    bool expanded;          /* カテゴリ行が展開しているか。ノート行では true */
    bool selected;          /* 選択中のノートの行か */
};

#endif
