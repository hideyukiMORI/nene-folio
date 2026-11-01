/* #510: public window/idle boundaries; no real clock or delivered timer is needed. */
#include "accessible_request.h"
#include "appearance_adapter.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
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
    const char *names[] = {"00_short", "01_long"};
    const size_t lines[] = {8, 1000};
    char source[3001];
    for (size_t index = 0; index < 2; ++index)
    {
        for (size_t line = 0; line < lines[index]; ++line)
        {
            source[line * 3] = 'x';
            source[line * 3 + 1] = '\n';
            source[line * 3 + 2] = '\n';
        }
        struct note_text *_Nullable body = nullptr;
        require(note_text_create(source, lines[index] * 3, &body) == NOTE_TEXT_ACCEPTED,
                "fixture text");
        require(disk.write_note(disk.adapter, "schedule", names[index], body) == PERSISTENCE_STORED,
                "fixture note");
        note_text_destroy(body);
    }
    /* Native directory enumeration is unordered; select slots from an explicit fixture ledger. */
    const char order[] = "{\"version\":1,\"notes\":[\"00_short\",\"01_long\"]}";
    struct note_ledger *_Nullable ledger = nullptr;
    require(note_ledger_parse(order, sizeof order - 1, &ledger) == NOTE_LEDGER_ACCEPTED,
            "fixture note order");
    require(disk.write_note_ledger(disk.adapter, "schedule", ledger) == PERSISTENCE_STORED,
            "explicit fixture note order");
    note_ledger_destroy(ledger);
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
    struct folio_ports ports = {.persistence = &disk,
                                .appearance = &appearance,
                                .regex = &regex,
                                .web_link = &web,
                                .transfer = &transfer};
    require(folio_state_create(&ports, &fixture->state) == FOLIO_STATE_READY, "application state");
    require(folio_state_set_minimap(fixture->state, true) == FOLIO_STATE_READY, "minimap enabled");
    require(folio_state_set_number(fixture->state, true) == FOLIO_STATE_READY,
            "line numbers enabled");
    require(folio_state_set_expansion(fixture->state, 0, true) == FOLIO_STATE_READY,
            "expanded notes");
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
}

static void destroy_fixture(struct native_fixture *_Nonnull fixture)
{
    folio_state_destroy(fixture->state);
    font_bundle_destroy(fixture->fonts);
    regex_adapter_destroy(fixture->regex);
    appearance_adapter_destroy(fixture->appearance);
    persistence_adapter_destroy(fixture->files);
    storage_state_destroy(fixture->location);
    storage_adapter_destroy(fixture->storage);
}

static WNDPROC original_body;
static bool in_slice;
static bool fail_sample;
static size_t sampled;

static LRESULT CALLBACK observe_body(HWND handle, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (in_slice && message == EM_LINEINDEX)
    {
        ++sampled;
        if (fail_sample)
            return -1;
    }
    return CallWindowProcW(original_body, handle, message, wparam, lparam);
}

