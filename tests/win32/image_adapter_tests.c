/* #520: bounded native image I/O, WIC, worker cancellation and cache freshness. */
#include "image_adapter.h"
#include "image_bitmap.h"
#include "image_decode.h"
#include "image_download.h"
#include "image_export.h"
#include "image_file.h"
#include "image_source.h"
#include "note_archive.h"
#include "note_text.h"
#include "storage_path.h"
#include "utf8_text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wincodec.h>
#include <windows.h>

static unsigned checks;
static wchar_t directory[1024];
static unsigned phases[4];
static bool stop_decode;
static HANDLE _Nullable cancel_after;
static void require(bool value, const char *_Nonnull label)
{
    ++checks;
    if (!value)
    {
        fprintf(stderr, "FAIL: %s\n", label);
        exit(EXIT_FAILURE);
    }
}
static HANDLE _Nonnull relative_timer(unsigned long milliseconds)
{
    HANDLE timer = CreateWaitableTimerW(nullptr, TRUE, nullptr);
    require(timer != nullptr, "test timer");
    LARGE_INTEGER due = {.QuadPart = -(LONGLONG)milliseconds * 10000};
    require(SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE), "relative test interval");
    return timer;
}
static void path_for(const wchar_t *_Nonnull leaf, wchar_t *_Nonnull path)
{
    require(swprintf_s(path, 1024, L"%ls\\%ls", directory, leaf) > 0, "fixture path");
}
static void make_directory(const wchar_t *_Nonnull leaf)
{
    wchar_t path[1024];
    path_for(leaf, path);
    require(CreateDirectoryW(path, nullptr), "fixture directory");
}
static void write_image(const wchar_t *_Nonnull leaf, struct image_size size, bool jpeg)
{
    wchar_t path[1024];
    path_for(leaf, path);
    void *_Nullable created = nullptr;
    require(SUCCEEDED(CoCreateInstance(&CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                       &IID_IWICImagingFactory, &created)),
            "WIC fixture factory");
    IWICImagingFactory *factory = created;
    IWICStream *_Nullable stream = nullptr;
    require(SUCCEEDED(factory->lpVtbl->CreateStream(factory, &stream)), "WIC fixture stream");
    require(SUCCEEDED(stream->lpVtbl->InitializeFromFilename(stream, path, GENERIC_WRITE)),
            "WIC output");
    IWICBitmapEncoder *_Nullable encoder = nullptr;
    const GUID *container = jpeg ? &GUID_ContainerFormatJpeg : &GUID_ContainerFormatPng;
    require(SUCCEEDED(factory->lpVtbl->CreateEncoder(factory, container, nullptr, &encoder)),
            "fixture encoder");
    require(
        SUCCEEDED(encoder->lpVtbl->Initialize(encoder, (IStream *)stream, WICBitmapEncoderNoCache)),
        "encoder init");
    IWICBitmapFrameEncode *_Nullable frame = nullptr;
    IPropertyBag2 *_Nullable properties = nullptr;
    require(SUCCEEDED(encoder->lpVtbl->CreateNewFrame(encoder, &frame, &properties)),
            "fixture frame");
    require(SUCCEEDED(frame->lpVtbl->Initialize(frame, properties)) &&
                SUCCEEDED(frame->lpVtbl->SetSize(frame, size.width, size.height)),
            "fixture dimensions");
    GUID format = GUID_WICPixelFormat24bppBGR;
    require(SUCCEEDED(frame->lpVtbl->SetPixelFormat(frame, &format)) &&
                IsEqualGUID(&format, &GUID_WICPixelFormat24bppBGR),
            "fixture pixel format");
    UINT stride = size.width * 3;
    unsigned char *_Nullable row = malloc(stride);
    require(row != nullptr, "fixture row");
    memset(row, 0x80, stride);
    for (UINT index = 0; index < size.height; ++index)
        require(SUCCEEDED(frame->lpVtbl->WritePixels(frame, 1, stride, stride, row)),
                "fixture scanline");
    free(row);
    require(SUCCEEDED(frame->lpVtbl->Commit(frame)) && SUCCEEDED(encoder->lpVtbl->Commit(encoder)),
            "fixture commit");
    if (properties != nullptr)
        properties->lpVtbl->Release(properties);
    frame->lpVtbl->Release(frame);
    encoder->lpVtbl->Release(encoder);
    stream->lpVtbl->Release(stream);
    factory->lpVtbl->Release(factory);
}
static struct storage_path *_Nonnull prepare(void)
{
    DWORD length = GetModuleFileNameW(nullptr, directory, 1024);
    require(length > 0 && length < 1024, "test executable location");
    wchar_t *_Nullable slash = wcsrchr(directory, L'\\');
    require(slash != nullptr, "test directory");
    *slash = L'\0';
    wchar_t suffix[64];
    require(swprintf_s(suffix, 64, L"\\images520-%lu", GetCurrentProcessId()) > 0,
            "fixture suffix");
    require(wcslen(directory) + wcslen(suffix) < 1024, "fixture fits");
    memcpy(directory + wcslen(directory), suffix, (wcslen(suffix) + 1) * sizeof(wchar_t));
    require(CreateDirectoryW(directory, nullptr), "unique fixture root");
    make_directory(L"A");
    make_directory(L"A\\images");
    write_image(L"A\\images\\a.png", (struct image_size){4096, 1024}, false);
    write_image(L"A\\outside.jpg", (struct image_size){320, 160}, true);
    struct utf8_text *_Nullable utf8 = nullptr;
    require(utf8_text_create(directory, wcslen(directory), &utf8) == UTF8_TEXT_CONVERTED,
            "root utf8");
    struct storage_path *_Nullable root = nullptr;
    require(storage_path_create(utf8_text_bytes(utf8), utf8_text_length(utf8), &root) ==
                STORAGE_PATH_READY,
            "root value");
    utf8_text_destroy(utf8);
    return root;
}
static struct image_source *_Nonnull source_for(const char *_Nonnull value)
{
    struct image_source *_Nullable source = nullptr;
    require(image_source_create(value, (struct note_search_span){0}, &source) == IMAGE_READY,
            "request source");
    return source;
}
static enum image_outcome wait_for(const struct image_port *_Nonnull port, uint64_t ticket,
                                   struct image_bitmap *_Nullable *_Nonnull out)
{
    HANDLE budget = relative_timer(15000);
    enum image_outcome result = IMAGE_PENDING;
    while (result == IMAGE_PENDING && WaitForSingleObject(budget, 0) == WAIT_TIMEOUT)
    {
        result = port->poll(port->adapter, ticket, out);
        if (result == IMAGE_PENDING)
            Sleep(1);
    }
    require(result != IMAGE_PENDING, "bounded worker completion");
    CloseHandle(budget);
    return result;
}
static enum image_outcome request_image(const struct image_port *_Nonnull port,
                                        const char *_Nonnull value,
                                        struct image_bitmap *_Nullable *_Nonnull out)
{
    struct image_source *source = source_for(value);
    struct image_request request = {.category = "A", .source = source};
    uint64_t ticket = 0;
    require(port->request(port->adapter, &request, &ticket) == IMAGE_PENDING,
            "copied asynchronous request");
    image_source_destroy(source);
    return wait_for(port, ticket, out);
}
static void expect_size(const struct image_port *_Nonnull port, const char *_Nonnull value,
                        struct image_size expected)
{
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(request_image(port, value, &bitmap) == IMAGE_READY, "local decode succeeds");
    struct image_size actual = image_bitmap_size(bitmap);
    require(actual.width == expected.width && actual.height == expected.height,
            "bounded aspect-preserving decoded dimensions");
    image_bitmap_destroy(bitmap);
}
static bool cancelled(const struct image_work *_Nonnull work)
{
    (void)work;
    return stop_decode ||
           (cancel_after != nullptr && WaitForSingleObject(cancel_after, 0) == WAIT_OBJECT_0);
}
static void progress(const struct image_work *_Nonnull work, enum image_phase phase)
{
    (void)work;
    ++phases[(size_t)phase];
}
static void phases_contract(const struct image_port *_Nonnull port)
{
    struct image_source *source = source_for("images/a.png");
    struct image_request request = {.category = "A", .source = source};
    struct image_file *_Nullable file = nullptr;
    require(image_file_read(directory, &request, IMAGE_EXPORT_CONTAINED, &file) == IMAGE_READY,
            "original read");
    struct image_encoded encoded = {.bytes = image_file_bytes(file),
                                    .length = image_file_length(file)};
    struct image_work work = {
        .adapter = port->adapter, .cancelled = cancelled, .progress = progress};
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(image_decode_read(encoded, &work, &bitmap) == IMAGE_READY, "direct adapter decode");
    require(phases[IMAGE_PHASE_DECODING] == 1 && phases[IMAGE_PHASE_RESIZING] == 1,
            "processing status after decode begins");
    image_bitmap_destroy(bitmap);
    bitmap = nullptr;
    stop_decode = true;
    require(image_decode_read(encoded, &work, &bitmap) == IMAGE_CANCELLED && bitmap == nullptr,
            "cancel before decode preserves result");
    stop_decode = false;
    image_file_destroy(file);
    image_source_destroy(source);
}
static void asset_contract(const struct image_port *_Nonnull port)
{
    struct image_source *source = source_for("images/a.png");
    struct image_request request = {.category = "A", .source = source};
    struct image_asset *_Nullable asset = nullptr;
    require(port->asset(port->adapter, &request, IMAGE_EXPORT_CONTAINED, &asset) == IMAGE_READY,
            "contained image included");
    const struct image_asset_input *content = image_asset_content(asset);
    require(strcmp(content->relative, "images/a.png") == 0 && content->length > 8 &&
                content->bytes[0] == 137,
            "ZIP owns original PNG bytes and relative path");
    image_asset_destroy(asset);
    image_source_destroy(source);
    source = source_for("outside.jpg");
    request.source = source;
    asset = nullptr;
    require(port->asset(port->adapter, &request, IMAGE_EXPORT_CONTAINED, &asset) == IMAGE_SKIPPED &&
                asset == nullptr,
            "outside image requires choice");
    require(port->asset(port->adapter, &request, IMAGE_EXPORT_ALL_LOCAL, &asset) == IMAGE_READY &&
                image_asset_content(asset)->relative[0] == '\0',
            "explicit outside inclusion");
    image_asset_destroy(asset);
    image_source_destroy(source);
}
static void cancel_contract(const struct image_port *_Nonnull port)
{
    struct image_source *source = source_for("images/a.png");
    struct image_request request = {.category = "A", .source = source};
    uint64_t first = 0, second = 0;
    require(port->request(port->adapter, &request, &first) == IMAGE_PENDING, "first generation");
    port->cancel(port->adapter);
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(port->poll(port->adapter, first, &bitmap) == IMAGE_CANCELLED && bitmap == nullptr,
            "cancelled generation cannot publish");
    require(port->request(port->adapter, &request, &second) == IMAGE_PENDING && second != first,
            "new generation");
    image_source_destroy(source);
    require(wait_for(port, second, &bitmap) == IMAGE_READY, "next generation completes");
    image_bitmap_destroy(bitmap);
}
static void rejected_files(const struct image_port *_Nonnull port)
{
    const char *paths[] = {"images/missing.png", "http://example.com/a.png", "//server/a.png"};
    const enum image_outcome outcomes[] = {IMAGE_FAILED, IMAGE_UNSUPPORTED, IMAGE_UNSUPPORTED};
    for (size_t index = 0; index < 3; ++index)
    {
        struct image_bitmap *_Nullable bitmap = nullptr;
        require(request_image(port, paths[index], &bitmap) == outcomes[index] && bitmap == nullptr,
                "missing or prohibited source rejected");
    }
    wchar_t path[1024];
    path_for(L"A\\images\\bad.png", path);
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    require(file != INVALID_HANDLE_VALUE, "corrupt fixture");
    DWORD written = 0;
    require(WriteFile(file, "nope", 4, &written, nullptr) && written == 4, "corrupt bytes");
    CloseHandle(file);
    struct image_bitmap *_Nullable bitmap = nullptr;
    require(request_image(port, "images/bad.png", &bitmap) == IMAGE_FAILED && bitmap == nullptr,
            "corrupt PNG rejected");
    file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    require(file != INVALID_HANDLE_VALUE, "oversized fixture");
    LARGE_INTEGER size = {.QuadPart = 16 * 1024 * 1024 + 1};
    require(SetFilePointerEx(file, size, nullptr, FILE_BEGIN) && SetEndOfFile(file),
            "oversized original");
    CloseHandle(file);
    require(request_image(port, "images/bad.png", &bitmap) == IMAGE_TOO_LARGE && bitmap == nullptr,
            "input byte bound before decode");
}
static void export_zip(const struct image_port *_Nonnull port, const char *_Nonnull output)
{
    const char source[] =
        "\xEF\xBB\xBF\xEF\xBB\xBF# Export\r\n\r\n![inside](images/a.png)\r\n"
        "![outside](<outside.jpg> \"title\")\r\n![web](https://example.com/a.png)\r\n";
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(source, sizeof source - 1, &body) == NOTE_TEXT_ACCEPTED,
            "export original Markdown");
    struct image_export *_Nullable images = nullptr;
    require(image_export_create(port, IMAGE_EXPORT_ALL_LOCAL, 1, &images) == FOLIO_STATE_READY,
            "real image export");
    struct archive_note note = {.category = "A", .name = "one", .body = body};
    require(image_export_append(images, &note) == FOLIO_STATE_READY, "real local image snapshot");
    struct note_archive *_Nullable archive = nullptr;
    require(note_archive_create(&note, 1, image_export_assets(images), &archive) ==
                NOTE_ARCHIVE_READY,
            "real image ZIP");
    require(note_text_file_length(body) == sizeof source - 1 &&
                memcmp(note_text_file_bytes(body), source, sizeof source - 1) == 0,
            "source BOM CRLF and paths unchanged");
    wchar_t destination[1024];
    require(MultiByteToWideChar(CP_UTF8, 0, output, -1, destination, 1024) > 0,
            "export destination");
    HANDLE file = CreateFileW(destination, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
    require(file != INVALID_HANDLE_VALUE, "new ZIP artifact");
    DWORD written = 0;
    require(WriteFile(file, note_archive_bytes(archive), (DWORD)note_archive_length(archive),
                      &written, nullptr) &&
                written == note_archive_length(archive),
            "ZIP bytes");
    CloseHandle(file);
    const wchar_t *leaves[] = {L"A\\images\\a.png", L"A\\outside.jpg"};
    for (size_t index = 0; index < 2; ++index)
    {
        wchar_t from[1024], to[1024];
        path_for(leaves[index], from);
        require(swprintf_s(to, 1024, L"%ls.%zu.original", destination, index) > 0 &&
                    CopyFileW(from, to, TRUE),
                "original image evidence");
    }
    image_export_destroy(images);
    note_archive_destroy(archive);
    note_text_destroy(body);
}
static void cleanup(void)
{
    const wchar_t *files[] = {L"A\\images\\a.png", L"A\\images\\bad.png", L"A\\outside.jpg"};
    wchar_t path[1024];
    for (size_t index = 0; index < 3; ++index)
    {
        path_for(files[index], path);
        require(DeleteFileW(path), "remove owned test file");
    }
    path_for(L"A\\images", path);
    require(RemoveDirectoryW(path), "remove owned images directory");
    path_for(L"A", path);
    require(RemoveDirectoryW(path) && RemoveDirectoryW(directory), "remove owned fixture");
}
static void shutdown_case(const struct image_port *_Nonnull port, const char *_Nonnull url)
{
    struct image_source *source = source_for(url);
    struct image_request request = {.category = "A", .source = source};
    uint64_t ticket = 0;
    require(port->request(port->adapter, &request, &ticket) == IMAGE_PENDING,
            "shutdown while network pending");
    image_source_destroy(source);
    Sleep(1500);
}
static void download_case(const struct image_port *_Nonnull port, char *_Nonnull *_Nonnull args)
{
    struct image_work work = {
        .adapter = port->adapter, .cancelled = cancelled, .progress = progress};
    unsigned long delay = strtoul(args[4], nullptr, 10);
    cancel_after = delay == 0 ? nullptr : relative_timer(delay);
    struct image_download *_Nullable download = nullptr;
    puts("download-start");
    fflush(stdout);
    enum image_outcome actual = image_download_read(args[2], &work, &download);
    printf("download outcome=%d bytes=%zu\n", actual,
           download == nullptr ? 0 : image_download_bytes(download).length);
    fflush(stdout);
    require((unsigned long)actual == strtoul(args[3], nullptr, 10), "expected download outcome");
    image_download_destroy(download);
    if (cancel_after != nullptr)
        CloseHandle(cancel_after);
    cancel_after = nullptr;
}
int main(int argc, char *_Nonnull *_Nonnull argv)
{
    require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "test COM");
    struct storage_path *root = prepare();
    struct image_adapter *_Nullable adapter = nullptr;
    require(image_adapter_create(root, &adapter) == IMAGE_READY, "native image adapter");
    struct image_port port = image_adapter_port(adapter);
    if (argc == 1)
    {
        phases_contract(&port);
        expect_size(&port, "images/a.png", (struct image_size){2048, 512});
        expect_size(&port, "images/a.png", (struct image_size){2048, 512});
        asset_contract(&port);
        cancel_contract(&port);
        write_image(L"A\\images\\a.png", (struct image_size){100, 50}, false);
        expect_size(&port, "images/a.png", (struct image_size){100, 50});
        expect_size(&port, "outside.jpg", (struct image_size){320, 160});
        rejected_files(&port);
    }
    if (argc == 3 && strcmp(argv[1], "--export") == 0)
        export_zip(&port, argv[2]);
    if (argc == 3 && strcmp(argv[1], "--shutdown") == 0)
        shutdown_case(&port, argv[2]);
    if (argc == 5 && strcmp(argv[1], "--download-case") == 0)
        download_case(&port, argv);
    if (argc == 2)
    {
        struct image_bitmap *_Nullable bitmap = nullptr;
        require(request_image(&port, argv[1], &bitmap) == IMAGE_READY,
                "real HTTPS download and decode");
        image_bitmap_destroy(bitmap);
    }
    puts("shutdown-start");
    fflush(stdout);
    image_adapter_destroy(adapter);
    puts("shutdown-complete");
    fflush(stdout);
    storage_path_destroy(root);
    if (argc > 1)
    {
        wchar_t path[1024];
        path_for(L"A\\images\\bad.png", path);
        HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
        require(file != INVALID_HANDLE_VALUE, "network cleanup marker");
        CloseHandle(file);
    }
    cleanup();
    CoUninitialize();
    printf("image adapter: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
