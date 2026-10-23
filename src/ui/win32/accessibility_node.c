#include "accessibility_node.h"

#include "accessibility_host.h"
#include "accessible_request.h"
#include "accessible_view.h"

#include <UIAutomation.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

const HRESULT accessibility_node_unavailable = (HRESULT)UIA_E_ELEMENTNOTAVAILABLE;
const HRESULT accessibility_node_disabled = (HRESULT)UIA_E_ELEMENTNOTENABLED;
const HRESULT accessibility_node_invalid = (HRESULT)UIA_E_INVALIDOPERATION;

/* COMのinterface部分だけをSDK境界で回収する。公開先の状態はhostから毎回読む。 */
struct accessibility_node
{
    IRawElementProviderSimple simple;
    IRawElementProviderFragment fragment;
    IRawElementProviderFragmentRoot root;
    IInvokeProvider invoke;
    ISelectionProvider selection;
    ISelectionItemProvider selection_item;
    IExpandCollapseProvider expand;
    IScrollProvider scroll;
    IScrollItemProvider scroll_item;
    IToggleProvider toggle;
    LONG references;
    struct accessibility_host *_Nonnull host;
    struct accessible_item item;
    struct accessible_view previous;
    bool observed;
    bool disconnected;
};

static bool has_invoke(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_SETTINGS || kind == ACCESSIBLE_CLOSE || kind == ACCESSIBLE_MENU ||
           kind == ACCESSIBLE_NEW_NOTE || kind == ACCESSIBLE_NEW_CATEGORY ||
           kind == ACCESSIBLE_NOTE || kind == ACCESSIBLE_SAVE || kind == ACCESSIBLE_HELP ||
           kind == ACCESSIBLE_FIND || kind == ACCESSIBLE_SURFACE_CHOICE ||
           kind == ACCESSIBLE_SURFACE_BUTTON;
}

static bool has_selection(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_TREE || kind == ACCESSIBLE_LAYOUT_GROUP ||
           kind == ACCESSIBLE_MODE_GROUP || kind == ACCESSIBLE_SURFACE_LIST;
}

static bool has_selection_item(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_NOTE || kind == ACCESSIBLE_LAYOUT_DRAWER ||
           kind == ACCESSIBLE_LAYOUT_BOTH || kind == ACCESSIBLE_LAYOUT_NOTE ||
           kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT ||
           kind == ACCESSIBLE_SURFACE_CHOICE || kind == ACCESSIBLE_SURFACE_LANGUAGE;
}

static bool has_scroll(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_TREE || kind == ACCESSIBLE_SURFACE_LIST;
}

static bool has_scroll_item(struct accessible_item item)
{
    return accessible_item_row(item) || item.kind == ACCESSIBLE_SURFACE_CHOICE ||
           item.kind == ACCESSIBLE_SURFACE_LANGUAGE || item.kind == ACCESSIBLE_SURFACE_TOGGLE ||
           item.kind == ACCESSIBLE_SURFACE_HELP;
}

static bool has_expand(enum accessible_kind kind)
{
    return kind == ACCESSIBLE_CATEGORY || kind == ACCESSIBLE_NOTE || kind == ACCESSIBLE_MENU;
}

static struct accessibility_node *_Nonnull simple_owner(IRawElementProviderSimple *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, simple));
}

static struct accessibility_node *_Nonnull fragment_owner(
    IRawElementProviderFragment *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, fragment));
}

static struct accessibility_node *_Nonnull root_owner(
    IRawElementProviderFragmentRoot *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value - offsetof(struct accessibility_node, root));
}

static struct accessibility_node *_Nonnull invoke_owner(IInvokeProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, invoke));
}

static struct accessibility_node *_Nonnull selection_owner(ISelectionProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, selection));
}

static struct accessibility_node *_Nonnull selection_item_owner(
    ISelectionItemProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, selection_item));
}

static struct accessibility_node *_Nonnull expand_owner(IExpandCollapseProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, expand));
}

static struct accessibility_node *_Nonnull scroll_owner(IScrollProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, scroll));
}

static struct accessibility_node *_Nonnull scroll_item_owner(IScrollItemProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, scroll_item));
}

static struct accessibility_node *_Nonnull toggle_owner(IToggleProvider *_Nonnull value)
{
    return (struct accessibility_node *)((char *)value -
                                         offsetof(struct accessibility_node, toggle));
}

static HRESULT read_view(struct accessibility_node *_Nonnull node,
                         struct accessible_view *_Nonnull out)
{
    return node->disconnected ? (HRESULT)UIA_E_ELEMENTNOTAVAILABLE
                              : accessibility_host_read(node->host, &node->item, out);
}

static void destroy(struct accessibility_node *_Nonnull node)
{
    accessibility_host_release(node->host);
    free(node);
}

static ULONG add_ref(struct accessibility_node *_Nonnull node)
{
    return (ULONG)InterlockedIncrement(&node->references);
}

static ULONG release(struct accessibility_node *_Nonnull node)
{
    LONG count = InterlockedDecrement(&node->references);
    if (count == 0)
    {
        destroy(node);
    }
    return (ULONG)count;
}

/* QueryInterfaceのvoid**はCOM ABIだけの境界。製品の状態は具体型を保つ。 */
static IUnknown *_Nullable pattern_for(struct accessibility_node *_Nonnull node, PATTERNID id);

static void *_Nullable pattern_interface(struct accessibility_node *_Nonnull node, REFIID id)
{
    static const struct
    {
        const IID *_Nonnull id;
        PATTERNID pattern;
    } patterns[] = {{&IID_IInvokeProvider, UIA_InvokePatternId},
                    {&IID_ISelectionProvider, UIA_SelectionPatternId},
                    {&IID_ISelectionItemProvider, UIA_SelectionItemPatternId},
                    {&IID_IExpandCollapseProvider, UIA_ExpandCollapsePatternId},
                    {&IID_IScrollProvider, UIA_ScrollPatternId},
                    {&IID_IScrollItemProvider, UIA_ScrollItemPatternId},
                    {&IID_IToggleProvider, UIA_TogglePatternId}};
    for (size_t index = 0; index < sizeof patterns / sizeof patterns[0]; ++index)
    {
        if (IsEqualIID(id, patterns[index].id))
        {
            return pattern_for(node, patterns[index].pattern);
        }
    }
    return nullptr;
}

