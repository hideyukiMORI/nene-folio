#ifndef NENEFOLIO_MARKDOWN_TABLE_H
#define NENEFOLIO_MARKDOWN_TABLE_H
#include "markdown_line.h"
#include "markdown_row_kind.h"
#include "markdown_rtf_outcome.h"
struct text_buffer;
/* A count above the native limit is returned intact, never truncated. */
[[nodiscard]] size_t markdown_table_columns(struct markdown_line line);
[[nodiscard]] bool markdown_table_delimiter(struct markdown_line line);
[[nodiscard]] enum markdown_rtf_outcome markdown_table_row(struct text_buffer *_Nonnull out,
                                                           struct markdown_line line,
                                                           struct markdown_line delimiter,
                                                           enum markdown_row_kind kind);
#endif
