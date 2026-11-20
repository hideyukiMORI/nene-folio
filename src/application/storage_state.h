#ifndef NENEFOLIO_STORAGE_STATE_H
#define NENEFOLIO_STORAGE_STATE_H
#include "folio_language.h"
#include "storage_outcome.h"
#include "storage_phase.h"
#include "window_placement.h"
#include <stddef.h>
struct storage_state;
struct storage_port;
struct storage_path;
[[nodiscard]] enum storage_outcome
storage_state_create(const struct storage_port *_Nonnull port,
                     struct storage_state *_Nullable *_Nonnull out);
/* #1071 / ADR0118: この起動が使うはずの data の場所だけを引く（起動設定があればその場所、無ければ
 * 既定。場所を確かめも作りもしない）。2つ目の起動が、同じ data で動いている窓を探すために使う。
 * 起動設定が読めなければその結果で、out は変えない。成功したら呼び出し側が storage_path_destroy。
 */
[[nodiscard]] enum storage_outcome
storage_state_configured_directory(const struct storage_port *_Nonnull port,
                                   struct storage_path *_Nullable *_Nonnull out);
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
/* #891 / ADR0108: the remembered window place of this machine. The value is what the file
 * held at start (or the default), then what this run chose or recorded. */
[[nodiscard]] struct window_placement
storage_state_window(const struct storage_state *_Nonnull state);
/* The user's choice; written at once. On a failed write the choice in memory is unchanged. */
[[nodiscard]] enum storage_outcome
storage_state_set_window_restore(struct storage_state *_Nonnull state, bool restore);
/* The place the window had when it closed. Nothing is written while the choice is off. */
[[nodiscard]] enum storage_outcome
storage_state_record_window(struct storage_state *_Nonnull state,
                            const struct window_placement *_Nonnull recorded);
void storage_state_destroy(struct storage_state *_Nullable state);
#endif
