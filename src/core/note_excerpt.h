/* Immutable, owned source prefix. Never interprets a BOM or rewrites source bytes (ADR0094). */
#ifndef NENEFOLIO_NOTE_EXCERPT_H
#define NENEFOLIO_NOTE_EXCERPT_H
#include "note_excerpt_extent.h"
#include "note_excerpt_line.h"
#include "note_excerpt_outcome.h"
struct note_text;
struct note_excerpt;
constexpr size_t note_excerpt_byte_limit = 16384;
constexpr size_t note_excerpt_line_limit = 80;
/* 1..80 logical lines, including an empty final line only when its separator fits.
 * Excludes an entire CRLF or UTF-8 scalar if it would cross the prefix boundary.
 * Empty source is one empty logical line. Failure leaves out unchanged. */
[[nodiscard]] enum note_excerpt_outcome
note_excerpt_create(const struct note_text *_Nonnull source,
                    struct note_excerpt *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_excerpt_bytes(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] size_t note_excerpt_length(const struct note_excerpt *_Nonnull excerpt);
[[nodiscard]] size_t note_excerpt_line_count(const struct note_excerpt *_Nonnull excerpt);
/* index < line_count is required. */
[[nodiscard]] struct note_excerpt_line
note_excerpt_line(const struct note_excerpt *_Nonnull excerpt, size_t index);
[[nodiscard]] enum note_excerpt_extent
note_excerpt_extent(const struct note_excerpt *_Nonnull excerpt);
void note_excerpt_destroy(struct note_excerpt *_Nullable excerpt);
#endif
