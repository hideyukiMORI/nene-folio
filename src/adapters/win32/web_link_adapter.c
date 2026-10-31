#include "web_link_adapter.h"
#include "utf16_text.h"
#include "web_uri.h"
#include <windows.h>

#include <objbase.h>
#include <shellapi.h>

static enum web_link_outcome open_native(const wchar_t *_Nonnull uri)
{
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE)
        return WEB_LINK_UNAVAILABLE;
    SHELLEXECUTEINFOW request = {.cbSize = sizeof request,
                                 .fMask = SEE_MASK_FLAG_NO_UI,
                                 .lpVerb = L"open",
                                 .lpFile = uri,
                                 .nShow = SW_SHOWNORMAL};
    BOOL opened = ShellExecuteExW(&request);
    if (SUCCEEDED(initialized))
        CoUninitialize();
    return opened ? WEB_LINK_OPENED : WEB_LINK_UNAVAILABLE;
}

static enum web_link_outcome open_uri(const struct web_uri *_Nonnull uri)
{
    struct utf16_text *_Nullable native = nullptr;
    switch (utf16_text_create(web_uri_bytes(uri), web_uri_length(uri), &native))
    {
    case UTF16_TEXT_CONVERTED:
        break;
    case UTF16_TEXT_INVALID_UTF8:
        return WEB_LINK_UNAVAILABLE;
    case UTF16_TEXT_OUT_OF_MEMORY:
        return WEB_LINK_OUT_OF_MEMORY;
    }
    enum web_link_outcome outcome = open_native(utf16_text_units(native));
    utf16_text_destroy(native);
    return outcome;
}

struct web_link_port web_link_adapter_port(void)
{
    return (struct web_link_port){.open = open_uri};
}
