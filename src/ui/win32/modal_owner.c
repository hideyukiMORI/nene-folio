#include "modal_owner.h"
#include "folio_message.h"
#include "ui_part.h"
#include <stdlib.h>
#include <string.h>

struct modal_owner
{
    HWND _Nonnull root;
    HWND _Nullable proxy;
    HWND _Nullable found;
    HWND _Nullable focus;
    unsigned found_depth;
    RECT previous;
    bool arranging;
    bool minimized;
    bool resizing;
    int move_x;
    int move_y;
};

static const wchar_t owner_class[] = L"NeNeFolioModalOwner";
static const wchar_t positioned_property[] = L"NeNeFolio.ModalPositioned";

static LRESULT CALLBACK owner_procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_MOUSEACTIVATE)
        return MA_NOACTIVATE;
    if (message == WM_ENTERIDLE && wparam == MSGF_DIALOGBOX && lparam != 0)
    {
        HWND root = modal_owner_root(window);
        if (root != nullptr)
            modal_owner_anchor(root, (HWND)lparam);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

static bool register_owner(void)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW existing = {0};
    if (GetClassInfoW(instance, owner_class, &existing))
        return true;
    WNDCLASSW kind = {
        .lpfnWndProc = owner_procedure, .hInstance = instance, .lpszClassName = owner_class};
    return RegisterClassW(&kind) != 0;
}

enum modal_owner_outcome modal_owner_create(HWND _Nonnull root,
                                            struct modal_owner *_Nullable *_Nonnull out)
{
    *out = nullptr;
    struct modal_owner *owner = malloc(sizeof *owner);
    if (owner == nullptr)
        return MODAL_OWNER_UNAVAILABLE;
    *owner = (struct modal_owner){.root = root};
    GetWindowRect(root, &owner->previous);
    if (register_owner())
        owner->proxy = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, owner_class, L"",
                                       WS_POPUP, owner->previous.left, owner->previous.top,
                                       owner->previous.right - owner->previous.left,
                                       owner->previous.bottom - owner->previous.top, root, nullptr,
                                       GetModuleHandleW(nullptr), nullptr);
    if (owner->proxy == nullptr)
    {
        modal_owner_destroy(owner);
        return MODAL_OWNER_UNAVAILABLE;
    }
    *out = owner;
    return MODAL_OWNER_READY;
}

void modal_owner_destroy(struct modal_owner *_Nullable owner)
{
    if (owner == nullptr)
        return;
    if (owner->proxy != nullptr && IsWindow(owner->proxy))
        DestroyWindow(owner->proxy);
    free(owner);
}

HWND _Nullable modal_owner_root(HWND _Nullable window)
{
    return window == nullptr ? nullptr : GetAncestor(window, GA_ROOTOWNER);
}

RECT modal_owner_work_area(HWND _Nonnull root)
{
    RECT area;
    GetClientRect(root, &area);
    area.top += MulDiv(ui_part_surface_title_height, (int)GetDpiForWindow(root), 96);
    MapWindowPoints(root, nullptr, (POINT *)&area, 2);
    return area;
}

static bool owner_dialog(const struct modal_owner *_Nonnull owner, HWND window)
{
    wchar_t kind[32];
    return window != owner->root && window != owner->proxy &&
           modal_owner_root(window) == owner->root && GetClassNameW(window, kind, 32) != 0 &&
           wcscmp(kind, L"#32770") == 0;
}

static unsigned ownership_depth(HWND window)
{
    unsigned depth = 0;
    while ((window = GetWindow(window, GW_OWNER)) != nullptr)
        depth += 1;
    return depth;
}

static BOOL CALLBACK find_dialog(HWND window, LPARAM lparam)
{
    struct modal_owner *owner = (struct modal_owner *)lparam;
    if (owner_dialog(owner, window) && IsWindowVisible(window))
    {
        unsigned depth = ownership_depth(window);
        if (owner->found == nullptr || depth > owner->found_depth)
        {
            owner->found = window;
            owner->found_depth = depth;
        }
    }
    return TRUE;
}

HWND _Nonnull modal_owner_current(struct modal_owner *_Nonnull owner)
{
    owner->found = nullptr;
    owner->found_depth = 0;
    EnumThreadWindows(GetWindowThreadProcessId(owner->root, nullptr), find_dialog, (LPARAM)owner);
    return owner->found == nullptr ? owner->proxy : owner->found;
}

enum modal_cancel_outcome modal_owner_cancel(struct modal_owner *_Nonnull owner)
{
    HWND current = modal_owner_current(owner);
    if (current == owner->proxy)
        return MODAL_CANCEL_NOT_VISIBLE;
    LRESULT result = SendMessageW(current, folio_message_modal_cancel, 0, 0);
    if (result == 2)
        return MODAL_CANCEL_REJECTED;
    if (result == 0)
        SendMessageW(current, WM_CLOSE, 0, 0);
    return MODAL_CANCEL_ACCEPTED;
}

void modal_owner_capture_focus(struct modal_owner *_Nonnull owner)
{
    HWND focus = GetFocus();
    HWND current = modal_owner_current(owner);
    if (focus != nullptr && (focus == current || IsChild(current, focus)))
        owner->focus = focus;
}

