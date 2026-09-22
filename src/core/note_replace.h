/* 本文と一致の列と置換文字列から、新しい本文を組み立てる純関数（ADR 0028 の決定 5）。
 * ICU の replaceAll / appendReplacement は使わない。数えるのも書くのもここである。
 * 「必要長を数える → 確保する → 書く」の 2 段を中で通し、確保の前に上限で断る（決定 4(c)）。
 * 一致の選び方（1 件・全部・各論理行の最初の 1 件）は replace_scope が決める。 */
#ifndef NENEFOLIO_NOTE_REPLACE_H
#define NENEFOLIO_NOTE_REPLACE_H

#include "note_replace_outcome.h"
#include "note_replace_plan.h"

#include <stddef.h>

struct replace_edit;

/* 出力の上限（UTF-16 のコード単位・約 8MB・決定 4(c)）。組み立ての前に数えて超えれば
 * TOO_LARGE で何も作らない。 */
constexpr size_t note_replace_limit = 4000000;

/* 置き換えの結果を 1 つ作る。呼び出し側が replace_edit_destroy する。
 * 一致が無ければ NOT_FOUND で out は触らない（本文は変えない）。 */
[[nodiscard]] enum note_replace_outcome
note_replace_build(const struct note_replace_plan *_Nonnull plan,
                   struct replace_edit *_Nullable *_Nonnull out);

#endif
