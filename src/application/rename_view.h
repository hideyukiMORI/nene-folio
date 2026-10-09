/* 未完了の改名の表示値（ARC-011 / ADR 0022 の決定 2）。
 * 文字列は folio_state が所有し、次の意図まで有効。表示文言は core が持つ。 */
#ifndef NENEFOLIO_RENAME_VIEW_H
#define NENEFOLIO_RENAME_VIEW_H

#include "rename_kind.h"

struct rename_view
{
    enum rename_kind kind;
    const char *_Nonnull from; /* 終端付き UTF-8。元名であり、実体の現存は保証しない */
    const char *_Nonnull to;   /* 終端付き UTF-8。移し終えたい名前 */
};

#endif
