/* 本文の表示倍率だけを変えるnative操作。文書の書式や保存操作ではない（ADR0067）。 */
#ifndef NENEFOLIO_PANE_ZOOM_COMMAND_H
#define NENEFOLIO_PANE_ZOOM_COMMAND_H

enum pane_zoom_command : unsigned short
{
    PANE_ZOOM_COMMAND_IN = 121,
    PANE_ZOOM_COMMAND_OUT,
    PANE_ZOOM_COMMAND_RESET
};

#endif
