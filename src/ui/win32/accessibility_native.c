#include "accessibility_native.h"

/* 直接注釈のGUIDはuuid.libに含まれないため、この唯一の書込口で実体化する。 */
#include <initguid.h>
#include <oleacc.h>

/* UIA の Name の性質の GUID（UIAutomationCoreApi.h の Name_Property_GUID と同じ値）。
 * その見出しは UIA の型一式を要し、ほかの単位の UIA の定数と重複定義になるので値だけを持つ。 */
DEFINE_GUID(uia_name_property, 0xc3a6921b, 0x4a99, 0x44f1, 0xbc, 0xa6, 0x61, 0x18, 0x70, 0x52, 0xc4,
            0x31);

/* MSAA の名前に加えて UIA の Name も注釈する。Win32 EDIT の UIA proxy は MSAA の名前の注釈を
 * 読まず、検索欄の Name が空になっていた（ADR0060 の決定 5）。値の正本は同じ表示名。 */
HRESULT accessibility_native_name(HWND _Nonnull window, const char16_t *_Nonnull name)
{
    IAccPropServices *_Nullable services = nullptr;
    HRESULT result = CoCreateInstance(&CLSID_AccPropServices, nullptr, CLSCTX_INPROC_SERVER,
                                      &IID_IAccPropServices, (void **)&services);
    if (FAILED(result))
    {
        return result;
    }
    result = services->lpVtbl->SetHwndPropStr(services, window, (DWORD)OBJID_CLIENT, CHILDID_SELF,
                                              PROPID_ACC_NAME, name);
    if (SUCCEEDED(result))
    {
        result = services->lpVtbl->SetHwndPropStr(services, window, (DWORD)OBJID_CLIENT,
                                                  CHILDID_SELF, uia_name_property, name);
    }
    services->lpVtbl->Release(services);
    if (SUCCEEDED(result))
    {
        NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, window, OBJID_CLIENT, CHILDID_SELF);
    }
    return result;
}
