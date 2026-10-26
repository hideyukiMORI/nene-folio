#ifndef NENEFOLIO_MARKDOWN_LINE_H
#define NENEFOLIO_MARKDOWN_LINE_H
#include <stdbool.h>
#include <stddef.h>
/* Borrowed source query. All consumers validate the byte/content bounds. */
struct markdown_line
{
    const char *_Nonnull bytes;
    size_t length;
    size_t next;
    size_t quote;
    size_t indent;
    size_t content;
    size_t prefix;
    size_t marker;
    size_t number;
    char delimiter;
    bool ordered;
};
[[nodiscard]] bool markdown_line_valid(struct markdown_line line);
[[nodiscard]] struct markdown_line markdown_line_read(const char *_Nonnull source, size_t length,
                                                      size_t start);
[[nodiscard]] bool markdown_line_blank(struct markdown_line line);
[[nodiscard]] size_t markdown_line_setext(struct markdown_line line);
[[nodiscard]] bool markdown_line_thematic(struct markdown_line line);
[[nodiscard]] size_t markdown_line_fence(struct markdown_line line);
[[nodiscard]] size_t markdown_line_heading(struct markdown_line line);
void markdown_line_list(struct markdown_line *_Nonnull line);
#endif
