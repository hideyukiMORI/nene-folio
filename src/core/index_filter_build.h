/* ADR0073/0074: the sole matcher; only this opaque owner is mutable while building. */
#ifndef NENEFOLIO_INDEX_FILTER_BUILD_H
#define NENEFOLIO_INDEX_FILTER_BUILD_H
#include "index_filter_entry.h"
#include "index_filter_outcome.h"
#include "index_filter_query.h"
#include "index_filter_readiness.h"
#include "index_filter_step.h"
#include "index_ranked_note.h"
struct index_filter_build;
/* Borrow immutable term bytes and optional tag set until READY or destroy. NAME_BODY empty
 * conditions (no term and no tags) precede UTF-8 rejection; NAMES accepts empty conditions.
 * then count >= SIZE_MAX / sizeof(size_t) is refused before allocation. Failure preserves out.
 * General terms with entries own length size_t prefix elements until READY; their product is
 * checked before allocation. Count zero creates READY without that scratch or a term borrow.
 * Entries arrive in strict ledger order. UTF-8/skip/prefix preparation is synchronous. */
[[nodiscard]] enum index_filter_outcome
index_filter_build_create(const struct index_filter_query *_Nonnull query, size_t count,
                          struct index_filter_build *_Nullable *_Nonnull out);
/* Entry/name/body are borrowed only during this call. Resubmit the same unchanged entry until
 * ENTRY_DONE/ALL_DONE. Mutation requires discarding the build first. Work counts entry start,
 * tag comparison bytes including NUL, name bytes including NUL, single/tail/scan comparison bytes
 * (one prefix fallback per comparison). Tag equality is exact, with query's ALL/ANY membership.
 * A missing tag set is nonmatching only when tag conditions are present.
 * General source length N uses at most 3N comparison work; preparation/cleanup time is outside it.
 * Positive work with MORE
 * consumes work; zero work does not advance. READY returns ALL_DONE without reading entry/term. */
/* NAMES additionally counts each source scalar subsequence comparison (at most 4 bytes decoded
 * per scalar), category length byte/NUL, and each projected row or bucket transition as one work.
 * Entry names/category names must be valid UTF-8. The final entry remains borrowed for resubmission
 * until projection finishes; projection does not read its strings. */
[[nodiscard]] enum index_filter_step
index_filter_build_step(struct index_filter_build *_Nonnull build,
                        const struct index_filter_entry *_Nonnull entry, size_t *_Nonnull work);
[[nodiscard]] enum index_filter_readiness
index_filter_build_readiness(const struct index_filter_build *_Nonnull build);
/* READY is required; ref also requires index < count. These do not expose partial results. */
[[nodiscard]] size_t index_filter_build_count(const struct index_filter_build *_Nonnull sealed);
[[nodiscard]] struct note_ref
index_filter_build_ref(const struct index_filter_build *_Nonnull sealed, size_t index);
/* READY is required. NAME_BODY has no ranked projection. NAMES retains all matches in ledger
 * order and separately exposes at most index_ranked_limit notes in stable rank order. */
[[nodiscard]] size_t
index_filter_build_ranked_count(const struct index_filter_build *_Nonnull sealed);
[[nodiscard]] struct index_ranked_note
index_filter_build_ranked(const struct index_filter_build *_Nonnull sealed, size_t index);
/* Requires READY. Success returns a new READY owner; OOM preserves input and out. */
[[nodiscard]] enum index_filter_outcome
index_filter_build_removed(const struct index_filter_build *_Nonnull sealed, struct note_ref target,
                           struct index_filter_build *_Nullable *_Nonnull out);
/* Never frees borrowed term/tags/name/body. READY has released its query borrows. */
void index_filter_build_destroy(struct index_filter_build *_Nullable build);
#endif
