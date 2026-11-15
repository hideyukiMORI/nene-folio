#ifndef NENEFOLIO_NOTE_BYTE_SPAN_H
#define NENEFOLIO_NOTE_BYTE_SPAN_H
#include <stddef.h>
/* Half-open byte offsets into one immutable UTF-8 source; never native UTF-16 positions. */
struct note_byte_span
{
    size_t start;
    size_t end;
};
#endif
