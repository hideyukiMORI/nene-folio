#include "accessibility_host.h"

#include "accessibility_node.h"
#include "accessible_request.h"
#include "accessible_view.h"
#include "folio_message.h"
#include "folio_window.h"

/* 直接注釈のGUIDはuuid.libに含まれないため、この所有者でSDK定義を実体化する。 */
#include <initguid.h>
#include <oleacc.h>
#include <stdlib.h>
#include <string.h>

/* COM参照は窓より長く生きてよい。ownerを外した後はUI状態へ到達しない。 */
struct accessibility_host
{
    LONG references;
    struct folio_window *_Nullable owner;
    struct accessibility_node *_Nonnull *_Nullable nodes;
    size_t count;
    size_t capacity;
    IAccPropServices *_Nullable annotations;
    bool apartment;
    bool refreshing;
    bool executing;
    bool scheduled;
    bool pending;
    bool had_focus;
    struct accessible_item focused;
    struct accessible_item selections[3];
    bool had_selection[3];
    uint64_t revision;
    unsigned visibility;
    struct accessible_request request;
    char16_t filter_name[1024];
    char16_t body_name[1024];
};

HRESULT accessibility_host_create(struct folio_window *_Nonnull window,
                                  struct accessibility_host *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessibility_host *_Nullable host = calloc(1, sizeof *host);
    if (host == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(result))
    {
        free(host);
        return result;
    }
    host->apartment = true;
    host->references = 1;
    host->owner = window;
    /* void**はCOMの出力ABIに限定する。 */
    result = CoCreateInstance(&CLSID_AccPropServices, nullptr, CLSCTX_INPROC_SERVER,
                              &IID_IAccPropServices, (void **)&host->annotations);
    if (FAILED(result))
    {
        accessibility_host_detach(host);
        accessibility_host_release(host);
        return result;
    }
    *out = host;
    return S_OK;
}

void accessibility_host_add_ref(struct accessibility_host *_Nonnull host)
{
    (void)InterlockedIncrement(&host->references);
}

void accessibility_host_release(struct accessibility_host *_Nonnull host)
{
    if (InterlockedDecrement(&host->references) == 0)
    {
        free(host->nodes);
        free(host);
    }
}

static void remove_node(struct accessibility_host *_Nonnull host, size_t index)
{
    struct accessibility_node *_Nonnull node = host->nodes[index];
    host->count -= 1;
    memmove(host->nodes + index, host->nodes + index + 1,
            (host->count - index) * sizeof *host->nodes);
    accessibility_node_disconnect(node);
    accessibility_node_release(node);
}

void accessibility_host_detach(struct accessibility_host *_Nonnull host)
{
    host->owner = nullptr; /* Disconnectが再入する前に公開先を閉じる。 */
    host->pending = false;
    host->scheduled = false;
    while (host->count != 0)
    {
        remove_node(host, host->count - 1);
    }
    if (host->annotations != nullptr)
    {
        host->annotations->lpVtbl->Release(host->annotations);
        host->annotations = nullptr;
    }
    if (host->apartment)
    {
        host->apartment = false;
        CoUninitialize();
    }
}

