/* 1 回の走査で見る本文とパターン（ADR 0028 の決定 2）。どちらも借りるだけで所有しない。
 * 本文は長さ 0 でも実体のある番地であること（ICU は NULL を拒む・probe の §8）。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_REGEX_REQUEST_H
#define NENEFOLIO_REGEX_REQUEST_H

#include <stddef.h>
#include <uchar.h>

struct regex_request
{
    const char16_t *_Nonnull text;
    size_t length;
    const char16_t *_Nonnull pattern;
    size_t pattern_length;
};

#endif
