/* #520: body-first loading, processing notice and mode cancellation through the real window. */
#include "accessible_request.h"
#include "accessible_view.h"
#include "appearance_adapter.h"
#include "file_bytes.h"
#include "file_times.h"
#include "folio_menu_action.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
#include "image_bitmap.h"
#include "image_port.h"
#include "note_file_times.h"
#include "note_history.h"
#include "note_ledger.h"
#include "note_text.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "storage_adapter.h"
#include "storage_state.h"
#include "transfer_adapter.h"
#include "web_link_adapter.h"
#include <commctrl.h>
#include <richedit.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

struct native_fixture
{
    struct storage_adapter *_Nullable storage;
    struct storage_state *_Nullable location;
    struct persistence_adapter *_Nullable files;
    struct appearance_adapter *_Nullable appearance;
    struct regex_adapter *_Nullable regex;
    struct image_adapter *_Nullable images;
    struct font_bundle *_Nullable fonts;
    struct folio_state *_Nullable state;
};

static unsigned checks;
static bool diagram_case;
static bool history_case;

static const char history_current[] = "# Current 日本語 😀 **saved**";
static const char history_previous[] = "> Previous 猫 🐾 `old`";
static const char history_oldest[] = "- Oldest 履歴 🌸";

static void require(bool condition, const char *_Nonnull message)
{
    ++checks;
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void seed_history(struct persistence_port disk)
{
    const char *versions[] = {history_oldest, history_previous};
    for (size_t index = 0; index < sizeof versions / sizeof versions[0]; ++index)
    {
        struct note_text *_Nullable body = nullptr;
        require(note_text_create(versions[index], strlen(versions[index]), &body) ==
                    NOTE_TEXT_ACCEPTED,
                "valid history fixture");
        require(disk.write_note(disk.adapter, "schedule", "one", body) == PERSISTENCE_STORED &&
                    disk.archive_note(disk.adapter, "schedule", "one") == PERSISTENCE_STORED,
                "fixture history uses the canonical archive port");
        note_text_destroy(body);
    }
}

static void seed_notes(struct persistence_port disk)
{
    enum persistence_outcome created = disk.create_category(disk.adapter, "schedule");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "test category");
    created = disk.create_category(disk.adapter, "target");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "move category");
    const char *source =
        diagram_case ? "Body before diagrams\n\n```mermaid\nflowchart LR\nA --> B\n```\n\nAfter\n"
                     : "Body before images\n\n![alt](images/one.png)\n\nBody after images\n";
    if (history_case)
    {
        seed_history(disk);
        source = history_current;
    }
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(source, strlen(source), &body) == NOTE_TEXT_ACCEPTED, "media note");
    require(disk.write_note(disk.adapter, "schedule", "one", body) == PERSISTENCE_STORED,
            "image note stored");
    note_text_destroy(body);
}
struct image_adapter
{
    uint64_t ticket;
    size_t requests;
    enum image_phase phase;
    enum image_outcome outcome;
    char category[64];
};
static struct image_adapter controlled;
static enum image_outcome request_image(struct image_adapter *_Nonnull adapter,
                                        const struct image_request *_Nonnull request,
                                        uint64_t *_Nonnull ticket)
{
    require(strlen(request->category) < sizeof adapter->category, "request category fits");
    memcpy(adapter->category, request->category, strlen(request->category) + 1);
    *ticket = ++adapter->ticket;
    ++adapter->requests;
    adapter->phase = IMAGE_PHASE_READING;
    adapter->outcome = IMAGE_PENDING;
    return IMAGE_PENDING;
}
static enum image_outcome poll_image(struct image_adapter *_Nonnull adapter, uint64_t ticket,
                                     struct image_bitmap *_Nullable *_Nonnull out)
{
    if (ticket != adapter->ticket)
        return IMAGE_CANCELLED;
    if (adapter->outcome != IMAGE_READY)
        return adapter->outcome;
    static const unsigned char pixel[] = {0x40, 0x90, 0xE0, 0xFF};
    return image_bitmap_create(pixel, (struct image_size){1, 1}, out);
}
static enum image_outcome request_diagram(struct image_adapter *_Nonnull adapter,
                                          const struct diagram_request *_Nonnull request,
                                          uint64_t *_Nonnull ticket)
{
    (void)request;
    *ticket = ++adapter->ticket;
    ++adapter->requests;
    adapter->phase = IMAGE_PHASE_DIAGRAM_PARSING;
    adapter->outcome = IMAGE_PENDING;
    return IMAGE_PENDING;
}
static enum image_phase image_phase(struct image_adapter *_Nonnull adapter)
{
    return adapter->phase;
}
static void cancel_image(struct image_adapter *_Nonnull adapter)
{
    ++adapter->ticket;
    adapter->phase = IMAGE_PHASE_IDLE;
}
static enum image_outcome image_asset(struct image_adapter *_Nonnull adapter,
                                      const struct image_request *_Nonnull request,
                                      enum image_export_policy policy,
                                      struct image_asset *_Nullable *_Nonnull out)
{
    (void)adapter;
    (void)request;
    (void)policy;
    (void)out;
    return IMAGE_FAILED;
}
static void create_fixture(struct native_fixture *_Nonnull fixture)
{
    require(storage_adapter_create(&fixture->storage) == STORAGE_READY, "storage adapter");
    require(appearance_adapter_create(&fixture->appearance) == APPEARANCE_ADAPTER_CREATED,
            "appearance adapter");
    require(regex_adapter_create(&fixture->regex) == REGEX_ADAPTER_CREATED, "regex adapter");
    require(font_bundle_register(&fixture->fonts) == FONT_BUNDLE_READY, "bundled fonts");
    struct storage_port storage = storage_adapter_port(fixture->storage);
    require(storage_state_create(&storage, &fixture->location) == STORAGE_READY,
            "portable storage");
    require(persistence_adapter_create(storage_state_directory(fixture->location),
                                       &fixture->files) == PERSISTENCE_ADAPTER_CREATED,
            "dedicated test data");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    seed_notes(disk);
    struct appearance_port appearance = appearance_adapter_port(fixture->appearance);
    struct regex_port regex = regex_adapter_port(fixture->regex);
    struct web_link_port web = web_link_adapter_port();
    struct transfer_port transfer = transfer_adapter_port();
    fixture->images = &controlled;
    struct image_port images = {.adapter = &controlled,
                                .request = request_image,
                                .diagram = request_diagram,
                                .poll = poll_image,
                                .phase = image_phase,
                                .cancel = cancel_image,
                                .asset = image_asset};
    struct folio_ports ports = {.persistence = &disk,
                                .appearance = &appearance,
                                .regex = &regex,
                                .web_link = &web,
                                .transfer = &transfer,
                                .images = &images};
    require(folio_state_create(&ports, &fixture->state) == FOLIO_STATE_READY, "application state");
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "Japanese status");
    require(folio_state_set_number(fixture->state, true) == FOLIO_STATE_READY,
            "line numbers enabled");
    require(folio_state_set_expansion(fixture->state, 0, true) == FOLIO_STATE_READY,
            "expanded notes");
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
}

