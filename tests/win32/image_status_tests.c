/* #520: body-first loading, processing notice and mode cancellation through the real window. */
#include "accessibility_node.h"
#include "accessible_request.h"
#include "accessible_surface_button.h"
#include "accessible_view.h"
#include "appearance_adapter.h"
#include "clipboard_adapter.h"
#include "discovery_scale_command.h"
#include "discovery_text.h"
#include "discovery_trace_phase.h"
#include "file_bytes.h"
#include "file_times.h"
#include "folio_menu_action.h"
#include "folio_message.h"
#include "folio_settings.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
#include "icon_paint.h"
#include "image_bitmap.h"
#include "image_port.h"
#include "name_list.h"
#include "note_file_times.h"
#include "note_history.h"
#include "note_identity.h"
#include "note_ledger.h"
#include "note_pane_code_button.h"
#include "note_store_request.h"
#include "note_tag.h"
#include "note_tags.h"
#include "note_text.h"
#include "persistence_adapter.h"
#include "product_link.h"
#include "product_version.h"
#include "regex_adapter.h"
#include "rename_view.h"
#include "storage_adapter.h"
#include "storage_native_path.h"
#include "storage_state.h"
#include "tag_catalog.h"
#include "transfer_adapter.h"
#include "ui_face.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "web_link_adapter.h"
#include "web_uri.h"
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>

#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windowsx.h>

struct native_fixture
{
    struct storage_adapter *_Nullable storage;
    struct storage_state *_Nullable location;
    struct persistence_adapter *_Nullable files;
    struct appearance_adapter *_Nullable appearance;
    struct regex_adapter *_Nullable regex;
    struct clipboard_adapter *_Nullable clipboard_adapter;
    struct image_adapter *_Nullable images;
    struct font_bundle *_Nullable fonts;
    struct folio_state *_Nullable state;
};

static unsigned checks;
static bool diagram_case;
static bool history_case;
static bool replace_case;
static bool replace_save_case;
static bool search_case;
static bool note_switch_case;
static bool zoom_case;
static bool theme_preview_case;
static bool settings_edit_case;
static bool mode_save_case;
static bool discard_case;
static bool palette_case;
static bool paste_case;
static bool format_case;
static bool favorites_case;
static bool tags_case;
static bool discovery_case;
static bool motion_case;
static bool motion_visual_case;
static bool motion_live_case;
static size_t scale_note_count;
static size_t scale_source_bytes;
static bool scale_trace_case;
static size_t scale_trace_idle;
static enum discovery_trace_phase scale_trace_phase;
static enum appearance_motion motion_observation = APPEARANCE_MOTION_ALLOWED;
static enum appearance_outcome motion_read_outcome = APPEARANCE_READY;
static bool motion_check_modal;
static bool motion_saw_modal;
static bool body_discovery_case;
static bool body_discovery_copy_case;
static bool body_discovery_visual_case;
static bool body_discovery_measure_case;
static bool discovery_visual_case;
static bool discovery_copy_case;
static bool discovery_measure_case;
static WNDPROC discovery_preview_procedure;
static unsigned discovery_rejected_text;
static WNDPROC progress_layer_procedure;
static unsigned progress_layer_moves;
static unsigned progress_layer_paints;
static bool tag_management_case;
static bool tag_management_visual_case;
static bool tag_management_measure_case;
static WNDPROC management_body_procedure;
static unsigned management_rejected_streams;
static bool tags_visual_case;
static bool tags_reload_case;
static bool tags_metrics_case;
static bool favorites_visual_case;
static bool favorites_ui_case;
static bool favorite_high_contrast;
static bool code_copy_case;
static bool code_copy_visual_case;
static bool code_copy_keys_case_selected;
static bool code_copy_scroll_case;
static char code_copy_recorded[128];
static unsigned code_copy_records;
static enum clipboard_outcome code_copy_result = CLIPBOARD_WRITTEN;
static enum clipboard_outcome code_copy_record(struct clipboard_adapter *_Nonnull adapter,
                                               const char *_Nonnull bytes, size_t length);

static enum appearance_outcome read_favorite_appearance(struct appearance_adapter *_Nonnull adapter,
                                                        struct appearance_snapshot *_Nonnull out)
{
    (void)adapter;
    if (motion_case && motion_read_outcome != APPEARANCE_READY)
        return motion_read_outcome;
    *out = (struct appearance_snapshot){
        .theme = FOLIO_THEME_LIGHT,
        .motion = motion_case ? motion_observation : APPEARANCE_MOTION_UNKNOWN,
        .contrast = favorite_high_contrast ? FOLIO_CONTRAST_HIGH : FOLIO_CONTRAST_STANDARD,
        .window = {0, 0, 0},
        .window_text = {255, 255, 255},
        .button = {0, 0, 0},
        .button_text = {255, 255, 255},
        .highlight = {255, 255, 0},
        .highlight_text = {0, 0, 0}};
    return APPEARANCE_READY;
}
static bool about_case;
static bool about_visual_case;
static unsigned about_opened;
static char about_uri[128];
static enum web_link_outcome about_port_result = WEB_LINK_OPENED;
static const struct folio_state *_Nullable about_state;
static bool layout_case;
static bool save_as_case;
static bool rename_case;
static bool move_edit_case;
static bool unicode_save_case;
static bool first_save_case;
static bool first_save_modal_case;
static bool empty_save_case;
static bool locked_save_case;
static bool filter_save_case;
static bool session_save_case;
static bool session_untitled_case;
static bool session_ime_case;
static bool session_modal_case;
static bool close_modal_case;
static bool export_case;

static const char history_current[] = "# Current 日本語 😀 **saved**";
static const char tags_current[] =
    "---\ntitle: 'Keep' # unknown\ntags: [red, 'blue'] # keep\n---\n# Note 日本語 😀";
static const wchar_t tags_original[] =
    L"---\rtitle: 'Keep' # unknown\rtags: [red, 'blue'] # keep\r---\r# Note 日本語 😀";
static const char body_discovery_source[] =
    "---\r\ntags: [red, blue]\n---\r日本😀\r\nneedle first\nending";
static const char body_discovery_other[] = "before\r\nOther needle\r\nlast";
static const wchar_t tags_added[] =
    L"---\rtitle: 'Keep' # unknown\rtags: [red, 'blue', \"新規 😀\"] # keep\r---\r# Note 日本語 😀";
static const char tags_saved[] =
    "---\ntitle: 'Keep' # unknown\ntags: [red, 'blue', \"新規 😀\"] # keep\n---\n# Note 日本語 😀";
static const char history_previous[] = "> Previous 猫 🐾 `old`";
static const char history_oldest[] = "- Oldest 履歴 🌸";
static const char switch_other[] = "> Other 別ノート 🐾";
static const char switch_saved[] = "# Current 日本語 😀 **saved** 追記 🙂";
static const wchar_t switch_original[] = L"# Current 日本語 😀 **saved**";
static const wchar_t switch_edited[] = L"# Current 日本語 😀 **saved** 追記 🙂";
static const char search_current[] = "猫 😀 aaaa 日本語";
static const char code_copy_current[] =
    "コピー 前文\n\n```c\nint a;\n\tb;\n```\n\n> ```\n> 引用 😀\n>   二行\n> ```\n\n終わり コピー";
static const wchar_t code_copy_source[] =
    L"コピー 前文\r\r```c\rint a;\r\tb;\r```\r\r> ```\r> 引用 😀\r>   二行\r> ```\r\r終わり コピー";
static const char code_copy_gallery[] =
    "# コードブロック\n\n本文の段落です。Body paragraph.\n\n"
    "```python\ndef hello(name):\n    return f\"こんにちは {name}\"\n```\n\n"
    "言語の指定が無いブロック:\n\n```\nplain text block\nsecond line\n```\n\n"
    "```VeryLongUnknownLanguageName-ABCDEFGHIJKLMNOPQRSTUVWXYZ-0123456789-"
    "abcdefghijklmnopqrstuvwxyz\n"
    "payload\n```\n\n> 引用の中:\n>\n> ```sh\n> ls -la\n> ```\n\n- リスト\n\n  ```json\n  {\"a\": "
    "1}\n  ```\n";
static const wchar_t search_original[] = L"猫 😀 aaaa 日本語";
static const wchar_t search_edited[] = L"猫 😀 aaaa 日本語 🐾";
static const char replace_current[] = "猫1 😀\n猫2 **saved**\n猫1 `old`";
static const wchar_t replace_original[] = L"猫1 😀\r猫2 **saved**\r猫1 `old`";
static const wchar_t replace_all[] = L"1-猫-🐾\r 😀\r2-猫-🐾\r **saved**\r1-猫-🐾\r `old`";

static const char first_save_bytes[] = "# New 日本語 🐾\nnext";
static const wchar_t first_save_edited[] = L"# New 日本語 🐾\rnext";

static const char unicode_current[] =
    "\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF# 日本語 😀\r\n記号 \xEF\xBF\xBD\v中"
    "\xEF\xBB\xBF末\t\\{}\r\n\r\n";
static const char unicode_saved[] =
    "\xEF\xBB\xBF\xEF\xBB\xBF\xEF\xBB\xBF# 日本語 😀\r\n記号 \xEF\xBF\xBD\v中"
    "\xEF\xBB\xBF末\t\\{}\r\n\r\n 追記 🙂";
static const wchar_t unicode_original[] =
    L"\uFEFF\uFEFF# 日本語 😀\r記号 \uFFFD\v中\uFEFF末\t\\{}\r\r";
static const wchar_t unicode_edited[] =
    L"\uFEFF\uFEFF# 日本語 😀\r記号 \uFFFD\v中\uFEFF末\t\\{}\r\r 追記 🙂";

static void require(bool condition, const char *_Nonnull message)
{
    ++checks;
    if (!condition)
    {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

/* 製品リンクの外部起動を捕捉する。実adapterへ渡さない（ADR0097）。 */
static enum web_link_outcome about_open_link(const struct web_uri *_Nonnull uri)
{
    about_opened += 1;
    require(strcpy_s(about_uri, sizeof about_uri, web_uri_bytes(uri)) == 0,
            "the product URI fits the observation buffer");
    return about_port_result;
}

static enum note_store_outcome fixture_store(struct persistence_port disk,
                                             const char *_Nonnull category,
                                             const char *_Nonnull note,
                                             const struct note_text *_Nonnull body)
{
    const struct note_store_request request = {category, note, body, nullptr};
    return disk.store_note(disk.adapter, &request);
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
        require(fixture_store(disk, "schedule", "one", body) == NOTE_STORE_STORED,
                "fixture history uses the canonical existing-note save port");
        note_text_destroy(body);
    }
}

static const char *_Nonnull text_fixture_source(const char *_Nonnull fallback)
{
    if (search_case)
        return search_current;
    if (code_copy_case)
        return code_copy_visual_case || code_copy_scroll_case ? code_copy_gallery
                                                              : code_copy_current;
    if (unicode_save_case)
        return unicode_current;
    return fallback;
}

static void seed_tags(struct persistence_port disk)
{
    if (tags_reload_case)
        return;
    const char *categories[] = {"schedule", "schedule", "target"};
    const char *names[] = {"one", "two", "three"};
    const char *sources[] = {tags_current, "---\ntags: [blue, green]\n---\nOther",
                             tags_metrics_case ? "Plain"
                                               : "---\nitem:\n  child: value\n---\nUnsupported"};
    for (size_t at = 0; at < 3; ++at)
    {
        struct note_text *body = nullptr;
        require(note_text_create(sources[at], strlen(sources[at]), &body) == NOTE_TEXT_ACCEPTED,
                "tag native source");
        require(fixture_store(disk, categories[at], names[at], body) == NOTE_STORE_STORED,
                "dedicated tag source saved");
        note_text_destroy(body);
    }
}

static void seed_categories(struct persistence_port disk)
{
    enum persistence_outcome created = disk.create_category(disk.adapter, "schedule");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "test category");
    created = disk.create_category(disk.adapter, "target");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "move category");
}

static void seed_scale_note(struct persistence_port disk, size_t index,
                            const struct note_text *_Nonnull body)
{
    char name[32];
    require(snprintf(name, sizeof name, "note-%04zu", index) > 0, "bounded scale note name");
    const char *category = index % 2 == 0 ? "schedule" : "target";
    enum persistence_outcome made = disk.create_note(disk.adapter, category, name, body);
    require(made == PERSISTENCE_STORED || made == PERSISTENCE_NAME_TAKEN,
            "create or retain the exact private scale source");
    if (made == PERSISTENCE_NAME_TAKEN)
    {
        struct note_text *retained = nullptr;
        require(disk.read_note(disk.adapter, category, name, &retained) == PERSISTENCE_LOADED &&
                    note_text_equals(retained, body),
                "retained scale source must have the independently generated bytes");
        note_text_destroy(retained);
    }
}

static void seed_scale(struct persistence_port disk)
{
    const char common[] = "\ncommon-scale\nordinary-end\n";
    const char unique[] = "\ncommon-scale\nunique-scale\n";
    require(sizeof common == sizeof unique && scale_source_bytes > sizeof common,
            "equal-size scale tails fit the declared source size");
    size_t prefix = scale_source_bytes - (sizeof common - 1);
    char *source = malloc(scale_source_bytes + 1);
    require(source != nullptr, "bounded scale source buffer");
    memset(source, 'a', prefix);
    for (size_t at = 79; at < prefix; at += 80)
        source[at] = '\n';
    for (size_t at = 0; at < scale_note_count; ++at)
    {
        memcpy(source + prefix, at + 1 == scale_note_count ? unique : common, sizeof common);
        struct note_text *body = nullptr;
        require(note_text_create(source, scale_source_bytes, &body) == NOTE_TEXT_ACCEPTED,
                "scale source has exactly the declared byte length");
        seed_scale_note(disk, at, body);
        note_text_destroy(body);
    }
    free(source);
}

static void seed_notes(struct persistence_port disk)
{
    seed_categories(disk);
    if (scale_note_count != 0)
    {
        seed_scale(disk);
        return;
    }
    if (tags_case)
    {
        seed_tags(disk);
        return;
    }
    const char *source =
        diagram_case ? "Body before diagrams\n\n```mermaid\nflowchart LR\nA --> B\n```\n\nAfter\n"
                     : "Body before images\n\n![alt](images/one.png)\n\nBody after images\n";
    if (history_case || replace_case || search_case || note_switch_case || zoom_case ||
        theme_preview_case || settings_edit_case || mode_save_case || discard_case ||
        palette_case || paste_case || format_case || favorites_case || layout_case ||
        save_as_case || rename_case || replace_save_case || move_edit_case || first_save_case ||
        first_save_modal_case || empty_save_case || locked_save_case || unicode_save_case ||
        filter_save_case || session_save_case || session_untitled_case || session_ime_case ||
        session_modal_case || close_modal_case || export_case || about_case)
    {
        seed_history(disk);
        source = history_case || note_switch_case || zoom_case || theme_preview_case ||
                         settings_edit_case || mode_save_case || discard_case || palette_case ||
                         paste_case || format_case || favorites_case || layout_case ||
                         save_as_case || rename_case || replace_save_case || move_edit_case ||
                         first_save_case || first_save_modal_case || empty_save_case ||
                         locked_save_case || filter_save_case || session_save_case ||
                         session_untitled_case || session_ime_case || session_modal_case ||
                         close_modal_case || export_case || about_case
                     ? history_current
                     : replace_current;
    }
    source = text_fixture_source(source);
    struct note_text *_Nullable body = nullptr;
    require(note_text_create(source, strlen(source), &body) == NOTE_TEXT_ACCEPTED, "media note");
    require(fixture_store(disk, "schedule", "one", body) == NOTE_STORE_STORED, "image note stored");
    note_text_destroy(body);
    if (note_switch_case || favorites_case)
    {
        body = nullptr;
        require(note_text_create(switch_other, strlen(switch_other), &body) == NOTE_TEXT_ACCEPTED,
                "valid second note");
        require(fixture_store(disk, "schedule", "two", body) == NOTE_STORE_STORED,
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
static void scale_trace_emit(const char *_Nonnull phase)
{
    printf("{\"event\":\"cost\",\"phase\":\"%s\",\"command\":%u,\"idle\":%zu}\n", phase,
           (unsigned)DISCOVERY_SCALE_FRESH, scale_trace_idle);
    require(fflush(stdout) == 0, "publish the private read/idle observation without a C clock");
}

static enum persistence_outcome scale_observed_read(struct persistence_adapter *_Nonnull adapter,
                                                    const char *_Nonnull category,
                                                    const char *_Nonnull note,
                                                    struct note_text *_Nullable *_Nonnull out)
{
    bool observed = scale_trace_phase != DISCOVERY_TRACE_OFF;
    if (observed)
    {
        require(scale_trace_phase == DISCOVERY_TRACE_IDLE, "at most one read inside each idle");
        scale_trace_phase = DISCOVERY_TRACE_READ;
        scale_trace_emit("read_begin");
    }
    struct persistence_port original = persistence_adapter_port(adapter);
    enum persistence_outcome outcome = original.read_note(adapter, category, note, out);
    if (observed)
    {
        require(scale_trace_phase == DISCOVERY_TRACE_READ,
                "the real read does not reenter tracing");
        scale_trace_emit("read_end");
        scale_trace_phase = DISCOVERY_TRACE_READ_DONE;
    }
    return outcome;
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
    if (scale_trace_case)
        disk.read_note = scale_observed_read;
    struct appearance_port appearance = appearance_adapter_port(fixture->appearance);
    if (favorites_case || tags_case || code_copy_case || about_visual_case)
        appearance.read = read_favorite_appearance;
    struct regex_port regex = regex_adapter_port(fixture->regex);
    struct web_link_port web = web_link_adapter_port();
    if (about_case)
        web.open = about_open_link;
    require(clipboard_adapter_create(&fixture->clipboard_adapter) == CLIPBOARD_ADAPTER_CREATED,
            "clipboard adapter");
    struct clipboard_port clipboard = clipboard_adapter_port(fixture->clipboard_adapter);
    /* Off the isolated station the copy cases only record: the real clipboard is never written. */
    if (code_copy_visual_case || code_copy_keys_case_selected || code_copy_scroll_case)
        clipboard.write = code_copy_record;
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
                                .clipboard = &clipboard,
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
    clipboard_adapter_destroy(fixture->clipboard_adapter);
    appearance_adapter_destroy(fixture->appearance);
    persistence_adapter_destroy(fixture->files);
    storage_state_destroy(fixture->location);
    storage_adapter_destroy(fixture->storage);
}

static void pump(struct folio_window *_Nonnull window)
{
    MSG message;
    unsigned count = 0;
    for (; count < 1000 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE); ++count)
    {
        if (message.message == WM_TIMER)
            continue;
        if (!folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (favorites_case && count == 1000)
    {
        printf("favorites: pump limit reached; final message=%u\n", message.message);
        fflush(stdout);
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

static void save_bitmap(HDC dc, HBITMAP bitmap, RECT client, const char *_Nonnull path)
{
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
    save_bitmap(dc, bitmap, client, path);
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

static void tags_capture(HWND root, const char *_Nonnull path);

static struct accessible_view band_view(struct folio_window *_Nonnull window,
                                        enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "band boundary view");
    return view;
}

static void band_tag_visibility(struct folio_window *_Nonnull window, bool visible)
{
    struct accessible_view tag = band_view(window, ACCESSIBLE_TAG_EDIT);
    require(tag.offscreen != visible, "tag strip follows the active inline band");
    struct accessible_item parent = {.kind = ACCESSIBLE_PANE_TOOLS}, child;
    HRESULT result =
        folio_window_accessible_navigate(window, &parent, ACCESSIBLE_NAVIGATION_FIRST, &child);
    bool found = false;
    size_t visited = 0;
    while (result == S_OK && visited < 20)
    {
        found |= child.kind == ACCESSIBLE_TAG_EDIT;
        ++visited;
        result =
            folio_window_accessible_navigate(window, &child, ACCESSIBLE_NAVIGATION_NEXT, &child);
    }
    require(result == S_FALSE && found == visible,
            "UIA child enumeration agrees with tag visibility");
    if (visible)
    {
        struct accessible_view group = band_view(window, ACCESSIBLE_PANE_TOOLS);
        require(group.bounds.top <= tag.bounds.top && group.bounds.bottom >= tag.bounds.bottom,
                "pane tools bounds contain the visible tag entry");
    }
}

static void band_fixed_controls(struct folio_window *_Nonnull window, RECT *_Nonnull bounds,
                                bool capture)
{
    const enum accessible_kind kinds[] = {ACCESSIBLE_BREADCRUMB, ACCESSIBLE_MODE_GROUP,
                                          ACCESSIBLE_SAVE, ACCESSIBLE_NOTICE};
    for (size_t at = 0; at < sizeof kinds / sizeof kinds[0]; ++at)
    {
        struct accessible_view view = band_view(window, kinds[at]);
        require(!view.offscreen, "note identity, mode, save and status remain visible");
        if (capture)
            bounds[at] = view.bounds;
        if (!capture)
            require(EqualRect(&bounds[at], &view.bounds), "permanent control bounds are unchanged");
    }
}

static void band_body_geometry(struct folio_window *_Nonnull window, enum folio_command command)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct accessible_view status = band_view(window, ACCESSIBLE_NOTICE);
    RECT bounds;
    require(GetWindowRect(body, &bounds), "minimum window body geometry");
    UINT dpi = GetDpiForWindow(root);
    int minimum = MulDiv(command == FOLIO_COMMAND_REPLACE ? 32 : 52, (int)dpi, 96);
    printf("short band %d: body=(%ld,%ld,%ld,%ld) statusTop=%ld dpi=%u\n", command, bounds.left,
           bounds.top, bounds.right, bounds.bottom, status.bounds.top, dpi);
    require(bounds.bottom - bounds.top >= minimum && bounds.bottom <= status.bounds.top,
            "minimum-height search and replace keep a usable body above the status bar");
    struct accessible_view surface = band_view(window, ACCESSIBLE_SURFACE);
    require(surface.bounds.bottom <= bounds.top, "inline controls do not cover the body");
}

static void short_band_case(struct native_fixture *_Nonnull fixture, enum folio_command command,
                            enum folio_language language)
{
    require(folio_state_set_language(fixture->state, language) == FOLIO_STATE_READY,
            "short band language");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "geometry window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SetWindowPos(root, nullptr, 20, 20, 420, 360, SWP_NOZORDER | SWP_NOACTIVATE);
    pump(window);
    RECT fixed[4];
    band_fixed_controls(window, fixed, true);
    band_tag_visibility(window, true);
    invoke_operation(window, command);
    pump(window);
    band_body_geometry(window, command);
    band_tag_visibility(window, false);
    band_fixed_controls(window, fixed, false);
    char path[64];
    require(snprintf(path, sizeof path, "short-band-%d-%d.bmp", command, language) > 0,
            "short band capture path");
    tags_capture(root, path);
    SendMessageW(root, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    struct accessible_item surface = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view closed;
    require(FAILED(folio_window_accessible_read(window, &surface, &closed)) || closed.offscreen,
            "Escape closes the command band through the normal key handler");
    band_tag_visibility(window, true);
    band_fixed_controls(window, fixed, false);
    folio_window_destroy(window);
}

static void short_window_band(struct native_fixture *_Nonnull fixture, enum folio_command command)
{
    require(folio_state_begin_edit(fixture->state) == FOLIO_STATE_READY, "geometry edit state");
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t at = 0; at < sizeof languages / sizeof languages[0]; ++at)
        short_band_case(fixture, command, languages[at]);
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

static enum persistence_outcome named_history_file(struct persistence_port disk,
                                                   const char *_Nonnull name, size_t version,
                                                   struct note_text *_Nullable *_Nonnull out)
{
    if (version == 0)
        return disk.read_note(disk.adapter, "schedule", name, out);
    struct history_version which = {.category = "schedule", .note = name, .version = version};
    return disk.read_history(disk.adapter, &which, out);
}

static enum persistence_outcome history_file(struct persistence_port disk, size_t version,
                                             struct note_text *_Nullable *_Nonnull out)
{
    return named_history_file(disk, "one", version, out);
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

static void expect_named_history_file(struct persistence_port disk, const char *_Nonnull name,
                                      size_t version, const struct note_text *_Nullable expected)
{
    struct note_text *_Nullable actual = nullptr;
    enum persistence_outcome outcome = named_history_file(disk, name, version, &actual);
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

static void expect_history_file(struct persistence_port disk, size_t version,
                                const struct note_text *_Nullable expected)
{
    expect_named_history_file(disk, "one", version, expected);
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

static void expect_note_selection(struct folio_window *_Nonnull window, size_t selected)
{
    for (size_t index = 0; index < 2; ++index)
    {
        struct accessible_item row = {.kind = ACCESSIBLE_NOTE,
                                      .ref = {.category = 0, .note = index},
                                      .revision = folio_window_accessible_revision(window)};
        struct accessible_view view = {0};
        require(folio_window_accessible_read(window, &row, &view) == S_OK &&
                    view.selected == (index == selected),
                "only the selected note exposes IsSelected, independently of focus");
    }
    struct accessible_item category = {.kind = ACCESSIBLE_CATEGORY,
                                       .ref = {.category = 0},
                                       .revision = folio_window_accessible_revision(window)};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &category, &view) == S_OK && !view.selected,
            "a category does not inherit the selected note state");
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE};
    struct accessible_item current = {0};
    require(folio_window_accessible_selected(window, &tree, &current) == S_OK &&
                current.kind == ACCESSIBLE_NOTE && current.ref.category == 0 &&
                current.ref.note == selected,
            "Tree GetSelection agrees with the selected row");
    require(folio_window_accessible_read(window, &tree, &view) == S_OK && view.selection_required,
            "the tree retains the existing document selection requirement");
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
            "focus, select or invoke the current public note row");
    pump(window);
}

static struct note_text *_Nullable switch_other_history[note_history_depth];

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
    /* Reseeding a reused data directory already left history; compare with the start (#847). */
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_named_history_file(disk, "two", version, switch_other_history[version - 1]);
}

static void capture_other_history(struct native_fixture *_Nonnull fixture)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    for (size_t version = 1; version <= note_history_depth; ++version)
    {
        enum persistence_outcome outcome =
            named_history_file(disk, "two", version, &switch_other_history[version - 1]);
        require(outcome == PERSISTENCE_LOADED || outcome == PERSISTENCE_ABSENT,
                "baseline of the second note's history is readable or absent");
    }
}

static void release_other_history(void)
{
    for (size_t version = 0; version < note_history_depth; ++version)
    {
        note_text_destroy(switch_other_history[version]);
        switch_other_history[version] = nullptr;
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
    choose_note(window, 1, ACCESSIBLE_ACTION_FOCUS);
    struct accessible_item focused = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focused) == S_OK &&
                focused.kind == ACCESSIBLE_NOTE && focused.ref.category == 0 &&
                focused.ref.note == 1,
            "Focus actually moves to the other note without selecting it");
    expect_note_selection(window, 0);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, snapshot);
    const enum accessible_action operations[] = {ACCESSIBLE_ACTION_SELECT,
                                                 ACCESSIBLE_ACTION_INVOKE};
    for (size_t index = 0; index < sizeof operations / sizeof operations[0]; ++index)
    {
        choose_note(window, 0, operations[index]);
        expect_note_selection(window, 0);
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
    expect_note_selection(window, 1);
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
    expect_note_selection(window, 0);
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
    capture_other_history(fixture);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real note switching window");
    pump(window);
    expect_note_selection(window, 0);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_other_unchanged(fixture);
    reselect_unsaved(window, fixture, &snapshot);
    switch_and_return(window, fixture, &snapshot);
    require(controlled.requests == 0, "plain note switching does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(snapshot.files[version]);
    release_other_history();
}

struct native_body_snapshot
{
    wchar_t text[192];
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
    wmemset(snapshot->text, L'\x7F', sizeof snapshot->text / sizeof snapshot->text[0]);
    LRESULT length = SendMessageW(body, EM_GETTEXTEX, (WPARAM)&request, (LPARAM)snapshot->text);
    require(length >= 0 && (size_t)length < sizeof snapshot->text / sizeof snapshot->text[0] - 1 &&
                snapshot->text[length] == L'\0',
            "short native fixture writes a terminated snapshot, including empty text");
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

/* #719: 置換の両入力から本文だけ保存し、nativeの入力とUndo/Redoを保つ。 */
static void expect_replace_save_input(HWND input, const wchar_t *_Nonnull expected, DWORD first,
                                      DWORD last)
{
    wchar_t text[16];
    DWORD start = 0;
    DWORD end = 0;
    SendMessageW(input, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
    require(IsWindowVisible(input) && GetWindowTextW(input, text, 16) > 0 &&
                wcscmp(text, expected) == 0 && start == first && end == last,
            "body save preserves each visible replacement input and its selection");
}

static void prepare_replace_save(struct folio_window *_Nonnull window,
                                 struct folio_state *_Nonnull state)
{
    invoke_operation(window, FOLIO_COMMAND_REPLACE);
    pump(window);
    HWND pattern = folio_window_accessible_window(window, ACCESSIBLE_REPLACE_PATTERN);
    HWND replacement = folio_window_accessible_window(window, ACCESSIBLE_REPLACE_TEXT);
    require(pattern != nullptr && replacement != nullptr && SetWindowTextW(pattern, L"日本語") &&
                SetWindowTextW(replacement, L"猫 🐾"),
            "open and populate both ordinary replacement inputs");
    pump(window);
    require(folio_state_replace_count(state) == 1, "real ICU preview finds one pending match");
    SendMessageW(pattern, EM_SETSEL, 1, 3);
    SendMessageW(replacement, EM_SETSEL, 2, 4);
}

static void save_replace_input(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture, enum accessible_kind which,
                               const char *_Nonnull saved)
{
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    prepare_replace_save(window, fixture->state);
    verify_history_unchanged(fixture, &history);
    HWND input = folio_window_accessible_window(window, which);
    SetFocus(input);
    require(GetFocus() == input, "the selected replacement input owns focus before saving");
    struct native_body_snapshot opened = {0};
    capture_native_body(window, &opened);
    expect_editor(window, fixture->state, opened.text, FOLIO_NOTE_CHANGED);
    HWND pattern = folio_window_accessible_window(window, ACCESSIBLE_REPLACE_PATTERN);
    HWND replacement = folio_window_accessible_window(window, ACCESSIBLE_REPLACE_TEXT);
    for (size_t attempt = 0; attempt < 2; ++attempt)
    {
        SendMessageW(input, WM_CHAR, 0x13, 1);
        pump(window);
        expect_replace_save_input(pattern, L"日本語", 1, 3);
        expect_replace_save_input(replacement, L"猫 🐾", 2, 4);
        expect_native_body(window, &opened);
        expect_editor(window, fixture->state, opened.text, FOLIO_NOTE_SAME);
        expect_body_saved(fixture, &history, saved);
    }
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    require(!IsWindowVisible(pattern) && !IsWindowVisible(replacement) &&
                GetFocus() == folio_window_accessible_window(window, ACCESSIBLE_BODY),
            "closing the retained replacement surface returns focus to the body");
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void run_replace_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated replacement-save note");
    struct history_snapshot first = {0};
    struct history_snapshot second = {0};
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real replacement-save window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) != 0, "the initial edit has native Undo");
    save_replace_input(window, fixture, ACCESSIBLE_REPLACE_PATTERN, switch_saved);
    capture_history(fixture, &first);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "the restored original has native Redo");
    verify_history_unchanged(fixture, &first);
    save_replace_input(window, fixture, ACCESSIBLE_REPLACE_TEXT, history_current);
    capture_history(fixture, &second);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "native Redo survives replacement-input save");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &second);
    require(controlled.requests == 0, "plain replacement-save does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
    {
        note_text_destroy(first.files[version]);
        note_text_destroy(second.files[version]);
    }
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

static void discard_and_verify(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture,
                               const struct history_snapshot *_Nonnull history)
{
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(GetFocus() == body, "discard returns focus to the editable body");
    require(SendMessageW(body, EM_CANUNDO, 0, 0) == 0 && SendMessageW(body, EM_CANREDO, 0, 0) == 0,
            "discard removes both undoable and redoable edits");
    require(SendMessageW(body, EM_UNDO, 0, 0) == 0 && SendMessageW(body, EM_REDO, 0, 0) == 0,
            "discarded edits cannot return through Undo or Redo");
    pump(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, history);
}

/* #702: Discard resets the native edit history without publishing a saved revision. */
static void run_discard_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated discard note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real discard window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) != 0, "changed edit is undoable before discard");
    discard_and_verify(window, fixture, &history);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "unchanged text still has a redoable edit before discard");
    discard_and_verify(window, fixture, &history);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "new edits remain redoable after discard");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &history);
    discard_and_verify(window, fixture, &history);
    require(controlled.requests == 0, "plain discard does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void palette_query(struct folio_window *_Nonnull window, const wchar_t *_Nonnull text,
                          HRESULT expected_selection)
{
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_HELP},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "open the operation palette through its public header action");
    pump(window);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr && GetFocus() == input && SetWindowTextW(input, text),
            "the focused palette input receives the query");
    pump(window);
    wchar_t actual[64];
    require(GetWindowTextW(input, actual, 64) == (int)wcslen(text) && wcscmp(actual, text) == 0,
            "the palette keeps the exact query in its own input");
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item selected = {0};
    require(folio_window_accessible_selected(window, &list, &selected) == expected_selection,
            "the public palette selection distinguishes matching and unmatched queries");
}

static void palette_roundtrip(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull history)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    palette_query(window, L"write", S_OK);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_native_body(window, &snapshot);
    verify_history_unchanged(fixture, history);
    palette_query(window, L"no-such-palette-command", S_FALSE);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr, "the unmatched palette still has its own input");
    SendMessageW(input, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    expect_native_body(window, &snapshot);
    verify_history_unchanged(fixture, history);
}

/* #704: Filtering and cancelling the palette preserves both changed and redoable edits. */
static void run_palette_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated palette note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real palette window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    palette_roundtrip(window, fixture, &history);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "Undo leaves a redoable insertion before opening the palette");
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    palette_roundtrip(window, fixture, &history);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo succeeds after cancelling the palette");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    require(controlled.requests == 0, "plain palette queries do not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #479: The clipboard belongs to a different window station, never to the user's desktop. */
static HWINSTA _Nonnull isolate_paste_clipboard(void)
{
    HWINSTA station = CreateWindowStationW(nullptr, 0, WINSTA_ALL_ACCESS, nullptr);
    require(station != nullptr && SetProcessWindowStation(station), "isolated clipboard station");
    wchar_t name[256];
    DWORD needed = 0;
    require(GetUserObjectInformationW(GetProcessWindowStation(), UOI_NAME, name, sizeof name,
                                      &needed) &&
                _wcsicmp(name, L"WinSta0") != 0,
            "clipboard station is not the interactive station");
    return station;
}

static void publish_clipboard(UINT format, const void *_Nonnull bytes, size_t size)
{
    HGLOBAL block = GlobalAlloc(GMEM_MOVEABLE, size);
    require(block != nullptr, "clipboard allocation");
    void *data = GlobalLock(block);
    require(data != nullptr, "clipboard lock");
    memcpy(data, bytes, size);
    GlobalUnlock(block);
    require(SetClipboardData(format, block) != nullptr, "clipboard owns the published bytes");
}

static void rich_clipboard(HWND _Nonnull owner)
{
    const wchar_t text[] = L"Bold Light";
    const char rich[] = "{\\rtf1\\ansi{\\fonttbl{\\f0 Times New Roman;}}"
                        "{\\colortbl;\\red220\\green20\\blue20;}"
                        "\\pard\\qc\\f0\\fs36\\cf1\\b Bold\\b0  Light}";
    require(OpenClipboard(owner) && EmptyClipboard(), "open isolated clipboard");
    publish_clipboard(RegisterClipboardFormatW(L"Rich Text Format"), rich, sizeof rich);
    publish_clipboard(CF_UNICODETEXT, text, sizeof text);
    require(CloseClipboard(), "close isolated clipboard");
}

static CHARFORMAT2W character_format(HWND _Nonnull body, LONG position)
{
    CHARRANGE selection = {.cpMin = position, .cpMax = position + 1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(body, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    return format;
}

/* Exercise the same translate/dispatch order as main.c, including the generated WM_CHAR. */
static bool native_key_input_to(struct folio_window *_Nonnull window, HWND body, UINT key,
                                BYTE modifiers)
{
    if (key == 0)
    {
        SendMessageW(body, WM_PASTE, 0, 0);
        pump(window);
        return false;
    }
    BYTE original[256], keyboard[256] = {0};
    require(GetKeyboardState(original), "save fixture keyboard state");
    keyboard[VK_CONTROL] = (modifiers & FCONTROL) != 0 ? 0x80 : 0;
    keyboard[VK_SHIFT] = (modifiers & FSHIFT) != 0 ? 0x80 : 0;
    keyboard[key] = 0x80;
    require(SetKeyboardState(keyboard), "fixture key modifiers");
    MSG message = {.hwnd = body, .message = WM_KEYDOWN, .wParam = key, .lParam = 1};
    bool translated = folio_window_translate(window, &message);
    if (!translated)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    pump(window);
    if (format_case)
        printf("key route=%u translated=%d visible=%d enabled=%d focus=%d ctrl=%d left=%d\n", key,
               translated, IsWindowVisible(body), IsWindowEnabled(body), GetFocus() == body,
               GetKeyState(VK_CONTROL), GetKeyState(VK_LCONTROL));
    require(SetKeyboardState(original), "restore fixture keyboard state");
    return translated;
}

static bool native_key_input(struct folio_window *_Nonnull window, UINT key, BYTE modifiers)
{
    return native_key_input_to(window, folio_window_accessible_window(window, ACCESSIBLE_BODY), key,
                               modifiers);
}

/* PrintWindow is black on the private station: render the actual native document to a memory DC. */
static void paste_pixels(struct folio_window *_Nonnull window, const char *_Nonnull path,
                         const struct theme_palette *_Nonnull palette)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    RECT client = {.right = 900, .bottom = 160};
    HDC source = GetDC(body);
    HDC dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    struct rgb_color background = palette->pane;
    SetDCBrushColor(dc, RGB(background.red, background.green, background.blue));
    FillRect(dc, &client, (HBRUSH)GetStockObject(DC_BRUSH));
    RECT page = {.right = MulDiv(client.right, 1440, GetDeviceCaps(dc, LOGPIXELSX)),
                 .bottom = MulDiv(client.bottom, 1440, GetDeviceCaps(dc, LOGPIXELSY))};
    RECT text = page;
    text.left = 120;
    text.top = 120;
    FORMATRANGE format = {.hdc = dc, .hdcTarget = dc, .rc = text, .rcPage = page, .chrg = {0, -1}};
    LRESULT end = SendMessageW(body, EM_FORMATRANGE, TRUE, (LPARAM)&format);
    SendMessageW(body, EM_FORMATRANGE, FALSE, 0);
    GETTEXTLENGTHEX count = {.flags = GTL_PRECISE | GTL_NUMCHARS, .codepage = 1200};
    LRESULT length = SendMessageW(body, EM_GETTEXTLENGTHEX, (WPARAM)&count, 0);
    require(length >= 0 && end >= length, "native output includes the complete short body");
    SelectObject(dc, old);
    save_bitmap(dc, bitmap, client, path);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(body, source);
}

static void paste_roundtrip(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture, UINT key, BYTE modifiers)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARFORMAT2W original = character_format(body, 0);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 9};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    native_key_input(window, key, modifiers);
    expect_editor(window, fixture->state, L"# Bold Light 日本語 😀 **saved**", FOLIO_NOTE_CHANGED);
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selection);
    require(selection.cpMin == 12 && selection.cpMax == 12, "paste collapses caret after text");
    if (key == 0)
        paste_pixels(window, "paste-editor.bmp", folio_state_palette(fixture->state));
    CHARFORMAT2W first = character_format(body, 2), last = character_format(body, 7);
    printf("paste key=%u modifiers=%u heights=%ld/%ld/%ld weights=%u/%u/%u\n", key, modifiers,
           original.yHeight, first.yHeight, last.yHeight, original.wWeight, first.wWeight,
           last.wWeight);
    require(original.yHeight == first.yHeight && original.yHeight == last.yHeight &&
                original.wWeight == first.wWeight && original.wWeight == last.wWeight &&
                original.crTextColor == first.crTextColor &&
                original.crTextColor == last.crTextColor &&
                wcscmp(original.szFaceName, first.szFaceName) == 0 &&
                wcscmp(original.szFaceName, last.szFaceName) == 0 &&
                (first.dwEffects & (CFE_BOLD | CFE_ITALIC | CFE_UNDERLINE)) == 0,
            "clipboard character formatting is not imported");
    PARAFORMAT2 paragraph = {.cbSize = sizeof paragraph};
    SendMessageW(body, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph);
    require(paragraph.wAlignment == PFA_LEFT, "clipboard paragraph formatting is not imported");
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) != 0, "paste marks the editor dirty");
    selection = (CHARRANGE){.cpMin = 12, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    SendMessageW(body, EM_STOPGROUPTYPING, 0, 0);
    SendMessageW(body, WM_CHAR, '!', 1);
    pump(window);
    expect_editor(window, fixture->state, L"# Bold Light! 日本語 😀 **saved**", FOLIO_NOTE_CHANGED);
    CHARFORMAT2W typed = character_format(body, 12);
    require(typed.yHeight == original.yHeight && typed.crTextColor == original.crTextColor &&
                (typed.dwEffects & (CFE_BOLD | CFE_ITALIC | CFE_UNDERLINE)) == 0,
            "continued typing keeps the editor format");
    history_undo(window);
    expect_editor(window, fixture->state, L"# Bold Light 日本語 😀 **saved**", FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "paste is redoable");
    expect_editor(window, fixture->state, L"# Bold Light 日本語 😀 **saved**", FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
}

static void single_clipboard(HWND _Nonnull owner, UINT format, const void *_Nonnull bytes,
                             size_t size)
{
    require(OpenClipboard(owner) && EmptyClipboard(), "replace isolated clipboard contents");
    if (format != 0)
        publish_clipboard(format, bytes, size);
    require(CloseClipboard(), "close single-format clipboard");
}

static void paste_text_formats(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    const wchar_t unicode[] = L"日本語 😀\r\n\tNext";
    single_clipboard(body, CF_UNICODETEXT, unicode, sizeof unicode);
    SendMessageW(body, EM_SETSEL, 0, -1);
    native_key_input(window, 'V', FCONTROL);
    expect_editor(window, fixture->state, L"日本語 😀\r\tNext", FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    const char ansi[] = "ANSI plain";
    single_clipboard(body, CF_TEXT, ansi, sizeof ansi);
    SendMessageW(body, EM_SETSEL, 0, -1);
    native_key_input(window, 0, 0);
    expect_editor(window, fixture->state, L"ANSI plain", FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    rich_clipboard(body);
    const wchar_t preferred[] = L"Unicode wins";
    require(OpenClipboard(body), "add independent Unicode representation");
    publish_clipboard(CF_UNICODETEXT, preferred, sizeof preferred);
    require(CloseClipboard(), "close mixed representations");
    SendMessageW(body, EM_SETSEL, 0, -1);
    native_key_input(window, VK_INSERT, FSHIFT);
    expect_editor(window, fixture->state, preferred, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
}

static void expect_paste_blocked(struct folio_window *_Nonnull window)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    native_key_input(window, 0, 0);
    expect_native_body(window, &snapshot);
    native_key_input(window, 'V', FCONTROL);
    expect_native_body(window, &snapshot);
    native_key_input(window, VK_INSERT, FSHIFT);
    expect_native_body(window, &snapshot);
}

static void paste_unavailable_formats(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    const char rich[] = "{\\rtf1\\ansi\\b Rich only}";
    single_clipboard(body, RegisterClipboardFormatW(L"Rich Text Format"), rich, sizeof rich);
    SendMessageW(body, EM_SETSEL, 2, 9);
    expect_paste_blocked(window);
    const char html[] = "<b>HTML only</b>";
    single_clipboard(body, RegisterClipboardFormatW(L"HTML Format"), html, sizeof html);
    expect_paste_blocked(window);
    single_clipboard(body, 0, "", 0);
    expect_paste_blocked(window);
}

static void paste_input_guards(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    rich_clipboard(body);
    require(SendMessageW(body, EM_SETREADONLY, TRUE, 0) != 0, "test readonly editor");
    expect_paste_blocked(window);
    require(SendMessageW(body, EM_SETREADONLY, FALSE, 0) != 0, "restore writable editor");
    EnableWindow(body, FALSE);
    expect_paste_blocked(window);
    EnableWindow(body, TRUE);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    EnableWindow(root, FALSE);
    expect_paste_blocked(window);
    EnableWindow(root, TRUE);
    SendMessageW(body, WM_IME_STARTCOMPOSITION, 0, 0);
    expect_paste_blocked(window);
    SendMessageW(body, WM_IME_ENDCOMPOSITION, 0, 0);
    action(window, ACCESSIBLE_VIEW);
    CHARFORMAT2W heading = character_format(body, 0);
    require((heading.dwEffects & CFE_BOLD) != 0, "view retains Markdown heading formatting");
    SendMessageW(body, EM_SETSEL, 0, 0);
    paste_pixels(window, "paste-view.bmp", folio_state_palette(fixture->state));
    expect_paste_blocked(window);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    CHARFORMAT2W edit = character_format(body, 0);
    require((edit.dwEffects & CFE_BOLD) == 0 && edit.yHeight == 220,
            "return to edit restores the plain editor format");
}

static void expect_editor_font(HWND _Nonnull body, const CHARFORMAT2W *_Nonnull expected)
{
    CHARRANGE selected;
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    GETTEXTLENGTHEX count = {.flags = GTL_PRECISE | GTL_NUMCHARS, .codepage = 1200};
    LRESULT length = SendMessageW(body, EM_GETTEXTLENGTHEX, (WPARAM)&count, 0);
    require(length > 0, "font comparison covers a nonempty native document");
    wchar_t text[256];
    GETTEXTEX request = {.cb = sizeof text, .flags = GT_RAWTEXT, .codepage = 1200};
    require(SendMessageW(body, EM_GETTEXTEX, (WPARAM)&request, (LPARAM)text) == length &&
                (size_t)length < sizeof text / sizeof text[0],
            "font comparison reads every native UTF-16 unit");
    DWORD mask = CFM_FACE | CFM_SIZE | CFM_WEIGHT | CFM_COLOR | CFM_BOLD | CFM_ITALIC |
                 CFM_UNDERLINE | CFM_STRIKEOUT;
    DWORD effects = CFE_BOLD | CFE_ITALIC | CFE_UNDERLINE | CFE_STRIKEOUT;
    require((expected->dwMask & mask) == mask, "original editor format is fully defined");
    for (LONG position = 0; position < length; ++position)
    {
        CHARFORMAT2W format = character_format(body, position);
        bool symbol =
            text[position] == L'●' || (text[position] >= 0xd800 && text[position] <= 0xdfff);
        bool matching_face = wcscmp(format.szFaceName, expected->szFaceName) == 0;
        if (text[position] >= 0xd800 && text[position] <= 0xdfff)
            require(!matching_face && (format.dwEffects & CFE_FONTBOUND) != 0,
                    "emoji keeps native fallback beyond the bundled CJK font");
        require((format.dwMask & mask) == mask && format.yHeight == expected->yHeight &&
                    format.wWeight == expected->wWeight &&
                    format.crTextColor == expected->crTextColor &&
                    (format.dwEffects & effects) == (expected->dwEffects & effects) &&
                    (symbol || matching_face),
                "every script keeps the document font, size, weight and color");
    }
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
}

static void paste_font_roundtrip(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture, UINT key, BYTE modifiers)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    const wchar_t text[] =
        L"● 日本語の文章と金額100を確認 (id12345・￥1,234) を整える。\r"
        L"API項目と英数字の表示を確認する。\r日本語の前後にABCを置く。\r简体汉语ABC。😀";
    const wchar_t edited[] = L"● 日本語の文章と金額100を確認 (id12345・￥1,234) を整える。\r"
                             L"API項目と英数字の表示を確認する。\r日本語の前後にABCを置く。\r简体汉"
                             L"语ABC。😀追記Ab漢字😀";
    CHARFORMAT2W original = character_format(body, 0);
    const wchar_t *const faces[] = {[FOLIO_LANGUAGE_JA] = L"Noto Sans JP",
                                    [FOLIO_LANGUAGE_EN] = L"Noto Sans JP",
                                    [FOLIO_LANGUAGE_ZH_HANS] = L"Noto Sans SC"};
    require(wcscmp(original.szFaceName, faces[folio_state_language(fixture->state)]) == 0,
            "the fixture starts with its language's bundled document font");
    expect_editor_font(body, &original);
    single_clipboard(body, CF_UNICODETEXT, text, sizeof text);
    SendMessageW(body, EM_SETSEL, 0, -1);
    native_key_input(window, key, modifiers);
    expect_editor(window, fixture->state, text, FOLIO_NOTE_CHANGED);
    CHARRANGE selection;
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selection);
    require(selection.cpMin == (LONG)wcslen(text) && selection.cpMax == selection.cpMin,
            "mixed-script paste leaves the caret after the complete text");
    expect_editor_font(body, &original);
    if (key == 0)
        paste_pixels(window, "paste-fonts.bmp", folio_state_palette(fixture->state));
    SendMessageW(body, EM_STOPGROUPTYPING, 0, 0);
    const wchar_t typed[] = L"追記Ab漢字😀";
    for (size_t at = 0; at < wcslen(typed); ++at)
        SendMessageW(body, WM_CHAR, typed[at], 1);
    pump(window);
    expect_editor(window, fixture->state, edited, FOLIO_NOTE_CHANGED);
    expect_editor_font(body, &original);
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) != 0, "continued input remains modified");
    history_undo(window);
    expect_editor(window, fixture->state, text, FOLIO_NOTE_CHANGED);
    expect_editor_font(body, &original);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "mixed-script paste is one redo unit");
    expect_editor(window, fixture->state, text, FOLIO_NOTE_CHANGED);
    expect_editor_font(body, &original);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_editor_font(body, &original);
}

static void paste_font_languages(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_EN, FOLIO_LANGUAGE_ZH_HANS,
                                             FOLIO_LANGUAGE_JA};
    for (size_t index = 0; index < sizeof languages / sizeof languages[0]; ++index)
    {
        struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                              .action = ACCESSIBLE_ACTION_INVOKE};
        require(folio_window_accessible_action(window, &settings) == S_OK,
                "open settings for the next font language");
        pump(window);
        select_setting_control(window, 3 + (size_t)languages[index], ACCESSIBLE_SURFACE_LANGUAGE);
        surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
        require(folio_state_language(fixture->state) == languages[index],
                "font fixture adopts the selected language");
        action(window, ACCESSIBLE_VIEW);
        action(window, ACCESSIBLE_EDIT);
        expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
        paste_font_roundtrip(window, fixture, 'V', FCONTROL);
    }
}

static void run_paste_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated paste note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real paste window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    rich_clipboard(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    paste_roundtrip(window, fixture, 0, 0);
    paste_roundtrip(window, fixture, 'V', FCONTROL);
    paste_roundtrip(window, fixture, 'V', FCONTROL | FSHIFT);
    paste_roundtrip(window, fixture, VK_INSERT, FSHIFT);
    paste_text_formats(window, fixture);
    paste_font_roundtrip(window, fixture, 0, 0);
    paste_font_roundtrip(window, fixture, 'V', FCONTROL);
    paste_font_roundtrip(window, fixture, 'V', FCONTROL | FSHIFT);
    paste_font_roundtrip(window, fixture, VK_INSERT, FSHIFT);
    paste_font_languages(window, fixture);
    paste_unavailable_formats(window);
    paste_input_guards(window, fixture);
    verify_history_unchanged(fixture, &history);
    require(controlled.requests == 0, "clipboard text does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static bool observe_format_key(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture, UINT key, BYTE modifiers)
{
    printf("format begin key=%u modifiers=%u\n", key, modifiers);
    fflush(stdout);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARFORMAT2W before = character_format(body, 2);
    require(before.yHeight == 220 && (before.dwEffects & CFE_BOLD) == 0,
            "each formatting key starts in the plain editor");
    PARAFORMAT2 paragraph_before = {.cbSize = sizeof paragraph_before};
    SendMessageW(body, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph_before);
    require(paragraph_before.wAlignment == PFA_LEFT && paragraph_before.wNumbering == 0,
            "each formatting key starts with the plain paragraph");
    SendMessageW(body, EM_SETSEL, 2, 9);
    native_key_input(window, key, modifiers);
    wchar_t text[256];
    GetWindowTextW(body, text, sizeof text / sizeof text[0]);
    bool same_text = wcscmp(text, switch_original) == 0;
    CHARFORMAT2W after = character_format(body, 2);
    PARAFORMAT2 paragraph_after = {.cbSize = sizeof paragraph_after};
    SendMessageW(body, EM_GETPARAFORMAT, 0, (LPARAM)&paragraph_after);
    bool same_character = before.yHeight == after.yHeight && before.wWeight == after.wWeight &&
                          before.crTextColor == after.crTextColor &&
                          before.dwEffects == after.dwEffects &&
                          wcscmp(before.szFaceName, after.szFaceName) == 0;
    bool same_paragraph = paragraph_before.wAlignment == paragraph_after.wAlignment &&
                          paragraph_before.wNumbering == paragraph_after.wNumbering &&
                          paragraph_before.dyLineSpacing == paragraph_after.dyLineSpacing &&
                          paragraph_before.bLineSpacingRule == paragraph_after.bLineSpacingRule;
    printf("format key=%u modifiers=%u text=%d character=%d paragraph=%d "
           "height=%ld/%ld effects=%lu/%lu align=%u/%u spacing=%ld/%ld rule=%u/%u number=%u/%u\n",
           key, modifiers, same_text, same_character, same_paragraph, before.yHeight, after.yHeight,
           before.dwEffects, after.dwEffects, paragraph_before.wAlignment,
           paragraph_after.wAlignment, paragraph_before.dyLineSpacing,
           paragraph_after.dyLineSpacing, paragraph_before.bLineSpacingRule,
           paragraph_after.bLineSpacingRule, paragraph_before.wNumbering,
           paragraph_after.wNumbering);
    fflush(stdout);
    return same_text && same_character && same_paragraph;
}

static void paragraph_keys_preserve_edit(struct folio_window *_Nonnull window)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    const UINT keys[] = {'R', '2', '5'};
    for (size_t at = 0; at < sizeof keys / sizeof keys[0]; ++at)
    {
        require(native_key_input(window, keys[at], FCONTROL),
                "paragraph-only shortcut is consumed before character translation");
        expect_native_body(window, &snapshot);
    }
    PARAFORMAT2 paragraph = {.cbSize = sizeof paragraph};
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_GETPARAFORMAT, 0,
                 (LPARAM)&paragraph);
    require(paragraph.wAlignment == PFA_LEFT && paragraph.dyLineSpacing == 20 &&
                paragraph.bLineSpacingRule == 5,
            "blocked shortcuts retain plain paragraph spacing and alignment");
}

static void format_edit_history(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    append_switch_text(window);
    SendMessageW(body, EM_SETSEL, 10, 12);
    paragraph_keys_preserve_edit(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    native_key_input(window, 'Z', FCONTROL);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "typing remains redoable");
    SendMessageW(body, EM_SETSEL, 10, 12);
    paragraph_keys_preserve_edit(window);
    native_key_input(window, 'Y', FCONTROL);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    native_key_input(window, 'Z', FCONTROL);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
}

static void format_neighbor_inputs(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    native_key_input(window, 'A', FCONTROL);
    CHARRANGE selected = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    printf("Ctrl+A selection=%ld/%ld length=%d\n", selected.cpMin, selected.cpMax,
           GetWindowTextLengthW(body));
    fflush(stdout);
    /* RichEdit's Select All can include its final paragraph mark. */
    LONG length = GetWindowTextLengthW(body);
    require(selected.cpMin == 0 && selected.cpMax >= length && selected.cpMax <= length + 1,
            "Ctrl+A still selects the complete body");
    SendMessageW(body, EM_SETSEL, 2, 2);
    native_key_input(window, VK_RIGHT, FCONTROL | FSHIFT);
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin == 2 && selected.cpMax > 2, "word selection still extends to the right");
    CHARRANGE end = {.cpMin = -1, .cpMax = -1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&end);
    SendMessageW(body, WM_CHAR, L'追', 1);
    native_key_input(window, 'I', FCONTROL);
    native_key_input(window, VK_RETURN, 0);
    expect_editor(window, fixture->state, L"# Current 日本語 😀 **saved**追\t\r",
                  FOLIO_NOTE_CHANGED);
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    native_key_input(window, '3', FCONTROL | FSHIFT);
    require(folio_state_layout_mode(fixture->state) == FOLIO_LAYOUT_NOTE,
            "Ctrl+Shift+3 still selects note layout");
    native_key_input(window, '2', FCONTROL | FSHIFT);
    require(folio_state_layout_mode(fixture->state) == FOLIO_LAYOUT_BOTH,
            "Ctrl+Shift+2 is not a blocked line-spacing shortcut");
    native_key_input(window, 'E', FCONTROL);
    CHARFORMAT2W heading = character_format(body, 0);
    require((heading.dwEffects & CFE_BOLD) != 0 && heading.yHeight > 220,
            "the product mode shortcut retains formatted Markdown viewing");
    native_key_input(window, 'E', FCONTROL);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
}

static void format_route_controls(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(IsWindowVisible(body), "format diagnosis uses a visible body on the private desktop");
    SendMessageW(body, EM_SETSEL, 0, -1);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"first\rsecond\rthird");
    SendMessageW(body, EM_SETSEL, 0, 0);
    require(native_key_input(window, 'J', FCONTROL),
            "Ctrl+J is handled by the product caret accelerator");
    CHARRANGE selected = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin > 0 && selected.cpMin == selected.cpMax, "Ctrl+J moves down a line");
    require(native_key_input(window, 'K', FCONTROL),
            "Ctrl+K is handled by the product caret accelerator");
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin == 0 && selected.cpMax == 0, "Ctrl+K moves back up");
    require(native_key_input(window, 'L', FCONTROL),
            "Ctrl+L is handled by the product caret accelerator");
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin == 1 && selected.cpMax == 1, "Ctrl+L moves right one character");
    require(native_key_input(window, 'H', FCONTROL),
            "Ctrl+H is handled by the product caret accelerator");
    expect_editor(window, fixture->state, L"first\rsecond\rthird", FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    CHARRANGE end = {.cpMin = -1, .cpMax = -1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&end);
    native_key_input(window, VK_RETURN, FCONTROL);
    wchar_t text[128];
    GETTEXTEX request = {.cb = sizeof text, .flags = GT_RAWTEXT, .codepage = 1200};
    LRESULT length = SendMessageW(body, EM_GETTEXTEX, (WPARAM)&request, (LPARAM)text);
    require(length > 0 && length < 128, "Ctrl+Enter observation fits");
    printf("Ctrl+Enter length=%lld last=%u\n", (long long)length, (unsigned)text[length - 1]);
    expect_editor(window, fixture->state, L"# Current 日本語 😀 **saved**\r", FOLIO_NOTE_CHANGED);
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
}

static void run_format_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated format key note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real window for native formatting keys");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    format_route_controls(window, fixture);
    const UINT ordinary[] = {'J', 'R', 'L', '1', '2', '5', 'B', 'U'};
    const UINT shifted[] = {'A', 'L', VK_OEM_PERIOD, VK_OEM_COMMA, VK_LEFT, VK_RIGHT};
    bool unchanged = true;
    for (size_t at = 0; at < sizeof ordinary / sizeof ordinary[0]; ++at)
    {
        unchanged &= observe_format_key(window, fixture, ordinary[at], FCONTROL);
        invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
        pump(window);
    }
    for (size_t at = 0; at < sizeof shifted / sizeof shifted[0]; ++at)
    {
        unchanged &= observe_format_key(window, fixture, shifted[at], FCONTROL | FSHIFT);
        invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
        pump(window);
    }
    require(unchanged, "native formatting keys preserve the plain editor body and format");
    format_edit_history(window, fixture);
    format_neighbor_inputs(window, fixture);
    verify_history_unchanged(fixture, &history);
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

struct layout_step
{
    enum folio_layout_mode mode;
    enum accessible_kind control;
    enum folio_command command;
    enum accessible_kind focus;
    enum accessible_kind repeated_focus;
};

/* A narrow drawer exposes the existing chooser, not the hidden three-choice controls. */
static bool select_layout(struct folio_window *_Nonnull window,
                          const struct layout_step *_Nonnull step)
{
    struct accessible_item item = {.kind = step->control};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK,
            "read the public layout choice");
    if (view.enabled)
    {
        require(!view.offscreen, "select only a visible layout choice");
        action(window, step->control);
        return false;
    }
    require(view.offscreen, "a hidden layout choice is disabled");
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_LAYOUT_CHOOSER},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_read(window, &request.item, &view) == S_OK && view.enabled &&
                !view.offscreen,
            "the narrow layout chooser is visible and enabled");
    require(folio_window_accessible_action(window, &request) == S_OK, "open the layout chooser");
    pump(window);
    for (size_t at = 0; at < folio_menu_action_count(FOLIO_MENU_LAYOUT); ++at)
    {
        const struct folio_menu_action *entry = folio_menu_action_at(FOLIO_MENU_LAYOUT, at);
        if (folio_menu_action_kind(entry) == FOLIO_MENU_ACTION_COMMAND &&
            folio_menu_action_command(entry) == step->command)
        {
            request.item = (struct accessible_item){
                .kind = ACCESSIBLE_SURFACE_CHOICE,
                .slot = at,
                .revision = folio_window_accessible_surface_revision(window)};
            require(folio_window_accessible_action(window, &request) == S_OK,
                    "invoke the requested layout through its visible chooser row");
            pump(window);
            return true;
        }
    }
    require(false, "the requested layout exists in the chooser");
    return false;
}

static void expect_layout(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture,
                          const struct layout_step *_Nonnull step,
                          const struct native_body_snapshot *_Nonnull snapshot)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(folio_state_layout_mode(fixture->state) == step->mode &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "layout selection changes only layout, retaining edit mode");
    require((IsWindowVisible(body) != 0) == (step->mode != FOLIO_LAYOUT_DRAWER),
            "layout selection hides the body only in drawer mode");
    const enum accessible_kind choices[] = {ACCESSIBLE_LAYOUT_DRAWER, ACCESSIBLE_LAYOUT_BOTH,
                                            ACCESSIBLE_LAYOUT_NOTE, ACCESSIBLE_VIEW,
                                            ACCESSIBLE_EDIT};
    for (size_t index = 0; index < sizeof choices / sizeof choices[0]; ++index)
    {
        struct accessible_item choice = {.kind = choices[index]};
        struct accessible_view view = {0};
        require(folio_window_accessible_read(window, &choice, &view) == S_OK &&
                    view.selected ==
                        (choices[index] == step->control || choices[index] == ACCESSIBLE_EDIT),
                "layout and mode groups retain exactly their current selected choice");
    }
    expect_native_body(window, snapshot);
}

static void expect_layout_focus(struct folio_window *_Nonnull window, enum accessible_kind expected)
{
    if (expected == ACCESSIBLE_BODY)
        return; /* The native HWND focus is checked by expect_native_body. */
    struct accessible_item focused = {0};
    enum accessible_kind root =
        expected == ACCESSIBLE_DRAWER ? ACCESSIBLE_DRAWER : ACCESSIBLE_WINDOW;
    require(folio_window_accessible_focus(window, root, &focused) == S_OK,
            "layout focus belongs to a visible public item");
    require(expected == ACCESSIBLE_DRAWER || focused.kind == expected,
            "the menu opener or its visible layout replacement retains header focus");
}

static void layout_roundtrip(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull history)
{
    /* ADR0075: chooser focus maps to the visible BOTH choice, then survives later Select actions.
     */
    const struct layout_step steps[] = {
        {FOLIO_LAYOUT_NOTE, ACCESSIBLE_LAYOUT_NOTE, FOLIO_COMMAND_LAYOUT_NOTE, ACCESSIBLE_BODY,
         ACCESSIBLE_BODY},
        {FOLIO_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH, FOLIO_COMMAND_LAYOUT_BOTH, ACCESSIBLE_BODY,
         ACCESSIBLE_BODY},
        {FOLIO_LAYOUT_DRAWER, ACCESSIBLE_LAYOUT_DRAWER, FOLIO_COMMAND_LAYOUT_DRAWER,
         ACCESSIBLE_DRAWER, ACCESSIBLE_LAYOUT_CHOOSER},
        {FOLIO_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH, FOLIO_COMMAND_LAYOUT_BOTH,
         ACCESSIBLE_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH},
        {FOLIO_LAYOUT_NOTE, ACCESSIBLE_LAYOUT_NOTE, FOLIO_COMMAND_LAYOUT_NOTE,
         ACCESSIBLE_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH},
        {FOLIO_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH, FOLIO_COMMAND_LAYOUT_BOTH,
         ACCESSIBLE_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH}};
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "start each layout roundtrip from the public body focus action");
    pump(window);
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    require(snapshot.focus == body, "the layout roundtrip starts with body focus");
    bool used_chooser = false;
    for (size_t at = 0; at < sizeof steps / sizeof steps[0]; ++at)
    {
        printf("layout step=%zu select\n", at);
        used_chooser |= select_layout(window, &steps[at]);
        snapshot.focus = steps[at].focus == ACCESSIBLE_BODY ? body : root;
        expect_layout(window, fixture, &steps[at], &snapshot);
        expect_layout_focus(window, steps[at].focus);
        printf("layout step=%zu repeat\n", at);
        used_chooser |= select_layout(window, &steps[at]);
        snapshot.focus = steps[at].repeated_focus == ACCESSIBLE_BODY ? body : root;
        expect_layout(window, fixture, &steps[at], &snapshot);
        expect_layout_focus(window, steps[at].repeated_focus);
        verify_history_unchanged(fixture, history);
    }
    require(used_chooser, "the roundtrip exercises the narrow chooser as well as visible choices");
}

/* #711: Hiding and restoring the real editor must not save, reload or discard its undo history. */
static void run_layout_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated layout note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real window for layout edit retention");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    layout_roundtrip(window, fixture, &history);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "Undo leaves the layout input redoable");
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    layout_roundtrip(window, fixture, &history);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo succeeds after the layout roundtrip");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    require(controlled.requests == 0, "plain layout changes do not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void native_file_path(struct native_fixture *_Nonnull fixture,
                             const wchar_t *_Nonnull suffix, wchar_t path[storage_native_capacity])
{
    size_t length = 0;
    size_t count = wcslen(suffix) + 1;
    require(storage_native_path_fill(storage_state_directory(fixture->location), path, &length) &&
                length + count <= storage_native_capacity,
            "dedicated raw file path fits");
    memcpy(path + length, suffix, count * sizeof *suffix);
}

static void expect_category_copy(struct native_fixture *_Nonnull fixture,
                                 const char *_Nonnull category, const char *_Nonnull name,
                                 const char *_Nonnull expected)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *text = nullptr;
    require(disk.read_note(disk.adapter, category, name, &text) == PERSISTENCE_LOADED &&
                text != nullptr,
            "the new copy is readable");
    require(note_text_file_length(text) == strlen(expected) &&
                memcmp(note_text_file_bytes(text), expected, strlen(expected)) == 0,
            "the new copy contains every expected Markdown byte");
    note_text_destroy(text);
    for (size_t version = 1; version <= note_history_depth; ++version)
    {
        text = nullptr;
        struct history_version which = {.category = category, .note = name, .version = version};
        require(disk.read_history(disk.adapter, &which, &text) == PERSISTENCE_ABSENT &&
                    text == nullptr,
                "a newly created copy has no archived history");
        note_text_destroy(text);
    }
}

static void expect_copy(struct native_fixture *_Nonnull fixture, const char *_Nonnull name,
                        const char *_Nonnull expected)
{
    expect_category_copy(fixture, "schedule", name, expected);
}

static void expect_name_operation_body(struct folio_window *_Nonnull window,
                                       const struct native_body_snapshot *_Nonnull before)
{
    struct native_body_snapshot after = {0};
    capture_native_body(window, &after);
    require(wcscmp(after.text, before->text) == 0 &&
                after.selection.cpMin == before->selection.cpMin &&
                after.selection.cpMax == before->selection.cpMax,
            "Name operation retains all native text units and the selection");
    require(after.undo == before->undo && after.redo == before->redo,
            "Name operation retains native Undo and Redo availability");
}

static void submit_ex(struct folio_window *_Nonnull window, const wchar_t *_Nonnull command)
{
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_TREE},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "open Ex from the public tree focus without typing into the editor");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SendMessageW(root, WM_CHAR, ':', 0);
    pump(window);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr && IsWindowVisible(input) && GetFocus() == input,
            "the real Ex input is visible and focused");
    require(SetWindowTextW(input, command), "enter the dedicated Ex command");
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    pump(window);
}

static void save_as_command(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture, char name[64])
{
    size_t count = folio_state_note_count(fixture->state);
    int length = snprintf(name, 64, "zzcopy-%zu", count);
    require(length > 0 && length < 64, "dedicated copy name fits");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *absent = nullptr;
    require(disk.read_note(disk.adapter, "schedule", name, &absent) == PERSISTENCE_ABSENT &&
                absent == nullptr,
            "repeat runs never overwrite a previous dedicated copy");
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    wchar_t command[96];
    require(swprintf(command, 96, L":saveas zzcopy-%zu", count) > 0, "the Save As command fits");
    submit_ex(window, command);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(!IsWindowVisible(input) && folio_state_note_count(fixture->state) == count + 1 &&
                strcmp(folio_state_document_name(fixture->state), name) == 0,
            "Ex creates and selects exactly one new note and closes");
    expect_name_operation_body(window, &before);
}

static void expect_save_as_original(struct native_fixture *_Nonnull fixture,
                                    const struct history_snapshot *_Nonnull history,
                                    uint64_t copies)
{
    require(folio_state_store_revision(fixture->state) == history->store_revision + copies,
            "only the requested new copies publish a store revision");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    for (size_t version = 0; version <= note_history_depth; ++version)
        expect_history_file(disk, version, history->files[version]);
}

/* #733: Both ledger and OS name collisions preserve the open editor and old files. */
static void collision_file_id(struct native_fixture *_Nonnull fixture,
                              const wchar_t *_Nonnull suffix,
                              BY_HANDLE_FILE_INFORMATION *_Nonnull identity)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, suffix, path);
    HANDLE file = CreateFileW(path, FILE_READ_ATTRIBUTES,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "the collision target already exists");
    require(GetFileInformationByHandle(file, identity), "the collision target has a file identity");
    require(CloseHandle(file), "release the identity handle before Save As");
}

static void expect_case_alias(struct native_fixture *_Nonnull fixture)
{
    BY_HANDLE_FILE_INFORMATION original = {0};
    BY_HANDLE_FILE_INFORMATION alias = {0};
    collision_file_id(fixture, L"\\schedule\\one.md", &original);
    collision_file_id(fixture, L"\\schedule\\ONE.md", &alias);
    require(original.dwVolumeSerialNumber == alias.dwVolumeSerialNumber &&
                original.nFileIndexHigh == alias.nFileIndexHigh &&
                original.nFileIndexLow == alias.nFileIndexLow,
            "the case variant refers to the same existing file in this dedicated directory");
}

static void expect_no_save_as_temporary(struct native_fixture *_Nonnull fixture)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\.nenefolio-write-*.tmp", path);
    WIN32_FIND_DATAW entry = {0};
    HANDLE found = FindFirstFileW(path, &entry);
    DWORD error = found == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    if (found != INVALID_HANDLE_VALUE)
        require(FindClose(found), "close the unexpected temporary file search");
    require(found == INVALID_HANDLE_VALUE && error == ERROR_FILE_NOT_FOUND,
            "a refused Save As leaves no temporary write file in the dedicated category");
}

static void expect_collision_ledger(struct native_fixture *_Nonnull fixture,
                                    const struct note_ledger *_Nullable before,
                                    const struct name_list *_Nonnull public_names)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_ledger *after = nullptr;
    struct name_list *files = nullptr;
    size_t count = name_list_count(public_names);
    enum persistence_outcome outcome = disk.read_note_ledger(disk.adapter, "schedule", &after);
    if (before == nullptr)
        require(outcome == PERSISTENCE_ABSENT && after == nullptr,
                "rejection does not create an initially absent ledger");
    else
        require(outcome == PERSISTENCE_LOADED && after != nullptr &&
                    note_ledger_count(after) == note_ledger_count(before),
                "rejection preserves the existing ledger length");
    require(folio_state_note_count(fixture->state) == count,
            "rejection preserves the public note count");
    require(disk.scan_notes(disk.adapter, "schedule", &files) == PERSISTENCE_LOADED &&
                files != nullptr && name_list_count(files) == count,
            "rejection creates no extra Markdown file");
    for (size_t index = 0; index < count; ++index)
    {
        const char *name = name_list_at(public_names, index);
        require(strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, index}), name) ==
                        0 &&
                    name_list_contains(files, name),
                "every public row and existing filename is preserved");
    }
    if (before != nullptr)
        for (size_t index = 0; index < note_ledger_count(before); ++index)
            require(strcmp(note_ledger_name(after, index), note_ledger_name(before, index)) == 0,
                    "every persisted ledger row retains its name and order");
    name_list_destroy(files);
    note_ledger_destroy(after);
    expect_no_save_as_temporary(fixture);
}

static void reject_save_as(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture, const wchar_t *_Nonnull command)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.modified != 0 && before.undo != 0 &&
                before.selection.cpMin < before.selection.cpMax,
            "collision starts with changed text, real Undo and a nonempty selection");
    submit_ex(window, command);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    wchar_t query[64];
    require(IsWindowVisible(input) && GetFocus() == input && GetWindowTextW(input, query, 64) > 0 &&
                wcscmp(query, command) == 0,
            "name collision retains the exact Ex input and its focus");
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_STATUS,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view status = {0};
    require(folio_window_accessible_read(window, &item, &status) == S_OK && !status.offscreen &&
                wcsstr(status.name, L"同じ名前のノートがあります。") != nullptr,
            "name collision exposes the existing inline reason");
    expect_name_operation_body(window, &before);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) == before.modified,
            "rejection preserves the native modified flag");
    SendMessageW(input, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    require(!IsWindowVisible(input) && GetFocus() != input,
            "Escape closes the rejected command without leaving focus in the hidden input");
    expect_name_operation_body(window, &before);
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) == before.modified,
            "dismissing the rejected command preserves the native modified flag");
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_NAMED &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                folio_state_selection(fixture->state, &selected) && selected.category == 0 &&
                strcmp(folio_state_note_name(fixture->state, selected), "one") == 0,
            "rejection retains the original named document and selection");
}

static void save_as_collisions(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture,
                               const struct history_snapshot *_Nonnull history)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_ledger *ledger = nullptr;
    enum persistence_outcome outcome = disk.read_note_ledger(disk.adapter, "schedule", &ledger);
    require((outcome == PERSISTENCE_LOADED && ledger != nullptr) ||
                (outcome == PERSISTENCE_ABSENT && ledger == nullptr),
            "snapshot the existing ledger or its initial absence before any collision");
    struct name_list *public_names = nullptr;
    require(name_list_create(&public_names) == NAME_LIST_ACCEPTED && public_names != nullptr,
            "snapshot all public note names before any collision");
    for (size_t index = 0; index < folio_state_note_count(fixture->state); ++index)
    {
        const char *name = folio_state_note_name(fixture->state, (struct note_ref){0, index});
        require(name_list_append(public_names, name, strlen(name)) == NAME_LIST_ACCEPTED,
                "snapshot each public name in its original order");
    }
    expect_collision_ledger(fixture, ledger, public_names);
    expect_case_alias(fixture);
    const wchar_t *commands[] = {L":saveas one", L":saveas ONE"};
    for (size_t index = 0; index < sizeof commands / sizeof commands[0]; ++index)
    {
        reject_save_as(window, fixture, commands[index]);
        verify_history_unchanged(fixture, history);
        expect_collision_ledger(fixture, ledger, public_names);
        expect_case_alias(fixture);
    }
    note_ledger_destroy(ledger);
    name_list_destroy(public_names);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "native Redo succeeds after both collisions");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, history);
    CHARRANGE selection = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
}

/* #735/#737/#739/#741/#743: Observe and finish only the real name dialog owned by this test. */
enum name_dialog_action
{
    NAME_DIALOG_CANCEL,
    NAME_DIALOG_CLOSE,
    NAME_DIALOG_DISCARD,
    NAME_DIALOG_SAVE,
    NAME_DIALOG_RETRY_SAVE,
    NAME_DIALOG_SESSION_CANCEL,
    NAME_DIALOG_ROOT_CLOSE
};

struct name_rejection_probe;

struct name_dialog_request
{
    enum name_dialog_action action;
    bool discard;
    const wchar_t *title;
    const wchar_t *name;
    const struct name_rejection_probe *rejection;
};

struct name_dialog_probe
{
    HWND root;
    HWND body;
    HWND drawer;
    HWND dialog;
    UINT_PTR timer;
    unsigned ticks;
    unsigned closed;
    struct name_dialog_request request;
};

static struct name_dialog_probe name_dialog;
static const wchar_t cancelled_name[] = L"zzcancel 日本語 🐾";

static void enter_pending_destination(HWND dialog)
{
    HWND name = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    HWND categories = FindWindowExW(dialog, nullptr, L"LISTBOX", nullptr);
    HWND cancel = GetDlgItem(dialog, IDCANCEL);
    require(name != nullptr && categories != nullptr && cancel != nullptr &&
                (GetWindowLongPtrW(name, GWL_STYLE) & ES_READONLY) == 0 && IsWindowEnabled(name) &&
                IsWindowEnabled(categories) && IsWindowEnabled(cancel) && GetFocus() == name,
            "the name modal opens editable name, category list and cancel with name focus");
    HWND discard = GetDlgItem(dialog, 3);
    require(name_dialog.request.discard
                ? discard != nullptr && IsWindowVisible(discard) && IsWindowEnabled(discard)
                : discard == nullptr,
            "only untitled departure offers a discard button");
    if (name_dialog.request.discard)
    {
        wchar_t label[64];
        require(GetWindowTextW(discard, label, 64) > 0 && wcscmp(label, L"保存しない(&D)") == 0,
                "the departure action is explicitly labelled Don't Save");
    }
    require(IsWindowEnabled(name_dialog.root) && !IsWindowEnabled(name_dialog.body) &&
                !IsWindowEnabled(name_dialog.drawer),
            "the name modal leaves the root enabled while disabling its content controls");
    require(SetWindowTextW(name, name_dialog.request.name),
            "enter a valid unsaved destination name");
    LRESULT target = SendMessageW(categories, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"target");
    require(target != LB_ERR && SendMessageW(categories, LB_GETCURSEL, 0, 0) != target &&
                SendMessageW(categories, LB_SETCURSEL, (WPARAM)target, 0) != LB_ERR &&
                SendMessageW(categories, LB_GETCURSEL, 0, 0) == target,
            "change only the name dialog's selected destination category");
    wchar_t value[64];
    require(GetWindowTextW(name, value, 64) > 0 && wcscmp(value, name_dialog.request.name) == 0,
            "the modal has a real pending destination name rather than an empty form");
}

static void accept_name_dialog(HWND dialog)
{
    require(IsWindowEnabled(GetDlgItem(dialog, IDOK)), "the entered destination can be submitted");
    SendMessageW(GetDlgItem(dialog, IDOK), BM_CLICK, 0, 0);
}

static void retry_name_dialog(HWND dialog, const struct name_rejection_probe *_Nonnull probe);
static void probe_session_name_dialog(HWND dialog,
                                      const struct name_rejection_probe *_Nonnull probe);

static void close_root_from_name_dialog(HWND dialog,
                                        const struct name_rejection_probe *_Nonnull probe);

static BOOL CALLBACK close_name_dialog(HWND dialog, LPARAM parameter)
{
    wchar_t kind[32];
    if (GetAncestor(dialog, GA_ROOTOWNER) != (HWND)parameter || !IsWindowVisible(dialog) ||
        GetClassNameW(dialog, kind, 32) == 0 || wcscmp(kind, L"#32770") != 0)
        return TRUE;
    wchar_t title[64];
    require(GetWindowTextW(dialog, title, 64) > 0 && wcscmp(title, name_dialog.request.title) == 0,
            "the owned modal is the intended name dialog");
    enter_pending_destination(dialog);
    require(KillTimer(nullptr, name_dialog.timer), "stop the owned name dialog observer");
    name_dialog.timer = 0;
    name_dialog.dialog = dialog;
    ++name_dialog.closed;
    switch (name_dialog.request.action)
    {
    case NAME_DIALOG_CANCEL:
        SendMessageW(GetDlgItem(dialog, IDCANCEL), BM_CLICK, 0, 0);
        break;
    case NAME_DIALOG_CLOSE:
        SendMessageW(dialog, WM_CLOSE, 0, 0);
        break;
    case NAME_DIALOG_SAVE:
        accept_name_dialog(dialog);
        break;
    case NAME_DIALOG_RETRY_SAVE:
        require(name_dialog.request.rejection != nullptr,
                "rejection checks borrow live fixture state");
        retry_name_dialog(dialog, name_dialog.request.rejection);
        accept_name_dialog(dialog);
        break;
    case NAME_DIALOG_SESSION_CANCEL:
        require(name_dialog.request.rejection != nullptr,
                "session checks borrow live fixture state for the synchronous modal call");
        probe_session_name_dialog(dialog, name_dialog.request.rejection);
        SendMessageW(GetDlgItem(dialog, IDCANCEL), BM_CLICK, 0, 0);
        break;
    case NAME_DIALOG_ROOT_CLOSE:
        require(name_dialog.request.rejection != nullptr,
                "normal close checks borrow live fixture state for the synchronous modal call");
        close_root_from_name_dialog(dialog, name_dialog.request.rejection);
        break;
    case NAME_DIALOG_DISCARD:
        SendMessageW(GetDlgItem(dialog, 3), BM_CLICK, 0, 0);
        break;
    }
    return FALSE;
}

static void CALLBACK observe_name_dialog(HWND window, UINT message, UINT_PTR timer,
                                         DWORD tick_millis)
{
    (void)window;
    (void)message;
    (void)tick_millis;
    if (name_dialog.timer == 0)
        return;
    require(timer == name_dialog.timer && ++name_dialog.ticks <= 100,
            "the owned name dialog observer stays within its finite deadline");
    EnumThreadWindows(GetCurrentThreadId(), close_name_dialog, (LPARAM)name_dialog.root);
}

static void start_name_dialog(struct folio_window *_Nonnull window,
                              struct name_dialog_request request)
{
    name_dialog = (struct name_dialog_probe){
        .root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW),
        .body = folio_window_accessible_window(window, ACCESSIBLE_BODY),
        .drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER),
        .request = request};
    require(IsWindow(name_dialog.drawer) && name_dialog.drawer != name_dialog.root &&
                IsChild(name_dialog.root, name_dialog.drawer),
            "the native drawer belongs to the root and is not the abstract tree host");
    name_dialog.timer = SetTimer(nullptr, 0, 50, observe_name_dialog);
    require(name_dialog.timer != 0, "arm the finite name dialog observer");
}

static void finish_name_dialog(void)
{
    if (name_dialog.timer != 0)
    {
        require(KillTimer(nullptr, name_dialog.timer), "cancel an unused name dialog observer");
        name_dialog.timer = 0;
    }
    require(name_dialog.closed == 1 && !IsWindow(name_dialog.dialog) &&
                IsWindow(name_dialog.root) && IsWindowEnabled(name_dialog.root) &&
                IsWindowEnabled(name_dialog.body) && IsWindowEnabled(name_dialog.drawer),
            "one owned name dialog closes and the root and content controls are restored");
}

static void expect_name_file_set(struct persistence_port disk, const char *_Nonnull category,
                                 const struct name_list *_Nonnull before)
{
    struct name_list *after = nullptr;
    require(disk.scan_notes(disk.adapter, category, &after) == PERSISTENCE_LOADED &&
                after != nullptr && name_list_count(after) == name_list_count(before),
            "the name operation preserves the category's complete file count");
    for (size_t index = 0; index < name_list_count(before); ++index)
        require(name_list_contains(after, name_list_at(before, index)),
                "the name operation preserves every prior Markdown filename");
    name_list_destroy(after);
}

static void cancel_save_as(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture, bool close_message)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK && GetFocus() == body,
            "open Save As from the actual editor focus");
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    struct note_ref selected = {0};
    require(folio_state_selection(fixture->state, &selected), "the original has a selected row");
    char name[64];
    int length = snprintf(name, sizeof name, "%s", folio_state_document_name(fixture->state));
    require(length > 0 && (size_t)length < sizeof name &&
                before.selection.cpMin < before.selection.cpMax &&
                (close_message ? before.redo != 0 : before.undo != 0),
            "cancellation starts with a named document, selection and the intended Undo or Redo");
    uint64_t revision = folio_state_store_revision(fixture->state);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct name_list *source_files = nullptr;
    struct name_list *target_files = nullptr;
    require(disk.scan_notes(disk.adapter, "schedule", &source_files) == PERSISTENCE_LOADED &&
                source_files != nullptr &&
                disk.scan_notes(disk.adapter, "target", &target_files) == PERSISTENCE_LOADED &&
                target_files != nullptr,
            "snapshot both dedicated category file sets before name cancellation");
    start_name_dialog(window,
                      (struct name_dialog_request){.title = L"別名で保存",
                                                   .name = cancelled_name,
                                                   .action = close_message ? NAME_DIALOG_CLOSE
                                                                           : NAME_DIALOG_CANCEL});
    bool translated = native_key_input(window, 'S', FCONTROL | FSHIFT);
    finish_name_dialog();
    require(translated, "Ctrl+Shift+S translates through the registered accelerator");
    expect_native_body(window, &before);
    struct note_ref after = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_NAMED &&
                strcmp(folio_state_document_name(fixture->state), name) == 0 &&
                folio_state_selection(fixture->state, &after) &&
                after.category == selected.category && after.note == selected.note &&
                folio_state_store_revision(fixture->state) == revision,
            "name cancellation retains the named selected row and publishes no store revision");
    expect_name_file_set(disk, "schedule", source_files);
    expect_name_file_set(disk, "target", target_files);
    name_list_destroy(source_files);
    name_list_destroy(target_files);
}

static void save_as_edit(struct folio_window *_Nonnull window,
                         struct native_fixture *_Nonnull fixture,
                         const struct history_snapshot *_Nonnull history)
{
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    cancel_save_as(window, fixture, false);
    expect_save_as_original(fixture, history, 0);
    save_as_collisions(window, fixture, history);
    char edited_name[64];
    save_as_command(window, fixture, edited_name);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_copy(fixture, edited_name, switch_saved);
    expect_save_as_original(fixture, history, 1);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo survives Save As of changed text");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "second Save As starts with native Redo");
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    cancel_save_as(window, fixture, true);
    expect_copy(fixture, edited_name, switch_saved);
    expect_save_as_original(fixture, history, 1);
    char redo_name[64];
    save_as_command(window, fixture, redo_name);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_copy(fixture, redo_name, history_current);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo survives Save As of undone text");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_copy(fixture, redo_name, history_current);
    expect_copy(fixture, edited_name, switch_saved);
    expect_save_as_original(fixture, history, 2);
}

/* #715: The native Save As path must not reload the editor or save the original first. */
static void run_save_as_case(struct native_fixture *_Nonnull fixture)
{
    require(strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, 0}), "one") == 0,
            "copies remain after the original in the dedicated fixture");
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated original note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real window for Save As");
    pump(window);
    save_as_edit(window, fixture, &history);
    action(window, ACCESSIBLE_VIEW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 5};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    struct native_body_snapshot view = {0};
    capture_native_body(window, &view);
    require(wcsstr(view.text, L"**saved**") == nullptr,
            "the rendered view differs from the Markdown source being copied");
    char view_name[64];
    save_as_command(window, fixture, view_name);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "Save As preserves VIEW and readonly mode");
    expect_copy(fixture, view_name, history_current);
    expect_save_as_original(fixture, &history, 3);
    require(controlled.requests == 0, "plain Save As does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void select_rename_source(struct native_fixture *_Nonnull fixture)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct name_list *names = nullptr;
    require(disk.scan_notes(disk.adapter, "schedule", &names) == PERSISTENCE_LOADED &&
                names != nullptr &&
                name_list_count(names) == folio_state_note_count(fixture->state),
            "all dedicated rename notes belong to the fixture category");
    size_t count = name_list_count(names);
    name_list_destroy(names);
    for (size_t index = 0; index < count; ++index)
    {
        if (strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, index}), "one") == 0)
        {
            require(folio_state_select_note(fixture->state, 0, index, FOLIO_UNTITLED_PROTECT) ==
                        FOLIO_STATE_READY,
                    "select the new original without moving a previous renamed fixture");
            return;
        }
    }
    require(false, "the dedicated original exists in the current ledger");
}

static void expect_rename_path_absent(struct native_fixture *_Nonnull fixture,
                                      const wchar_t *_Nonnull suffix)
{
    wchar_t root[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), root, &length),
            "the dedicated data path is available");
    wchar_t path[storage_native_capacity];
    require(swprintf(path, storage_native_capacity, L"%ls%ls", root, suffix) > 0,
            "the dedicated rename path fits");
    require(GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES &&
                GetLastError() == ERROR_FILE_NOT_FOUND,
            "the old history directory or completed rename journal is absent");
}

static void expect_rename_name_absent(struct native_fixture *_Nonnull fixture,
                                      const char *_Nonnull name)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    for (size_t version = 0; version <= note_history_depth; ++version)
        expect_named_history_file(disk, name, version, nullptr);
    wchar_t wide[64];
    require(MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name, -1, wide, 64) > 0,
            "the dedicated rename name converts to UTF-16");
    wchar_t suffix[128];
    require(swprintf(suffix, 128, L"\\.history\\schedule\\%ls", wide) > 0,
            "the dedicated history suffix fits");
    expect_rename_path_absent(fixture, suffix);
}

static void expect_renamed_files(struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull expected)
{
    require(folio_state_store_revision(fixture->state) == expected->store_revision,
            "rename saves only changed editor contents, once per change");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    const char *name = folio_state_document_name(fixture->state);
    for (size_t version = 0; version <= note_history_depth; ++version)
        expect_named_history_file(disk, name, version, expected->files[version]);
}

static void expect_rename_ledger(struct native_fixture *_Nonnull fixture)
{
    size_t count = folio_state_note_count(fixture->state);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_ledger *ledger = nullptr;
    require(disk.read_note_ledger(disk.adapter, "schedule", &ledger) == PERSISTENCE_LOADED &&
                ledger != nullptr && note_ledger_count(ledger) == count,
            "rename publishes the complete note ledger without adding or removing rows");
    for (size_t index = 0; index < count; ++index)
        require(strcmp(note_ledger_name(ledger, index),
                       folio_state_note_name(fixture->state, (struct note_ref){0, index})) == 0,
                "all renamed ledger rows retain the public order and exact UTF-8 names");
    note_ledger_destroy(ledger);
}

static void expect_rename_row(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture, const wchar_t *_Nonnull name)
{
    struct note_ref selected = {0};
    require(folio_state_selection(fixture->state, &selected) && selected.category == 0 &&
                strcmp(folio_state_note_name(fixture->state, selected),
                       folio_state_document_name(fixture->state)) == 0,
            "the renamed document remains the selected public note");
    struct accessible_item item = {.kind = ACCESSIBLE_NOTE,
                                   .ref = selected,
                                   .revision = folio_window_accessible_revision(window)};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK &&
                wcscmp(view.name, name) == 0,
            "the selected row exposes the exact UTF-16 rename input");
    require(view.selected, "the renamed row retains its selected state through the main window");
    expect_rename_ledger(fixture);
}

static void rename_command(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture, unsigned step)
{
    size_t count = folio_state_note_count(fixture->state);
    char old[64];
    int length = snprintf(old, 64, "%s", folio_state_document_name(fixture->state));
    require(length > 0 && length < 64, "the original name fits before its ledger is replaced");
    char name[64];
    const char *prefix = step == 2 ? "zzrename" : "zz改名 猫🐾";
    length = snprintf(name, 64, "%s-%zu-%u", prefix, count, step);
    require(length > 0 && length < 64, "the dedicated destination name fits");
    expect_rename_name_absent(fixture, name);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    const wchar_t *wide_prefix = step == 2 ? L"zzrename" : L"zz改名 猫🐾";
    wchar_t wide_name[64];
    require(swprintf(wide_name, 64, L"%ls-%zu-%u", wide_prefix, count, step) > 0,
            "the independently written UTF-16 destination fits");
    wchar_t command[96];
    require(swprintf(command, 96, L":rename %ls", wide_name) > 0,
            "the dedicated rename command fits");
    submit_ex(window, command);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(!IsWindowVisible(input) && folio_state_note_count(fixture->state) == count &&
                strcmp(folio_state_document_name(fixture->state), name) == 0,
            "rename changes only the selected name and closes Ex");
    expect_rename_row(window, fixture, wide_name);
    expect_name_operation_body(window, &before);
    expect_rename_name_absent(fixture, old);
    struct rename_view pending = {.kind = RENAME_KIND_NOTE, .from = "", .to = ""};
    require(!folio_state_rename_pending(fixture->state, &pending),
            "the completed rename leaves no application recovery intent");
    expect_rename_path_absent(fixture, L"\\.rename.json");
}

static void rename_same_name(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull history)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    submit_ex(window, L":rename one");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    wchar_t query[64];
    require(IsWindowVisible(input) && GetFocus() == input && GetWindowTextW(input, query, 64) > 0 &&
                wcscmp(query, L":rename one") == 0,
            "same-name rejection retains the Ex input");
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_STATUS,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view status = {0};
    require(folio_window_accessible_read(window, &item, &status) == S_OK && !status.offscreen &&
                wcsstr(status.name, L"同じ名前です。") != nullptr,
            "same-name rejection exposes its existing inline reason");
    expect_name_operation_body(window, &before);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, history);
    expect_rename_path_absent(fixture, L"\\.rename.json");
    SendMessageW(input, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    require(!IsWindowVisible(input), "Escape closes the retained Ex input");
    expect_name_operation_body(window, &before);
}

static void expected_rename_save(struct history_snapshot *_Nonnull expected,
                                 struct note_text *_Nonnull content)
{
    for (size_t version = note_history_depth; version > 0; --version)
        expected->files[version] = expected->files[version - 1];
    expected->files[0] = content;
    ++expected->store_revision;
}

static void rename_edit(struct folio_window *_Nonnull window,
                        struct native_fixture *_Nonnull fixture,
                        const struct history_snapshot *_Nonnull history,
                        struct history_snapshot *_Nonnull expected)
{
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    rename_same_name(window, fixture, history);
    rename_command(window, fixture, 1);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_renamed_files(fixture, expected);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo survives rename of changed text");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "the second rename starts with native Redo");
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    rename_command(window, fixture, 2);
    struct note_text *original = history->files[0];
    require(original != nullptr, "the captured original is available for the second save");
    expected_rename_save(expected, original);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_renamed_files(fixture, expected);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo survives rename of undone text");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_renamed_files(fixture, expected);
}

/* #717: Save changed text before rename, moving every history slot without reloading the editor. */
static void run_rename_case(struct native_fixture *_Nonnull fixture)
{
    select_rename_source(fixture);
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct note_text *edited = nullptr;
    require(note_text_create(switch_saved, strlen(switch_saved), &edited) == NOTE_TEXT_ACCEPTED,
            "valid expected edited Markdown for rename");
    struct history_snapshot expected = history;
    expected_rename_save(&expected, edited);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real window for rename edit retention");
    pump(window);
    rename_edit(window, fixture, &history, &expected);
    action(window, ACCESSIBLE_VIEW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 5};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    struct native_body_snapshot view = {0};
    capture_native_body(window, &view);
    require(wcsstr(view.text, L"**saved**") == nullptr,
            "the view differs from the raw Markdown being renamed");
    rename_command(window, fixture, 3);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "rename preserves VIEW and readonly mode");
    expect_renamed_files(fixture, &expected);
    require(controlled.requests == 0, "plain rename does not request image work");
    folio_window_destroy(window);
    note_text_destroy(edited);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #721: 実drawerのdragで保存先を移し、未保存本文とnative Undo/Redoを保つ。 */
static POINT move_row_point(struct folio_window *_Nonnull window, enum accessible_kind kind,
                            size_t category)
{
    struct accessible_item item = {.kind = kind,
                                   .ref = {.category = category, .note = 0},
                                   .revision = folio_window_accessible_revision(window)};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen &&
                !IsRectEmpty(&view.bounds),
            "drag uses the current visible public row bounds");
    POINT point = {.x = view.bounds.left + (view.bounds.right - view.bounds.left) / 2,
                   .y = view.bounds.top + (view.bounds.bottom - view.bounds.top) / 2};
    require(ScreenToClient(folio_window_accessible_window(window, ACCESSIBLE_DRAWER), &point),
            "public row coordinates map into the native drawer");
    return point;
}

static void drag_edit_note(struct folio_window *_Nonnull window, struct folio_state *_Nonnull state,
                           size_t from, size_t to)
{
    POINT source = move_row_point(window, ACCESSIBLE_NOTE, from);
    POINT target = move_row_point(window, ACCESSIBLE_CATEGORY, to);
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    /* Pointer press intentionally moves focus from the body to the root/tree. */
    before.focus = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM((WORD)source.x, (WORD)source.y));
    require(GetCapture() == drawer, "native row press captures the dedicated drawer");
    SendMessageW(drawer, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM((WORD)target.x, (WORD)target.y));
    SendMessageW(drawer, WM_LBUTTONUP, 0, MAKELPARAM((WORD)target.x, (WORD)target.y));
    pump(window);
    require(GetCapture() != drawer, "native row release ends drag capture");
    expect_native_body(window, &before);
    struct note_ref selected = {0};
    require(folio_state_selection(state, &selected) && selected.category == to &&
                selected.note == 0 && folio_state_note_count(state) == 1,
            "drag moves the selected note to the destination without duplicating it");
    struct pane_title_view title = folio_state_pane_title(state);
    require(title.any && strcmp(title.category, folio_state_category_name(state, to)) == 0 &&
                strcmp(title.note, "one") == 0,
            "the current document title follows the moved note");
}

static void expect_move_ledger(struct persistence_port disk, const char *_Nonnull category,
                               bool contains_note)
{
    struct note_ledger *ledger = nullptr;
    require(disk.read_note_ledger(disk.adapter, category, &ledger) == PERSISTENCE_LOADED &&
                ledger != nullptr,
            "both category ledgers are written after native drag");
    require(note_ledger_count(ledger) == (contains_note ? 1U : 0U),
            "each ledger has exactly the expected moved-note count");
    if (contains_note)
        require(strcmp(note_ledger_name(ledger, 0), "one") == 0,
                "the destination ledger contains the moved note name");
    note_ledger_destroy(ledger);
}

struct move_snapshot
{
    struct history_snapshot source;
    struct note_text *_Nullable target[note_history_depth + 1];
};

static void capture_move_history(struct native_fixture *_Nonnull fixture,
                                 struct move_snapshot *_Nonnull history)
{
    capture_history(fixture, &history->source);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *target = nullptr;
    require(disk.read_note(disk.adapter, "target", "one", &target) == PERSISTENCE_ABSENT &&
                target == nullptr,
            "dedicated move fixture starts without a target note");
    note_text_destroy(target);
    for (size_t version = 1; version <= note_history_depth; ++version)
    {
        struct history_version which = {.category = "target", .note = "one", .version = version};
        enum persistence_outcome read =
            disk.read_history(disk.adapter, &which, &history->target[version]);
        require(read == PERSISTENCE_LOADED || read == PERSISTENCE_ABSENT,
                "retain any target history from previous dedicated runs");
    }
}

static void expect_move_bytes(const struct note_text *_Nonnull actual,
                              const struct note_text *_Nonnull expected)
{
    size_t bytes = note_text_file_length(expected);
    require(note_text_file_length(actual) == bytes &&
                memcmp(note_text_file_bytes(actual), note_text_file_bytes(expected), bytes) == 0,
            "moved or stored file matches every expected byte");
}

static const struct note_text *_Nullable moved_target_version(
    const struct move_snapshot *_Nonnull history, bool saved, size_t version)
{
    if (!saved)
        return history->target[version];
    if (version == 1)
        return history->source.files[0];
    return history->target[version - 1];
}

static void expect_target_history(struct persistence_port disk,
                                  const struct move_snapshot *_Nonnull history, bool saved)
{
    for (size_t version = 1; version <= note_history_depth; ++version)
    {
        const struct note_text *expected = moved_target_version(history, saved, version);
        struct history_version which = {.category = "target", .note = "one", .version = version};
        struct note_text *text = nullptr;
        enum persistence_outcome read = disk.read_history(disk.adapter, &which, &text);
        if (expected == nullptr)
            require(read == PERSISTENCE_ABSENT && text == nullptr,
                    "target history has no unexpected version");
        else
        {
            require(read == PERSISTENCE_LOADED && text != nullptr,
                    "target history has each expected version");
            expect_move_bytes(text, expected);
        }
        note_text_destroy(text);
    }
}

static void expect_move_note(struct persistence_port disk, const char *_Nonnull category,
                             const struct note_text *_Nullable expected)
{
    struct note_text *text = nullptr;
    enum persistence_outcome read = disk.read_note(disk.adapter, category, "one", &text);
    if (expected == nullptr)
        require(read == PERSISTENCE_ABSENT && text == nullptr, "the old moved-note path is absent");
    else
    {
        require(read == PERSISTENCE_LOADED && text != nullptr, "the moved-note path is readable");
        expect_move_bytes(text, expected);
    }
    note_text_destroy(text);
}

static void expect_move_files(struct native_fixture *_Nonnull fixture,
                              const struct move_snapshot *_Nonnull history, bool moved,
                              bool target_saved)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    require(folio_state_store_revision(fixture->state) ==
                history->source.store_revision + (target_saved ? 1U : 0U),
            "only the explicit target save advances the store revision");
    struct note_text *saved = nullptr;
    require(note_text_create(switch_saved, strlen(switch_saved), &saved) == NOTE_TEXT_ACCEPTED,
            "valid moved-file save expectation");
    const struct note_text *current = target_saved ? saved : history->source.files[0];
    expect_move_note(disk, "schedule", moved ? nullptr : current);
    expect_move_note(disk, "target", moved ? current : nullptr);
    for (size_t version = 1; version <= note_history_depth; ++version)
        expect_history_file(disk, version, history->source.files[version]);
    expect_target_history(disk, history, target_saved);
    expect_move_ledger(disk, "schedule", !moved);
    expect_move_ledger(disk, "target", moved);
    note_text_destroy(saved);
}

static void save_moved_body(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture,
                            const struct move_snapshot *_Nonnull history)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetFocus(body);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    for (size_t attempt = 0; attempt < 2; ++attempt)
    {
        SendMessageW(body, WM_CHAR, 0x13, 1);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
        expect_move_files(fixture, history, true, true);
    }
}

static void expect_return_save(struct native_fixture *_Nonnull fixture,
                               const struct move_snapshot *_Nonnull history)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    require(folio_state_store_revision(fixture->state) == history->source.store_revision + 2,
            "one target save and one returned save publish exactly two revisions");
    struct note_text *previous = nullptr;
    require(note_text_create(switch_saved, strlen(switch_saved), &previous) == NOTE_TEXT_ACCEPTED,
            "valid previously saved moved text");
    expect_history_file(disk, 0, history->source.files[0]);
    expect_history_file(disk, 1, previous);
    for (size_t version = 2; version <= note_history_depth; ++version)
        expect_history_file(disk, version, history->source.files[version - 1]);
    expect_move_note(disk, "target", nullptr);
    expect_target_history(disk, history, true);
    note_text_destroy(previous);
}

static void save_returned_body(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture,
                               const struct move_snapshot *_Nonnull history)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetFocus(body);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    for (size_t attempt = 0; attempt < 2; ++attempt)
    {
        SendMessageW(body, WM_CHAR, 0x13, 1);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
        expect_return_save(fixture, history);
    }
}

static void move_edit_roundtrip(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct move_snapshot *_Nonnull history)
{
    drag_edit_note(window, fixture->state, 0, 1);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_move_files(fixture, history, true, false);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo survives the outward category drag");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_move_files(fixture, history, true, false);
    save_moved_body(window, fixture, history);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "return drag starts with native Redo");
    expect_move_files(fixture, history, true, true);
    drag_edit_note(window, fixture->state, 1, 0);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_move_files(fixture, history, false, true);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "Redo survives the return category drag");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_move_files(fixture, history, false, true);
    save_returned_body(window, fixture, history);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0,
            "Redo survives saving at the returned location");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    expect_return_save(fixture, history);
}

static void run_move_edit_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_note_count(fixture->state) == 1 &&
                folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
            "select the sole dedicated drag note");
    struct move_snapshot history = {0};
    capture_move_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real native drag window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) != 0, "outward drag starts with native Undo");
    verify_history_unchanged(fixture, &history.source);
    move_edit_roundtrip(window, fixture, &history);
    require(controlled.requests == 0, "plain drag note does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
    {
        note_text_destroy(history.source.files[version]);
        note_text_destroy(history.target[version]);
    }
}

static void run_other_fixture(struct native_fixture *_Nonnull fixture, int argc,
                              char *_Nonnull *_Nonnull argv)
{
    if (layout_case)
    {
        run_layout_case(fixture);
        return;
    }
    if (format_case)
    {
        run_format_case(fixture);
        return;
    }
    if (paste_case)
    {
        run_paste_case(fixture);
        return;
    }
    if (palette_case)
    {
        run_palette_case(fixture);
        return;
    }
    if (discard_case)
    {
        run_discard_case(fixture);
        return;
    }
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

/* #723/#727: Read exact bytes, including an existing zero-byte file, without decoding. */
static void expect_raw_file(struct native_fixture *_Nonnull fixture, const wchar_t *_Nonnull suffix,
                            const char *_Nonnull expected)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, suffix, path);
    struct file_bytes *bytes = nullptr;
    require(file_bytes_read(path, &bytes) == PERSISTENCE_LOADED && bytes != nullptr,
            "read the actual file without text interpretation");
    require(file_bytes_length(bytes) == strlen(expected) &&
                memcmp(file_bytes_data(bytes), expected, strlen(expected)) == 0,
            "raw file matches every literal expected byte");
    file_bytes_destroy(bytes);
}

static void expect_unicode_saved(struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history)
{
    expect_body_saved(fixture, history, unicode_saved);
    expect_raw_file(fixture, L"\\schedule\\one.md", unicode_saved);
    expect_raw_file(fixture, L"\\.history\\schedule\\one\\1.md", unicode_current);
}

static void unicode_same_save(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull history)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 6};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        SendMessageW(body, WM_CHAR, 0x13, 0);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, unicode_original, FOLIO_NOTE_SAME);
        verify_history_unchanged(fixture, history);
        expect_raw_file(fixture, L"\\schedule\\one.md", unicode_current);
    }
}

static void unicode_changed_save(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history)
{
    append_switch_text(window);
    expect_editor(window, fixture->state, unicode_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, history);
    expect_raw_file(fixture, L"\\schedule\\one.md", unicode_current);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selection = {.cpMin = 6, .cpMax = 9};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.undo != 0 && before.redo == 0 && before.focus == body,
            "Unicode insertion has native Undo and body focus");
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        SendMessageW(body, WM_CHAR, 0x13, 0);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, unicode_edited, FOLIO_NOTE_SAME);
        expect_unicode_saved(fixture, history);
    }
}

static void unicode_saved_roundtrip(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct history_snapshot *_Nonnull history)
{
    history_undo(window);
    expect_editor(window, fixture->state, unicode_original, FOLIO_NOTE_CHANGED);
    expect_unicode_saved(fixture, history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "Unicode insertion remains redoable after saving");
    pump(window);
    expect_editor(window, fixture->state, unicode_edited, FOLIO_NOTE_SAME);
    expect_unicode_saved(fixture, history);
    action(window, ACCESSIBLE_VIEW);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "saved Unicode document enters readonly View");
    expect_unicode_saved(fixture, history);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, unicode_edited, FOLIO_NOTE_SAME);
    require(GetFocus() == body, "Unicode document returns to the same editor");
    expect_unicode_saved(fixture, history);
}

static void run_unicode_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated Unicode note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    expect_raw_file(fixture, L"\\schedule\\one.md", unicode_current);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real Unicode save window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, unicode_original, FOLIO_NOTE_SAME);
    unicode_same_save(window, fixture, &history);
    unicode_changed_save(window, fixture, &history);
    unicode_saved_roundtrip(window, fixture, &history);
    require(controlled.requests == 0, "Unicode Markdown does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #725: NEW saves the departing note; first SAVE names the native draft without reloading it. */
static void expect_first_save_count(struct native_fixture *_Nonnull fixture, size_t count)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct name_list *names = nullptr;
    require(disk.scan_notes(disk.adapter, "schedule", &names) == PERSISTENCE_LOADED &&
                names != nullptr,
            "scan the dedicated first-save files");
    require(name_list_count(names) == count && folio_state_note_count(fixture->state) == count,
            "physical note files and public rows have the expected count");
    name_list_destroy(names);
}

static void first_save_departure(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history)
{
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    size_t count = folio_state_note_count(fixture->state);
    expect_first_save_count(fixture, count);
    struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &create) == S_OK,
            "invoke the public header New Note button");
    pump(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_document_category(fixture->state) == 0 &&
                !folio_state_selection(fixture->state, &selected) &&
                folio_state_document_name(fixture->state)[0] == '\0',
            "the blank draft has a category but no fictitious name or selected row");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(GetFocus() == body && SendMessageW(body, EM_CANUNDO, 0, 0) == 0 &&
                SendMessageW(body, EM_CANREDO, 0, 0) == 0 &&
                SendMessageW(body, EM_GETMODIFY, 0, 0) == 0,
            "New Note focuses a clean editor without the previous document's native history");
    expect_first_save_count(fixture, count);
    expect_body_saved(fixture, history, switch_saved);
}

static void expect_first_save_ledger(struct native_fixture *_Nonnull fixture,
                                     const char *_Nonnull name, size_t count)
{
    struct note_ref selected = {0};
    require(folio_state_selection(fixture->state, &selected) && selected.category == 0 &&
                strcmp(folio_state_note_name(fixture->state, selected), name) == 0,
            "the first saved note is the selected public row");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_ledger *ledger = nullptr;
    require(disk.read_note_ledger(disk.adapter, "schedule", &ledger) == PERSISTENCE_LOADED &&
                ledger != nullptr && note_ledger_count(ledger) == count,
            "first save publishes the complete note ledger");
    for (size_t index = 0; index < count; ++index)
        require(strcmp(note_ledger_name(ledger, index),
                       folio_state_note_name(fixture->state, (struct note_ref){0, index})) == 0,
                "all persisted note rows match the public order");
    note_ledger_destroy(ledger);
    expect_first_save_count(fixture, count);
}

static void first_save_command(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture, char name[64])
{
    size_t count = folio_state_note_count(fixture->state);
    int length = snprintf(name, 64, "zznew-%zu", count);
    require(length > 0 && length < 64, "dedicated first-save name fits");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *absent = nullptr;
    require(disk.read_note(disk.adapter, "schedule", name, &absent) == PERSISTENCE_ABSENT &&
                absent == nullptr,
            "first-save repeat never overwrites an older dedicated note");
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.modified != 0 && before.undo != 0 && before.redo == 0,
            "the draft has a real native insertion before first save");
    wchar_t command[96];
    require(swprintf(command, 96, L":w zznew-%zu", count) > 0, "first-save Ex command fits");
    submit_ex(window, command);
    require(!IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT)) &&
                folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_NAMED &&
                strcmp(folio_state_document_name(fixture->state), name) == 0,
            "successful first save names the draft and closes Ex");
    expect_name_operation_body(window, &before);
    require(SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_GETMODIFY, 0,
                         0) == before.modified,
            "first save preserves the native modified mark");
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
    expect_copy(fixture, name, first_save_bytes);
    expect_first_save_ledger(fixture, name, count + 1);
}

static void first_save_roundtrip(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull original,
                                 const char *_Nonnull name)
{
    history_undo(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    expect_copy(fixture, name, first_save_bytes);
    expect_save_as_original(fixture, original, 1);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "first save preserves Redo back to the saved draft");
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    before.focus = body;
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "explicitly return focus from Ex to the named draft");
    pump(window);
    expect_native_body(window, &before);
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        SendMessageW(body, WM_CHAR, 0x13, 0);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
        expect_copy(fixture, name, first_save_bytes);
        expect_save_as_original(fixture, original, 1);
    }
}

/* #737: Cancelling SAVE or NEW retains even an empty or redoable untitled document. */
static void cancel_untitled(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture,
                            const struct history_snapshot *_Nonnull original, bool departure)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK && GetFocus() == body,
            "open the untitled name modal from the actual editor focus");
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct name_list *source_files = nullptr;
    struct name_list *target_files = nullptr;
    require(disk.scan_notes(disk.adapter, "schedule", &source_files) == PERSISTENCE_LOADED &&
                source_files != nullptr &&
                disk.scan_notes(disk.adapter, "target", &target_files) == PERSISTENCE_LOADED &&
                target_files != nullptr,
            "snapshot both file sets before cancelling an untitled operation");
    size_t count = folio_state_note_count(fixture->state);
    start_name_dialog(window,
                      (struct name_dialog_request){.title = departure ? L"無題のノート" : L"保存",
                                                   .name = cancelled_name,
                                                   .discard = departure,
                                                   .action = NAME_DIALOG_CANCEL});
    if (departure)
    {
        struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                            .action = ACCESSIBLE_ACTION_INVOKE};
        require(folio_window_accessible_action(window, &create) == S_OK,
                "invoke New Note while the previous untitled document remains open");
    }
    else
        (void)native_key_input(window, 'S', FCONTROL);
    finish_name_dialog();
    pump(window);
    expect_native_body(window, &before);
    expect_editor(window, fixture->state, before.text, FOLIO_NOTE_CHANGED);
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_document_category(fixture->state) == 0 &&
                folio_state_document_name(fixture->state)[0] == '\0' &&
                !folio_state_selection(fixture->state, &selected) &&
                folio_state_note_count(fixture->state) == count,
            "cancellation keeps the untitled category without a name or selected fictitious row");
    verify_history_unchanged(fixture, original);
    expect_name_file_set(disk, "schedule", source_files);
    expect_name_file_set(disk, "target", target_files);
    expect_first_save_count(fixture, count);
    name_list_destroy(source_files);
    name_list_destroy(target_files);
}

static void first_save_cancel_edited(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture,
                                     const struct history_snapshot *_Nonnull original)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct native_body_snapshot inserted = {0};
    capture_native_body(window, &inserted);
    require(inserted.modified != 0 && inserted.undo != 0 && inserted.redo == 0 &&
                inserted.selection.cpMin < inserted.selection.cpMax,
            "edited untitled cancellation starts with native Undo and a nonempty selection");
    cancel_untitled(window, fixture, original, false);
    cancel_untitled(window, fixture, original, true);
    history_undo(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "undoing the insertion leaves an empty but redoable untitled document");
    cancel_untitled(window, fixture, original, true);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0,
            "native Redo survives cancellation of departure from the empty untitled document");
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, original);
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&inserted.selection);
}

static void discard_untitled(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull original)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct name_list *source_files = nullptr;
    struct name_list *target_files = nullptr;
    require(disk.scan_notes(disk.adapter, "schedule", &source_files) == PERSISTENCE_LOADED &&
                source_files != nullptr &&
                disk.scan_notes(disk.adapter, "target", &target_files) == PERSISTENCE_LOADED &&
                target_files != nullptr,
            "snapshot both file sets before approving Don't Save");
    size_t count = folio_state_note_count(fixture->state);
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = cancelled_name,
                                                           .discard = true,
                                                           .action = NAME_DIALOG_DISCARD});
    struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &create) == S_OK,
            "invoke New Note and explicitly approve discarding the old untitled document");
    finish_name_dialog();
    pump(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct native_body_snapshot after = {0};
    capture_native_body(window, &after);
    require(after.selection.cpMin == 0 && after.selection.cpMax == 0 && after.modified == 0 &&
                after.undo == 0 && after.redo == 0 && after.focus == body,
            "the next empty editor has initial caret and focus without discarded edit history");
    require(SendMessageW(body, EM_UNDO, 0, 0) == 0 && SendMessageW(body, EM_REDO, 0, 0) == 0,
            "actual Undo and Redo cannot restore the discarded untitled text");
    pump(window);
    expect_native_body(window, &after);
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_document_category(fixture->state) == 0 &&
                folio_state_document_name(fixture->state)[0] == '\0' &&
                !folio_state_selection(fixture->state, &selected) &&
                folio_state_note_count(fixture->state) == count,
            "the next untitled keeps NEW's category rather than the dialog's unused destination");
    verify_history_unchanged(fixture, original);
    expect_name_file_set(disk, "schedule", source_files);
    expect_name_file_set(disk, "target", target_files);
    expect_first_save_count(fixture, count);
    name_list_destroy(source_files);
    name_list_destroy(target_files);
}

/* #739: Don't Save starts a fresh native document, including when only Redo retains the text. */
static void first_save_discard(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture,
                               const struct history_snapshot *_Nonnull original)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    for (size_t pass = 0; pass < 2; ++pass)
    {
        SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)first_save_edited);
        pump(window);
        expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_CHANGED);
        require(GetFocus() == body && SendMessageW(body, EM_CANUNDO, 0, 0) != 0,
                "the untitled document owns real undoable text before Don't Save");
        if (pass == 1)
        {
            history_undo(window);
            expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
            require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
                    "the empty untitled document still holds the discarded candidate in Redo");
        }
        discard_untitled(window, fixture, original);
    }
}

static void run_first_save_case(struct native_fixture *_Nonnull fixture)
{
    require(strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, 0}), "one") == 0,
            "retained new notes follow the original in the dedicated fixture");
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the note to save before New Note");
    struct history_snapshot initial = {0};
    capture_history(fixture, &initial);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real New Note and first-save window");
    pump(window);
    first_save_departure(window, fixture, &initial);
    struct history_snapshot original = {0};
    capture_history(fixture, &original);
    cancel_untitled(window, fixture, &original, true);
    first_save_discard(window, fixture, &original);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)first_save_edited);
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &original);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 6};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    first_save_cancel_edited(window, fixture, &original);
    char name[64];
    first_save_command(window, fixture, name);
    expect_save_as_original(fixture, &original, 1);
    first_save_roundtrip(window, fixture, &original, name);
    require(controlled.requests == 0, "plain new Markdown does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
    {
        note_text_destroy(initial.files[version]);
        note_text_destroy(original.files[version]);
    }
}

/* #741: The real name dialog saves the existing native draft into another category. */
static void expect_modal_destination(struct native_fixture *_Nonnull fixture,
                                     const char *_Nonnull name,
                                     const struct name_list *_Nonnull previous)
{
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_NAMED &&
                folio_state_document_category(fixture->state) == 1 &&
                strcmp(folio_state_document_name(fixture->state), name) == 0 &&
                folio_state_selection(fixture->state, &selected) && selected.category == 1 &&
                strcmp(folio_state_note_name(fixture->state, selected), name) == 0,
            "the modal names and selects the first saved note in its chosen category");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct name_list *files = nullptr;
    size_t count = name_list_count(previous) + 1;
    require(disk.scan_notes(disk.adapter, "target", &files) == PERSISTENCE_LOADED &&
                files != nullptr && name_list_count(files) == count &&
                name_list_contains(files, name),
            "the target contains exactly one additional Markdown file");
    for (size_t index = 0; index < name_list_count(previous); ++index)
        require(name_list_contains(files, name_list_at(previous, index)),
                "every older destination filename is retained");
    struct note_ledger *ledger = nullptr;
    require(disk.read_note_ledger(disk.adapter, "target", &ledger) == PERSISTENCE_LOADED &&
                ledger != nullptr && note_ledger_count(ledger) == count,
            "the modal publishes the complete destination ledger");
    for (size_t index = 0; index < count; ++index)
        require(strcmp(note_ledger_name(ledger, index),
                       folio_state_note_name(fixture->state, (struct note_ref){1, index})) == 0 &&
                    name_list_contains(files, note_ledger_name(ledger, index)),
                "every persisted destination row matches the public order and a real file");
    name_list_destroy(files);
    note_ledger_destroy(ledger);
    expect_category_copy(fixture, "target", name, first_save_bytes);
}

static void expect_modal_cursor(struct native_fixture *_Nonnull fixture)
{
    enum folio_cursor_kind kind = FOLIO_CURSOR_NOTE;
    struct note_ref cursor = {0};
    require(folio_state_cursor(fixture->state, &kind, &cursor) && kind == FOLIO_CURSOR_CATEGORY &&
                cursor.category == 0,
            "first save does not steal the existing source category cursor");
}

struct modal_save_files
{
    struct history_snapshot original;
    struct name_list *source;
    struct name_list *target;
    struct note_ledger *source_ledger;
    struct note_ledger *target_ledger;
    size_t count;
    char name[64];
};

/* #743: A rejected destination keeps the same owned dialog and every unsaved byte. */
struct name_rejection_probe
{
    struct folio_window *window;
    struct native_fixture *fixture;
    const struct native_body_snapshot *body;
    const struct modal_save_files *files;
};

struct name_rejection_step
{
    const wchar_t *name;
    const wchar_t *category;
    const wchar_t *reason;
};

static void snapshot_modal_ledger(struct persistence_port disk, const char *_Nonnull category,
                                  struct note_ledger *_Nullable *_Nonnull out)
{
    enum persistence_outcome outcome = disk.read_note_ledger(disk.adapter, category, out);
    require((outcome == PERSISTENCE_LOADED && *out != nullptr) ||
                (outcome == PERSISTENCE_ABSENT && *out == nullptr),
            "snapshot the category ledger or its initial absence before rejected names");
}

static void expect_rejected_ledger(struct native_fixture *_Nonnull fixture, size_t category,
                                   const struct note_ledger *_Nullable before)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    const char *name = folio_state_category_name(fixture->state, category);
    struct note_ledger *after = nullptr;
    enum persistence_outcome outcome = disk.read_note_ledger(disk.adapter, name, &after);
    if (before == nullptr)
        require(outcome == PERSISTENCE_ABSENT && after == nullptr,
                "rejection does not create an initially absent category ledger");
    else
    {
        require(outcome == PERSISTENCE_LOADED && after != nullptr &&
                    note_ledger_count(after) == note_ledger_count(before),
                "rejection preserves the category ledger's complete row count");
        for (size_t index = 0; index < note_ledger_count(before); ++index)
            require(strcmp(note_ledger_name(after, index), note_ledger_name(before, index)) == 0 &&
                        strcmp(folio_state_note_name(fixture->state,
                                                     (struct note_ref){category, index}),
                               note_ledger_name(before, index)) == 0,
                    "rejection preserves every persisted and public row in its original order");
    }
    note_ledger_destroy(after);
}

static void expect_rejected_draft(const struct name_rejection_probe *_Nonnull probe)
{
    struct native_fixture *fixture = probe->fixture;
    expect_name_operation_body(probe->window, probe->body);
    HWND body = folio_window_accessible_window(probe->window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) == probe->body->modified,
            "a rejected destination keeps the native modified flag");
    expect_editor(probe->window, fixture->state, probe->body->text, FOLIO_NOTE_CHANGED);
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_document_category(fixture->state) == 0 &&
                folio_state_document_name(fixture->state)[0] == '\0' &&
                !folio_state_selection(fixture->state, &selected) &&
                folio_state_note_count(fixture->state) == probe->files->count,
            "rejection retains the unnamed document and adds no selected or public row");
    expect_modal_cursor(fixture);
    verify_history_unchanged(fixture, &probe->files->original);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_name_file_set(disk, "schedule", probe->files->source);
    expect_name_file_set(disk, "target", probe->files->target);
    expect_rejected_ledger(fixture, 0, probe->files->source_ledger);
    expect_rejected_ledger(fixture, 1, probe->files->target_ledger);
}

static void submit_rejected_name(HWND dialog, const struct name_rejection_step *_Nonnull step,
                                 const struct name_rejection_probe *_Nonnull probe)
{
    HWND name = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    HWND categories = FindWindowExW(dialog, nullptr, L"LISTBOX", nullptr);
    HWND reason = FindWindowExW(dialog, name, L"EDIT", nullptr);
    require(IsWindow(name) && IsWindow(categories) && IsWindow(reason) &&
                (GetWindowLongPtrW(reason, GWL_STYLE) & (ES_READONLY | ES_MULTILINE)) ==
                    (ES_READONLY | ES_MULTILINE),
            "the rejection reason is the owned readonly multiline field");
    LRESULT category =
        SendMessageW(categories, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)step->category);
    require(SetWindowTextW(name, step->name) && category != LB_ERR &&
                SendMessageW(categories, LB_SETCURSEL, (WPARAM)category, 0) == category,
            "enter the exact rejected name and its intended category");
    accept_name_dialog(dialog);
    require(
        IsWindow(dialog) && IsWindowVisible(dialog) &&
            GetAncestor(dialog, GA_ROOTOWNER) == name_dialog.root && GetFocus() == name &&
            IsWindowEnabled(name_dialog.root) && !IsWindowEnabled(name_dialog.body) &&
            !IsWindowEnabled(name_dialog.drawer) && IsWindowEnabled(name) &&
            IsWindowEnabled(categories) && IsWindowEnabled(GetDlgItem(dialog, IDOK)) &&
            IsWindowEnabled(GetDlgItem(dialog, IDCANCEL)),
        "rejection keeps this same modal open, editable and focused while content stays disabled");
    wchar_t value[256] = {0};
    int length = GetWindowTextW(name, value, 256);
    require(length >= 0 && (size_t)length == wcslen(step->name) && wcscmp(value, step->name) == 0 &&
                SendMessageW(categories, LB_GETCURSEL, 0, 0) == category,
            "rejection retains all entered name units and the selected category");
    require(IsWindowVisible(reason) && GetWindowTextW(reason, value, 256) > 0 &&
                wcscmp(value, step->reason) == 0,
            "the visible inline reason matches the independent expected message");
    expect_rejected_draft(probe);
}

static void retry_name_dialog(HWND dialog, const struct name_rejection_probe *_Nonnull probe)
{
    const struct name_rejection_step steps[] = {
        {.name = L"", .category = L"target", .reason = L"名前を入力してください。"},
        {.name = L"CON",
         .category = L"target",
         .reason = L"使えない名前です。予約名・末尾の空白やピリオド・区切りを避け、"
                   L".mdを含め255バイト以内で指定してください。"},
        {.name = L"one",
         .category = L"schedule",
         .reason = L"同じ名前のノートがあります。別の名前を指定してください。"
                   L"既存ファイルは変更していません。"}};
    for (size_t index = 0; index < sizeof steps / sizeof steps[0]; ++index)
        submit_rejected_name(dialog, &steps[index], probe);
    enter_pending_destination(dialog);
}

static void modal_save_roundtrip(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct modal_save_files *_Nonnull files)
{
    history_undo(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    expect_modal_destination(fixture, files->name, files->target);
    expect_save_as_original(fixture, &files->original, 1);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "modal first save preserves actual native Redo to the saved draft");
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.focus == body, "Undo and Redo keep focus in the saved editor");
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        (void)native_key_input(window, 'S', FCONTROL);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
        expect_modal_destination(fixture, files->name, files->target);
        expect_save_as_original(fixture, &files->original, 1);
        expect_modal_cursor(fixture);
    }
}

static void modal_save_draft(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct modal_save_files *_Nonnull files)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &create) == S_OK, "invoke the real New button");
    pump(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_document_category(fixture->state) == 0 &&
                folio_state_document_name(fixture->state)[0] == '\0' &&
                !folio_state_selection(fixture->state, &selected),
            "New creates only an unnamed draft in the source category");
    verify_history_unchanged(fixture, &files->original);
    expect_name_file_set(disk, "schedule", files->source);
    expect_name_file_set(disk, "target", files->target);
    require(folio_state_focus_cursor(fixture->state, FOLIO_CURSOR_CATEGORY,
                                     (struct note_ref){0, 0}) == FOLIO_STATE_READY,
            "establish a source category cursor in the new draft before first save");
    expect_modal_cursor(fixture);
}

static void modal_save_accept(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              struct modal_save_files *_Nonnull files)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)first_save_edited);
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_CHANGED);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 6};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.modified != 0 && before.undo != 0 && before.redo == 0 && before.focus == body &&
                before.selection.cpMin < before.selection.cpMax,
            "the draft has a real native insertion, nonempty selection and editor focus");
    wchar_t input[64];
    int length = snprintf(files->name, 64, "zzmodal 日本語 🐾-%zu", files->count);
    int units = swprintf(input, 64, L"zzmodal 日本語 🐾-%zu", files->count);
    require(length > 0 && length < 64 && units > 0 && units < 64,
            "independent UTF-8 and UTF-16 destination literals fit");
    struct note_text *absent = nullptr;
    require(disk.read_note(disk.adapter, "target", files->name, &absent) == PERSISTENCE_ABSENT &&
                absent == nullptr,
            "the name dialog never overwrites an older first-save note");
    const struct name_rejection_probe rejection = {
        .window = window, .fixture = fixture, .body = &before, .files = files};
    start_name_dialog(window, (struct name_dialog_request){.title = L"保存",
                                                           .name = input,
                                                           .action = NAME_DIALOG_RETRY_SAVE,
                                                           .rejection = &rejection});
    (void)native_key_input(window, 'S', FCONTROL);
    finish_name_dialog();
    pump(window);
    expect_native_body(window, &before);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
    expect_modal_destination(fixture, files->name, files->target);
    expect_modal_cursor(fixture);
    require(folio_state_note_count(fixture->state) == files->count + 1,
            "first save adds exactly one public note");
    expect_save_as_original(fixture, &files->original, 1);
    expect_name_file_set(disk, "schedule", files->source);
}

static struct folio_window *_Nonnull begin_modal_save(struct native_fixture *_Nonnull fixture,
                                                      struct modal_save_files *_Nonnull files)
{
    require(strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, 0}), "one") == 0 &&
                strcmp(folio_state_category_name(fixture->state, 1), "target") == 0 &&
                folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY,
            "select the source beside the dedicated destination category");
    capture_history(fixture, &files->original);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    require(disk.scan_notes(disk.adapter, "schedule", &files->source) == PERSISTENCE_LOADED &&
                files->source != nullptr &&
                disk.scan_notes(disk.adapter, "target", &files->target) == PERSISTENCE_LOADED &&
                files->target != nullptr,
            "snapshot both categories before creating an unsaved draft");
    files->count = folio_state_note_count(fixture->state);
    require(files->count == name_list_count(files->source) + name_list_count(files->target),
            "the fixture has exactly the two snapshotted categories");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real first-save name dialog host");
    pump(window);
    modal_save_draft(window, fixture, files);
    snapshot_modal_ledger(disk, "schedule", &files->source_ledger);
    snapshot_modal_ledger(disk, "target", &files->target_ledger);
    return window;
}

static void release_modal_save(struct modal_save_files *_Nonnull files)
{
    name_list_destroy(files->source);
    name_list_destroy(files->target);
    note_ledger_destroy(files->source_ledger);
    note_ledger_destroy(files->target_ledger);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(files->original.files[version]);
}

static void run_first_save_modal_case(struct native_fixture *_Nonnull fixture)
{
    struct modal_save_files files = {0};
    struct folio_window *window = begin_modal_save(fixture, &files);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    modal_save_accept(window, fixture, &files);
    modal_save_roundtrip(window, fixture, &files);
    expect_name_file_set(disk, "schedule", files.source);
    require(folio_state_note_count(fixture->state) == files.count + 1 && controlled.requests == 0,
            "SAME saves add no public notes and plain Markdown requests no image work");
    folio_window_destroy(window);
    release_modal_save(&files);
}

/* #727: An empty named document is saved, archived and reloaded as a real document. */
static void expect_empty_saved(struct native_fixture *_Nonnull fixture,
                               const struct history_snapshot *_Nonnull history)
{
    struct note_ref selected = {0};
    require(folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_NAMED &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                folio_state_selection(fixture->state, &selected) && selected.category == 0 &&
                strcmp(folio_state_note_name(fixture->state, selected), "one") == 0,
            "empty save retains the named and selected document");
    expect_body_saved(fixture, history, "");
    expect_raw_file(fixture, L"\\schedule\\one.md", "");
    expect_raw_file(fixture, L"\\.history\\schedule\\one\\1.md", history_current);
}

static void empty_save_twice(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull history)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.text[0] == L'\0' && before.selection.cpMin == 0 && before.selection.cpMax == 0 &&
                before.focus == body,
            "empty save starts with empty native text, collapsed selection and body focus");
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        SendMessageW(body, WM_CHAR, 0x13, 0);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, L"", FOLIO_NOTE_SAME);
        expect_empty_saved(fixture, history);
    }
}

static void empty_saved_roundtrip(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture,
                                  const struct history_snapshot *_Nonnull history)
{
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_empty_saved(fixture, history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "the complete deletion remains redoable after saving");
    pump(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_SAME);
    expect_empty_saved(fixture, history);
    action(window, ACCESSIBLE_VIEW);
    struct native_body_snapshot view = {0};
    capture_native_body(window, &view);
    require(view.text[0] == L'\0' && folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "empty saved document enters readonly View with no placeholder text");
    expect_empty_saved(fixture, history);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_SAME);
    require(IsWindowEnabled(body) && GetFocus() == body,
            "empty document is enabled and focused after Edit reload");
    empty_save_twice(window, fixture, history);
}

static void run_empty_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated empty-save note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    expect_raw_file(fixture, L"\\schedule\\one.md", history_current);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real empty-save window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE all = {.cpMin = 0, .cpMax = -1};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&all);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"");
    pump(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    struct native_body_snapshot deleted = {0};
    capture_native_body(window, &deleted);
    require(deleted.modified != 0 && deleted.undo != 0 && deleted.redo == 0,
            "full deletion is a modified native Undo unit");
    verify_history_unchanged(fixture, &history);
    expect_raw_file(fixture, L"\\schedule\\one.md", history_current);
    empty_save_twice(window, fixture, &history);
    empty_saved_roundtrip(window, fixture, &history);
    require(controlled.requests == 0, "empty body does not request image work");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #729: Observe only this fixture's native failure dialog; never close unrelated windows. */
struct failure_probe
{
    HWND root;
    UINT_PTR timer;
    unsigned ticks;
    unsigned closed;
    const wchar_t *expected;
};

static struct failure_probe failure;

static BOOL CALLBACK close_history_failure(HWND dialog, LPARAM parameter)
{
    wchar_t kind[32];
    if (GetAncestor(dialog, GA_ROOTOWNER) != (HWND)parameter || !IsWindowVisible(dialog) ||
        GetClassNameW(dialog, kind, 32) == 0 || wcscmp(kind, L"#32770") != 0)
        return TRUE;
    HWND text = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    HWND accept = GetDlgItem(dialog, IDOK);
    wchar_t message[512];
    require(text != nullptr && accept != nullptr && IsWindowEnabled(accept) &&
                GetWindowTextW(text, message, 512) > 0 &&
                wcsstr(message, failure.expected) != nullptr,
            "the fixture's themed failure dialog reports the expected refusal");
    require(KillTimer(nullptr, failure.timer), "stop the owned failure observer timer");
    if (motion_check_modal)
    {
        require(!KillTimer(failure.root, 86), "native modal already destroyed the motion timer");
        SendMessageW(failure.root, WM_TIMER, 86, 0);
        motion_saw_modal = true;
    }
    failure.timer = 0;
    ++failure.closed;
    SendMessageW(accept, BM_CLICK, 0, 0);
    return FALSE;
}

static void CALLBACK observe_history_failure(HWND window, UINT message, UINT_PTR timer,
                                             DWORD tick_millis)
{
    (void)window;
    (void)message;
    (void)tick_millis;
    if (failure.timer == 0)
        return;
    require(timer == failure.timer && ++failure.ticks <= 100,
            "the owned failure observer stays within its finite deadline");
    EnumThreadWindows(GetCurrentThreadId(), close_history_failure, (LPARAM)failure.root);
}

static HANDLE lock_native_note(struct native_fixture *_Nonnull fixture)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\one.md", path);
    HANDLE locked =
        CreateFileW(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(locked != INVALID_HANDLE_VALUE, "hold only the dedicated note without sharing");
    return locked;
}

static void begin_expected_failure(struct folio_window *_Nonnull window,
                                   const wchar_t *_Nonnull expected)
{
    failure = (struct failure_probe){
        .root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW), .expected = expected};
    failure.timer = SetTimer(nullptr, 0, 50, observe_history_failure);
    require(failure.timer != 0, "start the owned failure observer timer");
}

static void begin_history_failure(struct folio_window *_Nonnull window)
{
    begin_expected_failure(window, L"履歴を書けなかったので保存していません。"
                                   L"編集中の本文は残っています。");
}

static void end_history_failure(void)
{
    if (failure.timer != 0)
    {
        require(KillTimer(nullptr, failure.timer), "cancel an unused failure observer");
        failure.timer = 0;
    }
}

static void expect_locked_target(const struct folio_state *_Nonnull state)
{
    struct note_ref selected = {0};
    require(folio_state_document_kind(state) == FOLIO_DOCUMENT_NAMED &&
                strcmp(folio_state_document_name(state), "one") == 0 &&
                folio_state_selection(state, &selected) && selected.category == 0 &&
                strcmp(folio_state_note_name(state, selected), "one") == 0,
            "save refusal and retry retain the named document and selected row");
}

/* #745: A busy source is refused before either Markdown or history can be recycled. */
struct trash_refusal_snapshot
{
    struct native_body_snapshot body;
    enum pane_mode mode;
    LONG_PTR read_only;
    size_t count;
    struct name_list *files;
    struct name_list *public_names;
    struct note_ledger *ledger;
};

static void capture_trash_refusal(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture,
                                  struct trash_refusal_snapshot *_Nonnull snapshot)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 2, .cpMax = 7};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    capture_native_body(window, &snapshot->body);
    require(snapshot->body.selection.cpMin < snapshot->body.selection.cpMax,
            "trash refusal starts with a nonempty native selection");
    snapshot->mode = folio_state_pane_mode(fixture->state);
    snapshot->read_only = GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY;
    snapshot->count = folio_state_note_count(fixture->state);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    require(disk.scan_notes(disk.adapter, "schedule", &snapshot->files) == PERSISTENCE_LOADED &&
                snapshot->files != nullptr,
            "snapshot the source category's complete file set before trash refusal");
    snapshot_modal_ledger(disk, "schedule", &snapshot->ledger);
    require(name_list_create(&snapshot->public_names) == NAME_LIST_ACCEPTED &&
                snapshot->public_names != nullptr,
            "snapshot the public source rows independently of other categories");
    for (size_t index = 0; index < name_list_count(snapshot->files); ++index)
    {
        const char *name = folio_state_note_name(fixture->state, (struct note_ref){0, index});
        require(name_list_contains(snapshot->files, name) &&
                    name_list_append(snapshot->public_names, name, strlen(name)) ==
                        NAME_LIST_ACCEPTED,
                "snapshot each source row in public order with its existing file");
    }
}

static void expect_trash_retained(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture,
                                  const struct trash_refusal_snapshot *_Nonnull snapshot)
{
    expect_name_operation_body(window, &snapshot->body);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_WINDOW)) &&
                IsWindowEnabled(body) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_TREE)),
            "trash refusal restores enabled input to the root, body and tree");
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) == snapshot->body.modified &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) == snapshot->read_only &&
                folio_state_pane_mode(fixture->state) == snapshot->mode,
            "trash refusal retains the native modified flag, read-only state and pane mode");
    require(folio_state_unsaved(fixture->state, switch_original, wcslen(switch_original)) ==
                    FOLIO_NOTE_SAME &&
                folio_state_note_count(fixture->state) == snapshot->count,
            "trash refusal retains the saved document content and total public count");
    expect_locked_target(fixture->state);
    if (snapshot->mode == PANE_MODE_EDIT)
        expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_name_file_set(disk, "schedule", snapshot->files);
    expect_rejected_ledger(fixture, 0, snapshot->ledger);
    for (size_t index = 0; index < name_list_count(snapshot->public_names); ++index)
        require(strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, index}),
                       name_list_at(snapshot->public_names, index)) == 0,
                "every source row retains its order even when its ledger was absent");
}

static void expect_recycle_blocked(struct native_fixture *_Nonnull fixture)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\one.md", path);
    HANDLE attempted =
        CreateFileW(path, DELETE | FILE_READ_ATTRIBUTES, 0, nullptr, OPEN_EXISTING,
                    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    DWORD error = GetLastError();
    if (attempted != INVALID_HANDLE_VALUE)
        require(CloseHandle(attempted), "release an unexpectedly admitted preflight handle");
    require(attempted == INVALID_HANDLE_VALUE && error == ERROR_SHARING_VIOLATION,
            "the canonical recycle preflight is blocked by sharing before any move");
}

static void expect_trash_busy_surface(struct folio_window *_Nonnull window)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    wchar_t query[64];
    require(IsWindowVisible(input) && GetFocus() == input && GetWindowTextW(input, query, 64) > 0 &&
                wcscmp(query, L":trashnote") == 0,
            "busy trash retains the exact Ex command and focus for correction");
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_STATUS,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view status = {0};
    require(folio_window_accessible_read(window, &item, &status) == S_OK && !status.offscreen &&
                wcsstr(status.name, L"ほかのプログラムが使用中のため、ノートを移していません。") !=
                    nullptr,
            "busy trash reports its independent inline refusal reason");
}

static void refused_native_trash(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history)
{
    struct trash_refusal_snapshot snapshot = {0};
    capture_trash_refusal(window, fixture, &snapshot);
    HANDLE locked = lock_native_note(fixture);
    expect_recycle_blocked(fixture);
    submit_ex(window, L":trashnote");
    require(CloseHandle(locked), "release the source only after the real trash refusal returns");
    expect_trash_busy_surface(window);
    expect_trash_retained(window, fixture, &snapshot);
    verify_history_unchanged(fixture, history);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SendMessageW(input, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    require(!IsWindowVisible(input) && GetFocus() != input,
            "Escape dismisses busy trash without leaving focus in the hidden Ex input");
    expect_trash_retained(window, fixture, &snapshot);
    verify_history_unchanged(fixture, history);
    note_ledger_destroy(snapshot.ledger);
    name_list_destroy(snapshot.public_names);
    name_list_destroy(snapshot.files);
}

static void trash_refusal_roundtrip(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct history_snapshot *_Nonnull history)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "the first busy trash command starts in the real read-only view");
    refused_native_trash(window, fixture, history);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "the second busy trash starts with a real redoable edit and SAME content");
    refused_native_trash(window, fixture, history);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0,
            "the original input remains redoable after busy trash and Ex dismissal");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
}

static void refused_native_save(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct history_snapshot *_Nonnull history)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.modified != 0 && before.undo != 0 && before.redo == 0,
            "save refusal starts with a real native edit and Undo");
    HANDLE locked = lock_native_note(fixture);
    begin_history_failure(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, WM_CHAR, 0x13, 0);
    end_history_failure();
    require(CloseHandle(locked), "release the dedicated note's exclusive handle");
    pump(window);
    require(failure.closed == 1, "saving a locked original reports exactly one history failure");
    expect_locked_target(fixture->state);
    expect_native_body(window, &before);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, history);
    expect_raw_file(fixture, L"\\schedule\\one.md", history_current);
}

static void retry_locked_save(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull history)
{
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "the unsaved edit remains redoable after the refusal");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, history);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        SendMessageW(body, WM_CHAR, 0x13, 0);
        pump(window);
        expect_native_body(window, &before);
        expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
        expect_locked_target(fixture->state);
        expect_body_saved(fixture, history, switch_saved);
        expect_raw_file(fixture, L"\\schedule\\one.md", switch_saved);
        expect_raw_file(fixture, L"\\.history\\schedule\\one\\1.md", history_current);
    }
}

/* #749: A failed normal close keeps this window and can later complete its original save. */
static void pump_refused_close(struct folio_window *_Nonnull window, HWND root)
{
    MSG message;
    for (unsigned count = 0; count < 1000 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE);
         ++count)
    {
        require(message.message != WM_QUIT && IsWindow(root),
                "a refused normal close never publishes quit or destroys the root");
        if (message.message == WM_TIMER)
            continue;
        if (!folio_window_translate(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    require(IsWindow(root) && folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == root &&
                !PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE),
            "the exact root remains alive without a hidden quit after close refusal");
}

static void refused_normal_close(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 2, .cpMax = 7};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    size_t total = folio_state_note_count(fixture->state);
    require(IsWindow(root), "normal close starts with the existing live root");
    require(before.focus == body, "normal-close fixture starts with focus in its native body");
    require(before.redo != 0 && before.selection.cpMin < before.selection.cpMax,
            "normal close starts with redoable input and a nonempty selection");
    HANDLE locked = lock_native_note(fixture);
    begin_history_failure(window);
    SendMessageW(root, WM_CLOSE, 0, 0);
    pump_refused_close(window, root);
    end_history_failure();
    require(CloseHandle(locked), "release only this source after normal-close refusal returns");
    require(failure.closed == 1, "normal close reports one owned history failure");
    expect_native_body(window, &before);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_locked_target(fixture->state);
    require(IsWindowEnabled(root) && IsWindow(body) && IsWindowEnabled(body) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_TREE)) &&
                folio_state_note_count(fixture->state) == total,
            "close refusal restores enabled input and retains the public note count");
    verify_history_unchanged(fixture, history);
    expect_raw_file(fixture, L"\\schedule\\one.md", switch_saved);
}

static void close_busy_roundtrip(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "return to the existing editor before requesting normal close");
    pump(window);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    refused_normal_close(window, fixture, &history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "the same native redo remains usable after close refusal");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &history);
    close_theme_window(window);
    expect_body_saved(fixture, &history, history_current);
    expect_raw_file(fixture, L"\\schedule\\one.md", history_current);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void run_locked_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated locked-save note");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real locked-save window");
    pump(window);
    trash_refusal_roundtrip(window, fixture, &history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    verify_history_unchanged(fixture, &history);
    refused_native_save(window, fixture, &history);
    retry_locked_save(window, fixture, &history);
    close_busy_roundtrip(window, fixture);
    require(controlled.requests == 0 && failure.timer == 0,
            "plain locked-save case leaves no image requests or observer timer");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #747: Index focus saves through the real shortcut and retains failed native input. */
struct filter_save_snapshot
{
    struct native_body_snapshot body;
    size_t matches;
    size_t total;
};

static void finish_filter_work(struct folio_window *_Nonnull window,
                               const struct folio_state *_Nonnull state)
{
    for (size_t step = 0; step < 1000 && folio_window_idle_work_ready(window); ++step)
    {
        folio_window_continue_idle_work(window);
        pump(window);
    }
    require(!folio_state_index_filter_pending(state),
            "bounded public idle work finishes the query");
}

static void expect_filter_terms(struct folio_window *_Nonnull window,
                                const struct folio_state *_Nonnull state, size_t matches)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    wchar_t term[16];
    require(GetWindowTextW(input, term, 16) == 2 && wcscmp(term, L"追記") == 0 &&
                strcmp(folio_state_index_filter_term(state), "追記") == 0 &&
                strcmp(folio_state_search_term(state), "Current") == 0,
            "global and local search terms retain their separate owners");
    require(!folio_state_index_filter_pending(state) && folio_state_filtering(state) &&
                folio_state_index_filter_count(state) == matches,
            "the completed global result reflects only saved content");
}

static void prepare_filter_save(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                struct filter_save_snapshot *_Nonnull snapshot)
{
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                strstr(history_current, "追記") == nullptr &&
                strstr(switch_saved, "追記") != nullptr,
            "the saved fixture does not yet match the later edit's independent query");
    require(folio_state_set_search_term(fixture->state, u"Current", 7) == FOLIO_STATE_READY,
            "seed a distinct note-local term through its public owner");
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_FILTER},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "focus the real global query while still viewing");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    require(GetFocus() == input && SetWindowTextW(input, L"追記"), "enter the native index query");
    pump(window);
    finish_filter_work(window, fixture->state);
    snapshot->matches = folio_state_index_filter_count(fixture->state);
    snapshot->total = folio_state_note_count(fixture->state);
    expect_filter_terms(window, fixture->state, snapshot->matches);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    capture_native_body(window, &snapshot->body);
    require(snapshot->body.focus == body && snapshot->body.modified != 0 &&
                snapshot->body.undo != 0 && snapshot->body.redo == 0 &&
                snapshot->body.selection.cpMin < snapshot->body.selection.cpMax,
            "filter save starts with real native edits, selection and Undo");
}

static void refused_filter_save(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct history_snapshot *_Nonnull history,
                                const struct filter_save_snapshot *_Nonnull snapshot)
{
    HANDLE locked = lock_native_note(fixture);
    begin_history_failure(window);
    bool translated = native_key_input(window, 'F', FCONTROL | FSHIFT);
    end_history_failure();
    require(CloseHandle(locked), "release the dedicated source after the real focus refusal");
    pump(window);
    require(translated && failure.closed == 1,
            "Ctrl+Shift+F reports one owned history failure without modal reentry");
    expect_native_body(window, &snapshot->body);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_locked_target(fixture->state);
    require(IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_WINDOW)) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_BODY)) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_TREE)) &&
                folio_state_note_count(fixture->state) == snapshot->total,
            "failure restores enabled input and preserves the public note count");
    expect_filter_terms(window, fixture->state, snapshot->matches);
    verify_history_unchanged(fixture, history);
    expect_raw_file(fixture, L"\\schedule\\one.md", history_current);
}

static void retry_filter_save(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct history_snapshot *_Nonnull history,
                              const struct filter_save_snapshot *_Nonnull snapshot)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    struct native_body_snapshot focused = snapshot->body;
    focused.focus = input;
    for (size_t repeat = 0; repeat < 2; ++repeat)
    {
        require(native_key_input(window, 'F', FCONTROL | FSHIFT),
                "translate the real filter shortcut");
        finish_filter_work(window, fixture->state);
        DWORD start = 0;
        DWORD end = 0;
        SendMessageW(input, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
        require(GetFocus() == input && start == 0 && end == 2,
                "successful focus selects the full retained global term");
        expect_native_body(window, &focused);
        expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
        expect_locked_target(fixture->state);
        require(folio_state_note_count(fixture->state) == snapshot->total,
                "focus save keeps the total public note count");
        expect_filter_terms(window, fixture->state, snapshot->matches + 1);
        expect_body_saved(fixture, history, switch_saved);
        expect_raw_file(fixture, L"\\schedule\\one.md", switch_saved);
        struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                           .action = ACCESSIBLE_ACTION_FOCUS};
        require(folio_window_accessible_action(window, &focus) == S_OK,
                "return focus to the existing editor without reloading its body");
        pump(window);
        expect_native_body(window, &snapshot->body);
    }
}

static void undo_filter_save(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             const struct history_snapshot *_Nonnull history, size_t matches)
{
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_body_saved(fixture, history, switch_saved);
    expect_filter_terms(window, fixture->state, matches);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "filter focus saves retain the original redoable native edit");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_body_saved(fixture, history, switch_saved);
    expect_filter_terms(window, fixture->state, matches);
}

static void run_filter_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated filter-save source");
    expect_locked_target(fixture->state);
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real filter-save window");
    pump(window);
    struct filter_save_snapshot snapshot = {0};
    prepare_filter_save(window, fixture, &snapshot);
    verify_history_unchanged(fixture, &history);
    refused_filter_save(window, fixture, &history, &snapshot);
    retry_filter_save(window, fixture, &history, &snapshot);
    undo_filter_save(window, fixture, &history, snapshot.matches + 1);
    require(controlled.requests == 0 && failure.timer == 0,
            "filter save leaves no image work or observer timer");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #751: Only the dedicated root receives session notifications; no OS session is ended. */
static void expect_session_live(struct folio_window *_Nonnull window, HWND root, bool enabled)
{
    MSG message;
    require(IsWindow(root) && folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == root &&
                (IsWindowEnabled(root) != 0) == enabled &&
                folio_window_accessible_ready(window) == enabled,
            "session negotiation retains the same root and the expected input availability");
    require(!PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE),
            "session negotiation does not publish quit before approved final destruction");
}

static void expect_session_notice(struct folio_window *_Nonnull window,
                                  const wchar_t *_Nonnull reason, bool blocked)
{
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view status = {0};
    require(folio_window_accessible_read(window, &item, &status) == S_OK && !status.offscreen,
            "session status is exposed through the existing notice");
    require((wcsstr(status.name, reason) != nullptr) == blocked,
            "session save refusal has its independent reason and cancellation clears it");
}

static void refused_session_save(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history,
                                 const struct native_body_snapshot *_Nonnull before)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    size_t total = folio_state_note_count(fixture->state);
    HANDLE locked = lock_native_note(fixture);
    require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == FALSE,
            "a named save sharing violation refuses the session synchronously without a modal");
    expect_session_live(window, root, true);
    expect_session_notice(window, L"保存処理が未完了です。保存を再試行してください。", true);
    expect_native_body(window, before);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_locked_target(fixture->state);
    SendMessageW(root, WM_ENDSESSION, TRUE, (LPARAM)ENDSESSION_LOGOFF);
    expect_session_live(window, root, true);
    expect_native_body(window, before);
    expect_session_notice(window, L"保存処理が未完了です。保存を再試行してください。", true);
    SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
    expect_session_live(window, root, true);
    expect_session_notice(window, L"保存処理が未完了です。保存を再試行してください。", false);
    expect_native_body(window, before);
    require(folio_state_note_count(fixture->state) == total,
            "refused and cancelled session notifications retain all existing note rows");
    require(CloseHandle(locked), "release only the dedicated note after rejected notifications");
    verify_history_unchanged(fixture, history);
    expect_raw_file(fixture, L"\\schedule\\one.md", history_current);
}

static void approve_session_save(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture,
                                 const struct history_snapshot *_Nonnull history,
                                 const struct native_body_snapshot *_Nonnull before)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    for (unsigned repeat = 0; repeat < 2; ++repeat)
    {
        require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == TRUE,
                "successful named save and duplicate session query approve termination");
        expect_session_live(window, root, false);
        require(native_key_input(window, 'X', 0),
                "the real message translator consumes body input while termination is approved");
        struct native_body_snapshot frozen = *before;
        frozen.focus = GetFocus();
        expect_native_body(window, &frozen);
        expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
        expect_locked_target(fixture->state);
        expect_body_saved(fixture, history, switch_saved);
    }
    SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
    expect_session_live(window, root, true);
    expect_session_notice(window, L"保存処理が未完了です。保存を再試行してください。", false);
    expect_native_body(window, before);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_body_saved(fixture, history, switch_saved);
}

static void finish_session_save(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct history_snapshot *_Nonnull history)
{
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_CHANGED);
    expect_body_saved(fixture, history, switch_saved);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "cancellation preserves the actual native redo of the already-saved input");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_SAME);
    expect_body_saved(fixture, history, switch_saved);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == TRUE,
            "unchanged saved input approves a new session without another save");
    expect_session_live(window, root, false);
    expect_body_saved(fixture, history, switch_saved);
    SendMessageW(root, WM_ENDSESSION, TRUE, (LPARAM)ENDSESSION_LOGOFF);
    require(!IsWindow(root) && folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == nullptr,
            "approved final session notification destroys the root through canonical finalization");
    MSG message;
    require(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) &&
                message.message == WM_QUIT && message.wParam == 0,
            "approved final session destruction publishes successful quit");
    expect_body_saved(fixture, history, switch_saved);
    expect_raw_file(fixture, L"\\schedule\\one.md", switch_saved);
}

static void run_session_save_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated named session-save source");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real session-save window on the dedicated desktop");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE selected = {.cpMin = 10, .cpMax = 12};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selected);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.focus == body && before.modified != 0 && before.undo != 0 && before.redo == 0 &&
                before.selection.cpMin < before.selection.cpMax,
            "session save begins with a focused modified native body, selection and real Undo");
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &history);
    refused_session_save(window, fixture, &history, &before);
    approve_session_save(window, fixture, &history, &before);
    finish_session_save(window, fixture, &history);
    require(controlled.requests == 0 && failure.timer == 0,
            "session save uses no image request or modal observer");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #753: Even an empty untitled draft refuses noninteractive session termination. */
static void refuse_untitled_session(const struct name_rejection_probe *_Nonnull probe)
{
    struct folio_window *window = probe->window;
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == FALSE,
            "an untitled draft refuses the session synchronously without asking for a name");
    expect_session_live(window, root, true);
    expect_session_notice(window, L"無題のノートを保存してから終了してください。", true);
    expect_native_body(window, probe->body);
    expect_rejected_draft(probe);
    SendMessageW(root, WM_ENDSESSION, TRUE, (LPARAM)ENDSESSION_LOGOFF);
    expect_session_live(window, root, true);
    expect_session_notice(window, L"無題のノートを保存してから終了してください。", true);
    expect_native_body(window, probe->body);
    expect_rejected_draft(probe);
    SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
    expect_session_live(window, root, true);
    expect_session_notice(window, L"無題のノートを保存してから終了してください。", false);
    expect_native_body(window, probe->body);
    expect_rejected_draft(probe);
    require(IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_BODY)) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_TREE)) &&
                name_dialog.timer == 0 && failure.timer == 0,
            "the refusal preserves content input and needs no modal observer");
}

static void untitled_session_states(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct modal_save_files *_Nonnull files)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.text[0] == L'\0' && before.modified == 0 && before.undo == 0 &&
                before.redo == 0 && before.focus == body,
            "the new empty draft has clean native flags but is still unsaved");
    const struct name_rejection_probe probe = {
        .window = window, .fixture = fixture, .body = &before, .files = files};
    refuse_untitled_session(&probe);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)first_save_edited);
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_CHANGED);
    CHARRANGE selection = {.cpMin = 2, .cpMax = 6};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    capture_native_body(window, &before);
    require(before.modified != 0 && before.undo != 0 && before.redo == 0 &&
                before.selection.cpMin < before.selection.cpMax && before.focus == body,
            "the edited draft has native Undo and a nonempty selection");
    refuse_untitled_session(&probe);
    history_undo(window);
    expect_editor(window, fixture->state, L"", FOLIO_NOTE_CHANGED);
    capture_native_body(window, &before);
    require(before.text[0] == L'\0' && before.undo == 0 && before.redo != 0 && before.focus == body,
            "Undo returns to an empty unsaved draft with real Redo");
    refuse_untitled_session(&probe);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0,
            "session refusal and cancellation preserve actual Redo to the edited draft");
    pump(window);
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_CHANGED);
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
}

static void expect_session_first_saved(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture,
                                       const struct modal_save_files *_Nonnull files)
{
    expect_editor(window, fixture->state, first_save_edited, FOLIO_NOTE_SAME);
    expect_modal_destination(fixture, files->name, files->target);
    expect_modal_cursor(fixture);
    expect_save_as_original(fixture, &files->original, 1);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_name_file_set(disk, "schedule", files->source);
    expect_rejected_ledger(fixture, 0, files->source_ledger);
    require(folio_state_note_count(fixture->state) == files->count + 1,
            "manual first save publishes exactly one note after three refused sessions");
}

static void save_session_draft(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture,
                               struct modal_save_files *_Nonnull files)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    wchar_t input[64];
    int length = snprintf(files->name, 64, "zzsession 日本語 🐾-%zu", files->count);
    int units = swprintf(input, 64, L"zzsession 日本語 🐾-%zu", files->count);
    require(length > 0 && length < 64 && units > 0 && units < 64,
            "independent manual-save destination literals fit");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *absent = nullptr;
    require(disk.read_note(disk.adapter, "target", files->name, &absent) == PERSISTENCE_ABSENT &&
                absent == nullptr,
            "the manual save destination cannot overwrite a previous session-test note");
    start_name_dialog(window, (struct name_dialog_request){
                                  .title = L"保存", .name = input, .action = NAME_DIALOG_SAVE});
    (void)native_key_input(window, 'S', FCONTROL);
    finish_name_dialog();
    pump(window);
    expect_native_body(window, &before);
    expect_session_first_saved(window, fixture, files);
}

static void finish_untitled_session(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct modal_save_files *_Nonnull files)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == TRUE,
            "a manually saved draft now approves session termination without another write");
    expect_session_live(window, root, false);
    expect_session_notice(window, L"無題のノートを保存してから終了してください。", false);
    expect_session_first_saved(window, fixture, files);
    SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
    expect_session_live(window, root, true);
    expect_native_body(window, &before);
    expect_session_first_saved(window, fixture, files);
    require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == TRUE,
            "the same saved draft approves a second session after cancellation");
    expect_session_live(window, root, false);
    SendMessageW(root, WM_ENDSESSION, TRUE, (LPARAM)ENDSESSION_LOGOFF);
    require(!IsWindow(root) && folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == nullptr,
            "final approved session destroys the manually saved draft window");
    MSG message;
    require(PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_REMOVE) &&
                message.message == WM_QUIT && message.wParam == 0,
            "final approved session publishes successful quit");
    expect_modal_destination(fixture, files->name, files->target);
    expect_save_as_original(fixture, &files->original, 1);
    expect_name_file_set(persistence_adapter_port(fixture->files), "schedule", files->source);
    expect_rejected_ledger(fixture, 0, files->source_ledger);
}

static void run_session_untitled_case(struct native_fixture *_Nonnull fixture)
{
    struct modal_save_files files = {0};
    struct folio_window *window = begin_modal_save(fixture, &files);
    untitled_session_states(window, fixture, &files);
    save_session_draft(window, fixture, &files);
    finish_untitled_session(window, fixture, &files);
    require(controlled.requests == 0 && name_dialog.timer == 0 && failure.timer == 0,
            "untitled session handling leaves no image requests or modal observer timers");
    folio_window_destroy(window);
    release_modal_save(&files);
}

/* #755: Synthetic composition notifications protect all four input owners. */
struct session_ime_target
{
    HWND handle;
    bool field;
};

struct session_ime_probe
{
    struct folio_window *window;
    HWND root;
    struct native_fixture *fixture;
    const struct history_snapshot *history;
    struct native_body_snapshot body;
    struct session_ime_target target;
    wchar_t input[64];
    DWORD first;
    DWORD last;
    size_t count;
    enum folio_note_change change;
};

static void expect_composition_retained(const struct session_ime_probe *_Nonnull probe,
                                        bool composing)
{
    struct folio_window *window = probe->window;
    MSG message;
    require(IsWindow(probe->root) && IsWindowEnabled(probe->root) &&
                folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == probe->root &&
                folio_window_accessible_ready(window) == !composing &&
                !PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE),
            "session refusal keeps an enabled root, composition readiness and no quit");
    expect_native_body(window, &probe->body);
    expect_editor(window, probe->fixture->state, probe->body.text, probe->change);
    expect_locked_target(probe->fixture->state);
    verify_history_unchanged(probe->fixture, probe->history);
    require(folio_state_note_count(probe->fixture->state) == probe->count &&
                name_dialog.timer == 0 && failure.timer == 0,
            "composition refuses before saving, prompting or changing public note rows");
    if (probe->target.field)
        expect_replace_save_input(probe->target.handle, probe->input, probe->first, probe->last);
}

static void refuse_composing_session(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture,
                                     const struct history_snapshot *_Nonnull history,
                                     struct session_ime_target target)
{
    struct session_ime_probe probe = {.window = window,
                                      .root =
                                          folio_window_accessible_window(window, ACCESSIBLE_WINDOW),
                                      .fixture = fixture,
                                      .history = history,
                                      .target = target,
                                      .count = folio_state_note_count(fixture->state)};
    capture_native_body(window, &probe.body);
    require(wcscmp(probe.body.text, switch_original) == 0 ||
                wcscmp(probe.body.text, switch_edited) == 0,
            "the IME probe begins with an independently expected original or edited body");
    probe.change =
        wcscmp(probe.body.text, switch_edited) == 0 ? FOLIO_NOTE_CHANGED : FOLIO_NOTE_SAME;
    require(IsWindow(target.handle) && IsWindowVisible(target.handle) &&
                GetFocus() == target.handle,
            "composition targets the visible focused native owner");
    if (target.field)
    {
        require(GetWindowTextW(target.handle, probe.input, 64) > 0,
                "the focused composition field starts with a nonempty independent query");
        SendMessageW(target.handle, EM_GETSEL, (WPARAM)&probe.first, (LPARAM)&probe.last);
        require(probe.first < probe.last, "the composition query starts with a nonempty selection");
    }
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SendMessageW(target.handle, WM_IME_STARTCOMPOSITION, 0, 0);
    for (unsigned attempt = 0; attempt < 2; ++attempt)
    {
        require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == FALSE,
                "composition and composition after a cancelled session both refuse synchronously");
        expect_session_notice(window, L"文字の変換を確定してから終了してください。", true);
        expect_composition_retained(&probe, true);
        SendMessageW(root, WM_ENDSESSION, TRUE, (LPARAM)ENDSESSION_LOGOFF);
        expect_composition_retained(&probe, true);
        expect_session_notice(window, L"文字の変換を確定してから終了してください。", true);
        SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
        expect_session_notice(window, L"文字の変換を確定してから終了してください。", false);
        expect_composition_retained(&probe, true);
    }
    SendMessageW(target.handle, WM_IME_ENDCOMPOSITION, 0, 0);
    pump(window);
    SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
    expect_composition_retained(&probe, false);
}

static void focus_session_body(struct folio_window *_Nonnull window)
{
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_BODY},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "return focus through the existing public body action");
    pump(window);
}

static void session_body_composition(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture,
                                     const struct history_snapshot *_Nonnull history)
{
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    /* Native RichEdit START replaces a selection before QUERY; start at a caret instead. */
    CHARRANGE selection = {.cpMin = 10, .cpMax = 10};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) != 0 &&
                SendMessageW(body, EM_CANUNDO, 0, 0) != 0,
            "body composition starts with an unsaved native insertion and Undo");
    refuse_composing_session(window, fixture, history, (struct session_ime_target){.handle = body});
    selection.cpMax = 12;
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SetFocus(root);
    refuse_composing_session(window, fixture, history, (struct session_ime_target){.handle = root});
    focus_session_body(window);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0,
            "body and root composition preserve actual Undo and leave the insertion redoable");
}

static void session_filter_composition(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture,
                                       const struct history_snapshot *_Nonnull history)
{
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_FILTER},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "focus the real index query with a SAME redoable body");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    require(SetWindowTextW(input, L"Current"), "enter the independent global query");
    pump(window);
    finish_filter_work(window, fixture->state);
    SendMessageW(input, EM_SETSEL, 1, 4);
    expect_replace_save_input(input, L"Current", 1, 4);
    require(strcmp(folio_state_index_filter_term(fixture->state), "Current") == 0 &&
                folio_state_index_filter_count(fixture->state) == 1,
            "exactly the dedicated saved source matches the global query");
    refuse_composing_session(window, fixture, history,
                             (struct session_ime_target){.handle = input, .field = true});
    finish_filter_work(window, fixture->state);
    expect_replace_save_input(input, L"Current", 1, 4);
    require(strcmp(folio_state_index_filter_term(fixture->state), "Current") == 0 &&
                folio_state_index_filter_count(fixture->state) == 1 &&
                !folio_state_index_filter_pending(fixture->state),
            "session cancellation and composition end retain the completed global result");
    focus_session_body(window);
}

static void session_find_composition(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture,
                                     const struct history_snapshot *_Nonnull history)
{
    invoke_operation(window, FOLIO_COMMAND_FIND);
    pump(window);
    search_term(window, L"日本語");
    expect_search_result(window, 10, 13, L"1 / 1 件");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SendMessageW(input, EM_SETSEL, 1, 3);
    expect_replace_save_input(input, L"日本語", 1, 3);
    uint64_t revision = folio_window_accessible_surface_revision(window);
    refuse_composing_session(window, fixture, history,
                             (struct session_ime_target){.handle = input, .field = true});
    expect_replace_save_input(input, L"日本語", 1, 3);
    expect_search_result(window, 10, 13, L"1 / 1 件");
    require(folio_window_accessible_surface_revision(window) == revision &&
                strcmp(folio_state_search_term(fixture->state), "日本語") == 0 &&
                strcmp(folio_state_index_filter_term(fixture->state), "Current") == 0,
            "the same Find surface and both separately owned search terms survive composition");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    focus_session_body(window);
}

static void session_settings_composition(struct folio_window *_Nonnull window,
                                         struct native_fixture *_Nonnull fixture,
                                         const struct history_snapshot *_Nonnull history)
{
    struct theme_snapshot settings = {.choice = folio_state_theme_choice(fixture->state),
                                      .settings = read_theme_settings(fixture)};
    struct accessible_request open = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                      .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &open) == S_OK,
            "open settings through its existing public button");
    pump(window);
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    require(!IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT)),
            "the settings surface has no visible native query field");
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item selected;
    require(folio_window_accessible_selected(window, &list, &selected) == S_OK,
            "snapshot the actual settings row before composition on its focused layer");
    refuse_composing_session(window, fixture, history,
                             (struct session_ime_target){.handle = layer});
    struct accessible_item after;
    struct accessible_item surface = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view view;
    require(folio_window_accessible_surface_revision(window) == list.revision &&
                folio_window_accessible_selected(window, &list, &after) == S_OK &&
                accessible_item_equal(selected, after) &&
                folio_window_accessible_read(window, &surface, &view) == S_OK && !view.offscreen,
            "composition refusal keeps the same visible settings surface and selected row");
    expect_theme_settings(fixture, &settings);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    expect_theme_settings(fixture, &settings);
    file_bytes_destroy(settings.settings);
    focus_session_body(window);
}

static void run_session_ime_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated named composition source");
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real session-composition window on the dedicated desktop");
    pump(window);
    session_body_composition(window, fixture, &history);
    session_filter_composition(window, fixture, &history);
    session_find_composition(window, fixture, &history);
    session_settings_composition(window, fixture, &history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "all composition owners preserve actual Redo to the original insertion");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &history);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    approve_session_save(window, fixture, &history, &before);
    finish_session_save(window, fixture, &history);
    require(controlled.requests == 0 && name_dialog.timer == 0 && failure.timer == 0,
            "composition session tests leave no image request or modal observer");
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

/* #757: The real Save As modal keeps its content lock through session cancellation. */
static void expect_named_modal_files(const struct name_rejection_probe *_Nonnull probe)
{
    struct native_fixture *fixture = probe->fixture;
    expect_editor(probe->window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_locked_target(fixture->state);
    require(folio_state_note_count(fixture->state) == probe->files->count,
            "session refusal inside Save As publishes no named note");
    verify_history_unchanged(fixture, &probe->files->original);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_name_file_set(disk, "schedule", probe->files->source);
    expect_name_file_set(disk, "target", probe->files->target);
    expect_rejected_ledger(fixture, 0, probe->files->source_ledger);
    expect_rejected_ledger(fixture, 1, probe->files->target_ledger);
}

static void expect_session_name_dialog(HWND dialog,
                                       const struct name_rejection_probe *_Nonnull probe,
                                       const struct native_body_snapshot *_Nonnull inside)
{
    struct folio_window *window = probe->window;
    HWND name = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    HWND categories = FindWindowExW(dialog, nullptr, L"LISTBOX", nullptr);
    require(IsWindow(dialog) && IsWindowVisible(dialog) &&
                GetAncestor(dialog, GA_ROOTOWNER) == name_dialog.root &&
                folio_window_accessible_window(window, ACCESSIBLE_WINDOW) == name_dialog.root &&
                IsWindowEnabled(name_dialog.root) && !folio_window_accessible_ready(window),
            "the same owned name dialog and enabled product root remain blocked for content");
    require(!IsWindowEnabled(name_dialog.body) && !IsWindowEnabled(name_dialog.drawer) &&
                !IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_FILTER)) &&
                IsWindowEnabled(name) && IsWindowEnabled(categories) &&
                IsWindowEnabled(GetDlgItem(dialog, IDOK)) &&
                IsWindowEnabled(GetDlgItem(dialog, IDCANCEL)) && GetFocus() == name,
            "session cancellation keeps background disabled and dialog controls usable");
    wchar_t value[64] = {0};
    DWORD first = 0;
    DWORD last = 0;
    SendMessageW(name, EM_GETSEL, (WPARAM)&first, (LPARAM)&last);
    LRESULT target = SendMessageW(categories, LB_FINDSTRINGEXACT, (WPARAM)-1, (LPARAM)L"target");
    require(GetWindowTextW(name, value, 64) > 0 && wcscmp(value, cancelled_name) == 0 &&
                first == 1 && last == 4 && target != LB_ERR &&
                SendMessageW(categories, LB_GETCURSEL, 0, 0) == target,
            "the pending Unicode name, selection and destination category survive every notice");
    MSG message;
    require(!PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE),
            "an unapproved session does not publish quit from the name dialog loop");
    expect_native_body(window, inside);
    expect_named_modal_files(probe);
}

static void probe_session_name_dialog(HWND dialog,
                                      const struct name_rejection_probe *_Nonnull probe)
{
    HWND name = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    SendMessageW(name, EM_SETSEL, 1, 4);
    struct native_body_snapshot inside = *probe->body;
    inside.focus = name;
    expect_session_name_dialog(dialog, probe, &inside);
    for (unsigned attempt = 0; attempt < 2; ++attempt)
    {
        require(
            SendMessageW(name_dialog.root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) ==
                FALSE,
            "the live Save As modal refuses each session before saving or opening another modal");
        expect_session_notice(probe->window, L"開いている操作を終えてから終了してください。", true);
        expect_session_name_dialog(dialog, probe, &inside);
        SendMessageW(name_dialog.root, WM_ENDSESSION, TRUE, (LPARAM)ENDSESSION_LOGOFF);
        expect_session_notice(probe->window, L"開いている操作を終えてから終了してください。", true);
        expect_session_name_dialog(dialog, probe, &inside);
        SendMessageW(name_dialog.root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
        expect_session_notice(probe->window, L"開いている操作を終えてから終了してください。",
                              false);
        expect_session_name_dialog(dialog, probe, &inside);
    }
}

static void capture_named_modal_files(struct native_fixture *_Nonnull fixture,
                                      struct modal_save_files *_Nonnull files)
{
    capture_history(fixture, &files->original);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    require(disk.scan_notes(disk.adapter, "schedule", &files->source) == PERSISTENCE_LOADED &&
                files->source != nullptr &&
                disk.scan_notes(disk.adapter, "target", &files->target) == PERSISTENCE_LOADED &&
                files->target != nullptr,
            "snapshot both named-document categories before the real modal opens");
    snapshot_modal_ledger(disk, "schedule", &files->source_ledger);
    snapshot_modal_ledger(disk, "target", &files->target_ledger);
    files->count = folio_state_note_count(fixture->state);
    require(files->count == name_list_count(files->source) + name_list_count(files->target),
            "all existing notes belong to the two snapshotted categories");
}

static void cancel_session_name_modal(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture,
                                      const struct modal_save_files *_Nonnull files)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(before.focus == body && before.modified != 0 && before.undo != 0 && before.redo == 0 &&
                before.selection.cpMin < before.selection.cpMax,
            "Save As opens with focused unsaved text, nonempty selection and native Undo");
    const struct name_rejection_probe probe = {
        .window = window, .fixture = fixture, .body = &before, .files = files};
    start_name_dialog(window, (struct name_dialog_request){.title = L"別名で保存",
                                                           .name = cancelled_name,
                                                           .action = NAME_DIALOG_SESSION_CANCEL,
                                                           .rejection = &probe});
    require(native_key_input(window, 'S', FCONTROL | FSHIFT),
            "the registered Ctrl+Shift+S accelerator opens the real Save As dialog");
    finish_name_dialog();
    pump(window);
    expect_session_live(window, name_dialog.root, true);
    expect_native_body(window, &before);
    expect_session_notice(window, L"開いている操作を終えてから終了してください。", false);
    expect_named_modal_files(&probe);
}

static void run_session_modal_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the named source before session refusal in Save As");
    struct modal_save_files files = {0};
    capture_named_modal_files(fixture, &files);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real session-modal window on the dedicated desktop");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    CHARRANGE selection = {.cpMin = 10, .cpMax = 12};
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_EXSETSEL, 0,
                 (LPARAM)&selection);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    cancel_session_name_modal(window, fixture, &files);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    approve_session_save(window, fixture, &files.original, &before);
    finish_session_save(window, fixture, &files.original);
    require(controlled.requests == 0 && name_dialog.timer == 0 && failure.timer == 0,
            "modal session checks leave no image request or observer timer");
    folio_window_destroy(window);
    release_modal_save(&files);
}

/* #759: Closing the root from Save As cancels that name before the original save. */
static void close_root_from_name_dialog(HWND dialog,
                                        const struct name_rejection_probe *_Nonnull probe)
{
    HWND name = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    SendMessageW(name, EM_SETSEL, 1, 4);
    struct native_body_snapshot inside = *probe->body;
    inside.focus = name;
    expect_native_body(probe->window, &inside);
    expect_editor(probe->window, probe->fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    require(IsWindowEnabled(name_dialog.root) && !IsWindowEnabled(name_dialog.body) &&
                !IsWindowEnabled(name_dialog.drawer) &&
                !folio_window_accessible_ready(probe->window),
            "normal close begins with an enabled root and the live Save As content lock");
    begin_history_failure(probe->window);
    SendMessageW(name_dialog.root, WM_CLOSE, 0, 0);
    MSG message;
    require(IsWindow(name_dialog.root) &&
                folio_window_accessible_window(probe->window, ACCESSIBLE_WINDOW) ==
                    name_dialog.root &&
                !folio_window_accessible_ready(probe->window) && failure.closed == 0 &&
                !PeekMessageW(&message, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE),
            "the root and unsaved document outlive the modal callback before deferred saving");
    /* Cancellation may move focus; only the completed outer operation must restore it. */
    inside.focus = GetFocus();
    expect_native_body(probe->window, &inside);
    expect_editor(probe->window, probe->fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    expect_locked_target(probe->fixture->state);
    require(folio_state_note_count(probe->fixture->state) == probe->files->count &&
                folio_state_store_revision(probe->fixture->state) ==
                    probe->files->original.store_revision,
            "the inner close request publishes neither a new name nor a save revision");
}

static void refuse_close_from_name(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture,
                                   const struct modal_save_files *_Nonnull files)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.focus == folio_window_accessible_window(window, ACCESSIBLE_BODY) &&
                before.modified != 0 && before.undo != 0 && before.redo == 0 &&
                before.selection.cpMin < before.selection.cpMax,
            "modal close starts with focused modified text, nonempty selection and native Undo");
    const struct name_rejection_probe probe = {
        .window = window, .fixture = fixture, .body = &before, .files = files};
    expect_named_modal_files(&probe);
    HANDLE locked = lock_native_note(fixture);
    start_name_dialog(window, (struct name_dialog_request){.title = L"別名で保存",
                                                           .name = cancelled_name,
                                                           .action = NAME_DIALOG_ROOT_CLOSE,
                                                           .rejection = &probe});
    require(native_key_input(window, 'S', FCONTROL | FSHIFT),
            "the real Ctrl+Shift+S opens Save As before its root receives normal close");
    finish_name_dialog();
    end_history_failure();
    require(CloseHandle(locked), "release the original only after the deferred save refusal");
    pump_refused_close(window, name_dialog.root);
    require(failure.closed == 1 && IsWindowEnabled(name_dialog.root) &&
                IsWindowEnabled(name_dialog.body) && IsWindowEnabled(name_dialog.drawer) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_FILTER)) &&
                folio_window_accessible_ready(window),
            "one deferred history refusal returns the original root to usable content");
    expect_native_body(window, &before);
    expect_named_modal_files(&probe);
}

static void retry_close_after_name(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture,
                                   const struct modal_save_files *_Nonnull files)
{
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &files->original);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0 && SendMessageW(body, EM_REDO, 0, 0) != 0,
            "the original native edit is redoable after cancellation and deferred save refusal");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    verify_history_unchanged(fixture, &files->original);
    close_theme_window(window);
    expect_body_saved(fixture, &files->original, switch_saved);
    expect_raw_file(fixture, L"\\schedule\\one.md", switch_saved);
    expect_locked_target(fixture->state);
    require(folio_state_note_count(fixture->state) == files->count,
            "retry closes after saving the original without publishing the cancelled name");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_name_file_set(disk, "schedule", files->source);
    expect_name_file_set(disk, "target", files->target);
    expect_rejected_ledger(fixture, 0, files->source_ledger);
    expect_rejected_ledger(fixture, 1, files->target_ledger);
}

static void run_close_modal_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the named original before normal close from Save As");
    struct modal_save_files files = {0};
    capture_named_modal_files(fixture, &files);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real close-modal window on the dedicated desktop");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    CHARRANGE selection = {.cpMin = 10, .cpMax = 12};
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_EXSETSEL, 0,
                 (LPARAM)&selection);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    refuse_close_from_name(window, fixture, &files);
    retry_close_after_name(window, fixture, &files);
    require(controlled.requests == 0 && name_dialog.timer == 0 && failure.timer == 0,
            "normal close from a name dialog leaves no image request or observer timer");
    folio_window_destroy(window);
    release_modal_save(&files);
}

/* #767: Use the real operations menu, native file picker and publication without saving. */
enum export_dialog_phase
{
    EXPORT_DIALOG_ENTER,
    EXPORT_DIALOG_SUBMIT,
    EXPORT_DIALOG_NOTICE
};

struct export_dialog_probe
{
    struct folio_window *window;
    const struct native_body_snapshot *body;
    const wchar_t *path;
    bool modal_notice;
    HWND root;
    HWND picker;
    HWND notice;
    HWND filename;
    UINT_PTR timer;
    unsigned ticks;
    unsigned inputs;
    unsigned notices;
    bool cancel;
    enum export_dialog_phase phase;
};

static struct export_dialog_probe exporting;

static BOOL CALLBACK find_export_filename(HWND child, LPARAM parameter)
{
    (void)parameter;
    wchar_t kind[32];
    wchar_t value[64];
    if (GetDlgCtrlID(child) != 1001 || GetClassNameW(child, kind, 32) == 0 ||
        wcscmp(kind, L"Edit") != 0 || GetWindowTextW(child, value, 64) == 0)
        return TRUE;
    if (wcscmp(value, L"one") != 0 && wcscmp(value, L"one.md") != 0)
        return TRUE;
    exporting.filename = child;
    ++exporting.inputs;
    return TRUE;
}

static void expect_export_modal(HWND dialog)
{
    HWND body = folio_window_accessible_window(exporting.window, ACCESSIBLE_BODY);
    HWND drawer = folio_window_accessible_window(exporting.window, ACCESSIBLE_DRAWER);
    require(IsWindowEnabled(exporting.root) && !IsWindowEnabled(body) && !IsWindowEnabled(drawer) &&
                !folio_window_accessible_ready(exporting.window),
            "the export modal keeps the shell enabled and locks only content");
    struct native_body_snapshot inside = *exporting.body;
    inside.focus = GetFocus();
    require(inside.focus != nullptr && IsChild(dialog, inside.focus),
            "the active export modal owns keyboard focus");
    expect_native_body(exporting.window, &inside);
}

static void expect_export_filename(void)
{
    wchar_t value[storage_native_capacity];
    int length = GetWindowTextW(exporting.filename, value, storage_native_capacity);
    require(length > 0 && (size_t)length == wcslen(exporting.path) &&
                wcscmp(value, exporting.path) == 0,
            "the native filename contains the complete intended absolute Unicode path");
}

static void choose_export_destination(HWND dialog)
{
    wchar_t title[128];
    require(GetWindowTextW(dialog, title, 128) > 0 &&
                wcscmp(title, L"Markdownを書き出す（編集中の内容を含む）") == 0,
            "the native picker is specifically the current Markdown export");
    EnumChildWindows(dialog, find_export_filename, 0);
    require(exporting.inputs == 1 && exporting.filename != nullptr &&
                IsWindowEnabled(exporting.filename),
            "the native picker exposes exactly one editable filename initialized from one");
    expect_export_modal(dialog);
    SetFocus(exporting.filename);
    require(GetFocus() == exporting.filename, "focus the actual native filename editor");
    SendMessageW(exporting.filename, EM_SETSEL, 0, -1);
    SendMessageW(exporting.filename, EM_REPLACESEL, TRUE, (LPARAM)exporting.path);
    expect_export_filename();
    exporting.picker = dialog;
    exporting.phase = EXPORT_DIALOG_SUBMIT;
}

static void submit_export_destination(HWND dialog)
{
    require(dialog == exporting.picker, "submit the same owned picker after its input dispatch");
    expect_export_filename();
    expect_export_modal(dialog);
    int command = exporting.cancel ? IDCANCEL : IDOK;
    HWND button = GetDlgItem(dialog, command);
    require(button != nullptr && IsWindowEnabled(button),
            "the selected save or cancel action is available");
    if (exporting.cancel || !exporting.modal_notice)
    {
        require(KillTimer(nullptr, exporting.timer),
                "stop the picker-only observer before cancellation or an inline refusal");
        exporting.timer = 0;
    }
    exporting.phase = EXPORT_DIALOG_NOTICE;
    require(PostMessageW(dialog, WM_COMMAND, MAKEWPARAM(command, BN_CLICKED), (LPARAM)button),
            "submit only the owned native picker through its actual command");
}

static void finish_export_notice(HWND dialog)
{
    HWND text = FindWindowExW(dialog, nullptr, L"EDIT", nullptr);
    HWND accept = GetDlgItem(dialog, IDOK);
    wchar_t message[512];
    require(text != nullptr && accept != nullptr && IsWindowEnabled(accept) &&
                GetWindowTextW(text, message, 512) > 0 &&
                wcscmp(message, L"書き出しました。元のノートはそのままです。") == 0,
            "the separate product notice confirms export rather than an error or overwrite");
    expect_export_modal(dialog);
    require(!IsWindow(exporting.picker) && KillTimer(nullptr, exporting.timer),
            "the picker is closed before the product notice and observer stop");
    exporting.timer = 0;
    exporting.notice = dialog;
    ++exporting.notices;
    SendMessageW(accept, BM_CLICK, 0, 0);
}

static BOOL CALLBACK observe_export_window(HWND dialog, LPARAM parameter)
{
    wchar_t kind[32];
    if (GetAncestor(dialog, GA_ROOTOWNER) != (HWND)parameter || !IsWindowVisible(dialog) ||
        GetClassNameW(dialog, kind, 32) == 0 || wcscmp(kind, L"#32770") != 0)
        return TRUE;
    switch (exporting.phase)
    {
    case EXPORT_DIALOG_ENTER:
        choose_export_destination(dialog);
        break;
    case EXPORT_DIALOG_SUBMIT:
        submit_export_destination(dialog);
        break;
    case EXPORT_DIALOG_NOTICE:
        if (dialog != exporting.picker)
            finish_export_notice(dialog);
        break;
    }
    return FALSE;
}

static void CALLBACK observe_export_dialog(HWND window, UINT message, UINT_PTR timer, DWORD ticks)
{
    (void)window;
    (void)message;
    (void)ticks;
    if (exporting.timer == 0)
        return;
    require(timer == exporting.timer && ++exporting.ticks <= 100,
            "the export observer stays within its finite deadline");
    EnumThreadWindows(GetCurrentThreadId(), observe_export_window, (LPARAM)exporting.root);
}

static void export_directory(wchar_t path[storage_native_capacity])
{
    DWORD length = GetModuleFileNameW(nullptr, path, storage_native_capacity);
    require(length > 0 && length < storage_native_capacity, "the test executable path fits");
    while (length != 0 && path[length - 1] != L'\\')
        --length;
    require(length != 0 && storage_native_capacity - length > 32,
            "export artifacts stay beside the dedicated executable and outside its data");
    for (unsigned attempt = 1; attempt <= 1000; ++attempt)
    {
        int written = swprintf_s(path + length, storage_native_capacity - length,
                                 L"export-native-%04u", attempt);
        require(written > 0, "the dedicated export directory name fits");
        if (CreateDirectoryW(path, nullptr))
            return;
        require(GetLastError() == ERROR_ALREADY_EXISTS, "only a prior export directory is skipped");
    }
    require(false, "a fresh export artifact directory is available");
}

static void expect_export_files(struct native_fixture *_Nonnull fixture,
                                const struct modal_save_files *_Nonnull files)
{
    expect_locked_target(fixture->state);
    require(folio_state_note_count(fixture->state) == files->count,
            "export does not create or select a note in the notebook");
    verify_history_unchanged(fixture, &files->original);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    expect_name_file_set(disk, "schedule", files->source);
    expect_name_file_set(disk, "target", files->target);
    expect_rejected_ledger(fixture, 0, files->source_ledger);
    expect_rejected_ledger(fixture, 1, files->target_ledger);
}

static void report_export_mismatch(const wchar_t *_Nonnull path, const char *_Nonnull expected,
                                   const struct file_bytes *_Nullable bytes,
                                   enum persistence_outcome read)
{
    fprintf(stderr, "export diagnostic: read=%d expected=%zu actual=%zu path=", (int)read,
            strlen(expected), bytes == nullptr ? 0 : file_bytes_length(bytes));
    for (size_t index = 0; path[index] != L'\0'; ++index)
        fprintf(stderr, "%04x ", (unsigned)path[index]);
    fprintf(stderr, "\nactual bytes:");
    if (bytes != nullptr)
        for (size_t index = 0; index < file_bytes_length(bytes); ++index)
            fprintf(stderr, " %02x", (unsigned char)file_bytes_data(bytes)[index]);
    fprintf(stderr, "\nexpected bytes:");
    for (size_t index = 0; index < strlen(expected); ++index)
        fprintf(stderr, " %02x", (unsigned char)expected[index]);
    fprintf(stderr, "\n");
}

static void expect_export_result(const wchar_t *_Nonnull path, const char *_Nullable expected)
{
    struct file_bytes *bytes = nullptr;
    enum persistence_outcome read = file_bytes_read(path, &bytes);
    if (expected == nullptr)
        require(read == PERSISTENCE_ABSENT && bytes == nullptr,
                "cancelling the selected filename publishes no external file");
    else
    {
        if (read != PERSISTENCE_LOADED || bytes == nullptr ||
            file_bytes_length(bytes) != strlen(expected) ||
            memcmp(file_bytes_data(bytes), expected, strlen(expected)) != 0)
        {
            report_export_mismatch(path, expected, bytes, read);
        }
        require(read == PERSISTENCE_LOADED && bytes != nullptr &&
                    file_bytes_length(bytes) == strlen(expected) &&
                    memcmp(file_bytes_data(bytes), expected, strlen(expected)) == 0,
                "the actual exported file matches every independent UTF-8 literal byte");
    }
    file_bytes_destroy(bytes);
}

static void export_current_to(struct folio_window *_Nonnull window, const wchar_t *_Nonnull path,
                              const char *_Nullable expected, bool modal_notice)
{
    focus_session_body(window);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    require(before.selection.cpMin < before.selection.cpMax,
            "export starts with a nonempty native selection");
    exporting = (struct export_dialog_probe){
        .window = window,
        .body = &before,
        .path = path,
        .modal_notice = modal_notice,
        .root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW),
        .cancel = expected == nullptr,
        .phase = EXPORT_DIALOG_ENTER};
    exporting.timer = SetTimer(nullptr, 0, 50, observe_export_dialog);
    require(exporting.timer != 0, "arm the owned finite export observer");
    invoke_operation(window, FOLIO_COMMAND_EXPORT_NOTE);
    pump(window);
    require(exporting.timer == 0 && exporting.picker != nullptr && !IsWindow(exporting.picker) &&
                !IsWindow(exporting.notice) &&
                exporting.notices == (expected != nullptr && modal_notice ? 1U : 0U),
            "one native picker and only the expected product notice close completely");
    require(IsWindowEnabled(exporting.root) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_BODY)) &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_DRAWER)) &&
                folio_window_accessible_ready(window),
            "the same root and content return to their ready state after export");
    expect_native_body(window, &before);
    expect_export_result(path, expected);
}

struct export_file_request
{
    const wchar_t *directory;
    const wchar_t *leaf;
    const char *expected;
};

static void export_step(struct folio_window *_Nonnull window,
                        struct native_fixture *_Nonnull fixture,
                        const struct modal_save_files *_Nonnull files,
                        struct export_file_request request)
{
    wchar_t path[storage_native_capacity];
    require(swprintf_s(path, storage_native_capacity, L"%s\\%s", request.directory, request.leaf) >
                0,
            "the dedicated destination path fits");
    CHARRANGE selection = {.cpMin = 2, .cpMax = 6};
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_EXSETSEL, 0,
                 (LPARAM)&selection);
    enum pane_mode mode = folio_state_pane_mode(fixture->state);
    export_current_to(window, path, request.expected, true);
    require(folio_state_pane_mode(fixture->state) == mode,
            "current-note export keeps the existing VIEW or EDIT mode");
    expect_export_files(fixture, files);
}

static void expect_export_directory(const wchar_t *_Nonnull directory, unsigned expected)
{
    wchar_t pattern[storage_native_capacity];
    require(swprintf_s(pattern, storage_native_capacity, L"%s\\*", directory) > 0,
            "the external export enumeration fits");
    WIN32_FIND_DATAW entry = {0};
    HANDLE found = FindFirstFileW(pattern, &entry);
    require(found != INVALID_HANDLE_VALUE, "enumerate the dedicated external output directory");
    unsigned count = 0;
    do
    {
        if (wcscmp(entry.cFileName, L".") == 0 || wcscmp(entry.cFileName, L"..") == 0)
            continue;
        require((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                    (wcscmp(entry.cFileName, L"変更 日本語.md") == 0 ||
                     wcscmp(entry.cFileName, L"取消後 日本語.md") == 0 ||
                     wcscmp(entry.cFileName, L"閲覧 日本語.md") == 0),
                "export leaves only requested files and no temporary artifacts");
        ++count;
    } while (FindNextFileW(found, &entry));
    require(GetLastError() == ERROR_NO_MORE_FILES && FindClose(found) && count == expected,
            "the complete external file set contains exactly the expected successful exports");
}

/* #771: A real name-taken refusal preserves Redo and permits a fresh-name retry. */
static void expect_export_metadata(const wchar_t *_Nonnull path,
                                   const WIN32_FILE_ATTRIBUTE_DATA *_Nonnull before)
{
    WIN32_FILE_ATTRIBUTE_DATA after = {0};
    require(GetFileAttributesExW(path, GetFileExInfoStandard, &after) &&
                after.nFileSizeHigh == before->nFileSizeHigh &&
                after.nFileSizeLow == before->nFileSizeLow &&
                CompareFileTime(&after.ftCreationTime, &before->ftCreationTime) == 0 &&
                CompareFileTime(&after.ftLastWriteTime, &before->ftLastWriteTime) == 0,
            "the refused output retains its original size and creation/write timestamps");
}

static void refuse_and_retry_export(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct modal_save_files *_Nonnull files,
                                    const wchar_t *_Nonnull directory)
{
    wchar_t path[storage_native_capacity];
    require(swprintf_s(path, storage_native_capacity, L"%s\\変更 日本語.md", directory) > 0,
            "the already published destination path fits");
    WIN32_FILE_ATTRIBUTE_DATA before = {0};
    require(GetFileAttributesExW(path, GetFileExInfoStandard, &before),
            "the rejected destination exists before the second save picker");
    export_current_to(window, path, switch_saved, false);
    struct accessible_item notice = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view text = {0};
    require(folio_window_accessible_read(window, &notice, &text) == S_OK && !text.offscreen &&
                wcscmp(text.name, L"その名前のファイルがあります。別の名前を指定してください。") ==
                    0,
            "the existing inline notice exactly reports the name-taken refusal");
    expect_export_metadata(path, &before);
    expect_export_directory(directory, 1);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "refused export keeps the original EDIT mode");
    expect_export_files(fixture, files);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0,
            "actual Redo succeeds immediately after the name-taken refusal");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    export_step(window, fixture, files,
                (struct export_file_request){directory, L"取消後 日本語.md", history_current});
    expect_export_result(path, switch_saved);
    expect_export_metadata(path, &before);
    expect_export_directory(directory, 2);
}

static void run_export_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the named source before external export");
    struct modal_save_files files = {0};
    capture_named_modal_files(fixture, &files);
    wchar_t directory[storage_native_capacity];
    export_directory(directory);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real export window on the dedicated desktop");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    export_step(window, fixture, &files,
                (struct export_file_request){directory, L"取消 日本語.md", nullptr});
    export_step(window, fixture, &files,
                (struct export_file_request){directory, L"変更 日本語.md", switch_saved});
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANREDO, 0, 0) != 0, "the second export retains real Redo");
    CHARRANGE selection = {.cpMin = 2, .cpMax = 6};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&selection);
    refuse_and_retry_export(window, fixture, &files, directory);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0, "actual Redo succeeds after external export");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    action(window, ACCESSIBLE_VIEW);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                (GetWindowLongPtrW(body, GWL_STYLE) & ES_READONLY) != 0,
            "the third export starts with the readonly rendered VIEW");
    export_step(window, fixture, &files,
                (struct export_file_request){directory, L"閲覧 日本語.md", history_current});
    expect_export_directory(directory, 3);
    require(controlled.requests == 0 && exporting.timer == 0,
            "plain single-note exports leave no image requests or observers");
    folio_window_destroy(window);
    release_modal_save(&files);
}

static struct accessible_item favorite_item(struct folio_window *_Nonnull window)
{
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE};
    struct accessible_item item = {0};
    require(folio_window_accessible_selected(window, &tree, &item) == S_OK &&
                item.kind == ACCESSIBLE_NOTE && item.ref.category == 0 && item.ref.note == 0,
            "obtain the current row identity through the public tree");
    return item;
}

static void expect_favorite_saved(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture, bool expected)
{
    struct accessible_item item = favorite_item(window);
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && view.favorite == expected,
            "UIA exposes favorite independently of note selection");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_ledger *ledger = nullptr;
    require(disk.read_note_ledger(disk.adapter, "schedule", &ledger) == PERSISTENCE_LOADED &&
                note_ledger_favorite(ledger, 0) == expected,
            "native operation persists the same favorite through the canonical codec");
    note_ledger_destroy(ledger);
}

/* Click the same visible star rectangle that a pointer can reach, with cancellation as a separate
 * case. */
static void favorite_click(struct folio_window *_Nonnull window, bool cancel)
{
    struct accessible_item item = favorite_item(window);
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen,
            "favorite row is on screen");
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    POINT point = {view.bounds.right - MulDiv(28, (int)GetDpiForWindow(drawer), 96),
                   (view.bounds.top + view.bounds.bottom) / 2};
    ScreenToClient(drawer, &point);
    LPARAM position = MAKELPARAM(point.x, point.y);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(point.x - 1, point.y));
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    if (cancel)
        ReleaseCapture();
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    pump(window);
}

static void expect_favorite_scope_name(struct folio_window *_Nonnull window, bool favorites)
{
    const enum accessible_kind kinds[] = {ACCESSIBLE_SCOPE_ALL, ACCESSIBLE_SCOPE_FAVORITES};
    const char16_t *names[] = {u"すべて", u"お気に入り"};
    for (size_t index = 0; index < sizeof kinds / sizeof kinds[0]; ++index)
    {
        struct accessible_item item = {.kind = kinds[index]};
        struct accessible_view view = {0};
        require(folio_window_accessible_read(window, &item, &view) == S_OK &&
                    wcscmp((const wchar_t *)view.name, (const wchar_t *)names[index]) == 0 &&
                    view.selected == (favorites == (index == 1)) && !view.offscreen,
                "textual scope choices expose the selected value even when no row matches");
    }
    struct accessible_item group = {.kind = ACCESSIBLE_SCOPE_GROUP}, selected = {0};
    require(folio_window_accessible_selected(window, &group, &selected) == S_OK &&
                selected.kind == (favorites ? ACCESSIBLE_SCOPE_FAVORITES : ACCESSIBLE_SCOPE_ALL),
            "scope selection is owned by the same application value");
}

static LPARAM favorite_position(struct folio_window *_Nonnull window)
{
    struct accessible_item item = favorite_item(window);
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "visible favorite fixture");
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    POINT point = {view.bounds.right - MulDiv(28, (int)GetDpiForWindow(drawer), 96),
                   (view.bounds.top + view.bounds.bottom) / 2};
    ScreenToClient(drawer, &point);
    return MAKELPARAM(point.x, point.y);
}

static uint64_t favorite_pixel_hash(HDC dc, RECT bounds)
{
    uint64_t value = 14695981039346656037u;
    for (int y = bounds.top; y < bounds.bottom; ++y)
        for (int x = bounds.left; x < bounds.right; ++x)
            value = (value ^ GetPixel(dc, x, y)) * 1099511628211u;
    return value;
}

/* Capture the real drawer; hashes compare the title and star without exposing product internals. */
static uint64_t favorite_pixels(struct folio_window *_Nonnull window, const char *_Nonnull path,
                                uint64_t *_Nonnull star)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT client;
    GetClientRect(drawer, &client);
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(drawer, dc, PW_CLIENTONLY), "render real favorite drawer pixels");
    struct accessible_item item = favorite_item(window);
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "measure favorite row");
    MapWindowPoints(nullptr, drawer, (POINT *)&view.bounds, 2);
    int dpi = (int)GetDpiForWindow(drawer);
    RECT title = view.bounds;
    title.left = MulDiv(40, dpi, 96);
    title.right -= MulDiv(58, dpi, 96);
    uint64_t title_hash = favorite_pixel_hash(dc, title);
    RECT glyph = view.bounds;
    glyph.left = client.right - MulDiv(42, dpi, 96);
    glyph.right = client.right - MulDiv(14, dpi, 96);
    *star = favorite_pixel_hash(dc, glyph);
    printf("drawer=%ld dpi=%d row=%ld scope=28 glyph=16 target=28 title=%llu star=%llu file=%s\n",
           client.right, dpi, view.bounds.bottom - view.bounds.top, (unsigned long long)title_hash,
           (unsigned long long)*star, path);
    SelectObject(dc, old);
    save_bitmap(dc, bitmap, client, path);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
    return title_hash;
}

static void favorite_visibility_pixels(struct folio_window *_Nonnull window)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    uint64_t hidden = 0, hovered = 0, focused = 0;
    uint64_t title = favorite_pixels(window, "favorites-hidden.bmp", &hidden);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, favorite_position(window));
    require(favorite_pixels(window, "favorites-hover.bmp", &hovered) == title && hovered != hidden,
            "hover reveals only the star while title pixels stay fixed");
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    struct accessible_request request = {.item = favorite_item(window),
                                         .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &request) == S_OK, "focus the real note cursor");
    (void)favorite_pixels(window, "favorites-focused.bmp", &focused);
    require(focused != hidden, "real index focus reveals the outline");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
}

static void favorite_label_measure(HDC dc, enum folio_language language, UINT dpi)
{
    wchar_t face[LF_FACESIZE];
    ui_face_for(language, face);
    HFONT font = CreateFontW(-MulDiv(12, (int)dpi, 96), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    HGDIOBJ old = SelectObject(dc, font);
    const enum ui_text labels[] = {UI_TEXT_SCOPE_ALL, UI_TEXT_TITLE_FAVORITES};
    for (size_t at = 0; at < sizeof labels / sizeof labels[0]; ++at)
    {
        char16_t text[64];
        size_t count = 0;
        require(utf16_text_fill(ui_text_line(labels[at], language), text, 64, &count) ==
                    UTF16_TEXT_FILL_READY,
                "scope label conversion");
        SIZE size;
        require(GetTextExtentPoint32W(dc, text, (int)count, &size) &&
                    size.cx + MulDiv(16, (int)dpi, 96) <= MulDiv(84, (int)dpi, 96),
                "full scope labels fit the 200 DIP drawer with 8 DIP side room");
        printf("scope-text language=%u dpi=%u label=%u width=%ld available=%d face=%ls\n",
               (unsigned)language, dpi, (unsigned)labels[at], size.cx, MulDiv(84, (int)dpi, 96),
               face);
    }
    SelectObject(dc, old);
    DeleteObject(font);
}

static void favorite_scaled_glyphs(HWND drawer)
{
    RECT client = {.right = 320, .bottom = 160};
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    FillRect(dc, &client, (HBRUSH)GetStockObject(WHITE_BRUSH));
    const UINT scales[] = {96, 144, 192};
    for (size_t at = 0; at < sizeof scales / sizeof scales[0]; ++at)
    {
        int size = MulDiv(16, (int)scales[at], 96), x = 24 + (int)at * 100;
        RECT outline = {x, 24, x + size, 24 + size};
        RECT filled = {x, 80, x + size, 80 + size};
        icon_paint_fill(dc, outline, ICON_PAINT_STAR_OUTLINE, RGB(0, 0, 0));
        icon_paint_fill(dc, filled, ICON_PAINT_STAR, RGB(0, 0, 0));
        const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                                 FOLIO_LANGUAGE_ZH_HANS};
        for (size_t lang = 0; lang < sizeof languages / sizeof languages[0]; ++lang)
            favorite_label_measure(dc, languages[lang], scales[at]);
    }
    SelectObject(dc, old);
    save_bitmap(dc, bitmap, client, "favorites-glyphs-96-144-192.bmp");
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
}

static void favorite_visual_matrix(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t index = 0; index < 9; ++index)
    {
        favorite_high_contrast = index / 3 == 2;
        require(
            folio_state_set_language(fixture->state, languages[index % 3]) == FOLIO_STATE_READY &&
                folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                folio_state_set_theme(fixture->state, index / 3 == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                     : FOLIO_THEME_CHOICE_LIGHT) ==
                    FOLIO_STATE_READY,
            "matrix uses application language/theme and an injected OS appearance snapshot");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "visual matrix window");
        pump(window);
        struct accessible_request width = {.item = {.kind = ACCESSIBLE_SPLITTER},
                                           .action = ACCESSIBLE_ACTION_SET_RANGE,
                                           .range_value = 200};
        require(folio_window_accessible_action(window, &width) == S_OK, "minimum drawer width");
        SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
        char path[64];
        int written = snprintf(path, sizeof path, "favorites-width200-%zu.bmp", index);
        require(written > 0 && (size_t)written < sizeof path, "matrix evidence path");
        uint64_t star = 0;
        (void)favorite_pixels(window, path, &star);
        if (index == 0)
            favorite_scaled_glyphs(folio_window_accessible_window(window, ACCESSIBLE_DRAWER));
        folio_window_destroy(window);
    }
    favorite_high_contrast = false;
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY &&
                folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY,
            "restore native fixture language and appearance");
}

static void favorite_hidden_input(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LPARAM point = favorite_position(window);
    SetFocus(body);
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    LPARAM original = SetMessageExtraInfo((LPARAM)0xFF515780u);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, point);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(drawer, WM_LBUTTONUP, 0, point);
    SetMessageExtraInfo(original);
    pump(window);
    require(!folio_state_note_favorite(fixture->state, (struct note_ref){0, 0}),
            "a touch-derived move and tap on a hidden star do not favorite the note");
    SetFocus(body);
    favorite_click(window, false);
    expect_favorite_saved(window, fixture, true);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(drawer, WM_LBUTTONUP, 0, point);
    expect_favorite_saved(window, fixture, true);
    favorite_click(window, false);
    expect_favorite_saved(window, fixture, false);
}

static void favorite_reordered_input(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    struct accessible_item first = favorite_item(window), second = {0};
    require(folio_window_accessible_navigate(window, &first, ACCESSIBLE_NAVIGATION_NEXT, &second) ==
                    S_OK &&
                second.kind == ACCESSIBLE_NOTE && second.ref.note == 1,
            "second unstarred row");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, favorite_position(window));
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(20, HIWORD(favorite_position(window))));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    struct accessible_request request = {.item = second, .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "focus second row without rebuilding the cached layout");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &second, &view) == S_OK, "second row geometry");
    POINT point = {view.bounds.right - MulDiv(28, (int)GetDpiForWindow(drawer), 96),
                   (view.bounds.top + view.bounds.bottom) / 2};
    ScreenToClient(drawer, &point);
    LPARAM position = MAKELPARAM(point.x, point.y);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    require(folio_state_note_favorite(fixture->state, second.ref),
            "actual keyboard cursor makes its outline actionable");
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    require(!folio_state_note_favorite(fixture->state, first.ref) &&
                folio_state_note_favorite(fixture->state, second.ref),
            "repeated click cannot favorite the different row now under the same coordinate");
    first = favorite_item(window);
    require(folio_window_accessible_navigate(window, &first, ACCESSIBLE_NAVIGATION_PREVIOUS,
                                             &second) == S_OK &&
                second.kind == ACCESSIBLE_NOTE && second.ref.note == 1,
            "starred row moved before the selected note");
    request.item = second;
    request.action = ACCESSIBLE_ACTION_TOGGLE_FAVORITE;
    require(folio_window_accessible_action(window, &request) == S_OK,
            "restore second row through UIA");
    request.item = favorite_item(window);
    request.action = ACCESSIBLE_ACTION_FOCUS;
    require(folio_window_accessible_action(window, &request) == S_OK, "restore the first cursor");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
}

static void favorite_cancel_input(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    LPARAM point = favorite_position(window);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(20, HIWORD(point)));
    SendMessageW(drawer, WM_MOUSEMOVE, 0, point);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(drawer, WM_LBUTTONUP, 0, MAKELPARAM(1, 1));
    expect_favorite_saved(window, fixture, false);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(drawer, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), 0);
    SendMessageW(drawer, WM_LBUTTONUP, 0, point);
    expect_favorite_saved(window, fixture, false);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, point);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(drawer, WM_SIZE, 0, 0);
    SendMessageW(drawer, WM_LBUTTONUP, 0, point);
    expect_favorite_saved(window, fixture, false);
}

static void select_favorite_scope(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_request request = {.item = {.kind = kind},
                                         .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK, "UIA scope selection");
    pump(window);
}

/* The body-only shortcut helper deliberately targets RichEdit. Scope traversal uses real focus
 * and both key transitions, as the main message loop does. */
static void favorite_key(struct folio_window *_Nonnull window, UINT key)
{
    BYTE original[256], keyboard[256] = {0};
    require(GetKeyboardState(original), "save scope keyboard state");
    keyboard[key] = 0x80;
    require(SetKeyboardState(keyboard), "scope key pressed");
    MSG message = {.hwnd = GetFocus(), .message = WM_KEYDOWN, .wParam = key, .lParam = 1};
    if (!folio_window_translate(window, &message))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    pump(window);
    keyboard[key] = 0;
    require(SetKeyboardState(keyboard), "scope key released");
    message = (MSG){
        .hwnd = GetFocus(), .message = WM_KEYUP, .wParam = key, .lParam = (LPARAM)0xC0000001u};
    if (!folio_window_translate(window, &message))
        DispatchMessageW(&message);
    pump(window);
    require(SetKeyboardState(original), "restore scope keyboard state");
}

static void favorite_empty_pixels(struct folio_window *_Nonnull window, const char *_Nonnull path)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT client;
    GetClientRect(drawer, &client);
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(drawer, dc, PW_CLIENTONLY), "render the empty favorite explanation");
    SelectObject(dc, old);
    save_bitmap(dc, bitmap, client, path);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
}

static void favorite_empty_scope(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    favorite_click(window, false);
    HWND filter = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    SetWindowTextW(filter, L"no-favorite-match");
    for (unsigned at = 0; at < 100 && folio_state_index_filter_pending(fixture->state); ++at)
        folio_window_continue_idle_work(window);
    select_favorite_scope(window, ACCESSIBLE_SCOPE_FAVORITES);
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &tree, &view) == S_OK &&
                wcsstr(view.help, L"一致する") != nullptr &&
                folio_state_index_filter_count(fixture->state) == 0,
            "existing favorites with no search match have a distinct empty explanation");
    favorite_empty_pixels(window, "favorites-no-match.bmp");
    wchar_t term[64];
    GetWindowTextW(filter, term, 64);
    require(wcscmp(term, L"no-favorite-match") == 0, "scope change preserves the search term");
    SetWindowTextW(filter, L"");
    pump(window);
    struct accessible_request request = {.item = favorite_item(window),
                                         .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "focus the only favorite row");
    request.action = ACCESSIBLE_ACTION_TOGGLE_FAVORITE;
    require(folio_window_accessible_action(window, &request) == S_OK,
            "remove the last focused favorite");
    struct accessible_item focus = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_SCOPE_FAVORITES,
            "empty tree focus lands on the selected scope");
    select_favorite_scope(window, ACCESSIBLE_SCOPE_ALL);
}

static void favorite_scope_input(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND filter = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    SetFocus(filter);
    favorite_key(window, VK_TAB);
    struct accessible_item focus = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_SCOPE_ALL,
            "Tab enters the selected scope after search");
    favorite_key(window, VK_RIGHT);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL,
            "arrow moves only focus within scope group");
    favorite_key(window, VK_SPACE);
    expect_favorite_scope_name(window, true);
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &tree, &view) == S_OK && view.help[0] != 0,
            "empty favorites have an accessible explanation");
    favorite_empty_pixels(window, "favorites-empty.bmp");
    favorite_key(window, VK_TAB);
    favorite_key(window, VK_TAB);
    require(folio_window_accessible_focus(window, ACCESSIBLE_WINDOW, &focus) != S_OK ||
                focus.kind != ACCESSIBLE_TREE,
            "Tab skips an empty tree");
    select_favorite_scope(window, ACCESSIBLE_SCOPE_ALL);
    favorite_empty_scope(window, fixture);
    require(IsWindow(root), "scope keeps the same window");
}

static void favorite_tooltips(struct folio_window *_Nonnull window)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    const enum accessible_kind kinds[] = {ACCESSIBLE_SCOPE_ALL, ACCESSIBLE_SCOPE_FAVORITES};
    for (size_t at = 0; at < sizeof kinds / sizeof kinds[0]; ++at)
    {
        wchar_t text[1024] = {0};
        TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                          .hwnd = drawer,
                          .uId = kinds[at],
                          .lpszText = text};
        require(tooltip != nullptr && SendMessageW(tooltip, TTM_GETTOOLINFOW, 0, (LPARAM)&tool),
                "scope has a registered native tooltip");
        tool.lpszText = text;
        SendMessageW(tooltip, TTM_GETTEXTW, 1024, (LPARAM)&tool);
        struct accessible_item item = {.kind = kinds[at]};
        struct accessible_view view = {0};
        require(folio_window_accessible_read(window, &item, &view) == S_OK &&
                    wcscmp(text, view.name) == 0,
                "tooltip and UIA use the same full scope name");
        MapWindowPoints(drawer, nullptr, (POINT *)&tool.rect, 2);
        require(EqualRect(&tool.rect, &view.bounds),
                "tooltip and UIA share the visible scope rectangle");
    }
}

static void favorite_edit_roundtrip(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture,
                                    const struct history_snapshot *_Nonnull history)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    puts("favorites body: cancel and click");
    fflush(stdout);
    favorite_click(window, true);
    expect_native_body(window, &snapshot);
    require(!folio_state_note_favorite(fixture->state, (struct note_ref){0, 0}),
            "capture cancellation never toggles a star");
    favorite_click(window, false);
    expect_favorite_saved(window, fixture, true);
    expect_native_body(window, &snapshot);
    struct accessible_request toggle = {.item = favorite_item(window),
                                        .action = ACCESSIBLE_ACTION_TOGGLE_FAVORITE};
    require(folio_window_accessible_action(window, &toggle) == S_OK, "UIA toggles the same note");
    pump(window);
    expect_favorite_saved(window, fixture, false);
    expect_native_body(window, &snapshot);
    favorite_click(window, false);
    puts("favorites body: scope via menu");
    fflush(stdout);
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    pump(window);
    snapshot.focus =
        GetFocus(); /* The menu's public focus policy changes focus, not editor state. */
    expect_native_body(window, &snapshot);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_FAVORITES,
            "favorites-only command is applied without saving the editor");
    expect_favorite_scope_name(window, true);
    puts("favorites body: remove last favorite");
    fflush(stdout);
    toggle.item = favorite_item(window);
    require(folio_window_accessible_action(window, &toggle) == S_OK,
            "unstar the only filtered note");
    pump(window);
    expect_native_body(window, &snapshot);
    struct accessible_view hidden = {0};
    require(folio_window_accessible_read(window, &toggle.item, &hidden) != S_OK &&
                folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_NAMED,
            "filtered row disappears while its edited document remains open");
    expect_favorite_scope_name(window, true);
    puts("favorites body: restore scope and order");
    fflush(stdout);
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    pump(window);
    expect_favorite_scope_name(window, false);
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_FIRST);
    pump(window);
    snapshot.focus = GetFocus();
    expect_native_body(window, &snapshot);
    verify_history_unchanged(fixture, history);
}

static void prepare_favorites(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_note_order(fixture->state, NOTE_ORDER_FAVORITES_FIRST) ==
                FOLIO_STATE_READY,
            "each favorite fixture starts with stable favorite priority");
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select dedicated favorite note");
    for (size_t at = 0; at < 2; ++at)
        if (folio_state_note_favorite(fixture->state, (struct note_ref){0, at}))
            require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, at}) ==
                        FOLIO_STATE_READY,
                    "prepare both unstarred rows");
}

static void run_favorites_visual(struct native_fixture *_Nonnull fixture)
{
    prepare_favorites(fixture);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "favorite visual fixture");
    pump(window);
    favorite_visibility_pixels(window);
    folio_window_destroy(window);
    require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, 0}) ==
                FOLIO_STATE_READY,
            "filled star for the language and palette matrix");
    favorite_visual_matrix(fixture);
}

static void run_favorites_ui(struct native_fixture *_Nonnull fixture)
{
    prepare_favorites(fixture);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "favorite interaction fixture");
    pump(window);
    favorite_tooltips(window);
    puts("favorites UI: hidden star and touch");
    fflush(stdout);
    favorite_hidden_input(window, fixture);
    puts("favorites UI: reordered rows");
    fflush(stdout);
    favorite_reordered_input(window, fixture);
    puts("favorites UI: cancellation");
    fflush(stdout);
    favorite_cancel_input(window, fixture);
    puts("favorites UI: scope, keyboard and empty results");
    fflush(stdout);
    favorite_scope_input(window, fixture);
    folio_window_destroy(window);
}

static void run_favorites_case(struct native_fixture *_Nonnull fixture)
{
    prepare_favorites(fixture);
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real window for favorites");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_SETSEL, 10, 12);
    favorite_edit_roundtrip(window, fixture, &history);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    SendMessageW(body, EM_SETSEL, 10, 12);
    favorite_edit_roundtrip(window, fixture, &history);
    require(SendMessageW(body, EM_REDO, 0, 0) != 0,
            "Redo remains available after favorites operations");
    pump(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITE);
    pump(window);
    expect_favorite_saved(window, fixture, true);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    folio_state_set_note_scope(fixture->state, NOTE_SCOPE_FAVORITES);
    enum note_order saved_order = folio_state_note_order(fixture->state);
    folio_window_destroy(window);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
    destroy_fixture(fixture);
    *fixture = (struct native_fixture){0};
    create_fixture(fixture);
    require(folio_state_note_favorite(fixture->state, (struct note_ref){0, 0}) &&
                folio_state_note_order(fixture->state) == saved_order &&
                folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL,
            "fresh application reads the star and preference but resets the transient scope");
}

static void run_extended_fixture(struct native_fixture *_Nonnull fixture, int argc,
                                 char *_Nonnull *_Nonnull argv)
{
    if (filter_save_case)
    {
        run_filter_save_case(fixture);
        return;
    }
    if (locked_save_case)
    {
        run_locked_save_case(fixture);
        return;
    }
    if (empty_save_case)
    {
        run_empty_save_case(fixture);
        return;
    }
    if (first_save_modal_case)
    {
        run_first_save_modal_case(fixture);
        return;
    }
    if (first_save_case)
    {
        run_first_save_case(fixture);
        return;
    }
    if (unicode_save_case)
    {
        run_unicode_save_case(fixture);
        return;
    }
    if (move_edit_case)
    {
        run_move_edit_case(fixture);
        return;
    }
    if (replace_save_case)
    {
        run_replace_save_case(fixture);
        return;
    }
    if (rename_case)
    {
        run_rename_case(fixture);
        return;
    }
    if (save_as_case)
    {
        run_save_as_case(fixture);
        return;
    }
    run_other_fixture(fixture, argc, argv);
}

static void run_remaining_fixture(struct native_fixture *_Nonnull fixture, int argc,
                                  char *_Nonnull *_Nonnull argv)
{
    if (favorites_visual_case)
    {
        run_favorites_visual(fixture);
        return;
    }
    if (favorites_ui_case)
    {
        run_favorites_ui(fixture);
        return;
    }
    if (favorites_case)
        run_favorites_case(fixture);
    else if (export_case)
        run_export_case(fixture);
    else if (close_modal_case)
        run_close_modal_case(fixture);
    else if (session_modal_case)
        run_session_modal_case(fixture);
    else if (session_ime_case)
        run_session_ime_case(fixture);
    else if (session_save_case)
        run_session_save_case(fixture);
    else if (session_untitled_case)
        run_session_untitled_case(fixture);
    else
        run_extended_fixture(fixture, argc, argv);
}

static void tags_open(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_request request = {.item = {.kind = kind},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "open tag surface from its visible entry");
    pump(window);
}

static void tags_settle(struct folio_window *_Nonnull window,
                        const struct folio_state *_Nonnull state)
{
    for (size_t step = 0; step < 1000 && folio_window_idle_work_ready(window); ++step)
    {
        folio_window_continue_idle_work(window);
        pump(window);
    }
    require(!folio_state_index_work_pending(state),
            "shared idle completes tag catalog and matching");
}

static struct accessible_item tags_row(struct folio_window *_Nonnull window,
                                       const wchar_t *_Nonnull name)
{
    uint64_t revision = folio_window_accessible_surface_revision(window);
    for (size_t slot = 0; slot < 32; ++slot)
    {
        struct accessible_item item = {
            .kind = ACCESSIBLE_SURFACE_TOGGLE, .slot = slot, .revision = revision};
        struct accessible_view view = {0};
        if (folio_window_accessible_read(window, &item, &view) != S_OK)
            break;
        if (wcsncmp(view.name, name, wcslen(name)) == 0)
            return item;
    }
    require(false, "tag name exists in the public checklist");
    return (struct accessible_item){0};
}

static void tags_toggle(struct folio_window *_Nonnull window, const wchar_t *_Nonnull name)
{
    struct accessible_request request = {.item = tags_row(window, name),
                                         .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK, "toggle the public tag row");
    pump(window);
}

static void tags_add(struct folio_window *_Nonnull window)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr && SetWindowTextW(input, L"新規 😀"),
            "enter an exact Unicode tag name");
    pump(window);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_ADD);
}

static void tags_expect_text(struct folio_window *_Nonnull window, const wchar_t *_Nonnull expected)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    if (wcscmp(snapshot.text, expected) != 0)
        fwprintf(stderr, L"tag actual=[%ls] expected=[%ls]\n", snapshot.text, expected);
    require(wcscmp(snapshot.text, expected) == 0,
            "tag edit preserves complete original metadata and body");
}

static void tags_expect_saved(struct native_fixture *_Nonnull fixture,
                              const char *_Nonnull expected)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *body = nullptr;
    require(disk.read_note(disk.adapter, "schedule", "one", &body) == PERSISTENCE_LOADED &&
                note_text_length(body) == strlen(expected) &&
                memcmp(note_text_bytes(body), expected, strlen(expected)) == 0,
            "saved source exactly retains unrelated metadata, comments and body");
    note_text_destroy(body);
}

static void tags_edit_roundtrip(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    action(window, ACCESSIBLE_EDIT);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetFocus(body);
    SendMessageW(body, EM_SETSEL, 3, 8);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    tags_settle(window, fixture->state);
    const struct tag_catalog *catalog = folio_state_tag_catalog(fixture->state);
    require(tag_catalog_count(catalog) == 3 && tag_catalog_unavailable(catalog) == 1,
            "catalog distinguishes unsupported metadata from known empty tags");
    struct accessible_item stale = tags_row(window, L"red");
    tags_add(window);
    tags_add(window);
    require(note_tags_count(folio_state_edit_tags(fixture->state)) == 3,
            "adding an existing name is idempotent");
    struct accessible_view view = {0};
    require(FAILED(folio_window_accessible_read(window, &stale, &view)),
            "changed candidates invalidate old UIA row generations");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_CANCEL);
    expect_native_body(window, &before);
    tags_expect_saved(fixture, tags_current);
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    tags_add(window);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_APPLY);
    tags_expect_text(window, tags_added);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                SendMessageW(body, EM_GETMODIFY, 0, 0),
            "EDIT tag application stays unsaved");
    tags_expect_saved(fixture, tags_current);
    require(SendMessageW(body, EM_UNDO, 0, 0), "one native Undo removes the entire tag change");
    pump(window);
    tags_expect_text(window, tags_original);
    capture_native_body(window, &before);
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_APPLY);
    expect_native_body(window, &before);
    require(SendMessageW(body, EM_REDO, 0, 0), "unchanged tag apply preserves native Redo");
    pump(window);
    tags_expect_text(window, tags_added);
    require(SendMessageW(body, EM_UNDO, 0, 0), "restore original source before VIEW save");
    pump(window);
}

static void tags_view_refusal(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    tags_add(window);
    HANDLE locked = lock_native_note(fixture);
    begin_history_failure(window);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_APPLY);
    end_history_failure();
    require(CloseHandle(locked), "release dedicated tag save lock");
    require(failure.closed == 1 && folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "VIEW tag save refusal reports once and retains the changed editor");
    tags_expect_text(window, tags_added);
    tags_expect_saved(fixture, tags_current);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_UNDO, 0, 0), "failed VIEW save retains one native Undo");
    pump(window);
    tags_expect_text(window, tags_original);
    action(window, ACCESSIBLE_VIEW);
}

static void tags_view_save(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture)
{
    action(window, ACCESSIBLE_VIEW);
    struct native_body_snapshot rendered = {0};
    capture_native_body(window, &rendered);
    require(wcsstr(rendered.text, L"title:") == nullptr &&
                wcsstr(rendered.text, L"Note 日本語") != nullptr,
            "VIEW hides supported front matter while showing the body");
    tags_view_refusal(window, fixture);
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    tags_add(window);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_APPLY);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW,
            "VIEW tag application returns after common save");
    tags_expect_saved(fixture, tags_saved);
}

static struct accessible_view tags_view(struct folio_window *_Nonnull window,
                                        enum accessible_kind kind, size_t slot)
{
    struct accessible_item item = {
        .kind = kind, .slot = slot, .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen,
            "tag control has visible public bounds");
    return view;
}

static void tags_keyboard(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture)
{
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    tags_settle(window, fixture->state);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    SendMessageW(input, WM_IME_STARTCOMPOSITION, 0, 0);
    favorite_key(window, VK_RETURN);
    favorite_key(window, VK_ESCAPE);
    require(IsWindowVisible(input) && GetFocus() == input, "synthetic IME owns Enter and Escape");
    SendMessageW(input, WM_IME_ENDCOMPOSITION, 0, 0);
    favorite_key(window, VK_TAB);
    struct accessible_item focus = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_WINDOW, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_SURFACE_TOGGLE,
            "Tab reaches a tag checkbox");
    struct accessible_view before = tags_view(window, focus.kind, focus.slot);
    favorite_key(window, VK_SPACE);
    struct accessible_view after = tags_view(window, focus.kind, focus.slot);
    require(after.selected != before.selected, "Space toggles only the focused tag checkbox");
    favorite_key(window, VK_ESCAPE);
    require(!IsWindowVisible(input) && folio_state_edit_tags(fixture->state) == nullptr,
            "Escape outside IME cancels the staged session");
    tags_expect_saved(fixture, tags_saved);
}

static void tags_capture(HWND root, const char *_Nonnull path)
{
    RECT client;
    require(GetClientRect(root, &client), "tag screenshot bounds");
    HDC source = GetDC(root), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(root, dc, PW_CLIENTONLY), "render the real tag window and native input");
    SelectObject(dc, old);
    save_bitmap(dc, bitmap, client, path);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(root, source);
}

static struct accessible_view tags_footer_before(struct folio_window *_Nonnull window)
{
    if (!tags_metrics_case)
        return tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_STATUS,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && view.offscreen &&
                view.name[0] == 0,
            "a clean catalog has no accessible failure announcement before rejection");
    return view;
}

static void tags_regions(struct folio_window *_Nonnull window,
                         const struct accessible_view *_Nonnull footer)
{
    struct accessible_view input = tags_view(window, ACCESSIBLE_COMMAND_INPUT, 0);
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    struct accessible_view button =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_TAG_APPLY);
    require(input.bounds.bottom <= list.bounds.top && list.bounds.bottom <= button.bounds.top &&
                list.bounds.bottom > list.bounds.top,
            "input, a usable tag row and action buttons do not overlap");
    if (!footer->offscreen)
        require(list.bounds.bottom <= footer->bounds.top &&
                    footer->bounds.bottom <= button.bounds.top,
                "a visible footer fits between tag rows and action buttons");
    require(list.checklist && !list.selection_required, "tag list exposes independent toggles");
}

static void tags_geometry(struct folio_window *_Nonnull window, size_t index)
{
    struct accessible_view footer = tags_footer_before(window);
    tags_regions(window, &footer);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_ADD);
    struct accessible_view failure_text = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    require(failure_text.name[0] != 0, "empty tag input has an accessible failure reason");
    tags_regions(window, &failure_text);
    if (tags_metrics_case)
        require(failure_text.bounds.bottom - failure_text.bounds.top >
                    footer.bounds.bottom - footer.bounds.top,
                "an input error allocates new footer height when no catalog notice existed");
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    struct accessible_view button =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_TAG_APPLY);
    printf("tag geometry %zu: list=%ld footer=%ld failure=%ld button=%ld\n", index,
           list.bounds.bottom - list.bounds.top, footer.bounds.bottom - footer.bounds.top,
           failure_text.bounds.bottom - failure_text.bounds.top,
           button.bounds.bottom - button.bounds.top);
    char path[64];
    require(snprintf(path, sizeof path, "tags-error-%zu.bmp", index) > 0, "tag screenshot name");
    tags_capture(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), path);
}

static void tags_filter_geometry(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture, size_t at)
{
    surface_button(window, ACCESSIBLE_SURFACE_TAG_CANCEL);
    action(window, ACCESSIBLE_LAYOUT_DRAWER);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 320, 360, SWP_NOZORDER | SWP_NOACTIVATE),
            "minimum drawer-only tag filter window");
    tags_open(window, ACCESSIBLE_TAG_FILTER);
    tags_settle(window, fixture->state);
    struct accessible_view input = tags_view(window, ACCESSIBLE_COMMAND_INPUT, 0);
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    struct accessible_view footer = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    struct accessible_view button =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_TAG_CLEAR);
    require(input.bounds.bottom <= list.bounds.top && list.bounds.bottom <= footer.bounds.top &&
                footer.bounds.bottom <= button.bounds.top && list.bounds.bottom > list.bounds.top,
            "minimum drawer filter retains separated input, one row and all actions");
    char path[64];
    require(snprintf(path, sizeof path, "tags-filter-%zu.bmp", at) > 0, "filter screenshot path");
    tags_capture(root, path);
}

static void tags_visual_matrix(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t at = 0; at < 9; ++at)
    {
        favorite_high_contrast = at / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[at % 3]) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at / 3 == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                      : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "tag matrix language and palette");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "tag matrix window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 420, 360, SWP_NOZORDER | SWP_NOACTIVATE),
                "minimum tag window size");
        pump(window);
        tags_open(window, ACCESSIBLE_TAG_EDIT);
        tags_settle(window, fixture->state);
        tags_geometry(window, at);
        tags_filter_geometry(window, fixture, at);
        folio_window_destroy(window);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
    favorite_high_contrast = false;
}

static void tags_filter_roundtrip(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    tags_open(window, ACCESSIBLE_TAG_FILTER);
    tags_settle(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_ANY);
    require(folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ANY,
            "OR can be chosen before selecting any tag");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_ALL);
    tags_toggle(window, L"red");
    tags_settle(window, fixture->state);
    require(folio_state_index_filter_count(fixture->state) == 1,
            "one tag matches its saved source");
    tags_toggle(window, L"green");
    tags_settle(window, fixture->state);
    require(folio_state_index_filter_count(fixture->state) == 0,
            "ALL tags produce an explicit empty result");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_ANY);
    tags_settle(window, fixture->state);
    require(folio_state_index_filter_count(fixture->state) == 2,
            "ANY tags match either saved source");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    require(SetWindowTextW(input, L"Other"), "set ordinary global text filter");
    tags_settle(window, fixture->state);
    require(folio_state_index_filter_count(fixture->state) == 1,
            "global text is ANDed with the tag query");
    require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, 0}) ==
                FOLIO_STATE_READY,
            "mark only the different saved source as favorite");
    select_favorite_scope(window, ACCESSIBLE_SCOPE_FAVORITES);
    require(folio_state_index_filter_count(fixture->state) == 0,
            "favorite scope is ANDed with text and tag conditions");
    tags_open(window, ACCESSIBLE_TAG_FILTER);
    tags_settle(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_CLEAR);
    tags_settle(window, fixture->state);
    require(folio_state_index_filter_count(fixture->state) == 3 &&
                folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
                strlen(folio_state_index_filter_term(fixture->state)) == 0,
            "clear all conditions restores even the unsupported source");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    tags_expect_saved(fixture, tags_saved);
}

static void tags_metrics_regression(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "tag notice metrics fixture");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 420, 360, SWP_NOZORDER | SWP_NOACTIVATE),
            "minimum notice regression window");
    pump(window);
    tags_open(window, ACCESSIBLE_TAG_EDIT);
    tags_settle(window, fixture->state);
    require(tag_catalog_unavailable(folio_state_tag_catalog(fixture->state)) == 0,
            "no existing notice reserves error height");
    tags_geometry(window, 9);
    folio_window_destroy(window);
}

static void run_tags_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select dedicated tagged note");
    if (tags_reload_case)
    {
        tags_expect_saved(fixture, tags_current);
        size_t count = 0;
        require(folio_state_note_tag_count(fixture->state, u"", 0, &count) == FOLIO_STATE_READY &&
                    count == 2,
                "fresh process reads retained Markdown tags without reseeding");
        return;
    }
    if (tags_visual_case)
    {
        tags_visual_matrix(fixture);
        return;
    }
    if (tags_metrics_case)
    {
        tags_metrics_regression(fixture);
        return;
    }
    if (folio_state_note_favorite(fixture->state, (struct note_ref){0, 0}))
        require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, 0}) ==
                    FOLIO_STATE_READY,
                "restore unstarred initial tag fixture");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "tag native window on dedicated desktop");
    pump(window);
    tags_edit_roundtrip(window, fixture);
    tags_view_save(window, fixture);
    tags_keyboard(window, fixture);
    tags_filter_roundtrip(window, fixture);
    require(controlled.requests == 0, "tag fixtures start no unrelated media work");
    folio_window_destroy(window);
}

static void management_set_input(struct folio_window *_Nonnull window, const wchar_t *_Nonnull text)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(input != nullptr && SetWindowTextW(input, text), "enter a management name");
    pump(window);
}

static void management_open(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture, const wchar_t *_Nonnull source)
{
    invoke_operation(window, FOLIO_COMMAND_MANAGE_TAGS);
    pump(window);
    tags_settle(window, fixture->state);
    management_set_input(window, source);
    uint64_t revision = folio_window_accessible_surface_revision(window);
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST, .revision = revision};
    struct accessible_item selected = {0};
    require(folio_window_accessible_selected(window, &list, &selected) == S_OK &&
                selected.kind == ACCESSIBLE_SURFACE_CHOICE && selected.slot == 0,
            "management exposes the selected choice through SelectionPattern");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &selected, &view) == S_OK && view.selected &&
                wcsncmp(view.name, source, wcslen(source)) == 0,
            "the selection provider and selected row agree");
}

static void management_preview(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    for (size_t step = 0;
         step < 100 && folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_PREPARING;
         ++step)
    {
        require(folio_window_idle_work_ready(window), "management preparation is scheduled");
        folio_window_continue_idle_work(window);
        pump(window);
    }
    struct tag_management_summary summary = folio_state_tag_management_summary(fixture->state);
    require(summary.phase == TAG_MANAGEMENT_PREVIEW && summary.pending == 2 &&
                summary.skipped == 1 && summary.changed == 0,
            "fresh preview distinguishes two changes and one unsupported note");
    struct accessible_request row = {
        .item = {.kind = ACCESSIBLE_SURFACE_TEXT,
                 .slot = 0,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &row) == S_OK,
            "preview rows are focusable read-only text");
    favorite_key(window, VK_RETURN);
    require(folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_PREVIEW,
            "Enter on a preview row cannot publish changes");
}

static void management_finish(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    for (size_t step = 0;
         step < 100 && folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_APPLYING;
         ++step)
    {
        folio_window_continue_idle_work(window);
        pump(window);
    }
    require(folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_RESULT,
            "one-file idle writes finish at the result surface");
}

static void management_external_source(struct native_fixture *_Nonnull fixture,
                                       const char *_Nonnull text)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\one.md", path);
    require(file_bytes_store(path, text, strlen(text)) == PERSISTENCE_STORED,
            "external fixture changes only its own Markdown file");
}

static void management_history_snapshot(struct native_fixture *_Nonnull fixture,
                                        struct history_snapshot *_Nonnull snapshot)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    snapshot->store_revision = folio_state_store_revision(fixture->state);
    for (size_t version = 0; version <= note_history_depth; ++version)
    {
        enum persistence_outcome result = history_file(disk, version, &snapshot->files[version]);
        require(result == PERSISTENCE_LOADED || result == PERSISTENCE_ABSENT,
                "management captures present and absent history slots");
    }
}

static HANDLE management_lock(struct native_fixture *_Nonnull fixture, const wchar_t *_Nonnull leaf,
                              DWORD sharing)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, leaf, path);
    HANDLE handle = CreateFileW(path, GENERIC_READ, sharing, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    require(handle != INVALID_HANDLE_VALUE, "lock only the dedicated management fixture");
    return handle;
}

static void management_port_refusals(struct native_fixture *_Nonnull fixture,
                                     const struct note_store_request *_Nonnull request)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    HANDLE locked = management_lock(fixture, L"\\schedule\\one.md", 0);
    require(disk.store_note(disk.adapter, request) == NOTE_STORE_SOURCE_UNREADABLE,
            "locked expected source is refused before history");
    require(CloseHandle(locked), "release source read lock");
    management_external_source(fixture, "\xFF");
    require(disk.store_note(disk.adapter, request) == NOTE_STORE_SOURCE_UNREADABLE,
            "invalid UTF-8 expected source is never overwritten");
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\one.md", path);
    require(DeleteFileW(path),
            "remove only the dedicated fixture to simulate an external deletion");
    require(disk.store_note(disk.adapter, request) == NOTE_STORE_SOURCE_CHANGED,
            "missing expected source is not recreated");
    management_external_source(fixture, tags_current);
}

static void management_port_publication(struct native_fixture *_Nonnull fixture,
                                        const struct note_store_request *_Nonnull request)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    require(fixture_store(disk, "schedule", "one", request->expected) == NOTE_STORE_STORED,
            "seed a history slot through the sole save entry");
    HANDLE history = management_lock(fixture, L"\\.history\\schedule\\one\\1.md", FILE_SHARE_READ);
    require(disk.store_note(disk.adapter, request) == NOTE_STORE_HISTORY_FAILED,
            "history rotation failure prevents note publication");
    require(CloseHandle(history), "release fixture history lock");
    tags_expect_saved(fixture, tags_current);
    HANDLE note = management_lock(fixture, L"\\schedule\\one.md", FILE_SHARE_READ);
    require(disk.store_note(disk.adapter, request) == NOTE_STORE_WRITE_FAILED,
            "publication lock is distinguished from history failure");
    require(CloseHandle(note), "release fixture publication lock");
    tags_expect_saved(fixture, tags_current);
    require(disk.store_note(disk.adapter, request) == NOTE_STORE_STORED,
            "the same checked request can retry the actual write");
    tags_expect_saved(fixture, "---\ntags: [managed]\n---\nPublished");
    expect_history_file(disk, 1, request->expected);
    management_external_source(fixture, tags_current);
}

static void management_port_boundaries(struct native_fixture *_Nonnull fixture)
{
    struct note_text *expected = nullptr;
    struct note_text *replacement = nullptr;
    const char target[] = "---\ntags: [managed]\n---\nPublished";
    require(note_text_create(tags_current, sizeof tags_current - 1, &expected) ==
                    NOTE_TEXT_ACCEPTED &&
                note_text_create(target, sizeof target - 1, &replacement) == NOTE_TEXT_ACCEPTED,
            "actual persistence uses immutable expected and replacement sources");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_store_request request = {"schedule", "one", expected, expected};
    struct history_snapshot history = {0};
    management_history_snapshot(fixture, &history);
    require(disk.store_note(disk.adapter, &request) == NOTE_STORE_UNCHANGED,
            "checked equal bytes do not archive or publish");
    verify_history_unchanged(fixture, &history);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
    request.body = replacement;
    management_port_refusals(fixture, &request);
    management_port_publication(fixture, &request);
    note_text_destroy(expected);
    note_text_destroy(replacement);
}

/* Reject the real native boundary without a production-only test seam. */
static LRESULT CALLBACK management_reject_stream(HWND window, UINT message, WPARAM wparam,
                                                 LPARAM lparam)
{
    if (message == EM_STREAMIN)
    {
        EDITSTREAM *stream = (EDITSTREAM *)lparam;
        stream->dwError = ERROR_READ_FAULT;
        management_rejected_streams += 1;
        return 0;
    }
    return CallWindowProcW(management_body_procedure, window, message, wparam, lparam);
}

static void management_save_refused(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture)
{
    struct history_snapshot history = {0};
    management_history_snapshot(fixture, &history);
    begin_expected_failure(window, L"本文読込失敗。同じモードで再試行。");
    struct accessible_request save = {.item = {.kind = ACCESSIBLE_SAVE},
                                      .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &save) == S_OK,
            "invoke ordinary Save against an unavailable native body");
    end_history_failure();
    require(failure.closed == 1, "Save rejects the unavailable body with the existing failure");
    verify_history_unchanged(fixture, &history);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void management_failed_pane(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture)
{
    management_open(window, fixture, L"blue");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    management_preview(window, fixture);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    management_body_procedure =
        (WNDPROC)SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)management_reject_stream);
    require(management_body_procedure != nullptr, "observe the actual native publication boundary");
    uint64_t revision = folio_state_store_revision(fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    folio_window_continue_idle_work(window);
    SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)management_body_procedure);
    pump(window);
    struct tag_management_summary summary = folio_state_tag_management_summary(fixture->state);
    require(management_rejected_streams == 1 && summary.phase == TAG_MANAGEMENT_RESULT &&
                summary.changed == 1 && summary.pending == 1 && summary.failed == 0 &&
                folio_state_store_revision(fixture->state) == revision + 1,
            "failed native adoption retains the publication and stops remaining writes");
    tags_expect_saved(fixture,
                      "---\ntitle: 'Keep' # unknown\ntags: [red] # keep\n---\n# Note 日本語 😀");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    management_save_refused(window, fixture);
    tags_settle(window, fixture->state);
    require(folio_state_store_revision(fixture->state) == revision + 1,
            "ordinary idle cannot restart the stopped publication");
    struct accessible_request recover = {.item = {.kind = ACCESSIBLE_EDIT},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &recover) == S_OK,
            "invoke the existing native recovery action");
    pump(window);
    tags_expect_text(window,
                     L"---\rtitle: 'Keep' # unknown\rtags: [red] # keep\r---\r# Note 日本語 😀");
    require(SendMessageW(body, EM_CANUNDO, 0, 0) == 0 &&
                SendMessageW(body, EM_GETMODIFY, 0, 0) == 0,
            "retrying native adoption uses the already published saved baseline");
}

static void management_native_failure(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "separate window for native failure and recovery");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    management_failed_pane(window, fixture);
    action(window, ACCESSIBLE_VIEW);
    folio_window_destroy(window);
    management_external_source(fixture, tags_current);
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "restore and reload the dedicated source after failure verification");
}

static void management_entry_refusal(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture,
                                     const struct history_snapshot *_Nonnull history)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    invoke_operation(window, FOLIO_COMMAND_MANAGE_TAGS);
    pump(window);
    struct accessible_view notice = tags_view(window, ACCESSIBLE_NOTICE, 0);
    require(folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_CLOSED &&
                wcsstr(notice.name, L"ファイルの内容が変わったため保存していません。") != nullptr,
            "external conflict refuses entry and exposes its inline reason");
    expect_native_body(window, &before);
    verify_history_unchanged(fixture, history);
}

static void management_checked_entry(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    management_external_source(fixture, "---\ntags: [external]\n---\nChanged outside");
    struct history_snapshot history = {0};
    management_history_snapshot(fixture, &history);
    management_entry_refusal(window, fixture, &history);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_SETSEL, (WPARAM)-1, -1);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L" local");
    pump(window);
    management_entry_refusal(window, fixture, &history);
    require(SendMessageW(body, EM_UNDO, 0, 0), "entry conflict retains the real undo record");
    pump(window);
    tags_expect_text(window, tags_original);
    management_external_source(fixture, tags_current);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void management_rename_current(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture)
{
    management_open(window, fixture, L"blue");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_set_input(window, L"green");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    SendMessageW(input, WM_IME_STARTCOMPOSITION, 0, 0);
    favorite_key(window, VK_RETURN);
    favorite_key(window, VK_ESCAPE);
    require(IsWindowVisible(input) && GetFocus() == input &&
                folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_CLOSED,
            "synthetic management IME owns Enter and Escape before preparation");
    SendMessageW(input, WM_IME_ENDCOMPOSITION, 0, 0);
    struct accessible_view notice = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    require(wcsstr(notice.name, L"統合") != nullptr, "existing destination makes merging explicit");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_preview(window, fixture);
    tags_expect_saved(fixture, tags_current);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    require(folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_APPLYING,
            "only explicit apply starts publication");
    folio_window_continue_idle_work(window);
    pump(window);
    struct tag_management_summary summary = folio_state_tag_management_summary(fixture->state);
    require(summary.changed == 1 && summary.pending == 1, "one idle publishes at most one target");
    tags_expect_text(
        window,
        L"---\rtitle: 'Keep' # unknown\rtags: [red, \"green\"] # keep\r---\r# Note 日本語 😀");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SendMessageW(body, EM_CANUNDO, 0, 0) == 0 &&
                SendMessageW(body, EM_GETMODIFY, 0, 0) == 0,
            "selected published source becomes the native saved baseline immediately");
    management_finish(window, fixture);
    summary = folio_state_tag_management_summary(fixture->state);
    require(summary.changed == 2 && summary.skipped == 1 && summary.failed == 0,
            "merge reports two publications and the unsupported source");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    tags_expect_saved(
        fixture,
        "---\ntitle: 'Keep' # unknown\ntags: [red, \"green\"] # keep\n---\n# Note 日本語 😀");
}

static void management_preview_conflict(struct folio_window *_Nonnull window,
                                        struct native_fixture *_Nonnull fixture)
{
    management_open(window, fixture, L"blue");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_set_input(window, L"green");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_preview(window, fixture);
    const char external[] = "---\ntags: [external]\n---\nChanged after review";
    management_external_source(fixture, external);
    struct history_snapshot history = {0};
    management_history_snapshot(fixture, &history);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    folio_window_continue_idle_work(window);
    pump(window);
    require(folio_state_tag_management_status(fixture->state, 0) == TAG_APPLY_CONFLICT,
            "real persistence rechecks the prepared source before history or publication");
    verify_history_unchanged(fixture, &history);
    management_finish(window, fixture);
    struct tag_management_summary summary = folio_state_tag_management_summary(fixture->state);
    require(summary.changed == 1 && summary.skipped == 2 && summary.failed == 0,
            "one external conflict does not block the independent valid note");
    tags_expect_text(window, tags_original);
    tags_expect_saved(fixture, external);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    management_external_source(fixture, tags_current);
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\two.md", path);
    const char other[] = "---\ntags: [blue, green]\n---\nOther";
    require(file_bytes_store(path, other, sizeof other - 1) == PERSISTENCE_STORED,
            "restore the independent fixture after conflict verification");
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void management_stop_retry(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    management_open(window, fixture, L"green");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    management_preview(window, fixture);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    folio_window_continue_idle_work(window);
    pump(window);
    favorite_key(window, VK_ESCAPE);
    struct tag_management_summary summary = folio_state_tag_management_summary(fixture->state);
    require(summary.phase == TAG_MANAGEMENT_RESULT && summary.changed == 1 && summary.pending == 1,
            "Escape stops remaining writes without undoing the published note");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    tags_settle(window, fixture->state);
    summary = folio_state_tag_management_summary(fixture->state);
    require(summary.phase == TAG_MANAGEMENT_PREVIEW && summary.pending == 1 && summary.changed == 0,
            "retry scans again and excludes the already changed note");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_finish(window, fixture);
    require(folio_state_tag_management_summary(fixture->state).changed == 1,
            "fresh retry publishes only the remaining note");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    tags_expect_saved(fixture,
                      "---\ntitle: 'Keep' # unknown\ntags: [red] # keep\n---\n# Note 日本語 😀");
}

static void management_close_cancel(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture)
{
    struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &create) == S_OK,
            "invoke a new untitled note before close cancellation");
    pump(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"unsaved management note");
    pump(window);
    management_open(window, fixture, L"red");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    tags_settle(window, fixture->state);
    require(folio_state_tag_management_summary(fixture->state).pending == 1,
            "untitled body is excluded from the all-note preview");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = cancelled_name,
                                                           .discard = true,
                                                           .action = NAME_DIALOG_CANCEL});
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_CLOSE, 0, 0);
    pump_refused_close(window, folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    require(name_dialog.timer == 0 && name_dialog.closed == 1 && !IsWindow(name_dialog.dialog) &&
                IsWindow(name_dialog.root) && IsWindowEnabled(name_dialog.root) &&
                !IsWindowEnabled(name_dialog.body),
            "cancelled close restores the management surface and keeps its background blocked");
    pump(window);
    struct tag_management_summary summary = folio_state_tag_management_summary(fixture->state);
    require(summary.phase == TAG_MANAGEMENT_RESULT && summary.changed == 0 && summary.pending == 1,
            "cancelled root close cannot resume queued tag publications");
    tags_settle(window, fixture->state);
    require(folio_state_tag_management_summary(fixture->state).changed == 0,
            "later idle work does not restart the stopped batch");
    tags_expect_text(window, L"unsaved management note");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
}

static void management_capture(struct folio_window *_Nonnull window, size_t index, size_t phase)
{
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    struct accessible_view footer = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    struct accessible_view primary =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    struct accessible_view secondary =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_TAG_SECONDARY);
    require(list.bounds.bottom > list.bounds.top && list.bounds.bottom <= footer.bounds.top &&
                footer.bounds.bottom <= primary.bounds.top &&
                primary.bounds.right <= secondary.bounds.left &&
                primary.bounds.bottom == secondary.bounds.bottom,
            "minimum management geometry keeps list, status and both actions separate");
    char path[96];
    require(sprintf_s(path, sizeof path, "tag-management-%zu-%zu.bmp", index, phase) > 0,
            "management screenshot name");
    tags_capture(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), path);
    if (phase == 3 && index < 2)
    {
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(
            RedrawWindow(root, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW),
            "complete pending redraw before the diagnostic second capture");
        require(sprintf_s(path, sizeof path, "tag-management-repaint-%zu.bmp", index) > 0,
                "management repaint screenshot name");
        tags_capture(root, path);
    }
}

static void management_visual_flow(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture, size_t index)
{
    management_open(window, fixture, L"blue");
    management_capture(window, index, 0);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_set_input(window, L"green");
    management_capture(window, index, 1);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_preview(window, fixture);
    management_capture(window, index, 2);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    favorite_key(window, VK_ESCAPE);
    require(folio_state_tag_management_summary(fixture->state).changed == 0,
            "visual result comes from stopping before the first write");
    management_capture(window, index, 3);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
}

static void run_management_visual(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t at = 0; at < 9; ++at)
    {
        favorite_high_contrast = at / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[at % 3]) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at / 3 == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                      : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "management matrix language and palette");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "management matrix window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 420, 360, SWP_NOZORDER | SWP_NOACTIVATE),
                "minimum management window size");
        pump(window);
        management_visual_flow(window, fixture, at);
        folio_window_destroy(window);
    }
    favorite_high_contrast = false;
}

static void management_measure_phase(const char *_Nonnull phase, size_t sample)
{
    printf("{\"phase\":\"%s\",\"sample\":%zu}\n", phase, sample);
    require(fflush(stdout) == 0 && getchar() == '\n',
            "external stopwatch acknowledges the deterministic management phase");
}

static void management_measure_sample(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture, size_t sample)
{
    management_open(window, fixture, L"blue");
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    management_set_input(window, L"green");
    management_measure_phase("ready", sample);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    for (size_t step = 0;
         step < 100 && folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_PREPARING;
         ++step)
    {
        folio_window_continue_idle_work(window);
        pump(window);
    }
    require(folio_state_tag_management_phase(fixture->state) == TAG_MANAGEMENT_PREVIEW,
            "measured preparation finishes the complete preview");
    management_measure_phase("prepared", sample);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_PRIMARY);
    folio_window_continue_idle_work(window);
    pump(window);
    require(folio_state_tag_management_summary(fixture->state).changed == 1,
            "measured first step publishes and adopts the current note");
    management_measure_phase("current", sample);
    management_finish(window, fixture);
    require(folio_state_tag_management_summary(fixture->state).changed == 2,
            "measured completion publishes the independent target");
    management_measure_phase("finished", sample);
    surface_button(window, ACCESSIBLE_SURFACE_TAG_SECONDARY);
}

static void run_management_measure(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "measured management window");
    pump(window);
    for (size_t sample = 0; sample < 7; ++sample)
    {
        management_measure_sample(window, fixture, sample);
        management_external_source(fixture, tags_current);
        wchar_t path[storage_native_capacity];
        native_file_path(fixture, L"\\schedule\\two.md", path);
        const char other[] = "---\ntags: [blue, green]\n---\nOther";
        require(file_bytes_store(path, other, sizeof other - 1) == PERSISTENCE_STORED,
                "restore measurement fixtures outside timed intervals");
    }
    folio_window_destroy(window);
}

static void run_tag_management_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the managed note");
    if (tag_management_measure_case)
    {
        run_management_measure(fixture);
        return;
    }
    if (tag_management_visual_case)
    {
        run_management_visual(fixture);
        return;
    }
    management_port_boundaries(fixture);
    management_native_failure(fixture);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "management window on isolated desktop");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    management_checked_entry(window, fixture);
    management_preview_conflict(window, fixture);
    management_rename_current(window, fixture);
    management_stop_retry(window, fixture);
    management_close_cancel(window, fixture);
    require(controlled.requests == 0, "management starts no unrelated media work");
    folio_window_destroy(window);
}

/* #839 / ADR0098: the copy action of a fenced code block in the real window.
 * --code-copy runs on the isolated clipboard station with the real adapter (clicks only: that
 * station never shows a window, and the product accepts keys only on a visible body).
 * --code-copy-keys and --code-copy-visual run on the private desktop with a recording port, so
 * no case can reach the interactive clipboard. */
static enum clipboard_outcome code_copy_record(struct clipboard_adapter *_Nonnull adapter,
                                               const char *_Nonnull bytes, size_t length)
{
    (void)adapter;
    require(length < sizeof code_copy_recorded, "recorded block fits the fixture buffer");
    memcpy(code_copy_recorded, bytes, length);
    code_copy_recorded[length] = '\0';
    ++code_copy_records;
    return code_copy_result;
}

static bool code_copy_text(HWND _Nonnull owner, wchar_t *_Nonnull out, size_t capacity)
{
    require(OpenClipboard(owner), "inspect the isolated clipboard");
    HANDLE data = GetClipboardData(CF_UNICODETEXT);
    if (data != nullptr)
    {
        const wchar_t *text = GlobalLock(data);
        require(text != nullptr && wcscpy_s(out, capacity, text) == 0,
                "isolated clipboard text fits the fixture buffer");
        GlobalUnlock(data);
    }
    require(CloseClipboard(), "release the isolated clipboard");
    return data != nullptr;
}

static void code_copy_clear(HWND _Nonnull owner)
{
    require(OpenClipboard(owner) && EmptyClipboard() && CloseClipboard(),
            "empty the isolated clipboard");
}

static void code_copy_expect(HWND _Nonnull owner, const wchar_t *_Nullable expected)
{
    wchar_t text[128] = L"";
    bool present = code_copy_text(owner, text, 128);
    if (expected == nullptr)
    {
        require(!present, "nothing reached the isolated clipboard");
        return;
    }
    require(present && wcscmp(text, expected) == 0,
            "the isolated clipboard holds exactly the block: CRLF lines, no trailing newline");
}

/* The isolated station never makes a window visible, so the line is read as the live notice. */
static void code_copy_notice(struct folio_window *_Nonnull window, const wchar_t *_Nonnull expected)
{
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    require(folio_window_accessible_read(window, &item, &view) == S_OK && view.live &&
                !IsRectEmpty(&view.bounds) && wcscmp(view.name, expected) == 0,
            "the result is the live notice line below the body");
}

static LONG code_copy_word(HWND _Nonnull body, size_t nth)
{
    FINDTEXTEXW find = {{0, -1}, L"コピー", {0, 0}};
    for (size_t at = 0; at <= nth; ++at)
    {
        require(SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find) >= 0,
                "the copy word occurrence exists in the native document");
        find.chrg.cpMin = find.chrgText.cpMax;
    }
    return find.chrgText.cpMin;
}

struct code_copy_state
{
    struct native_body_snapshot body;
    POINT scroll;
    uint64_t revision;
};

static void code_copy_capture(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              struct code_copy_state *_Nonnull out)
{
    capture_native_body(window, &out->body);
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), EM_GETSCROLLPOS, 0,
                 (LPARAM)&out->scroll);
    out->revision = folio_state_store_revision(fixture->state);
}

/* Text, dirty, Undo/Redo, scroll, mode and storage. The caller compares the selection. */
static void code_copy_unchanged(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const struct code_copy_state *_Nonnull before)
{
    struct code_copy_state after = {0};
    code_copy_capture(window, fixture, &after);
    require(wcscmp(after.body.text, before->body.text) == 0 &&
                after.body.modified == before->body.modified &&
                after.body.undo == before->body.undo && after.body.redo == before->body.redo,
            "copying preserves all native text units, dirty, Undo and Redo");
    require(after.scroll.x == before->scroll.x && after.scroll.y == before->scroll.y &&
                after.revision == before->revision &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW,
            "copying preserves scroll, storage revision and the view mode");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *stored = nullptr;
    require(
        disk.read_note(disk.adapter, "schedule", "one", &stored) == PERSISTENCE_LOADED &&
            stored != nullptr && note_text_file_length(stored) == strlen(code_copy_current) &&
            memcmp(note_text_file_bytes(stored), code_copy_current, strlen(code_copy_current)) == 0,
        "copying never rewrites the stored Markdown");
    note_text_destroy(stored);
}

static void code_copy_click(struct folio_window *_Nonnull window, LONG position)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    POINTL point = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&point, position + 1);
    LPARAM where = MAKELPARAM(point.x + 2, point.y + 6);
    SendMessageW(body, WM_MOUSEMOVE, 0, where);
    SendMessageW(body, WM_LBUTTONDOWN, MK_LBUTTON, where);
    SendMessageW(body, WM_LBUTTONUP, 0, where);
    pump(window);
}

static void code_copy_key(struct folio_window *_Nonnull window, LONG caret)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetFocus(body);
    SendMessageW(body, EM_SETSEL, (WPARAM)caret, caret);
    native_key_input(window, VK_RETURN, FCONTROL);
}

static void code_copy_pointer_case(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG label = code_copy_word(body, 1);
    CHARRANGE range = {label, label + 3};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&range);
    CHARFORMAT2W format = {.cbSize = sizeof format};
    SendMessageW(body, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
    require((format.dwEffects & CFE_LINK) != 0,
            "the action is a native link, so RichEdit raises EN_LINK for the pointer");
    ENLINK hover = {
        .nmhdr = {.hwndFrom = body, .idFrom = (UINT_PTR)GetDlgCtrlID(body), .code = EN_LINK},
        .msg = WM_SETCURSOR,
        .chrg = range};
    require(SendMessageW(GetParent(body), WM_NOTIFY, hover.nmhdr.idFrom, (LPARAM)&hover) == 1 &&
                GetCursor() == LoadCursorW(nullptr, IDC_HAND),
            "the native link pointer notification shows the action pointer");
}

/* A click on the button copies without leaving the caret in it: the selection is as before. */
static void code_copy_selection(HWND _Nonnull body, CHARRANGE expected)
{
    CHARRANGE selected = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&selected);
    require(selected.cpMin == expected.cpMin && selected.cpMax == expected.cpMax,
            "a click-activated copy restores the selection held before the press");
}

static void code_copy_click_case(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG label = code_copy_word(body, 1);
    SendMessageW(body, EM_SETSEL, 2, 2);
    struct code_copy_state before = {0};
    code_copy_capture(window, fixture, &before);
    code_copy_clear(root);
    code_copy_click(window, label);
    code_copy_expect(root, L"int a;\r\n\tb;");
    code_copy_notice(window, L"コードをコピーしました。");
    code_copy_unchanged(window, fixture, &before);
    code_copy_selection(body, before.body.selection);
    /* A range selected before the click survives it too, and the view does not scroll. */
    CHARRANGE range = {2, 5};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&range);
    code_copy_capture(window, fixture, &before);
    code_copy_click(window, label);
    code_copy_expect(root, L"int a;\r\n\tb;");
    code_copy_unchanged(window, fixture, &before);
    code_copy_selection(body, before.body.selection);
    SendMessageW(body, EM_SETSEL, 2, 2);
    code_copy_capture(window, fixture, &before);
    code_copy_click(window, code_copy_word(body, 2));
    code_copy_expect(root, L"引用 😀\r\n  二行");
    code_copy_unchanged(window, fixture, &before);
    /* The same word in prose is body text, not an action. */
    code_copy_clear(root);
    code_copy_click(window, code_copy_word(body, 0));
    code_copy_click(window, code_copy_word(body, 3));
    code_copy_expect(root, nullptr);
    /* A drag that starts on the action is a selection, not an activation. */
    POINTL point = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&point, label + 1);
    SendMessageW(body, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(point.x + 2, point.y + 6));
    SendMessageW(body, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(point.x + 60, point.y + 40));
    SendMessageW(body, WM_LBUTTONUP, 0, MAKELPARAM(point.x + 60, point.y + 40));
    pump(window);
    code_copy_expect(root, nullptr);
    code_copy_unchanged(window, fixture, &before);
    /* A native range copy keeps the visible words, as for the label: no field instruction. */
    SendMessageW(body, EM_SETSEL, 0, -1);
    SendMessageW(body, WM_COPY, 0, 0);
    code_copy_expect(
        root,
        L"コピー "
        L"前文\r\nc\x2003\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0\xA0\r\n"
        L"int "
        L"a;\r\n\tb;"
        L"\r\n\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0\xA0コピー\xA0\xA0\xA0\xA0\r\n引用 😀\r\n"
        L"  二行"
        L"\r\n終わり コピー");
    code_copy_clear(root);
    SendMessageW(body, EM_SETSEL, 2, 2);
}

/* Another window keeps the clipboard open: the reason is shown inline and nothing changes. */
static void code_copy_busy_case(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND holder = CreateWindowExW(0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                  GetModuleHandleW(nullptr), nullptr);
    require(holder != nullptr, "a second clipboard user on the isolated station");
    code_copy_clear(root);
    LONG label = code_copy_word(body, 1);
    struct code_copy_state before = {0};
    code_copy_capture(window, fixture, &before);
    require(OpenClipboard(holder), "the second user holds the isolated clipboard open");
    code_copy_click(window, label);
    require(CloseClipboard(), "the second user releases the isolated clipboard");
    DestroyWindow(holder);
    code_copy_notice(window, L"ほかのアプリがクリップボードを使用中です。もう一度お試しください。");
    code_copy_expect(root, nullptr);
    code_copy_unchanged(window, fixture, &before);
    code_copy_click(window, label);
    code_copy_expect(root, L"int a;\r\n\tb;");
    code_copy_notice(window, L"コードをコピーしました。");
}

static void code_copy_recorded_expect(const char *_Nullable expected, unsigned records)
{
    if (code_copy_records != records)
        fprintf(stderr, "code copy port calls=%u expected=%u\n", code_copy_records, records);
    require(code_copy_records == records, "the port was reached the expected number of times");
    if (expected != nullptr)
        require(strcmp(code_copy_recorded, expected) == 0,
                "the port received exactly the block: LF lines, no trailing newline");
}

/* In-note search sees the two prose words only, and an open surface accepts no activation. */
static void code_copy_search_case(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG first = code_copy_word(body, 0);
    LONG label = code_copy_word(body, 1);
    LONG last = code_copy_word(body, 3);
    SetFocus(body);
    CHARRANGE initial = {0};
    SendMessageW(body, EM_EXSETSEL, 0, (LPARAM)&initial);
    invoke_operation(window, FOLIO_COMMAND_FIND);
    pump(window);
    search_term(window, L"コピー");
    expect_search_result(window, first, first + 3, L"1 / 2 件");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, last, last + 3, L"2 / 2 件");
    surface_button(window, ACCESSIBLE_SURFACE_NEXT);
    expect_search_result(window, first, first + 3, L"1 / 2 件");
    search_term(window, L"int a");
    LONG code = label + 8; /* the word, four no-break spaces and the paragraph mark */
    expect_search_result(window, code, code + 5, L"1 / 1 件");
    /* The native link notifications arrive while the surface is open and are not accepted. */
    unsigned records = code_copy_records;
    ENLINK notice = {
        .nmhdr = {.hwndFrom = body, .idFrom = (UINT_PTR)GetDlgCtrlID(body), .code = EN_LINK},
        .msg = WM_SETCURSOR,
        .chrg = {label, label + 3}};
    require(SendMessageW(GetParent(body), WM_NOTIFY, notice.nmhdr.idFrom, (LPARAM)&notice) == 1 &&
                GetCursor() == LoadCursorW(nullptr, IDC_IBEAM),
            "an open surface shows the text pointer over the action");
    notice.msg = WM_LBUTTONDOWN;
    SendMessageW(GetParent(body), WM_NOTIFY, notice.nmhdr.idFrom, (LPARAM)&notice);
    notice.msg = WM_LBUTTONUP;
    SendMessageW(GetParent(body), WM_NOTIFY, notice.nmhdr.idFrom, (LPARAM)&notice);
    pump(window);
    code_copy_recorded_expect(nullptr, records);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void code_copy_edit_case(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, code_copy_source, FOLIO_NOTE_SAME);
    code_copy_clear(root);
    code_copy_click(window, 12);
    code_copy_click(window, 0);
    code_copy_expect(root, nullptr);
    expect_editor(window, fixture->state, code_copy_source, FOLIO_NOTE_SAME);
    action(window, ACCESSIBLE_VIEW);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW,
            "an unchanged note returns to the view");
    code_copy_click(window, code_copy_word(body, 1));
    code_copy_expect(root, L"int a;\r\n\tb;");
}

/* Ctrl+Enter with the caret on the action, on a visible body with the recording port. */
static void code_copy_keys_case(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG label = code_copy_word(body, 2);
    SetFocus(body);
    SendMessageW(body, EM_SETSEL, (WPARAM)(label + 1), label + 1);
    struct code_copy_state before = {0};
    code_copy_capture(window, fixture, &before);
    require(native_key_input(window, VK_RETURN, FCONTROL),
            "Ctrl+Enter is handled by the product link accelerator");
    code_copy_recorded_expect("引用 😀\n  二行", 1);
    expect_notice(window, L"コードをコピーしました。");
    code_copy_unchanged(window, fixture, &before);
    expect_native_body(window, &before.body);
    code_copy_key(window, code_copy_word(body, 0) + 1);
    code_copy_key(window, code_copy_word(body, 3) + 1);
    code_copy_recorded_expect(nullptr, 1);
    code_copy_result = CLIPBOARD_UNAVAILABLE;
    code_copy_key(window, code_copy_word(body, 1));
    code_copy_recorded_expect("int a;\n\tb;", 2);
    expect_notice(window, L"クリップボードへコピーできませんでした。");
    code_copy_result = CLIPBOARD_OUT_OF_MEMORY;
    code_copy_key(window, code_copy_word(body, 1) + 2);
    code_copy_recorded_expect("int a;\n\tb;", 3);
    expect_notice(window, L"記憶域が足りません。");
    code_copy_result = CLIPBOARD_WRITTEN;
    action(window, ACCESSIBLE_EDIT);
    expect_editor(window, fixture->state, code_copy_source, FOLIO_NOTE_SAME);
    /* While editing, Ctrl+Enter is the editor's own key and never a copy. */
    code_copy_key(window, 12);
    code_copy_recorded_expect(nullptr, 3);
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
    expect_editor(window, fixture->state, code_copy_source, FOLIO_NOTE_SAME);
    action(window, ACCESSIBLE_VIEW);
}

static size_t code_copy_count(const DWORD *_Nonnull pixels, RECT client, RECT area, DWORD wanted)
{
    RECT shared;
    if (!IntersectRect(&shared, &client, &area))
        return 0;
    size_t count = 0;
    for (LONG y = shared.top; y < shared.bottom; ++y)
        for (LONG x = shared.left; x < shared.right; ++x)
            count += (pixels[(size_t)y * (size_t)client.right + (size_t)x] & 0xFFFFFF) == wanted;
    return count;
}

/* Pixels of one colour inside a client rectangle of the body, as the window prints itself. */
static size_t code_copy_pixels(HWND _Nonnull body, RECT area, struct rgb_color color)
{
    /* The top-level window prints its children; the area is moved into its client coordinates. */
    HWND root = GetAncestor(body, GA_ROOT);
    RECT client;
    GetClientRect(root, &client);
    MapWindowPoints(body, root, (POINT *)&area, 2);
    BITMAPINFO info = {.bmiHeader = {.biSize = sizeof(BITMAPINFOHEADER),
                                     .biWidth = client.right,
                                     .biHeight = -client.bottom,
                                     .biPlanes = 1,
                                     .biBitCount = 32,
                                     .biCompression = BI_RGB}};
    void *bits = nullptr;
    HDC screen = GetDC(root);
    HDC memory = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    require(memory != nullptr && bitmap != nullptr && bits != nullptr, "body capture surface");
    HGDIOBJ old = SelectObject(memory, bitmap);
    require(PrintWindow(root, memory, PW_CLIENTONLY), "print the window with its decoration");
    DWORD wanted = ((DWORD)color.red << 16) | ((DWORD)color.green << 8) | color.blue;
    size_t count = code_copy_count(bits, client, area, wanted);
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ReleaseDC(root, screen);
    return count;
}

static RECT code_copy_button_rect(HWND _Nonnull body, LONG word)
{
    POINTL left = {0}, right = {0}, below = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&left, word - 12);
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&right, word + 7);
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&below, word + 8);
    return (RECT){left.x, left.y, right.x, below.y};
}

/* #839 round 6: the painted button over the in-document tab. Rest shows the icon on the code
 * surface, the pointer adds the border and the label, a successful copy shows the done colour
 * until the pane's timer, and repeated loads keep the GDI object count. */
static void code_copy_decoration_case(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct rtf_palette colors = folio_state_palette(fixture->state)->rtf;
    RECT button = code_copy_button_rect(body, code_copy_word(body, 1));
    size_t area = (size_t)(button.right - button.left) * (size_t)(button.bottom - button.top);
    RECT top = {button.left, button.top, button.right, button.top + 1};
    require(area > 0 && code_copy_pixels(body, button, colors.code_background) > area / 2 &&
                code_copy_pixels(body, button, colors.code_comment) < area / 4 &&
                code_copy_pixels(body, top, colors.code_comment) == 0,
            "at rest the button covers the fallback tab with the code surface and no border");
    LPARAM inside = MAKELPARAM(button.left + 3, button.top + 3);
    SendMessageW(body, WM_MOUSEMOVE, 0, inside);
    require(code_copy_pixels(body, top, colors.code_comment) ==
                    (size_t)(button.right - button.left) &&
                code_copy_pixels(body, button, colors.code_text) > 0,
            "the pointer adds the border and the label");
    SendMessageW(body, WM_MOUSELEAVE, 0, 0);
    require(code_copy_pixels(body, top, colors.code_comment) == 0 &&
                code_copy_pixels(body, button, colors.code_text) == 0,
            "leaving returns the quiet icon");
    unsigned records = code_copy_records;
    code_copy_click(window, code_copy_word(body, 1));
    require(code_copy_records == records + 1 &&
                code_copy_pixels(body, button, colors.code_string) > 0,
            "a successful copy shows the done look");
    SendMessageW(body, WM_TIMER, note_pane_code_button_timer_id, 0);
    require(code_copy_pixels(body, button, colors.code_string) == 0,
            "the done look ends with the pane's timer");
    code_copy_result = CLIPBOARD_BUSY;
    code_copy_click(window, code_copy_word(body, 1));
    code_copy_result = CLIPBOARD_WRITTEN;
    require(code_copy_pixels(body, button, colors.code_string) == 0,
            "a failed copy keeps the normal look");
    SendMessageW(body, WM_MOUSELEAVE, 0, 0);
    DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (unsigned round = 0; round < 5; ++round)
    {
        action(window, ACCESSIBLE_EDIT);
        action(window, ACCESSIBLE_VIEW);
        SendMessageW(body, WM_MOUSEMOVE, 0, inside);
        SendMessageW(body, WM_MOUSELEAVE, 0, 0);
        require(code_copy_pixels(body, button, colors.code_background) > area / 2,
                "the decoration returns after every reload");
    }
    require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before,
            "repeated loads and paints keep the GDI object count");
}

/* One synthetic click at a client point of the body; true when the recording port was reached. */
static bool code_copy_hit(struct folio_window *_Nonnull window, LONG x, LONG y)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    unsigned before = code_copy_records;
    LPARAM where = MAKELPARAM(x, y);
    SendMessageW(body, WM_MOUSEMOVE, 0, where);
    SendMessageW(body, WM_LBUTTONDOWN, MK_LBUTTON, where);
    SendMessageW(body, WM_LBUTTONUP, 0, where);
    pump(window);
    return code_copy_records == before + 1;
}

/* The chip of the first block: its extent in the document, and the pixels that activate it. */
static void code_copy_target_case(struct folio_window *_Nonnull window, int zoom)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG word = code_copy_word(body, 1);
    POINTL left = {0}, right = {0}, text = {0};
    /* Bring the whole row into view: an earlier case may have scrolled the narrow window. */
    POINT origin = {0};
    SendMessageW(body, EM_SETSCROLLPOS, 0, (LPARAM)&origin);
    pump(window);
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&left, word - 12);
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&right, word + 7);
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&text, word);
    RECT client;
    GetClientRect(body, &client);
    require(SendMessageW(body, EM_EXLINEFROMCHAR, 0, word - 12) ==
                    SendMessageW(body, EM_EXLINEFROMCHAR, 0, word + 6) &&
                right.x > left.x && right.x > client.right / 2,
            "the chip stays on one row at the right side of the block");
    LONG middle = (left.x + right.x) / 2;
    LONG top = left.y;
    while (top > left.y - 40 && code_copy_hit(window, middle, top - 1))
        --top;
    LONG bottom = left.y;
    while (bottom < left.y + 80 && code_copy_hit(window, middle, bottom + 1))
        ++bottom;
    printf("code copy chip target: zoom=%d dpi=%u width=%ld height=%ld text_inset=%ld "
           "right_gap=%ld\n",
           zoom, GetDpiForWindow(body), right.x - left.x, bottom - top + 1, text.x - left.x,
           client.right - right.x);
    fflush(stdout);
    require(code_copy_hit(window, left.x + 1, left.y + 2) &&
                code_copy_hit(window, right.x - 2, left.y + 2),
            "the padding on both sides of the word activates the copy");
    require(code_copy_hit(window, middle, top) && code_copy_hit(window, middle, bottom) &&
                bottom - top + 1 >= 12,
            "the row activates the copy from its top edge to its bottom edge");
    require(!code_copy_hit(window, left.x - 6, left.y + 2),
            "the gap between the label and the chip is not part of the action");
}

/* The same chip pressed repeatedly reports every time, also after a different result line. */
static void code_copy_repeat_case(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG word = code_copy_word(body, 1);
    unsigned records = code_copy_records;
    code_copy_click(window, word);
    expect_notice(window, L"コードをコピーしました。");
    code_copy_click(window, word);
    code_copy_recorded_expect("int a;\n\tb;", records + 2);
    expect_notice(window, L"コードをコピーしました。");
    code_copy_result = CLIPBOARD_BUSY;
    code_copy_click(window, word);
    expect_notice(window, L"ほかのアプリがクリップボードを使用中です。もう一度お試しください。");
    code_copy_result = CLIPBOARD_WRITTEN;
    code_copy_click(window, word);
    code_copy_recorded_expect("int a;\n\tb;", records + 4);
    expect_notice(window, L"コードをコピーしました。");
    /* The document order read by assistive technology stays label, action, code. */
    FINDTEXTEXW label = {{0, -1}, L"c\x2003", {0, 0}};
    FINDTEXTEXW code = {{0, -1}, L"int a;", {0, 0}};
    require(SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&label) >= 0 &&
                SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&code) >= 0 &&
                label.chrgText.cpMin < word && word < code.chrgText.cpMin,
            "the document order is language label, copy action, code");
}

static void code_copy_zoom_case(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    wchar_t before[192], after[192];
    GetWindowTextW(body, before, 192);
    require(SetWindowPos(root, nullptr, 20, 20, 420, 360, SWP_NOZORDER | SWP_NOACTIVATE),
            "minimum window size");
    pump(window);
    zoom_wheel(window, 5 * WHEEL_DELTA, 150);
    GetWindowTextW(body, after, 192);
    LONG label = code_copy_word(body, 1);
    FINDTEXTEXW find = {{0, -1}, L"int a;", {0, 0}};
    require(wcscmp(before, after) == 0 &&
                SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN, (LPARAM)&find) >= 0 &&
                SendMessageW(body, EM_EXLINEFROMCHAR, 0, label) <
                    SendMessageW(body, EM_EXLINEFROMCHAR, 0, find.chrgText.cpMin),
            "narrow enlarged layout keeps the header row above its code without losing text");
    unsigned records = code_copy_records;
    code_copy_click(window, label);
    code_copy_recorded_expect("int a;\n\tb;", records + 1);
    code_copy_key(window, code_copy_word(body, 2) + 2);
    code_copy_recorded_expect("引用 😀\n  二行", records + 2);
    code_copy_target_case(window, 150);
    /* Scrolling keeps the button on its block: the fallback tab never shows at the new place. */
    struct rtf_palette colors = folio_state_palette(fixture->state)->rtf;
    RECT still = code_copy_button_rect(body, label);
    POINT scrolled = {0, 24};
    SendMessageW(body, EM_SETSCROLLPOS, 0, (LPARAM)&scrolled);
    pump(window);
    RECT moved = code_copy_button_rect(body, label);
    size_t area = (size_t)(moved.right - moved.left) * (size_t)(moved.bottom - moved.top);
    require(moved.top == still.top - 24 &&
                code_copy_pixels(body, moved, colors.code_background) > area / 2 &&
                code_copy_pixels(body, moved, colors.code_comment) < area / 4,
            "after a scroll the button is painted at the block's new position");
    LONG quoted = code_copy_word(body, 2);
    POINTL edge = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&edge, quoted + 7);
    RECT client;
    GetClientRect(body, &client);
    require(SendMessageW(body, EM_EXLINEFROMCHAR, 0, quoted - 12) ==
                    SendMessageW(body, EM_EXLINEFROMCHAR, 0, quoted + 6) &&
                edge.x > client.right / 2,
            "the chip of a quoted block is not split and stays at the right at 420px and 150%");
    zoom_wheel(window, -5 * WHEEL_DELTA, 100);
}

/* Quote and list blocks at the minimum width, at 100% and enlarged. */
static void code_copy_narrow_visual(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY &&
                folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                folio_state_set_theme(fixture->state, FOLIO_THEME_CHOICE_LIGHT) ==
                    FOLIO_STATE_READY,
            "narrow copy action language and palette");
    /* The window minimum is wider than 420px, so the drawer stays open to narrow the body. */
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "narrow copy action window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 420, 1000, SWP_NOZORDER | SWP_NOACTIVATE),
            "narrow copy action size");
    pump(window);
    tags_capture(root, "code-copy-narrow-ja-light.bmp");
    zoom_wheel(window, 5 * WHEEL_DELTA, 150);
    tags_capture(root, "code-copy-narrow-150-ja-light.bmp");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    FINDTEXTEXW find = {{0, -1}, L"コピー", {0, 0}};
    require(SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find) >= 0,
            "the first copy action at narrow width");
    POINTL point = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&point, find.chrgText.cpMin);
    SendMessageW(body, WM_MOUSEMOVE, 0, MAKELPARAM(point.x + 2, point.y + 6));
    tags_capture(root, "code-copy-hover-narrow-150-ja-light.bmp");
    code_copy_click(window, find.chrgText.cpMin);
    tags_capture(root, "code-copy-done-narrow-150-ja-light.bmp");
    SendMessageW(body, WM_TIMER, note_pane_code_button_timer_id, 0);
    zoom_wheel(window, -5 * WHEEL_DELTA, 100);
    folio_window_destroy(window);
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
}

static void code_copy_capture_state(HWND root, const char *_Nonnull state,
                                    const char *_Nonnull language, const char *_Nonnull look)
{
    char path[64];
    require(snprintf(path, sizeof path, "code-copy-%s-%s-%s.bmp", state, language, look) > 0,
            "copy action screenshot path");
    tags_capture(root, path);
}

/* Rest, pointer over the first button, and the done look after a recorded copy. High contrast
 * is not decorated, so only its rest look is captured. */
static void code_copy_state_captures(struct folio_window *_Nonnull window,
                                     const char *_Nonnull language, const char *_Nonnull look,
                                     size_t at)
{
    static const wchar_t *const words[] = {L"コピー", L"Copy", L"复制"};
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    code_copy_capture_state(root, "rest", language, look);
    if (at / 3 == 2)
        return;
    FINDTEXTEXW find = {{0, -1}, words[at % 3], {0, 0}};
    require(SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find) >= 0,
            "the first copy action of the gallery");
    POINTL point = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&point, find.chrgText.cpMin);
    /* No pump here: the real pointer is elsewhere, so the queued WM_MOUSELEAVE would clear it. */
    SendMessageW(body, WM_MOUSEMOVE, 0, MAKELPARAM(point.x + 2, point.y + 6));
    code_copy_capture_state(root, "hover", language, look);
    code_copy_click(window, find.chrgText.cpMin);
    code_copy_capture_state(root, "done", language, look);
}

static void code_copy_visual(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    const char *const names[] = {"ja", "en", "zh"};
    const char *const looks[] = {"light", "dark", "contrast"};
    for (size_t at = 0; at < 9; ++at)
    {
        favorite_high_contrast = at / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[at % 3]) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at / 3 == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                      : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "copy action matrix language and palette");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "copy action matrix window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 640, 1000, SWP_NOZORDER | SWP_NOACTIVATE),
                "copy action matrix size");
        pump(window);
        code_copy_state_captures(window, names[at % 3], looks[at / 3], at);
        folio_window_destroy(window);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
    favorite_high_contrast = false;
    code_copy_narrow_visual(fixture);
}

/* #839 round 8: every header row that is fully inside the pane shows its icon after any scroll
 * path. The expected place comes from the row's own first and last character only. */
static unsigned code_copy_scroll_rows(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture,
                                      const char *_Nonnull path)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    struct rgb_color muted = folio_state_palette(fixture->state)->rtf.code_comment;
    RECT client;
    GetClientRect(body, &client);
    int numerator = 0, denominator = 0;
    SendMessageW(body, EM_GETZOOM, (WPARAM)&numerator, (LPARAM)&denominator);
    int zoom = denominator != 0 ? MulDiv(100, numerator, denominator) : 100;
    LONG row = MulDiv(MulDiv(360, (int)GetDpiForWindow(body), 1440), zoom, 100);
    LONG icon = MulDiv(MulDiv(20, (int)GetDpiForWindow(body), 96), zoom, 100);
    unsigned visible = 0;
    for (size_t block = 0; block < 5; ++block)
    {
        LONG word = code_copy_word(body, block);
        POINTL start = {0}, end = {0};
        SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&start, word - 12);
        SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&end, word + 7);
        if (start.y < 0 || start.y + row > client.bottom || end.y != start.y)
            continue;
        ++visible;
        RECT place = {end.x - icon, start.y, end.x, start.y + row};
        if (code_copy_pixels(body, place, muted) < 20)
        {
            fprintf(stderr, "scroll path %s: block %zu row at y=%ld has no icon\n", path, block,
                    start.y);
            require(false, "a fully visible header row shows its copy icon");
        }
    }
    return visible;
}

static void code_copy_scroll_bar(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture, WPARAM request,
                                 const char *_Nonnull path)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    unsigned seen = 0;
    for (unsigned step = 0; step < 25; ++step)
    {
        SendMessageW(body, WM_VSCROLL, request, 0);
        pump(window);
        seen += code_copy_scroll_rows(window, fixture, path);
    }
    require(seen > 0, "the scroll path showed header rows to check");
}

static void code_copy_scroll_keys(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SetFocus(body);
    const UINT keys[] = {VK_NEXT, VK_NEXT, VK_PRIOR, VK_DOWN, VK_DOWN,
                         VK_DOWN, VK_UP,   VK_END,   VK_HOME};
    const BYTE modifiers[] = {0, 0, 0, 0, 0, 0, 0, FCONTROL, FCONTROL};
    for (size_t at = 0; at < sizeof keys / sizeof keys[0]; ++at)
    {
        native_key_input(window, keys[at], modifiers[at]);
        (void)code_copy_scroll_rows(window, fixture, "keys");
    }
}

static void code_copy_scroll_positions(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    unsigned seen = 0;
    for (LONG y = 0; y < 900; y += 31)
    {
        POINT origin = {0, y};
        SendMessageW(body, EM_SETSCROLLPOS, 0, (LPARAM)&origin);
        pump(window);
        seen += code_copy_scroll_rows(window, fixture, "EM_SETSCROLLPOS");
    }
    require(seen > 0, "pixel scrolling showed header rows to check");
    POINT top = {0, 0};
    SendMessageW(body, EM_SETSCROLLPOS, 0, (LPARAM)&top);
    pump(window);
}

static void run_code_copy_scroll(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "code copy scroll window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(SetWindowPos(root, nullptr, 20, 20, 900, 560, SWP_NOZORDER | SWP_NOACTIVATE),
            "short code copy window");
    pump(window);
    require(code_copy_scroll_rows(window, fixture, "load") > 0, "the first rows are decorated");
    code_copy_scroll_bar(window, fixture, SB_LINEDOWN, "SB_LINEDOWN");
    code_copy_scroll_bar(window, fixture, SB_LINEUP, "SB_LINEUP");
    code_copy_scroll_bar(window, fixture, SB_PAGEDOWN, "SB_PAGEDOWN");
    code_copy_scroll_bar(window, fixture, SB_PAGEUP, "SB_PAGEUP");
    SendMessageW(body, WM_VSCROLL, SB_BOTTOM, 0);
    pump(window);
    (void)code_copy_scroll_rows(window, fixture, "SB_BOTTOM");
    SendMessageW(body, WM_VSCROLL, SB_TOP, 0);
    pump(window);
    (void)code_copy_scroll_rows(window, fixture, "SB_TOP");
    code_copy_scroll_positions(window, fixture);
    code_copy_scroll_keys(window, fixture);
    zoom_wheel(window, 5 * WHEEL_DELTA, 150);
    code_copy_scroll_positions(window, fixture);
    code_copy_scroll_bar(window, fixture, SB_LINEDOWN, "SB_LINEDOWN at 150%");
    zoom_wheel(window, -5 * WHEEL_DELTA, 100);
    require(SetWindowPos(root, nullptr, 20, 20, 1100, 420, SWP_NOZORDER | SWP_NOACTIVATE),
            "resized code copy window");
    pump(window);
    code_copy_scroll_positions(window, fixture);
    action(window, ACCESSIBLE_LAYOUT_NOTE);
    code_copy_scroll_positions(window, fixture);
    action(window, ACCESSIBLE_LAYOUT_BOTH);
    code_copy_scroll_bar(window, fixture, SB_LINEDOWN, "SB_LINEDOWN after layout switch");
    folio_window_destroy(window);
}

static void run_code_copy_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated code note");
    if (code_copy_visual_case)
    {
        code_copy_visual(fixture);
        return;
    }
    if (code_copy_scroll_case)
    {
        run_code_copy_scroll(fixture);
        return;
    }
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real code copy window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1000, 750, SWP_NOZORDER | SWP_NOACTIVATE),
            "fixed code copy window");
    pump(window);
    code_copy_pointer_case(window);
    if (code_copy_keys_case_selected)
    {
        code_copy_keys_case(window, fixture);
        code_copy_search_case(window);
        code_copy_decoration_case(window, fixture);
        code_copy_target_case(window, 100);
        code_copy_repeat_case(window);
        code_copy_zoom_case(window, fixture);
    }
    else
    {
        code_copy_click_case(window, fixture);
        code_copy_busy_case(window, fixture);
        code_copy_edit_case(window, fixture);
    }
    require(controlled.requests == 0, "copying code does not request image work");
    folio_window_destroy(window);
}

static void run_existing_fixture(struct native_fixture *_Nonnull fixture, int argc,
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
        run_remaining_fixture(fixture, argc, argv);
}

static void discovery_settle(struct folio_window *_Nonnull window,
                             struct folio_state *_Nonnull state)
{
    for (size_t step = 0; step < 1000 && folio_window_idle_work_ready(window); ++step)
    {
        folio_window_continue_idle_work(window);
        pump(window);
    }
    require(!folio_state_discovery_work_pending(state),
            "discovery idle reaches a stable result and preview");
}

static void discovery_query(struct folio_window *_Nonnull window,
                            struct folio_state *_Nonnull state, const wchar_t *_Nonnull query)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(SetWindowTextW(input, query), "native discovery query accepts text");
    pump(window);
    discovery_settle(window, state);
}

static struct accessible_item discovery_row(struct folio_window *_Nonnull window, size_t at)
{
    return (struct accessible_item){.kind = ACCESSIBLE_SURFACE_CHOICE,
                                    .slot = at,
                                    .revision = folio_window_accessible_surface_revision(window)};
}

static void discovery_choose(struct folio_window *_Nonnull window, size_t at)
{
    struct accessible_request request = {.item = discovery_row(window, at),
                                         .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "UIA selects a discovery candidate without opening");
    pump(window);
}

static void discovery_body_unchanged(struct folio_window *_Nonnull window,
                                     const struct native_body_snapshot *_Nonnull before)
{
    struct native_body_snapshot expected = *before;
    expected.focus = GetFocus();
    expect_native_body(window, &expected);
}

static void discovery_preview_text(struct folio_window *_Nonnull window,
                                   const wchar_t *_Nonnull expected)
{
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    wchar_t text[512];
    require(preview != nullptr && IsWindowVisible(preview) &&
                GetWindowTextW(preview, text, 512) >= 0 && wcscmp(text, expected) == 0,
            "readonly native preview shows exact logical source lines");
    require((GetWindowLongPtrW(preview, GWL_STYLE) & ES_READONLY) != 0,
            "source preview cannot edit the stored note");
    SendMessageW(preview, WM_CHAR, L'X', 0);
    require(GetWindowTextW(preview, text, 512) >= 0 && wcscmp(text, expected) == 0,
            "native character input does not mutate the source preview");
}

static void discovery_selection_boundary(struct folio_window *_Nonnull window,
                                         struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_SETSEL, 3, 8);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    require(folio_state_discovery_summary(fixture->state).shown == 3,
            "empty name query lists every saved note without body filtering");
    struct accessible_item original = discovery_row(window, 0);
    uint64_t generation = original.revision;
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    require(folio_window_accessible_surface_revision(window) == generation,
            "selection and preview do not replace the UIA result/status generation");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &original, &view) == S_OK,
            "same-result UIA identities survive selection and preview");
    discovery_preview_text(window, L"---\r\ntags: [blue, green]\r\n---\r\nOther");
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    SendMessageW(layer, WM_KEYDOWN, VK_SPACE, 0);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "Space on a result does not open a different document");
    discovery_body_unchanged(window, &before);
    discovery_query(window, fixture->state, L"zzzzmissing");
    require(folio_state_discovery_summary(fixture->state).shown == 0 &&
                !IsWindowVisible(
                    folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW)) &&
                FAILED(folio_window_accessible_read(window, &original, &view)),
            "new empty results discard old preview and old UIA result identity");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    discovery_body_unchanged(window, &before);
}

static void discovery_ime_boundary(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    uint64_t before = folio_state_discovery_summary(fixture->state).results_revision;
    SendMessageW(input, WM_IME_STARTCOMPOSITION, 0, 0);
    require(SetWindowTextW(input, L"two"), "stage synthetic IME input");
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    SendMessageW(input, WM_KEYDOWN, VK_ESCAPE, 0);
    SendMessageW(input, WM_KEYDOWN, VK_DOWN, 0);
    require(folio_state_discovery_summary(fixture->state).results_revision == before &&
                folio_state_discovery_summary(fixture->state).shown == 3,
            "synthetic active composition does not query, open, close or move candidates");
    SendMessageW(input, WM_IME_ENDCOMPOSITION, 0, 0);
    pump(window);
    discovery_settle(window, fixture->state);
    require(folio_state_discovery_summary(fixture->state).shown == 1,
            "composition completion applies only the current committed query");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED &&
                strcmp(folio_state_document_name(fixture->state), "two") == 0,
            "explicit Open adopts the selected saved document");
}

static void discovery_untitled_boundary(struct folio_window *_Nonnull window,
                                        struct native_fixture *_Nonnull fixture)
{
    struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &create) == S_OK,
            "create a private untitled discovery draft");
    pump(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"discovery draft");
    pump(window);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_query(window, fixture->state, L"one");
    discovery_body_unchanged(window, &before);
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = cancelled_name,
                                                           .discard = true,
                                                           .action = NAME_DIALOG_CANCEL});
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(name_dialog.timer == 0 && name_dialog.closed == 1 && !IsWindowEnabled(body) &&
                folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY,
            "cancelled departure keeps the draft and discovery surface");
    discovery_body_unchanged(window, &before);
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = L"discovery-saved",
                                                           .discard = true,
                                                           .action = NAME_DIALOG_SAVE});
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    finish_name_dialog();
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "saved draft insertion still opens the owned original destination identity");
    struct note_identity *saved = nullptr;
    struct note_ref saved_ref = {0};
    require(note_identity_create("target", "discovery-saved", &saved) == NOTE_IDENTITY_READY &&
                folio_state_resolve_note(fixture->state, saved, &saved_ref) == FOLIO_STATE_READY,
            "the exact draft destination was created in the prompt category");
    note_identity_destroy(saved);
    require(folio_state_trash_note(fixture->state, saved_ref, u"", 0) == FOLIO_STATE_READY,
            "remove only this fixture's saved draft through the common note operation");
}

static void discovery_named_save(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_SETSEL, (WPARAM)-1, -1);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L" saved");
    pump(window);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    struct history_snapshot history = {0};
    management_history_snapshot(fixture, &history);
    HANDLE locked = lock_native_note(fixture);
    begin_history_failure(window);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    end_history_failure();
    require(CloseHandle(locked), "release only the discovery fixture's save lock");
    require(failure.closed == 1 &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "failed named save refuses discovery entry");
    discovery_body_unchanged(window, &before);
    verify_history_unchanged(fixture, &history);
    tags_expect_saved(fixture, tags_current);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    tags_expect_saved(
        fixture,
        "---\ntitle: 'Keep' # unknown\ntags: [red, 'blue'] # keep\n---\n# Note 日本語 😀 saved");
    require(folio_state_store_revision(fixture->state) == history.store_revision + 1,
            "successful named entry saves exactly once before searching");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    require(SendMessageW(body, EM_UNDO, 0, 0), "entry save preserves native undo");
    pump(window);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    tags_expect_saved(fixture, tags_current);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static LRESULT CALLBACK discovery_reject_preview(HWND window, UINT message, WPARAM wparam,
                                                 LPARAM lparam)
{
    if (message == WM_SETTEXT && ((const wchar_t *)lparam)[0] != L'\0')
    {
        discovery_rejected_text += 1;
        return FALSE;
    }
    return CallWindowProcW(discovery_preview_procedure, window, message, wparam, lparam);
}

static void discovery_failed_preview(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    discovery_preview_procedure =
        (WNDPROC)SetWindowLongPtrW(preview, GWLP_WNDPROC, (LONG_PTR)discovery_reject_preview);
    require(discovery_preview_procedure != nullptr, "observe the real preview text boundary");
    require(SendMessageW(preview, WM_SETTEXT, 0, (LPARAM)L"probe rejection") == FALSE,
            "native message rejection reaches the caller");
    discovery_rejected_text = 0;
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    SetWindowLongPtrW(preview, GWLP_WNDPROC, (LONG_PTR)discovery_preview_procedure);
    struct accessible_view status = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    require(discovery_rejected_text == 1 && !IsWindowVisible(preview) &&
                wcsstr(status.name, L"本文読込失敗") != nullptr,
            "native text rejection clears the old preview and exposes its failure");
    discovery_body_unchanged(window, &before);
    discovery_choose(window, 0);
    discovery_settle(window, fixture->state);
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    discovery_preview_text(window, L"---\r\ntags: [blue, green]\r\n---\r\nOther");
    status = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    require(wcsstr(status.name, L"本文読込失敗") == nullptr,
            "successful new preview clears the previous native failure");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void discovery_unreadable_query(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture)
{
    HANDLE locked = management_lock(fixture, L"\\schedule\\two.md", 0);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_query(window, fixture->state, L"two");
    struct discovery_summary summary = folio_state_discovery_summary(fixture->state);
    require(summary.shown == 1 && summary.unreadable == 1 &&
                summary.preview == DISCOVERY_PREVIEW_UNREADABLE,
            "name match remains available while its saved source is unreadable");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "unreadable destination cannot replace the original document");
    require(CloseHandle(locked), "release only the fixture's unreadable source");
    wchar_t limit[514];
    for (size_t at = 0; at < 513; ++at)
        limit[at] = L'x';
    limit[513] = L'\0';
    discovery_query(window, fixture->state, limit);
    struct accessible_view open =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    struct accessible_view status = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    require(!open.enabled && wcsstr(status.name, L"256文字以内") != nullptr &&
                folio_state_discovery_summary(fixture->state).shown == 0,
            "over-limit native input disables Open and discards old results");
    discovery_query(window, fixture->state, L"one");
    require(folio_state_discovery_summary(fixture->state).shown == 1,
            "a corrected query recovers without reopening the surface");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void discovery_seed_conditions(struct native_fixture *_Nonnull fixture)
{
    struct note_tag *red = nullptr, *blue = nullptr;
    struct note_tags *tags = nullptr;
    require(note_tag_create("red", 3, &red) == NOTE_TAG_READY &&
                note_tag_create("blue", 4, &blue) == NOTE_TAG_READY,
            "create exact discovery conditions");
    const struct note_tag *names[] = {red, blue};
    require(note_tags_create(names, 2, &tags) == NOTE_TAGS_READY &&
                folio_state_set_tag_filter(fixture->state, tags, TAG_MATCH_ALL) ==
                    FOLIO_STATE_READY,
            "the list owns two selected conditions");
    note_tags_destroy(tags);
    note_tag_destroy(red);
    note_tag_destroy(blue);
    if (!folio_state_note_favorite(fixture->state, (struct note_ref){0, 0}))
        require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, 0}) ==
                    FOLIO_STATE_READY,
                "condition fixture has one favorite");
    folio_state_set_note_scope(fixture->state, NOTE_SCOPE_FAVORITES);
}

static void discovery_condition_boundary(struct folio_window *_Nonnull window,
                                         struct native_fixture *_Nonnull fixture)
{
    discovery_seed_conditions(fixture);
    tags_settle(window, fixture->state);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    require(folio_state_discovery_summary(fixture->state).shown == 1,
            "the copied star and ALL tag conditions are applied together");
    struct accessible_request query_focus = {.item = {.kind = ACCESSIBLE_COMMAND_INPUT},
                                             .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &query_focus) == S_OK &&
                GetFocus() == folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT),
            "the common native focus route reaches the discovery query too");
    struct accessible_view chip =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_TAG);
    require(wcsncmp(chip.name, L"red", 3) == 0, "first tag name leads its accessible label");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG);
    chip = tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_TAG);
    require(wcsncmp(chip.name, L"blue", 4) == 0, "next tag is individually reachable by name");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_TAG);
    discovery_settle(window, fixture->state);
    require(note_tags_count(folio_state_discovery_tags(fixture->state)) == 1 &&
                note_tags_count(folio_state_tag_filter(fixture->state)) == 2,
            "individual tag removal changes only the search session");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_CLEAR);
    discovery_settle(window, fixture->state);
    require(folio_state_discovery_summary(fixture->state).shown == 3 &&
                folio_state_note_scope(fixture->state) == NOTE_SCOPE_FAVORITES &&
                note_tags_count(folio_state_tag_filter(fixture->state)) == 2,
            "clear all expands the search while preserving list conditions");
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_DISCOVERY_PREVIEW},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK && GetFocus() == preview,
            "readonly source receives keyboard focus through its native provider");
    SendMessageW(preview, WM_KEYDOWN, VK_TAB, 0);
    require(GetFocus() != preview, "Tab leaves the readonly source");
    SetFocus(preview);
    SendMessageW(preview, WM_KEYDOWN, VK_F1, 0);
    pump(window);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "the common help shortcut closes discovery from its native preview");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    struct note_tags *empty = nullptr;
    require(note_tags_empty(&empty) == NOTE_TAGS_READY &&
                folio_state_set_tag_filter(fixture->state, empty, TAG_MATCH_ALL) ==
                    FOLIO_STATE_READY,
            "restore only the private fixture's list conditions");
    note_tags_destroy(empty);
    folio_state_set_note_scope(fixture->state, NOTE_SCOPE_ALL);
}

static void discovery_copy(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_query(window, fixture->state, L"two");
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    SendMessageW(preview, EM_SETSEL, 0, -1);
    SendMessageW(preview, WM_COPY, 0, 0);
    require(OpenClipboard(preview), "open only the isolated station's copied text");
    HGLOBAL block = GetClipboardData(CF_UNICODETEXT);
    require(block != nullptr, "native source copy publishes Unicode text");
    const wchar_t *copied = GlobalLock(block);
    require(copied != nullptr && wcscmp(copied, L"---\r\ntags: [blue, green]\r\n---\r\nOther") == 0,
            "copy preserves the full source without the separately painted line numbers");
    GlobalUnlock(block);
    require(CloseClipboard(), "close the isolated clipboard");
    SendMessageW(preview, EM_LINESCROLL, 0, 2);
    require(SendMessageW(preview, EM_GETMODIFY, 0, 0) == 0,
            "source scroll and copy never mark the readonly text modified");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void discovery_measure(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    for (size_t sample = 0; sample < 7; ++sample)
    {
        management_measure_phase("ready", sample);
        invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
        pump(window);
        for (size_t step = 0;
             step < 1000 && folio_state_discovery_summary(fixture->state).phase != DISCOVERY_READY;
             ++step)
        {
            folio_window_continue_idle_work(window);
            pump(window);
        }
        require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY,
                "measured name matching reaches completed results");
        require(folio_state_discovery_summary(fixture->state).source_checked == 0,
                "name-only matching performs no saved body reads");
        management_measure_phase("matched", sample);
        discovery_settle(window, fixture->state);
        require(folio_state_discovery_summary(fixture->state).source_checked == 1,
                "initial preview reads one source in the new session");
        management_measure_phase("preview", sample);
        discovery_query(window, fixture->state, L"one");
        discovery_query(window, fixture->state, L"");
        require(folio_state_discovery_summary(fixture->state).source_checked == 1,
                "two warm queries reuse the already observed source");
        management_measure_phase("warm", sample);
        discovery_choose(window, 1);
        discovery_settle(window, fixture->state);
        require(folio_state_discovery_summary(fixture->state).source_checked == 2,
                "new candidate preview reads only its own source");
        management_measure_phase("selected", sample);
        discovery_choose(window, 0);
        discovery_settle(window, fixture->state);
        require(folio_state_discovery_summary(fixture->state).source_checked == 2,
                "cached candidate preview performs no new source read");
        management_measure_phase("cached", sample);
        surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    }
}

static void discovery_visual(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t at = 0; at < 9; ++at)
    {
        favorite_high_contrast = at / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[at % 3]) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at / 3 == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                      : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "discovery visual matrix uses exact language and palette");
        folio_state_set_note_scope(fixture->state, NOTE_SCOPE_FAVORITES);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "private discovery visual window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 560, 360, SWP_NOZORDER | SWP_NOACTIVATE),
                "minimum discovery window");
        pump(window);
        invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
        pump(window);
        discovery_settle(window, fixture->state);
        char path[80];
        require(snprintf(path, sizeof path, "discovery-list-%zu.bmp", at) > 0, "list capture name");
        tags_capture(root, path);
        struct accessible_view previous =
            tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_PREVIOUS_TAG);
        struct accessible_view tag =
            tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_TAG);
        struct accessible_view next =
            tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_NEXT_TAG);
        require(previous.bounds.right <= tag.bounds.left && tag.bounds.right <= next.bounds.left &&
                    tag.bounds.right > tag.bounds.left,
                "minimum star plus multiple tags retain distinct condition targets");
        surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE);
        HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
        RECT client = {0};
        GetClientRect(preview, &client);
        require(IsWindowVisible(preview) && client.bottom >= 16,
                "minimum preview retains an actual source line above scrollbars");
        require(snprintf(path, sizeof path, "discovery-preview-%zu.bmp", at) > 0,
                "preview capture name");
        tags_capture(root, path);
        require(SetWindowPos(root, nullptr, 20, 20, 1250, 850, SWP_NOZORDER | SWP_NOACTIVATE),
                "wide discovery geometry");
        pump(window);
        struct accessible_view rows = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
        struct accessible_view source = tags_view(window, ACCESSIBLE_DISCOVERY_PREVIEW, 0);
        require(rows.bounds.right < source.bounds.left && source.bounds.bottom > source.bounds.top,
                "wide layout shows both candidates and source without overlap");
        require(snprintf(path, sizeof path, "discovery-wide-%zu.bmp", at) > 0, "wide capture name");
        tags_capture(root, path);
        folio_window_destroy(window);
        folio_state_close_discovery(fixture->state);
    }
    favorite_high_contrast = false;
}

static void body_discovery_write(struct native_fixture *_Nonnull fixture,
                                 const wchar_t *_Nonnull leaf, const char *_Nonnull source)
{
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, leaf, path);
    require(file_bytes_store(path, source, strlen(source)) == PERSISTENCE_STORED,
            "change only a dedicated body-discovery fixture file");
}

static void body_discovery_mode(struct folio_window *_Nonnull window, size_t slot)
{
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_SURFACE_TAB,
                 .slot = slot,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "select the public name/body radio through the shared surface group");
    pump(window);
}

static void body_discovery_open(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture,
                                const wchar_t *_Nonnull query)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    body_discovery_mode(window, 1);
    discovery_query(window, fixture->state, query);
}

static void body_discovery_return_one(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_query(window, fixture->state, L"one");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "restore the first fixture document through the existing name-open path");
}

static void body_discovery_expected_selection(struct folio_window *_Nonnull window,
                                              const wchar_t *_Nonnull query, LRESULT modified)
{
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    const wchar_t *found = wcsstr(snapshot.text, query);
    printf("body match expected=%td..%td actual=%ld..%ld modified=%lld\n",
           found == nullptr ? -1 : found - snapshot.text,
           found == nullptr ? -1 : found - snapshot.text + (ptrdiff_t)wcslen(query),
           snapshot.selection.cpMin, snapshot.selection.cpMax, (long long)snapshot.modified);
    fflush(stdout);
    require(found != nullptr && snapshot.selection.cpMin == found - snapshot.text &&
                snapshot.selection.cpMax == found - snapshot.text + (ptrdiff_t)wcslen(query) &&
                snapshot.modified == modified,
            "native selection independently matches the first substring of GT_RAWTEXT and "
            "preserves the expected modification state");
}

static void body_discovery_preview_visible(HWND preview)
{
    DWORD first = 0, last = 0;
    SendMessageW(preview, EM_GETSEL, (WPARAM)&first, (LPARAM)&last);
    LRESULT position = SendMessageW(preview, EM_POSFROMCHAR, first, 0);
    RECT text = {0};
    SendMessageW(preview, EM_GETRECT, 0, (LPARAM)&text);
    HDC device = GetDC(preview);
    HGDIOBJ previous = SelectObject(device, (HFONT)SendMessageW(preview, WM_GETFONT, 0, 0));
    TEXTMETRICW metrics = {0};
    require(GetTextMetricsW(device, &metrics), "measure the actual native preview line height");
    SelectObject(device, previous);
    ReleaseDC(preview, device);
    int x = GET_X_LPARAM(position), y = GET_Y_LPARAM(position);
    printf("body preview selection=%lu..%lu first=(%d,%d) text=%ld,%ld,%ld,%ld height=%ld\n", first,
           last, x, y, text.left, text.top, text.right, text.bottom, metrics.tmHeight);
    fflush(stdout);
    require(position != -1 && x >= text.left && x < text.right && y >= text.top &&
                y + metrics.tmHeight <= text.bottom,
            "the selected first match is actually visible after native preview layout");
}

static void body_discovery_preview_selection(struct folio_window *_Nonnull window,
                                             const wchar_t *_Nonnull query)
{
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    wchar_t text[2048];
    require(GetWindowTextW(preview, text, 2048) > 0, "read bounded native source preview");
    const wchar_t *found = wcsstr(text, query);
    DWORD first = 0, last = 0;
    SendMessageW(preview, EM_GETSEL, (WPARAM)&first, (LPARAM)&last);
    require(found != nullptr && first == (size_t)(found - text) && last == first + wcslen(query) &&
                (GetWindowLongPtrW(preview, GWL_STYLE) & ES_NOHIDESEL) != 0,
            "CRLF-projected native preview selects the independent first occurrence even without "
            "focus");
}

static void body_discovery_mode_keys(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    SetFocus(input);
    SendMessageW(input, WM_KEYDOWN, VK_TAB, 0);
    struct accessible_item focus = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_WINDOW, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_SURFACE_TAB && focus.slot == 0,
            "Tab from query reaches the selected name/body group once");
    SendMessageW(layer, WM_KEYDOWN, VK_RIGHT, 0);
    struct accessible_view names = tags_view(window, ACCESSIBLE_SURFACE_TAB, 0);
    struct accessible_view body = tags_view(window, ACCESSIBLE_SURFACE_TAB, 1);
    require(names.selected && !body.selected && body.focused,
            "mode arrow moves focus independently from the selected radio");
    SendMessageW(layer, WM_KEYDOWN, VK_RETURN, 0);
    require(folio_state_discovery_summary(fixture->state).mode == DISCOVERY_MODE_BODY &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_INPUT,
            "Enter on a mode chooses it and never opens a note");
    struct accessible_view status = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    struct accessible_view open =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(wcsstr(status.name, L"検索語を入力") != nullptr && !open.enabled &&
                wcscmp(open.name, L"一致行を開く") == 0 &&
                wcsstr(open.help, L"編集表示") != nullptr,
            "empty-body guidance and explicit editing action have matching accessible labels");
    discovery_query(window, fixture->state, L"needle");
    require(folio_state_discovery_summary(fixture->state).shown == 3,
            "body mode searches every saved source, including unsupported metadata without tags");
    struct accessible_item stale = discovery_row(window, 0);
    SendMessageW(input, WM_IME_STARTCOMPOSITION, 0, 0);
    struct accessible_request change = {
        .item = {.kind = ACCESSIBLE_SURFACE_TAB,
                 .slot = 0,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &change) == accessibility_node_disabled &&
                folio_state_discovery_summary(fixture->state).mode == DISCOVERY_MODE_BODY,
            "composition blocks both native and accessible mode activation");
    SendMessageW(input, WM_IME_ENDCOMPOSITION, 0, 0);
    discovery_settle(window, fixture->state);
    body_discovery_mode(window, 0);
    discovery_settle(window, fixture->state);
    struct accessible_view ignored = {0};
    require(folio_state_discovery_summary(fixture->state).shown == 0 &&
                FAILED(folio_window_accessible_read(window, &stale, &ignored)),
            "mode change preserves the query but invalidates body rows and preview identities");
    body_discovery_mode(window, 1);
    discovery_settle(window, fixture->state);
    require(folio_state_discovery_summary(fixture->state).shown == 3,
            "returning to body mode restores results for the preserved query");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static LRESULT CALLBACK progress_layer_observer(HWND window, UINT message, WPARAM wparam,
                                                LPARAM lparam)
{
    if (message == WM_WINDOWPOSCHANGING)
        progress_layer_moves += 1;
    RECT update = {0};
    if (message == WM_PAINT && GetUpdateRect(window, &update, FALSE))
    {
        /* Motion may repaint a line a few pixels thick; the status row is taller and wider. */
        LONG narrow = update.right - update.left < update.bottom - update.top
                          ? update.right - update.left
                          : update.bottom - update.top;
        if (narrow > 8)
            progress_layer_paints += 1;
    }
    return CallWindowProcW(progress_layer_procedure, window, message, wparam, lparam);
}

/* Independent of the product predicate: only the checked-source count may differ. */
static bool progress_only_step(const struct discovery_summary *_Nonnull before,
                               const struct discovery_summary *_Nonnull after)
{
    return before->source_checked != after->source_checked && before->phase == after->phase &&
           before->mode == after->mode && before->preview == after->preview &&
           before->notice == after->notice && before->total == after->total &&
           before->shown == after->shown && before->selected == after->selected &&
           before->unreadable == after->unreadable && before->unsupported == after->unsupported &&
           before->source_total == after->source_total && before->revision == after->revision &&
           before->results_revision == after->results_revision;
}

/* #837/#844: an idle that only reads one more source keeps the arranged surface, and repaints the
 * count only at the shared cadence or at the last source. */
static void body_discovery_progress_idle(struct folio_window *_Nonnull window,
                                         struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    body_discovery_mode(window, 1);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    HWND layer = GetParent(input);
    require(SetWindowTextW(input, L"needle"), "native body query starts a new source pass");
    pump(window);
    progress_layer_procedure =
        (WNDPROC)SetWindowLongPtrW(layer, GWLP_WNDPROC, (LONG_PTR)progress_layer_observer);
    require(progress_layer_procedure != nullptr, "observe the real discovery surface window");
    size_t quiet = 0;
    for (size_t step = 0; step < 1000 && folio_window_idle_work_ready(window); ++step)
    {
        struct discovery_summary before = folio_state_discovery_summary(fixture->state);
        progress_layer_moves = 0;
        progress_layer_paints = 0;
        folio_window_continue_idle_work(window);
        struct discovery_summary after = folio_state_discovery_summary(fixture->state);
        if (progress_only_step(&before, &after))
        {
            unsigned expected =
                discovery_text_progress_shown(after.source_checked, after.source_total) ? 1 : 0;
            require(progress_layer_moves == 0 && !GetUpdateRect(layer, nullptr, FALSE) &&
                        progress_layer_paints == expected,
                    "a progress-only idle never arranges, and paints only a shown count");
            quiet += 1;
        }
        pump(window);
    }
    SetWindowLongPtrW(layer, GWLP_WNDPROC, (LONG_PTR)progress_layer_procedure);
    require(discovery_text_progress_shown(0, 0) && discovery_text_progress_shown(8, 100) &&
                discovery_text_progress_shown(100, 100) && !discovery_text_progress_shown(1, 100) &&
                !discovery_text_progress_shown(7, 100) && !discovery_text_progress_shown(99, 100) &&
                discovery_text_progress_shown(3, 3),
            "the count is shown every eighth source and at the last one, without a clock");
    require(quiet != 0 && !folio_state_discovery_work_pending(fixture->state),
            "the private fixture reaches its result through at least one progress-only idle");
    body_discovery_preview_selection(window, L"needle");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void body_discovery_same_editor(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_SETSEL, (WPARAM)-1, -1);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"!");
    require(SendMessageW(body, EM_UNDO, 0, 0), "prepare a redoable native edit before discovery");
    pump(window);
    SendMessageW(body, EM_SETSEL, 1, 3);
    struct native_body_snapshot before = {0};
    capture_native_body(window, &before);
    body_discovery_open(window, fixture, L"needle");
    discovery_body_unchanged(window, &before);
    body_discovery_preview_selection(window, L"needle");
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    body_discovery_preview_selection(window, L"needle");
    discovery_body_unchanged(window, &before);
    discovery_choose(window, 0);
    discovery_settle(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    body_discovery_expected_selection(window, L"needle", before.modified);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED &&
                SendMessageW(body, EM_CANUNDO, 0, 0) == before.undo &&
                SendMessageW(body, EM_CANREDO, 0, 0) == before.redo &&
                SendMessageW(body, EM_REDO, 0, 0) != 0,
            "same EDIT open changes only selection and leaves the actual redo chain usable");
    require(SendMessageW(body, EM_UNDO, 0, 0), "restore the fixture after the redo proof");
    pump(window);
    action(window, ACCESSIBLE_VIEW);
    body_discovery_open(window, fixture, L"needle first");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "same VIEW target enters the canonical editing path");
    body_discovery_expected_selection(window, L"needle first", 0);
}

static void body_discovery_changed_source(struct folio_window *_Nonnull window,
                                          struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"Other needle");
    body_discovery_write(fixture, L"\\schedule\\two.md", "changed prefix\nOther needle\nnew end");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "changed source is adopted and opened while closing the completed finder");
    struct native_body_snapshot snapshot = {0};
    capture_native_body(window, &snapshot);
    struct accessible_view notice = tags_view(window, ACCESSIBLE_NOTICE, 0);
    require(wcscmp(snapshot.text, L"changed prefix\rOther needle\rnew end") == 0 &&
                snapshot.selection.cpMin == snapshot.selection.cpMax &&
                wcsstr(notice.name, L"一致箇所へは移動していません") != nullptr,
            "reloaded bytes differ from the captured source, so stale match coordinates are never "
            "applied");
    body_discovery_write(fixture, L"\\schedule\\two.md", body_discovery_other);
    body_discovery_return_one(window, fixture);
    body_discovery_open(window, fixture, L"Other needle");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    body_discovery_expected_selection(window, L"Other needle", 0);
    body_discovery_return_one(window, fixture);
}

static void body_discovery_read_failure(struct folio_window *_Nonnull window,
                                        struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"Other needle");
    HANDLE locked = management_lock(fixture, L"\\schedule\\two.md", 0);
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "post-search read failure keeps the original document and finder");
    require(CloseHandle(locked), "release only the isolated target read lock");
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\two.md", path);
    require(DeleteFileW(path), "remove only the private target to test disappearance");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "post-search missing source never recreates or adopts the missing target");
    body_discovery_write(fixture, L"\\schedule\\two.md", body_discovery_other);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void body_discovery_uneditable_native(struct folio_window *_Nonnull window,
                                             struct native_fixture *_Nonnull fixture)
{
    body_discovery_write(fixture, L"\\schedule\\two.md", "Other needle\a");
    body_discovery_open(window, fixture, L"Other needle");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "known uneditable snapshot is rejected before departure and remains searchable");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    body_discovery_write(fixture, L"\\schedule\\two.md", body_discovery_other);
    body_discovery_open(window, fixture, L"Other needle");
    body_discovery_write(fixture, L"\\schedule\\two.md", "Other needle\a");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED &&
                strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW,
            "source becoming uneditable after search is adopted with the existing VIEW protection");
    struct accessible_view notice = tags_view(window, ACCESSIBLE_NOTICE, 0);
    require(wcsstr(notice.name, L"閲覧") != nullptr,
            "reread editing refusal retains the existing visible protection notice");
    body_discovery_write(fixture, L"\\schedule\\two.md", body_discovery_other);
    body_discovery_return_one(window, fixture);
    action(window, ACCESSIBLE_EDIT);
}

static void body_discovery_native_refusal(struct folio_window *_Nonnull window,
                                          struct native_fixture *_Nonnull fixture, bool same)
{
    if (same)
        action(window, ACCESSIBLE_VIEW);
    body_discovery_open(window, fixture, same ? L"needle first" : L"Other needle");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    management_rejected_streams = 0;
    management_body_procedure =
        (WNDPROC)SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)management_reject_stream);
    require(management_body_procedure != nullptr, "intercept the private native stream boundary");
    begin_expected_failure(window, L"本文読込失敗。同じモードで再試行。");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    end_history_failure();
    require(failure.closed == 1, "native refusal reports its existing pane failure once");
    require(SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)management_body_procedure) != 0,
            "restore the original native body procedure");
    require(
        management_rejected_streams == 1 &&
            folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED &&
            strcmp(folio_state_document_name(fixture->state), same ? "one" : "two") == 0 &&
            folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
        "failed stream closes the finder but preserves the adopted document and unavailable pane");
    struct accessible_request recover = {.item = {.kind = ACCESSIBLE_EDIT},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &recover) == S_OK,
            "invoke the existing editing retry even when EDIT is already selected");
    pump(window);
    struct native_body_snapshot recovered = {0};
    capture_native_body(window, &recovered);
    printf("body recovery same=%d selection=%ld..%ld modified=%lld undo=%lld\n", same,
           recovered.selection.cpMin, recovered.selection.cpMax, (long long)recovered.modified,
           (long long)recovered.undo);
    fflush(stdout);
    require(wcsstr(recovered.text, same ? L"needle first" : L"Other needle") != nullptr &&
                recovered.modified == 0 && recovered.undo == 0 &&
                recovered.selection.cpMin == recovered.selection.cpMax,
            "existing retry restores saved source without applying the old search selection");
    if (!same)
        body_discovery_return_one(window, fixture);
}

static void body_discovery_remove_draft(struct native_fixture *_Nonnull fixture)
{
    struct note_identity *identity = nullptr;
    struct note_ref ref = {0};
    require(note_identity_create("target", "body-discovery-saved", &identity) ==
                    NOTE_IDENTITY_READY &&
                folio_state_resolve_note(fixture->state, identity, &ref) == FOLIO_STATE_READY,
            "resolve the exact draft created by the private body-discovery prompt");
    note_identity_destroy(identity);
    require(folio_state_trash_note(fixture->state, ref, u"", 0) == FOLIO_STATE_READY,
            "remove only this fixture's saved draft through the common note operation");
}

static void body_discovery_untitled(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture)
{
    struct accessible_request create = {.item = {.kind = ACCESSIBLE_NEW_NOTE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &create) == S_OK,
            "private draft for captured match target");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_REPLACESEL, TRUE, (LPARAM)L"body-discovery draft");
    pump(window);
    body_discovery_open(window, fixture, L"Other needle");
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = cancelled_name,
                                                           .discard = true,
                                                           .action = NAME_DIALOG_CANCEL});
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    require(name_dialog.timer == 0 && name_dialog.closed == 1 &&
                folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY,
            "cancelled draft departure preserves the original draft and body finder");
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = L"body-discovery-saved",
                                                           .discard = true,
                                                           .action = NAME_DIALOG_SAVE});
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    finish_name_dialog();
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "captured body identity and snapshot survive prompt insertion and finder invalidation");
    body_discovery_expected_selection(window, L"Other needle", 0);
    body_discovery_return_one(window, fixture);
    body_discovery_remove_draft(fixture);
}

static void body_discovery_refuse_paste(struct folio_window *_Nonnull window,
                                        struct native_fixture *_Nonnull fixture)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    require(GetFocus() == input, "paste keys target the focused native discovery input");
    const wchar_t *invalid[] = {L"Other\r\nneedle", L"Other\nneedle", L"Other\rneedle"};
    const UINT keys[] = {0, 'V', VK_INSERT};
    const BYTE modifiers[] = {0, FCONTROL, FSHIFT};
    for (size_t at = 0; at < 9; ++at)
    {
        SendMessageW(input, EM_SETSEL, 0, -1);
        require(OpenClipboard(input) && EmptyClipboard(), "own the isolated paste clipboard");
        publish_clipboard(CF_UNICODETEXT, invalid[at / 3],
                          (wcslen(invalid[at / 3]) + 1) * sizeof(wchar_t));
        require(CloseClipboard(), "release the isolated paste clipboard");
        (void)native_key_input_to(window, input, keys[at % 3], modifiers[at % 3]);
        wchar_t current[32];
        require(GetWindowTextW(input, current, 32) == 6 && wcscmp(current, L"needle") == 0 &&
                    folio_state_discovery_summary(fixture->state).phase == DISCOVERY_FAILED &&
                    folio_state_discovery_summary(fixture->state).notice ==
                        FOLIO_STATE_DISCOVERY_QUERY_INVALID &&
                    folio_state_discovery_summary(fixture->state).shown == 0,
                "newline paste is refused before single-line EDIT truncation and old matches are "
                "removed");
        discovery_query(window, fixture->state, L"needle");
    }
}

static void body_discovery_clipboard(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"needle");
    body_discovery_refuse_paste(window, fixture);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    wchar_t long_query[801];
    for (size_t at = 0; at < 800; at += 2)
    {
        long_query[at] = 0xd83d;
        long_query[at + 1] = 0xde00;
    }
    long_query[800] = L'\0';
    require(OpenClipboard(input) && EmptyClipboard(), "isolated over-limit paste");
    publish_clipboard(CF_UNICODETEXT, long_query, sizeof long_query);
    require(CloseClipboard(), "release isolated long query");
    SendMessageW(input, EM_SETSEL, 0, -1);
    SendMessageW(input, WM_PASTE, 0, 0);
    require(folio_state_discovery_summary(fixture->state).notice ==
                FOLIO_STATE_DISCOVERY_QUERY_LIMIT,
            "800 UTF16 units cannot be silently cut into an accepted 256-scalar query");
    const wchar_t suffix[] = L" first";
    require(OpenClipboard(input) && EmptyClipboard(), "isolated valid query paste");
    publish_clipboard(CF_UNICODETEXT, suffix, sizeof suffix);
    require(CloseClipboard(), "release isolated valid query");
    SendMessageW(input, EM_SETSEL, 6, 6);
    SendMessageW(input, WM_PASTE, 0, 0);
    discovery_settle(window, fixture->state);
    require(folio_state_discovery_summary(fixture->state).shown == 1,
            "valid paste is composed with the existing query and reaches the common matcher");
    body_discovery_preview_selection(window, L"needle first");
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    SendMessageW(preview, WM_COPY, 0, 0);
    require(OpenClipboard(preview), "inspect only the isolated copied match");
    HGLOBAL block = GetClipboardData(CF_UNICODETEXT);
    const wchar_t *copied = block == nullptr ? nullptr : GlobalLock(block);
    require(copied != nullptr && wcscmp(copied, L"needle first") == 0,
            "native match copy contains exactly the highlighted text without line numbers");
    GlobalUnlock(block);
    require(CloseClipboard(), "close isolated copied selection");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void body_discovery_visual_seed(struct native_fixture *_Nonnull fixture)
{
    char source[2048] = "---\r\ntags: [red, blue]\r\n---\r\n";
    for (size_t line = 0; line < 39; ++line)
        require(strcat_s(source, sizeof source, "前の行😀\r\n") == 0, "bounded visual context");
    require(strcat_s(source, sizeof source, "needle 日本😀\r\n末尾\r\n") == 0,
            "visual matching source line");
    body_discovery_write(fixture, L"\\schedule\\one.md", source);
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "reload the visual fixture after writing its long context");
    discovery_seed_conditions(fixture);
}

static void body_discovery_input_geometry(struct folio_window *_Nonnull window)
{
    struct accessible_view input = tags_view(window, ACCESSIBLE_COMMAND_INPUT, 0);
    struct accessible_view names = tags_view(window, ACCESSIBLE_SURFACE_TAB, 0);
    struct accessible_view body = tags_view(window, ACCESSIBLE_SURFACE_TAB, 1);
    require(input.bounds.right < names.bounds.left && names.bounds.right == body.bounds.left &&
                body.selected && input.bounds.top >= body.bounds.top &&
                input.bounds.bottom <= body.bounds.bottom && input.bounds.bottom > input.bounds.top,
            "minimum width keeps query and two radio targets on one nonoverlapping row");
}

static void body_discovery_visual(struct native_fixture *_Nonnull fixture)
{
    body_discovery_visual_seed(fixture);
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t at = 0; at < 9; ++at)
    {
        favorite_high_contrast = at / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[at % 3]) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at / 3 == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                      : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "body visual matrix uses the requested language and palette");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "private body visual window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 560, 360, SWP_NOZORDER | SWP_NOACTIVATE),
                "minimum body-search window");
        body_discovery_open(window, fixture, L"needle 日本😀");
        require(folio_state_discovery_summary(fixture->state).shown == 1,
                "visual match satisfies independent favorites and tags");
        body_discovery_input_geometry(window);
        char path[80];
        require(snprintf(path, sizeof path, "body-discovery-list-%zu.bmp", at) > 0,
                "body result capture name");
        tags_capture(root, path);
        surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE);
        HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
        require(IsWindowVisible(preview), "minimum body preview is visible");
        body_discovery_preview_selection(window, L"needle 日本😀");
        body_discovery_preview_visible(preview);
        struct accessible_view preview_view = tags_view(window, ACCESSIBLE_DISCOVERY_PREVIEW, 0);
        require(
            wcsstr(preview_view.name, L"23") != nullptr &&
                wcsstr(preview_view.name, L"45") != nullptr,
            "accessible source range names the actual context lines instead of restarting at one");
        require(snprintf(path, sizeof path, "body-discovery-preview-%zu.bmp", at) > 0,
                "body preview capture name");
        tags_capture(root, path);
        require(SetWindowPos(root, nullptr, 20, 20, 1250, 850, SWP_NOZORDER | SWP_NOACTIVATE),
                "wide body-search window");
        pump(window);
        struct accessible_view rows = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
        preview_view = tags_view(window, ACCESSIBLE_DISCOVERY_PREVIEW, 0);
        body_discovery_preview_visible(preview);
        require(rows.bounds.right < preview_view.bounds.left &&
                    preview_view.bounds.bottom > preview_view.bounds.top,
                "wide body mode shows results and selected source side by side");
        require(snprintf(path, sizeof path, "body-discovery-wide-%zu.bmp", at) > 0,
                "wide body capture name");
        tags_capture(root, path);
        folio_window_destroy(window);
        folio_state_close_discovery(fixture->state);
    }
    favorite_high_contrast = false;
}

static void body_discovery_measure(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture)
{
    for (size_t sample = 0; sample < 7; ++sample)
    {
        management_measure_phase("ready", sample);
        invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
        body_discovery_mode(window, 1);
        HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
        require(SetWindowTextW(input, L"needle"), "measured body query");
        for (size_t step = 0;
             step < 1000 && folio_state_discovery_summary(fixture->state).source_checked < 3;
             ++step)
        {
            folio_window_continue_idle_work(window);
            pump(window);
        }
        require(folio_state_discovery_summary(fixture->state).source_checked == 3,
                "measured first pass reads all three saved sources");
        management_measure_phase("read", sample);
        for (size_t step = 0;
             step < 1000 && folio_state_discovery_summary(fixture->state).phase != DISCOVERY_READY;
             ++step)
        {
            folio_window_continue_idle_work(window);
            pump(window);
        }
        require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY,
                "body matching and positions complete together");
        management_measure_phase("matched", sample);
        discovery_settle(window, fixture->state);
        body_discovery_preview_selection(window, L"needle");
        management_measure_phase("preview", sample);
        discovery_query(window, fixture->state, L"needle first");
        discovery_query(window, fixture->state, L"needle");
        require(folio_state_discovery_summary(fixture->state).source_checked == 3,
                "warm queries keep the same completed corpus pass");
        management_measure_phase("warm", sample);
        discovery_choose(window, 1);
        discovery_settle(window, fixture->state);
        body_discovery_preview_selection(window, L"needle");
        management_measure_phase("selected", sample);
        surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    }
}

static void run_body_discovery_case(struct native_fixture *_Nonnull fixture)
{
    body_discovery_write(fixture, L"\\schedule\\one.md", body_discovery_source);
    body_discovery_write(fixture, L"\\schedule\\two.md", body_discovery_other);
    body_discovery_write(fixture, L"\\target\\three.md",
                         "---\nitem:\n  child: value\n---\nneedle unknown");
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "body native baseline is the exact private mixed-newline source");
    folio_state_set_note_scope(fixture->state, NOTE_SCOPE_ALL);
    if (body_discovery_visual_case)
    {
        body_discovery_visual(fixture);
        return;
    }
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "private body-discovery native window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1050, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "wide body fixture");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    if (body_discovery_copy_case)
        body_discovery_clipboard(window, fixture);
    else if (body_discovery_measure_case)
        body_discovery_measure(window, fixture);
    else
    {
        body_discovery_progress_idle(window, fixture);
        body_discovery_mode_keys(window, fixture);
        body_discovery_same_editor(window, fixture);
        body_discovery_changed_source(window, fixture);
        body_discovery_read_failure(window, fixture);
        body_discovery_uneditable_native(window, fixture);
        body_discovery_native_refusal(window, fixture, true);
        body_discovery_native_refusal(window, fixture, false);
        body_discovery_untitled(window, fixture);
    }
    require(controlled.requests == 0,
            "saved source search and preview do not request images or diagrams");
    folio_window_destroy(window);
}

static void run_discovery_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select discovery baseline note");
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "discovery modal fixture starts in Japanese");
    folio_state_set_note_scope(fixture->state, NOTE_SCOPE_ALL);
    if (discovery_visual_case)
    {
        discovery_seed_conditions(fixture);
        discovery_visual(fixture);
        return;
    }
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "private native discovery window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1050, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "wide discovery window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    if (discovery_copy_case || discovery_measure_case)
    {
        if (discovery_copy_case)
            discovery_copy(window, fixture);
        if (discovery_measure_case)
            discovery_measure(window, fixture);
        folio_window_destroy(window);
        return;
    }
    discovery_selection_boundary(window, fixture);
    discovery_ime_boundary(window, fixture);
    discovery_untitled_boundary(window, fixture);
    discovery_named_save(window, fixture);
    discovery_failed_preview(window, fixture);
    discovery_unreadable_query(window, fixture);
    discovery_condition_boundary(window, fixture);
    require(controlled.requests == 0, "source preview starts no image or diagram work");
    folio_window_destroy(window);
}

static HWND motion_layer(struct folio_window *_Nonnull window)
{
    return GetParent(folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT));
}

static void motion_tick(struct folio_window *_Nonnull window, unsigned count)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    for (unsigned at = 0; at < count; ++at)
    {
        SendMessageW(root, WM_TIMER, 86, 0);
        UpdateWindow(motion_layer(window));
    }
}

static WNDPROC motion_preview_original;
static unsigned motion_preview_reads;
static unsigned motion_preview_writes;

static LRESULT CALLBACK motion_preview_observe(HWND window, UINT message, WPARAM wparam,
                                               LPARAM lparam)
{
    if (message == EM_GETRECT || message == EM_GETFIRSTVISIBLELINE || message == WM_GETTEXT ||
        message == WM_GETTEXTLENGTH)
        ++motion_preview_reads;
    if (message == WM_SETTEXT || message == EM_SETSEL)
        ++motion_preview_writes;
    return CallWindowProcW(motion_preview_original, window, message, wparam, lparam);
}

static void motion_frames(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    struct discovery_summary before = folio_state_discovery_summary(fixture->state);
    uint64_t revision = folio_window_accessible_surface_revision(window);
    HWND layer = motion_layer(window);
    tags_capture(layer, "motion-open-start.bmp");
    motion_tick(window, 4);
    tags_capture(layer, "motion-open-middle.bmp");
    motion_tick(window, 4);
    tags_capture(layer, "motion-open-final.bmp");
    require(folio_state_set_motion(fixture->state, false) == FOLIO_STATE_READY, "motion OFF");
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_SETTINGCHANGE, 0, 0);
    tags_capture(layer, "motion-open-off.bmp");
    require(before.revision == folio_state_discovery_summary(fixture->state).revision &&
                revision == folio_window_accessible_surface_revision(window),
            "opening frames and static fallback do not publish result or UIA revisions");
    require(folio_state_set_motion(fixture->state, true) == FOLIO_STATE_READY, "motion ON");
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    tags_capture(layer, "motion-selection-start.bmp");
    motion_tick(window, 3);
    tags_capture(layer, "motion-selection-middle.bmp");
    motion_tick(window, 3);
    tags_capture(layer, "motion-selection-final.bmp");
    require(folio_state_set_motion(fixture->state, false) == FOLIO_STATE_READY, "selection OFF");
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_SETTINGCHANGE, 0, 0);
    tags_capture(layer, "motion-selection-off.bmp");
    require(folio_state_discovery_summary(fixture->state).selected == 1 &&
                revision == folio_window_accessible_surface_revision(window),
            "selection frames do not alter the chosen row or its UIA identity");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void motion_clip_boundary(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_motion(fixture->state, true) == FOLIO_STATE_READY, "clip motion ON");
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    UpdateWindow(motion_layer(window));
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    motion_preview_original =
        (WNDPROC)SetWindowLongPtrW(preview, GWLP_WNDPROC, (LONG_PTR)motion_preview_observe);
    require(motion_preview_original != nullptr, "observe only the private preview messages");
    motion_preview_reads = 0;
    motion_preview_writes = 0;
    uint64_t revision = folio_window_accessible_surface_revision(window);
    motion_tick(window, 8);
    require(motion_preview_reads == 0 && motion_preview_writes == 0 &&
                folio_window_accessible_surface_revision(window) == revision,
            "frame-only repaints do not read or rewrite native preview or update UIA");
    require(SetWindowLongPtrW(preview, GWLP_WNDPROC, (LONG_PTR)motion_preview_original) != 0,
            "restore the private preview procedure");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void motion_generation_boundary(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    motion_tick(window, 8);
    discovery_choose(window, 1);
    motion_tick(window, 2);
    discovery_choose(window, 2);
    discovery_choose(window, 0);
    discovery_settle(window, fixture->state);
    uint64_t revision = folio_window_accessible_surface_revision(window);
    motion_tick(window, 6);
    require(folio_state_discovery_summary(fixture->state).selected == 0 &&
                folio_window_accessible_surface_revision(window) == revision &&
                !KillTimer(root, 86),
            "rapid selections replace the old effect and finish without a queued replay");
    discovery_choose(window, 1);
    discovery_query(window, fixture->state, L"no-such-motion-note");
    require(folio_state_discovery_summary(fixture->state).shown == 0 && !KillTimer(root, 86),
            "a new empty result generation discards the former row effect");
    discovery_query(window, fixture->state, L"");
    require(folio_state_discovery_summary(fixture->state).shown == 3 && !KillTimer(root, 86),
            "automatic first-row selection never starts an arrival effect");
    discovery_choose(window, 1);
    RECT bounds;
    require(GetWindowRect(root, &bounds), "current DPI transition bounds");
    UINT dpi = GetDpiForWindow(root);
    SendMessageW(root, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), (LPARAM)&bounds);
    require(!KillTimer(root, 86), "DPI notification settles the old row effect");
    body_discovery_mode(window, 1);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_INPUT &&
                !KillTimer(root, 86),
            "empty body input leaves no motion timer");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static struct accessible_item motion_setting(struct folio_window *_Nonnull window,
                                             enum folio_language language)
{
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item item = {0};
    HRESULT status =
        folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &item);
    char16_t name[ui_text_unit_limit];
    size_t written = 0;
    require(utf16_text_fill(ui_text_line(UI_TEXT_SETTINGS_MOTION, language), name,
                            ui_text_unit_limit, &written) == UTF16_TEXT_FILL_READY,
            "motion setting label");
    for (size_t count = 0; status == S_OK && count < 20; ++count)
    {
        struct accessible_view view = {0};
        require(folio_window_accessible_read(window, &item, &view) == S_OK, "read setting row");
        if (wcscmp(view.name, name) == 0)
            return item;
        status = folio_window_accessible_navigate(window, &item, ACCESSIBLE_NAVIGATION_NEXT, &item);
    }
    require(false, "motion setting is discoverable");
    return item;
}

static void motion_setting_boundary(struct folio_window *_Nonnull window,
                                    struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    motion_observation = APPEARANCE_MOTION_DISABLED;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    struct accessible_request open = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                      .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &open) == S_OK, "open motion settings");
    struct accessible_item item = motion_setting(window, folio_state_language(fixture->state));
    struct accessible_request request = {.item = item, .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &request) == S_OK, "focus motion choice");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && view.enabled &&
                view.focused && view.help[0] != 0 && !folio_state_motion_available(fixture->state),
            "OS-disabled motion remains operable and explains static rendering");
    bool before = folio_state_motion(fixture->state);
    request.action = ACCESSIBLE_ACTION_SELECT;
    require(folio_window_accessible_action(window, &request) == S_OK &&
                folio_state_motion(fixture->state) != before &&
                !folio_state_motion_enabled(fixture->state),
            "public toggle changes only the saved preference while OS motion is disabled");
    require(folio_window_accessible_action(window, &request) == S_OK &&
                folio_state_motion(fixture->state) == before,
            "same public toggle restores choice");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    motion_observation = APPEARANCE_MOTION_ALLOWED;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
}

static void motion_pause_boundary(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SendMessageW(input, WM_IME_STARTCOMPOSITION, 0, 0);
    require(!KillTimer(root, 86), "IME start destroys the decorative timer immediately");
    SendMessageW(input, WM_IME_ENDCOMPOSITION, 0, 0);
    discovery_settle(window, fixture->state);
    require(!KillTimer(root, 86), "IME end does not replay the opening animation");
    discovery_choose(window, 1);
    ShowWindow(root, SW_HIDE);
    require(!KillTimer(root, 86), "hiding destroys the selection timer");
    ShowWindow(root, SW_SHOWNOACTIVATE);
    require(!KillTimer(root, 86), "showing a ready result does not replay an animation");
    discovery_choose(window, 0);
    ShowWindow(root, SW_MINIMIZE);
    require(IsIconic(root) && !KillTimer(root, 86), "minimizing destroys decorative timers");
    ShowWindow(root, SW_RESTORE);
    require(!KillTimer(root, 86), "restore does not replay the opening animation");
    discovery_choose(window, 1);
    SetWindowPos(root, nullptr, 20, 20, 900, 660, SWP_NOZORDER | SWP_NOACTIVATE);
    require(!KillTimer(root, 86), "resizing settles the old selection geometry");
    discovery_choose(window, 0);
    SendMessageW(root, folio_message_report_outcome, FOLIO_STATE_OUT_OF_MEMORY, 0);
    require(!KillTimer(root, 86), "inline finder failure also destroys its decorative timer");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    require(!KillTimer(root, 86), "closing leaves no discovery timer");
    SendMessageW(root, WM_TIMER, 86, 0);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "a queued stale timer cannot reopen discovery");
}

static void motion_modal_boundary(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    action(window, ACCESSIBLE_VIEW);
    body_discovery_open(window, fixture, L"red");
    require(folio_state_discovery_summary(fixture->state).shown != 0,
            "modal fixture finds the saved first note source");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    management_rejected_streams = 0;
    management_body_procedure =
        (WNDPROC)SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)management_reject_stream);
    require(management_body_procedure != nullptr, "observe the private native adoption boundary");
    motion_check_modal = true;
    motion_saw_modal = false;
    begin_expected_failure(window, L"本文読込失敗。同じモードで再試行。");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    end_history_failure();
    require(motion_saw_modal && failure.closed == 1 && management_rejected_streams == 1,
            "real native adoption failure modal observes already stopped motion");
    motion_check_modal = false;
    require(SetWindowLongPtrW(body, GWLP_WNDPROC, (LONG_PTR)management_body_procedure) != 0,
            "restore the private body procedure after the refusal");
    require(!KillTimer(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), 86),
            "modal return does not replay a closed finder animation");
}

static void motion_os_boundary(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    const enum appearance_motion observations[] = {APPEARANCE_MOTION_DISABLED,
                                                   APPEARANCE_MOTION_UNKNOWN};
    for (size_t at = 0; at < sizeof observations / sizeof observations[0]; ++at)
    {
        invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
        discovery_settle(window, fixture->state);
        motion_observation = observations[at];
        SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
        require(!folio_state_motion_enabled(fixture->state) && !KillTimer(root, 86),
                "both OS-disabled and unavailable observations settle the native timer");
        surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
        motion_observation = APPEARANCE_MOTION_ALLOWED;
        SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    }
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    favorite_high_contrast = true;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    require(!folio_state_motion_enabled(fixture->state) && !KillTimer(root, 86),
            "high contrast settles the native timer without changing saved ON");
    favorite_high_contrast = false;
    motion_read_outcome = APPEARANCE_UNAVAILABLE;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    require(!folio_state_motion_enabled(fixture->state) && folio_state_motion(fixture->state) &&
                !KillTimer(root, 86),
            "whole appearance failure stays static with saved ON");
    motion_read_outcome = APPEARANCE_READY;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    require(folio_state_motion_enabled(fixture->state) && !KillTimer(root, 86),
            "OS recovery enables future effects without replaying an old one");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void motion_visual_settings(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture, size_t sample)
{
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "open visual settings");
    request.item = motion_setting(window, folio_state_language(fixture->state));
    request.action = ACCESSIBLE_ACTION_FOCUS;
    require(folio_window_accessible_action(window, &request) == S_OK, "reveal motion help");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &request.item, &view) == S_OK && !view.offscreen &&
                view.help[0] != 0 && view.bounds.bottom > view.bounds.top,
            "motion help is visible and accessible in every size and language");
    char path[80];
    require(snprintf(path, sizeof path, "motion-settings-%zu.bmp", sample) > 0,
            "motion settings capture path");
    tags_capture(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), path);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void motion_visual_search(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture, size_t sample)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    motion_tick(window, 3);
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    motion_tick(window, 2);
    char path[80];
    require(snprintf(path, sizeof path, "motion-search-%zu.bmp", sample) > 0,
            "motion search capture path");
    tags_capture(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), path);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void motion_visual(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t sample = 0; sample < 18; ++sample)
    {
        size_t combination = sample / 2;
        favorite_high_contrast = combination / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[combination % 3]) ==
                        FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, combination / 3 == 1
                                                              ? FOLIO_THEME_CHOICE_DARK
                                                              : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "motion visual palette and language");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "private motion visual window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, sample % 2 == 0 ? 560 : 1250,
                             sample % 2 == 0 ? 360 : 850, SWP_NOZORDER | SWP_NOACTIVATE),
                "motion visual minimum or wide size");
        motion_visual_settings(window, fixture, sample);
        motion_visual_search(window, fixture, sample);
        folio_window_destroy(window);
    }
    favorite_high_contrast = false;
}

constexpr UINT motion_live_message = WM_APP + 197;
static unsigned motion_live_job;
static unsigned motion_live_ticks;
static unsigned motion_live_idle;
static bool motion_live_slow;
static enum discovery_scale_command scale_job;
static bool scale_read_reported;
static bool scale_match_reported;
static void motion_live_result(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture, unsigned command)
{
    struct discovery_summary summary = folio_state_discovery_summary(fixture->state);
    printf("{\"event\":\"result\",\"command\":%u,\"ticks\":%u,\"idle\":%u,"
           "\"shown\":%zu,\"selected\":%zu,\"revision\":%llu,\"motion\":%s}\n",
           command, motion_live_ticks, motion_live_idle, summary.shown, summary.selected,
           (unsigned long long)folio_window_accessible_surface_revision(window),
           folio_state_motion_enabled(fixture->state) ? "true" : "false");
    require(fflush(stdout) == 0, "publish live measurement event");
}

static void motion_live_source(struct native_fixture *_Nonnull fixture)
{
    char *source = malloc(256 * 1024 + 32);
    require(source != nullptr, "bounded slow-search fixture source");
    memset(source, 'a', 256 * 1024);
    memcpy(source + 256 * 1024, " zz_motion", 11);
    body_discovery_write(fixture, L"\\schedule\\one.md", source);
    free(source);
}

static void motion_live_query(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture, WPARAM command)
{
    if (command == 7)
    {
        surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
        motion_live_source(fixture);
        invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
        body_discovery_mode(window, 1);
        motion_live_slow = true;
    }
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    const wchar_t *query = command == 7 ? L"zz_motion" : command == 1 ? L"one" : L"";
    require(SetWindowTextW(input, query), "live native query input");
    motion_live_job = (unsigned)command;
}

static bool motion_live_request(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture, WPARAM command)
{
    if (command == 6)
        return false;
    if (command == 3 || command == 4)
    {
        require(folio_state_set_motion(fixture->state, command == 4) == FOLIO_STATE_READY,
                "live measured motion preference");
        SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_SETTINGCHANGE, 0,
                     0);
    }
    if (command == 1 || command == 2 || command == 7)
        motion_live_query(window, fixture, command);
    else
        motion_live_result(window, fixture, (unsigned)command);
    return true;
}

static const wchar_t *_Nonnull scale_query(enum discovery_scale_command command)
{
    switch (command)
    {
    case DISCOVERY_SCALE_FRESH:
    case DISCOVERY_SCALE_UNIQUE:
        return L"unique-scale";
    case DISCOVERY_SCALE_ABSENT:
        return L"absent-scale";
    case DISCOVERY_SCALE_COMMON:
        return L"common-scale";
    case DISCOVERY_SCALE_NAMES:
        return L"note-";
    case DISCOVERY_SCALE_IDLE:
    case DISCOVERY_SCALE_STOP:
    case DISCOVERY_SCALE_PING:
        return L"";
    }
    return L"";
}

static void scale_emit(const struct native_fixture *_Nonnull fixture, const char *_Nonnull event)
{
    struct discovery_summary summary = folio_state_discovery_summary(fixture->state);
    printf("{\"event\":\"%s\",\"command\":%u,\"checked\":%zu,\"total\":%zu,"
           "\"shown\":%zu,\"idle\":%u,\"ticks\":%u}\n",
           event, (unsigned)scale_job, summary.source_checked, summary.total, summary.shown,
           motion_live_idle, motion_live_ticks);
    require(fflush(stdout) == 0, "publish external scale observation");
}

static void scale_observe(const struct native_fixture *_Nonnull fixture)
{
    if (scale_job == DISCOVERY_SCALE_IDLE)
        return;
    struct discovery_summary summary = folio_state_discovery_summary(fixture->state);
    require(summary.phase != DISCOVERY_FAILED, "scale search must not hide a failure as a time");
    if (scale_job == DISCOVERY_SCALE_FRESH && !scale_read_reported &&
        summary.source_checked == scale_note_count)
    {
        scale_emit(fixture, "read");
        scale_read_reported = true;
    }
    if (!scale_match_reported && summary.phase == DISCOVERY_READY)
    {
        scale_emit(fixture, "matched");
        scale_match_reported = true;
    }
}

static void scale_open(struct folio_window *_Nonnull window,
                       const struct native_fixture *_Nonnull fixture,
                       enum discovery_scale_command command)
{
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_SURFACE_BUTTON,
                 .slot = ACCESSIBLE_SURFACE_CLOSE,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_INVOKE};
    if (folio_state_discovery_summary(fixture->state).phase != DISCOVERY_CLOSED)
        require(folio_window_accessible_action(window, &request) == S_OK,
                "close the old scale session without a nested pump");
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    request.item =
        (struct accessible_item){.kind = ACCESSIBLE_SURFACE_TAB,
                                 .slot = command == DISCOVERY_SCALE_NAMES ? 0 : 1,
                                 .revision = folio_window_accessible_surface_revision(window)};
    request.action = ACCESSIBLE_ACTION_SELECT;
    require(folio_window_accessible_action(window, &request) == S_OK,
            "select the scale search mode without a nested pump");
}

static void scale_start(struct folio_window *_Nonnull window,
                        struct native_fixture *_Nonnull fixture,
                        enum discovery_scale_command command)
{
    require(scale_job == DISCOVERY_SCALE_IDLE, "measurement commands do not overlap");
    scale_job = command;
    scale_read_reported = false;
    scale_match_reported = false;
    scale_trace_idle = 0;
    scale_emit(fixture, "begin");
    if (command == DISCOVERY_SCALE_FRESH || command == DISCOVERY_SCALE_NAMES)
        scale_open(window, fixture, command);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    require(SetWindowTextW(input, scale_query(command)), "set the measured native scale query");
    scale_observe(fixture);
}

static bool scale_request(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture, const MSG *_Nonnull message)
{
    require(message->wParam <= DISCOVERY_SCALE_PING, "bounded private measurement command");
    enum discovery_scale_command command = (enum discovery_scale_command)message->wParam;
    switch (command)
    {
    case DISCOVERY_SCALE_FRESH:
    case DISCOVERY_SCALE_ABSENT:
    case DISCOVERY_SCALE_COMMON:
    case DISCOVERY_SCALE_UNIQUE:
    case DISCOVERY_SCALE_NAMES:
        scale_start(window, fixture, command);
        return true;
    case DISCOVERY_SCALE_STOP:
        require(scale_job == DISCOVERY_SCALE_IDLE, "stop after the last observed result");
        return false;
    case DISCOVERY_SCALE_PING:
        printf("{\"event\":\"ping\",\"id\":%lld,\"command\":%u}\n", (long long)message->lParam,
               (unsigned)scale_job);
        require(fflush(stdout) == 0, "publish the UI thread's ping acknowledgement");
        return true;
    case DISCOVERY_SCALE_IDLE:
        require(false, "idle is not an external command");
        return false;
    }
    return false;
}

static void scale_verify_rows(struct folio_window *_Nonnull window,
                              const struct native_fixture *_Nonnull fixture)
{
    struct discovery_summary summary = folio_state_discovery_summary(fixture->state);
    bool unique = scale_job == DISCOVERY_SCALE_FRESH || scale_job == DISCOVERY_SCALE_UNIQUE;
    size_t prefix = scale_source_bytes - (sizeof "\ncommon-scale\nunique-scale\n" - 1);
    size_t expected_byte = prefix + (unique ? sizeof "\ncommon-scale\n" - 1 : 1);
    size_t expected_line = prefix / 80 + (unique ? 3 : 2);
    for (size_t at = 0; at < summary.shown; ++at)
    {
        struct index_ranked_note row = folio_state_discovery_row(fixture->state, at);
        require(row.match.bytes.start == expected_byte &&
                    row.match.bytes.end == expected_byte + 12 && row.match.line == expected_line,
                "scale results retain independent exact tail byte and line positions");
    }
    if (unique)
    {
        char name[32];
        require(snprintf(name, sizeof name, "note-%04zu", scale_note_count - 1) > 0,
                "unique scale destination name");
        struct index_ranked_note row = folio_state_discovery_row(fixture->state, 0);
        require(strcmp(folio_state_note_name(fixture->state, row.ref), name) == 0,
                "the single scale match belongs to the last generated source");
    }
    body_discovery_preview_selection(window, scale_query(scale_job));
}

static void scale_finish(struct folio_window *_Nonnull window,
                         const struct native_fixture *_Nonnull fixture)
{
    if (scale_job == DISCOVERY_SCALE_IDLE)
        return;
    struct discovery_summary summary = folio_state_discovery_summary(fixture->state);
    bool many = scale_job == DISCOVERY_SCALE_COMMON || scale_job == DISCOVERY_SCALE_NAMES;
    size_t expected = many ? scale_note_count : scale_job == DISCOVERY_SCALE_ABSENT ? 0 : 1;
    size_t shown = expected < 200 ? expected : 200;
    require(summary.phase == DISCOVERY_READY && scale_match_reported && summary.total == expected &&
                summary.shown == shown && summary.unreadable == 0 && summary.unsupported == 0 &&
                summary.notice == FOLIO_STATE_READY,
            "independent scale result totals and the 200-row display limit agree");
    require(summary.source_checked == (scale_job == DISCOVERY_SCALE_NAMES ? 1 : scale_note_count),
            "body queries reuse the full pass; name-only preview reads one source");
    if (shown != 0 && scale_job != DISCOVERY_SCALE_NAMES)
        scale_verify_rows(window, fixture);
    require(strcmp(folio_state_document_name(fixture->state), "note-0000") == 0,
            "measurement never adopts a result into the current document");
    scale_emit(fixture, "result");
    scale_job = DISCOVERY_SCALE_IDLE;
}

static bool discovery_live_request(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture,
                                   const MSG *_Nonnull message)
{
    return scale_note_count != 0 ? scale_request(window, fixture, message)
                                 : motion_live_request(window, fixture, message->wParam);
}

static void discovery_live_finish(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    if (scale_note_count != 0)
    {
        scale_finish(window, fixture);
        return;
    }
    if (motion_live_job != 0 && !folio_state_discovery_work_pending(fixture->state))
    {
        motion_live_result(window, fixture, motion_live_job);
        motion_live_job = 0;
        motion_live_slow = false;
    }
}

static void motion_live_dispatch(struct folio_window *_Nonnull window, const MSG *_Nonnull message)
{
    if (message->message == WM_TIMER && message->wParam == 86)
    {
        ++motion_live_ticks;
        if (motion_live_slow)
        {
            puts("{\"event\":\"delay\"}");
            require(fflush(stdout) == 0 && getchar() == '\n',
                    "external driver delays delivery without a test clock");
        }
    }
    if (!folio_window_translate(window, message))
    {
        TranslateMessage(message);
        DispatchMessageW(message);
    }
}

static void scale_continue_idle(struct folio_window *_Nonnull window)
{
    bool observed = scale_trace_case && scale_job == DISCOVERY_SCALE_FRESH && !scale_read_reported;
    if (observed)
    {
        require(scale_trace_phase == DISCOVERY_TRACE_OFF, "observed idle steps never overlap");
        ++scale_trace_idle;
        scale_trace_phase = DISCOVERY_TRACE_IDLE;
        scale_trace_emit("idle_begin");
    }
    folio_window_continue_idle_work(window);
    if (observed)
    {
        require(scale_trace_phase == DISCOVERY_TRACE_IDLE ||
                    scale_trace_phase == DISCOVERY_TRACE_READ_DONE,
                "the idle step finishes with no outstanding read");
        scale_trace_emit("idle_end");
        scale_trace_phase = DISCOVERY_TRACE_OFF;
    }
}

static bool motion_live_step(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture)
{
    MSG message;
    if (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        if (message.message == motion_live_message)
            return discovery_live_request(window, fixture, &message);
        motion_live_dispatch(window, &message);
        return true;
    }
    if (folio_window_idle_work_ready(window))
    {
        ++motion_live_idle;
        scale_continue_idle(window);
        scale_observe(fixture);
        return true;
    }
    discovery_live_finish(window, fixture);
    require(WaitMessage(), "live loop waits only when the production idle path has no work");
    return true;
}

static void motion_live(struct folio_window *_Nonnull window,
                        struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_settle(window, fixture->state);
    printf("{\"event\":\"ready\",\"thread\":%lu}\n", GetCurrentThreadId());
    require(fflush(stdout) == 0, "publish the private live-loop thread");
    while (motion_live_step(window, fixture))
    {
    }
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static void run_scale_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_set_motion(fixture->state, false) == FOLIO_STATE_READY,
            "scale baseline selects only the first private note with decoration off");
    folio_state_set_note_scope(fixture->state, NOTE_SCOPE_ALL);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "private scale window uses the production search surface");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1050, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "fixed wide scale window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    printf("{\"event\":\"ready\",\"thread\":%lu,\"notes\":%zu,\"sourceBytes\":%zu,"
           "\"traceReads\":%s}\n",
           GetCurrentThreadId(), scale_note_count, scale_source_bytes,
           scale_trace_case ? "true" : "false");
    require(fflush(stdout) == 0, "publish scale setup separately from search measurements");
    while (motion_live_step(window, fixture))
    {
    }
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    folio_window_destroy(window);
}

static void run_motion_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_motion(fixture->state, true) == FOLIO_STATE_READY &&
                folio_state_set_theme(fixture->state, FOLIO_THEME_CHOICE_DARK) ==
                    FOLIO_STATE_READY &&
                folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                    FOLIO_STATE_READY &&
                folio_state_motion_enabled(fixture->state),
            "private motion fixture has explicit saved and OS permission");
    if (motion_visual_case)
    {
        motion_visual(fixture);
        return;
    }
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "motion native window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1050, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "wide motion fixture");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    if (motion_live_case)
    {
        motion_live(window, fixture);
        folio_window_destroy(window);
        return;
    }
    struct native_body_snapshot before;
    capture_native_body(window, &before);
    motion_frames(window, fixture);
    motion_clip_boundary(window, fixture);
    motion_generation_boundary(window, fixture);
    motion_setting_boundary(window, fixture);
    motion_pause_boundary(window, fixture);
    motion_os_boundary(window, fixture);
    discovery_body_unchanged(window, &before);
    motion_modal_boundary(window, fixture);
    folio_window_destroy(window);
}

/* #838 / ADR0097: アプリの情報の面。外部起動は観測用portが受け、実ブラウザーを開かない。 */
static void about_open(struct folio_window *_Nonnull window)
{
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "open settings before the About surface");
    pump(window);
    surface_button(window, ACCESSIBLE_SURFACE_ABOUT);
}

static void about_expect_name(const struct accessible_view *_Nonnull view,
                              const char *_Nonnull expected, const char *_Nonnull message)
{
    char16_t units[ui_text_unit_limit];
    size_t written = 0;
    require(utf16_text_fill(expected, units, ui_text_unit_limit, &written) ==
                    UTF16_TEXT_FILL_READY &&
                wcscmp(view->name, units) == 0,
            message);
}

static void about_expect_title(struct folio_window *_Nonnull window, enum ui_text title)
{
    struct accessible_item root = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &root, &view) == S_OK && !view.offscreen &&
                view.dialog,
            "the settings frame stays open as a dialog");
    about_expect_name(&view, ui_text_line(title, folio_state_language(about_state)),
                      "the surface name is the title of the current face");
}

static void about_expect_focus(struct folio_window *_Nonnull window,
                               enum accessible_surface_button slot)
{
    struct accessible_item focused = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_SURFACE, &focused) == S_OK &&
                focused.kind == ACCESSIBLE_SURFACE_BUTTON && focused.slot == slot,
            "keyboard focus is on the expected About control");
    struct accessible_view view = tags_view(window, ACCESSIBLE_SURFACE_BUTTON, slot);
    require(view.focusable && view.focused && view.enabled,
            "the focused control is visible and reports focus");
}

static void about_key(struct folio_window *_Nonnull window, WPARAM key, bool shift)
{
    BYTE original[256], keyboard[256] = {0};
    require(GetKeyboardState(original), "save About keyboard state");
    keyboard[VK_SHIFT] = shift ? 0x80 : 0;
    require(SetKeyboardState(keyboard), "About key modifiers");
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    require(GetFocus() == layer, "the surface layer owns focus without an input field");
    SendMessageW(layer, WM_KEYDOWN, key, 0);
    pump(window);
    require(SetKeyboardState(original), "restore About keyboard state");
}

static void about_focus_button(struct folio_window *_Nonnull window,
                               enum accessible_surface_button slot)
{
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_SURFACE_BUTTON,
                 .slot = slot,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "focus a public About control");
    about_expect_focus(window, slot);
}

/* 開いた直後は戻る。Tabと矢印はリンク4つ→戻るを循環し、移動だけでは外部を開かない。 */
static void about_cycle(struct folio_window *_Nonnull window)
{
    const enum accessible_surface_button order[] = {
        ACCESSIBLE_SURFACE_ABOUT_REPOSITORY, ACCESSIBLE_SURFACE_ABOUT_RELEASES,
        ACCESSIBLE_SURFACE_ABOUT_STORE, ACCESSIBLE_SURFACE_ABOUT_ISSUES, ACCESSIBLE_SURFACE_BACK};
    const WPARAM forward[] = {VK_TAB, VK_RIGHT, VK_DOWN, VK_TAB, VK_RIGHT};
    about_expect_focus(window, ACCESSIBLE_SURFACE_BACK);
    for (size_t at = 0; at < sizeof order / sizeof order[0]; ++at)
    {
        about_key(window, forward[at], false);
        about_expect_focus(window, order[at]);
    }
    about_key(window, VK_LEFT, false);
    about_expect_focus(window, ACCESSIBLE_SURFACE_ABOUT_ISSUES);
    about_key(window, VK_UP, false);
    about_expect_focus(window, ACCESSIBLE_SURFACE_ABOUT_STORE);
    about_key(window, VK_TAB, true);
    about_expect_focus(window, ACCESSIBLE_SURFACE_ABOUT_RELEASES);
    require(about_opened == 0, "moving focus never reaches the web link port");
}

/* 文字行は表示順にTextとして読める。版は製品の版の唯一の入力から作る。 */
static void about_texts(struct folio_window *_Nonnull window, enum folio_language language)
{
    const enum ui_text labels[] = {UI_TEXT_APP_LOGO,          UI_TEXT_SETTINGS_VERSION,
                                   UI_TEXT_ABOUT_DESCRIPTION, UI_TEXT_ABOUT_LICENSE_HEADING,
                                   UI_TEXT_ABOUT_LICENSE,     UI_TEXT_ABOUT_FONTS,
                                   UI_TEXT_ABOUT_NOTICES,     UI_TEXT_ABOUT_LINKS_HEADING,
                                   UI_TEXT_ABOUT_LINKS_HINT};
    struct accessible_item row = {.kind = ACCESSIBLE_SURFACE_LIST,
                                  .revision = folio_window_accessible_surface_revision(window)};
    HRESULT result =
        folio_window_accessible_navigate(window, &row, ACCESSIBLE_NAVIGATION_FIRST, &row);
    LONG bottom = 0;
    size_t count = 0;
    for (; result == S_OK && count < sizeof labels / sizeof labels[0]; ++count)
    {
        char expected[ui_text_unit_limit];
        struct ui_text_request request = {
            .id = labels[count], .language = language, .name = product_version_text()};
        struct accessible_view view = {0};
        require(ui_text_format(&request, expected, sizeof expected) == UI_TEXT_FORMAT_READY &&
                    row.kind == ACCESSIBLE_SURFACE_TEXT &&
                    folio_window_accessible_read(window, &row, &view) == S_OK && !view.focusable,
                "About line is readable, non-interactive Text");
        about_expect_name(&view, expected, "About line matches the catalog in visual order");
        require(!view.offscreen && view.bounds.top >= bottom,
                "About lines are all visible in the wide window and do not overlap");
        bottom = view.bounds.bottom;
        result = folio_window_accessible_navigate(window, &row, ACCESSIBLE_NAVIGATION_NEXT, &row);
    }
    require(result == S_FALSE && count == sizeof labels / sizeof labels[0],
            "the About list has exactly its text lines");
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    struct accessible_view link =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_ABOUT_ISSUES);
    require(list.scroll_limit == 0 && link.bounds.bottom <= bottom,
            "the wide window shows the whole About content without scrolling");
    about_expect_title(window, UI_TEXT_ABOUT_TITLE);
}

/* リンクはNameが表示名、HelpTextが行き先。EnterとUIA Invokeが同じ意図を一度だけ渡す。 */
static void about_links(struct folio_window *_Nonnull window, enum folio_language language)
{
    const enum ui_text labels[] = {UI_TEXT_ABOUT_LINK_REPOSITORY, UI_TEXT_ABOUT_LINK_RELEASES,
                                   UI_TEXT_ABOUT_LINK_STORE, UI_TEXT_ABOUT_LINK_ISSUES};
    for (size_t link = 0; link < product_link_count; ++link)
    {
        enum accessible_surface_button slot =
            (enum accessible_surface_button)(ACCESSIBLE_SURFACE_ABOUT_REPOSITORY + link);
        about_focus_button(window, slot);
        struct accessible_view view = tags_view(window, ACCESSIBLE_SURFACE_BUTTON, slot);
        char16_t uri[ui_text_unit_limit];
        size_t written = 0;
        about_expect_name(&view, ui_text_line(labels[link], language),
                          "link Name is its visible label");
        require(utf16_text_fill(product_link_uri((enum product_link)link), uri, ui_text_unit_limit,
                                &written) == UTF16_TEXT_FILL_READY &&
                    wcscmp(view.help, uri) == 0,
                "link HelpText is the destination before activation");
        about_opened = 0;
        about_key(window, link % 2 == 0 ? VK_RETURN : VK_SPACE, false);
        require(about_opened == 1 &&
                    strcmp(about_uri, product_link_uri((enum product_link)link)) == 0,
                "Enter or Space passes exactly the official URI once");
        surface_button(window, slot);
        require(about_opened == 2 &&
                    strcmp(about_uri, product_link_uri((enum product_link)link)) == 0,
                "UIA Invoke reaches the same intent once");
        about_expect_title(window, UI_TEXT_ABOUT_TITLE);
        about_expect_focus(window, slot);
    }
}

static bool about_status(struct folio_window *_Nonnull window,
                         struct accessible_view *_Nonnull view)
{
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_STATUS,
                                   .revision = folio_window_accessible_surface_revision(window)};
    require(folio_window_accessible_read(window, &item, view) == S_OK, "About status is readable");
    return !view->offscreen;
}

/* 失敗は面内の1行。面は開いたままで、次の成功が行を消す。 */
static void about_failure(struct folio_window *_Nonnull window, enum folio_language language)
{
    struct accessible_view status = {0};
    require(!about_status(window, &status), "a successful link leaves no notice");
    about_port_result = WEB_LINK_UNAVAILABLE;
    about_opened = 0;
    surface_button(window, ACCESSIBLE_SURFACE_ABOUT_ISSUES);
    require(about_opened == 1 && about_status(window, &status),
            "a refused launch shows one notice inside the surface");
    about_expect_name(&status, folio_state_failure_line(FOLIO_STATE_WEB_LINK_UNAVAILABLE, language),
                      "the notice is the existing link failure line");
    struct accessible_view back =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_BACK);
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    require(list.bounds.bottom <= status.bounds.top && status.bounds.bottom <= back.bounds.top,
            "content, notice and Back do not overlap");
    about_expect_title(window, UI_TEXT_ABOUT_TITLE);
    about_expect_focus(window, ACCESSIBLE_SURFACE_ABOUT_ISSUES);
    about_port_result = WEB_LINK_OPENED;
    about_key(window, VK_RETURN, false);
    require(about_opened == 2 && !about_status(window, &status),
            "the next successful launch clears the notice");
}

static void about_expect_settings(struct folio_window *_Nonnull window)
{
    about_expect_title(window, UI_TEXT_COMMAND_SETTINGS);
    struct accessible_item focused = {0};
    require(folio_window_accessible_focus(window, ACCESSIBLE_SURFACE, &focused) == S_OK &&
                focused.kind == ACCESSIBLE_SURFACE_BUTTON &&
                focused.slot == ACCESSIBLE_SURFACE_ABOUT,
            "returning selects the About row of Settings");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &focused, &view) == S_OK && !view.offscreen &&
                view.focused,
            "the selected About row is revealed");
}

/* 開いた直後のEnter・Esc・戻るは設定のAbout行へ戻り、どれも外部を開かない。 */
static void about_return(struct folio_window *_Nonnull window)
{
    about_opened = 0;
    about_key(window, VK_RETURN, false);
    about_expect_settings(window);
    about_key(window, VK_RETURN, false);
    about_expect_title(window, UI_TEXT_ABOUT_TITLE);
    about_expect_focus(window, ACCESSIBLE_SURFACE_BACK);
    about_key(window, VK_ESCAPE, false);
    about_expect_settings(window);
    about_key(window, VK_SPACE, false);
    about_expect_focus(window, ACCESSIBLE_SURFACE_BACK);
    surface_button(window, ACCESSIBLE_SURFACE_BACK);
    about_expect_settings(window);
    require(about_opened == 0, "opening and leaving About never launches a browser");
    surface_button(window, ACCESSIBLE_SURFACE_ABOUT);
}

static void about_click(struct folio_window *_Nonnull window, POINT down, POINT up)
{
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    require(ScreenToClient(layer, &down) && ScreenToClient(layer, &up), "About pointer position");
    SendMessageW(layer, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(down.x, down.y));
    SendMessageW(layer, WM_LBUTTONUP, 0, MAKELPARAM(up.x, up.y));
    pump(window);
}

/* 押下と解放が同じ釦のときだけ開く。 */
static void about_pointer(struct folio_window *_Nonnull window)
{
    struct accessible_view store =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_ABOUT_STORE);
    struct accessible_view back =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_BACK);
    POINT link = {(store.bounds.left + store.bounds.right) / 2,
                  (store.bounds.top + store.bounds.bottom) / 2};
    POINT other = {(back.bounds.left + back.bounds.right) / 2,
                   (back.bounds.top + back.bounds.bottom) / 2};
    about_opened = 0;
    about_click(window, link, other);
    require(about_opened == 0, "releasing on another control cancels the link");
    about_expect_title(window, UI_TEXT_ABOUT_TITLE);
    about_click(window, link, link);
    require(about_opened == 1 && strcmp(about_uri, product_link_uri(PRODUCT_LINK_STORE)) == 0,
            "press and release on the same link opens it once");
    about_expect_title(window, UI_TEXT_ABOUT_TITLE);
}

/* 最小の窓では内容だけがスクロールし、focusした釦と戻るは見える。 */
static void about_minimum(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    about_focus_button(window, ACCESSIBLE_SURFACE_BACK);
    require(SetWindowPos(root, nullptr, 20, 20, 560, 360, SWP_NOZORDER | SWP_NOACTIVATE),
            "minimum About window");
    pump(window);
    struct accessible_view list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    printf("about geometry: viewport=%d limit=%d scroll=%d\n", list.viewport, list.scroll_limit,
           list.scroll);
    require(list.scroll_limit > 0 && list.scroll == 0 && !list.selection_required,
            "the minimum window scrolls the About content");
    about_focus_button(window, ACCESSIBLE_SURFACE_ABOUT_ISSUES);
    struct accessible_view link =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_ABOUT_ISSUES);
    struct accessible_view back =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_BACK);
    list = tags_view(window, ACCESSIBLE_SURFACE_LIST, 0);
    require(list.scroll > 0 && link.bounds.top >= list.bounds.top &&
                link.bounds.bottom <= list.bounds.bottom && link.bounds.bottom <= back.bounds.top,
            "the focused link is revealed above the fixed Back button");
    about_focus_button(window, ACCESSIBLE_SURFACE_BACK);
    require(SetWindowPos(root, nullptr, 20, 20, 1250, 850, SWP_NOZORDER | SWP_NOACTIVATE),
            "restore the wide About window");
    pump(window);
}

static void about_language(struct native_fixture *_Nonnull fixture, enum folio_language language)
{
    require(folio_state_set_language(fixture->state, language) == FOLIO_STATE_READY,
            "About language");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "private About language window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1250, 850, SWP_NOZORDER | SWP_NOACTIVATE),
            "wide About language window");
    pump(window);
    about_open(window);
    about_texts(window, language);
    about_links(window, language);
    about_failure(window, language);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    folio_window_destroy(window);
}

static void about_surface_checks(struct folio_window *_Nonnull window)
{
    about_open(window);
    about_cycle(window);
    about_texts(window, FOLIO_LANGUAGE_JA);
    about_links(window, FOLIO_LANGUAGE_JA);
    about_failure(window, FOLIO_LANGUAGE_JA);
    about_pointer(window);
    about_minimum(window);
    about_return(window);
    uint64_t revision = folio_window_accessible_surface_revision(window);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    struct accessible_item root = {.kind = ACCESSIBLE_SURFACE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &root, &view) == S_OK && view.offscreen &&
                folio_window_accessible_surface_revision(window) != revision,
            "Close hides the whole surface and retires its controls");
}

static void run_about_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated About note");
    about_state = fixture->state;
    struct history_snapshot history = {0};
    capture_history(fixture, &history);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real About window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 20, 20, 1250, 850, SWP_NOZORDER | SWP_NOACTIVATE),
            "wide About window");
    pump(window);
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    struct native_body_snapshot closed = {0};
    capture_native_body(window, &closed);
    about_surface_checks(window);
    expect_native_body(window, &closed);
    expect_editor(window, fixture->state, switch_edited, FOLIO_NOTE_CHANGED);
    history_undo(window);
    expect_editor(window, fixture->state, switch_original, FOLIO_NOTE_SAME);
    verify_history_unchanged(fixture, &history);
    folio_window_destroy(window);
    about_language(fixture, FOLIO_LANGUAGE_EN);
    about_language(fixture, FOLIO_LANGUAGE_ZH_HANS);
    for (size_t version = 0; version <= note_history_depth; ++version)
        note_text_destroy(history.files[version]);
}

static void about_capture(struct folio_window *_Nonnull window, size_t sample,
                          const char *_Nonnull suffix)
{
    static const char *const languages[] = {"ja", "en", "zh"};
    static const char *const themes[] = {"light", "dark", "contrast"};
    size_t combination = sample / 2;
    char path[80];
    require(snprintf(path, sizeof path, "about-%s-%s-%s%s.bmp", languages[combination % 3],
                     themes[combination / 3], sample % 2 == 0 ? "560x360" : "1250x850", suffix) > 0,
            "About capture path");
    tags_capture(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), path);
}

/* 設定の入口行、開いた直後、失敗の通知行（先頭のリンクへfocus）を同じ窓で採る。 */
static void about_visual_capture(struct folio_window *_Nonnull window, size_t sample)
{
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "open visual settings");
    pump(window);
    request.item =
        (struct accessible_item){.kind = ACCESSIBLE_SURFACE_BUTTON,
                                 .slot = ACCESSIBLE_SURFACE_ABOUT,
                                 .revision = folio_window_accessible_surface_revision(window)};
    request.action = ACCESSIBLE_ACTION_FOCUS;
    require(folio_window_accessible_action(window, &request) == S_OK, "reveal the About row");
    about_capture(window, sample, "-entry");
    surface_button(window, ACCESSIBLE_SURFACE_ABOUT);
    about_capture(window, sample, "");
    about_port_result = WEB_LINK_UNAVAILABLE;
    about_key(window, VK_TAB, false);
    about_key(window, VK_RETURN, false);
    about_port_result = WEB_LINK_OPENED;
    about_capture(window, sample, "-notice");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

/* 三言語×明/暗/高コントラスト×最小と広い窓。色と重なりは採取した画像で確認する。 */
static void about_visual(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    about_state = fixture->state;
    for (size_t sample = 0; sample < 18; ++sample)
    {
        size_t combination = sample / 2;
        favorite_high_contrast = combination / 3 == 2;
        require(folio_state_set_language(fixture->state, languages[combination % 3]) ==
                        FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, combination / 3 == 1
                                                              ? FOLIO_THEME_CHOICE_DARK
                                                              : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "About visual palette and language");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "private About visual window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, sample % 2 == 0 ? 560 : 1250,
                             sample % 2 == 0 ? 360 : 850, SWP_NOZORDER | SWP_NOACTIVATE),
                "About visual minimum or wide size");
        pump(window);
        about_visual_capture(window, sample);
        folio_window_destroy(window);
    }
    favorite_high_contrast = false;
}

static void run_fixture(struct native_fixture *_Nonnull fixture, int argc,
                        char *_Nonnull *_Nonnull argv)
{
    if (scale_note_count != 0)
        run_scale_case(fixture);
    else if (about_visual_case)
        about_visual(fixture);
    else if (about_case)
        run_about_case(fixture);
    else if (motion_case)
        run_motion_case(fixture);
    else if (body_discovery_case)
        run_body_discovery_case(fixture);
    else if (discovery_case)
        run_discovery_case(fixture);
    else if (tag_management_case)
        run_tag_management_case(fixture);
    else if (tags_case)
        run_tags_case(fixture);
    else if (code_copy_case)
        run_code_copy_case(fixture);
    else
        run_existing_fixture(fixture, argc, argv);
}

static void select_favorites_fixture(const char *_Nonnull selector)
{
    favorites_visual_case = strcmp(selector, "--favorites-visual") == 0;
    favorites_ui_case = strcmp(selector, "--favorites-ui") == 0;
    favorites_case =
        strcmp(selector, "--favorites") == 0 || favorites_visual_case || favorites_ui_case;
}

static void select_scale_fixture(const char *_Nonnull selector)
{
    scale_trace_case = strcmp(selector, "--discovery-cost") == 0;
    if (scale_trace_case)
    {
        scale_note_count = 1000;
        scale_source_bytes = 4096;
        return;
    }
    scale_note_count = strcmp(selector, "--discovery-scale-small") == 0  ? 100
                       : strcmp(selector, "--discovery-scale-many") == 0 ? 1000
                       : strcmp(selector, "--discovery-scale-long") == 0 ? 32
                                                                         : 0;
    scale_source_bytes = strcmp(selector, "--discovery-scale-long") == 0 ? 1024 * 1024 : 4096;
}

static void select_tags_fixture(const char *_Nonnull selector)
{
    select_scale_fixture(selector);
    motion_visual_case = strcmp(selector, "--motion-visual") == 0;
    motion_live_case = strcmp(selector, "--motion-live") == 0;
    motion_case = strcmp(selector, "--motion") == 0 || motion_visual_case || motion_live_case;
    body_discovery_visual_case = strcmp(selector, "--body-discovery-visual") == 0;
    body_discovery_copy_case = strcmp(selector, "--body-discovery-copy") == 0;
    body_discovery_measure_case = strcmp(selector, "--body-discovery-measure") == 0;
    body_discovery_case = strcmp(selector, "--body-discovery") == 0 || body_discovery_visual_case ||
                          body_discovery_copy_case || body_discovery_measure_case;
    discovery_visual_case = strcmp(selector, "--discovery-visual") == 0;
    discovery_copy_case = strcmp(selector, "--discovery-copy") == 0;
    discovery_measure_case = strcmp(selector, "--discovery-measure") == 0;
    discovery_case = strcmp(selector, "--discovery") == 0 || discovery_visual_case ||
                     discovery_copy_case || discovery_measure_case;
    tag_management_visual_case = strcmp(selector, "--tag-management-visual") == 0;
    tag_management_measure_case = strcmp(selector, "--tag-management-measure") == 0;
    tag_management_case = strcmp(selector, "--tag-management") == 0 || tag_management_visual_case ||
                          tag_management_measure_case;
    tags_visual_case = strcmp(selector, "--tags-visual") == 0;
    tags_reload_case = strcmp(selector, "--tags-reload") == 0;
    tags_metrics_case = strcmp(selector, "--tags-metrics") == 0;
    tags_case = strcmp(selector, "--tags") == 0 || tags_visual_case || tags_reload_case ||
                tags_metrics_case || tag_management_case || discovery_case || body_discovery_case ||
                motion_case || scale_note_count != 0;
}

static void select_clipboard_fixture(const char *_Nonnull selector)
{
    paste_case = strcmp(selector, "--paste") == 0;
    code_copy_visual_case = strcmp(selector, "--code-copy-visual") == 0;
    code_copy_keys_case_selected = strcmp(selector, "--code-copy-keys") == 0;
    code_copy_scroll_case = strcmp(selector, "--code-copy-scroll") == 0;
    code_copy_case = strcmp(selector, "--code-copy") == 0 || code_copy_visual_case ||
                     code_copy_keys_case_selected || code_copy_scroll_case;
}

static void select_about_fixture(const char *_Nonnull selector)
{
    about_visual_case = strcmp(selector, "--about-visual") == 0;
    about_case = strcmp(selector, "--about") == 0 || about_visual_case;
}

static HWINSTA fixture_clipboard_station(void)
{
    bool copies = paste_case || discovery_copy_case || body_discovery_copy_case ||
                  (code_copy_case && !code_copy_visual_case && !code_copy_keys_case_selected &&
                   !code_copy_scroll_case);
    return copies ? isolate_paste_clipboard() : nullptr;
}

int main(int argc, char *_Nonnull *_Nonnull argv)
{
    const char *selector = argc == 2 ? argv[1] : "";
    select_tags_fixture(selector);
    select_clipboard_fixture(selector);
    format_case = strcmp(selector, "--format-keys") == 0;
    select_favorites_fixture(selector);
    layout_case = strcmp(selector, "--layout") == 0;
    save_as_case = strcmp(selector, "--save-as") == 0;
    rename_case = strcmp(selector, "--rename") == 0;
    move_edit_case = strcmp(selector, "--move-edit") == 0;
    unicode_save_case = strcmp(selector, "--unicode-save") == 0;
    first_save_case = strcmp(selector, "--first-save") == 0;
    first_save_modal_case = strcmp(selector, "--first-save-modal") == 0;
    empty_save_case = strcmp(selector, "--empty-save") == 0;
    locked_save_case = strcmp(selector, "--locked-save") == 0;
    filter_save_case = strcmp(selector, "--filter-save") == 0;
    session_save_case = strcmp(selector, "--session-save") == 0;
    session_untitled_case = strcmp(selector, "--session-untitled") == 0;
    session_ime_case = strcmp(selector, "--session-ime") == 0;
    session_modal_case = strcmp(selector, "--session-modal") == 0;
    close_modal_case = strcmp(selector, "--close-modal") == 0;
    export_case = strcmp(selector, "--export") == 0;
    select_about_fixture(selector);
    HWINSTA original_station = GetProcessWindowStation();
    HDESK original_desktop = GetThreadDesktop(GetCurrentThreadId());
    HWINSTA paste_station = fixture_clipboard_station();
    HDESK desktop = CreateDesktopW(L"NeNeFolioImageStatus520", nullptr, nullptr, 0,
                                   DESKTOP_CREATEWINDOW | DESKTOP_CREATEMENU | DESKTOP_READOBJECTS |
                                       DESKTOP_WRITEOBJECTS,
                                   nullptr);
    require(desktop != nullptr && SetThreadDesktop(desktop), "isolated desktop");
    require(SUCCEEDED(OleInitialize(nullptr)), "COM apartment");
    diagram_case = strcmp(selector, "--mermaid") == 0;
    history_case = strcmp(selector, "--history") == 0;
    replace_case = strcmp(selector, "--replace") == 0;
    replace_save_case = strcmp(selector, "--replace-save") == 0;
    search_case = strcmp(selector, "--search") == 0;
    note_switch_case = strcmp(selector, "--note-switch") == 0;
    zoom_case = strcmp(selector, "--zoom") == 0;
    theme_preview_case = strcmp(selector, "--theme-preview") == 0;
    settings_edit_case = strcmp(selector, "--settings-edit") == 0;
    mode_save_case = strcmp(selector, "--mode-save") == 0;
    discard_case = strcmp(selector, "--discard") == 0;
    palette_case = strcmp(selector, "--palette") == 0;
    struct native_fixture fixture = {0};
    create_fixture(&fixture);
    run_fixture(&fixture, argc, argv);
    destroy_fixture(&fixture);
    OleUninitialize();
    if (paste_station != nullptr)
    {
        require(SetProcessWindowStation(original_station), "restore the process station");
        require(SetThreadDesktop(original_desktop), "restore the original thread desktop");
        require(CloseDesktop(desktop), "release the isolated clipboard desktop");
        require(CloseWindowStation(paste_station), "release the isolated clipboard station");
    }
    else
        CloseDesktop(desktop);
    printf("image/status: %u checks passed\n", checks);
    return EXIT_SUCCESS;
}
