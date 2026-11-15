/* Private scalar/line measurements used only while constructing note_excerpt. */
#ifndef NENEFOLIO_NOTE_EXCERPT_SPAN_H
#define NENEFOLIO_NOTE_EXCERPT_SPAN_H
#include "note_excerpt.h"
struct note_excerpt_span
{
    size_t offsets[note_excerpt_line_limit];
    size_t lines;
    size_t length;
    enum note_excerpt_extent extent;
};
#endif