static void destroy_fixture(struct native_fixture *_Nonnull fixture)
{
    folio_state_destroy(fixture->state);
    cancel_image(fixture->images);
    font_bundle_destroy(fixture->fonts);
    regex_adapter_destroy(fixture->regex);
    appearance_adapter_destroy(fixture->appearance);
    persistence_adapter_destroy(fixture->files);
    storage_state_destroy(fixture->location);
    storage_adapter_destroy(fixture->storage);
}

static void pump(struct folio_window *_Nonnull window)
{
    MSG message;
    for (unsigned count = 0; count < 1000 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE);
         ++count)
    {
        if (message.message == WM_TIMER)
            continue;
        if (!folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}
static void action(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_request request = {.item = {.kind = kind},
                                         .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK, "mode action during loading");
    pump(window);
}
static void expect_notice(struct folio_window *_Nonnull window, const wchar_t *_Nonnull expected)
{
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen &&
                !IsRectEmpty(&view.bounds),
            "visible status line");
    require(wcscmp(view.name, expected) == 0, "processing status text");
}
static void body_first(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(root != nullptr && body != nullptr, "native windows");
    SetWindowPos(root, nullptr, 20, 20, 1000, 750, SWP_NOZORDER | SWP_NOACTIVATE);
    struct accessible_request selection = {
        .item = {.kind = ACCESSIBLE_NOTE,
                 .ref = {.category = 0, .note = 0},
                 .revision = folio_window_accessible_revision(window)},
        .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &selection) == S_OK, "select image note");
    pump(window);
    wchar_t text[1024];
    GetWindowTextW(body, text, 1024);
    require(wcsstr(text, L"Body before images") != nullptr && controlled.requests == 0,
            "text is rendered before image request");
    SendMessageW(root, WM_TIMER, 85, 0);
    require(controlled.requests == 1, "lazy image request after body");
    expect_notice(window, L"画像を読み込んでいます");
    controlled.phase = IMAGE_PHASE_RESIZING;
    SendMessageW(root, WM_TIMER, 85, 0);
    expect_notice(window, L"画像を加工しています");
    uint64_t ticket = controlled.ticket;
    action(window, ACCESSIBLE_EDIT);
    require(controlled.ticket > ticket && controlled.phase == IMAGE_PHASE_IDLE,
            "edit cancels pending processing");
    GetWindowTextW(body, text, 1024);
    require(wcsstr(text, L"![alt](images/one.png)") != nullptr, "edit contains original path");
    action(window, ACCESSIBLE_VIEW);
    require(controlled.requests == 1, "retry waits for next idle interval");
    SendMessageW(root, WM_TIMER, 85, 0);
    require(controlled.requests == 2, "view retry starts a new image request");
}
static void move_origin(struct folio_window *_Nonnull window, struct folio_state *_Nonnull state)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 0, .cpMax = 4};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    size_t requests = controlled.requests;
    uint64_t ticket = controlled.ticket;
    require(folio_state_move_note(state, (struct note_ref){0, 0}, (struct note_ref){1, 0}) ==
                    FOLIO_STATE_READY &&
                controlled.ticket > ticket,
            "move immediately cancels previous image request");
    SendMessageW(root, WM_TIMER, 85, 0);
    require(controlled.requests == requests, "moved document prepares placeholders first");
    CHARRANGE actual = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&actual);
    require(actual.cpMin == selected.cpMin && actual.cpMax == selected.cpMax,
            "move refresh preserves visible selection");
    SendMessageW(root, WM_TIMER, 85, 0);
    require(controlled.requests == requests + 1 && strcmp(controlled.category, "target") == 0,
            "new request uses the moved Markdown directory");
    require(folio_state_move_note(state, (struct note_ref){1, 0}, (struct note_ref){0, 0}) ==
                FOLIO_STATE_READY,
            "restore isolated fixture location");
}

