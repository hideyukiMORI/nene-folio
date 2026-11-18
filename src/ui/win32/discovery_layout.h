#ifndef NENEFOLIO_DISCOVERY_LAYOUT_H
#define NENEFOLIO_DISCOVERY_LAYOUT_H
#include <windows.h>
/* One geometry value feeds paint, native child placement, hit tests and UIA. */
struct discovery_layout
{
    RECT input;
    RECT names;
    RECT body;
    RECT conditions;
    RECT status;
    RECT rows;
    RECT preview_title;
    RECT preview_status;
    RECT preview;
    RECT star;
    RECT previous_tag;
    RECT tag;
    RECT next_tag;
    RECT clear;
    RECT toggle;
    RECT open;
    RECT edit;
    bool wide;
    bool compact;
};
#endif
