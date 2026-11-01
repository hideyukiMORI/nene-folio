#ifndef NENEFOLIO_NOTE_IMPORT_REQUEST_H
#define NENEFOLIO_NOTE_IMPORT_REQUEST_H
#include <stddef.h>
struct storage_path;
/* Borrowed immutable choices; each path was accepted by the canonical path factory. */
struct note_import_request
{
    size_t category;
    struct storage_path *_Nonnull const *_Nonnull paths;
    size_t count;
};
#endif
