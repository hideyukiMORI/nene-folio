/* 左のドロワー（FR-003 / FR-012）。
 * 自前描画の 1 ウィンドウクラスで、共通コントロールを使わない（ADR 0002）。
 * application が作った drawer_layout の行をメモリ DC で描いて一括転送し、クリックを意図として
 * application へ渡すだけで、判断を持たない（ARC-011 / C-017）。HWND とフォントの所有者はこの型 1
 * つ（ARC-005）。 */
#ifndef NENEFOLIO_DRAWER_WINDOW_H
#define NENEFOLIO_DRAWER_WINDOW_H

#include "accessible_item.h"
#include "accessible_navigation.h"
#include "drawer_window_outcome.h"
#include "folio_state_outcome.h"

#include <objbase.h>
#include <windows.h>

struct drawer_window;
struct folio_state;
struct accessibility_host;
struct accessible_view;
struct accessible_request;

/* parent の子として作る。state は drawer より長く生きていなければならない。 */
[[nodiscard]] enum drawer_window_outcome
drawer_window_create(HWND _Nonnull parent, struct folio_state *_Nonnull state,
                     struct accessibility_host *_Nonnull accessibility,
                     struct drawer_window *_Nullable *_Nonnull out);
/* 親が配置に使う。ウィンドウが既に破棄されていれば nullptr。 */
[[nodiscard]] HWND _Nullable drawer_window_handle(const struct drawer_window *_Nonnull drawer);
/* 常設の「すべてのノートを検索」の欄を置く矩形（ADR 0024 の決定 6）。
 * 欄は主窓の子なので、寸法を測れるドロワーが答え、主窓がそこへ動かす。
 * localの同じ矩形から親client座標へ変換して答える（ADR0047）。
 * ウィンドウが既に破棄されていれば空の矩形。 */
[[nodiscard]] RECT drawer_window_filter_rect(const struct drawer_window *_Nonnull drawer);
/* 主窓が受けた鍵をスクロールの意図に変える（FR-012 / ADR 0009 の決定 5）。
 * ドロワーはフォーカスを取らないので、鍵は主窓からここへ渡す。動くのは
 * VK_UP / VK_DOWN（ノート行 1 つ）と VK_PRIOR / VK_NEXT（1 画面から 1 行を引いた量）だけで、
 * ほかの鍵では何も起きない。 */
void drawer_window_scroll_key(struct drawer_window *_Nonnull drawer, WPARAM key);
/* カーソルの行を見える位置へ寄せる意図を出す（FR-018 / ADR 0013 の決定 6・ADR 0015 の決定 6）。
 * 寸法を測れるのがドロワーなので、主窓は鍵でカーソルを動かしたあとここを呼ぶ。
 * カーソルが無ければ何も動かない（カテゴリ行のカーソルにも効く）。 */
void drawer_window_reveal_cursor(struct drawer_window *_Nonnull drawer);
/* 言語が変わったときに書体を取り直す（ADR 0032 の決定 6）。
 * 値の正本は core の ui_font で、ここは HFONT を作り直して全面を無効にするだけ。 */
void drawer_window_refont(struct drawer_window *_Nonnull drawer);
/* テーマが変わったときに色を取り直す（ADR 0031 の決定 6）。
 * 値の正本は folio_palette で、ここは写しを更新して全面を無効にするだけ。 */
void drawer_window_recolor(struct drawer_window *_Nonnull drawer);
void drawer_window_destroy(struct drawer_window *_Nullable drawer);

[[nodiscard]] HRESULT drawer_window_accessible_read(struct drawer_window *_Nonnull self,
                                                    const struct accessible_item *_Nonnull item,
                                                    struct accessible_view *_Nonnull out);
[[nodiscard]] HRESULT drawer_window_accessible_navigate(struct drawer_window *_Nonnull self,
                                                        const struct accessible_item *_Nonnull item,
                                                        enum accessible_navigation direction,
                                                        struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT drawer_window_accessible_at(struct drawer_window *_Nonnull self, POINT point,
                                                  struct accessible_item *_Nonnull out);
[[nodiscard]] enum folio_state_outcome
drawer_window_accessible_scroll(struct drawer_window *_Nonnull self,
                                const struct accessible_request *_Nonnull request);

void drawer_window_choose_color(struct drawer_window *_Nonnull drawer, size_t category);
void drawer_window_context_menu(struct drawer_window *_Nonnull drawer);
#endif
