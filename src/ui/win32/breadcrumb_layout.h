/* パンくずの描画だけに使う矩形と寸法（ADR 0017）。状態の所有者ではない。 */
#ifndef NENEFOLIO_BREADCRUMB_LAYOUT_H
#define NENEFOLIO_BREADCRUMB_LAYOUT_H

#include <windows.h>

struct breadcrumb_layout
{
    RECT ordinal;
    RECT category;
    RECT note;
    int padding;
    int tip;
};

#endif
