#include "recycle_item.h"

#include <sherrors.h>
#include <shlobj.h>
#include <windows.h>

static enum trash_outcome failure(HRESULT result)
{
    if (result == COPYENGINE_E_RECYCLE_FORCE_NUKE || result == COPYENGINE_E_RECYCLE_SIZE_TOO_BIG ||
        result == COPYENGINE_E_RECYCLE_PATH_TOO_LONG ||
        result == COPYENGINE_E_RECYCLE_BIN_NOT_FOUND || result == E_NOTIMPL ||
        result == E_NOINTERFACE)
    {
        return TRASH_UNAVAILABLE;
    }
    return TRASH_FAILED;
}

static enum trash_outcome transfer_item(IShellItem *_Nonnull source, IShellItem *_Nonnull bin)
{
    IShellItem *parent = nullptr;
    HRESULT result = source->lpVtbl->GetParent(source, &parent);
    if (FAILED(result))
    {
        return failure(result);
    }
    ITransferSource *transfer = nullptr;
    result = parent->lpVtbl->BindToHandler(parent, nullptr, &BHID_Transfer, &IID_ITransferSource,
                                           (void **)&transfer);
    parent->lpVtbl->Release(parent);
    if (FAILED(result))
    {
        return failure(result);
    }
    IShellItem *destination = nullptr;
    result = transfer->lpVtbl->RecycleItem(transfer, source, bin, TSF_NORMAL, &destination);
    transfer->lpVtbl->Release(transfer);
    bool moved = SUCCEEDED(result) && result != COPYENGINE_S_NOT_HANDLED && destination != nullptr;
    if (destination != nullptr)
    {
        destination->lpVtbl->Release(destination);
    }
    return moved ? TRASH_TRASHED : failure(result);
}

static enum trash_outcome send_path(const wchar_t *_Nonnull path)
{
    IShellItem *bin = nullptr;
    HRESULT result = SHGetKnownFolderItem(&FOLDERID_RecycleBinFolder, KF_FLAG_DEFAULT, nullptr,
                                          &IID_IShellItem, (void **)&bin);
    if (FAILED(result))
    {
        return failure(result);
    }
    IShellItem *source = nullptr;
    result = SHCreateItemFromParsingName(path, nullptr, &IID_IShellItem, (void **)&source);
    enum trash_outcome outcome = failure(result);
    if (SUCCEEDED(result))
    {
        outcome = transfer_item(source, bin);
        source->lpVtbl->Release(source);
    }
    bin->lpVtbl->Release(bin);
    return outcome;
}

enum trash_outcome recycle_item_send(const wchar_t *_Nonnull path)
{
    HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (initialized == RPC_E_CHANGED_MODE)
    {
        return TRASH_UNAVAILABLE;
    }
    if (FAILED(initialized))
    {
        return TRASH_FAILED;
    }
    enum trash_outcome outcome = send_path(path);
    CoUninitialize();
    return outcome;
}
