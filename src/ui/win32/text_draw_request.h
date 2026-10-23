/* 単行または折返しの描画要求。描画中だけ借りる値。 */
#ifndef NENEFOLIO_TEXT_DRAW_REQUEST_H
#define NENEFOLIO_TEXT_DRAW_REQUEST_H

#include <windows.h>

struct text_draw_request
{
    RECT bounds;
    UINT format;
    bool wrapped;
};

#endif
