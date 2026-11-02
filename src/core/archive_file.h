#ifndef NENEFOLIO_ARCHIVE_FILE_H
#define NENEFOLIO_ARCHIVE_FILE_H
#include <stddef.h>
struct archive_file
{
    const char *_Nonnull path;
    const char *_Nonnull bytes;
    size_t length;
};
#endif