static RECT loading_notice(struct folio_window *_Nonnull window, bool resizing)
{
    action(window, ACCESSIBLE_EDIT);
    action(window, ACCESSIBLE_VIEW);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SendMessageW(root, WM_TIMER, 85, 0);
    controlled.phase = resizing ? IMAGE_PHASE_RESIZING : IMAGE_PHASE_READING;
    SendMessageW(root, WM_TIMER, 85, 0);
    pump(window);
    expect_notice(window, resizing ? L"画像を加工しています" : L"画像を読み込んでいます");
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "loading notice bounds");
    POINT points[] = {{view.bounds.left, view.bounds.top}, {view.bounds.right, view.bounds.bottom}};
    MapWindowPoints(HWND_DESKTOP, root, points, 2);
    return (RECT){points[0].x, points[0].y, points[1].x, points[1].y};
}

static void completed_image(struct folio_window *_Nonnull window, bool resizing)
{
    RECT notice = loading_notice(window, resizing);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {0, 4}, actual = {0};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    LRESULT dirty = SendMessageW(body, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(body, EM_CANUNDO, 0, 0);
    wchar_t before[1024], after[1024];
    int length = GetWindowTextW(body, before, 1024);
    RECT body_before, body_after;
    GetWindowRect(body, &body_before);
    ValidateRect(root, nullptr);
    controlled.phase = IMAGE_PHASE_IDLE;
    controlled.outcome = IMAGE_READY;
    SendMessageW(root, WM_TIMER, 85, 0);
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen &&
                wcsstr(view.name, L"画像") == nullptr && wcsstr(view.name, L"F1") != nullptr,
            "completed image replaces loading notice with normal status");
    RECT damage;
    require(GetUpdateRect(root, &damage, FALSE) && damage.left <= notice.left &&
                damage.top <= notice.top && damage.right >= notice.right &&
                damage.bottom >= notice.bottom,
            "completed image repaints the entire old notice area");
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&actual);
    GetWindowRect(body, &body_after);
    require(EqualRect(&body_before, &body_after), "completion does not move the body window");
    require(selected.cpMin == actual.cpMin && selected.cpMax == actual.cpMax,
            "completion preserves selection");
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(body, EM_CANUNDO, 0, 0) == undo,
            "completion preserves dirty and Undo");
    require(GetWindowTextW(body, after, 1024) == length &&
                memcmp(before, after, ((size_t)length + 1) * sizeof before[0]) == 0,
            "completion preserves displayed text");
    UpdateWindow(root);
    require(!GetUpdateRect(root, &damage, FALSE), "completion damage was painted");
}
static void file_timestamp_roundtrip(void)
{
    const wchar_t *path = L"status-530-attributes.md";
    require(file_bytes_store(path, "before", 6) == PERSISTENCE_STORED, "attribute fixture");
    HANDLE file = CreateFileW(path, GENERIC_READ | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    SYSTEMTIME fixed = {.wYear = 2020, .wMonth = 2, .wDay = 29, .wHour = 12};
    FILETIME created, updated, observed;
    require(file != INVALID_HANDLE_VALUE && SystemTimeToFileTime(&fixed, &created) &&
                SetFileTime(file, &created, nullptr, &created),
            "fixed file attributes");
    CloseHandle(file);
    require(file_bytes_store(path, "after", 5) == PERSISTENCE_STORED, "atomic replacement");
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE && GetFileTime(file, &observed, nullptr, &updated) &&
                CompareFileTime(&created, &observed) == 0 &&
                CompareFileTime(&created, &updated) < 0,
            "replacement preserves creation and advances the last write");
    struct note_file_times *times = nullptr;
    require(file_times_read(path, &times) == PERSISTENCE_LOADED &&
                strncmp(note_file_times_created(times), "2020-", 5) == 0,
            "adapter converts filesystem values into local display values");
    note_file_times_destroy(times);
    require(file_bytes_store(path, "blocked", 7) == PERSISTENCE_UNWRITABLE,
            "a locked destination refuses replacement");
    CloseHandle(file);
    struct file_bytes *bytes = nullptr;
    require(file_bytes_read(path, &bytes) == PERSISTENCE_LOADED && file_bytes_length(bytes) == 5 &&
                memcmp(file_bytes_data(bytes), "after", 5) == 0,
            "failed publication retains the previous bytes");
    file_bytes_destroy(bytes);
    require(DeleteFileW(path), "remove attribute fixture");
    times = nullptr;
    require(file_times_read(path, &times) == PERSISTENCE_UNREADABLE && times == nullptr,
            "absent attributes leave output untouched");
}

