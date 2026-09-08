/* 枠の無い主窓（FR-001 / FR-013）。WS_POPUP に WS_THICKFRAME を足し、WM_NCCALCSIZE を 0 で返して
 * 枠を消し、WM_NCHITTEST を自前で判定する（ADR 0002）。左にドロワーを子として置く。
 * 終了コードは持たない。窓が閉じたことは PostQuitMessage で合成ルートへ伝えるだけで、
 * プロセスの終了コードは合成ルートが決める（ARC-006）。 */
#ifndef NENEFOLIO_FOLIO_WINDOW_H
#define NENEFOLIO_FOLIO_WINDOW_H

#include "folio_window_outcome.h"

struct folio_state;
struct folio_window;

/* state は窓より長く生きていなければならない。作った窓はすぐ表示する。 */
[[nodiscard]] enum folio_window_outcome
folio_window_create(struct folio_state *_Nonnull state,
                    struct folio_window *_Nullable *_Nonnull out);
void folio_window_destroy(struct folio_window *_Nullable window);

#endif
