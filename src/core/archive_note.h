#ifndef NENEFOLIO_ARCHIVE_NOTE_H
#define NENEFOLIO_ARCHIVE_NOTE_H
struct note_text;
/* Borrowed, validated ledger names and note text for one immutable archive snapshot. */
struct archive_note
{
    const char *_Nonnull category;
    const char *_Nonnull name;
    const struct note_text *_Nonnull body;
};
#endif