static void status_pixels(HWND root, const char *_Nonnull path,
                          const struct theme_palette *_Nonnull palette)
{
    RECT client;
    GetClientRect(root, &client);
    HDC source = GetDC(root);
    HDC dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(root, dc, PW_CLIENTONLY), "render real status pixels");
    struct rgb_color background = palette->status_background;
    require(GetPixel(dc, 2, client.bottom - 12) ==
                RGB(background.red, background.green, background.blue),
            "status background uses the canonical palette role");
    SelectObject(dc, old);
    BITMAPINFO info = {.bmiHeader = {.biSize = sizeof(BITMAPINFOHEADER),
                                     .biWidth = client.right,
                                     .biHeight = -client.bottom,
                                     .biPlanes = 1,
                                     .biBitCount = 32,
                                     .biCompression = BI_RGB}};
    size_t length = (size_t)client.right * (size_t)client.bottom * 4;
    void *pixels = malloc(length);
    require(pixels != nullptr && GetDIBits(dc, bitmap, 0, (UINT)client.bottom, pixels, &info,
                                           DIB_RGB_COLORS) == (int)client.bottom,
            "capture status evidence");
    BITMAPFILEHEADER header = {.bfType = 0x4D42,
                               .bfSize = (DWORD)(sizeof header + sizeof info.bmiHeader + length),
                               .bfOffBits = sizeof header + sizeof info.bmiHeader};
    FILE *output = nullptr;
    require(fopen_s(&output, path, "wb") == 0 && output != nullptr, "open status evidence");
    require(fwrite(&header, sizeof header, 1, output) == 1 &&
                fwrite(&info.bmiHeader, sizeof info.bmiHeader, 1, output) == 1 &&
                fwrite(pixels, length, 1, output) == 1,
            "write status evidence");
    fclose(output);
    free(pixels);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(root, source);
}

static HWND status_tooltip(HWND root)
{
    for (HWND candidate = GetWindow(root, GW_HWNDFIRST); candidate != nullptr;
         candidate = GetWindow(candidate, GW_HWNDNEXT))
    {
        wchar_t name[64];
        if (GetWindow(candidate, GW_OWNER) == root && GetClassNameW(candidate, name, 64) > 0 &&
            wcscmp(name, TOOLTIPS_CLASSW) == 0)
            return candidate;
    }
    return nullptr;
}

