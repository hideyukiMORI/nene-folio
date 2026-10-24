/* 幅/利用可能高/DPI/font/modeだけから導く配置。結果文字列や業務状態は保持しない。 */
#ifndef NENEFOLIO_COMMAND_BAND_LAYOUT_H
#define NENEFOLIO_COMMAND_BAND_LAYOUT_H

#include "command_surface_mode.h"

#include <stddef.h>
#include <windows.h>

/* 置換の欄が持つ EDIT の数（パターンと置換文字列・ADR 0028 の決定 8(a)）。 */
constexpr size_t replace_input_count = 2;

struct command_band_layout
{
    bool valid;
    int width;
    int available_height;
    UINT dpi;
    HFONT _Nullable font;
    enum command_surface_mode mode;
    RECT label[replace_input_count];
    RECT field[replace_input_count];
    RECT action[replace_input_count];
    RECT close;
    RECT result;
    int height;
    int pane_top;
    int pane_bottom;
    bool narrow;
    int measured_width;
    int result_height;
};

#endif
