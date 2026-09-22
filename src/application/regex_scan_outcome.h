/* 正規表現の走査の結果（C-005 / ADR 0028 の決定 2）。ICU の `UErrorCode` は adapter が
 * この閉じた集合へ写し、生の数値を上へ出さない（ARC-010）。 */
#ifndef NENEFOLIO_REGEX_SCAN_OUTCOME_H
#define NENEFOLIO_REGEX_SCAN_OUTCOME_H

enum regex_scan_outcome : unsigned char
{
    REGEX_SCAN_READY,
    /* パターンが文法に合わない。位置は regex_pattern_error が持つ */
    REGEX_SCAN_BAD_PATTERN,
    /* 1 回の一致操作が上限の steps を超えた（決定 4(a)）。時計は誰も読んでいない */
    REGEX_SCAN_TIMED_OUT,
    /* 後戻りの山が上限を超えた（U_REGEX_STACK_OVERFLOW） */
    REGEX_SCAN_TOO_COMPLEX,
    /* 一致が regex_match_limit を超えた。走査は打ち切っている（決定 4(b)） */
    REGEX_SCAN_TOO_MANY,
    REGEX_SCAN_OUT_OF_MEMORY
};

#endif
