/* 一度の描画に使う、操作パレットの行の値（C-003）。 */
#ifndef NENEFOLIO_COMMAND_ROW_H
#define NENEFOLIO_COMMAND_ROW_H

#include "folio_command.h"

#include <windows.h>

struct command_row
{
    RECT bounds;
    enum folio_command command;
    bool selected;
};

#endif
