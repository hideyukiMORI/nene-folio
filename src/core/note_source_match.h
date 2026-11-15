/* Read-only coordinate projection, like note_byte_span; never a mutable source owner. */
#ifndef NENEFOLIO_NOTE_SOURCE_MATCH_H
#define NENEFOLIO_NOTE_SOURCE_MATCH_H
#include "note_byte_span.h"
#include "note_search_span.h"
constexpr size_t note_source_context_lines = 20;
struct note_source_match
{
    struct note_byte_span bytes;
    struct note_search_span native;
    size_t line; /* One-based logical line of the first byte. */
    size_t line_start;
    size_t context_start;
    size_t context_line;
};
#endif