static void invoke_operation(struct folio_window *_Nonnull window, enum folio_command command)
{
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_MENU},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    if (command == FOLIO_COMMAND_FIND)
    {
        request.item.kind = ACCESSIBLE_FIND;
        require(folio_window_accessible_action(window, &request) == S_OK, "open Find");
        return;
    }
    require(folio_window_accessible_action(window, &request) == S_OK, "open operations menu");
    for (size_t at = 0; at < folio_menu_action_count(FOLIO_MENU_OPERATIONS); ++at)
    {
        const struct folio_menu_action *entry = folio_menu_action_at(FOLIO_MENU_OPERATIONS, at);
        if (folio_menu_action_kind(entry) == FOLIO_MENU_ACTION_COMMAND &&
            folio_menu_action_command(entry) == command)
        {
            request.item = (struct accessible_item){
                .kind = ACCESSIBLE_SURFACE_CHOICE,
                .slot = at,
                .revision = folio_window_accessible_surface_revision(window)};
            require(folio_window_accessible_action(window, &request) == S_OK,
                    "invoke the existing command from operations");
            return;
        }
    }
    require(false, "requested operation exists");
}

static void short_window_band(struct native_fixture *_Nonnull fixture, enum folio_command command)
{
    require(folio_state_begin_edit(fixture->state) == FOLIO_STATE_READY, "geometry edit state");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "geometry window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetWindowPos(root, nullptr, 20, 20, 420, 360, SWP_NOZORDER | SWP_NOACTIVATE);
    invoke_operation(window, command);
    pump(window);
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view status;
    RECT body_rect;
    require(folio_window_accessible_read(window, &item, &status) == S_OK &&
                GetWindowRect(body, &body_rect) && body_rect.bottom <= status.bounds.top,
            "minimum-height search and replace keep the body above the status bar");
    SendMessageW(root, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    struct accessible_item surface = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view closed;
    require(FAILED(folio_window_accessible_read(window, &surface, &closed)) || closed.offscreen,
            "Escape closes the command band through the normal key handler");
    folio_window_destroy(window);
}

static void normal_status(struct native_fixture *_Nonnull fixture, enum folio_language language,
                          enum folio_theme_choice theme, int width)
{
    if (language == FOLIO_LANGUAGE_ZH_HANS)
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_DRAWER);
    require(folio_state_set_language(fixture->state, language) == FOLIO_STATE_READY &&
                folio_state_set_theme(fixture->state, theme) == FOLIO_STATE_READY,
            "status language and theme");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "status window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SetWindowPos(root, nullptr, 20, 20, width, 600, SWP_NOZORDER | SWP_NOACTIVATE);
    pump(window);
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen &&
                !view.live && wcsstr(view.name, L"F1") != nullptr &&
                wcsstr(view.name, L":") != nullptr,
            "normal status is visible, complete and not a live announcement");
    wchar_t created[17];
    require(MultiByteToWideChar(CP_UTF8, 0, folio_state_note_times(fixture->state).created, -1,
                                created, 17) == 17 &&
                wcsstr(view.name, created) != nullptr,
            "even narrow UIA retains the complete creation time");
    HWND tooltip = status_tooltip(root);
    wchar_t text[1024] = {0};
    TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                      .hwnd = root,
                      .uId = ACCESSIBLE_NOTICE,
                      .lpszText = text};
    require(tooltip != nullptr, "real status tooltip");
    SendMessageW(tooltip, TTM_GETTEXTW, 1024, (LPARAM)&tool);
    if (wcscmp(text, view.name) != 0)
        fwprintf(stderr, L"tooltip=[%ls] UIA=[%ls]\n", text, view.name);
    require(wcscmp(text, view.name) == 0,
            "tooltip and UIA expose the same full status information");
    const char *paths[] = {"status-ja-light.bmp", "status-en-dark.bmp", "status-zh-narrow.bmp"};
    status_pixels(root, paths[language], folio_state_palette(fixture->state));
    folio_window_destroy(window);
}

