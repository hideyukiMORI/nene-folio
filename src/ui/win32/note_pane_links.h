#ifndef NENEFOLIO_NOTE_PANE_LINKS_H
#define NENEFOLIO_NOTE_PANE_LINKS_H
#include "note_pane_link_outcome.h"
#include "note_search_span.h"
#include <uchar.h>
#include <windows.h>

#include <richedit.h>
#include <tom.h>
struct note_pane_links;
struct utf8_text;
[[nodiscard]] enum note_pane_link_outcome
note_pane_links_create(ITextDocument *_Nonnull document,
                       struct note_pane_links *_Nullable *_Nonnull out);
void note_pane_links_destroy(struct note_pane_links *_Nullable links);
[[nodiscard]] size_t note_pane_links_visible(const struct note_pane_links *_Nullable links,
                                             size_t native);
[[nodiscard]] CHARRANGE note_pane_links_native(const struct note_pane_links *_Nullable links,
                                               struct note_search_span span);
[[nodiscard]] size_t note_pane_links_compact(const struct note_pane_links *_Nonnull links,
                                             char16_t *_Nonnull units, size_t length);
[[nodiscard]] enum note_pane_link_outcome
note_pane_links_target(ITextDocument *_Nonnull document, CHARRANGE selection,
                       struct utf8_text *_Nullable *_Nonnull out);
void note_pane_links_input(struct note_pane_links *_Nullable links, UINT message, LPARAM position);
void note_pane_links_finish_input(struct note_pane_links *_Nullable links, UINT message);
[[nodiscard]] bool note_pane_links_clicked(struct note_pane_links *_Nonnull links,
                                           HWND _Nonnull window, const ENLINK *_Nonnull notice);
#endif
