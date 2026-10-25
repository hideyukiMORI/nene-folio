#ifndef NENEFOLIO_THEME_CANDIDATES_H
#define NENEFOLIO_THEME_CANDIDATES_H

#include "candidate_create_outcome.h"
#include "folio_language.h"
#include "folio_theme_choice.h"
#include <stdint.h>

struct theme_candidates;
struct utf8_text;

[[nodiscard]] enum candidate_create_outcome
theme_candidates_create(struct theme_candidates *_Nullable *_Nonnull out);
void theme_candidates_destroy(struct theme_candidates *_Nullable candidates);
void theme_candidates_invalidate(struct theme_candidates *_Nonnull candidates);
[[nodiscard]] bool theme_candidates_current(const struct theme_candidates *_Nonnull candidates,
                                            enum folio_language language, uint64_t revision);
void theme_candidates_prepare(struct theme_candidates *_Nonnull candidates,
                              const struct utf8_text *_Nonnull query, enum folio_language language,
                              uint64_t revision);
[[nodiscard]] size_t theme_candidates_count(const struct theme_candidates *_Nonnull candidates);
[[nodiscard]] bool theme_candidates_at(const struct theme_candidates *_Nonnull candidates,
                                       size_t index, enum folio_theme_choice *_Nonnull out);

#endif
