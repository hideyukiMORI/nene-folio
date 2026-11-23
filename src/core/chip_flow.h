/* 札を左から詰めて並べ、入らなければ次の行へ送る。使ってよい行の数に入らないときは、後ろの
 * 札から畳んで「+N」の札に置き換える（ADR0122）。状態も確保も持たない。 */
#ifndef NENEFOLIO_CHIP_FLOW_H
#define NENEFOLIO_CHIP_FLOW_H

#include "chip_flow_input.h"
#include "chip_flow_result.h"

/* places は input->count 個ぶんの置き場。結果の shown 個が有効で、残りの中身は決まらない。
 * 0 番の札と「開く札」は、行の数を超えても必ず置く（そのとき rows は input->rows より大きい）。 */
[[nodiscard]] struct chip_flow_result
chip_flow_arrange(const struct chip_flow_input *_Nonnull input,
                  struct chip_flow_place *_Nonnull places);

#endif
