#ifndef NENEFOLIO_COMMAND_CANDIDATES_H
#define NENEFOLIO_COMMAND_CANDIDATES_H

#include "candidate_create_outcome.h"
#include "command_candidate_kind.h"
#include "folio_command.h"
#include "ui_text.h"
#include <stdint.h>

struct command_candidates;
struct utf8_text;

/* UI ownerの派生資源。選択と公開参照の世代はfolio_windowが所有する（ADR0065）。 */
[[nodiscard]] enum candidate_create_outcome
command_candidates_create(struct command_candidates *_Nullable *_Nonnull out);
void command_candidates_destroy(struct command_candidates *_Nullable candidates);
void command_candidates_invalidate(struct command_candidates *_Nonnull candidates);
[[nodiscard]] bool command_candidates_current(const struct command_candidates *_Nonnull candidates,
                                              enum folio_language language, uint64_t revision);
void command_candidates_prepare(struct command_candidates *_Nonnull candidates,
                                const struct utf8_text *_Nonnull query,
                                enum folio_language language, uint64_t revision);
[[nodiscard]] size_t command_candidates_count(const struct command_candidates *_Nonnull candidates);
[[nodiscard]] size_t
command_candidates_child_count(const struct command_candidates *_Nonnull candidates);
[[nodiscard]] bool command_candidates_at(const struct command_candidates *_Nonnull candidates,
                                         size_t index, enum folio_command *_Nonnull out);
[[nodiscard]] bool command_candidates_ordinal(const struct command_candidates *_Nonnull candidates,
                                              enum command_candidate_kind kind, size_t slot,
                                              size_t *_Nonnull out);
[[nodiscard]] bool command_candidates_child(const struct command_candidates *_Nonnull candidates,
                                            size_t index,
                                            enum command_candidate_kind *_Nonnull kind,
                                            size_t *_Nonnull slot);
[[nodiscard]] size_t command_candidates_group_count(void);
[[nodiscard]] size_t command_candidates_group_index(enum folio_command command);
/* groupは0以上group_count未満。 */
[[nodiscard]] enum ui_text command_candidates_group_label(size_t group);

#endif
