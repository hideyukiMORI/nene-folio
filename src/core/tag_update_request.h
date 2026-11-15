#ifndef NENEFOLIO_TAG_UPDATE_REQUEST_H
#define NENEFOLIO_TAG_UPDATE_REQUEST_H
struct note_tag;
struct note_text;
/* Names come from validated ledgers. Missing source means no available saved body.
 * Missing replacement means remove the source tag; every value is borrowed for create only. */
struct tag_update_request
{
    const char *_Nonnull category;
    const char *_Nonnull note;
    const struct note_text *_Nullable source;
    const struct note_tag *_Nonnull from;
    const struct note_tag *_Nullable to;
};
#endif
