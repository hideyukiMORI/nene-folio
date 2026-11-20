#ifndef NENEFOLIO_OPEN_REQUEST_LINES_H
#define NENEFOLIO_OPEN_REQUEST_LINES_H
#include <stddef.h>
/* 原文（Markdown のファイル）の行の範囲。1 始まりで、first <= last。
 * 値の構造体（C-003 の例外。note_byte_span と同じ）: open_request が返すものは必ずこの形で、
 * 受け取る側の初期値 {0, 0} は「まだ無い」を表すだけで、どこからも行として使わない。 */
struct open_request_lines
{
    size_t first;
    size_t last;
};
#endif