static HRESULT reserve_node(struct accessibility_host *_Nonnull host)
{
    if (host->count < host->capacity)
    {
        return S_OK;
    }
    if (host->capacity > SIZE_MAX / 2 / sizeof *host->nodes)
    {
        return E_OUTOFMEMORY;
    }
    size_t capacity = host->capacity == 0 ? 32 : host->capacity * 2;
    struct accessibility_node *_Nonnull *_Nullable grown =
        realloc(host->nodes, capacity * sizeof *host->nodes);
    if (grown == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    host->nodes = grown;
    host->capacity = capacity;
    return S_OK;
}

HRESULT accessibility_host_node(struct accessibility_host *_Nonnull host,
                                struct accessible_item item,
                                struct accessibility_node *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessible_view view = {0};
    HRESULT result = accessibility_host_read(host, &item, &view);
    if (FAILED(result))
    {
        return result;
    }
    for (size_t index = 0; index < host->count; ++index)
    {
        if (accessible_item_equal(accessibility_node_item(host->nodes[index]), item))
        {
            *out = host->nodes[index];
            accessibility_node_add_ref(*out);
            return S_OK;
        }
    }
    result = reserve_node(host);
    if (FAILED(result))
    {
        return result;
    }
    struct accessibility_node *_Nullable node = nullptr;
    result = accessibility_node_create(host, item, &node);
    if (FAILED(result))
    {
        return result;
    }
    host->nodes[host->count] = node;
    host->count += 1;
    accessibility_node_refresh(node);
    accessibility_node_add_ref(node);
    *out = node;
    return S_OK;
}

HRESULT accessibility_host_read(struct accessibility_host *_Nonnull host,
                                const struct accessible_item *_Nonnull item,
                                struct accessible_view *_Nonnull out)
{
    if (host->owner == nullptr)
    {
        return accessibility_node_unavailable;
    }
    return folio_window_accessible_read(host->owner, item, out);
}

HRESULT accessibility_host_navigate(struct accessibility_host *_Nonnull host,
                                    const struct accessible_item *_Nonnull item,
                                    enum accessible_navigation direction,
                                    struct accessible_item *_Nonnull out)
{
    if (host->owner == nullptr)
    {
        return accessibility_node_unavailable;
    }
    return folio_window_accessible_navigate(host->owner, item, direction, out);
}

HRESULT accessibility_host_focus(struct accessibility_host *_Nonnull host,
                                 enum accessible_kind root, struct accessible_item *_Nonnull out)
{
    if (host->owner == nullptr)
    {
        return accessibility_node_unavailable;
    }
    return folio_window_accessible_focus(host->owner, root, out);
}

HRESULT accessibility_host_selected(struct accessibility_host *_Nonnull host,
                                    const struct accessible_item *_Nonnull item,
                                    struct accessible_item *_Nonnull out)
{
    if (host->owner == nullptr)
    {
        return accessibility_node_unavailable;
    }
    return folio_window_accessible_selected(host->owner, item, out);
}

HRESULT accessibility_host_at(struct accessibility_host *_Nonnull host, enum accessible_kind root,
                              POINT point, struct accessible_item *_Nonnull out)
{
    if (host->owner == nullptr)
    {
        return accessibility_node_unavailable;
    }
    return folio_window_accessible_at(host->owner, root, point, out);
}

HWND _Nullable accessibility_host_window(struct accessibility_host *_Nonnull host,
                                         enum accessible_kind root)
{
    return host->owner == nullptr ? nullptr : folio_window_accessible_window(host->owner, root);
}

static HRESULT action_ready(struct accessibility_host *_Nonnull host,
                            const struct accessible_request *_Nonnull request)
{
    struct accessible_view view = {0};
    HRESULT result = accessibility_host_read(host, &request->item, &view);
    if (FAILED(result))
    {
        return result;
    }
    if (!view.enabled)
    {
        return accessibility_node_disabled;
    }
    if (host->refreshing || host->executing || !folio_window_accessible_ready(host->owner))
    {
        return accessibility_node_invalid;
    }
    return S_OK;
}

HRESULT accessibility_host_action(struct accessibility_host *_Nonnull host,
                                  const struct accessible_request *_Nonnull request)
{
    HRESULT result = action_ready(host, request);
    if (FAILED(result))
    {
        return result;
    }
    accessibility_host_add_ref(host);
    host->executing = true;
    result = folio_window_accessible_action(host->owner, request);
    host->executing = false;
    accessibility_host_schedule(host);
    accessibility_host_release(host);
    return result;
}

HRESULT accessibility_host_post(struct accessibility_host *_Nonnull host,
                                const struct accessible_request *_Nonnull request)
{
    HRESULT result = action_ready(host, request);
    if (FAILED(result))
    {
        return result;
    }
    if (host->pending)
    {
        return accessibility_node_invalid;
    }
    host->request = *request;
    host->pending = true;
    HWND window = accessibility_host_window(host, ACCESSIBLE_WINDOW);
    if (!PostMessageW(window, folio_message_accessibility_invoke, 0, 0))
    {
        host->pending = false;
        return E_FAIL;
    }
    return S_OK;
}

void accessibility_host_dispatch(struct accessibility_host *_Nonnull host)
{
    if (!host->pending)
    {
        return;
    }
    struct accessible_request request = host->request;
    host->pending = false;
    (void)accessibility_host_action(host, &request);
}

void accessibility_host_schedule(struct accessibility_host *_Nonnull host)
{
    if (host->owner == nullptr || host->scheduled)
    {
        return;
    }
    HWND window = accessibility_host_window(host, ACCESSIBLE_WINDOW);
    host->scheduled = PostMessageW(window, folio_message_accessibility_changed, 0, 0) != 0;
}

static void annotate(struct accessibility_host *_Nonnull host, enum accessible_kind kind,
                     char16_t *_Nonnull previous)
{
    struct accessible_item item = {.kind = kind};
    struct accessible_view view = {0};
    if (FAILED(accessibility_host_read(host, &item, &view)) || wcscmp(previous, view.name) == 0)
    {
        return;
    }
    HWND window = accessibility_host_window(host, kind);
    if (window == nullptr || host->annotations == nullptr)
    {
        return;
    }
    HRESULT result = host->annotations->lpVtbl->SetHwndPropStr(
        host->annotations, window, (DWORD)OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, view.name);
    if (SUCCEEDED(result))
    {
        memcpy(previous, view.name, sizeof view.name);
        NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, window, OBJID_CLIENT, CHILDID_SELF);
    }
}

