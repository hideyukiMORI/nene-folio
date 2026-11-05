/* #520: body-first loading, processing notice and mode cancellation through the real window. */
#include "accessible_request.h"
#include "accessible_surface_button.h"
#include "accessible_view.h"
#include "appearance_adapter.h"
#include "file_bytes.h"
#include "file_times.h"
#include "folio_menu_action.h"
#include "folio_settings.h"
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
#include "storage_native_path.h"
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
static bool replace_case;
static bool search_case;
static bool note_switch_case;
static bool zoom_case;
static bool theme_preview_case;
static bool settings_edit_case;
static bool mode_save_case;

static const char history_current[] = "# Current 日本語 😀 **saved**";
static const char history_previous[] = "> Previous 猫 🐾 `old`";
static const char history_oldest[] = "- Oldest 履歴 🌸";
static const char switch_other[] = "> Other 別ノート 🐾";
static const char switch_saved[] = "# Current 日本語 😀 **saved** 追記 🙂";
static const wchar_t switch_original[] = L"# Current 日本語 😀 **saved**";
static const wchar_t switch_edited[] = L"# Current 日本語 😀 **saved** 追記 🙂";
static const char search_current[] = "猫 😀 aaaa 日本語";
static const wchar_t search_original[] = L"猫 😀 aaaa 日本語";
static const wchar_t search_edited[] = L"猫 😀 aaaa 日本語 🐾";
static const char replace_current[] = "猫1 😀\n猫2 **saved**\n猫1 `old`";
static const wchar_t replace_original[] = L"猫1 😀\r猫2 **saved**\r猫1 `old`";
static const wchar_t replace_all[] = L"1-猫-🐾\r 😀\r2-猫-🐾\r **saved**\r1-猫-🐾\r `old`";

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
    if (history_case || replace_case || search_case || note_switch_case || zoom_case ||
        theme_preview_case || settings_edit_case || mode_save_case)
    {
        seed_history(disk);
        source = history_case || note_switch_case || zoom_case || theme_preview_case ||
                         settings_edit_case || mode_save_case
                     ? history_current
                     : replace_current;
    }
    if (search_case)
        source = search_current;
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(source, strlen(source), &body) == NOTE_TEXT_ACCEPTED, "media note");
    require(disk.write_note(disk.adapter, "schedule", "one", body) == PERSISTENCE_STORED,
            "image note stored");
    note_text_destroy(body);
    if (note_switch_case)
    {
        body = nullptr;
        require(note_text_create(switch_other, strlen(switch_other), &body) == NOTE_TEXT_ACCEPTED,
                "valid second note");
        require(disk.write_note(disk.adapter, "schedule", "two", body) == PERSISTENCE_STORED,
                "second dedicated note stored");
        note_text_destroy(body);
    }
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
    GETTEXTEX request = {.cb = sizeof text, .flags = GT_RAWTEXT, .codepage = 1200};
    LRESULT length = SendMessageW(body, EM_GETTEXTEX, (WPARAM)&request, (LPARAM)text);
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

/* #653: 公開置換入力とボタンから、実ICU・RichEdit・保存の境界を確認する。 */
static void prepare_replacement(struct folio_window *_Nonnull window,
                                struct folio_state *_Nonnull state)
{
    invoke_operation(window, FOLIO_COMMAND_REPLACE);
    pump(window);
    HWND pattern = folio_window_accessible_window(window, ACCESSIBLE_REPLACE_PATTERN);
    HWND replacement = folio_window_accessible_window(window, ACCESSIBLE_REPLACE_TEXT);
    require(pattern != nullptr && replacement != nullptr &&
                SetWindowTextW(pattern, L"(猫)([12])") &&
                SetWindowTextW(replacement, L"\\2-\\1-🐾\\n"),
            "set ordinary pattern and capture/newline replacement in the native inputs");
    pump(window);
    require(folio_state_replace_count(state) == 3, "real ICU preview finds three matches");
}

static void surface_button(struct folio_window *_Nonnull window,
                           enum accessible_surface_button button)
{
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_SURFACE_BUTTON,
                 .slot = button,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "invoke the current public surface button");
    pump(window);
}

static void expect_replace_position(struct folio_window *_Nonnull window,
                                    struct folio_state *_Nonnull state, LONG caret, size_t count)
{
    CHARRANGE selected = {0};
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_EXGETSEL, 0,
                 (LPARAM)&selected);
    require(selected.cpMin == caret && selected.cpMax == caret,
            "replacement positions the native UTF16 caret");
    require(folio_state_replace_count(state) == count, "replacement refreshes the preview count");
    struct accessible_item item = {.kind = ACCESSIBLE_REPLACE_PATTERN,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen,
            "replacement leaves its input band open");
}

