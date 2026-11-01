/* native操作部品のpointer表示だけを追跡する、子HWND寿命の状態。 */
#ifndef NENEFOLIO_DIALOG_POINTER_STATE_H
#define NENEFOLIO_DIALOG_POINTER_STATE_H

#include "dialog_pointer_kind.h"
#include <windows.h>

struct dialog_pointer_state
{
    WNDPROC original;
    enum dialog_pointer_kind kind;
    bool hovered;
};

#endif
