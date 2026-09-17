/* md 原文の論理行（改行区切り）の数え上げ（FR-021 / ADR 0026 の決定 2）。
 * 対象は RichEdit が今表示している平文で、段落区切りは CR 1 つに揃っている（ADR 0023 の決定 1）。
 * 位置は UTF-16 単位で、EM_LINEINDEX / EM_EXSETSEL の位置と 1 対 1 である
 * （core が UTF-16 を受ける note_search と同じ理由・C-014 の境界）。
 * 本文は借りるだけで所有しない。作った表は本文が変わるまで有効で、所有者は note_pane。 */
#ifndef NENEFOLIO_LINE_INDEX_H
#define NENEFOLIO_LINE_INDEX_H

#include "line_index_outcome.h"
#include "line_mark.h"

#include <stddef.h>
#include <uchar.h>

struct line_index;

/* 本文から CR の位置の昇順の表を作る。空の本文は 1 行、末尾が CR なら空の最終行も 1 行と数える。
 * units は呼び出しの間だけ借り、表は複製しない。 */
[[nodiscard]] enum line_index_outcome line_index_create(const char16_t *_Nonnull units,
                                                        size_t count,
                                                        struct line_index *_Nullable *_Nonnull out);
/* 論理行の数（1 以上）。 */
[[nodiscard]] size_t line_index_count(const struct line_index *_Nonnull index);
/* 文字位置の論理行番号と、そこが論理行の先頭かどうか。position は 0 以上・本文の長さ以下。
 * 外なら OUT_OF_RANGE で out は触らない。 */
[[nodiscard]] enum line_index_outcome line_index_at(const struct line_index *_Nonnull index,
                                                    size_t position,
                                                    struct line_mark *_Nonnull out);
/* 論理行の先頭の文字位置。number は 1 以上・行数以下。外なら OUT_OF_RANGE で out は触らない。 */
[[nodiscard]] enum line_index_outcome line_index_start(const struct line_index *_Nonnull index,
                                                       size_t number, size_t *_Nonnull out);
/* 番号の帯の幅を決める桁数。最小 3 桁（決定 2 の (d)）。 */
[[nodiscard]] size_t line_index_digits(const struct line_index *_Nonnull index);
void line_index_destroy(struct line_index *_Nullable index);

#endif
