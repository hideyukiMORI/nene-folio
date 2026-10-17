#include "accessibility_native.h"

/* 直接注釈のGUIDはuuid.libに含まれないため、この唯一の書込口で実体化する。 */
#include <initguid.h>
#include <oleacc.h>

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
    services->lpVtbl->Release(services);
    if (SUCCEEDED(result))
    {
        NotifyWinEvent(EVENT_OBJECT_NAMECHANGE, window, OBJID_CLIENT, CHILDID_SELF);
    }
    return result;
}
