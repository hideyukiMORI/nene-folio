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
struct storage_state;
struct folio_window;
/* UIA hostの同期処理（通知・snapshot復元まで）も終了の安全境界へ含める。 */
void folio_window_begin_dispatch(struct folio_window *_Nonnull self);
void folio_window_end_dispatch(struct folio_window *_Nonnull self);
struct accessible_view;
struct accessible_request;

/* state は窓より長く生きていなければならない。作った窓はすぐ表示する。
 * fonts は入口が窓を作る前に登録した同梱の書体の結果で、READY 以外なら最初の描画のあとに
 * 失敗の箱を 1 回出す（ADR 0036 の決定 5）。 */
[[nodiscard]] enum folio_window_outcome
folio_window_create(struct folio_state *_Nonnull state, struct storage_state *_Nonnull storage,
                    enum font_bundle_outcome fonts, struct folio_window *_Nullable *_Nonnull out);
/* メッセージループのTranslateMessageより前。処理済みの鍵は再dispatchしない。 */
[[nodiscard]] bool folio_window_translate(const struct folio_window *_Nonnull window,
                                          const MSG *_Nonnull message);
/* 合成ルートが空queueで使う副作用のない可否判定。索引の一件を優先し、次に有限sampleを進める。 */
[[nodiscard]] bool folio_window_idle_work_ready(const struct folio_window *_Nonnull self);
void folio_window_continue_idle_work(struct folio_window *_Nonnull self);
/* #1071 / ADR0118: 外から来た「開く」依頼のリンク（UTF-8、length バイト）を預かる。起動の引数と、
 * 動いている窓へ渡された依頼の、共通の入口。ここでは写して自分へ知らせるだけで、検証して開くのは
 * メッセージの待ちの先頭（folio_message_open_link）。結果は右の窓の 1 行と描き直しで伝える。
 * 面を開いている・ほかの操作の途中なら、開かずに 1 行で知らせる。預かれるのは 1 つで、開く前に
 * 次が来たら新しいほうに替わる。 */
void folio_window_open_link(struct folio_window *_Nonnull self, const char *_Nonnull bytes,
                            size_t length);
/* #1070 / ADR0119 の決定 2: data フォルダの下で何かが変わった（アプリ自身の保存でも来る）。
 * ここでは読まない。知らせから少し置いて、窓が静かなときに一覧と開いているノートを読み直す
 * （ほかのアプリから戻ったときと同じ読み直し）。置いているあいだの知らせは 1 回にまとまる。 */
void folio_window_data_changed(struct folio_window *_Nonnull self);
/* 知らせをまとめている最中か（最初の知らせから、読み直しを頼むまで）。合成ルートは、この
 * あいだ次の知らせを頼まない。副作用は無い。 */
[[nodiscard]] bool folio_window_data_settling(const struct folio_window *_Nonnull self);
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
[[nodiscard]] uint64_t
folio_window_accessible_surface_revision(const struct folio_window *_Nonnull self);
[[nodiscard]] uint64_t folio_window_accessible_revision(const struct folio_window *_Nonnull self);
/* #975: changes whenever the headings column's rows stop meaning what they meant. */
[[nodiscard]] uint64_t
folio_window_accessible_outline_revision(const struct folio_window *_Nonnull self);
/* 絞り込みの札の行のタグの札の並び（条件の改訂・置かれた数・「+N」の有無）を 1 つの値で返す。
 * 変わったら、host が前の札を捨てて、群の子の変化を知らせる（#1247）。 */
[[nodiscard]] uint64_t
folio_window_accessible_filter_revision(const struct folio_window *_Nonnull self);
/* ノートのタグの行の荷札の並び（荷札の写しの改訂・置かれた数・「+N」の有無）を 1 つの値で返す。
 * 行が出ていないあいだは 0。変わったら、host が前の荷札を捨てて、群の子の変化を知らせる（#1309）。
 */
[[nodiscard]] uint64_t
folio_window_accessible_note_tag_revision(const struct folio_window *_Nonnull self);

#endif
