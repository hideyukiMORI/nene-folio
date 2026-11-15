#ifndef NENEFOLIO_NOTE_TAGS_H
#define NENEFOLIO_NOTE_TAGS_H
#include "note_tag_presence.h"
#include "note_tags_outcome.h"
#include <stddef.h>
struct note_tag;
struct note_tags;
constexpr size_t note_tags_max_count = 32;
/* Copy validated names, deduplicate in first occurrence order; failure preserves out. */
[[nodiscard]] enum note_tags_outcome
note_tags_create(const struct note_tag *_Nonnull const *_Nonnull names, size_t count,
                 struct note_tags *_Nullable *_Nonnull out);
[[nodiscard]] enum note_tags_outcome note_tags_empty(struct note_tags *_Nullable *_Nonnull out);
[[nodiscard]] enum note_tags_outcome note_tags_copy(const struct note_tags *_Nonnull tags,
                                                    struct note_tags *_Nullable *_Nonnull out);
/* Pure transformation, never edits the input set or borrowed name. */
[[nodiscard]] enum note_tags_outcome note_tags_with(const struct note_tags *_Nonnull tags,
                                                    const struct note_tag *_Nonnull tag,
                                                    enum note_tag_presence presence,
                                                    struct note_tags *_Nullable *_Nonnull out);
[[nodiscard]] size_t note_tags_count(const struct note_tags *_Nonnull tags);
/* index < count; result lives as long as tags. */
[[nodiscard]] const struct note_tag *_Nonnull note_tags_at(const struct note_tags *_Nonnull tags,
                                                           size_t index);
[[nodiscard]] bool note_tags_contains(const struct note_tags *_Nonnull tags,
                                      const struct note_tag *_Nonnull tag);
/* Equality ignores order: tag assignment does not reorder a source representation. */
[[nodiscard]] bool note_tags_equal(const struct note_tags *_Nonnull left,
                                   const struct note_tags *_Nonnull right);
void note_tags_destroy(struct note_tags *_Nullable tags);
#endif
