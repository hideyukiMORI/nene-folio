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
void discovery_text_conditions(const struct folio_state *_Nonnull state, char *_Nonnull out,
                               size_t capacity);
void discovery_text_tag(const struct folio_state *_Nonnull state, size_t index, char *_Nonnull out,
                        size_t capacity);
void discovery_text_preview(const struct folio_state *_Nonnull state, char *_Nonnull out,
                            size_t capacity);
void discovery_text_title(const struct folio_state *_Nonnull state, char *_Nonnull out,
                          size_t capacity);
#endif
