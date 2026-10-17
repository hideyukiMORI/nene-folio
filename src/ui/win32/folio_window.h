/* 枠の無い主窓（FR-001 / FR-013）。WS_POPUP に WS_THICKFRAME を足し、WM_NCCALCSIZE を 0 で返して
 * 枠を消し、WM_NCHITTEST を自前で判定する（ADR 0002）。左にドロワーを子として置く。
 * 終了コードは持たない。窓が閉じたことは PostQuitMessage で合成ルートへ伝えるだけで、
 * プロセスの終了コードは合成ルートが決める（ARC-006）。 */
#ifndef NENEFOLIO_FOLIO_WINDOW_H
#define NENEFOLIO_FOLIO_WINDOW_H

#include "accessible_item.h"
#include "accessible_navigation.h"
#include "folio_window_outcome.h"
#include "font_bundle_outcome.h"

#include <objbase.h>
#include <windows.h>

struct folio_state;
struct folio_window;
struct accessible_view;
struct accessible_request;

/* state は窓より長く生きていなければならない。作った窓はすぐ表示する。
 * fonts は入口が窓を作る前に登録した同梱の書体の結果で、READY 以外なら最初の描画のあとに
 * 失敗の箱を 1 回出す（ADR 0036 の決定 5）。 */
[[nodiscard]] enum folio_window_outcome
folio_window_create(struct folio_state *_Nonnull state, enum font_bundle_outcome fonts,
                    struct folio_window *_Nullable *_Nonnull out);
/* メッセージループのTranslateMessageより前。処理済みの鍵は再dispatchしない。 */
[[nodiscard]] bool folio_window_translate(const struct folio_window *_Nonnull window,
                                          const MSG *_Nonnull message);
void folio_window_destroy(struct folio_window *_Nullable window);

/* UIAはこの表示境界だけを通る。状態の正本や保存経路をproviderへ複製しない。 */
[[nodiscard]] HRESULT folio_window_accessible_read(struct folio_window *_Nonnull self,
                                                   const struct accessible_item *_Nonnull item,
                                                   struct accessible_view *_Nonnull out);
[[nodiscard]] HRESULT folio_window_accessible_navigate(struct folio_window *_Nonnull self,
                                                       const struct accessible_item *_Nonnull item,
                                                       enum accessible_navigation direction,
                                                       struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT folio_window_accessible_focus(struct folio_window *_Nonnull self,
                                                    enum accessible_kind root,
                                                    struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT folio_window_accessible_selected(struct folio_window *_Nonnull self,
                                                       const struct accessible_item *_Nonnull item,
                                                       struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT folio_window_accessible_at(struct folio_window *_Nonnull self,
                                                 enum accessible_kind root, POINT point,
                                                 struct accessible_item *_Nonnull out);
[[nodiscard]] HWND _Nullable folio_window_accessible_window(struct folio_window *_Nonnull self,
                                                            enum accessible_kind kind);
[[nodiscard]] HRESULT
folio_window_accessible_action(struct folio_window *_Nonnull self,
                               const struct accessible_request *_Nonnull request);
[[nodiscard]] bool folio_window_accessible_ready(const struct folio_window *_Nonnull self);
[[nodiscard]] uint64_t folio_window_accessible_revision(const struct folio_window *_Nonnull self);

#endif
