#ifndef NENEFOLIO_ACCESSIBILITY_HOST_H
#define NENEFOLIO_ACCESSIBILITY_HOST_H

#include "accessible_item.h"
#include "accessible_menu_event.h"
#include "accessible_navigation.h"

#include <objbase.h>
#include <uchar.h>
#include <windows.h>

struct accessibility_host;
struct accessibility_node;
struct folio_window;
struct accessible_request;
struct accessible_view;

/* UIスレッドが所有する公開先。生成失敗はHRESULT、成功時だけoutを採用する。 */
[[nodiscard]] HRESULT accessibility_host_create(struct folio_window *_Nonnull window,
                                                struct accessibility_host *_Nullable *_Nonnull out);
void accessibility_host_add_ref(struct accessibility_host *_Nonnull host);
void accessibility_host_release(struct accessibility_host *_Nonnull host);
/* 窓の所有参照を手放す前に切断する。clientが持つ参照からwindow/stateへ戻れなくする。 */
void accessibility_host_detach(struct accessibility_host *_Nonnull host);
[[nodiscard]] HRESULT accessibility_host_node(struct accessibility_host *_Nonnull host,
                                              struct accessible_item item,
                                              struct accessibility_node *_Nullable *_Nonnull out);
[[nodiscard]] HRESULT accessibility_host_read(struct accessibility_host *_Nonnull host,
                                              const struct accessible_item *_Nonnull item,
                                              struct accessible_view *_Nonnull out);
[[nodiscard]] HRESULT accessibility_host_navigate(struct accessibility_host *_Nonnull host,
                                                  const struct accessible_item *_Nonnull item,
                                                  enum accessible_navigation direction,
                                                  struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT accessibility_host_focus(struct accessibility_host *_Nonnull host,
                                               enum accessible_kind root,
                                               struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT accessibility_host_selected(struct accessibility_host *_Nonnull host,
                                                  const struct accessible_item *_Nonnull item,
                                                  struct accessible_item *_Nonnull out);
[[nodiscard]] HRESULT accessibility_host_at(struct accessibility_host *_Nonnull host,
                                            enum accessible_kind root, POINT point,
                                            struct accessible_item *_Nonnull out);
[[nodiscard]] HWND _Nullable accessibility_host_window(struct accessibility_host *_Nonnull host,
                                                       enum accessible_kind root);
[[nodiscard]] HRESULT accessibility_host_action(struct accessibility_host *_Nonnull host,
                                                const struct accessible_request *_Nonnull request);
/* Invokeはmodalを開けるので1件の意図をqueueへ渡して返り、同じ実行口で採用する。 */
[[nodiscard]] HRESULT accessibility_host_post(struct accessibility_host *_Nonnull host,
                                              const struct accessible_request *_Nonnull request);
void accessibility_host_dispatch(struct accessibility_host *_Nonnull host);
void accessibility_host_schedule(struct accessibility_host *_Nonnull host);
void accessibility_host_refresh(struct accessibility_host *_Nonnull host);
/* modal/保存を伴い得る共通操作の開始を、直接入力でも同じ要素から通知する。 */
void accessibility_host_invoked(struct accessibility_host *_Nonnull host,
                                struct accessible_item item);
/* 保存の完了を UIA の通知で読み上げに渡す。視覚の面は出さない（ADR0060 の決定 8）。 */
void accessibility_host_notify(struct accessibility_host *_Nonnull host,
                               struct accessible_item item, const char16_t *_Nonnull text);
[[nodiscard]] LRESULT accessibility_host_object(struct accessibility_host *_Nonnull host,
                                                enum accessible_kind root, WPARAM wparam,
                                                LPARAM lparam);
void accessibility_host_changed(struct accessibility_host *_Nonnull host, UINT message);

void accessibility_host_menu(struct accessibility_host *_Nonnull host,
                             enum accessible_menu_event event);
#endif
