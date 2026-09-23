/* 履歴の一覧の 1 行の表示値（ARC-011 / ADR 0038 の決定 8）。文字列は folio_state が所有し、
 * 次の意図まで有効。「（読めません）」などの文言は UI 側が ui_text から引く（C-018）。 */
#ifndef NENEFOLIO_HISTORY_ROW_VIEW_H
#define NENEFOLIO_HISTORY_ROW_VIEW_H

#include <stddef.h>

struct history_row_view
{
    size_t version; /* 1 が最新。歯抜けのときは飛んだ番号のまま */
    /* 本文の最初の空でない論理行（UTF-8・終端なし）。読めない版と空の本文では長さ 0 */
    const char *_Nonnull first_line;
    size_t first_line_length;
    bool readable; /* 偽なら本文を持たず、戻せない */
};

#endif
