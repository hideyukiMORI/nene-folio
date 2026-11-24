/* 左の欄の絞り込みの札の行の配置（子窓の client 座標・画素）。描画・当たり・読み上げ・固定帯の
 * 高さが、同じ 1 つの結果を使う（ADR0122）。全メンバーが独立に妥当（C-003）。 */
#ifndef NENEFOLIO_DRAWER_FILTER_BAND_H
#define NENEFOLIO_DRAWER_FILTER_BAND_H

#include <windows.h>

struct drawer_filter_band
{
    RECT group;     /* 札の行の全体（1 行か 2 行） */
    RECT favorites; /* お気に入りの札 */
    RECT tags;      /* タグの面を開く札 */
    RECT clear;     /* 「× 解除」。条件が効いていないときは空 */
    int bottom;     /* 札の行（と解除の行）の下端＝カテゴリの段の上端 */
};

#endif
