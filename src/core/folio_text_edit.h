#ifndef NENEFOLIO_FOLIO_TEXT_EDIT_H
#define NENEFOLIO_FOLIO_TEXT_EDIT_H
/* #883: the note body's own editing operations, offered by its context menu. The note pane runs
 * them through the same paths as their keys; NONE marks a menu action that is not one of them. */
enum folio_text_edit : unsigned char
{
    FOLIO_TEXT_EDIT_NONE,
    FOLIO_TEXT_EDIT_UNDO,
    FOLIO_TEXT_EDIT_CUT,
    FOLIO_TEXT_EDIT_COPY,
    FOLIO_TEXT_EDIT_PASTE,
    FOLIO_TEXT_EDIT_SELECT_ALL
};
#endif
