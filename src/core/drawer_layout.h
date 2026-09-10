/* ドロワーの索引の配置（FR-003 / FR-004 / FR-012）。台帳と寸法から行の y 座標を決める純関数で、
 * UI は行を描くだけ（ARC-011 / ADR 0002）。展開していないカテゴリのノートは行にならない。
 * カテゴリ行の上には category_gap を空け、行は上から詰めて置く。
 * スクロール量を印として受けたあとは、行の top・hit と drop の y・drop_target の line_y が
 * すべて表示座標（内部の座標から丸めた量を引いたもの）になる。座標の変換はここ 1 か所（ADR 0009）。
 */
#ifndef NENEFOLIO_DRAWER_LAYOUT_H
#define NENEFOLIO_DRAWER_LAYOUT_H

#include "drawer_cursor.h"
#include "drawer_layout_outcome.h"
#include "drawer_metrics.h"
#include "drawer_row.h"
#include "drop_target.h"

#include <stddef.h>

struct category_ledger;
struct drawer_layout;
struct note_ledger;

/* notes はカテゴリと同じ数・同じ順の索引台帳。名前は layout へ複製され、台帳より長く生きてよい。 */
[[nodiscard]] enum drawer_layout_outcome
drawer_layout_create(const struct category_ledger *_Nonnull categories,
                     const struct note_ledger *_Nonnull const *_Nonnull notes,
                     struct drawer_metrics metrics, struct drawer_layout *_Nullable *_Nonnull out);
/* 選択とカーソルの印を付け直す（ADR 0015 の決定 8）。selection が無ければ（nullptr）面の印は
 * どの行にも付かず、cursor が無ければ角の印はどの行にも付かない。カーソルがカテゴリ行なら
 * そのカテゴリ行に付く。該当する行が無ければ（折り畳んだカテゴリの中・索引に無い）何にも付かない。
 */
void drawer_layout_mark(struct drawer_layout *_Nonnull layout,
                        const struct note_ref *_Nullable selection,
                        const struct drawer_cursor *_Nullable cursor);
/* スクロールできる最大の画素数（FR-012）。収まっていれば 0。
 * = max(0, 最後の行の下端 + bottom_padding − viewport_height)。 */
[[nodiscard]] int drawer_layout_scroll_limit(const struct drawer_layout *_Nonnull layout);
/* スクロール量（画素）を 0〜上限に丸めて配置に印として記憶する（drawer_layout_mark と同じ流儀）。
 * 以後の row / hit / drop / line_y はこの丸めた量を引いた表示座標になる。 */
void drawer_layout_scroll(struct drawer_layout *_Nonnull layout, int offset);
/* カーソルの行が頭の帯（top_padding）から下端（viewport_height）までに収まる最小のスクロール量
 * （FR-018 / ADR 0013 の決定 6・ADR 0015 の決定 6）。カテゴリ行のカーソルにも同じように効く。
 * 既に収まっていれば、いま印として持っている量をそのまま返す。
 * その行が無ければ（折り畳んだカテゴリの中・索引に無い）も同じで、量は動かない。 */
[[nodiscard]] int drawer_layout_reveal(const struct drawer_layout *_Nonnull layout,
                                       struct drawer_cursor cursor);
/* 上に隠れている行があるか（丸めた量 > 0）。UI はこの側にだけフェードを描く。 */
[[nodiscard]] bool drawer_layout_overflow_above(const struct drawer_layout *_Nonnull layout);
/* 下に隠れている行があるか（丸めた量 < 上限）。 */
[[nodiscard]] bool drawer_layout_overflow_below(const struct drawer_layout *_Nonnull layout);
[[nodiscard]] size_t drawer_layout_row_count(const struct drawer_layout *_Nonnull layout);
/* index は row_count 未満であること。 */
[[nodiscard]] struct drawer_row drawer_layout_row(const struct drawer_layout *_Nonnull layout,
                                                  size_t index);
/* y 座標にある行の番号。行の外なら false で index は触らない（FR-004 のヒットテスト）。
 * 頭の帯（top_padding より上）は行ではなく、スクロールでそこへ潜った行にも当たらない（ADR 0009）。
 */
[[nodiscard]] bool drawer_layout_hit(const struct drawer_layout *_Nonnull layout, int y,
                                     size_t *_Nonnull index);
/* source_row を掴んで y まで運んだときの落とし先（FR-009 / ADR 0007 の決定 1・ADR 0008 の決定 1）。
 * カテゴリ行なら候補はカテゴリの塊（カテゴリ行と展開中のノート行）の境界で、塊の中点で数える。
 * ノート行なら y のある塊（境界は隣り合う塊の間の中ほど）のカテゴリが移動先で、その塊が展開中なら
 * ノート行の中点で数え、折り畳んでいれば台帳のノート数（末尾）になる。
 * source_row は row_count 未満であること。 */
[[nodiscard]] struct drop_target drawer_layout_drop(const struct drawer_layout *_Nonnull layout,
                                                    size_t source_row, int y);
void drawer_layout_destroy(struct drawer_layout *_Nullable layout);

#endif
