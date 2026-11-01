#include "file_picker.h"
#include "storage_path.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include <shobjidl.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

constexpr size_t picker_text_capacity = 1024;

struct file_picker
{
    struct storage_path *_Nonnull *_Nonnull paths;
    size_t count;
};

static bool item_path(IShellItem *_Nonnull item, struct storage_path *_Nullable *_Nonnull out)
{
    PWSTR path = nullptr;
    HRESULT found = item->lpVtbl->GetDisplayName(item, SIGDN_FILESYSPATH, &path);
    if (FAILED(found) || path == nullptr)
        return false;
    struct utf8_text *_Nullable text = nullptr;
    enum utf8_text_outcome converted = utf8_text_create(path, wcslen(path), &text);
    CoTaskMemFree(path);
    if (converted != UTF8_TEXT_CONVERTED)
        return false;
    enum storage_path_outcome accepted =
        storage_path_create(utf8_text_bytes(text), utf8_text_length(text), out);
    utf8_text_destroy(text);
    return accepted == STORAGE_PATH_READY;
}

static bool dialog_text(const struct file_picker_request *_Nonnull request,
                        char16_t *_Nonnull title, char16_t *_Nonnull name)
{
    char narrow[picker_text_capacity];
    struct ui_text_request format = {
        .id = request->title, .language = request->language, .name = request->name};
    size_t written = 0;
    return ui_text_format(&format, narrow, sizeof narrow) == UI_TEXT_FORMAT_READY &&
           utf16_text_fill(narrow, title, picker_text_capacity, &written) ==
               UTF16_TEXT_FILL_READY &&
           utf16_text_fill(request->name, name, picker_text_capacity, &written) ==
               UTF16_TEXT_FILL_READY;
}

static bool configure_open(IFileOpenDialog *_Nonnull dialog,
                           const struct file_picker_request *_Nonnull request)
{
    char16_t title[picker_text_capacity];
    char16_t name[picker_text_capacity];
    if (!dialog_text(request, title, name))
        return false;
    FILEOPENDIALOGOPTIONS options = FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST |
                                    FOS_ALLOWMULTISELECT | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT;
    size_t written = 0;
    if (utf16_text_fill(ui_text_line(UI_TEXT_FILTER_MARKDOWN, request->language), name,
                        picker_text_capacity, &written) != UTF16_TEXT_FILL_READY)
        return false;
    COMDLG_FILTERSPEC filter = {name, L"*.md"};
    return SUCCEEDED(dialog->lpVtbl->SetOptions(dialog, options)) &&
           SUCCEEDED(dialog->lpVtbl->SetFileTypes(dialog, 1, &filter)) &&
           SUCCEEDED(dialog->lpVtbl->SetTitle(dialog, title));
}

static void destroy_prepared(struct file_picker *_Nullable picker,
                             struct storage_path *_Nonnull *_Nullable paths, size_t count)
{
    for (size_t index = 0; index < count; ++index)
        storage_path_destroy(paths[index]);
    free(paths);
    free(picker);
}

static enum file_picker_outcome collect(IShellItemArray *_Nonnull items,
                                        struct file_picker *_Nullable *_Nonnull out)
{
    DWORD count = 0;
    if (FAILED(items->lpVtbl->GetCount(items, &count)) || count == 0 || count > 65534)
        return FILE_PICKER_UNAVAILABLE;
    struct file_picker *_Nullable picker = malloc(sizeof *picker);
    struct storage_path *_Nonnull *_Nullable paths = calloc(count, sizeof *paths);
    if (picker == nullptr || paths == nullptr)
    {
        destroy_prepared(picker, paths, 0);
        return FILE_PICKER_UNAVAILABLE;
    }
    size_t used = 0;
    for (; used < count; ++used)
    {
        IShellItem *item = nullptr;
        if (FAILED(items->lpVtbl->GetItemAt(items, (DWORD)used, &item)))
            break;
        struct storage_path *_Nullable path = nullptr;
        bool accepted = item_path(item, &path);
        item->lpVtbl->Release(item);
        if (!accepted)
            break;
        paths[used] = path;
    }
    if (used != count)
    {
        destroy_prepared(picker, paths, used);
        return FILE_PICKER_UNAVAILABLE;
    }
    *picker = (struct file_picker){.paths = paths, .count = count};
    *out = picker;
    return FILE_PICKER_SELECTED;
}