/* #691: Previewing after Undo must keep each replacement as one redoable edit. */
static void redo_replacement(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull snapshot,
                             const wchar_t *_Nonnull expected)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "undone replacement can be redone");
    prepare_replacement(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_editor(window, fixture->state, replace_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
    require(GetFocus() == body && SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "closing replacement preview returns body focus and preserves Redo");
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "redo replacement once");
    pump(window);
    expect_editor(window, fixture->state, expected, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    history_undo(window);
    expect_editor(window, fixture->state, replace_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
}

static void replace_and_undo(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull snapshot)
{
    prepare_replacement(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_REPLACE_ONE);
    expect_editor(window, fixture->state, L"1-猫-🐾\r 😀\r猫2 **saved**\r猫1 `old`",
                  FOLIO_NOTE_CHANGED);
    expect_replace_position(window, fixture->state, 7, 2);
    verify_history_unchanged(fixture, snapshot);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    history_undo(window);
    expect_editor(window, fixture->state, replace_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
    redo_replacement(window, fixture, snapshot, L"1-猫-🐾\r 😀\r猫2 **saved**\r猫1 `old`");
    prepare_replacement(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_REPLACE_ALL);
    expect_editor(window, fixture->state, replace_all, FOLIO_NOTE_CHANGED);
    expect_replace_position(window, fixture->state, 0, 0);
    verify_history_unchanged(fixture, snapshot);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    history_undo(window);
    expect_editor(window, fixture->state, replace_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
    redo_replacement(window, fixture, snapshot, replace_all);
}

static void save_replacement(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull snapshot)
{
    prepare_replacement(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_REPLACE_ALL);
    expect_editor(window, fixture->state, replace_all, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_SAVE},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "explicit Save after replace");
    pump(window);
    expect_editor(window, fixture->state, replace_all, FOLIO_NOTE_SAME);
    require(folio_state_store_revision(fixture->state) == snapshot->store_revision + 1,
            "replacement is stored only by explicit Save");
    const char expected[] = "1-猫-🐾\n 😀\n2-猫-🐾\n **saved**\n1-猫-🐾\n `old`";
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(expected, sizeof expected - 1, &body) == NOTE_TEXT_ACCEPTED,
            "expected saved Unicode and LF bytes");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_history_file(disk, 0, body);
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_history_file(disk, version, snapshot->files[version - 1]);
    note_text_destroy(body);
}

static void run_replace_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated replacement note");
    struct history_snapshot snapshot = {0};
    capture_history(fixture, &snapshot);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real replacement window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, replace_original, FOLIO_NOTE_SAME);
    replace_and_undo(window, fixture, &snapshot);
    save_replacement(window, fixture, &snapshot);
    require(controlled.requests == 0, "plain replacement does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(snapshot.files[version]);
}

/* #655: 重なった一致・Unicode位置と本文/Undo保持を公開検索操作から確認する。 */
static void expect_search_result(struct folio_window *_Nonnull window, LONG start, LONG end,
                                 const wchar_t *_Nonnull status)
{
    CHARRANGE selected = {0};
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_EXGETSEL, 0,
                 (LPARAM)&selected);
    require(selected.cpMin == start && selected.cpMax == end,
            "public search selects the expected UTF16 range");
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_STATUS,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK &&
                wcscmp(view.name, status) == 0,
            "public search status reports the expected ordinal or no-match text");
}

static void search_term(struct folio_window *_Nonnull window, const wchar_t *_Nonnull text)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr && SetWindowTextW(input, text), "set the native search term");
    pump(window);
}

static void search_overlap_cycle(struct folio_window *_Nonnull window)
{
    search_term(window, L"AA");
    expect_search_result(window, 5, 7, L"1 / 3 件");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, 6, 8, L"2 / 3 件");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, 7, 9, L"3 / 3 件");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, 5, 7, L"1 / 3 件");
    surface_button(window, ACCESSIBLE_SURFACE_PREVIOUS);
    expect_search_result(window, 7, 9, L"3 / 3 件");
    surface_button(window, ACCESSIBLE_SURFACE_PREVIOUS);
    expect_search_result(window, 6, 8, L"2 / 3 件");
    surface_button(window, ACCESSIBLE_SURFACE_PREVIOUS);
    expect_search_result(window, 5, 7, L"1 / 3 件");
}

static void search_roundtrip(struct folio_window *_Nonnull window,
                             struct folio_state *_Nonnull state)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetFocus(body);
    CHARRANGE initial = {0};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&initial);
    LRESULT undo = SendMessageW(body, EM_CANUNDO, 0, 0);
    LRESULT redo = SendMessageW(body, EM_CANREDO, 0, 0);
    enum search_direction direction = folio_state_search_direction(state);
    invoke_operation(window, FOLIO_COMMAND_FIND);
    pump(window);
    search_overlap_cycle(window);
    search_term(window, L"😀");
    expect_search_result(window, 2, 4, L"1 / 1 件");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, 2, 4, L"1 / 1 件");
    surface_button(window, ACCESSIBLE_SURFACE_PREVIOUS);
    expect_search_result(window, 2, 4, L"1 / 1 件");
    search_term(window, L"不存在");
    expect_search_result(window, 2, 4, L"見つかりません");
    search_term(window, L"");
    expect_search_result(window, 2, 4, L"");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, 2, 4, L"");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    require(GetFocus() == body && SendMessageW(body, EM_CANUNDO, 0, 0) == undo &&
                SendMessageW(body, EM_CANREDO, 0, 0) == redo,
            "closing Find restores body focus and keeps Undo and Redo availability");
    require(folio_state_search_direction(state) == direction,
            "explicit previous and next buttons preserve the remembered direction");
}

