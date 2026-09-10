/* ドロワーの寸法（画素）。DPI に応じて UI が測った値で、全メンバーが独立に妥当（C-003）。 */
#ifndef NENEFOLIO_DRAWER_METRICS_H
#define NENEFOLIO_DRAWER_METRICS_H

struct drawer_metrics
{
    int top_padding;     /* 最初の行の上の余白（頭の帯を含む） */
    int row_height;      /* ノート行の高さ */
    int category_height; /* カテゴリ行の高さ */
    int category_gap;    /* カテゴリ行の上に空ける間 */
    int category_indent; /* カテゴリ行の番号の左端 */
    int note_indent;     /* ノート行の文字の左端 */
    int viewport_height; /* ドロワーの client の高さ。スクロール上限はここから決まる（FR-012） */
    int bottom_padding;  /* 最後の行の下に空ける余白。上限に含める */
};

#endif
