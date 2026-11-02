/* #520: body-first loading, processing notice and mode cancellation through the real window. */
#include "accessible_request.h"
#include "accessible_view.h"
#include "appearance_adapter.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
#include "image_port.h"
#include "note_ledger.h"
#include "note_text.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "storage_adapter.h"
#include "storage_state.h"
#include "transfer_adapter.h"
#include "web_link_adapter.h"
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

static void require(bool condition, const char *_Nonnull message)
{
    ++checks;
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void seed_notes(struct persistence_port disk)
{
    enum persistence_outcome created = disk.create_category(disk.adapter, "schedule");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "test category");
    created = disk.create_category(disk.adapter, "target");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "move category");
    const char source[] = "Body before images\n\n![alt](outside.png)\n\nBody after images\n";
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(source, sizeof source - 1, &body) == NOTE_TEXT_ACCEPTED, "image note");
    require(disk.write_note(disk.adapter, "schedule", "one", body) == PERSISTENCE_STORED,
            "image note stored");
    note_text_destroy(body);
}
struct image_adapter
{
    uint64_t ticket;
    size_t requests;
    enum image_phase phase;
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
    return IMAGE_PENDING;
}
static enum image_outcome poll_image(struct image_adapter *_Nonnull adapter, uint64_t ticket,
                                     struct image_bitmap *_Nullable *_Nonnull out)
{
    (void)out;
    return ticket == adapter->ticket ? IMAGE_PENDING : IMAGE_CANCELLED;
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
    require(wcsstr(text, L"![alt](outside.png)") != nullptr, "edit contains original path");
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
int main(void)
{
    HDESK desktop = CreateDesktopW(L"NeNeFolioImageStatus520", nullptr, nullptr, 0,
                                   DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_READOBJECTS |
                                       DESKTOP_WRITEOBJECTS,
                                   nullptr);
    require(desktop != nullptr && SetThreadDesktop(desktop), "isolated desktop");
    require(SUCCEEDED(OleInitialize(nullptr)), "COM apartment");
    struct native_fixture fixture = {0};
    create_fixture(&fixture);
    struct folio_window *_Nullable window = nullptr;
    require(folio_window_create(fixture.state, fixture.location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real main window");
    body_first(window);
    move_origin(window, fixture.state);
    folio_window_destroy(window);
    destroy_fixture(&fixture);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("image status: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