static void verify_icon(HICON icon, int size)
{
    ICONINFO info = {0};
    require(icon != nullptr && GetIconInfo(icon, &info), "window icon is a valid image");
    BITMAP bitmap = {0};
    require(GetObjectW(info.hbmColor, (int)sizeof bitmap, &bitmap) == (int)sizeof bitmap &&
                bitmap.bmWidth == size && bitmap.bmHeight == size && bitmap.bmBitsPixel == 32,
            "window icon uses its requested native size and 32-bit alpha");
    size_t bytes = (size_t)size * (size_t)size * 4;
    unsigned char *pixels = malloc(bytes);
    require(pixels != nullptr, "icon pixel buffer");
    HDC device = CreateCompatibleDC(nullptr);
    require(device != nullptr, "icon read device");
    BITMAPINFO format = {.bmiHeader = {.biSize = sizeof(BITMAPINFOHEADER),
                                       .biWidth = size,
                                       .biHeight = -size,
                                       .biPlanes = 1,
                                       .biBitCount = 32,
                                       .biCompression = BI_RGB}};
    require(GetDIBits(device, info.hbmColor, 0, (UINT)size, pixels, &format, DIB_RGB_COLORS) ==
                size,
            "icon alpha pixels are readable");
    size_t center = ((size_t)(size / 2) * (size_t)size + (size_t)(size / 2)) * 4 + 3;
    require(pixels[3] < 255 && pixels[(size_t)size * 4 - 1] < 255 &&
                pixels[(size_t)(size - 1) * (size_t)size * 4 + 3] < 255 &&
                pixels[bytes - 1] < 255 && pixels[center] == 255,
            "embedded icon retains antialiased transparent corners and opaque content");
    free(pixels);
    DeleteDC(device);
    DeleteObject(info.hbmColor);
    DeleteObject(info.hbmMask);
}

static void icon_pair(HWND root, UINT dpi)
{
    HICON large_icon = (HICON)SendMessageW(root, WM_GETICON, ICON_BIG, 0);
    HICON small_icon = (HICON)SendMessageW(root, WM_GETICON, ICON_SMALL, 0);
    require(large_icon != small_icon, "large and small icons have independent owned handles");
    verify_icon(large_icon, GetSystemMetricsForDpi(SM_CXICON, dpi));
    verify_icon(small_icon, GetSystemMetricsForDpi(SM_CXSMICON, dpi));
}

static void window_icons(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real window with application icons");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    icon_pair(root, GetDpiForWindow(root));
    RECT bounds;
    require(GetWindowRect(root, &bounds), "icon DPI rectangle");
    ICONINFO released = {0};
    static const UINT scales[] = {96, 120, 144, 168, 192, 216, 240, 288, 336, 384};
    for (size_t index = 0; index < sizeof scales / sizeof scales[0]; ++index)
    {
        HICON previous = (HICON)SendMessageW(root, WM_GETICON, ICON_BIG, 0);
        SendMessageW(root, WM_DPICHANGED, MAKEWPARAM(scales[index], scales[index]),
                     (LPARAM)&bounds);
        icon_pair(root, scales[index]);
        require(!GetIconInfo(previous, &released), "DPI replacement releases the old icon");
    }
    HICON large_icon = (HICON)SendMessageW(root, WM_GETICON, ICON_BIG, 0);
    HICON small_icon = (HICON)SendMessageW(root, WM_GETICON, ICON_SMALL, 0);
    folio_window_destroy(window);
    require(!GetIconInfo(large_icon, &released) && !GetIconInfo(small_icon, &released),
            "window destruction releases both icon handles");
}

static void run_status_case(struct native_fixture *_Nonnull fixture, const char *_Nonnull selector)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select status fixture note");
    if (strcmp(selector, "--status-ja") == 0)
        normal_status(fixture, FOLIO_LANGUAGE_JA, FOLIO_THEME_CHOICE_LIGHT, 1100);
    else if (strcmp(selector, "--status-en") == 0)
        normal_status(fixture, FOLIO_LANGUAGE_EN, FOLIO_THEME_CHOICE_DARK, 650);
    else if (strcmp(selector, "--status-zh") == 0)
        normal_status(fixture, FOLIO_LANGUAGE_ZH_HANS, FOLIO_THEME_CHOICE_LIGHT, 320);
    else if (strcmp(selector, "--status-find") == 0)
        short_window_band(fixture, FOLIO_COMMAND_FIND);
    else if (strcmp(selector, "--status-replace") == 0)
        short_window_band(fixture, FOLIO_COMMAND_REPLACE);
    else
        require(false, "known native status selector");
}

static void run_image_case(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real main window");
    body_first(window);
    move_origin(window, fixture->state);
    completed_image(window, false);
    completed_image(window, true);
    folio_window_destroy(window);
    file_timestamp_roundtrip();
}