static void search_unsaved(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture,
                           const struct history_snapshot *_Nonnull snapshot)
{
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, search_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE end = {.cpMin = -1, .cpMax = -1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&end);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L" 🐾");
    pump(window);
    expect_editor(window, fixture->state, search_edited, FOLIO_NOTE_CHANGED);
    search_roundtrip(window, fixture->state);
    expect_editor(window, fixture->state, search_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    history_undo(window);
    expect_editor(window, fixture->state, search_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
    /* #689: Searching a redoable document must not replace its native edit history. */
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "Undo leaves a redoable insertion before Find");
    search_roundtrip(window, fixture->state);
    expect_editor(window, fixture->state, search_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo succeeds after Find");
    pump(window);
    expect_editor(window, fixture->state, search_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    history_undo(window);
    expect_editor(window, fixture->state, search_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
}

static void run_search_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated search note");
    struct history_snapshot snapshot = {0};
    capture_history(fixture, &snapshot);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real search window");
    pump(window);
    search_roundtrip(window, fixture->state);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    wchar_t text[128];
    require(GetWindowTextW(body, text, 128) == (int)wcslen(search_original) &&
                wcscmp(text, search_original) == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "search preserves the one-line view text and readonly mode");
    verify_history_unchanged(fixture, &snapshot);
    search_unsaved(window, fixture, &snapshot);
    require(controlled.requests == 0, "plain search does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(snapshot.files[version]);
}

static void choose_note(struct folio_window *_Nonnull window, size_t index,
                        enum accessible_action operation)
{
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_NOTE,
                 .ref = {.category = 0, .note = index},
                 .revision = folio_window_accessible_revision(window)},
        .action = operation};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "select or invoke the current public note row");
    pump(window);
}

static void expect_other_unchanged(struct native_fixture *_Nonnull fixture)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *body = nullptr;
    require(disk.read_note(disk.adapter, "schedule", "two", &body) == PERSISTENCE_LOADED &&
                body != nullptr,
            "second note remains readable");
    require(note_text_file_length(body) == strlen(switch_other) &&
                memcmp(note_text_file_bytes(body), switch_other, strlen(switch_other)) == 0,
            "second note retains all stored bytes");
    note_text_destroy(body);
    for (size_t version = 1; version <= note_history_depth; ++version)
    {
        body = nullptr;
        struct history_version which = {.category = "schedule", .note = "two", .version = version};
        require(disk.read_history(disk.adapter, &which, &body) == PERSISTENCE_ABSENT &&
                    body == nullptr,
                "unchanged second note has no saved history");
        note_text_destroy(body);
    }
}

static void append_switch_text(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE end = {.cpMin = -1, .cpMax = -1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&end);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L" 追記 🙂");
    pump(window);
}

static void reselect_unsaved(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull snapshot)
{
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE before = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&before);
    const enum accessible_action operations[] = {ACCESSIBLE_ACTION_SELECT,
                                                 ACCESSIBLE_ACTION_INVOKE};
    for (size_t index = 0; index < sizeof operations / sizeof operations[0]; ++index)
    {
        choose_note(window, 0, operations[index]);
        expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
        CHARRANGE selected = {0};
        SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
        require(selected.cpMin == before.cpMin && selected.cpMax == before.cpMax &&
                    SendMessageW(body, EM_CANUNDO, 0, 0) != 0,
                "same-note selection preserves the native selection and Undo");
        verify_history_unchanged(fixture, snapshot);
        expect_other_unchanged(fixture);
    }
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, snapshot);
}

static void switch_and_return(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull snapshot)
{
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    choose_note(window, 1, ACCESSIBLE_ACTION_SELECT);
    expect_editor(window, fixture->state, L"> Other 別ノート 🐾", FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) == 0,
            "the next note cannot undo the previous document edit");
    require(folio_state_store_revision(fixture->state) == snapshot->store_revision + 1,
            "changing note stores the previous edit exactly once");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *saved = nullptr;
    require(note_text_create(switch_saved, strlen(switch_saved), &saved) == NOTE_TEXT_ACCEPTED,
            "valid saved note expectation");
    expect_history_file(disk, 0, saved);
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_history_file(disk, version, snapshot->files[version - 1]);
    expect_other_unchanged(fixture);
    choose_note(window, 0, ACCESSIBLE_ACTION_INVOKE);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) == 0 && GetFocus() == body,
            "Invoke focuses the reloaded note with a fresh Undo history");
    require(folio_state_store_revision(fixture->state) == snapshot->store_revision + 1,
            "leaving an unchanged note does not publish another save");
    expect_history_file(disk, 0, saved);
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_history_file(disk, version, snapshot->files[version - 1]);
    expect_other_unchanged(fixture);
    note_text_destroy(saved);
}

