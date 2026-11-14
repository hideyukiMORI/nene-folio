/* #504: UIAの公開境界を実windowへ接続し、行の重複と循環を有限workで拒否する。 */
#include "accessibility_node.h"
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
    UI_TEXT_SETTINGS_NUMBER_CHECK, UI_TEXT_SETTINGS_MINIMAP,     UI_TEXT_SETTINGS_FAVORITES_FIRST,
    UI_TEXT_STORAGE_ENTRY};

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
    const size_t ordinals[] = {1, 9};
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

/* #641: 同じwindowで再開した設定へ、前の世代の公開参照を転用しない。 */
static void verify_expired(struct folio_window *_Nonnull window, struct accessible_item item)
{
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == accessibility_node_unavailable,
            "old settings reference is unavailable");
    const enum accessible_navigation directions[] = {
        ACCESSIBLE_NAVIGATION_PARENT, ACCESSIBLE_NAVIGATION_NEXT, ACCESSIBLE_NAVIGATION_PREVIOUS,
        ACCESSIBLE_NAVIGATION_FIRST, ACCESSIBLE_NAVIGATION_LAST};
    for (size_t index = 0; index < sizeof directions / sizeof directions[0]; ++index)
    {
        struct accessible_item adjacent;
        require(folio_window_accessible_navigate(window, &item, directions[index], &adjacent) ==
                    accessibility_node_unavailable,
                "old settings reference cannot navigate");
    }
    struct accessible_request request = {.item = item, .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &request) == accessibility_node_unavailable,
            "old settings reference cannot focus");
}

static struct accessible_item close_button(uint64_t revision)
{
    return (struct accessible_item){
        .kind = ACCESSIBLE_SURFACE_BUTTON, .slot = ACCESSIBLE_SURFACE_CLOSE, .revision = revision};
}

static void verify_retired(struct folio_window *_Nonnull window, struct accessible_item list,
                           const struct accessible_item *_Nonnull rows)
{
    verify_expired(window, list);
    verify_expired(window, close_button(list.revision));
    for (size_t index = 0; index < sizeof row_names / sizeof row_names[0]; ++index)
        verify_expired(window, rows[index]);
}

static void verify_root(struct folio_window *_Nonnull window, bool closed)
{
    struct accessible_item root = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &root, &view) == S_OK && view.offscreen == closed,
            "stable surface root follows the current open state");
}

static struct accessible_item verify_open(struct folio_window *_Nonnull window,
                                          struct accessible_item *_Nonnull rows,
                                          enum folio_language language)
{
    invoke(window, (struct accessible_item){.kind = ACCESSIBLE_SETTINGS});
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    size_t count = walk(window, list, rows, language);
    reverse_walk(window, list, rows, count);
    verify_buttons(window, rows);
    verify_root(window, false);
    return list;
}

static void verify_reopen(struct folio_window *_Nonnull window, struct accessible_item previous,
                          const struct accessible_item *_Nonnull old_rows,
                          enum folio_language language)
{
    struct accessible_item rows[sizeof row_names / sizeof row_names[0]];
    struct accessible_item current = verify_open(window, rows, language);
    require(current.revision != previous.revision, "reopened settings have a new generation");
    for (size_t index = 0; index < sizeof rows / sizeof rows[0]; ++index)
        require(rows[index].kind == old_rows[index].kind &&
                    rows[index].slot == old_rows[index].slot &&
                    rows[index].revision == current.revision &&
                    !accessible_item_equal(rows[index], old_rows[index]),
                "same setting row has a distinct current identity");
    struct accessible_item focused;
    require(folio_window_accessible_focus(window, ACCESSIBLE_SURFACE, &focused) == S_OK,
            "current setting has focus before old requests");
    verify_retired(window, previous, old_rows);
    struct accessible_request request = {.item = close_button(previous.revision),
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == accessibility_node_unavailable,
            "old close button cannot close reopened settings");
    pump(window);
    require(folio_window_accessible_surface_revision(window) == current.revision,
            "old close request preserves the current generation");
    struct accessible_item after;
    require(folio_window_accessible_focus(window, ACCESSIBLE_SURFACE, &after) == S_OK &&
                accessible_item_equal(focused, after),
            "old requests preserve current settings focus");
    verify_root(window, false);
    verify_row(window, rows[0], 0, language);
    invoke(window, close_button(current.revision));
    verify_root(window, true);
}

static bool control_selected(struct folio_window *_Nonnull window, struct accessible_item item,
                             enum accessible_kind kind)
{
    require(item.kind == kind, "setting control has the expected role");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && view.enabled,
            "setting control is readable and enabled");
    return view.selected;
}

static void select_control(struct folio_window *_Nonnull window, struct accessible_item item)
{
    /* IToggleProvider::Toggle と ISelectionItemProvider::Select の公開先と同じ要求。 */
    struct accessible_request request = {.item = item, .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK, "select public setting");
    pump(window);
}

