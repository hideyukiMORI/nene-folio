#ifndef NENEFOLIO_ARCHIVE_CONTENTS_H
#define NENEFOLIO_ARCHIVE_CONTENTS_H
#include "archive_note.h"
#include <stddef.h>
struct archive_assets;
struct archive_contents
{
    const struct archive_note *_Nonnull notes;
    size_t count;
    const struct archive_assets *_Nullable assets;
};
#endif
