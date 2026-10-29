#ifndef NENEFOLIO_MARKDOWN_TABLE_FORMAT_H
#define NENEFOLIO_MARKDOWN_TABLE_FORMAT_H
#include "markdown_row_kind.h"
#include <stddef.h>

/* Borrowed display description, validated by the native table boundary. */
struct markdown_table_format
{
    enum markdown_row_kind kind;
    size_t indent_twips;
};
#endif
