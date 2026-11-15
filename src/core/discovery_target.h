/* Immutable intent kept across departure prompts; never a mutable body cache (ADR0095). */
#ifndef NENEFOLIO_DISCOVERY_TARGET_H
#define NENEFOLIO_DISCOVERY_TARGET_H
#include "discovery_target_outcome.h"
#include "note_source_match.h"
struct note_identity;
struct note_text;
struct discovery_target;
/* Identity, text and completed match refer to the same corpus entry. Copies all borrowed data;
 * failures leave both inputs and out unchanged. No ownership is transferred from the caller. */
[[nodiscard]] enum discovery_target_outcome
discovery_target_create(const struct note_identity *_Nonnull identity,
                        const struct note_text *_Nonnull body,
                        const struct note_source_match *_Nonnull match,
                        struct discovery_target *_Nullable *_Nonnull out);
[[nodiscard]] const struct note_identity *_Nonnull discovery_target_identity(
    const struct discovery_target *_Nonnull target);
[[nodiscard]] const struct note_text *_Nonnull discovery_target_body(
    const struct discovery_target *_Nonnull target);
[[nodiscard]] struct note_search_span
discovery_target_selection(const struct discovery_target *_Nonnull target);
void discovery_target_destroy(struct discovery_target *_Nullable target);
#endif
