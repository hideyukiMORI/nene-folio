#ifndef NENEFOLIO_DISCOVERY_TEXT_H
#define NENEFOLIO_DISCOVERY_TEXT_H
#include <stddef.h>
struct folio_state;
/* Presentation only; no retained body, result list or search state. */
void discovery_text_row(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                        size_t capacity);
void discovery_text_match(const struct folio_state *_Nonnull state, size_t index,
                          char *_Nonnull out, size_t capacity);
void discovery_text_name(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                         size_t capacity);
void discovery_text_status(const struct folio_state *_Nonnull state, char *_Nonnull out,
                           size_t capacity);
/* 読込件数の表示を描き直す件数か。時計を読まず、件数だけで決める（#844）。 */
[[nodiscard]] bool discovery_text_progress_shown(size_t checked, size_t total);
void discovery_text_conditions(const struct folio_state *_Nonnull state, char *_Nonnull out,
                               size_t capacity);
void discovery_text_tag(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                        size_t capacity);
void discovery_text_preview(const struct folio_state *_Nonnull state, char *_Nonnull out,
                            size_t capacity);
void discovery_text_title(const struct folio_state *_Nonnull state, char *_Nonnull out,
                          size_t capacity);
#endif