static HRESULT query(struct accessibility_node *_Nonnull node, REFIID id,
                     void *_Nullable *_Nullable out)
{
    if (out == nullptr)
        return E_POINTER;
    *out = pattern_interface(node, id);
    if (IsEqualIID(id, &IID_IUnknown) || IsEqualIID(id, &IID_IRawElementProviderSimple))
        *out = &node->simple;
    if (IsEqualIID(id, &IID_IRawElementProviderFragment))
        *out = &node->fragment;
    if (IsEqualIID(id, &IID_IRawElementProviderFragmentRoot) && accessible_item_root(node->item))
        *out = &node->root;
    if (*out == nullptr)
        return E_NOINTERFACE;
    (void)add_ref(node);
    return S_OK;
}

static HRESULT perform(struct accessibility_node *_Nonnull node, enum accessible_action action)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
    {
        return result;
    }
    struct accessible_request request = {.item = node->item, .action = action, .scroll_target = 0};
    return accessibility_host_action(node->host, &request);
}

static HRESULT fragment_for(struct accessibility_node *_Nonnull node, struct accessible_item item,
                            IRawElementProviderFragment *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nullable found = nullptr;
    HRESULT result = accessibility_host_node(node->host, item, &found);
    if (SUCCEEDED(result))
        *out = &found->fragment; /* host_nodeが所有参照を1つ返す。 */
    return result;
}

static HRESULT simple_for(struct accessibility_node *_Nonnull node, struct accessible_item item,
                          IRawElementProviderSimple *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nullable found = nullptr;
    HRESULT result = accessibility_host_node(node->host, item, &found);
    if (SUCCEEDED(result))
        *out = &found->simple;
    return result;
}

static HRESULT STDMETHODCALLTYPE simple_query(IRawElementProviderSimple *_Nonnull self, REFIID id,
                                              void *_Nullable *_Nullable out)
{
    return query(simple_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE simple_add(IRawElementProviderSimple *_Nonnull self)
{
    return add_ref(simple_owner(self));
}

static ULONG STDMETHODCALLTYPE simple_release(IRawElementProviderSimple *_Nonnull self)
{
    return release(simple_owner(self));
}

static HRESULT STDMETHODCALLTYPE simple_options(IRawElementProviderSimple *_Nonnull self,
                                                enum ProviderOptions *_Nonnull out)
{
    (void)self;
    /* OwnsSetFocusで、拒否すべきmodalの背後へUIA自身が先にfocusを移すのを防ぐ。 */
    *out = ProviderOptions_ServerSideProvider | ProviderOptions_UseComThreading |
           ProviderOptions_ProviderOwnsSetFocus;
    return S_OK;
}

static IUnknown *_Nullable pattern_for(struct accessibility_node *_Nonnull node, PATTERNID id)
{
    enum accessible_kind kind = node->item.kind;
    const struct
    {
        PATTERNID id;
        IUnknown *_Nonnull value;
        bool supported;
    } patterns[] = {
        {UIA_InvokePatternId, (IUnknown *)&node->invoke, has_invoke(kind)},
        {UIA_SelectionPatternId, (IUnknown *)&node->selection, has_selection(kind)},
        {UIA_SelectionItemPatternId, (IUnknown *)&node->selection_item, has_selection_item(kind)},
        {UIA_ExpandCollapsePatternId, (IUnknown *)&node->expand, has_expand(kind)},
        {UIA_ScrollPatternId, (IUnknown *)&node->scroll, has_scroll(kind)},
        {UIA_ScrollItemPatternId, (IUnknown *)&node->scroll_item, has_scroll_item(node->item)},
        {UIA_TogglePatternId, (IUnknown *)&node->toggle, kind == ACCESSIBLE_SURFACE_TOGGLE}};
    for (size_t index = 0; index < sizeof patterns / sizeof patterns[0]; ++index)
    {
        if (patterns[index].id == id && patterns[index].supported)
        {
            return patterns[index].value;
        }
    }
    return nullptr;
}

static HRESULT STDMETHODCALLTYPE simple_pattern(IRawElementProviderSimple *_Nonnull self,
                                                PATTERNID id, IUnknown *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nonnull node = simple_owner(self);
    *out = nullptr;
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
    {
        return result;
    }
    *out = pattern_for(node, id);
    if (*out != nullptr)
    {
        (void)add_ref(node);
    }
    return S_OK;
}

static const CONTROLTYPEID control_types[] = {
    [ACCESSIBLE_WINDOW] = UIA_WindowControlTypeId,
    [ACCESSIBLE_DRAWER] = UIA_GroupControlTypeId,
    [ACCESSIBLE_HEADER] = UIA_ToolBarControlTypeId,
    [ACCESSIBLE_LAYOUT_GROUP] = UIA_GroupControlTypeId,
    [ACCESSIBLE_LAYOUT_DRAWER] = UIA_RadioButtonControlTypeId,
    [ACCESSIBLE_LAYOUT_BOTH] = UIA_RadioButtonControlTypeId,
    [ACCESSIBLE_LAYOUT_NOTE] = UIA_RadioButtonControlTypeId,
    [ACCESSIBLE_SETTINGS] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_CLOSE] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_PANE_TOOLS] = UIA_ToolBarControlTypeId,
    [ACCESSIBLE_BREADCRUMB] = UIA_TextControlTypeId,
    [ACCESSIBLE_MODE_GROUP] = UIA_GroupControlTypeId,
    [ACCESSIBLE_ACTION_GROUP] = UIA_GroupControlTypeId,
    [ACCESSIBLE_VIEW] = UIA_RadioButtonControlTypeId,
    [ACCESSIBLE_EDIT] = UIA_RadioButtonControlTypeId,
    [ACCESSIBLE_MENU] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_SAVE] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_HELP] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_FIND] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_NEW_NOTE] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_NEW_CATEGORY] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_TREE] = UIA_TreeControlTypeId,
    [ACCESSIBLE_CATEGORY] = UIA_TreeItemControlTypeId,
    [ACCESSIBLE_NOTE] = UIA_TreeItemControlTypeId,
    [ACCESSIBLE_FILTER] = UIA_EditControlTypeId,
    [ACCESSIBLE_BODY] = UIA_DocumentControlTypeId,
    [ACCESSIBLE_NOTICE] = UIA_StatusBarControlTypeId,
    [ACCESSIBLE_GUIDE] = UIA_TextControlTypeId,
    [ACCESSIBLE_SURFACE] = UIA_PaneControlTypeId,
    [ACCESSIBLE_SURFACE_LIST] = UIA_ListControlTypeId,
    [ACCESSIBLE_SURFACE_CHOICE] = UIA_ListItemControlTypeId,
    [ACCESSIBLE_SURFACE_LANGUAGE] = UIA_RadioButtonControlTypeId,
    [ACCESSIBLE_SURFACE_TOGGLE] = UIA_CheckBoxControlTypeId,
    [ACCESSIBLE_SURFACE_BUTTON] = UIA_ButtonControlTypeId,
    [ACCESSIBLE_SURFACE_TEXT] = UIA_TextControlTypeId,
    [ACCESSIBLE_SURFACE_HELP] = UIA_TextControlTypeId,
    [ACCESSIBLE_SURFACE_STATUS] = UIA_StatusBarControlTypeId,
    [ACCESSIBLE_COMMAND_INPUT] = UIA_EditControlTypeId,
    [ACCESSIBLE_REPLACE_PATTERN] = UIA_EditControlTypeId,
    [ACCESSIBLE_REPLACE_TEXT] = UIA_EditControlTypeId,
};

