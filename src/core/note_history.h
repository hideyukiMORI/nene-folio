/* 保存の直前の本文を残す履歴の深さ（ADR 0012 の決定 1）。
 * `data/.history/<カテゴリ>/<ノート>/1.md`（最新）〜 `<depth>.md`（最古）の連番で、
 * これを超えた版は捨てる。時刻は名前にも中身にも使わない（ARC-007）。
 * ここが唯一の出どころで、adapters のローテーションと（後で入る）戻す操作が同じ値を見る。 */
#ifndef NENEFOLIO_NOTE_HISTORY_H
#define NENEFOLIO_NOTE_HISTORY_H

#include <stddef.h>

constexpr size_t note_history_depth = 5;

#endif
