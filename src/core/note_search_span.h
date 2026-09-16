/* 本文の中の範囲（UTF-16 単位・start 以上 end 未満）。探し始める位置（anchor）にも、
 * 見つかった一致にも同じ形を使う。全メンバーが独立に妥当なので完全型で公開する
 * （note_ref と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_NOTE_SEARCH_SPAN_H
#define NENEFOLIO_NOTE_SEARCH_SPAN_H

#include <stddef.h>

struct note_search_span
{
    size_t start;
    size_t end;
};

#endif
