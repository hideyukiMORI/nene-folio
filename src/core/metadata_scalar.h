/* The bounded one-line scalar reader used only by note_metadata (ADR0092). */
#ifndef NENEFOLIO_METADATA_SCALAR_H
#define NENEFOLIO_METADATA_SCALAR_H
#include "metadata_scalar_context.h"
#include "metadata_scalar_outcome.h"
#include <stddef.h>
struct metadata_scalar;
/* YAML printable characters, shared by one-line and block scalar validation. */
[[nodiscard]] bool metadata_scalar_printable(const char *_Nonnull bytes, size_t length);
/* No leading separation; length excludes the physical newline. flow stops at comma/].
 * Consumed excludes trailing spaces/comment. Copies decoded bytes; failure preserves out. */
[[nodiscard]] enum metadata_scalar_outcome
metadata_scalar_create(const char *_Nonnull bytes, size_t length,
                       enum metadata_scalar_context context,
                       struct metadata_scalar *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull metadata_scalar_bytes(
    const struct metadata_scalar *_Nonnull scalar);
[[nodiscard]] size_t metadata_scalar_length(const struct metadata_scalar *_Nonnull scalar);
[[nodiscard]] size_t metadata_scalar_consumed(const struct metadata_scalar *_Nonnull scalar);
[[nodiscard]] bool metadata_scalar_is_string(const struct metadata_scalar *_Nonnull scalar);
void metadata_scalar_destroy(struct metadata_scalar *_Nullable scalar);
#endif