static void boolean_value(VARIANT *_Nonnull out, bool value)
{
    out->vt = VT_BOOL;
    out->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}

static void property_boolean(const struct accessible_view *_Nonnull view, PROPERTYID id,
                             VARIANT *_Nonnull out)
{
    const struct
    {
        PROPERTYID id;
        bool value;
    } properties[] = {{UIA_IsEnabledPropertyId, view->enabled},
                      {UIA_IsOffscreenPropertyId, view->offscreen},
                      {UIA_IsKeyboardFocusablePropertyId, view->focusable},
                      {UIA_HasKeyboardFocusPropertyId, view->focused},
                      {UIA_IsControlElementPropertyId, true},
                      {UIA_IsContentElementPropertyId, true},
                      {UIA_IsDialogPropertyId, view->dialog}};
    for (size_t index = 0; index < sizeof properties / sizeof properties[0]; ++index)
    {
        if (properties[index].id == id)
        {
            boolean_value(out, properties[index].value);
            return;
        }
    }
}

/* 文字列の性質。AcceleratorKey は持つ部品だけが答え、無ければ既定（空）に任せる。 */
static const char16_t *_Nullable property_text(const struct accessible_view *_Nonnull view,
                                               PROPERTYID id)
{
    if (id == UIA_AccessKeyPropertyId && view->access_key[0] != u'\0')
        return view->access_key;
    if (id == UIA_NamePropertyId)
    {
        return view->name;
    }
    if (id == UIA_HelpTextPropertyId)
    {
        return view->help;
    }
    if (id == UIA_ItemStatusPropertyId && view->status[0] != u'\0')
    {
        return view->status;
    }
    return id == UIA_AcceleratorKeyPropertyId && view->accelerator[0] != u'\0' ? view->accelerator
                                                                               : nullptr;
}

static bool property_position(const struct accessible_view *_Nonnull view, PROPERTYID id,
                              VARIANT *_Nonnull out)
{
    if ((id == UIA_PositionInSetPropertyId || id == UIA_SizeOfSetPropertyId) && view->set_size > 0)
    {
        out->vt = VT_I4;
        out->lVal = id == UIA_PositionInSetPropertyId ? view->position : view->set_size;
        return true;
    }
    return false;
}

static HRESULT property_value(const struct accessible_view *_Nonnull view, PROPERTYID id,
                              VARIANT *_Nonnull out)
{
    const char16_t *_Nullable text = property_text(view, id);
    if (text != nullptr)
    {
        out->vt = VT_BSTR;
        out->bstrVal = SysAllocString(text);
        return out->bstrVal == nullptr ? E_OUTOFMEMORY : S_OK;
    }
    if (property_position(view, id, out))
        return S_OK;
    if (id == UIA_LiveSettingPropertyId)
    {
        out->vt = VT_I4;
        out->lVal = view->live ? Polite : Off;
        return S_OK;
    }
    property_boolean(view, id, out);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE simple_property(IRawElementProviderSimple *_Nonnull self,
                                                 PROPERTYID id, VARIANT *_Nonnull out)
{
    VariantInit(out);
    struct accessibility_node *_Nonnull node = simple_owner(self);
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
    {
        return result;
    }
    if (id == UIA_ControlTypePropertyId)
    {
        out->vt = VT_I4;
        out->lVal = control_types[node->item.kind];
        if (view.menu && node->item.kind == ACCESSIBLE_SURFACE_LIST)
            out->lVal = UIA_MenuControlTypeId;
        if (view.menu && node->item.kind == ACCESSIBLE_SURFACE_CHOICE)
            out->lVal = UIA_MenuItemControlTypeId;
        return S_OK;
    }
    return property_value(&view, id, out);
}

static HRESULT STDMETHODCALLTYPE simple_host(IRawElementProviderSimple *_Nonnull self,
                                             IRawElementProviderSimple *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nonnull node = simple_owner(self);
    *out = nullptr;
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result) || !accessible_item_root(node->item))
    {
        return result;
    }
    HWND window = accessibility_host_window(node->host, node->item.kind);
    return window == nullptr ? (HRESULT)UIA_E_ELEMENTNOTAVAILABLE
                             : UiaHostProviderFromHwnd(window, out);
}

