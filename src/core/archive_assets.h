#ifndef NENEFOLIO_ARCHIVE_ASSETS_H
#define NENEFOLIO_ARCHIVE_ASSETS_H
#include "archive_file.h"
#include "note_archive_outcome.h"
struct archive_assets;
[[nodiscard]] enum note_archive_outcome
archive_assets_create(struct archive_assets *_Nullable *_Nonnull out);
/* Copies the path; bytes remain borrowed until the archive snapshot is built. */
[[nodiscard]] enum note_archive_outcome
archive_assets_append(struct archive_assets *_Nonnull assets, struct archive_file file);
[[nodiscard]] size_t archive_assets_count(const struct archive_assets *_Nonnull assets);
[[nodiscard]] const struct archive_file *_Nonnull archive_assets_at(
    const struct archive_assets *_Nonnull assets, size_t index);
[[nodiscard]] bool archive_assets_contains(const struct archive_assets *_Nonnull assets,
                                           const char *_Nonnull path);
void archive_assets_destroy(struct archive_assets *_Nullable assets);
#endif