static void run_note_switch_case(struct native_fixture *_Nonnull fixture)
{
    require(strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, 0}), "one") == 0 &&
                strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, 1}), "two") == 0,
            "dedicated note order is stable");
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated first note");
    struct history_snapshot snapshot = {0};
    capture_history(fixture, &snapshot);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real note switching window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_other_unchanged(fixture);
    reselect_unsaved(window, fixture, &snapshot);
    switch_and_return(window, fixture, &snapshot);
    require(controlled.requests == 0, "plain note switching does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(snapshot.files[version]);
}

struct native_body_snapshot
{
    wchar_t text[128];
    CHARRANGE selection;
    LRESULT modified;
    LRESULT undo;
    LRESULT redo;
    HWND focus;
};

static void capture_native_body(struct folio_window *_Nonnull window,
                                struct native_body_snapshot *_Nonnull snapshot)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    GETTEXTEX request = {.cb = sizeof snapshot->text, .flags = GT_RAWTEXT, .codepage = 1200};
    LRESULT length = SendMessageW(body, EM_GETTEXTEX, (WPARAM)&request, (LPARAM)snapshot->text);
    require(length > 0 && (size_t)length < sizeof snapshot->text / sizeof snapshot->text[0] - 1,
            "short native fixture fits its native text snapshot");
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&snapshot->selection);
    snapshot->modified = SendMessageW(body, EM_GETMODIFY, 0, 0);
    snapshot->undo = SendMessageW(body, EM_CANUNDO, 0, 0);
    snapshot->redo = SendMessageW(body, EM_CANREDO, 0, 0);
    snapshot->focus = GetFocus();
}

static void expect_native_body(struct folio_window *_Nonnull window,
                               const struct native_body_snapshot *_Nonnull snapshot)
{
    struct native_body_snapshot after = {0};
    capture_native_body(window, &after);
    require(wcscmp(after.text, snapshot->text) == 0,
            "surface operation preserves all native text units");
    require(after.selection.cpMin == snapshot->selection.cpMin &&
                after.selection.cpMax == snapshot->selection.cpMax,
            "surface operation preserves the native selection");
    require(after.modified == snapshot->modified && after.undo == snapshot->undo &&
                after.redo == snapshot->redo && after.focus == snapshot->focus,
            "surface operation preserves native dirty, Undo, Redo and focus");
}

static void expect_zoom(struct folio_window *_Nonnull window, int percent)
{
    int numerator = 0;
    int denominator = 0;
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_GETZOOM, (WPARAM)&numerator, (LPARAM)&denominator) != 0,
            "native zoom is readable");
    require(denominator != 0 ? numerator * 100 == percent * denominator
                             : numerator == 0 && percent == 100,
            "native zoom matches the expected percentage");
}

static void zoom_wheel(struct folio_window *_Nonnull window, int delta, int percent)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    RECT bounds;
    require(GetWindowRect(body, &bounds) && !IsRectEmpty(&bounds), "zoom body has native bounds");
    LPARAM position = MAKELPARAM(bounds.left + 8, bounds.top + 8);
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_MOUSEWHEEL,
                 MAKEWPARAM(MK_CONTROL, (WORD)delta), position);
    pump(window);
    expect_zoom(window, percent);
}

static void zoom_edit_cycle(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture,
                            const struct history_snapshot *_Nonnull history)
{
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    const int deltas[] = {60, 60, -120, 3600, 120, -3600, -120, 1200};
    const int percentages[] = {150, 160, 150, 300, 300, 50, 50, 150};
    for (size_t index = 0; index < sizeof deltas / sizeof deltas[0]; ++index)
    {
        zoom_wheel(window, deltas[index], percentages[index]);
        expect_native_body(window, &snapshot);
    }
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, history);
}

static void zoom_redo(struct folio_window *_Nonnull window, struct native_fixture *_Nonnull fixture,
                      const struct history_snapshot *_Nonnull history)
{
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "Undo exposes one redoable insertion");
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    zoom_wheel(window, WHEEL_DELTA, 160);
    expect_native_body(window, &snapshot);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "native Redo succeeds after zoom");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, history);
}

