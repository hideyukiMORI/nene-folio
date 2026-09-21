/* 下見を実際に当てるときに渡すもの（ADR 0028 の決定 6）。いまの本文とその長さ・anchor（選択）・
 * 適用する一致の選び方を束ねる。本文は下見の写しと照合され、違えば REPLACE_STALE になる。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_REPLACE_APPLY_H
#define NENEFOLIO_REPLACE_APPLY_H

#include "note_search_span.h"
#include "replace_scope.h"

#include <stddef.h>
#include <uchar.h>

struct replace_apply
{
    const char16_t *_Nonnull text;
    size_t length;
    struct note_search_span anchor; /* REPLACE_ONE の探し始め。反転していれば拒む */
    enum replace_scope scope;
};

#endif
