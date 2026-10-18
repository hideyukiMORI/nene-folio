/* 未保存の合図の色（ADR0060 の決定 8・#349）。塗りが地（window / pane）に 3:1 以上ある
 * テーマでは filled で fill に塗って ink の字を載せ、どの塗りも届かないテーマでは fill と ink は
 * 同じ線の色（THEME_LINE_UNSAVED）で、2px の枠と字と丸をその色で描く。新しいトークンではない。 */
#ifndef NENEFOLIO_THEME_UNSAVED_H
#define NENEFOLIO_THEME_UNSAVED_H

#include "rgb_color.h"

struct theme_unsaved
{
    bool filled;
    struct rgb_color fill;
    struct rgb_color ink;
};

#endif
