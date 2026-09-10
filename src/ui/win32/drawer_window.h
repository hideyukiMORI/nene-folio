/* 左のドロワー（FR-003 / FR-012）。
 * 自前描画の 1 ウィンドウクラスで、共通コントロールを使わない（ADR 0002）。
 * application が作った drawer_layout の行をメモリ DC で描いて一括転送し、クリックを意図として
 * application へ渡すだけで、判断を持たない（ARC-011 / C-017）。HWND とフォントの所有者はこの型 1
 * つ（ARC-005）。 */
#ifndef NENEFOLIO_DRAWER_WINDOW_H
#define NENEFOLIO_DRAWER_WINDOW_H

#include "drawer_window_outcome.h"

#include <windows.h>

struct drawer_window;
struct folio_state;

/* parent の子として作る。state は drawer より長く生きていなければならない。 */
[[nodiscard]] enum drawer_window_outcome
drawer_window_create(HWND _Nonnull parent, struct folio_state *_Nonnull state,
                     struct drawer_window *_Nullable *_Nonnull out);
/* 親が配置に使う。ウィンドウが既に破棄されていれば nullptr。 */
[[nodiscard]] HWND _Nullable drawer_window_handle(const struct drawer_window *_Nonnull drawer);
/* 主窓が受けた鍵をスクロールの意図に変える（FR-012 / ADR 0009 の決定 5）。
 * ドロワーはフォーカスを取らないので、鍵は主窓からここへ渡す。動くのは
 * VK_UP / VK_DOWN（ノート行 1 つ）と VK_PRIOR / VK_NEXT（1 画面から 1 行を引いた量）だけで、
 * ほかの鍵では何も起きない。 */
void drawer_window_scroll_key(struct drawer_window *_Nonnull drawer, WPARAM key);
/* 選択中のノートの行を見える位置へ寄せる意図を出す（FR-018 / ADR 0013 の決定 6）。
 * 寸法を測れるのがドロワーなので、主窓は鍵で選択を動かしたあとここを呼ぶ。
 * 何も選んでいなければ何も動かない。 */
void drawer_window_reveal_selection(struct drawer_window *_Nonnull drawer);
void drawer_window_destroy(struct drawer_window *_Nullable drawer);

#endif
