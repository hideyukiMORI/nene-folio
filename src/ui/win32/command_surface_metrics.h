#ifndef NENEFOLIO_COMMAND_SURFACE_METRICS_H
#define NENEFOLIO_COMMAND_SURFACE_METRICS_H

#include "folio_language.h"
#include <windows.h>

/* 実font/幅で測った固定footer。結果だけの更新で再計測しない。 */
struct command_surface_metrics
{
    int width;
    UINT dpi;
    HFONT font;
    enum folio_language language;
    int notice;
    int settings_footer;
    int themes_footer;
};

#endif
