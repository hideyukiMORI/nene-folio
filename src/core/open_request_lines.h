#ifndef NENEFOLIO_OPEN_REQUEST_LINES_H
#define NENEFOLIO_OPEN_REQUEST_LINES_H
#include <stddef.h>
/* 原文（Markdown のファイル）の行の範囲。1 始まりで、first <= last。 */
struct open_request_lines
{
    size_t first;
    size_t last;
};
#endif
