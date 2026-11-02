/* #504: UIAの公開境界を実windowへ接続し、行の重複と循環を有限workで拒否する。 */
#include "accessible_request.h"
#include "accessible_surface_button.h"
#include "accessible_view.h"
#include "appearance_adapter.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
#include "image_adapter.h"
#include "persistence_adapter.h"
#include "regex_adapter.h"
#include "storage_adapter.h"
#include "storage_state.h"
#include "transfer_adapter.h"
#include "ui_text.h"
#include "utf16_text.h"
#include "web_link_adapter.h"

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
    struct appearance_port appearance = appearance_adapter_port(fixture->appearance);
    struct regex_port regex = regex_adapter_port(fixture->regex);
    struct web_link_port web = web_link_adapter_port();
    struct transfer_port transfer = transfer_adapter_port();
    require(image_adapter_create(storage_state_directory(fixture->location), &fixture->images) ==
                IMAGE_READY,
            "image adapter");
    struct image_port images = image_adapter_port(fixture->images);
    struct folio_ports ports = {.persistence = &disk,
                                .appearance = &appearance,
                                .regex = &regex,
                                .web_link = &web,
                                .transfer = &transfer,
                                .images = &images};
    require(folio_state_create(&ports, &fixture->state) == FOLIO_STATE_READY, "application state");
}

