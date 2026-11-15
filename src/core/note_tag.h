/* ADR0092: one identity for source and GUI names; no locale or normalization. */
#ifndef NENEFOLIO_NOTE_TAG_H
#define NENEFOLIO_NOTE_TAG_H
#include "note_tag_outcome.h"
#include <stddef.h>
struct note_tag;
constexpr size_t note_tag_max_points = 64;
constexpr size_t note_tag_max_bytes = 256;
/* Trim ASCII space/tab; reject empty, controls, line separators and BOM. Failure preserves out. */
[[nodiscard]] enum note_tag_outcome note_tag_create(const char *_Nonnull bytes, size_t length,
                                                    struct note_tag *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_tag_bytes(const struct note_tag *_Nonnull tag);
[[nodiscard]] size_t note_tag_length(const struct note_tag *_Nonnull tag);
[[nodiscard]] bool note_tag_equal(const struct note_tag *_Nonnull left,
                                  const struct note_tag *_Nonnull right);
void note_tag_destroy(struct note_tag *_Nullable tag);
#endif
