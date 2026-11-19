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
    FOLIO_TEXT_EDIT_SELECT_ALL,
    FOLIO_TEXT_EDIT_OPEN_LINK,    /* #900: the link under the caret, through the link path */
    FOLIO_TEXT_EDIT_COPY_LINK,    /* #900: its validated URI to the clipboard port */
    FOLIO_TEXT_EDIT_EDIT_HERE,    /* #1012: the editing view at this paragraph's source */
    FOLIO_TEXT_EDIT_COPY_MARKDOWN /* #1012: the selected blocks' Markdown to the clipboard */
};
#endif
