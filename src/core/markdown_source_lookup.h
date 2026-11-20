#ifndef NENEFOLIO_MARKDOWN_SOURCE_LOOKUP_H
#define NENEFOLIO_MARKDOWN_SOURCE_LOOKUP_H
#include "markdown_source_map.h"
#include "markdown_source_selection.h"
#include "note_byte_span.h"
#include <stddef.h>
struct note_text;
struct text_buffer;
/* #1012 / ADR0116: what the source map answers for the reading view's menu. The note text is
 * the one the map was made from. */
/* The byte where editing starts for the paragraph at `ordinal`: the block's content, or in a
 * table row the start of that cell's text. markdown_source_none when no block holds it. */
[[nodiscard]] size_t markdown_source_lookup_caret(const struct markdown_source_map *_Nonnull map,
                                                  const struct note_text *_Nonnull text,
                                                  size_t ordinal);
/* The Markdown of the blocks the selection touches, whole lines, with LF line breaks: the body's
 * lines from the first to the last block (a fence or a table whole), then each footnote's
 * definition after a blank line. False when no block holds the selection; the buffer may then
 * hold part of a copy and must not be used. */
[[nodiscard]] bool markdown_source_lookup_copy(const struct markdown_source_map *_Nonnull map,
                                               const struct note_text *_Nonnull text,
                                               struct markdown_source_selection selection,
                                               struct text_buffer *_Nonnull out);
/* #1071 / ADR0118: the other way. The paragraphs of the reading view that came from the source
 * bytes `source`: `first` is the first paragraph's ordinal and `last` the ordinal after the last
 * one (so `last - first` paragraphs). The blocks that overlap the span are taken from the first
 * one in the view's order while they follow one another; a span that lies between blocks (blank
 * lines) takes the next block after it. The rule before the footnotes has no source and is never
 * taken. False when no block is found. */
[[nodiscard]] bool
markdown_source_lookup_paragraphs(const struct markdown_source_map *_Nonnull map,
                                  struct note_byte_span source,
                                  struct markdown_source_selection *_Nonnull out);
#endif
