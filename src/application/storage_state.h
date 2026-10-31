#ifndef NENEFOLIO_STORAGE_STATE_H
#define NENEFOLIO_STORAGE_STATE_H
#include "folio_language.h"
#include "storage_outcome.h"
#include "storage_phase.h"
#include <stddef.h>
struct storage_state;
struct storage_port;
struct storage_path;
[[nodiscard]] enum storage_outcome
storage_state_create(const struct storage_port *_Nonnull port,
                     struct storage_state *_Nullable *_Nonnull out);
/* runtimeでは次回設定だけを変える。初回/復旧では起動するrootも同じ確定で採用する。 */
[[nodiscard]] enum storage_outcome storage_state_choose(struct storage_state *_Nonnull state,
                                                        const char *_Nonnull text, size_t length);
[[nodiscard]] enum storage_phase storage_state_phase(const struct storage_state *_Nonnull state);
[[nodiscard]] enum storage_outcome storage_state_notice(const struct storage_state *_Nonnull state);
[[nodiscard]] const struct storage_path *_Nonnull storage_state_directory(
    const struct storage_state *_Nonnull state);
[[nodiscard]] const struct storage_path *_Nonnull storage_state_next_directory(
    const struct storage_state *_Nonnull state);
[[nodiscard]] const struct storage_path *_Nonnull storage_state_default_directory(
    const struct storage_state *_Nonnull state);
[[nodiscard]] bool storage_state_pending(const struct storage_state *_Nonnull state);
[[nodiscard]] const char *_Nonnull storage_state_failure_line(enum storage_outcome outcome,
                                                              enum folio_language language);
void storage_state_destroy(struct storage_state *_Nullable state);
#endif
