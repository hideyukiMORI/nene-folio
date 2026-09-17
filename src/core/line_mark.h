/* 文字位置がどの論理行にあるか（ADR 0026 の決定 2 の (b)）。
 * 全メンバーが独立に妥当なので完全型で公開する（note_search_span と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_LINE_MARK_H
#define NENEFOLIO_LINE_MARK_H

#include <stddef.h>

struct line_mark
{
    size_t number; /* 1 始まりの論理行番号 */
    bool first;    /* その位置が論理行の先頭か（真のときだけ番号を描く） */
};

#endif
