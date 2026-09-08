/* ドロワーの寸法（画素）。DPI に応じて UI が測った値で、全メンバーが独立に妥当（C-003）。 */
#ifndef NENEFOLIO_DRAWER_METRICS_H
#define NENEFOLIO_DRAWER_METRICS_H

struct drawer_metrics
{
    int top_padding;     /* 最初の行の上の余白 */
    int row_height;      /* 1 行の高さ */
    int category_indent; /* カテゴリ行の文字の左端 */
    int note_indent;     /* ノート行の文字の左端 */
};

#endif
