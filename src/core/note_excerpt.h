/* Immutable, owned source window. Never interprets a BOM or rewrites source bytes (ADR0094/0095).
 */
#ifndef NENEFOLIO_NOTE_EXCERPT_H
#define NENEFOLIO_NOTE_EXCERPT_H
#include "note_excerpt_extent.h"
#include "note_excerpt_line.h"
#include "note_excerpt_outcome.h"
#include "note_snippet_clip.h"
#include "note_source_match.h"
struct note_text;
struct note_excerpt;
constexpr size_t note_excerpt_byte_limit = 16384;
constexpr size_t note_excerpt_line_limit = 80;
constexpr size_t note_excerpt_snippet_capacity = 200;
/* 1..80 logical lines, including an empty final line only when its separator fits.
 * Excludes an entire CRLF or UTF-8 scalar if it would cross the prefix boundary.
 * Empty source is one empty logical line. Failure leaves out unchanged. */
[[nodiscard]] enum note_excerpt_outcome
note_excerpt_create(const struct note_text *_Nonnull source,
                    struct note_excerpt *_Nullable *_Nonnull out);
/* The match comes from the completed matcher over this exact source. Checks range boundaries;
 * the source line ordinals are the matcher's coordinate projection, not recomputed from byte 0.
 * Single-line match must be nonempty and at most 1024 bytes. Failure preserves out. */
[[nodiscard]] enum note_excerpt_outcome
note_excerpt_create_match(const struct note_text *_Nonnull source,
                          const struct note_source_match *_Nonnull match,
                          struct note_excerpt *_Nullable *_Nonnull out);
/* Same-source completed match required; out has at least note_excerpt_snippet_capacity bytes.
 * Writes a bounded single-line display snippet; UI supplies marks for the returned omitted edges.
 */
[[nodiscard]] struct note_snippet_clip
note_excerpt_snippet(const struct note_text *_Nonnull source,
                     const struct note_source_match *_Nonnull match, char *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_excerpt_bytes(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] size_t note_excerpt_length(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] size_t note_excerpt_line_count(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] size_t note_excerpt_first_line(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] bool note_excerpt_leading_partial(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] bool note_excerpt_trailing_partial(const struct note_excerpt *_Nonnull excerpt);
/* UTF-16 positions in the CRLF-projected readonly EDIT, or {0,0} for a prefix. */
[[nodiscard]] struct note_search_span
note_excerpt_selection(const struct note_excerpt *_Nonnull excerpt);
/* index < line_count is required. */
[[nodiscard]] struct note_excerpt_line
note_excerpt_line(const struct note_excerpt *_Nonnull excerpt, size_t index);
[[nodiscard]] enum note_excerpt_extent
note_excerpt_extent(const struct note_excerpt *_Nonnull excerpt);
void note_excerpt_destroy(struct note_excerpt *_Nullable excerpt);
#endif
