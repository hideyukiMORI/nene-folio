/* B主操作のhover用の派生色対（ADR0060・#496）。固定theme tokenではない。
 * 各RGBは独立に妥当な値なので完全型で公開する（C-003）。 */
#ifndef NENEFOLIO_THEME_PRIMARY_HOVER_H
#define NENEFOLIO_THEME_PRIMARY_HOVER_H

#include "rgb_color.h"

struct theme_primary_hover
{
    struct rgb_color fill;
    struct rgb_color ink;
};

#endif