static enum file_picker_outcome open_result(IFileOpenDialog *_Nonnull dialog,
                                            struct file_picker *_Nullable *_Nonnull out)
{
    IShellItemArray *items = nullptr;
    if (FAILED(dialog->lpVtbl->GetResults(dialog, &items)))
        return FILE_PICKER_UNAVAILABLE;
    enum file_picker_outcome result = collect(items, out);
    items->lpVtbl->Release(items);
    return result;
}

enum file_picker_outcome file_picker_open(HWND _Nonnull owner,
                                          const struct file_picker_request *_Nonnull request,
                                          struct file_picker *_Nullable *_Nonnull out)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return FILE_PICKER_UNAVAILABLE;
    IFileOpenDialog *dialog = nullptr;
    HRESULT created = CoCreateInstance(&CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                       &IID_IFileOpenDialog, (void **)&dialog);
    enum file_picker_outcome result = FILE_PICKER_UNAVAILABLE;
    if (SUCCEEDED(created) && configure_open(dialog, request))
    {
        HRESULT shown = dialog->lpVtbl->Show(dialog, owner);
        if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            result = FILE_PICKER_CANCELLED;
        else if (SUCCEEDED(shown))
            result = open_result(dialog, out);
    }
    if (dialog != nullptr)
        dialog->lpVtbl->Release(dialog);
    CoUninitialize();
    return result;
}

static bool configure_save(IFileSaveDialog *_Nonnull dialog,
                           const struct file_picker_request *_Nonnull request)
{
    char16_t title[picker_text_capacity];
    char16_t name[picker_text_capacity];
    if (!dialog_text(request, title, name))
        return false;
    bool zip = strcmp(request->extension, "zip") == 0;
    char16_t label[picker_text_capacity];
    size_t written = 0;
    if (utf16_text_fill(
            ui_text_line(zip ? UI_TEXT_FILTER_ZIP : UI_TEXT_FILTER_MARKDOWN, request->language),
            label, picker_text_capacity, &written) != UTF16_TEXT_FILL_READY)
        return false;
    COMDLG_FILTERSPEC filter = {label, zip ? L"*.zip" : L"*.md"};
    FILEOPENDIALOGOPTIONS options = FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR |
                                    FOS_DONTADDTORECENT | FOS_STRICTFILETYPES;
    return SUCCEEDED(dialog->lpVtbl->SetOptions(dialog, options)) &&
           SUCCEEDED(dialog->lpVtbl->SetFileTypes(dialog, 1, &filter)) &&
           SUCCEEDED(dialog->lpVtbl->SetDefaultExtension(dialog, zip ? L"zip" : L"md")) &&
           SUCCEEDED(dialog->lpVtbl->SetFileName(dialog, name)) &&
           SUCCEEDED(dialog->lpVtbl->SetTitle(dialog, title));
}

static enum file_picker_outcome save_result(IFileSaveDialog *_Nonnull dialog,
                                            struct storage_path *_Nullable *_Nonnull out)
{
    IShellItem *item = nullptr;
    if (FAILED(dialog->lpVtbl->GetResult(dialog, &item)))
        return FILE_PICKER_UNAVAILABLE;
    bool accepted = item_path(item, out);
    item->lpVtbl->Release(item);
    return accepted ? FILE_PICKER_SELECTED : FILE_PICKER_UNAVAILABLE;
}

enum file_picker_outcome file_picker_save(HWND _Nonnull owner,
                                          const struct file_picker_request *_Nonnull request,
                                          struct storage_path *_Nullable *_Nonnull out)
{
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)))
        return FILE_PICKER_UNAVAILABLE;
    IFileSaveDialog *dialog = nullptr;
    HRESULT created = CoCreateInstance(&CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
                                       &IID_IFileSaveDialog, (void **)&dialog);
    enum file_picker_outcome result = FILE_PICKER_UNAVAILABLE;
    if (SUCCEEDED(created) && configure_save(dialog, request))
    {
        HRESULT shown = dialog->lpVtbl->Show(dialog, owner);
        if (shown == HRESULT_FROM_WIN32(ERROR_CANCELLED))
            result = FILE_PICKER_CANCELLED;
        else if (SUCCEEDED(shown))
            result = save_result(dialog, out);
    }
    if (dialog != nullptr)
        dialog->lpVtbl->Release(dialog);
    CoUninitialize();
    return result;
}

size_t file_picker_count(const struct file_picker *_Nonnull picker)
{
    return picker->count;
}

struct storage_path *_Nonnull const *_Nonnull file_picker_paths(
    const struct file_picker *_Nonnull picker)
{
    return picker->paths;
}

void file_picker_destroy(struct file_picker *_Nullable picker)
{
    if (picker != nullptr)
        destroy_prepared(picker, picker->paths, picker->count);
}
