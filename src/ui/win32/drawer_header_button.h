/* 固定帯で押しているボタン。行のdragとは別に一つだけ覚える（ADR0051）。 */
#ifndef NENEFOLIO_DRAWER_HEADER_BUTTON_H
#define NENEFOLIO_DRAWER_HEADER_BUTTON_H

enum drawer_header_button : unsigned char
{
    DRAWER_HEADER_NONE,
    DRAWER_HEADER_NEW_CATEGORY
};

#endif