static void run_zoom_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated zoom note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real zoom window");
    pump(window);
    expect_zoom(window, 100);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 2, .cpMax = 4};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    struct native_body_snapshot view = {0};
    capture_native_body(window, &view);
    zoom_wheel(window, 5 * WHEEL_DELTA, 150);
    expect_native_body(window, &view);
    verify_history_unchanged(fixture, &history);
    action(window, ACCESSIBLE_EDIT);
    expect_zoom(window, 150);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    zoom_edit_cycle(window, fixture, &history);
    zoom_redo(window, fixture, &history);
    action(window, ACCESSIBLE_VIEW);
    expect_zoom(window, 160);
    struct native_body_snapshot returned = {0};
    capture_native_body(window, &returned);
    require(wcscmp(view.text, returned.text) == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "mode roundtrip preserves the rendered view and readonly mode");
    verify_history_unchanged(fixture, &history);
    require(controlled.requests == 0, "plain zoom does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

struct theme_snapshot
{
    enum folio_theme_choice choice;
    enum folio_theme theme;
    struct theme_palette palette;
    struct file_bytes *_Nonnull settings;
};

static struct file_bytes *_Nonnull read_theme_settings(struct native_fixture *_Nonnull fixture)
{
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    const wchar_t suffix[] = L"\\settings.json";
    require(storage_native_path_fill(storage_state_directory(fixture->location), path, &length) &&
                length + sizeof suffix / sizeof suffix[0] <= storage_native_capacity,
            "dedicated settings path fits the native capacity");
    memcpy(path + length, suffix, sizeof suffix);
    struct file_bytes *bytes = nullptr;
    require(file_bytes_read(path, &bytes) == PERSISTENCE_LOADED && bytes != nullptr,
            "read the dedicated settings bytes");
    return bytes;
}

static void expect_theme_settings(struct native_fixture *_Nonnull fixture,
                                  const struct theme_snapshot *_Nonnull snapshot)
{
    require(folio_state_theme_choice(fixture->state) == snapshot->choice,
            "preview and cancellation preserve the saved theme choice");
    struct file_bytes *after = read_theme_settings(fixture);
    require(file_bytes_length(after) == file_bytes_length(snapshot->settings) &&
                memcmp(file_bytes_data(after), file_bytes_data(snapshot->settings),
                       file_bytes_length(after)) == 0,
            "preview and cancellation preserve all settings bytes");
    file_bytes_destroy(after);
}

static struct accessible_item theme_choice_row(struct folio_window *_Nonnull window)
{
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item row;
    require(folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &row) ==
                    S_OK &&
                row.kind == ACCESSIBLE_SURFACE_CHOICE,
            "filtered theme exposes its public choice row");
    return row;
}

static void preview_theme_candidate(struct folio_window *_Nonnull window,
                                    struct folio_state *_Nonnull state,
                                    enum folio_theme_choice saved)
{
    const bool use_nord = saved != FOLIO_THEME_CHOICE_NORD;
    const wchar_t *name = use_nord ? L"nord" : L"dracula";
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr && SetWindowTextW(input, name), "filter a normal theme candidate");
    pump(window);
    struct accessible_item row = theme_choice_row(window);
    struct accessible_view view;
    require(folio_window_accessible_read(window, &row, &view) == S_OK &&
                wcscmp((const wchar_t *)view.name, name) == 0,
            "the filtered row is the intended theme");
    struct accessible_request request = {.item = row, .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "Select previews the current theme row without invoking it");
    pump(window);
    require(folio_state_theme(state) == (use_nord ? FOLIO_THEME_NORD : FOLIO_THEME_DRACULA),
            "the display theme resolves to the preview candidate");
}

static void expect_theme_restored(struct native_fixture *_Nonnull fixture,
                                  const struct theme_snapshot *_Nonnull snapshot)
{
    expect_theme_settings(fixture, snapshot);
    require(folio_state_theme(fixture->state) == snapshot->theme &&
                theme_palette_equal(folio_state_palette(fixture->state), &snapshot->palette),
            "cancellation restores the initial effective palette and theme");
}

static void theme_preview_cycle(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct theme_snapshot *_Nonnull snapshot, bool go_back)
{
    struct native_body_snapshot closed = {0};
    capture_native_body(window, &closed);
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "invoke the public settings action");
    pump(window);
    surface_button(window, ACCESSIBLE_SURFACE_THEME);
    struct native_body_snapshot opened = {0};
    capture_native_body(window, &opened);
    preview_theme_candidate(window, fixture->state, snapshot->choice);
    expect_native_body(window, &opened);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_theme_settings(fixture, snapshot);
    if (go_back)
    {
        surface_button(window, ACCESSIBLE_SURFACE_BACK);
        expect_theme_restored(fixture, snapshot);
        opened.focus = GetFocus(); /* Returning to settings legitimately changes surface focus. */
        expect_native_body(window, &opened);
    }
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_native_body(window, &closed);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_theme_restored(fixture, snapshot);
}

static void expect_body_saved(struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull history,
                              const char *_Nonnull expected)
{
    require(folio_state_store_revision(fixture->state) == history->store_revision + 1,
            "native body save publishes exactly one revision");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *saved = nullptr;
    require(note_text_create(expected, strlen(expected), &saved) == NOTE_TEXT_ACCEPTED,
            "valid body save expectation");
    expect_history_file(disk, 0, saved);
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_history_file(disk, version, history->files[version - 1]);
    note_text_destroy(saved);
}