static void run_diagram_case(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real diagram window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetWindowPos(root, nullptr, 20, 20, 1000, 750, SWP_NOZORDER | SWP_NOACTIVATE);
    struct accessible_request selection = {
        .item = {.kind = ACCESSIBLE_NOTE,
                 .ref = {.category = 0, .note = 0},
                 .revision = folio_window_accessible_revision(window)},
        .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &selection) == S_OK, "select diagram note");
    pump(window);
    wchar_t text[1024];
    GetWindowTextW(body, text, 1024);
    require(wcsstr(text, L"Body before diagrams") != nullptr && controlled.requests == 0,
            "body visible before diagram request");
    SendMessageW(root, WM_TIMER, 85, 0);
    expect_notice(window, L"図を読み取っています");
    controlled.phase = IMAGE_PHASE_DIAGRAM_LAYOUT;
    SendMessageW(root, WM_TIMER, 85, 0);
    expect_notice(window, L"図を配置しています");
    controlled.phase = IMAGE_PHASE_DIAGRAM_DRAWING;
    SendMessageW(root, WM_TIMER, 85, 0);
    expect_notice(window, L"図を描画しています");
    uint64_t ticket = controlled.ticket;
    action(window, ACCESSIBLE_EDIT);
    require(controlled.ticket > ticket && controlled.phase == IMAGE_PHASE_IDLE,
            "edit cancels native diagram request");
    GetWindowTextW(body, text, 1024);
    require(wcsstr(text, L"```mermaid") != nullptr && wcsstr(text, L"A --> B") != nullptr,
            "editing restores exact diagram source");
    action(window, ACCESSIBLE_VIEW);
    SendMessageW(root, WM_TIMER, 85, 0);
    require(controlled.requests == 2, "view starts a fresh diagram request");
    controlled.outcome = IMAGE_READY;
    SendMessageW(root, WM_TIMER, 85, 0);
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK &&
                wcsstr(view.name, L"F1") != nullptr,
            "diagram completion restores normal status");
    folio_window_destroy(window);
}

struct history_snapshot
{
    struct note_text *_Nullable files[note_history_depth + 1];
    uint64_t store_revision;
};

static enum persistence_outcome history_file(struct persistence_port disk, size_t version,
                                             struct note_text *_Nullable *_Nonnull out)
{
    if (version == 0)
        return disk.read_note(disk.adapter, "schedule", "one", out);
    struct history_version which = {.category = "schedule", .note = "one", .version = version};
    return disk.read_history(disk.adapter, &which, out);
}

static void capture_history(struct native_fixture *_Nonnull fixture,
                            struct history_snapshot *_Nonnull snapshot)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    snapshot->store_revision = folio_state_store_revision(fixture->state);
    for (size_t version = 0; version <= note_history_depth; ++version)
    {
        enum persistence_outcome outcome = history_file(disk, version, &snapshot->files[version]);
        require(outcome == PERSISTENCE_LOADED || outcome == PERSISTENCE_ABSENT,
                "snapshot only valid dedicated note and history files");
    }
    require(snapshot->files[0] != nullptr && snapshot->files[1] != nullptr &&
                snapshot->files[2] != nullptr,
            "current note and two seeded versions exist");
}

static void expect_history_file(struct persistence_port disk, size_t version,
                                const struct note_text *_Nullable expected)
{
    struct note_text *_Nullable actual = nullptr;
    enum persistence_outcome outcome = history_file(disk, version, &actual);
    if (expected == nullptr)
        require(outcome == PERSISTENCE_ABSENT && actual == nullptr, "absent version stays absent");
    else
    {
        require(outcome == PERSISTENCE_LOADED && actual != nullptr, "stored version is readable");
        size_t length = note_text_file_length(expected);
        require(note_text_file_length(actual) == length &&
                    memcmp(note_text_file_bytes(actual), note_text_file_bytes(expected), length) ==
                        0,
                "stored file matches every expected byte");
    }
    note_text_destroy(actual);
}

static void verify_history_unchanged(struct native_fixture *_Nonnull fixture,
                                     const struct history_snapshot *_Nonnull snapshot)
{
    require(folio_state_store_revision(fixture->state) == snapshot->store_revision,
            "restore and Undo do not publish a save");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    for (size_t version = 0; version <= note_history_depth; ++version)
        expect_history_file(disk, version, snapshot->files[version]);
}

static void expect_editor(struct folio_window *_Nonnull window,
                          const struct folio_state *_Nonnull state,
                          const wchar_t *_Nonnull expected, enum folio_note_change change)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    wchar_t text[128];
    int length = GetWindowTextW(body, text, 128);
    require(length >= 0 && (size_t)length == wcslen(expected) && wcscmp(text, expected) == 0,
            "editor retains exact Markdown and Unicode units");
    require(folio_state_pane_mode(state) == PANE_MODE_EDIT &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) == 0,
            "restored document is editable");
    require(folio_state_unsaved(state, text, (size_t)length) == change,
            "canonical unsaved comparison matches the editor");
}