static void refresh_structure(struct accessibility_host *_Nonnull host)
{
    uint64_t revision = folio_window_accessible_revision(host->owner);
    if (revision == host->revision && revision != UINT64_MAX)
    {
        return;
    }
    host->revision = revision;
    size_t index = 0;
    while (index < host->count)
    {
        struct accessible_item item = accessibility_node_item(host->nodes[index]);
        struct accessible_view view = {0};
        if (accessible_item_row(item) && FAILED(accessibility_host_read(host, &item, &view)))
        {
            remove_node(host, index);
            continue;
        }
        index += 1;
    }
    struct accessible_item item = {.kind = ACCESSIBLE_TREE};
    struct accessibility_node *_Nullable tree = nullptr;
    if (SUCCEEDED(accessibility_host_node(host, item, &tree)))
    {
        accessibility_node_structure_changed(tree);
        accessibility_node_release(tree);
    }
}

static void raise_structure(struct accessibility_host *_Nonnull host, enum accessible_kind kind)
{
    struct accessibility_node *_Nullable node = nullptr;
    if (SUCCEEDED(accessibility_host_node(host, (struct accessible_item){.kind = kind}, &node)))
    {
        accessibility_node_structure_changed(node);
        accessibility_node_release(node);
    }
}

static void refresh_visibility(struct accessibility_host *_Nonnull host)
{
    const enum accessible_kind kinds[] = {ACCESSIBLE_DRAWER, ACCESSIBLE_PANE_TOOLS,
                                          ACCESSIBLE_FIND};
    unsigned visibility = 0;
    for (size_t index = 0; index < sizeof kinds / sizeof kinds[0]; ++index)
    {
        struct accessible_view view = {0};
        struct accessible_item item = {.kind = kinds[index]};
        if (SUCCEEDED(accessibility_host_read(host, &item, &view)) && !view.offscreen)
        {
            visibility |= 1U << index;
        }
    }
    if (host->visibility != visibility)
    {
        host->visibility = visibility;
        raise_structure(host, ACCESSIBLE_WINDOW);
        raise_structure(host, ACCESSIBLE_DRAWER);
    }
}

