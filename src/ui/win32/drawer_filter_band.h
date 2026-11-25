/* 左の欄の絞り込みの札の行の配置（子窓の client 座標・画素）。描画・当たり・読み上げ・固定帯の
 * 高さが、同じ 1 つの結果を使う（ADR0122）。全メンバーが独立に妥当（C-003）。 */
#ifndef NENEFOLIO_DRAWER_FILTER_BAND_H
#define NENEFOLIO_DRAWER_FILTER_BAND_H

#include "note_tags.h"

#include <windows.h>

struct drawer_filter_band
{
    RECT group;     /* 札の行の全体（1 行か 2 行） */
    RECT favorites; /* お気に入りの札 */
    /* タグごとの札（#1247）。先頭の shown 個が置かれ、後ろの hidden 個は「+N」に畳まれている */
    RECT chips[note_tags_max_count];
    size_t shown;
    size_t hidden;
    RECT more;      /* 「+N」の札。畳んだ分が無いときは空 */
    RECT tags;      /* タグの面を開く札 */
    RECT match_all; /* 「すべて含む」。タグの条件が 2 つ未満のときは空（#1249） */
    RECT match_any; /* 「いずれか」。同じ */
    RECT clear;     /* 「× 解除」。条件が効いていないときは空 */
    /* 検索の範囲の行（#1292）。条件と検索語の両方があるときだけ。無いときは 3 つとも空。
     * 見出しは、2 択と 1 行に入らない幅でも空 */
    RECT scope_label;
    RECT scope_all;      /* 「すべてのノート」 */
    RECT scope_filtered; /* 「絞り込みの中」 */
    int bottom;          /* 札の行（と解除の行・検索の範囲の行）の下端＝カテゴリの段の上端 */
};

#endif
