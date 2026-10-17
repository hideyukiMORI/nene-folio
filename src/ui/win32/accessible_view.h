#ifndef NENEFOLIO_ACCESSIBLE_VIEW_H
#define NENEFOLIO_ACCESSIBLE_VIEW_H

#include <uchar.h>
#include <windows.h>

/* 1回の問い合わせに対する表示値。選択や文書の所有者にはしない。 */
struct accessible_view
{
    char16_t name[1024];
    char16_t help[1024];
    bool dialog;
    bool live;
    RECT bounds; /* screen座標。描画/hit testと同じ矩形から写す。 */
    bool enabled;
    bool offscreen;
    bool focusable;
    bool focused;
    bool selected;
    bool expanded;
    bool selection_required;
    int scroll;
    int scroll_limit;
    int viewport;
};

#endif