static void observe_selection(struct accessibility_host *_Nonnull host, size_t index)
{
    const enum accessible_kind groups[] = {ACCESSIBLE_LAYOUT_GROUP, ACCESSIBLE_MODE_GROUP,
                                           ACCESSIBLE_TREE};
    struct accessible_item group = {.kind = groups[index]};
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    bool present = accessibility_host_selected(host, &group, &item) == S_OK;
    bool changed = host->had_selection[index] != present ||
                   (present && !accessible_item_equal(host->selections[index], item));
    host->selections[index] = item;
    host->had_selection[index] = present;
    if (!changed)
    {
        return;
    }
    struct accessibility_node *_Nullable node = nullptr;
    if (SUCCEEDED(accessibility_host_node(host, present ? item : group, &node)))
    {
        accessibility_node_selection_changed(node);
        accessibility_node_release(node);
    }
}

static void observe_focus(struct accessibility_host *_Nonnull host)
{
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    if (accessibility_host_focus(host, ACCESSIBLE_WINDOW, &item) != S_OK)
    {
        host->had_focus = false;
        return;
    }
    struct accessibility_node *_Nullable node = nullptr;
    if (SUCCEEDED(accessibility_host_node(host, item, &node)))
    {
        bool changed = !host->had_focus || !accessible_item_equal(host->focused, item);
        host->focused = item;
        host->had_focus = true;
        if (changed)
        {
            accessibility_node_focus_changed(node);
        }
        accessibility_node_release(node);
    }
}

void accessibility_host_refresh(struct accessibility_host *_Nonnull host)
{
    host->scheduled = false;
    if (host->owner == nullptr || host->refreshing || host->executing)
    {
        return;
    }
    accessibility_host_add_ref(host);
    host->refreshing = true;
    refresh_structure(host);
    refresh_visibility(host);
    annotate(host, ACCESSIBLE_FILTER, host->filter_name);
    annotate(host, ACCESSIBLE_BODY, host->body_name);
    observe_focus(host);
    for (size_t index = 0; index < 3; ++index)
    {
        observe_selection(host, index);
    }
    size_t count = host->count;
    for (size_t index = 0; index < count && index < host->count; ++index)
    {
        accessibility_node_refresh(host->nodes[index]);
    }
    host->refreshing = false;
    accessibility_host_release(host);
}

LRESULT accessibility_host_object(struct accessibility_host *_Nonnull host,
                                  enum accessible_kind root, WPARAM wparam, LPARAM lparam)
{
    HWND window = accessibility_host_window(host, root);
    if (!accessibility_node_is_request(lparam))
    {
        return DefWindowProcW(window, WM_GETOBJECT, wparam, lparam);
    }
    struct accessibility_node *_Nullable node = nullptr;
    if (FAILED(accessibility_host_node(host, (struct accessible_item){.kind = root}, &node)))
    {
        return 0;
    }
    LRESULT result = accessibility_node_object(node, window, wparam, lparam);
    accessibility_node_release(node);
    return result;
}

void accessibility_host_changed(struct accessibility_host *_Nonnull host, UINT message)
{
    switch (message)
    {
    case WM_SIZE:
    case WM_MOVE:
    case WM_SHOWWINDOW:
    case WM_DPICHANGED:
    case WM_ENABLE:
    case WM_SETFOCUS:
    case WM_KILLFOCUS:
    case WM_COMMAND:
    case WM_NOTIFY:
    case WM_KEYDOWN:
    case WM_CHAR:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_IME_STARTCOMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_ENTERMENULOOP:
    case WM_EXITMENULOOP:
    case folio_message_select_note:
    case folio_message_drawer_menu:
    case folio_message_accessibility_invoke:
        accessibility_host_schedule(host);
        break;
    default:
        break;
    }
}
