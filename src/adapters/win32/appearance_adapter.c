#include "appearance_adapter.h"

#include <stdlib.h>
#include <windows.h>

struct appearance_adapter
{
    HKEY _Nullable personalize; /* 読めなければ nullptr */
};

static const wchar_t personalize_key[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const wchar_t light_value[] = L"AppsUseLightTheme";

enum appearance_adapter_outcome
appearance_adapter_create(struct appearance_adapter *_Nullable *_Nonnull out)
{
    struct appearance_adapter *_Nullable adapter = calloc(1, sizeof *adapter);
    if (adapter == nullptr)
    {
        return APPEARANCE_ADAPTER_OUT_OF_MEMORY;
    }
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, personalize_key, 0, KEY_QUERY_VALUE, &key) ==
        ERROR_SUCCESS)
    {
        adapter->personalize = key;
    }
    *out = adapter;
    return APPEARANCE_ADAPTER_CREATED;
}

/* AppsUseLightTheme が 0 ならダーク。値が無い・読めないときは OS の既定と同じライト。 */
static enum folio_theme read_theme(struct appearance_adapter *_Nonnull adapter)
{
    if (adapter->personalize == nullptr)
    {
        return FOLIO_THEME_LIGHT;
    }
    DWORD value = 1;
    DWORD size = sizeof value;
    LSTATUS status = RegGetValueW(adapter->personalize, nullptr, light_value, RRF_RT_REG_DWORD,
                                  nullptr, &value, &size);
    if (status != ERROR_SUCCESS)
    {
        return FOLIO_THEME_LIGHT;
    }
    return value == 0 ? FOLIO_THEME_DARK : FOLIO_THEME_LIGHT;
}

struct appearance_port appearance_adapter_port(struct appearance_adapter *_Nonnull adapter)
{
    struct appearance_port port = {.adapter = adapter, .read_theme = read_theme};
    return port;
}

void appearance_adapter_destroy(struct appearance_adapter *_Nullable adapter)
{
    if (adapter == nullptr)
    {
        return;
    }
    if (adapter->personalize != nullptr)
    {
        RegCloseKey(adapter->personalize);
    }
    free(adapter);
}
