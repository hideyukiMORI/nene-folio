#ifndef NENEFOLIO_TAG_CANDIDATES_H
#define NENEFOLIO_TAG_CANDIDATES_H
#include "candidate_create_outcome.h"
#include <stddef.h>
#include <stdint.h>
struct tag_candidates;
struct tag_candidates_source;
struct note_tag;
/* UI projection cache. Names are owned copies, independent of application session/catalog. */
[[nodiscard]] enum candidate_create_outcome
tag_candidates_create(struct tag_candidates *_Nullable *_Nonnull out);
void tag_candidates_destroy(struct tag_candidates *_Nullable candidates);
void tag_candidates_invalidate(struct tag_candidates *_Nonnull candidates);
[[nodiscard]] bool tag_candidates_current(const struct tag_candidates *_Nonnull candidates,
                                          uint64_t revision);
[[nodiscard]] enum candidate_create_outcome
tag_candidates_prepare(struct tag_candidates *_Nonnull candidates,
                       const struct tag_candidates_source *_Nonnull source);
[[nodiscard]] size_t tag_candidates_count(const struct tag_candidates *_Nonnull candidates);
/* index < count. Copy lives until invalidate/prepare/destroy. */
[[nodiscard]] const struct note_tag *_Nonnull tag_candidates_tag(
    const struct tag_candidates *_Nonnull candidates, size_t index);
[[nodiscard]] size_t tag_candidates_usage(const struct tag_candidates *_Nonnull candidates,
                                          size_t index);
#endif
