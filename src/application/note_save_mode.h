#ifndef NENEFOLIO_NOTE_SAVE_MODE_H
#define NENEFOLIO_NOTE_SAVE_MODE_H
/* Both modes refuse to replace a file that changed outside since it was loaded or last saved
 * (ADR0119 decision 4): the editor keeps its text and the file keeps the outside version. */
enum note_save_mode : unsigned char
{
    /* The ordinary save. Unedited text is not saved and not compared; a file that was removed
     * outside is created again, since there is nothing to overwrite (ADR0117). */
    NOTE_SAVE_EDITS,
    /* Before a surface that works on the saved files (tag management, ADR0093): compares even
     * unedited text, and a removed file is a change. */
    NOTE_SAVE_CHECK_CURRENT
};
#endif
