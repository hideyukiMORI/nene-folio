#ifndef NENEFOLIO_NOTE_STORE_REQUEST_H
#define NENEFOLIO_NOTE_STORE_REQUEST_H
struct note_text;
/* Borrowed for one synchronous save. With `expected`, the file is replaced only while it still
 * holds that text; `recreate` lets a file that is gone be written again all the same. Without
 * `expected` the file is written whatever it holds. */
struct note_store_request
{
    const char *_Nonnull category;
    const char *_Nonnull note;
    const struct note_text *_Nonnull body;
    const struct note_text *_Nullable expected;
    bool recreate;
};
#endif
