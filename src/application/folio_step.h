/* 索引の選択を動かす歩み。閉じた集合（C-002）で、鍵の解釈は UI が行い、辿り方は application が
 * 持つ（ARC-011 / ADR 0013 の決定 5）。 */
#ifndef NENEFOLIO_FOLIO_STEP_H
#define NENEFOLIO_FOLIO_STEP_H

enum folio_step : unsigned char
{
    FOLIO_STEP_NEXT,     /* 次の見えるノート（j） */
    FOLIO_STEP_PREVIOUS, /* 前の見えるノート（k） */
    FOLIO_STEP_FIRST,    /* 最初の見えるノート（gg） */
    FOLIO_STEP_LAST      /* 最後の見えるノート（G） */
};

#endif
