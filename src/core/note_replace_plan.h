/* 1 回の置換で見るもの（ADR 0028 の決定 5 / 6）。本文・一致の列・置換文字列・選び方・anchor を
 * 束ねる（C-012 の引数 4 つの上限。ADR 0023 の 7 引数の失敗を繰り返さない）。
 * どれも借りるだけで所有しない。全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_NOTE_REPLACE_PLAN_H
#define NENEFOLIO_NOTE_REPLACE_PLAN_H

#include "note_search_span.h"
#include "replace_scope.h"

#include <stddef.h>
#include <uchar.h>

struct regex_matches;
struct replace_template;

struct note_replace_plan
{
    const char16_t *_Nonnull text;
    size_t length;
    const struct regex_matches *_Nonnull matches;
    const struct replace_template *_Nonnull replacement;
    enum replace_scope scope;
    struct note_search_span anchor; /* REPLACE_ONE のときだけ見る（決定 6） */
};

#endif
