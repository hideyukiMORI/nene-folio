#include "folder_picker.h"
#include "ui_text.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <shlobj.h>
#include <shobjidl.h>
#include <string.h>
#include <wchar.h>

static void initial_folder(IFileOpenDialog *_Nonnull dialog, const char *_Nonnull initial)
{
    struct utf16_text *_Nullable units = nullptr;
    if (utf16_text_create(initial, strlen(initial), &units) != UTF16_TEXT_CONVERTED)
        return;
    IShellItem *item = nullptr;
    HRESULT result = SHCreateItemFromParsingName(utf16_text_units(units), nullptr, &IID_IShellItem,
                                                 (void **)&item);
    if (SUCCEEDED(result))
    {
        (void)dialog->lpVtbl->SetFolder(dialog, item);
        item->lpVtbl->Release(item);
    }
    utf16_text_destroy(units);
}

static bool configure(IFileOpenDialog *_Nonnull dialog, enum folio_language language)
{
    FILEOPENDIALOGOPTIONS options = 0;
    if (FAILED(dialog->lpVtbl->GetOptions(dialog, &options)))
        return false;
    HRESULT result = dialog->lpVtbl->SetOptions(
        dialog, options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST |
                    FOS_NOCHANGEDIR | FOS_DONTADDTORECENT);
    char16_t title[ui_text_unit_limit];
    size_t written = 0;
    if (FAILED(result) || utf16_text_fill(ui_text_line(UI_TEXT_STORAGE_TITLE, language), title,
                                          ui_text_unit_limit, &written) != UTF16_TEXT_FILL_READY)
        return false;
    return SUCCEEDED(dialog->lpVtbl->SetTitle(dialog, title));
}

static enum folder_picker_outcome selected_path(IFileOpenDialog *_Nonnull dialog,
                                                struct utf8_text *_Nullable *_Nonnull out)
{
    IShellItem *item = nullptr;
    if (FAILED(dialog->lpVtbl->GetResult(dialog, &item)))
        return FOLDER_PICKER_UNAVAILABLE;
    PWSTR path = nullptr;
    HRESULT result = item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &path);
    enum folder_picker_outcome picked = FOLDER_PICKER_UNAVAILABLE;
    if (SUCCEEDED(result) && path != nullptr &&
        utf8_text_create(path, wcslen(path), out) == UTF8_TEXT_CONVERTED)
        picked = FOLDER_PICKER_SELECTED;
    CoTaskMemFree(path);
    item->lpVtbl->Release(item);
    return picked;
}

enum folder_picker_outcome folder_picker_show(HWND _Nonnull owner, const char *_Nonnull initial,
                                              enum folio_language language,
                                              struct utf8_text *_Nullable *_Nonnull out)
{
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(initialized))
        return FOLDER_PICKER_UNAVAILABLE;
    IFileOpenDialog *dialog = nullptr;
    HRESULT created = CoCreateInstance(&CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                       &IID_IFileOpenDialog, (void **)&dialog);
    if (FAILED(created))
    {
        CoUninitialize();
        return FOLDER_PICKER_UNAVAILABLE;
    }
    enum folder_picker_outcome result = FOLDER_PICKER_UNAVAILABLE;
    if (configure(dialog, language))
    {
        initial_folder(dialog, initial);
        HRESULT shown = dialog->lpVtbl->Show(dialog, owner);
        if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            result = FOLDER_PICKER_CANCELLED;
        else if (SUCCEEDED(shown))
            result = selected_path(dialog, out);
    }
    dialog->lpVtbl->Release(dialog);
    CoUninitialize();
    return result;
}
