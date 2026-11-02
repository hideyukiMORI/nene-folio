#ifndef NENEFOLIO_NOTE_ARCHIVE_H
#define NENEFOLIO_NOTE_ARCHIVE_H
#include "archive_note.h"
#include "note_archive_outcome.h"
#include <stddef.h>
struct note_archive;
struct archive_assets;
constexpr size_t note_archive_byte_limit = 128 * 1024 * 1024;
constexpr size_t note_archive_count_limit = 65534;
[[nodiscard]] enum note_archive_outcome
note_archive_create(const struct archive_note *_Nonnull notes, size_t count,
                    const struct archive_assets *_Nullable assets,
                    struct note_archive *_Nullable *_Nonnull out);
[[nodiscard]] const char *_Nonnull note_archive_bytes(const struct note_archive *_Nonnull archive);
[[nodiscard]] size_t note_archive_length(const struct note_archive *_Nonnull archive);
void note_archive_destroy(struct note_archive *_Nullable archive);
#endif
