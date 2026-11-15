#ifndef NENEFOLIO_TAG_CANDIDATES_SOURCE_H
#define NENEFOLIO_TAG_CANDIDATES_SOURCE_H
#include <stdint.h>
struct tag_catalog;
struct note_tags;
struct utf8_text;
/* A synchronous projection request. All inputs are borrowed only during prepare. */
struct tag_candidates_source
{
    const struct tag_catalog *_Nullable catalog;
    const struct note_tags *_Nullable selected;
    const struct utf8_text *_Nonnull query;
    uint64_t revision;
};
#endif
