/* 領域の表示。本文のVIEW/EDITとは独立し、applicationが一つだけ所有する（ADR0047）。 */
#ifndef NENEFOLIO_FOLIO_LAYOUT_MODE_H
#define NENEFOLIO_FOLIO_LAYOUT_MODE_H

enum folio_layout_mode : unsigned char
{
    FOLIO_LAYOUT_BOTH,
    FOLIO_LAYOUT_DRAWER,
    FOLIO_LAYOUT_NOTE
};

#endif
