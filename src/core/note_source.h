/* One UTF-8 scalar/paragraph walk shared by source positions and tag edits (ADR0095). */
#ifndef NENEFOLIO_NOTE_SOURCE_H
#define NENEFOLIO_NOTE_SOURCE_H
#include "note_source_projection.h"
#include <stddef.h>
#include <stdint.h>
/* Valid UTF-8, at < length, and a scalar/CRLF boundary are required. Advances one scalar or
 * one complete CRLF; all three source newline forms yield a single U+000D. */
[[nodiscard]] uint32_t note_source_next(const char *_Nonnull bytes, size_t length,
                                        size_t *_Nonnull at);
[[nodiscard]] size_t note_source_point_units(uint32_t point,
                                             enum note_source_projection projection);
/* The entire range must end at a scalar/CRLF boundary. No allocation or source mutation. */
[[nodiscard]] size_t note_source_units(const char *_Nonnull bytes, size_t length,
                                       enum note_source_projection projection);
#endif
