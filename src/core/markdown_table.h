#ifndef NENEFOLIO_MARKDOWN_TABLE_H
#define NENEFOLIO_MARKDOWN_TABLE_H
#include "markdown_line.h"
#include "markdown_rtf_outcome.h"
#include "markdown_table_format.h"
struct text_buffer;
/* A count above the native limit is returned intact, never truncated. */
[[nodiscard]] size_t markdown_table_columns(struct markdown_line line);
[[nodiscard]] bool markdown_table_delimiter(struct markdown_line line);
[[nodiscard]] bool markdown_table_starts(struct markdown_line line, struct markdown_line delimiter);
/* Product representation range, not a RichEdit implementation maximum. */
[[nodiscard]] bool markdown_table_native(size_t columns, size_t indent_twips);
[[nodiscard]] enum markdown_rtf_outcome markdown_table_row(struct text_buffer *_Nonnull out,
                                                           struct markdown_line line,
                                                           struct markdown_line delimiter,
                                                           struct markdown_table_format format);
#endif
