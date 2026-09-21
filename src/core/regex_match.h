/* 正規表現の一致 1 件の位置（UTF-16 単位・ADR 0028 の決定 2）。全体と群 1〜9 を持つ。
 * 全メンバーが独立に妥当な値の集まりなので完全型で公開する（note_search_span と同じ
 * C-003 の例外）。位置は RichEdit の EM_EXSETSEL と 1 対 1 で、変換も補正も要らない。
 * 10 個以上の群を持つパターンは受けるが、10 番目以降は報告しない（`\1`〜`\9` だけが参照できる）。
 */
#ifndef NENEFOLIO_REGEX_MATCH_H
#define NENEFOLIO_REGEX_MATCH_H

#include "note_search_span.h"

#include <stddef.h>

/* 置換文字列から参照できる群の数（決定 2 / 5）。 */
constexpr size_t regex_match_groups = 9;

struct regex_match
{
    struct note_search_span whole;
    /* 群 1 が groups[0]。参加しなかった群は present が偽で、span は使わない。 */
    struct
    {
        struct note_search_span span;
        bool present;
    } groups[regex_match_groups];
};

#endif
