/* ドラッグを離したときの落とし先（FR-009 / ADR 0007 の決定 1）。core が配置から決め、
 * UI は挿入線を引いて意図を出すだけで座標も番号も計算しない（ARC-011）。
 * 全メンバーが独立に妥当なので完全型で公開する（drawer_row と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_DROP_TARGET_H
#define NENEFOLIO_DROP_TARGET_H

#include "drop_kind.h"

#include <stddef.h>

struct drop_target
{
    enum drop_kind kind;
    size_t category; /* 掴んだ行が属するカテゴリの番号（台帳の順） */
    size_t index;    /* 移動後の番号。カテゴリなら categories の、ノートならそのカテゴリ内の番号。
                      * 掴んだ行と同じ番号なら「変わらない」 */
    int line_y;      /* 挿入線の y 座標（ドロワーの左上を原点とする画素） */
};

#endif