static void save_theme_input(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct theme_snapshot *_Nonnull theme,
                             const struct history_snapshot *_Nonnull history)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    SendMessageW(input, EM_SETSEL, 1, 3);
    struct native_body_snapshot opened = {0};
    capture_native_body(window, &opened);
    const enum folio_theme preview = folio_state_theme(fixture->state);
    const struct theme_palette palette = *folio_state_palette(fixture->state);
    const wchar_t *query = theme->choice != FOLIO_THEME_CHOICE_NORD ? L"nord" : L"dracula";
    for (size_t attempt = 0; attempt < 2; ++attempt)
    {
        SendMessageW(input, WM_CHAR, 0x13, 1); /* ADR0016: Ctrl+S in the native input. */
        pump(window);
        wchar_t text[16];
        DWORD start = 0;
        DWORD end = 0;
        SendMessageW(input, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        require(IsWindowVisible(input) && GetWindowTextW(input, text, 16) > 0 &&
                    wcscmp(text, query) == 0 && start == 1 && end == 3 && GetFocus() == input,
                "body save retains the visible theme query, selection and input focus");
        expect_native_body(window, &opened);
        expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
        expect_theme_settings(fixture, theme);
        require(folio_state_theme(fixture->state) == preview &&
                    theme_palette_equal(folio_state_palette(fixture->state), &palette),
                "body save retains the uncommitted theme preview");
        expect_body_saved(fixture, history, switch_saved);
    }
}

static void theme_save_cycle(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct theme_snapshot *_Nonnull theme,
                             const struct history_snapshot *_Nonnull history)
{
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    struct native_body_snapshot closed = {0};
    capture_native_body(window, &closed);
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "invoke settings before saving during a preview");
    pump(window);
    surface_button(window, ACCESSIBLE_SURFACE_THEME);
    preview_theme_candidate(window, fixture->state, theme->choice);
    save_theme_input(window, fixture, theme, history);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_native_body(window, &closed);
    expect_theme_restored(fixture, theme);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_body_saved(fixture, history, switch_saved);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "native Redo succeeds after body-only save");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_body_saved(fixture, history, switch_saved);
    expect_theme_restored(fixture, theme);
}

static struct folio_settings *_Nonnull saved_theme_settings(struct native_fixture *_Nonnull fixture)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct folio_settings *settings = nullptr;
    require(disk.read_settings(disk.adapter, &settings) == PERSISTENCE_LOADED &&
                settings != nullptr,
            "load the dedicated saved settings through the real adapter");
    return settings;
}

static void expect_committed_theme(struct native_fixture *_Nonnull fixture,
                                   const struct folio_settings *_Nonnull before,
                                   enum folio_theme_choice expected)
{
    struct folio_settings *after = saved_theme_settings(fixture);
    require(folio_settings_theme(after) == expected &&
                folio_state_theme_choice(fixture->state) == expected,
            "Invoke commits the expected theme to state and disk");
    require(folio_settings_number(after) == folio_settings_number(before) &&
                folio_settings_minimap(after) == folio_settings_minimap(before) &&
                folio_settings_language(after) == folio_settings_language(before),
            "theme commit retains every other saved setting");
    folio_settings_destroy(after);
}

static void invoke_theme_candidate(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture,
                                   const struct folio_settings *_Nonnull before,
                                   enum folio_theme_choice expected)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    SendMessageW(input, EM_SETSEL, 1, 3);
    struct native_body_snapshot opened = {0};
    capture_native_body(window, &opened);
    struct accessible_request request = {.item = theme_choice_row(window),
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "Invoke commits the current public theme candidate");
    pump(window);
    wchar_t query[16];
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(input, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    const wchar_t *name = expected == FOLIO_THEME_CHOICE_NORD ? L"nord" : L"dracula";
    require(IsWindowVisible(input) && GetWindowTextW(input, query, 16) > 0 &&
                wcscmp(query, name) == 0 && start == 1 && end == 3 && GetFocus() == input,
            "theme commit keeps the open query, selection and focus");
    struct accessible_item row = theme_choice_row(window);
    struct accessible_view view;
    require(folio_window_accessible_read(window, &row, &view) == S_OK && view.selected,
            "the committed theme row exposes its saved selection mark");
    expect_native_body(window, &opened);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_committed_theme(fixture, before, expected);
}

static void theme_commit_cycle(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_settings *before = saved_theme_settings(fixture);
    enum folio_theme_choice expected = folio_settings_theme(before) != FOLIO_THEME_CHOICE_NORD
                                           ? FOLIO_THEME_CHOICE_NORD
                                           : FOLIO_THEME_CHOICE_DRACULA;
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    struct native_body_snapshot closed = {0};
    capture_native_body(window, &closed);
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "open settings with an unsaved native edit before theme commit");
    pump(window);
    surface_button(window, ACCESSIBLE_SURFACE_THEME);
    preview_theme_candidate(window, fixture->state, folio_settings_theme(before));
    invoke_theme_candidate(window, fixture, before, expected);
    verify_history_unchanged(fixture, &history);
    struct theme_snapshot committed = {.choice = expected,
                                       .theme = folio_state_theme(fixture->state),
                                       .palette = *folio_state_palette(fixture->state),
                                       .settings = read_theme_settings(fixture)};
    preview_theme_candidate(window, fixture->state, expected);
    expect_theme_settings(fixture, &committed);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_native_body(window, &closed);
    expect_theme_restored(fixture, &committed);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo succeeds after theme commit and cancel");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &history);
    expect_theme_restored(fixture, &committed);
    file_bytes_destroy(committed.settings);
    folio_settings_destroy(before);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void close_theme_window(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(root != nullptr && IsWindow(root), "normal close starts with a live root window");
    SendMessageW(root, WM_CLOSE, 0, 0);
    MSG message;
    for (unsigned count = 0;
         count < 1000 && IsWindow(root) && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE);
         ++count)
    {
        require(message.message != WM_QUIT, "quit is posted only after the root is destroyed");
        if (message.message == WM_TIMER)
            continue;
        if (!folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    require(!IsWindow(root) && folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == nullptr,
            "normal close completes deferred saving and finalizes the native root");
    require(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) &&
                message.message == WM_QUIT && message.wParam == 0,
            "normal close publishes successful quit for the composition root");
}

