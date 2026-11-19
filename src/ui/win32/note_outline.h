/* #975 / ADR0114: 閲覧表示の横の「見出し」の欄。行の配置・当たり判定・描画と、欄だけが持つ状態
 * （欄の中の送り位置、キーボードの位置、ポインタの乗っている行、押している行）と、観測した
 * 「いま読んでいる見出し」を持つ。見出しの文字と位置は持たず、要るたびに note_pane へ尋ねる
 * （表示が正本・ARC-004）。本文・選択・Undo には触れない。 */
#ifndef NENEFOLIO_NOTE_OUTLINE_H
#define NENEFOLIO_NOTE_OUTLINE_H
#include "note_outline_outcome.h"
#include "note_outline_paint.h"
#include <stddef.h>
#include <stdint.h>

struct note_outline;
struct note_pane;
constexpr size_t note_outline_none = SIZE_MAX;
[[nodiscard]] enum note_outline_outcome
note_outline_create(struct note_outline *_Nullable *_Nonnull out);
void note_outline_destroy(struct note_outline *_Nullable outline);
/* 文書を読み込み直したとき。送り位置・キーボードの位置・観測を初めへ戻し、版を進める。 */
void note_outline_reset(struct note_outline *_Nonnull outline);
/* 読み込みごとに変わる。前の文書の行を指す値（読み上げの項目など）を見分けるために使う。 */
[[nodiscard]] uint64_t note_outline_revision(const struct note_outline *_Nonnull outline);
/* 本文が送られたあと。いま読んでいる見出しを尋ね直し、その行が欄の中に見えるように欄を送る。
 * 見え方が変わったら true。 */
[[nodiscard]] bool note_outline_track(struct note_outline *_Nonnull outline,
                                      struct note_pane *_Nonnull pane,
                                      const struct note_outline_paint *_Nonnull paint);
/* いま読んでいる見出しの行。無ければ note_outline_none。 */
[[nodiscard]] size_t note_outline_current(const struct note_outline *_Nonnull outline);
/* 欄に同時に見える行の数。 */
[[nodiscard]] size_t note_outline_rows(const struct note_outline_paint *_Nonnull paint);
/* その行の矩形（描画・当たり判定・読み上げで同じ）。欄の中に見えていない行は空。 */
[[nodiscard]] RECT note_outline_row_rect(const struct note_outline *_Nonnull outline,
                                         const struct note_outline_paint *_Nonnull paint,
                                         size_t index);
/* その点にある行。count は見出しの数。行の上でなければ note_outline_none。 */
[[nodiscard]] size_t note_outline_row_at(const struct note_outline *_Nonnull outline,
                                         const struct note_outline_paint *_Nonnull paint,
                                         POINT point, size_t count);
/* #1040: 欄の行が見出しを本文の先頭へ寄せた直後に呼ぶ。本文が上がりきれず別の見出しを返すときは、
 * 本文の答えが変わるまで index を「いま読んでいる見出し」として扱う（次の note_outline_track
 * から）。 */
void note_outline_choose(struct note_outline *_Nonnull outline, struct note_pane *_Nonnull pane,
                         size_t index);
/* #1026: 同じノートを流し直す前に hold、本文が元の行へ戻った後に release すると、欄の送り位置と
 * キーボードの位置が元のままになる（行の版は新しい）。release は位置が変わったら true。 */
void note_outline_hold(struct note_outline *_Nonnull outline);
[[nodiscard]] bool note_outline_release(struct note_outline *_Nonnull outline,
                                        const struct note_outline_paint *_Nonnull paint,
                                        size_t count);
/* #1018: 欄に見える行の数が変わったとき（窓の大きさが変わった）、最後の見出しの下に空きを残さず、
 * いま読んでいる見出しを欄の中に保つ。行の数が同じならホイールで送った位置を動かさない。位置が変わったら
 * true。 */
[[nodiscard]] bool note_outline_fit(struct note_outline *_Nonnull outline,
                                    const struct note_outline_paint *_Nonnull paint, size_t count);
/* 欄を rows 行だけ送る（負は上へ）。count は見出しの数。位置が変わったら true。 */
[[nodiscard]] bool note_outline_scroll(struct note_outline *_Nonnull outline,
                                       const struct note_outline_paint *_Nonnull paint, int rows,
                                       size_t count);
[[nodiscard]] size_t note_outline_cursor(const struct note_outline *_Nonnull outline);
/* キーボードの位置を置き、その行が見えるように欄を送る。count は見出しの数（0 なら何もしない）。 */
void note_outline_set_cursor(struct note_outline *_Nonnull outline,
                             const struct note_outline_paint *_Nonnull paint, size_t index,
                             size_t count);
/* ポインタの乗っている行・押している行。無ければ note_outline_none。変わったら true。 */
[[nodiscard]] bool note_outline_set_hover(struct note_outline *_Nonnull outline, size_t index);
[[nodiscard]] bool note_outline_set_pressed(struct note_outline *_Nonnull outline, size_t index);
[[nodiscard]] size_t note_outline_pressed(const struct note_outline *_Nonnull outline);
/* 欄を描く。行の文字と階層は pane から読む。pane が読み込み中なら題だけを描く。 */
void note_outline_draw(const struct note_outline *_Nonnull outline, struct note_pane *_Nonnull pane,
                       HDC _Nonnull device, const struct note_outline_paint *_Nonnull paint);
#endif
