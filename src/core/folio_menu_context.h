#ifndef NENEFOLIO_FOLIO_MENU_CONTEXT_H
#define NENEFOLIO_FOLIO_MENU_CONTEXT_H
enum folio_menu_context : unsigned char
{
    FOLIO_MENU_OPERATIONS,
    FOLIO_MENU_LAYOUT,
    FOLIO_MENU_CATEGORY,
    FOLIO_MENU_NOTE,
    FOLIO_MENU_EMPTY,
    FOLIO_MENU_TEXT_EDIT, /* #883: the note body while editing */
    FOLIO_MENU_TEXT_VIEW  /* #883: the note body while viewing (read-only) */
};
#endif
