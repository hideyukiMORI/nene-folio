/* 部品の描画が字の側へ返す値。字は各窓の UTF-8 の描画経路が、この色と矩形で描く。 */
#ifndef NENEFOLIO_UI_PART_INK_H
#define NENEFOLIO_UI_PART_INK_H

#include <windows.h>

struct ui_part_ink
{
    COLORREF text;
    RECT label;
};

#endif
