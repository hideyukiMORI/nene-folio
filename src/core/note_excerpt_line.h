#ifndef NENEFOLIO_NOTE_EXCERPT_LINE_H
#define NENEFOLIO_NOTE_EXCERPT_LINE_H
#include <stddef.h>
/* Byte span in the excerpt, excluding the logical line separator. */
struct note_excerpt_line
{
    size_t offset;
    size_t length;
};
#endif
