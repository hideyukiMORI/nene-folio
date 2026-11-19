#ifndef NENEFOLIO_MARKDOWN_SOURCE_MAP_H
#define NENEFOLIO_MARKDOWN_SOURCE_MAP_H
#include "markdown_rtf_outcome.h"
#include "markdown_source_entry.h"
#include <stddef.h>
/* #1008 / ADR0116: which source lines each paragraph of the reading view came from. Entries are
 * added by the one Markdown traversal as it writes each block; finishing counts the paragraph
 * marks each block wrote into the RTF, as RichEdit keeps them. The display stays the source of
 * truth for where the reader is (ARC-004): the view gives a paragraph ordinal, this map gives
 * the lines; when the view's count differs from `total`, the map is not used. */
struct markdown_source_map;
constexpr size_t markdown_source_none = (size_t)-1;
[[nodiscard]] enum markdown_rtf_outcome
markdown_source_map_create(struct markdown_source_map *_Nullable *_Nonnull out);
void markdown_source_map_destroy(struct markdown_source_map *_Nullable map);
/* `entry.rtf` is where the block's RTF begins; entries come in RTF order. False when out of
 * memory. */
[[nodiscard]] bool markdown_source_map_add(struct markdown_source_map *_Nonnull map,
                                           struct markdown_source_entry entry);
/* Counts the marks of each entry in the finished RTF. */
void markdown_source_map_finish(struct markdown_source_map *_Nonnull map, const char *_Nonnull rtf,
                                size_t length);
/* The marks the reading view shows (RichEdit takes a final \par into its own last mark). */
[[nodiscard]] size_t markdown_source_map_total(const struct markdown_source_map *_Nonnull map);
[[nodiscard]] size_t markdown_source_map_count(const struct markdown_source_map *_Nonnull map);
[[nodiscard]] struct markdown_source_entry
markdown_source_map_at(const struct markdown_source_map *_Nonnull map, size_t index);
/* The entry whose paragraphs include the ordinal (marks before the position in the view), or
 * markdown_source_none. */
[[nodiscard]] size_t markdown_source_map_find(const struct markdown_source_map *_Nonnull map,
                                              size_t ordinal);
#endif
