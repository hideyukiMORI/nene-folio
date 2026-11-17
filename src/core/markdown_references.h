#ifndef NENEFOLIO_MARKDOWN_REFERENCES_H
#define NENEFOLIO_MARKDOWN_REFERENCES_H
#include "markdown_rtf_outcome.h"
#include "markdown_span.h"
#include "markdown_view.h"
#include "note_search_span.h"
#include "note_text.h"
/* #861 / ADR0101: the link reference definitions of one note, found once before rendering.
 * Owns normalized labels and decoded destinations; borrows nothing after creation. */
struct markdown_references;
/* The longest label a definition may have (CommonMark: 999 characters, counted in bytes here). */
constexpr size_t markdown_references_label_limit = 999;
/* At most this many definitions are kept; later ones are ignored. */
constexpr size_t markdown_references_limit = 1024;
[[nodiscard]] enum markdown_rtf_outcome
markdown_references_create(const struct note_text *_Nonnull text,
                           struct markdown_references *_Nullable *_Nonnull out);
/* True when the line starting at `offset` of the source is a definition the table holds. */
[[nodiscard]] bool markdown_references_defines(const struct markdown_references *_Nullable table,
                                               size_t offset);
/* The definition whose label matches the view's [start, end), or SIZE_MAX. */
[[nodiscard]] size_t markdown_references_find(const struct markdown_references *_Nullable table,
                                              struct markdown_view view, size_t start, size_t end);
[[nodiscard]] const char *_Nonnull markdown_references_destination(
    const struct markdown_references *_Nonnull table, size_t index);
[[nodiscard]] size_t
markdown_references_destination_length(const struct markdown_references *_Nonnull table,
                                       size_t index);
/* #863: the definition's line as a span of the note's source (borrowed for the render), and
 * where its destination is inside that line, for the image path to read and record. */
[[nodiscard]] struct markdown_span
markdown_references_line(const struct markdown_references *_Nonnull table, size_t index);
[[nodiscard]] struct note_search_span
markdown_references_destination_span(const struct markdown_references *_Nonnull table,
                                     size_t index);
/* #869 / ADR0103: footnotes are one-line definitions whose label starts with `^`; a link
 * reference never names them. markdown_references_line / _destination_span give the text. */
[[nodiscard]] size_t markdown_references_footnote(const struct markdown_references *_Nullable table,
                                                  struct markdown_view view, size_t start,
                                                  size_t end);
/* The footnote's number, given in the order the note first refers to its footnotes. */
[[nodiscard]] size_t markdown_references_number(struct markdown_references *_Nonnull table,
                                                size_t index);
[[nodiscard]] size_t
markdown_references_numbered(const struct markdown_references *_Nullable table);
/* The definition that has number `rank + 1`. */
[[nodiscard]] size_t
markdown_references_numbered_at(const struct markdown_references *_Nonnull table, size_t rank);
void markdown_references_destroy(struct markdown_references *_Nullable table);
#endif
