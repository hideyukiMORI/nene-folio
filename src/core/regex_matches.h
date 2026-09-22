/* 走査が書き込む一致の列（ADR 0028 の決定 2）。呼び出し側が items と capacity を用意し、
 * adapter は capacity までを書いて count に**総数**を書く（capacity を超える一致は数えるだけ）。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_REGEX_MATCHES_H
#define NENEFOLIO_REGEX_MATCHES_H

#include "regex_match.h"

#include <stddef.h>

/* 一致の総数の上限（決定 4(b)）。これを超えたら走査を打ち切って TOO_MANY で断る。
 * 上限は core が持ち、ICU の steps（adapter の定数）とは別の話である。 */
constexpr size_t regex_match_limit = 1000000;

struct regex_matches
{
    struct regex_match *_Nonnull items;
    size_t capacity;
    size_t count;
};

#endif
