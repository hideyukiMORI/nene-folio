/* 主窓が所有する一時的な操作入力面。フォーカスの正本は引き続き Win32（ADR 0016）。 */
#ifndef NENEFOLIO_COMMAND_SURFACE_MODE_H
#define NENEFOLIO_COMMAND_SURFACE_MODE_H

enum command_surface_mode : unsigned char
{
    COMMAND_SURFACE_CLOSED,
    COMMAND_SURFACE_EX,
    COMMAND_SURFACE_PALETTE,
    /* ノート内検索。Ex と違い Enter で閉じない（ADR 0023 の決定 4） */
    COMMAND_SURFACE_SEARCH
};

#endif
