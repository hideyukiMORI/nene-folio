#ifndef NENEFOLIO_TAG_FILTER_TEXT_H
#define NENEFOLIO_TAG_FILTER_TEXT_H
#include <stddef.h>
struct folio_state;
/* Drawer paint and its accessible name share the same selected-count/mode label. */
void tag_filter_text_format(const struct folio_state *_Nonnull state, char *_Nonnull out,
                            size_t capacity);
#endif
