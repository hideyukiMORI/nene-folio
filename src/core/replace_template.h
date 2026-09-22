/* 利用者が打つ置換文字列を解析して所有する（ADR 0028 の決定 5）。ICU の置換文法は使わない
 * （`\n` が `n` になる・`$100` が打てない・末尾の `\` が黙って消える）ので、文法はここが正本。
 *
 * 文法（Vim 寄りの最小）:
 *   `&` と `\0` = 一致の全体 / `\1`〜`\9` = 群（参加しなかった群は空）
 *   `\r` と `\n` = CR（RichEdit の段落区切り） / `\\` = `\` / `\&` = `&` / `\/` = `/`
 *   `$` は普通の文字。それ以外の `\x` と末尾の単独の `\` は MALFORMED で拒む。
 *
 * 解析の結果は「断片（literal の並び）」の列で、各断片の後ろに差し込む群が 0 個か 1 個付く。
 * 組み立てるのは note_replace で、ここは確保をする以外は純粋な字句処理である（ARC-003）。 */
#ifndef NENEFOLIO_REPLACE_TEMPLATE_H
#define NENEFOLIO_REPLACE_TEMPLATE_H

#include "replace_template_outcome.h"

#include <stddef.h>
#include <uchar.h>

struct replace_template;

/* units は呼び出しの間だけ借りる。count が 0 でも空の置換文字列として受ける。 */
[[nodiscard]] enum replace_template_outcome
replace_template_create(const char16_t *_Nonnull units, size_t count,
                        struct replace_template *_Nullable *_Nonnull out);
/* 断片の数。常に 1 以上（最後の断片には群が付かない）。 */
[[nodiscard]] size_t replace_template_count(const struct replace_template *_Nonnull replacement);
/* index 番目の断片が置く文字（終端なし）。replacement が生きている間だけ有効。 */
[[nodiscard]] const char16_t *_Nonnull replace_template_literal(
    const struct replace_template *_Nonnull replacement, size_t index);
[[nodiscard]] size_t
replace_template_literal_length(const struct replace_template *_Nonnull replacement, size_t index);
/* その断片の後ろに差し込む群（0 = 一致の全体・1〜9 = 群）。差し込まないなら false で
 * out は触らない。index は 0 <= index < replace_template_count。 */
[[nodiscard]] bool replace_template_group(const struct replace_template *_Nonnull replacement,
                                          size_t index, size_t *_Nonnull out);
void replace_template_destroy(struct replace_template *_Nullable replacement);

#endif
