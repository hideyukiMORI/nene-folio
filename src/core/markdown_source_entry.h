#ifndef NENEFOLIO_MARKDOWN_SOURCE_ENTRY_H
#define NENEFOLIO_MARKDOWN_SOURCE_ENTRY_H
#include "markdown_source_kind.h"
#include <stddef.h>
/* #1008: one block of the reading view and where it came from. `first` and `marks` count the
 * paragraph marks (CR and U+0007 in the RichEdit text) the block makes; `begin` and `end` are
 * byte offsets of whole source lines (container prefixes included, the final newline not);
 * `caret` is where its content starts; blocks of one fence or one table share a `group`. */
struct markdown_source_entry
{
    size_t rtf;
    size_t first;
    size_t marks;
    size_t begin;
    size_t end;
    size_t caret;
    enum markdown_source_kind kind;
    size_t group;
};
#endif
