/* 主窓の共通頭と2領域。値はregions_forだけが導出する（ADR0047）。 */
#ifndef NENEFOLIO_FOLIO_REGIONS_H
#define NENEFOLIO_FOLIO_REGIONS_H

#include <windows.h>

struct folio_regions
{
    RECT header;
    RECT drawer;
    RECT splitter;
    RECT pane;
};

#endif
