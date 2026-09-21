/* 壊れたパターンの位置（ADR 0028 の決定 2）。REGEX_SCAN_BAD_PATTERN のときだけ意味を持つ。
 * 全メンバーが独立に妥当なので完全型で公開する（C-003 の例外）。 */
#ifndef NENEFOLIO_REGEX_PATTERN_ERROR_H
#define NENEFOLIO_REGEX_PATTERN_ERROR_H

#include <stddef.h>

struct regex_pattern_error
{
    /* 1 起算の UTF-16 コード単位。分からなければ 0（欄には位置を添えない） */
    size_t offset;
};

#endif
