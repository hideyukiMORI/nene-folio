#ifndef NENEFOLIO_MARKDOWN_BLOCKS_OPTIONS_H
#define NENEFOLIO_MARKDOWN_BLOCKS_OPTIONS_H
#include "folio_contrast.h"
#include "folio_language.h"
struct markdown_source_map;
/* How a full render is asked for: the language of the code blocks' Copy, whether it is
 * underlined (ADR0098), and the source map to fill (#1008 / ADR0116), if any. */
struct markdown_blocks_options
{
    enum folio_language language;
    enum folio_contrast contrast;
    struct markdown_source_map *_Nullable map;
};
#endif
