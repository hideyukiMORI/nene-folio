#ifndef NENEFOLIO_DIAGRAM_REQUEST_H
#define NENEFOLIO_DIAGRAM_REQUEST_H
#include "diagram_palette.h"
struct note_text;
/* Borrowed only for request submission; the single media worker owns its copy. */
struct diagram_request
{
    const struct note_text *_Nonnull source;
    struct diagram_palette palette;
    const char *_Nonnull face;
};
#endif
