/* 置換の下見（ADR 0028 の決定 6）。本文の**UTF-16 の写し**・一致の列と件数・置換文字列の
 * 解析結果・宛先を 1 つにまとめて所有する。application はこれを 1 つだけ持つ。
 *
 * ここだけ UTF-8 へ写さないのは **C-014 の名指しの例外**である: 位置が RichEdit の
 * EM_EXSETSEL と 1 対 1 でなければならないため（決定 6）。
 *
 * 「捨てる契機」は無い。渡された本文か宛先が写しと違えば replace_preview_holds が偽になるので、
 * 古い下見は構造的に使えない（保存・切替・改名・移動の 10 か所以上を数え上げない）。 */
#ifndef NENEFOLIO_REPLACE_PREVIEW_H
#define NENEFOLIO_REPLACE_PREVIEW_H

#include "note_replace_plan.h"
#include "note_search_span.h"
#include "replace_preview_outcome.h"
#include "replace_scope.h"
#include "replace_source.h"

#include <stddef.h>

struct regex_matches;
struct replace_preview;

/* source は複製して所有し、matches の配列は**写さずに引き取る**（ADR 0028 の補正 25）。
 * 所有権: READY のときだけ matches->items（malloc 系で確保したもの）は下見のものになり、
 * matches の capacity と count は 0 に戻る。呼び出し側は自分の手元の同じポインタを捨てる。
 * READY 以外（BAD_TEMPLATE・OUT_OF_MEMORY）では何も引き取らず、matches はそのまま残る。
 * 置換文字列はここで解析するので、文法が壊れていれば BAD_TEMPLATE で何も作らない。
 * count が capacity を超えていても総数のまま残し、組み立て（note_replace_build）が断る。 */
[[nodiscard]] enum replace_preview_outcome
replace_preview_create(const struct replace_source *_Nonnull source,
                       struct regex_matches *_Nonnull matches,
                       struct replace_preview *_Nullable *_Nonnull out);
/* 一致の総数（欄に出す「k 件」）。ゼロ幅の一致も 1 件。 */
[[nodiscard]] size_t replace_preview_count(const struct replace_preview *_Nonnull preview);
/* 渡された**本文と宛先**が写しと同じか（決定 6 の照合）。置換文字列は見ない。 */
[[nodiscard]] bool replace_preview_holds(const struct replace_preview *_Nonnull preview,
                                         const struct replace_source *_Nonnull source);
/* いまの下見から組み立ての計画を作る。本文も一致の列も置換文字列も下見が持つものを指すので、
 * 計画は下見より長く生きない。 */
[[nodiscard]] struct note_replace_plan
replace_preview_plan(const struct replace_preview *_Nonnull preview, enum replace_scope scope,
                     struct note_search_span anchor);
void replace_preview_destroy(struct replace_preview *_Nullable preview);

#endif
