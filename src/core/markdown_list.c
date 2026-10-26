#include "markdown_list.h"
#include <stdlib.h>
struct markdown_list
{
    struct markdown_list *_Nullable parent;
    size_t indent;
    size_t content_indent;
    size_t number;
    size_t depth;
    char delimiter;
    bool ordered;
};
enum markdown_rtf_outcome markdown_list_create(struct markdown_list *_Nullable parent,
                                               struct markdown_line line,
                                               struct markdown_list *_Nullable *_Nonnull out)
{
    if (!markdown_line_valid(line) || line.marker == 0)
        return MARKDOWN_RTF_INVALID_INPUT;
    struct markdown_list *list = calloc(1, sizeof *list);
    if (list == nullptr)
        return MARKDOWN_RTF_OUT_OF_MEMORY;
    *list = (struct markdown_list){.parent = parent,
                                   .indent = line.indent,
                                   .content_indent = line.indent + line.marker,
                                   .number = line.number,
                                   .depth = parent == nullptr ? 1 : parent->depth + 1,
                                   .delimiter = line.delimiter,
                                   .ordered = line.ordered};
    *out = list;
    return MARKDOWN_RTF_CONVERTED;
}
void markdown_list_destroy(struct markdown_list *_Nullable list)
{
    while (list != nullptr)
    {
        struct markdown_list *parent = list->parent;
        free(list);
        list = parent;
    }
}
void markdown_list_pop(struct markdown_list *_Nullable *_Nonnull list)
{
    if (*list == nullptr)
        return;
    struct markdown_list *removed = *list;
    *list = removed->parent;
    removed->parent = nullptr;
    markdown_list_destroy(removed);
}
void markdown_list_advance(struct markdown_list *_Nonnull list)
{
    ++list->number;
}
bool markdown_list_same(const struct markdown_list *_Nonnull list, struct markdown_line line)
{
    return list->ordered == line.ordered && list->delimiter == line.delimiter;
}
size_t markdown_list_indent(const struct markdown_list *_Nonnull list)
{
    return list->indent;
}
size_t markdown_list_content_indent(const struct markdown_list *_Nonnull list)
{
    return list->content_indent;
}
size_t markdown_list_number(const struct markdown_list *_Nonnull list)
{
    return list->number;
}
size_t markdown_list_depth(const struct markdown_list *_Nonnull list)
{
    return list->depth;
}
char markdown_list_delimiter(const struct markdown_list *_Nonnull list)
{
    return list->delimiter;
}
bool markdown_list_ordered(const struct markdown_list *_Nonnull list)
{
    return list->ordered;
}
