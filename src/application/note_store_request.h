#ifndef NENEFOLIO_NOTE_STORE_REQUEST_H
#define NENEFOLIO_NOTE_STORE_REQUEST_H
struct note_text;
/* Borrowed for one synchronous save. Omitted expected keeps ordinary save semantics. */
struct note_store_request
{
    const char *_Nonnull category;
    const char *_Nonnull note;
    const struct note_text *_Nonnull body;
    const struct note_text *_Nullable expected;
};
#endif
