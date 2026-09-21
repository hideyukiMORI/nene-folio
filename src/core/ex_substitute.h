/* `:%s/パターン/置換/[g]` を分けた結果（ADR 0028 の決定 8(b)）。位置は
 * folio_command_parse_substitute に渡した文字列の中の開始で、文字列は借りるだけである。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_EX_SUBSTITUTE_H
#define NENEFOLIO_EX_SUBSTITUTE_H

#include <stddef.h>

struct ex_substitute
{
    size_t pattern;
    size_t pattern_length;
    size_t replacement;
    size_t replacement_length;
    bool global; /* `g` があれば全部、無ければ各論理行の最初の一致だけ */
};

#endif
