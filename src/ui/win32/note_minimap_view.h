#ifndef NENEFOLIO_NOTE_MINIMAP_VIEW_H
#define NENEFOLIO_NOTE_MINIMAP_VIEW_H
/* 一回のnative観測。本文・選択・位置の所有者にはしない。 */
struct note_minimap_view
{
    int lines;
    int first;
    int visible;
};
#endif
