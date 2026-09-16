/* 絞り込みが読む「本文の写し」の置き場（FR-032 / ADR 0024 の決定 1）。
 * 写しはカテゴリ名とノート名で引く。ポートが md を指すのと同じ宛先なので、並び替えでは何も動かず、
 * 名前が変わる操作（改名・別のカテゴリへの移動）だけが写しを付け替える。
 * 本文は複製して所有し、元の note_text より長く生きてよい（ARC-004 / C-003）。
 * 写しが無いノート（まだ読んでいない・読めなかった）は index_filter が見えない扱いにする。 */
#ifndef NENEFOLIO_NOTE_CORPUS_H
#define NENEFOLIO_NOTE_CORPUS_H

#include "note_corpus_outcome.h"

struct note_corpus;
struct note_text;

[[nodiscard]] enum note_corpus_outcome
note_corpus_create(struct note_corpus *_Nullable *_Nonnull out);
/* その宛先の写しを body の複製で置き換える（無ければ足す）。名前は呼び出しの間だけ借りる。 */
[[nodiscard]] enum note_corpus_outcome note_corpus_put(struct note_corpus *_Nonnull corpus,
                                                       const char *_Nonnull category,
                                                       const char *_Nonnull note,
                                                       const struct note_text *_Nonnull body);
/* 同じカテゴリの中で写しの名前を付け替える（改名）。写しが無ければ何もせず ACCEPTED。 */
[[nodiscard]] enum note_corpus_outcome note_corpus_rename(struct note_corpus *_Nonnull corpus,
                                                          const char *_Nonnull category,
                                                          const char *_Nonnull from,
                                                          const char *_Nonnull to);
/* 同じ名前のまま写しを別のカテゴリへ移す（移動）。写しが無ければ何もせず ACCEPTED。 */
[[nodiscard]] enum note_corpus_outcome note_corpus_relocate(struct note_corpus *_Nonnull corpus,
                                                            const char *_Nonnull from_category,
                                                            const char *_Nonnull note,
                                                            const char *_Nonnull to_category);
/* その宛先の写し。無ければ nullptr（省略可能な値が無い・C-004）。次の put まで有効。 */
[[nodiscard]] const struct note_text *_Nullable note_corpus_body(
    const struct note_corpus *_Nonnull corpus, const char *_Nonnull category,
    const char *_Nonnull note);
void note_corpus_destroy(struct note_corpus *_Nullable corpus);

#endif
