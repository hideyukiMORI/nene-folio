/* Immutable saved-note tag usage, derived from the current ledger's completed corpus. */
#ifndef NENEFOLIO_TAG_CATALOG_H
#define NENEFOLIO_TAG_CATALOG_H
#include "tag_catalog_outcome.h"
#include <stddef.h>
struct note_tag;
struct note_tags;
struct tag_catalog;
/* Each element is one current saved note; nullptr means metadata is unavailable.
 * Available empty sets are known untagged. Borrow only during construction, copy unique names.
 * Order is exact UTF-8 byte order. Failure preserves out; no partial catalog is published. */
[[nodiscard]] enum tag_catalog_outcome
tag_catalog_create(const struct note_tags *_Nullable const *_Nonnull notes, size_t count,
                   struct tag_catalog *_Nullable *_Nonnull out);
[[nodiscard]] size_t tag_catalog_count(const struct tag_catalog *_Nonnull catalog);
/* Index < count; borrowed name lives as long as catalog. */
[[nodiscard]] const struct note_tag *_Nonnull tag_catalog_tag(
    const struct tag_catalog *_Nonnull catalog, size_t index);
[[nodiscard]] size_t tag_catalog_usage(const struct tag_catalog *_Nonnull catalog, size_t index);
[[nodiscard]] size_t tag_catalog_unavailable(const struct tag_catalog *_Nonnull catalog);
/* Exact identity, using the same sorted names; no locale/case folding. */
[[nodiscard]] bool tag_catalog_contains(const struct tag_catalog *_Nonnull catalog,
                                        const struct note_tag *_Nonnull tag);
void tag_catalog_destroy(struct tag_catalog *_Nullable catalog);
#endif