/* Deliberately discard timers: immediate work must not depend on wall-clock timing. */
static void pump_without_timers(struct folio_window *_Nonnull window)
{
    MSG message;
    for (unsigned count = 0; count < 1000; ++count)
    {
        if (!PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            return;
        if (message.message != WM_QUIT && message.message != WM_TIMER &&
            !folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    require(false, "finite message queue");
}

static void action(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_request request = {.item = {.kind = kind},
                                         .action = ACCESSIBLE_ACTION_SELECT};
    HRESULT result = folio_window_accessible_action(window, &request);
    if (result != S_OK)
        fprintf(stderr, "action kind=%u result=%lx\n", (unsigned)kind, (unsigned long)result);
    require(result == S_OK, "public action");
    pump_without_timers(window);
}

static void select_note(struct folio_window *_Nonnull window, size_t note)
{
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_NOTE,
                 .ref = {.category = 0, .note = note},
                 .revision = folio_window_accessible_revision(window)},
        .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK, "select note through UI");
    if (!folio_window_idle_work_ready(window))
        fprintf(stderr, "selected note %zu has no pending idle work\n", note);
    require(folio_window_idle_work_ready(window), "document ready without a timer");
    pump_without_timers(window);
    require(folio_window_idle_work_ready(window), "paint/inset keeps immediate work ready");
}

static void layout_key(struct folio_window *_Nonnull window, HWND root, WPARAM key)
{
    BYTE original[256];
    BYTE keys[256] = {0};
    require(GetKeyboardState(original) != 0, "thread keyboard state");
    keys[VK_CONTROL] = 0x80;
    keys[VK_SHIFT] = 0x80;
    require(SetKeyboardState(keys) != 0, "layout shortcut modifiers");
    MSG message = {.hwnd = root, .message = WM_KEYDOWN, .wParam = key};
    bool handled = folio_window_translate(window, &message);
    require(SetKeyboardState(original) != 0 && handled, "registered layout shortcut");
    pump_without_timers(window);
}

static size_t step(struct folio_window *_Nonnull window)
{
    sampled = 0;
    in_slice = true;
    folio_window_continue_idle_work(window);
    in_slice = false;
    return sampled;
}

static void finish(struct folio_window *_Nonnull window, HWND body)
{
    LRESULT lines = SendMessageW(body, EM_GETLINECOUNT, 0, 0);
    size_t expected = lines > 256 ? 256 : (size_t)lines;
    size_t total = 0;
    size_t slices = 0;
    while (folio_window_idle_work_ready(window) && slices < 17)
    {
        size_t count = step(window);
        require(count > 0 && count <= 16, "each idle step reads at most sixteen lines");
        total += count;
        ++slices;
        pump_without_timers(window);
    }
    require(!folio_window_idle_work_ready(window), "bounded work completes");
    require(total == expected && slices == (expected + 15) / 16, "one complete bounded snapshot");
}

static void verify_switches(struct folio_window *_Nonnull window, HWND body)
{
    select_note(window, 0);
    finish(window, body);
    select_note(window, 1);
    require(SendMessageW(body, EM_GETLINECOUNT, 0, 0) >= 256,
            "fixture has at least 256 native display lines without relying on wrapping");
    size_t first = step(window);
    if (first != 16)
        fprintf(stderr, "long document lines=%lld samples=%zu\n",
                (long long)SendMessageW(body, EM_GETLINECOUNT, 0, 0), first);
    require(first == 16, "long document begins in one slice");
    select_note(window, 0);
    finish(window, body);
    select_note(window, 1);
    fail_sample = true;
    require(step(window) == 1 && !folio_window_idle_work_ready(window),
            "native sample failure ends work");
    fail_sample = false;
    select_note(window, 0);
    finish(window, body);
    select_note(window, 1);
    finish(window, body);
}

static void verify_shared_idle(struct folio_window *_Nonnull window,
                               struct folio_state *_Nonnull state, HWND body)
{
    select_note(window, 0);
    finish(window, body);
    select_note(window, 1);
    HWND filter = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    require(filter != nullptr && SetWindowTextW(filter, L"x"), "start real index input");
    pump_without_timers(window);
    require(folio_state_index_filter_pending(state), "index work is pending with minimap");
    size_t total = 0;
    size_t turns = 0;
    bool map_before_filter = false;
    bool filter_before_map = false;
    while (folio_window_idle_work_ready(window) && turns < 32)
    {
        bool pending = folio_state_index_filter_pending(state);
        size_t count = step(window);
        require(count <= 16, "shared idle retains sample bound");
        total += count;
        map_before_filter = map_before_filter || (pending && count > 0);
        filter_before_map =
            filter_before_map || (!folio_state_index_filter_pending(state) && total < 256);
        ++turns;
        pump_without_timers(window);
    }
    require(map_before_filter, "map progresses before pending index completes");
    require(filter_before_map, "index completes before the long map completes");
    require(total == 256 && !folio_window_idle_work_ready(window),
            "both shared tasks finish with one bounded snapshot");
    require(SetWindowTextW(filter, L""), "clear completed filter");
    pump_without_timers(window);
    select_note(window, 0);
    finish(window, body);
}

static void verify_idle_cancel(struct folio_window *_Nonnull window,
                               struct folio_state *_Nonnull state, HWND root, HWND body)
{
    HWND filter = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    select_note(window, 1);
    require(SetWindowTextW(filter, L"x") && folio_state_index_filter_pending(state),
            "start another index request");
    require(SetWindowTextW(filter, L"") && !folio_state_index_filter_pending(state),
            "cancel index without cancelling map");
    finish(window, body);
    require(SetWindowTextW(filter, L"x") && folio_state_index_filter_pending(state),
            "index pending before hiding body");
    layout_key(window, root, '1');
    size_t turns = 0;
    while (folio_window_idle_work_ready(window) && turns++ < 10)
        require(step(window) == 0, "hidden minimap does not consume index turns");
    require(!folio_state_index_filter_pending(state) && !folio_window_idle_work_ready(window),
            "index finishes while the minimap is hidden");
    require(SetWindowTextW(filter, L""), "clear hidden filter");
    layout_key(window, root, '2');
    select_note(window, 0);
    finish(window, body);
    select_note(window, 1);
    finish(window, body);
}

static void verify_guards(struct folio_window *_Nonnull window, HWND root, HWND body)
{
    select_note(window, 0);
    SendMessageW(root, WM_ENTERMENULOOP, 0, 0);
    require(!folio_window_idle_work_ready(window) && step(window) == 0, "native menu pauses work");
    SendMessageW(root, WM_EXITMENULOOP, 0, 0);
    require(folio_window_idle_work_ready(window), "menu exit resumes pending work");
    EnableWindow(root, FALSE);
    require(!folio_window_idle_work_ready(window), "modal owner pauses work");
    EnableWindow(root, TRUE);
    require(folio_window_idle_work_ready(window), "enabled owner resumes work");
    SendMessageW(body, WM_IME_STARTCOMPOSITION, 0, 0);
    require(!folio_window_idle_work_ready(window) && step(window) == 0, "IME pauses sampling");
    SendMessageW(body, WM_IME_ENDCOMPOSITION, 0, 0);
    pump_without_timers(window);
    require(folio_window_idle_work_ready(window), "IME exit resumes pending work");
    finish(window, body);
    layout_key(window, root, '1');
    require(!folio_window_idle_work_ready(window), "hidden pane has no work");
    layout_key(window, root, '2');
    require(!folio_window_idle_work_ready(window), "layout restore retains settle delay");
    select_note(window, 1);
    finish(window, body);
}

static void verify_edit(struct folio_window *_Nonnull window, HWND root, HWND body)
{
    action(window, ACCESSIBLE_EDIT);
    require(folio_window_idle_work_ready(window), "edit document is immediately ready");
    CHARRANGE selected = {.cpMin = 2, .cpMax = 4};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    wchar_t before[4096], after[4096];
    int length = GetWindowTextW(body, before, 4096);
    LRESULT dirty = SendMessageW(body, EM_GETMODIFY, 0, 0);
    LRESULT undo = SendMessageW(body, EM_CANUNDO, 0, 0);
    HWND focus = GetFocus();
    finish(window, body);
    CHARRANGE actual = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&actual);
    require(GetWindowTextW(body, after, 4096) == length && wcscmp(before, after) == 0,
            "text unchanged");
    require(actual.cpMin == selected.cpMin && actual.cpMax == selected.cpMax,
            "selection unchanged");
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) == dirty &&
                SendMessageW(body, EM_CANUNDO, 0, 0) == undo && GetFocus() == focus,
            "dirty undo and focus unchanged");
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"z");
    pump_without_timers(window);
    require(!folio_window_idle_work_ready(window) && step(window) == 0,
            "typing retains input delay");
    require(SendMessageW(body, EM_UNDO, 0, 0) != 0, "native undo works");
    pump_without_timers(window);
    require(!folio_window_idle_work_ready(window), "second change restarts input delay");
    select_note(window, 0);
    finish(window, body);
    select_note(window, 1);
    finish(window, body);
    SetWindowPos(root, nullptr, 0, 0, 1000, 750, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump_without_timers(window);
    require(!folio_window_idle_work_ready(window), "normal resize retains settle delay");
    action(window, ACCESSIBLE_VIEW);
    require(folio_window_idle_work_ready(window), "view load cancels previous timer delay");
    finish(window, body);
}

int main(void)
{
    HDESK desktop = CreateDesktopW(L"NeNeFolioMinimapScheduling", nullptr, nullptr, 0,
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
            "real window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(root != nullptr && body != nullptr, "native windows");
    SetWindowPos(root, nullptr, 20, 20, 1200, 900, SWP_NOZORDER | SWP_NOACTIVATE);
    pump_without_timers(window);
    original_body = (WNDPROC)SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)observe_body);
    require(original_body != nullptr, "observe actual sample queries");
    verify_switches(window, body);
    verify_shared_idle(window, fixture.state, body);
    verify_idle_cancel(window, fixture.state, root, body);
    verify_guards(window, root, body);
    verify_edit(window, root, body);
    SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)original_body);
    folio_window_destroy(window);
    destroy_fixture(&fixture);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("minimap scheduling: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
