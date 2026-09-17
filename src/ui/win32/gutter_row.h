/* 番号の帯の 1 行（ADR 0026 の決定 3）。note_pane が主窓の client 座標へ直して並べ、
 * 主窓はそれを描くだけである（座標を直すのは note_pane 1 か所）。
 * 行の高さは一定でないので、高さも 1 行ずつ持つ（EM_POSFROMCHAR の実測・ADR 0026 の文脈）。
 * 全メンバーが独立に妥当なので完全型で公開する（note_search_span と同じ理由・C-003 の例外）。 */
#ifndef NENEFOLIO_GUTTER_ROW_H
#define NENEFOLIO_GUTTER_ROW_H

#include <stddef.h>

struct gutter_row
{
    int top;       /* 主窓の client 座標の y */
    int height;    /* その表示行の高さ */
    size_t number; /* 1 始まりの論理行番号 */
};

#endif