static void theme_close_cycle(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    struct theme_snapshot saved = {.choice = folio_state_theme_choice(fixture->state),
                                   .settings = read_theme_settings(fixture)};
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "open settings with an unsaved body before normal close");
    pump(window);
    surface_button(window, ACCESSIBLE_SURFACE_THEME);
    preview_theme_candidate(window, fixture->state, saved.choice);
    expect_theme_settings(fixture, &saved);
    verify_history_unchanged(fixture, &history);
    close_theme_window(window);
    expect_body_saved(fixture, &history, history_current);
    expect_theme_settings(fixture, &saved);
    file_bytes_destroy(saved.settings);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #661/#665/#671/#677: Theme operations retain edits; normal close saves only the body. */
static void run_theme_preview_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated theme preview note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real theme preview window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    struct theme_snapshot snapshot = {.choice = folio_state_theme_choice(fixture->state),
                                      .theme = folio_state_theme(fixture->state),
                                      .palette = *folio_state_palette(fixture->state),
                                      .settings = read_theme_settings(fixture)};
    theme_preview_cycle(window, fixture, &snapshot, false);
    verify_history_unchanged(fixture, &history);
    theme_preview_cycle(window, fixture, &snapshot, true);
    verify_history_unchanged(fixture, &history);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    expect_theme_restored(fixture, &snapshot);
    theme_save_cycle(window, fixture, &snapshot, &history);
    theme_commit_cycle(window, fixture);
    theme_close_cycle(window, fixture);
    require(controlled.requests == 0, "plain theme preview does not request image work");
    file_bytes_destroy(snapshot.settings);
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

struct settings_edit_values
{
    bool number;
    bool minimap;
    enum folio_language language;
    enum folio_theme_choice theme;
};

static void select_setting_control(struct folio_window *_Nonnull window, size_t ordinal,
                                   enum accessible_kind kind)
{
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item row;
    require(folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &row) ==
                S_OK,
            "settings exposes its first current row");
    for (size_t index = 0; index < ordinal; ++index)
    {
        struct accessible_item next;
        require(folio_window_accessible_navigate(window, &row, ACCESSIBLE_NAVIGATION_NEXT, &next) ==
                    S_OK,
                "settings exposes the requested current row");
        row = next;
    }
    require(row.kind == kind, "the requested setting has its expected public role");
    struct accessible_request request = {.item = row, .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "Select applies the current setting through the public boundary");
    pump(window);
}

static void expect_settings_values(struct native_fixture *_Nonnull fixture,
                                   const struct settings_edit_values *_Nonnull expected)
{
    struct folio_settings *saved = saved_theme_settings(fixture);
    require(folio_settings_number(saved) == expected->number &&
                folio_state_number(fixture->state) == expected->number &&
                folio_settings_minimap(saved) == expected->minimap &&
                folio_state_minimap(fixture->state) == expected->minimap,
            "saved and active booleans match the intended setting changes");
    require(folio_settings_language(saved) == expected->language &&
                folio_state_language(fixture->state) == expected->language &&
                folio_settings_theme(saved) == expected->theme &&
                folio_state_theme_choice(fixture->state) == expected->theme,
            "saved language matches its selection while theme remains unchanged");
    folio_settings_destroy(saved);
}

static void settings_edit_preserved(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct history_snapshot *_Nonnull history,
                                    const struct native_body_snapshot *_Nonnull body)
{
    struct accessible_item root = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &root, &view) == S_OK && !view.offscreen,
            "setting selection keeps the settings surface open");
    expect_native_body(window, body);
    verify_history_unchanged(fixture, history);
}

