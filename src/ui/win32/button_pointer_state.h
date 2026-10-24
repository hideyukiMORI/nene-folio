/* native BUTTONのpointer表示だけを追跡する、子HWND寿命の状態。 */
#ifndef NENEFOLIO_BUTTON_POINTER_STATE_H
#define NENEFOLIO_BUTTON_POINTER_STATE_H

#include <windows.h>

struct button_pointer_state
{
    WNDPROC original;
    bool hovered;
};

#endif
