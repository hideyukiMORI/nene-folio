/* 現在表示中のノート 1 つの中を探す判断（FR-011 / ADR 0023 の決定 2）。
 * 対象は RichEdit が今表示している平文で、位置は UTF-16 単位の EM_EXSETSEL の位置と 1 対 1。
 * 一致は UTF-16 単位の順序比較で、**ASCII の英字だけ大小を無視**する
 * （全角／半角・かな／カナ・合成済みと分解は区別する。正規表現は #41 の範囲）。
 * 確保をしない純関数で、本文も語も所有しない（ARC-001 / ARC-011）。 */
#ifndef NENEFOLIO_NOTE_SEARCH_H
#define NENEFOLIO_NOTE_SEARCH_H

#include "note_search_outcome.h"
#include "note_search_query.h"
#include "note_search_span.h"
#include "search_direction.h"

#include <stddef.h>

/* anchor から次の一致を探す。前方なら anchor.end 以降、後方なら anchor.start より前から探し、
 * 端に達したら反対の端から続けて（巡回）本文全体をちょうど 1 周見る。
 * 見つかれば out に一致の範囲（end = start + 語の長さ）を書いて FOUND。
 * 語が空なら NO_TERM、本文か語が整形式の UTF-16 でなければ MALFORMED、
 * 一致が無ければ NOT_FOUND で、いずれも out は触らない。
 * 本文と語が整形式なら、一致がサロゲート対を跨いで始まったり終わったりすることはない。 */
[[nodiscard]] enum note_search_outcome
note_search_next(const struct note_search_query *_Nonnull query, struct note_search_span anchor,
                 enum search_direction direction, struct note_search_span *_Nonnull out);
/* 本文にある一致の総数と、position から始まる一致が何番目かを数える（欄の「k / n 件」）。
 * 一致は重なりも数える（"aa" は "aaa" の中に 2 つ）。total には総数、ordinal には
 * position 以下から始まる一致の数（position が一致の開始位置なら、その 1 始まりの順番）を書く。
 * 一致が無ければ両方 0 で NOT_FOUND。語が空なら NO_TERM、整形式でなければ MALFORMED で、
 * どちらも total も ordinal も触らない。 */
[[nodiscard]] enum note_search_outcome
note_search_count(const struct note_search_query *_Nonnull query, size_t position,
                  size_t *_Nonnull total, size_t *_Nonnull ordinal);

#endif