void modal_owner_restore_focus(struct modal_owner *_Nonnull owner)
{
    HWND current = modal_owner_current(owner);
    if (current == owner->proxy || !IsWindowEnabled(current))
        return;
    SetActiveWindow(current);
    HWND focus = owner->focus;
    if (focus == nullptr || !IsWindow(focus) || !IsWindowVisible(focus) ||
        !IsWindowEnabled(focus) || (focus != current && !IsChild(current, focus)))
        focus = current;
    SetFocus(focus);
}

static RECT fit_monitor(RECT bounds)
{
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    if (!GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor))
        return bounds;
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    int left = bounds.left < monitor.rcWork.left ? monitor.rcWork.left : bounds.left;
    int top = bounds.top < monitor.rcWork.top ? monitor.rcWork.top : bounds.top;
    left = left + width > monitor.rcWork.right ? monitor.rcWork.right - width : left;
    top = top + height > monitor.rcWork.bottom ? monitor.rcWork.bottom - height : top;
    return (RECT){left, top, left + width, top + height};
}

void modal_owner_positioned(HWND _Nonnull dialog)
{
    SetPropW(dialog, positioned_property, modal_owner_root(dialog));
}

/* OS面を縮められない場合も、置ける側があれば主ヘッダーを露出させる。 */
static RECT avoid_header(HWND root, RECT bounds)
{
    RECT header;
    GetClientRect(root, &header);
    int edge = MulDiv(ui_part_edge, (int)GetDpiForWindow(root), 96);
    header.bottom = MulDiv(ui_part_surface_title_height, (int)GetDpiForWindow(root), 96);
    MapWindowPoints(root, nullptr, (POINT *)&header, 2);
    RECT overlap;
    if (!IntersectRect(&overlap, &header, &bounds))
        return bounds;
    MONITORINFO monitor = {.cbSize = sizeof monitor};
    if (!GetMonitorInfoW(MonitorFromRect(&bounds, MONITOR_DEFAULTTONEAREST), &monitor))
        return bounds;
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    if (header.top - edge - height >= monitor.rcWork.top)
        OffsetRect(&bounds, 0, header.top - edge - bounds.bottom);
    else if (header.right + edge + width <= monitor.rcWork.right)
        OffsetRect(&bounds, header.right + edge - bounds.left, 0);
    else if (header.left - edge - width >= monitor.rcWork.left)
        OffsetRect(&bounds, header.left - edge - bounds.right, 0);
    return bounds;
}

void modal_owner_anchor(HWND _Nonnull root, HWND _Nonnull dialog)
{
    wchar_t kind[32];
    if (modal_owner_root(dialog) != root || GetPropW(dialog, positioned_property) != nullptr ||
        GetClassNameW(dialog, kind, 32) == 0 || wcscmp(kind, L"#32770") != 0)
        return;
    RECT bounds;
    GetWindowRect(dialog, &bounds);
    int width = bounds.right - bounds.left;
    int height = bounds.bottom - bounds.top;
    RECT work = modal_owner_work_area(root);
    int edge = MulDiv(ui_part_edge, (int)GetDpiForWindow(root), 96);
    int left = (work.left + work.right - width) / 2;
    int top = (work.top + work.bottom - height) / 2;
    top = top < work.top + edge ? work.top + edge : top;
    bounds = fit_monitor((RECT){left, top, left + width, top + height});
    bounds = avoid_header(root, bounds);
    if (SetWindowPos(dialog, nullptr, bounds.left, bounds.top, 0, 0,
                     SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE))
        SetPropW(dialog, positioned_property, dialog);
}

static BOOL CALLBACK arrange_dialog(HWND window, LPARAM lparam)
{
    struct modal_owner *owner = (struct modal_owner *)lparam;
    if (!owner_dialog(owner, window))
        return TRUE;
    RECT bounds;
    GetWindowRect(window, &bounds);
    OffsetRect(&bounds, owner->move_x, owner->move_y);
    bounds = fit_monitor(bounds);
    SetWindowPos(window, nullptr, bounds.left, bounds.top, 0, 0,
                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    if (owner->resizing)
    {
        if (GetPropW(window, positioned_property) == owner->root)
            SendMessageW(window, folio_message_modal_reflow, 0, 0);
        else
        {
            RemovePropW(window, positioned_property);
            modal_owner_anchor(owner->root, window);
        }
    }
    return TRUE;
}

void modal_owner_arrange(struct modal_owner *_Nonnull owner)
{
    if (owner->arranging)
        return;
    if (IsIconic(owner->root))
    {
        owner->minimized = true;
        return;
    }
    RECT bounds;
    GetWindowRect(owner->root, &bounds);
    owner->move_x = bounds.left - owner->previous.left;
    owner->move_y = bounds.top - owner->previous.top;
    owner->resizing = owner->minimized ||
                      bounds.right - bounds.left != owner->previous.right - owner->previous.left ||
                      bounds.bottom - bounds.top != owner->previous.bottom - owner->previous.top;
    owner->previous = bounds;
    owner->minimized = false;
    owner->arranging = true;
    SetWindowPos(owner->proxy, nullptr, bounds.left, bounds.top, bounds.right - bounds.left,
                 bounds.bottom - bounds.top, SWP_NOZORDER | SWP_NOACTIVATE);
    if (owner->move_x != 0 || owner->move_y != 0 || owner->resizing)
        EnumThreadWindows(GetWindowThreadProcessId(owner->root, nullptr), arrange_dialog,
                          (LPARAM)owner);
    owner->arranging = false;
}
