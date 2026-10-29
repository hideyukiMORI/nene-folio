/* ADR0073: the sole incremental matcher; only this opaque owner is mutable while building. */
#ifndef NENEFOLIO_INDEX_FILTER_BUILD_H
#define NENEFOLIO_INDEX_FILTER_BUILD_H
#include "index_filter_entry.h"
#include "index_filter_outcome.h"
#include "index_filter_readiness.h"
#include "index_filter_step.h"
struct index_filter_build;
/* Borrow immutable term bytes until READY or destroy. Empty term precedes UTF-8 rejection,
 * then count >= SIZE_MAX / sizeof(size_t) is refused before allocation. Failure preserves out.
 * Count zero creates a READY owner with no term borrow. Entries arrive in strict ledger order. */
[[nodiscard]] enum index_filter_outcome
index_filter_build_create(const char *_Nonnull term, size_t length, size_t count,
                          struct index_filter_build *_Nullable *_Nonnull out);
/* Entry/name/body are borrowed only during this call. Resubmit the same unchanged entry until
 * ENTRY_DONE/ALL_DONE. Mutation requires discarding the build first. Work counts entry start,
 * name bytes including NUL, single/tail/head/full comparison bytes. Positive work with MORE
 * consumes work; zero work does not advance. READY returns ALL_DONE without reading entry/term. */
[[nodiscard]] enum index_filter_step
index_filter_build_step(struct index_filter_build *_Nonnull build,
                        const struct index_filter_entry *_Nonnull entry, size_t *_Nonnull work);
[[nodiscard]] enum index_filter_readiness
index_filter_build_readiness(const struct index_filter_build *_Nonnull build);
/* READY is required; ref also requires index < count. These do not expose partial results. */
[[nodiscard]] size_t index_filter_build_count(const struct index_filter_build *_Nonnull sealed);
[[nodiscard]] struct note_ref
index_filter_build_ref(const struct index_filter_build *_Nonnull sealed, size_t index);
/* Requires READY. Success returns a new READY owner; OOM preserves input and out. */
[[nodiscard]] enum index_filter_outcome
index_filter_build_removed(const struct index_filter_build *_Nonnull sealed, struct note_ref target,
                           struct index_filter_build *_Nullable *_Nonnull out);
/* Never frees borrowed term/name/body. READY has already released its term borrow. */
void index_filter_build_destroy(struct index_filter_build *_Nullable build);
#endif
