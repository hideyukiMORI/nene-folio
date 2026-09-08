/* 左のドロワー（FR-003 / FR-012）。
 * 自前描画の 1 ウィンドウクラスで、共通コントロールを使わない（ADR 0002）。
 * application が作った drawer_layout の行をメモリ DC で描いて一括転送するだけで、
 * 判断を持たない（ARC-011 / C-017）。HWND とフォントの所有者はこの型 1 つ（ARC-005）。 */
#ifndef NENEFOLIO_DRAWER_WINDOW_H
#define NENEFOLIO_DRAWER_WINDOW_H

#include "drawer_window_outcome.h"

#include <windows.h>

struct drawer_window;
struct folio_state;

/* parent の子として作る。state は drawer より長く生きていなければならない。 */
[[nodiscard]] enum drawer_window_outcome
drawer_window_create(HWND _Nonnull parent, const struct folio_state *_Nonnull state,
                     struct drawer_window *_Nullable *_Nonnull out);
/* 親が配置に使う。ウィンドウが既に破棄されていれば nullptr。 */
[[nodiscard]] HWND _Nullable drawer_window_handle(const struct drawer_window *_Nonnull drawer);
void drawer_window_destroy(struct drawer_window *_Nullable drawer);

#endif
