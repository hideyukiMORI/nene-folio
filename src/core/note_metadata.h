/* ADR0092: immutable metadata parsed from the owning note_text's immutable bytes.
 * NONE is different from unsupported/malformed. Only SUPPORTED is hidden in the viewer. */
#ifndef NENEFOLIO_NOTE_METADATA_H
#define NENEFOLIO_NOTE_METADATA_H
#include "note_metadata_outcome.h"
#include "note_metadata_status.h"
#include "note_tag_edit_outcome.h"
#include <stddef.h>
struct note_metadata;
struct note_tags;
struct note_text;
struct replace_edit;
constexpr size_t note_metadata_max_bytes = 65536;
constexpr size_t note_metadata_max_keys = 128;
/* Source is validated note_text bytes; it must stay immutable and alive until destroy.
 * Syntax failures are READY with a typed status, not a refusal to open the note. OOM preserves out.
 */
[[nodiscard]] enum note_metadata_outcome
note_metadata_create(const char *_Nonnull source, size_t length,
                     struct note_metadata *_Nullable *_Nonnull out);
[[nodiscard]] enum note_metadata_status
note_metadata_status(const struct note_metadata *_Nonnull metadata);
/* NONE and SUPPORTED have known membership; other states are unavailable, not known empty. */
[[nodiscard]] bool note_metadata_available(const struct note_metadata *_Nonnull metadata);
[[nodiscard]] const struct note_tags *_Nonnull note_metadata_tags(
    const struct note_metadata *_Nonnull metadata);
/* Zero unless SUPPORTED. Absolute source byte offset; do not slice/renumber media positions. */
[[nodiscard]] size_t note_metadata_body(const struct note_metadata *_Nonnull metadata);
/* One native editor replacement, using CR paragraphs and UTF-16 units. On every non-READY result,
 * out is unchanged. The source must be the current editor snapshot (or identical saved text). */
[[nodiscard]] enum note_tag_edit_outcome
note_metadata_edit(const struct note_metadata *_Nonnull metadata,
                   const struct note_tags *_Nonnull desired,
                   struct replace_edit *_Nullable *_Nonnull out);
/* The same validated byte patch as edit, preserving outside bytes and mixed line endings.
 * The owned result is a complete note. UNCHANGED and every refusal preserve out. */
[[nodiscard]] enum note_tag_edit_outcome
note_metadata_rewrite(const struct note_metadata *_Nonnull metadata,
                      const struct note_tags *_Nonnull desired,
                      struct note_text *_Nullable *_Nonnull out);
void note_metadata_destroy(struct note_metadata *_Nullable metadata);
#endif
