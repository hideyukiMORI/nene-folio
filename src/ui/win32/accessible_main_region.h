#ifndef NENEFOLIO_ACCESSIBLE_MAIN_REGION_H
#define NENEFOLIO_ACCESSIBLE_MAIN_REGION_H
/* Geometry dispatch is part of the canonical per-kind control table (CNF-009). */
enum accessible_main_region : unsigned char
{
    ACCESSIBLE_MAIN_NONE,
    ACCESSIBLE_MAIN_RANGE,
    ACCESSIBLE_MAIN_HEADER,
    ACCESSIBLE_MAIN_GROUP,
    ACCESSIBLE_MAIN_PANE
};
#endif
