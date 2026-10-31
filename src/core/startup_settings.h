/* ノート側settingsとは独立した、版1の次回保存先指定（ADR0077）。 */
#ifndef NENEFOLIO_STARTUP_SETTINGS_H
#define NENEFOLIO_STARTUP_SETTINGS_H
#include "startup_settings_outcome.h"
#include <stddef.h>
struct startup_settings;
struct storage_path;
struct json_writer;
/* directoryの不在は配布形式の既定場所。渡されたpathは複製する。 */
[[nodiscard]] enum startup_settings_outcome
startup_settings_create(const struct storage_path *_Nullable directory,
                        struct startup_settings *_Nullable *_Nonnull out);
[[nodiscard]] enum startup_settings_outcome
startup_settings_parse(const char *_Nonnull text, size_t length,
                       struct startup_settings *_Nullable *_Nonnull out);
void startup_settings_write(const struct startup_settings *_Nonnull settings,
                            struct json_writer *_Nonnull writer);
[[nodiscard]] const struct storage_path *_Nullable startup_settings_directory(
    const struct startup_settings *_Nonnull settings);
[[nodiscard]] bool startup_settings_equal(const struct startup_settings *_Nonnull left,
                                          const struct startup_settings *_Nonnull right);
void startup_settings_destroy(struct startup_settings *_Nullable settings);
#endif
