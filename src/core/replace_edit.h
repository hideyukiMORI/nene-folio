/* 置換 1 回ぶんの結果（ADR 0028 の決定 6）。「本文のこの範囲を、この UTF-16 で置き換える」
 * だけを持ち、RichEdit も選択も知らない。UI は EM_EXSETSEL → EM_REPLACESEL(TRUE) の
 * 1 回で反映するので、Undo も 1 単位になる（決定 7）。
 * 1 件置換なら範囲は一致そのもの、全置換なら本文全体（0〜長さ）と新しい本文である。 */
#ifndef NENEFOLIO_REPLACE_EDIT_H
#define NENEFOLIO_REPLACE_EDIT_H

#include "note_search_span.h"
#include "replace_edit_outcome.h"

#include <stddef.h>
#include <uchar.h>

struct replace_edit;

/* span を置き換える編集を、capacity 単位ぶんの空の入れ物として作る（note_replace が満たす）。 */
[[nodiscard]] enum replace_edit_outcome
replace_edit_create(struct note_search_span span, size_t capacity,
                    struct replace_edit *_Nullable *_Nonnull out);
/* 末尾へ足す。合計が capacity を超える呼び出しは、超えたぶんを捨てる（先に数えてから呼ぶ）。 */
void replace_edit_append(struct replace_edit *_Nonnull edit, const char16_t *_Nonnull units,
                         size_t count);
[[nodiscard]] struct note_search_span replace_edit_span(const struct replace_edit *_Nonnull edit);
/* 終端付き。edit が生きている間だけ有効。 */
[[nodiscard]] const char16_t *_Nonnull replace_edit_units(const struct replace_edit *_Nonnull edit);
[[nodiscard]] size_t replace_edit_length(const struct replace_edit *_Nonnull edit);
void replace_edit_destroy(struct replace_edit *_Nullable edit);

#endif
