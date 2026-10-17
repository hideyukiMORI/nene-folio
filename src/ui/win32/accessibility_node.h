#ifndef NENEFOLIO_ACCESSIBILITY_NODE_H
#define NENEFOLIO_ACCESSIBILITY_NODE_H

#include "accessible_item.h"

#include <objbase.h>
#include <windows.h>

struct accessibility_node;
struct accessibility_host;
struct IRawElementProviderSimple;
struct IRawElementProviderFragment;

/* SDKがCでは外部定義する定数を含むため、UIAヘッダをnode.cだけで読む。
 * 値を複製せず、そのSDK定数から作ったHRESULTを同じ境界で共有する。 */
extern const HRESULT accessibility_node_unavailable;
extern const HRESULT accessibility_node_disabled;
extern const HRESULT accessibility_node_invalid;

[[nodiscard]] HRESULT accessibility_node_create(struct accessibility_host *_Nonnull host,
                                                struct accessible_item item,
                                                struct accessibility_node *_Nullable *_Nonnull out);
void accessibility_node_add_ref(struct accessibility_node *_Nonnull node);
void accessibility_node_release(struct accessibility_node *_Nonnull node);
void accessibility_node_disconnect(struct accessibility_node *_Nonnull node);
[[nodiscard]] struct accessible_item
accessibility_node_item(const struct accessibility_node *_Nonnull node);
[[nodiscard]] struct IRawElementProviderSimple *_Nonnull accessibility_node_simple(
    struct accessibility_node *_Nonnull node);
[[nodiscard]] struct IRawElementProviderFragment *_Nonnull accessibility_node_fragment(
    struct accessibility_node *_Nonnull node);
/* 公開値の前回の写しだけを保持し、現在値との差を通知する。 */
void accessibility_node_refresh(struct accessibility_node *_Nonnull node);

void accessibility_node_structure_changed(struct accessibility_node *_Nonnull node);
void accessibility_node_focus_changed(struct accessibility_node *_Nonnull node);
void accessibility_node_selection_changed(struct accessibility_node *_Nonnull node);
[[nodiscard]] bool accessibility_node_is_request(LPARAM lparam);
[[nodiscard]] LRESULT accessibility_node_object(struct accessibility_node *_Nonnull node,
                                                HWND _Nonnull window, WPARAM wparam, LPARAM lparam);

#endif
