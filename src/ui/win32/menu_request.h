#ifndef NENEFOLIO_MENU_REQUEST_H
#define NENEFOLIO_MENU_REQUEST_H
#include "folio_menu_context.h"
#include "note_ref.h"
#include <windows.h>
/* 同期通知の値。座標は主窓client、anchor.topは上へ開く境界、bottomは下へ開く境界。 */
struct menu_request
{
    enum folio_menu_context context;
    struct note_ref target;
    RECT anchor;
    bool keyboard;
};
#endif
