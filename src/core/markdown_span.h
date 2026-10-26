#ifndef NENEFOLIO_MARKDOWN_SPAN_H
#define NENEFOLIO_MARKDOWN_SPAN_H
#include <stddef.h>
/* Borrowed source; offsets include one virtual newline between adjacent spans. */
struct markdown_span
{
    const char *_Nonnull bytes;
    size_t length;
    size_t offset;
};
#endif
