/* 面の外形から導く矩形。描画・hit・native子窓・UIAで共用する（ADR0060 決定22）。 */
#ifndef NENEFOLIO_UI_PART_SURFACE_REGION_H
#define NENEFOLIO_UI_PART_SURFACE_REGION_H

enum ui_part_surface_region : unsigned char
{
    UI_PART_SURFACE_TITLE,
    UI_PART_SURFACE_TITLE_TEXT,
    UI_PART_SURFACE_CLOSE,
    UI_PART_SURFACE_CONTENT
};

#endif