static HRESULT STDMETHODCALLTYPE fragment_query(IRawElementProviderFragment *_Nonnull self,
                                                REFIID id, void *_Nullable *_Nullable out)
{
    return query(fragment_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE fragment_add(IRawElementProviderFragment *_Nonnull self)
{
    return add_ref(fragment_owner(self));
}

static ULONG STDMETHODCALLTYPE fragment_release(IRawElementProviderFragment *_Nonnull self)
{
    return release(fragment_owner(self));
}

static bool navigation_intent(enum NavigateDirection direction,
                              enum accessible_navigation *_Nonnull out)
{
    switch (direction)
    {
    case NavigateDirection_Parent:
        *out = ACCESSIBLE_NAVIGATION_PARENT;
        return true;
    case NavigateDirection_NextSibling:
        *out = ACCESSIBLE_NAVIGATION_NEXT;
        return true;
    case NavigateDirection_PreviousSibling:
        *out = ACCESSIBLE_NAVIGATION_PREVIOUS;
        return true;
    case NavigateDirection_FirstChild:
        *out = ACCESSIBLE_NAVIGATION_FIRST;
        return true;
    case NavigateDirection_LastChild:
        *out = ACCESSIBLE_NAVIGATION_LAST;
        return true;
    }
    return false;
}

static HRESULT STDMETHODCALLTYPE
fragment_navigate(IRawElementProviderFragment *_Nonnull self, enum NavigateDirection direction,
                  IRawElementProviderFragment *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nonnull node = fragment_owner(self);
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    *out = nullptr;
    struct accessible_view view = {0};
    HRESULT available = read_view(node, &view);
    if (FAILED(available))
    {
        return available;
    }
    enum accessible_navigation intent = ACCESSIBLE_NAVIGATION_PARENT;
    if (!navigation_intent(direction, &intent))
    {
        return E_INVALIDARG;
    }
    HRESULT result = accessibility_host_navigate(node->host, &node->item, intent, &item);
    if (result == S_FALSE)
    {
        return S_OK;
    }
    return FAILED(result) ? result : fragment_for(node, item, out);
}

static HRESULT runtime_array(struct accessible_item item, SAFEARRAY *_Nullable *_Nonnull out)
{
    /* 16bitずつ分け、SDKのsigned intへ範囲外のcastを行わない。 */
    int parts[18] = {UiaAppendRuntimeId, (int)item.kind};
    uint64_t values[4] = {item.revision, (uint64_t)item.ref.category, (uint64_t)item.ref.note,
                          (uint64_t)item.slot};
    for (size_t group = 0; group < 4; ++group)
    {
        for (size_t part = 0; part < 4; ++part)
        {
            parts[2 + group * 4 + part] = (int)((values[group] >> (part * 16)) & 0xFFFFU);
        }
    }
    SAFEARRAY *_Nullable array = SafeArrayCreateVector(VT_I4, 0, 18);
    if (array == nullptr)
        return E_OUTOFMEMORY;
    for (LONG index = 0; index < 18; ++index)
    {
        HRESULT result = SafeArrayPutElement(array, &index, &parts[index]);
        if (FAILED(result))
        {
            SafeArrayDestroy(array);
            return result;
        }
    }
    *out = array;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE fragment_runtime(IRawElementProviderFragment *_Nonnull self,
                                                  SAFEARRAY *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nonnull node = fragment_owner(self);
    struct accessible_view view = {0};
    *out = nullptr;
    HRESULT result = read_view(node, &view);
    if (FAILED(result) || accessible_item_root(node->item))
        return result;
    return runtime_array(node->item, out);
}

static HRESULT STDMETHODCALLTYPE fragment_bounds(IRawElementProviderFragment *_Nonnull self,
                                                 struct UiaRect *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(fragment_owner(self), &view);
    if (SUCCEEDED(result))
    {
        *out = (struct UiaRect){view.bounds.left, view.bounds.top,
                                view.bounds.right - view.bounds.left,
                                view.bounds.bottom - view.bounds.top};
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE fragment_embedded(IRawElementProviderFragment *_Nonnull self,
                                                   SAFEARRAY *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessible_view view = {0};
    return read_view(fragment_owner(self), &view);
}

static HRESULT STDMETHODCALLTYPE fragment_focus(IRawElementProviderFragment *_Nonnull self)
{
    return perform(fragment_owner(self), ACCESSIBLE_ACTION_FOCUS);
}

static HRESULT STDMETHODCALLTYPE
fragment_root(IRawElementProviderFragment *_Nonnull self,
              IRawElementProviderFragmentRoot *_Nullable *_Nonnull out)
{
    struct accessibility_node *_Nonnull node = fragment_owner(self);
    struct accessible_view view = {0};
    *out = nullptr;
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
        return result;
    struct accessible_item item = {.kind = accessible_item_host(node->item)};
    struct accessibility_node *_Nullable root = nullptr;
    result = accessibility_host_node(node->host, item, &root);
    if (SUCCEEDED(result))
        *out = &root->root;
    return result;
}

static HRESULT STDMETHODCALLTYPE root_query(IRawElementProviderFragmentRoot *_Nonnull self,
                                            REFIID id, void *_Nullable *_Nullable out)
{
    return query(root_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE root_add(IRawElementProviderFragmentRoot *_Nonnull self)
{
    return add_ref(root_owner(self));
}

static ULONG STDMETHODCALLTYPE root_release(IRawElementProviderFragmentRoot *_Nonnull self)
{
    return release(root_owner(self));
}

static HRESULT STDMETHODCALLTYPE root_at(IRawElementProviderFragmentRoot *_Nonnull self, double x,
                                         double y,
                                         IRawElementProviderFragment *_Nullable *_Nonnull out)
{
    *out = nullptr;
    if (!isfinite(x) || !isfinite(y) || x < LONG_MIN || x > LONG_MAX || y < LONG_MIN ||
        y > LONG_MAX)
    {
        return E_INVALIDARG;
    }
    struct accessibility_node *_Nonnull node = root_owner(self);
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    POINT point = {(LONG)x, (LONG)y};
    HRESULT result = accessibility_host_at(node->host, node->item.kind, point, &item);
    if (result == S_FALSE)
    {
        return S_OK;
    }
    return FAILED(result) ? result : fragment_for(node, item, out);
}

static HRESULT STDMETHODCALLTYPE root_focus(IRawElementProviderFragmentRoot *_Nonnull self,
                                            IRawElementProviderFragment *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessibility_node *_Nonnull node = root_owner(self);
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    HRESULT result = accessibility_host_focus(node->host, node->item.kind, &item);
    if (result == S_FALSE)
    {
        return S_OK;
    }
    return FAILED(result) ? result : fragment_for(node, item, out);
}

static HRESULT STDMETHODCALLTYPE invoke_run(IInvokeProvider *_Nonnull self)
{
    struct accessibility_node *_Nonnull node = invoke_owner(self);
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
    {
        return result;
    }
    struct accessible_request request = {.item = node->item, .action = ACCESSIBLE_ACTION_INVOKE};
    return accessibility_host_post(node->host, &request);
}

static HRESULT selection_array(IRawElementProviderSimple *_Nullable selected,
                               SAFEARRAY *_Nullable *_Nonnull out)
{
    *out = SafeArrayCreateVector(VT_UNKNOWN, 0, selected == nullptr ? 0 : 1);
    if (*out == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    if (selected == nullptr)
    {
        return S_OK;
    }
    LONG index = 0;
    HRESULT result = SafeArrayPutElement(*out, &index, selected);
    if (FAILED(result))
    {
        SafeArrayDestroy(*out);
        *out = nullptr;
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE selection_get(ISelectionProvider *_Nonnull self,
                                               SAFEARRAY *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessibility_node *_Nonnull node = selection_owner(self);
    struct accessible_item item = {.kind = ACCESSIBLE_WINDOW};
    HRESULT result = accessibility_host_selected(node->host, &node->item, &item);
    if (FAILED(result))
    {
        return result;
    }
    IRawElementProviderSimple *_Nullable selected = nullptr;
    if (result != S_FALSE)
    {
        result = simple_for(node, item, &selected);
    }
    if (SUCCEEDED(result))
    {
        result = selection_array(selected, out);
    }
    if (selected != nullptr)
    {
        selected->lpVtbl->Release(selected);
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE selection_multiple(ISelectionProvider *_Nonnull self,
                                                    BOOL *_Nonnull out)
{
    struct accessible_view view = {0};
    *out = FALSE;
    return read_view(selection_owner(self), &view);
}

static HRESULT STDMETHODCALLTYPE selection_required(ISelectionProvider *_Nonnull self,
                                                    BOOL *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(selection_owner(self), &view);
    if (SUCCEEDED(result))
    {
        *out = view.selection_required;
    }
    return result;
}

static struct accessible_item selection_container(struct accessible_item item)
{
    if (accessible_item_surface(item))
        return (struct accessible_item){.kind = ACCESSIBLE_SURFACE_LIST, .revision = item.revision};
    enum accessible_kind kind = item.kind;
    if (kind == ACCESSIBLE_VIEW || kind == ACCESSIBLE_EDIT)
    {
        return (struct accessible_item){.kind = ACCESSIBLE_MODE_GROUP};
    }
    return (struct accessible_item){.kind = kind == ACCESSIBLE_NOTE ? ACCESSIBLE_TREE
                                                                    : ACCESSIBLE_LAYOUT_GROUP};
}

static HRESULT STDMETHODCALLTYPE selection_item_select(ISelectionItemProvider *_Nonnull self)
{
    return perform(selection_item_owner(self), ACCESSIBLE_ACTION_SELECT);
}

static HRESULT STDMETHODCALLTYPE selection_item_add_selection(ISelectionItemProvider *_Nonnull self)
{
    struct accessibility_node *_Nonnull node = selection_item_owner(self);
    struct accessible_view view = {0};
    HRESULT available = read_view(node, &view);
    if (FAILED(available))
    {
        return available;
    }
    struct accessible_item group = selection_container(node->item);
    struct accessible_item selected = {.kind = ACCESSIBLE_WINDOW};
    HRESULT result = accessibility_host_selected(node->host, &group, &selected);
    if (FAILED(result))
    {
        return result;
    }
    if (result != S_FALSE && !accessible_item_equal(selected, node->item))
    {
        return (HRESULT)UIA_E_INVALIDOPERATION;
    }
    return perform(node, ACCESSIBLE_ACTION_SELECT);
}

static HRESULT STDMETHODCALLTYPE selection_item_remove(ISelectionItemProvider *_Nonnull self)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(selection_item_owner(self), &view);
    if (FAILED(result))
    {
        return result;
    }
    /* 選択解除だけで保持文書を捨てない。新しい選択はSelectで保存経路を通す。 */
    return view.selected ? (HRESULT)UIA_E_INVALIDOPERATION : S_OK;
}

static HRESULT STDMETHODCALLTYPE selection_item_selected(ISelectionItemProvider *_Nonnull self,
                                                         BOOL *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(selection_item_owner(self), &view);
    if (SUCCEEDED(result))
    {
        *out = view.selected;
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE selection_item_container(
    ISelectionItemProvider *_Nonnull self, IRawElementProviderSimple *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessibility_node *_Nonnull node = selection_item_owner(self);
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
    {
        return result;
    }
    struct accessible_item group = selection_container(node->item);
    return simple_for(node, group, out);
}

static HRESULT STDMETHODCALLTYPE expand_open(IExpandCollapseProvider *_Nonnull self)
{
    struct accessibility_node *_Nonnull node = expand_owner(self);
    if (node->item.kind == ACCESSIBLE_MENU)
    {
        struct accessible_request request = {.item = node->item,
                                             .action = ACCESSIBLE_ACTION_EXPAND};
        return accessibility_host_post(node->host, &request);
    }
    return perform(node, ACCESSIBLE_ACTION_EXPAND);
}

static HRESULT STDMETHODCALLTYPE expand_close(IExpandCollapseProvider *_Nonnull self)
{
    return perform(expand_owner(self), ACCESSIBLE_ACTION_COLLAPSE);
}

static enum ExpandCollapseState expand_state(struct accessibility_node *_Nonnull node,
                                             const struct accessible_view *_Nonnull view)
{
    if (node->item.kind == ACCESSIBLE_NOTE)
    {
        return ExpandCollapseState_LeafNode;
    }
    return view->expanded ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;
}

static HRESULT STDMETHODCALLTYPE expand_get(IExpandCollapseProvider *_Nonnull self,
                                            enum ExpandCollapseState *_Nonnull out)
{
    struct accessibility_node *_Nonnull node = expand_owner(self);
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (SUCCEEDED(result))
    {
        *out = expand_state(node, &view);
    }
    return result;
}

static HRESULT scroll_to(struct accessibility_node *_Nonnull node, int target)
{
    struct accessible_request request = {
        .item = node->item, .action = ACCESSIBLE_ACTION_SCROLL, .scroll_target = target};
    return accessibility_host_action(node->host, &request);
}

static HRESULT scroll_delta(enum ScrollAmount amount, int page, int *_Nonnull out)
{
    switch (amount)
    {
    case ScrollAmount_NoAmount:
        *out = 0;
        return S_OK;
    case ScrollAmount_LargeDecrement:
        *out = -page;
        return S_OK;
    case ScrollAmount_SmallDecrement:
        *out = -(page / 10 + 1);
        return S_OK;
    case ScrollAmount_LargeIncrement:
        *out = page;
        return S_OK;
    case ScrollAmount_SmallIncrement:
        *out = page / 10 + 1;
        return S_OK;
    }
    return E_INVALIDARG;
}

static HRESULT STDMETHODCALLTYPE scroll_run(IScrollProvider *_Nonnull self,
                                            enum ScrollAmount horizontal,
                                            enum ScrollAmount vertical)
{
    struct accessibility_node *_Nonnull node = scroll_owner(self);
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result))
    {
        return result;
    }
    if (horizontal != ScrollAmount_NoAmount)
    {
        return (HRESULT)UIA_E_INVALIDOPERATION;
    }
    int delta = 0;
    result = scroll_delta(vertical, view.viewport, &delta);
    if (FAILED(result))
    {
        return result;
    }
    if (view.scroll_limit == 0 && delta != 0)
    {
        return (HRESULT)UIA_E_INVALIDOPERATION;
    }
    int64_t target = (int64_t)view.scroll + delta;
    target = target < 0 ? 0 : target;
    target = target > view.scroll_limit ? view.scroll_limit : target;
    return scroll_to(node, (int)target);
}

static HRESULT STDMETHODCALLTYPE scroll_percent(IScrollProvider *_Nonnull self, double horizontal,
                                                double vertical)
{
    if (!isfinite(horizontal) || !isfinite(vertical))
    {
        return E_INVALIDARG;
    }
    if (horizontal != UIA_ScrollPatternNoScroll)
    {
        return (HRESULT)UIA_E_INVALIDOPERATION;
    }
    struct accessibility_node *_Nonnull node = scroll_owner(self);
    struct accessible_view view = {0};
    HRESULT result = read_view(node, &view);
    if (FAILED(result) || vertical == UIA_ScrollPatternNoScroll)
    {
        return result;
    }
    if (vertical < 0 || vertical > 100)
    {
        return E_INVALIDARG;
    }
    if (view.scroll_limit == 0)
    {
        return (HRESULT)UIA_E_INVALIDOPERATION;
    }
    return scroll_to(node, (int)((double)view.scroll_limit * vertical / 100));
}

static double vertical_percent(const struct accessible_view *_Nonnull view)
{
    return view->scroll_limit == 0 ? UIA_ScrollPatternNoScroll
                                   : (double)view->scroll * 100 / view->scroll_limit;
}

static double vertical_size(const struct accessible_view *_Nonnull view)
{
    double total = (double)view->scroll_limit + view->viewport;
    return total <= 0 ? 100 : (double)view->viewport * 100 / total;
}

static HRESULT STDMETHODCALLTYPE scroll_horizontal_percent(IScrollProvider *_Nonnull self,
                                                           double *_Nonnull out)
{
    struct accessible_view view = {0};
    *out = UIA_ScrollPatternNoScroll;
    return read_view(scroll_owner(self), &view);
}

static HRESULT STDMETHODCALLTYPE scroll_vertical_percent(IScrollProvider *_Nonnull self,
                                                         double *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(scroll_owner(self), &view);
    if (SUCCEEDED(result))
    {
        *out = vertical_percent(&view);
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE scroll_horizontal_size(IScrollProvider *_Nonnull self,
                                                        double *_Nonnull out)
{
    struct accessible_view view = {0};
    *out = 100;
    return read_view(scroll_owner(self), &view);
}

static HRESULT STDMETHODCALLTYPE scroll_vertical_size(IScrollProvider *_Nonnull self,
                                                      double *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(scroll_owner(self), &view);
    if (SUCCEEDED(result))
    {
        *out = vertical_size(&view);
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE scroll_horizontal_enabled(IScrollProvider *_Nonnull self,
                                                           BOOL *_Nonnull out)
{
    struct accessible_view view = {0};
    *out = FALSE;
    return read_view(scroll_owner(self), &view);
}

static HRESULT STDMETHODCALLTYPE scroll_vertical_enabled(IScrollProvider *_Nonnull self,
                                                         BOOL *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(scroll_owner(self), &view);
    if (SUCCEEDED(result))
    {
        *out = view.scroll_limit > 0;
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE scroll_item_reveal(IScrollItemProvider *_Nonnull self)
{
    return perform(scroll_item_owner(self), ACCESSIBLE_ACTION_REVEAL);
}

static HRESULT STDMETHODCALLTYPE invoke_query(IInvokeProvider *_Nonnull self, REFIID id,
                                              void *_Nullable *_Nullable out)
{
    return query(invoke_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE invoke_add(IInvokeProvider *_Nonnull self)
{
    return add_ref(invoke_owner(self));
}

static ULONG STDMETHODCALLTYPE invoke_release(IInvokeProvider *_Nonnull self)
{
    return release(invoke_owner(self));
}

static HRESULT STDMETHODCALLTYPE selection_query(ISelectionProvider *_Nonnull self, REFIID id,
                                                 void *_Nullable *_Nullable out)
{
    return query(selection_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE selection_add(ISelectionProvider *_Nonnull self)
{
    return add_ref(selection_owner(self));
}

static ULONG STDMETHODCALLTYPE selection_release(ISelectionProvider *_Nonnull self)
{
    return release(selection_owner(self));
}

static HRESULT STDMETHODCALLTYPE selection_item_query(ISelectionItemProvider *_Nonnull self,
                                                      REFIID id, void *_Nullable *_Nullable out)
{
    return query(selection_item_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE selection_item_add(ISelectionItemProvider *_Nonnull self)
{
    return add_ref(selection_item_owner(self));
}

static ULONG STDMETHODCALLTYPE selection_item_release(ISelectionItemProvider *_Nonnull self)
{
    return release(selection_item_owner(self));
}

static HRESULT STDMETHODCALLTYPE expand_query(IExpandCollapseProvider *_Nonnull self, REFIID id,
                                              void *_Nullable *_Nullable out)
{
    return query(expand_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE expand_add(IExpandCollapseProvider *_Nonnull self)
{
    return add_ref(expand_owner(self));
}

static ULONG STDMETHODCALLTYPE expand_release(IExpandCollapseProvider *_Nonnull self)
{
    return release(expand_owner(self));
}

static HRESULT STDMETHODCALLTYPE scroll_query(IScrollProvider *_Nonnull self, REFIID id,
                                              void *_Nullable *_Nullable out)
{
    return query(scroll_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE scroll_add(IScrollProvider *_Nonnull self)
{
    return add_ref(scroll_owner(self));
}

static ULONG STDMETHODCALLTYPE scroll_release(IScrollProvider *_Nonnull self)
{
    return release(scroll_owner(self));
}

static HRESULT STDMETHODCALLTYPE scroll_item_query(IScrollItemProvider *_Nonnull self, REFIID id,
                                                   void *_Nullable *_Nullable out)
{
    return query(scroll_item_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE scroll_item_add(IScrollItemProvider *_Nonnull self)
{
    return add_ref(scroll_item_owner(self));
}

static ULONG STDMETHODCALLTYPE scroll_item_release(IScrollItemProvider *_Nonnull self)
{
    return release(scroll_item_owner(self));
}

static HRESULT STDMETHODCALLTYPE toggle_query(IToggleProvider *_Nonnull self, REFIID id,
                                              void *_Nullable *_Nullable out)
{
    return query(toggle_owner(self), id, out);
}

static ULONG STDMETHODCALLTYPE toggle_add(IToggleProvider *_Nonnull self)
{
    return add_ref(toggle_owner(self));
}

static ULONG STDMETHODCALLTYPE toggle_release(IToggleProvider *_Nonnull self)
{
    return release(toggle_owner(self));
}

static HRESULT STDMETHODCALLTYPE toggle_run(IToggleProvider *_Nonnull self)
{
    return perform(toggle_owner(self), ACCESSIBLE_ACTION_SELECT);
}

static HRESULT STDMETHODCALLTYPE toggle_get(IToggleProvider *_Nonnull self,
                                            enum ToggleState *_Nonnull out)
{
    struct accessible_view view = {0};
    HRESULT result = read_view(toggle_owner(self), &view);
    if (SUCCEEDED(result))
        *out = view.selected ? ToggleState_On : ToggleState_Off;
    return result;
}

static IToggleProviderVtbl toggle_table = {toggle_query, toggle_add, toggle_release, toggle_run,
                                           toggle_get};

static IRawElementProviderSimpleVtbl simple_table = {
    simple_query,   simple_add,      simple_release, simple_options,
    simple_pattern, simple_property, simple_host};

static IRawElementProviderFragmentVtbl fragment_table = {
    fragment_query,  fragment_add,      fragment_release, fragment_navigate, fragment_runtime,
    fragment_bounds, fragment_embedded, fragment_focus,   fragment_root};

static IRawElementProviderFragmentRootVtbl root_table = {root_query, root_add, root_release,
                                                         root_at, root_focus};

static IInvokeProviderVtbl invoke_table = {invoke_query, invoke_add, invoke_release, invoke_run};

static ISelectionProviderVtbl selection_table = {selection_query,    selection_add,
                                                 selection_release,  selection_get,
                                                 selection_multiple, selection_required};

static ISelectionItemProviderVtbl selection_item_table = {
    selection_item_query,    selection_item_add,           selection_item_release,
    selection_item_select,   selection_item_add_selection, selection_item_remove,
    selection_item_selected, selection_item_container};

static IExpandCollapseProviderVtbl expand_table = {expand_query, expand_add,   expand_release,
                                                   expand_open,  expand_close, expand_get};

static IScrollProviderVtbl scroll_table = {scroll_query,
                                           scroll_add,
                                           scroll_release,
                                           scroll_run,
                                           scroll_percent,
                                           scroll_horizontal_percent,
                                           scroll_vertical_percent,
                                           scroll_horizontal_size,
                                           scroll_vertical_size,
                                           scroll_horizontal_enabled,
                                           scroll_vertical_enabled};

static IScrollItemProviderVtbl scroll_item_table = {scroll_item_query, scroll_item_add,
                                                    scroll_item_release, scroll_item_reveal};

HRESULT accessibility_node_create(struct accessibility_host *_Nonnull host,
                                  struct accessible_item item,
                                  struct accessibility_node *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct accessibility_node *_Nullable node = calloc(1, sizeof *node);
    if (node == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    node->host = host;
    node->item = item;
    node->references = 1;
    node->simple.lpVtbl = &simple_table;
    node->fragment.lpVtbl = &fragment_table;
    node->root.lpVtbl = &root_table;
    node->invoke.lpVtbl = &invoke_table;
    node->selection.lpVtbl = &selection_table;
    node->selection_item.lpVtbl = &selection_item_table;
    node->expand.lpVtbl = &expand_table;
    node->scroll.lpVtbl = &scroll_table;
    node->scroll_item.lpVtbl = &scroll_item_table;
    node->toggle.lpVtbl = &toggle_table;
    accessibility_host_add_ref(host);
    *out = node;
    return S_OK;
}

void accessibility_node_add_ref(struct accessibility_node *_Nonnull node)
{
    (void)add_ref(node);
}

void accessibility_node_release(struct accessibility_node *_Nonnull node)
{
    (void)release(node);
}

void accessibility_node_disconnect(struct accessibility_node *_Nonnull node)
{
    node->disconnected = true; /* 再展開後も切断済み参照を復活させない。 */
    (void)UiaDisconnectProvider(&node->simple);
}

void accessibility_node_structure_changed(struct accessibility_node *_Nonnull node)
{
    (void)UiaRaiseStructureChangedEvent(&node->simple, StructureChangeType_ChildrenInvalidated,
                                        nullptr, 0);
}

void accessibility_node_focus_changed(struct accessibility_node *_Nonnull node)
{
    (void)UiaRaiseAutomationEvent(&node->simple, UIA_AutomationFocusChangedEventId);
}

void accessibility_node_invoked(struct accessibility_node *_Nonnull node)
{
    if (has_invoke(node->item.kind))
    {
        (void)UiaRaiseAutomationEvent(&node->simple, UIA_Invoke_InvokedEventId);
    }
}

void accessibility_node_notify(struct accessibility_node *_Nonnull node,
                               const char16_t *_Nonnull text)
{
    if (!UiaClientsAreListening())
    {
        return;
    }
    BSTR display = SysAllocString(text);
    BSTR activity = SysAllocString(u"NeNeFolio.Saved");
    if (display != nullptr && activity != nullptr)
    {
        (void)UiaRaiseNotificationEvent(&node->simple, NotificationKind_ActionCompleted,
                                        NotificationProcessing_MostRecent, display, activity);
    }
    SysFreeString(display);
    SysFreeString(activity);
}

void accessibility_node_selection_changed(struct accessibility_node *_Nonnull node)
{
    EVENTID event = has_selection(node->item.kind) ? UIA_Selection_InvalidatedEventId
                                                   : UIA_SelectionItem_ElementSelectedEventId;
    (void)UiaRaiseAutomationEvent(&node->simple, event);
}

bool accessibility_node_is_request(LPARAM lparam)
{
    return (LONG)lparam == UiaRootObjectId;
}

LRESULT accessibility_node_object(struct accessibility_node *_Nonnull node, HWND window,
                                  WPARAM wparam, LPARAM lparam)
{
    return UiaReturnRawElementProvider(window, wparam, lparam, &node->simple);
}

struct accessible_item accessibility_node_item(const struct accessibility_node *_Nonnull node)
{
    return node->item;
}

IRawElementProviderSimple *_Nonnull accessibility_node_simple(
    struct accessibility_node *_Nonnull node)
{
    return &node->simple;
}

IRawElementProviderFragment *_Nonnull accessibility_node_fragment(
    struct accessibility_node *_Nonnull node)
{
    return &node->fragment;
}

static void raise_boolean(struct accessibility_node *_Nonnull node, PROPERTYID property,
                          bool before, bool after)
{
    if (before == after)
    {
        return;
    }
    VARIANT old_value;
    VARIANT new_value;
    VariantInit(&old_value);
    VariantInit(&new_value);
    boolean_value(&old_value, before);
    boolean_value(&new_value, after);
    (void)UiaRaiseAutomationPropertyChangedEvent(&node->simple, property, old_value, new_value);
}

static void raise_number(struct accessibility_node *_Nonnull node, PROPERTYID property,
                         double before, double after)
{
    if (before == after)
    {
        return;
    }
    VARIANT old_value = {.vt = VT_R8, .dblVal = before};
    VARIANT new_value = {.vt = VT_R8, .dblVal = after};
    (void)UiaRaiseAutomationPropertyChangedEvent(&node->simple, property, old_value, new_value);
}

/* 文字列の性質（Name・ItemStatus）の変化。 */
static void raise_text(struct accessibility_node *_Nonnull node, PROPERTYID property,
                       const char16_t *_Nonnull before, const char16_t *_Nonnull after)
{
    if (wcscmp(before, after) == 0)
    {
        return;
    }
    VARIANT old_value = {.vt = VT_BSTR, .bstrVal = SysAllocString(before)};
    VARIANT new_value = {.vt = VT_BSTR, .bstrVal = SysAllocString(after)};
    if (old_value.bstrVal != nullptr && new_value.bstrVal != nullptr)
    {
        (void)UiaRaiseAutomationPropertyChangedEvent(&node->simple, property, old_value, new_value);
    }
    VariantClear(&old_value);
    VariantClear(&new_value);
}

static SAFEARRAY *_Nullable bounds_array(RECT bounds)
{
    SAFEARRAY *_Nullable array = SafeArrayCreateVector(VT_R8, 0, 4);
    if (array == nullptr)
    {
        return nullptr;
    }
    double parts[4] = {bounds.left, bounds.top, bounds.right - bounds.left,
                       bounds.bottom - bounds.top};
    for (LONG index = 0; index < 4; ++index)
    {
        if (FAILED(SafeArrayPutElement(array, &index, &parts[index])))
        {
            SafeArrayDestroy(array);
            return nullptr;
        }
    }
    return array;
}

static void raise_bounds(struct accessibility_node *_Nonnull node, RECT before, RECT after)
{
    if (EqualRect(&before, &after))
    {
        return;
    }
    VARIANT old_value = {.vt = VT_ARRAY | VT_R8, .parray = bounds_array(before)};
    VARIANT new_value = {.vt = VT_ARRAY | VT_R8, .parray = bounds_array(after)};
    if (old_value.parray != nullptr && new_value.parray != nullptr)
    {
        (void)UiaRaiseAutomationPropertyChangedEvent(&node->simple, UIA_BoundingRectanglePropertyId,
                                                     old_value, new_value);
    }
    VariantClear(&old_value);
    VariantClear(&new_value);
}

static void raise_expand(struct accessibility_node *_Nonnull node,
                         const struct accessible_view *_Nonnull before,
                         const struct accessible_view *_Nonnull after)
{
    if (!has_expand(node->item.kind) || before->expanded == after->expanded)
    {
        return;
    }
    VARIANT old_value = {.vt = VT_I4, .lVal = (LONG)expand_state(node, before)};
    VARIANT new_value = {.vt = VT_I4, .lVal = (LONG)expand_state(node, after)};
    (void)UiaRaiseAutomationPropertyChangedEvent(
        &node->simple, UIA_ExpandCollapseExpandCollapseStatePropertyId, old_value, new_value);
}

static void raise_patterns(struct accessibility_node *_Nonnull node,
                           const struct accessible_view *_Nonnull before,
                           const struct accessible_view *_Nonnull after)
{
    raise_expand(node, before, after);
    if (node->item.kind == ACCESSIBLE_SURFACE_TOGGLE && before->selected != after->selected)
    {
        VARIANT old_value = {.vt = VT_I4,
                             .lVal = before->selected ? ToggleState_On : ToggleState_Off};
        VARIANT new_value = {.vt = VT_I4,
                             .lVal = after->selected ? ToggleState_On : ToggleState_Off};
        (void)UiaRaiseAutomationPropertyChangedEvent(&node->simple, UIA_ToggleToggleStatePropertyId,
                                                     old_value, new_value);
    }
    if (has_selection_item(node->item.kind))
    {
        raise_boolean(node, UIA_SelectionItemIsSelectedPropertyId, before->selected,
                      after->selected);
    }
    if (has_selection(node->item.kind))
    {
        raise_boolean(node, UIA_SelectionIsSelectionRequiredPropertyId, before->selection_required,
                      after->selection_required);
    }
    if (has_scroll(node->item.kind))
    {
        raise_boolean(node, UIA_ScrollVerticallyScrollablePropertyId, before->scroll_limit > 0,
                      after->scroll_limit > 0);
        raise_number(node, UIA_ScrollVerticalScrollPercentPropertyId, vertical_percent(before),
                     vertical_percent(after));
        raise_number(node, UIA_ScrollVerticalViewSizePropertyId, vertical_size(before),
                     vertical_size(after));
    }
}

void accessibility_node_refresh(struct accessibility_node *_Nonnull node)
{
    struct accessible_view view = {0};
    if (FAILED(read_view(node, &view)))
    {
        return;
    }
    struct accessible_view before = node->previous;
    bool observed = node->observed;
    node->previous = view;
    node->observed = true; /* 通知が再入しても同じ差分を繰り返さない。 */
    (void)add_ref(node);
    if (view.live && !view.offscreen && view.name[0] != u'\0' &&
        (!observed || before.offscreen || wcscmp(before.name, view.name) != 0))
        (void)UiaRaiseAutomationEvent(&node->simple, UIA_LiveRegionChangedEventId);
    if (!observed)
    {
        (void)release(node);
        return;
    }
    raise_text(node, UIA_NamePropertyId, before.name, view.name);
    raise_text(node, UIA_ItemStatusPropertyId, before.status, view.status);
    raise_bounds(node, before.bounds, view.bounds);
    raise_boolean(node, UIA_IsEnabledPropertyId, before.enabled, view.enabled);
    raise_boolean(node, UIA_IsOffscreenPropertyId, before.offscreen, view.offscreen);
    raise_boolean(node, UIA_IsKeyboardFocusablePropertyId, before.focusable, view.focusable);
    raise_boolean(node, UIA_HasKeyboardFocusPropertyId, before.focused, view.focused);
    raise_patterns(node, &before, &view);
    (void)release(node);
}

void accessibility_node_menu_event(struct accessibility_node *_Nonnull node,
                                   enum accessible_menu_event event)
{
    EVENTID id = UIA_MenuOpenedEventId;
    switch (event)
    {
    case ACCESSIBLE_MENU_OPENED:
        id = UIA_MenuOpenedEventId;
        break;
    case ACCESSIBLE_MENU_CLOSED:
        id = UIA_MenuClosedEventId;
        break;
    }
    (void)UiaRaiseAutomationEvent(&node->simple, id);
}