static void adopt_history(struct folio_window *_Nonnull window, struct folio_state *_Nonnull state,
                          size_t index, const char *_Nonnull expected)
{
    invoke_operation(window, FOLIO_COMMAND_HISTORY);
    pump(window);
    require(folio_state_history_count(state) > index, "requested history row exists");
    const struct note_text *body = folio_state_history_body(state, index);
    require(body != nullptr && strcmp(note_text_bytes(body), expected) == 0,
            "row resolves to the expected archived version");
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item row;
    require(folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &row) ==
                S_OK,
            "history exposes a first row");
    for (size_t at = 0; at < index; ++at)
        require(folio_window_accessible_navigate(window, &row, ACCESSIBLE_NAVIGATION_NEXT, &row) ==
                    S_OK,
                "history exposes the requested following row");
    struct accessible_request request = {.item = row, .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "adopt public history row");
    pump(window);
    HWND editor = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {0};
    SendMessageW(editor, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin == 0 && selected.cpMax == 0 && GetFocus() == editor,
            "adoption puts the caret and focus at the body start");
    require(folio_state_history_count(state) == 0 &&
                folio_window_accessible_surface_revision(window) != list.revision,
            "adoption closes and retires the history list");
}

static void history_undo(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) != 0 && SendMessageW(body, EM_UNDO, 0, 0) != 0,
            "one native Undo succeeds");
    pump(window);
}

static void history_from_edit(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull snapshot)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE all = {.cpMin = 0, .cpMax = -1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&all);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"# Draft 未保存 🙂");
    pump(window);
    expect_editor(window, fixture->state, L"# Draft 未保存 🙂", FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    adopt_history(window, fixture->state, 0, history_previous);
    expect_editor(window, fixture->state, L"> Previous 猫 🐾 `old`", FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    history_undo(window);
    expect_editor(window, fixture->state, L"# Draft 未保存 🙂", FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
}

static void save_restored_history(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture,
                                  const struct history_snapshot *_Nonnull snapshot)
{
    adopt_history(window, fixture->state, 0, history_previous);
    verify_history_unchanged(fixture, snapshot);
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_SAVE},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "explicit public Save");
    pump(window);
    expect_editor(window, fixture->state, L"> Previous 猫 🐾 `old`", FOLIO_NOTE_SAME);
    require(folio_state_store_revision(fixture->state) == snapshot->store_revision + 1,
            "explicit Save publishes exactly one new store revision");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_history_file(disk, 0, snapshot->files[1]);
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_history_file(disk, version, snapshot->files[version - 1]);
}

/* #649: 通常の履歴採用は未保存の編集で、VIEW/EDITのどちらからもUndo一回で戻せる。 */
static void run_history_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated history note");
    struct history_snapshot snapshot = {0};
    capture_history(fixture, &snapshot);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real history window");
    pump(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW, "start from saved view");
    adopt_history(window, fixture->state, 1, history_oldest);
    expect_editor(window, fixture->state, L"- Oldest 履歴 🌸", FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &snapshot);
    history_undo(window);
    expect_editor(window, fixture->state, L"# Current 日本語 😀 **saved**", FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &snapshot);
    history_from_edit(window, fixture, &snapshot);
    save_restored_history(window, fixture, &snapshot);
    require(controlled.requests == 0, "plain history does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(snapshot.files[version]);
}

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    HDESK desktop = CreateDesktopW(L"NeNeFolioImageStatus520", nullptr, nullptr, 0,
                                   DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_READOBJECTS |
                                       DESKTOP_WRITEOBJECTS,
                                   nullptr);
    require(desktop != nullptr && SetThreadDesktop(desktop), "isolated desktop");
    require(SUCCEEDED(OleInitialize(nullptr)), "COM apartment");
    diagram_case = argc == 2 && strcmp(argv[1], "--mermaid") == 0;
    history_case = argc == 2 && strcmp(argv[1], "--history") == 0;
    struct native_fixture fixture = {0};
    create_fixture(&fixture);
    if (diagram_case)
        run_diagram_case(&fixture);
    else if (history_case)
        run_history_case(&fixture);
    else if (argc == 2 && strcmp(argv[1], "--icon") == 0)
        window_icons(&fixture);
    else if (argc == 2)
        run_status_case(&fixture, argv[1]);
    else
    {
        require(argc == 1, "valid argument count");
        run_image_case(&fixture);
    }
    destroy_fixture(&fixture);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("image/status: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