static void destroy_fixture(struct native_fixture *_Nonnull fixture)
{
    folio_state_destroy(fixture->state);
    image_adapter_destroy(fixture->images);
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
    for (unsigned count = 0; count < 1000; ++count)
    {
        if (!PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            return;
        if (message.message != WM_QUIT && !folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    require(false, "message queue terminates");
}

static void invoke(struct folio_window *_Nonnull window, struct accessible_item item)
{
    struct accessible_request request = {.item = item, .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "invoke public control");
    pump(window);
}

static const enum ui_text row_names[] = {
    UI_TEXT_SETTINGS_THEME,        UI_TEXT_THEME_CHOOSE,         UI_TEXT_SETTINGS_LANGUAGE,
    UI_TEXT_SETTINGS_LANGUAGE_JA,  UI_TEXT_SETTINGS_LANGUAGE_EN, UI_TEXT_SETTINGS_LANGUAGE_ZH_HANS,
    UI_TEXT_SETTINGS_NUMBER_CHECK, UI_TEXT_SETTINGS_MINIMAP,     UI_TEXT_STORAGE_ENTRY};

static void verify_row(struct folio_window *_Nonnull window, struct accessible_item item,
                       size_t ordinal, enum folio_language language)
{
    char16_t expected[ui_text_unit_limit];
    size_t written = 0;
    require(utf16_text_fill(ui_text_line(row_names[ordinal], language), expected,
                            ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY,
            "expected row name");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "row is readable");
    require(wcscmp(view.name, expected) == 0, "row name matches its visible setting");
    struct accessible_item parent;
    require(folio_window_accessible_navigate(window, &item, ACCESSIBLE_NAVIGATION_PARENT,
                                             &parent) == S_OK &&
                parent.kind == ACCESSIBLE_SURFACE_LIST,
            "row parent is the settings list");
    struct accessible_item child;
    require(folio_window_accessible_navigate(window, &item, ACCESSIBLE_NAVIGATION_FIRST, &child) ==
                S_FALSE,
            "setting row is a leaf");
}

static size_t walk(struct folio_window *_Nonnull window, struct accessible_item list,
                   struct accessible_item *_Nonnull rows, enum folio_language language)
{
    struct accessible_item item;
    HRESULT result =
        folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &item);
    size_t count = 0;
    while (result == S_OK && count < sizeof row_names / sizeof row_names[0])
    {
        for (size_t prior = 0; prior < count; ++prior)
            require(!accessible_item_equal(item, rows[prior]), "settings rows are unique");
        verify_row(window, item, count, language);
        rows[count++] = item;
        result = folio_window_accessible_navigate(window, &item, ACCESSIBLE_NAVIGATION_NEXT, &item);
    }
    require(result == S_FALSE && count == sizeof row_names / sizeof row_names[0],
            "forward walk covers all rows and ends");
    return count;
}

static void reverse_walk(struct folio_window *_Nonnull window, struct accessible_item list,
                         const struct accessible_item *_Nonnull rows, size_t count)
{
    struct accessible_item item;
    HRESULT result =
        folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_LAST, &item);
    while (result == S_OK && count > 0)
    {
        --count;
        require(accessible_item_equal(item, rows[count]), "reverse walk visits the same rows");
        result =
            folio_window_accessible_navigate(window, &item, ACCESSIBLE_NAVIGATION_PREVIOUS, &item);
    }
    require(result == S_FALSE && count == 0, "reverse walk ends before the first row");
}

static void verify_buttons(struct folio_window *_Nonnull window,
                           const struct accessible_item *_Nonnull rows)
{
    const size_t ordinals[] = {1, 8};
    for (size_t index = 0; index < sizeof ordinals / sizeof ordinals[0]; ++index)
    {
        struct accessible_item item = rows[ordinals[index]];
        require(item.kind == ACCESSIBLE_SURFACE_BUTTON, "setting action has Button identity");
        struct accessible_request request = {.item = item, .action = ACCESSIBLE_ACTION_FOCUS};
        require(folio_window_accessible_action(window, &request) == S_OK, "focus setting action");
        struct accessible_view view = {0};
        require(folio_window_accessible_read(window, &item, &view) == S_OK && view.focusable &&
                    view.focused && !view.offscreen && !IsRectEmpty(&view.bounds),
                "setting action focus and bounds match the visible row");
    }
}

static void verify_language(struct native_fixture *_Nonnull fixture, enum folio_language language)
{
    require(folio_state_set_language(fixture->state, language) == FOLIO_STATE_READY, "language");
    struct folio_window *_Nullable window = nullptr;
    enum folio_window_outcome created =
        folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window);
    if (created != FOLIO_WINDOW_CREATED)
        fprintf(stderr, "window outcome=%u native_error=%lu\n", (unsigned)created, GetLastError());
    require(created == FOLIO_WINDOW_CREATED, "real window");
    pump(window);
    invoke(window, (struct accessible_item){.kind = ACCESSIBLE_SETTINGS});
    uint64_t revision = folio_window_accessible_surface_revision(window);
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST, .revision = revision};
    struct accessible_item rows[sizeof row_names / sizeof row_names[0]];
    size_t count = walk(window, list, rows, language);
    reverse_walk(window, list, rows, count);
    verify_buttons(window, rows);
    invoke(window, (struct accessible_item){.kind = ACCESSIBLE_SURFACE_BUTTON,
                                            .slot = ACCESSIBLE_SURFACE_CLOSE,
                                            .revision = revision});
    struct accessible_view view = {0};
    require(FAILED(folio_window_accessible_read(window, &rows[count - 1], &view)),
            "closed setting references are invalid");
    folio_window_destroy(window);
}

int main(void)
{
    HDESK desktop = CreateDesktopW(L"NeNeFolioSettingsVerification", nullptr, nullptr, 0,
                                   DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_READOBJECTS |
                                       DESKTOP_WRITEOBJECTS,
                                   nullptr);
    require(desktop != nullptr && SetThreadDesktop(desktop), "isolated desktop");
    require(SUCCEEDED(OleInitialize(nullptr)), "COM apartment");
    struct native_fixture fixture = {0};
    create_fixture(&fixture);
    verify_language(&fixture, FOLIO_LANGUAGE_JA);
    verify_language(&fixture, FOLIO_LANGUAGE_EN);
    verify_language(&fixture, FOLIO_LANGUAGE_ZH_HANS);
    destroy_fixture(&fixture);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("settings accessibility: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
