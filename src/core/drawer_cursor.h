/* 索引のカーソルが止まっている行（ADR 0015 の決定 8）。ノート行なら ref の両方を、
 * カテゴリ行なら ref.category だけを使う（ref.note は使わない）。
 * 全メンバーが独立に妥当なので完全型で公開する（note_ref / drawer_row と同じ理由・C-003 の例外）。
 */
#ifndef NENEFOLIO_DRAWER_CURSOR_H
#define NENEFOLIO_DRAWER_CURSOR_H

#include "drawer_row_kind.h"
#include "note_ref.h"

struct drawer_cursor
{
    enum drawer_row_kind kind; /* 止まっている行の種類 */
    struct note_ref ref;       /* カテゴリ番号と、ノート行のときのノート番号 */
};

#endif
