#ifndef NENEFOLIO_NOTE_EXPORT_REQUEST_H
#define NENEFOLIO_NOTE_EXPORT_REQUEST_H
#include "image_export_policy.h"
#include "note_export_scope.h"
#include <stddef.h>
#include <uchar.h>
struct storage_path;
struct note_export_request
{
    enum note_export_scope scope;
    enum image_export_policy images;
    size_t category;
    const struct storage_path *_Nonnull destination;
    const char16_t *_Nonnull units;
    size_t count;
};
#endif
