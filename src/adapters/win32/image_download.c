#include "image_download.h"
#include "image_source.h"
#include "utf16_text.h"
#include <stdlib.h>
#include <windows.h>
#include <winhttp.h>
struct image_download
{
    SRWLOCK lock;
    HANDLE _Nullable completed;
    HANDLE _Nullable closed;
    bool callback_registered;
    DWORD notification;
    DWORD received;
    ULONGLONG started;
    HINTERNET _Nullable session;
    HINTERNET _Nullable connection;
    HINTERNET _Nullable request;
    unsigned char *_Nullable bytes;
    size_t length;
};
// Waiver: WVR-0001
// NOLINTNEXTLINE(readability-function-size)
static void CALLBACK status_callback(HINTERNET handle, DWORD_PTR context, DWORD status,
                                     void *_Nullable information, DWORD length)
{
    (void)handle;
    (void)information;
    struct image_download *download = (struct image_download *)context;
    if (status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING)
    {
        SetEvent(download->closed);
        return;
    }
    if (status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE ||
        status == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE ||
        status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE ||
        status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR)
    {
        AcquireSRWLockExclusive(&download->lock);
        download->notification = status;
        download->received = length;
        ReleaseSRWLockExclusive(&download->lock);
        SetEvent(download->completed);
    }
}
static bool register_callback(struct image_download *_Nonnull download)
{
    download->completed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    download->closed = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    DWORD_PTR context = (DWORD_PTR)download;
    if (download->completed == nullptr || download->closed == nullptr ||
        !WinHttpSetOption(download->request, WINHTTP_OPTION_CONTEXT_VALUE, &context,
                          sizeof context))
        return false;
    DWORD flags = WINHTTP_CALLBACK_FLAG_SENDREQUEST_COMPLETE |
                  WINHTTP_CALLBACK_FLAG_HEADERS_AVAILABLE | WINHTTP_CALLBACK_FLAG_READ_COMPLETE |
                  WINHTTP_CALLBACK_FLAG_REQUEST_ERROR | WINHTTP_CALLBACK_FLAG_HANDLES;
    download->callback_registered =
        WinHttpSetStatusCallback(download->request, status_callback, flags, 0) !=
        WINHTTP_INVALID_STATUS_CALLBACK;
    return download->callback_registered;
}
static enum image_outcome await_notification(struct image_download *_Nonnull download,
                                             const struct image_work *_Nonnull work, DWORD expected)
{
    DWORD waited = WAIT_TIMEOUT;
    while (waited == WAIT_TIMEOUT)
    {
        if (work->cancelled(work))
            return IMAGE_CANCELLED;
        if (GetTickCount64() - download->started >= 10000)
            return IMAGE_FAILED;
        waited = WaitForSingleObject(download->completed, 25);
    }
    if (waited != WAIT_OBJECT_0)
        return IMAGE_FAILED;
    AcquireSRWLockShared(&download->lock);
    DWORD status = download->notification;
    ReleaseSRWLockShared(&download->lock);
    return status == expected ? IMAGE_READY : IMAGE_FAILED;
}
static bool open_request(struct image_download *_Nonnull download, const wchar_t *_Nonnull url)
{
    wchar_t host[256];
    wchar_t path[image_source_byte_limit + 1];
    URL_COMPONENTS parts = {.dwStructSize = sizeof parts,
                            .lpszHostName = host,
                            .dwHostNameLength = 256,
                            .lpszUrlPath = path,
                            .dwUrlPathLength = image_source_byte_limit + 1};
    if (!WinHttpCrackUrl(url, 0, ICU_REJECT_USERPWD, &parts) ||
        parts.nScheme != INTERNET_SCHEME_HTTPS)
        return false;
    for (size_t index = 0; path[index] != L'\0'; ++index)
        if (path[index] == L'#')
        {
            path[index] = L'\0';
            break;
        }
    download->session =
        WinHttpOpen(L"NeNeFolio/Images", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                    WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC);
    if (download->session == nullptr ||
        !WinHttpSetTimeouts(download->session, 3000, 3000, 3000, 1000))
        return false;
    download->connection = WinHttpConnect(download->session, host, parts.nPort, 0);
    if (download->connection == nullptr)
        return false;
    download->request =
        WinHttpOpenRequest(download->connection, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                           WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (download->request == nullptr)
        return false;
    DWORD redirect = WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;
    DWORD credentials = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    return WinHttpSetOption(download->request, WINHTTP_OPTION_REDIRECT_POLICY, &redirect,
                            sizeof redirect) &&
           WinHttpSetOption(download->request, WINHTTP_OPTION_AUTOLOGON_POLICY, &credentials,
                            sizeof credentials) &&
           register_callback(download);
}
static enum image_outcome receive_headers(struct image_download *_Nonnull download,
                                          const struct image_work *_Nonnull work)
{
    ResetEvent(download->completed);
    if (!WinHttpSendRequest(download->request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, (DWORD_PTR)download))
        return IMAGE_FAILED;
    enum image_outcome sent =
        await_notification(download, work, WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE);
    if (sent != IMAGE_READY)
        return sent;
    ResetEvent(download->completed);
    if (!WinHttpReceiveResponse(download->request, nullptr))
        return IMAGE_FAILED;
    enum image_outcome received =
        await_notification(download, work, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE);
    if (received != IMAGE_READY)
        return received;
    DWORD status = 0;
    DWORD length = sizeof status;
    return WinHttpQueryHeaders(
               download->request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
               WINHTTP_HEADER_NAME_BY_INDEX, &status, &length, WINHTTP_NO_HEADER_INDEX) &&
                   status == 200
               ? IMAGE_READY
               : IMAGE_FAILED;
}
static enum image_outcome read_chunk(struct image_download *_Nonnull download,
                                     const struct image_work *_Nonnull work)
{
    size_t remaining = image_input_byte_limit + 1 - download->length;
    unsigned char *target = download->bytes + download->length;
    DWORD request = (DWORD)(remaining < 65536 ? remaining : 65536);
    ResetEvent(download->completed);
    if (!WinHttpReadData(download->request, target, request, nullptr))
        return IMAGE_FAILED;
    enum image_outcome received =
        await_notification(download, work, WINHTTP_CALLBACK_STATUS_READ_COMPLETE);
    if (received != IMAGE_READY)
        return received;
    AcquireSRWLockShared(&download->lock);
    DWORD read = download->received;
    ReleaseSRWLockShared(&download->lock);
    if (read == 0)
        return IMAGE_READY;
    download->length += read;
    return download->length > image_input_byte_limit ? IMAGE_TOO_LARGE : IMAGE_PENDING;
}
static enum image_outcome read_body(struct image_download *_Nonnull download,
                                    const struct image_work *_Nonnull work)
{
    download->bytes = malloc(image_input_byte_limit + 1);
    if (download->bytes == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    for (;;)
    {
        if (work->cancelled(work))
            return IMAGE_CANCELLED;
        enum image_outcome read = read_chunk(download, work);
        if (read != IMAGE_PENDING)
            return read;
    }
}
enum image_outcome image_download_read(const char *_Nonnull url,
                                       const struct image_work *_Nonnull work,
                                       struct image_download *_Nullable *_Nonnull out)
{
    wchar_t native[image_source_byte_limit + 1];
    size_t length = 0;
    if (utf16_text_fill(url, native, image_source_byte_limit + 1, &length) != UTF16_TEXT_FILL_READY)
        return IMAGE_UNSUPPORTED;
    struct image_download *_Nullable download = calloc(1, sizeof *download);
    if (download == nullptr)
        return IMAGE_OUT_OF_MEMORY;
    InitializeSRWLock(&download->lock);
    download->started = GetTickCount64();
    enum image_outcome outcome = IMAGE_FAILED;
    if (open_request(download, native))
        outcome = receive_headers(download, work);
    if (outcome == IMAGE_READY)
        outcome = read_body(download, work);
    if (outcome != IMAGE_READY)
    {
        image_download_destroy(download);
        return outcome;
    }
    *out = download;
    return IMAGE_READY;
}
struct image_encoded image_download_bytes(const struct image_download *_Nonnull download)
{
    return (struct image_encoded){.bytes = download->bytes, .length = download->length};
}
void image_download_destroy(struct image_download *_Nullable download)
{
    if (download == nullptr)
        return;
    if (download->request != nullptr)
    {
        WinHttpCloseHandle(download->request);
        if (download->callback_registered)
            WaitForSingleObject(download->closed, INFINITE);
    }
    if (download->connection != nullptr)
        WinHttpCloseHandle(download->connection);
    if (download->session != nullptr)
        WinHttpCloseHandle(download->session);
    if (download->completed != nullptr)
        CloseHandle(download->completed);
    if (download->closed != nullptr)
        CloseHandle(download->closed);
    free(download->bytes);
    free(download);
}
