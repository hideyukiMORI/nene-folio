#ifndef NENEFOLIO_MARKDOWN_SOURCE_SELECTION_H
#define NENEFOLIO_MARKDOWN_SOURCE_SELECTION_H
#include <stddef.h>
/* #1012: the paragraphs a selection in the reading view touches, as ordinals (marks before each
 * end), first <= last. */
struct markdown_source_selection
{
    size_t first;
    size_t last;
};
#endif