static void settings_edit_cycle(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct history_snapshot *_Nonnull history)
{
    struct settings_edit_values values = {.number = folio_state_number(fixture->state),
                                          .minimap = folio_state_minimap(fixture->state),
                                          .language = folio_state_language(fixture->state),
                                          .theme = folio_state_theme_choice(fixture->state)};
    struct native_body_snapshot closed = {0};
    capture_native_body(window, &closed);
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "open settings while retaining the native editing state");
    pump(window);
    struct native_body_snapshot opened = {0};
    capture_native_body(window, &opened);
    expect_settings_values(fixture, &values);
    for (size_t step = 0; step < 4; ++step)
    {
        select_setting_control(window, 6 + step % 2, ACCESSIBLE_SURFACE_TOGGLE);
        if (step % 2 == 0)
            values.number = !values.number;
        else
            values.minimap = !values.minimap;
        expect_settings_values(fixture, &values);
        settings_edit_preserved(window, fixture, history, &opened);
    }
    const enum folio_language languages[] = {FOLIO_LANGUAGE_EN, FOLIO_LANGUAGE_ZH_HANS,
                                             FOLIO_LANGUAGE_JA};
    for (size_t index = 0; index < sizeof languages / sizeof languages[0]; ++index)
    {
        select_setting_control(window, 3 + (size_t)languages[index], ACCESSIBLE_SURFACE_LANGUAGE);
        values.language = languages[index];
        expect_settings_values(fixture, &values);
        settings_edit_preserved(window, fixture, history, &opened);
    }
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_native_body(window, &closed);
    verify_history_unchanged(fixture, history);
}

/* #679: Setting changes preserve unsaved edits and a redoable native insertion. */
static void run_settings_edit_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated settings edit note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real settings edit window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    settings_edit_cycle(window, fixture, &history);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "Undo leaves a redoable insertion before settings");
    settings_edit_cycle(window, fixture, &history);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo succeeds after changing all settings");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    require(controlled.requests == 0, "plain setting changes do not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void reselect_edit_mode(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture,
                               const struct history_snapshot *_Nonnull history,
                               enum folio_note_change change)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        action(window, ACCESSIBLE_EDIT);
        expect_native_body(window, &snapshot);
        expect_editor(window, fixture->state, snapshot.text, change);
        verify_history_unchanged(fixture, history);
    }
}

static void expect_saved_view(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull history)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(wcscmp(snapshot.text, L"Current 日本語 😀 saved 追記 🙂") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "saved view renders the expected plain text and remains readonly");
    expect_body_saved(fixture, history, switch_saved);
}

static void saved_mode_roundtrip(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history)
{
    action(window, ACCESSIBLE_VIEW);
    expect_saved_view(window, fixture, history);
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        action(window, ACCESSIBLE_VIEW);
        expect_native_body(window, &snapshot);
        expect_saved_view(window, fixture, history);
    }
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_body_saved(fixture, history, switch_saved);
    action(window, ACCESSIBLE_VIEW);
    expect_saved_view(window, fixture, history);
}

/* #685: Same-mode selection keeps edits; leaving a changed edit saves one revision. */
static void run_mode_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated mode save note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real mode save window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    reselect_edit_mode(window, fixture, &history, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "Undo leaves a redoable insertion before same-mode selection");
    reselect_edit_mode(window, fixture, &history, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo succeeds after same-mode selection");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    saved_mode_roundtrip(window, fixture, &history);
    require(controlled.requests == 0, "plain mode selection does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void run_other_fixture(struct native_fixture *_Nonnull fixture, int argc,
                              char *_Nonnull *_Nonnull argv)
{
    if (mode_save_case)
    {
        run_mode_save_case(fixture);
        return;
    }
    if (settings_edit_case)
    {
        run_settings_edit_case(fixture);
        return;
    }
    if (argc == 2)
        run_status_case(fixture, argv[1]);
    else
    {
        require(argc == 1, "valid argument count");
        run_image_case(fixture);
    }
}

static void run_fixture(struct native_fixture *_Nonnull fixture, int argc,
                        char *_Nonnull *_Nonnull argv)
{
    if (diagram_case)
        run_diagram_case(fixture);
    else if (history_case)
        run_history_case(fixture);
    else if (replace_case)
        run_replace_case(fixture);
    else if (search_case)
        run_search_case(fixture);
    else if (note_switch_case)
        run_note_switch_case(fixture);
    else if (zoom_case)
        run_zoom_case(fixture);
    else if (theme_preview_case)
        run_theme_preview_case(fixture);
    else if (argc == 2 && strcmp(argv[1], "--icon") == 0)
        window_icons(fixture);
    else
        run_other_fixture(fixture, argc, argv);
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
    replace_case = argc == 2 && strcmp(argv[1], "--replace") == 0;
    search_case = argc == 2 && strcmp(argv[1], "--search") == 0;
    note_switch_case = argc == 2 && strcmp(argv[1], "--note-switch") == 0;
    zoom_case = argc == 2 && strcmp(argv[1], "--zoom") == 0;
    theme_preview_case = argc == 2 && strcmp(argv[1], "--theme-preview") == 0;
    settings_edit_case = argc == 2 && strcmp(argv[1], "--settings-edit") == 0;
    mode_save_case = argc == 2 && strcmp(argv[1], "--mode-save") == 0;
    struct native_fixture fixture = {0};
    create_fixture(&fixture);
    run_fixture(&fixture, argc, argv);
    destroy_fixture(&fixture);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("image/status: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
