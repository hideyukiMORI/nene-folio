/* ノートのタグの行の並び（#1263）。荷札・「+N」・「＋ タグ」の釦を、1 行に左から置く。描画・
 * 当たり判定・UIA の矩形が、同じ 1 回の配置を読む（C-003 の値の束）。 */
#ifndef NENEFOLIO_TAG_STRIP_LAYOUT_H
#define NENEFOLIO_TAG_STRIP_LAYOUT_H

#include "note_tags.h"

#include <windows.h>

struct tag_strip_layout
{
    RECT labels[note_tags_max_count]; /* 置いた荷札。先頭から shown 個が有効 */
    size_t shown;
    size_t hidden; /* 畳んだ荷札の数。0 なら「+N」は無い */
    RECT more;     /* 「+N」 */
    RECT add;      /* 「＋ タグ」の釦 */
};

#endif
