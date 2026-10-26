#ifndef NENEFOLIO_MARKDOWN_LIST_H
#define NENEFOLIO_MARKDOWN_LIST_H
#include "markdown_line.h"
#include "markdown_rtf_outcome.h"
struct markdown_list;
[[nodiscard]] enum markdown_rtf_outcome
markdown_list_create(struct markdown_list *_Nullable parent, struct markdown_line line,
                     struct markdown_list *_Nullable *_Nonnull out);
void markdown_list_destroy(struct markdown_list *_Nullable list);
void markdown_list_pop(struct markdown_list *_Nullable *_Nonnull list);
void markdown_list_advance(struct markdown_list *_Nonnull list);
[[nodiscard]] bool markdown_list_same(const struct markdown_list *_Nonnull list,
                                      struct markdown_line line);
[[nodiscard]] size_t markdown_list_indent(const struct markdown_list *_Nonnull list);
[[nodiscard]] size_t markdown_list_content_indent(const struct markdown_list *_Nonnull list);
[[nodiscard]] size_t markdown_list_number(const struct markdown_list *_Nonnull list);
[[nodiscard]] size_t markdown_list_depth(const struct markdown_list *_Nonnull list);
[[nodiscard]] char markdown_list_delimiter(const struct markdown_list *_Nonnull list);
[[nodiscard]] bool markdown_list_ordered(const struct markdown_list *_Nonnull list);
#endif