struct settings_values
{
    bool number;
    bool minimap;
    enum folio_language language;
};

static void verify_control_values(struct native_fixture *_Nonnull fixture,
                                  struct folio_window *_Nonnull window,
                                  struct accessible_item *_Nonnull rows,
                                  struct settings_values values)
{
    require(folio_state_number(fixture->state) == values.number &&
                folio_state_minimap(fixture->state) == values.minimap &&
                folio_state_language(fixture->state) == values.language,
            "public setting action changes only the intended application value");
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    (void)walk(window, list, rows, values.language);
    require(control_selected(window, rows[6], ACCESSIBLE_SURFACE_TOGGLE) == values.number,
            "line-number check matches application state");
    require(control_selected(window, rows[7], ACCESSIBLE_SURFACE_TOGGLE) == values.minimap,
            "minimap check matches application state");
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t index = 0; index < sizeof languages / sizeof languages[0]; ++index)
        require(control_selected(window, rows[3 + index], ACCESSIBLE_SURFACE_LANGUAGE) ==
                    (languages[index] == values.language),
                "exactly the current language row is selected");
}

/* #645: 公開SELECTと表示状態を、両booleanの全組合せと3言語で対応させる。 */
static void verify_controls(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window, enum folio_language language)
{
    struct settings_values values = {.number = folio_state_number(fixture->state),
                                     .minimap = folio_state_minimap(fixture->state),
                                     .language = language};
    invoke(window, (struct accessible_item){.kind = ACCESSIBLE_SETTINGS});
    struct accessible_item rows[sizeof row_names / sizeof row_names[0]];
    verify_control_values(fixture, window, rows, values);
    for (size_t step = 0; step < 4; ++step)
    {
        select_control(window, rows[6 + step % 2]);
        if (step % 2 == 0)
            values.number = !values.number;
        else
            values.minimap = !values.minimap;
        verify_control_values(fixture, window, rows, values);
    }
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t index = 0; index < sizeof languages / sizeof languages[0]; ++index)
    {
        select_control(window, rows[3 + index]);
        values.language = languages[index];
        verify_control_values(fixture, window, rows, values);
    }
    select_control(window, rows[3 + (size_t)language]);
    values.language = language;
    verify_control_values(fixture, window, rows, values);
    invoke(window, close_button(folio_window_accessible_surface_revision(window)));
    verify_root(window, true);
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
    struct accessible_item rows[sizeof row_names / sizeof row_names[0]];
    struct accessible_item list = verify_open(window, rows, language);
    invoke(window, close_button(list.revision));
    verify_retired(window, list, rows);
    verify_root(window, true);
    verify_reopen(window, list, rows, language);
    verify_controls(fixture, window, language);
    folio_window_destroy(window);
}

static void verify_languages(struct native_fixture *_Nonnull fixture)
{
    verify_language(fixture, FOLIO_LANGUAGE_JA);
    verify_language(fixture, FOLIO_LANGUAGE_EN);
    verify_language(fixture, FOLIO_LANGUAGE_ZH_HANS);
}

struct window_resources
{
    DWORD gdi;
    DWORD user;
    DWORD handles;
};

static DWORD gui_resources(DWORD kind)
{
    SetLastError(ERROR_SUCCESS);
    DWORD count = GetGuiResources(GetCurrentProcess(), kind);
    require(count != 0 || GetLastError() == ERROR_SUCCESS, "GUI resource count");
    return count;
}

static struct window_resources read_resources(unsigned round)
{
    struct window_resources resources = {.gdi = gui_resources(GR_GDIOBJECTS),
                                         .user = gui_resources(GR_USEROBJECTS)};
    require(GetProcessHandleCount(GetCurrentProcess(), &resources.handles) != 0,
            "process handle count");
    printf("resources,%u,%lu,%lu,%lu\n", round, resources.gdi, resources.user, resources.handles);
    fflush(stdout);
    return resources;
}

/* #639: 3言語の初回利用後、同じfixtureを保持したままwindowの生成・破棄をもう一巡する。 */
static void verify_resources(struct native_fixture *_Nonnull fixture)
{
    struct window_resources before = read_resources(0);
    verify_languages(fixture);
    struct window_resources after = read_resources(1);
    require(after.gdi <= before.gdi, "repeated settings windows do not retain more GDI objects");
    require(after.user <= before.user, "repeated settings windows do not retain more USER objects");
    require(after.handles <= before.handles,
            "repeated settings windows do not retain more process handles");
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
    verify_languages(&fixture);
    verify_resources(&fixture);
    destroy_fixture(&fixture);
    OleUninitialize();
    CloseDesktop(desktop);
    printf("settings accessibility: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
