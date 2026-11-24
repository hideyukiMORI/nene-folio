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
#include "folio_launch_link.h"
#include "folio_menu_action.h"
#include "folio_message.h"
#include "folio_open_link.h"
#include "folio_settings.h"
#include "folio_state.h"
#include "folio_window.h"
#include "font_bundle.h"
#include "icon_paint.h"
#include "image_bitmap.h"
#include "image_port.h"
#include "menu_request.h"
#include "name_list.h"
#include "note_file_times.h"
#include "note_history.h"
#include "note_identity.h"
#include "note_ledger.h"
#include "note_pane_code_button.h"
#include "note_pane_ole.h"
#include "note_pane_table.h"
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
#include "storage_path.h"
#include "storage_state.h"
#include "tag_catalog.h"
#include "test_window_file.h"
#include "text_buffer.h"
#include "transfer_adapter.h"
#include "ui_face.h"
#include "ui_part.h"
#include "ui_text.h"
#include "ui_text_request.h"
#include "utf16_text.h"
#include "utf8_text.h"
#include "web_link_adapter.h"
#include "web_uri.h"
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>
#include <shlobj.h>

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
static bool discovery_cut_case;
static bool move_category_case;
static bool move_category_visual;
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
static bool body_discovery_96;
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
static bool footnote_jump_case;
static bool definition_list_case;
static bool open_link_case;       /* #1071: a link opens a note and selects a place */
static bool request_prompt_case;  /* #1152: a request file while editing with unsaved text */
static bool outside_refresh_case; /* #1070: the data folder changed while the window was away */
static bool outline_case;
static bool outline_96; /* #975: the same case in 96 DPI windows, as on CI */
static bool code_copy_visual_case;
static bool bare_url_visual_case;
static bool search_key_visual_case;
static bool text_menu_case;
static bool window_place_case;
static bool code_copy_keys_case_selected;
static bool code_copy_scroll_case;
static bool code_copy_scroll_96;
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
/* #850: bare URLs next to Japanese text, in emphasis, a list, a table and beside <URL> forms. */
static const char bare_url_gallery[] =
    "# 囲まないURL\n\n## 見出しの `code` と **太字**（#853）\n\n"
    "詳細はhttps://example.com/docs/startを参照。\n\n"
    "ミラー: www.example.org/a/b?x=1 と (https://example.net/path)。\n\n"
    "**https://example.com/bold** と <https://example.com/angle> と "
    "[表示名](https://example.com/n)\n\n"
    "- 一覧の https://example.com/list.\n- `https://example.com/code` はコード\n\n"
    "| 名前 | URL |\n| -- | -- |\n| 例 | https://example.com/cell |\n\n"
    "脚注の例[^n]と[^m]（#869）。\n\n[^m]: **二つ目**の脚注と[参照][r]。\n[^n]: 一つ目の脚注。\n"
    "[r]: https://example.com/ref\n";
/* #971: definition lists: several descriptions, a wrapped one, a continued line, inline
 * formatting, and `: ` lines that are not descriptions (no term; inside a list). */
/* #1071: lines 1 heading, 3 and 5 paragraphs, 7-8 a list, 10 the last paragraph. */
static const char open_link_gallery[] =
    "# リンクで開く\n\n最初の段落。\n\n二つ目の段落に目印の文がある。\n\n- 項目A\n- 項目B\n\n"
    "最後の段落にも目印がある。\n";
static const char definition_gallery[] =
    "# 定義リスト\n\n閲覧表示\n: Markdownを整えて見せる表示。本文は**変えない**。\n\n編集表示\n"
    ": 原文をそのまま直す表示。長い説明は窓の幅で折り返し、左の線は説明のすべての行の横に続く。"
    "これは折り返しを確かめるための長い文で、三行目まで届く長さにしてある。\n"
    ": 保存は`自動`。\n続きの行は同じ説明。\n\n次の段落。\n\n"
    ": 用語の無い行は段落のまま。\n\n- リスト\n: リストの中は従来どおり\n\n表の前\n"
    ": 説明のすぐ後の表は、説明の行とつながらない。\n| 名前 | 値 |\n| -- | -- |\n| a | 1 |\n";
/* #975: headings of every level with text between them, long enough to scroll. */
static const char outline_gallery[] =
    "# 設計メモ\n\n最初の段落。\n\n## 背景\n\n背景の段落。\n\n## "
    "決めたこと\n\n決めたことの段落。\n\n"
    "### 色\n\n色の段落。\n\n### 配置\n\n配置の段落。\n\n#### 余白\n\n余白の段落。\n\n"
    "## とても長い見出しは欄の幅で切れて省略の印が付くことを確かめるための文\n\n段落。\n\n"
    "## 残り\n\n残りの段落。\n\n一\n\n二\n\n三\n\n四\n\n五\n\n六\n\n七\n\n八\n\n九\n\n十\n\n"
    "十一\n\n十二\n\n十三\n\n十四\n\n十五\n\n十六\n\n十七\n\n十八\n\n十九\n\n二十\n\n# "
    "付録\n\n付録の段落。\n";
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
    const struct note_store_request request = {category, note, body, nullptr, false};
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
    if (definition_list_case)
        return definition_gallery;
    if (open_link_case)
        return open_link_gallery;
    if (outline_case)
        return outline_gallery;
    if (bare_url_visual_case)
        return bare_url_gallery;
    if (code_copy_case)
        return code_copy_visual_case || code_copy_scroll_case ? code_copy_gallery
                                                              : code_copy_current;
    if (unicode_save_case)
        return unicode_current;
    return fallback;
}

/* #928: more notes than the finder shows (200), all with "cut" in the name and in the source.
 * They live in their own category, which the case removes again: the fixture folder is shared
 * with the other finder and tag cases, which count their notes. */
static void seed_cut(struct persistence_port disk)
{
    enum persistence_outcome created = disk.create_category(disk.adapter, "cut");
    require(created == PERSISTENCE_STORED || created == PERSISTENCE_NAME_TAKEN, "cut category");
    for (size_t at = 0; at < 210; ++at)
    {
        char name[16];
        require(snprintf(name, sizeof name, "cut-%03zu", at) > 0, "cut note name");
        struct note_text *body = nullptr;
        require(note_text_create("cutbody line", 12, &body) == NOTE_TEXT_ACCEPTED, "cut source");
        enum persistence_outcome made = disk.create_note(disk.adapter, "cut", name, body);
        require(made == PERSISTENCE_STORED || made == PERSISTENCE_NAME_TAKEN, "cut note saved");
        note_text_destroy(body);
    }
}

static void seed_tags(struct persistence_port disk)
{
    if (discovery_cut_case)
        seed_cut(disk);
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
    if (note_switch_case || favorites_case || open_link_case)
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
/* Nothing is taken in by this controlled port, except in the drop case: there a dropped path that
 * ends in .png is "copied" under its own file name and every other path is no image. No file is
 * touched either way. */
static bool image_drop_case;
static size_t image_drop_calls;
static enum image_adopt_outcome image_adopt(struct image_adapter *_Nonnull adapter,
                                            const char *_Nonnull category,
                                            const struct storage_path *_Nonnull source,
                                            struct text_buffer *_Nonnull leaf)
{
    (void)adapter;
    (void)category;
    if (!image_drop_case)
        return IMAGE_ADOPT_UNREADABLE;
    ++image_drop_calls;
    const char *path = storage_path_text(source);
    size_t length = strlen(path);
    const char *name = path + length;
    while (name > path && name[-1] != '\\' && name[-1] != '/')
        --name;
    if (length < 4 || strcmp(path + length - 4, ".png") != 0)
        return IMAGE_ADOPT_NOT_IMAGE;
    text_buffer_append_text(leaf, name);
    return IMAGE_ADOPT_COPIED;
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

/* #928: delete the private "cut" category of the fixture folder, file by file. Only names the
 * cut case itself creates there (its notes and the category's index) exist in it. */
static void remove_cut_category(const struct storage_state *_Nonnull location)
{
    wchar_t folder[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(location), folder, &length) &&
                length + 24 <= storage_native_capacity,
            "cut category path fits");
    memcpy(folder + length, L"\\cut", 5 * sizeof(wchar_t));
    wchar_t pattern[storage_native_capacity];
    require(swprintf_s(pattern, storage_native_capacity, L"%ls\\*", folder) > 0, "cut pattern");
    WIN32_FIND_DATAW entry;
    HANDLE search = FindFirstFileW(pattern, &entry);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do
    {
        wchar_t path[storage_native_capacity];
        if ((entry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
            swprintf_s(path, storage_native_capacity, L"%ls\\%ls", folder, entry.cFileName) > 0)
            DeleteFileW(path);
    } while (FindNextFileW(search, &entry));
    FindClose(search);
    RemoveDirectoryW(folder);
}

static void create_fixture(struct native_fixture *_Nonnull fixture)
{
    require(storage_adapter_create(&fixture->storage) == STORAGE_READY, "storage adapter");
    require(appearance_adapter_create(&fixture->appearance) == APPEARANCE_ADAPTER_CREATED,
            "appearance adapter");
    require(regex_adapter_create(&fixture->regex) == REGEX_ADAPTER_CREATED, "regex adapter");
    require(font_bundle_register(&fixture->fonts) == FONT_BUNDLE_READY, "bundled fonts");
    struct storage_port storage = storage_adapter_port(fixture->storage);
    test_window_file_attach(&storage); /* #894: in memory, never on disk */
    require(storage_state_create(&storage, &fixture->location) == STORAGE_READY,
            "portable storage");
    require(persistence_adapter_create(storage_state_directory(fixture->location),
                                       &fixture->files) == PERSISTENCE_ADAPTER_CREATED,
            "dedicated test data");
    /* A cut case that stopped early must not leave its notes for the cases that count them. */
    if (tags_case && !discovery_cut_case)
        remove_cut_category(fixture->location);
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
                                .asset = image_asset,
                                .adopt = image_adopt};
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

static void history_row_fonts(struct folio_window *_Nonnull window, struct accessible_item row);
static void palette_row_word_font(struct folio_window *_Nonnull window);

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
    history_row_fonts(window, row);
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
    palette_row_word_font(window);
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

/* #1149 / ADR0120 step 5: files dropped onto the body. The messages are synthesized (WM_DROPFILES
 * to the pane, EN_DROPFILES to the main window); a real drag from Explorer is checked by hand. */
constexpr size_t drop_text_capacity = 512;
static HDROP drop_handle(const wchar_t *_Nonnull const *_Nonnull paths, size_t count, POINT point)
{
    size_t units = 1;
    for (size_t at = 0; at < count; ++at)
        units += wcslen(paths[at]) + 1;
    HGLOBAL memory = GlobalAlloc(GHND, sizeof(DROPFILES) + units * sizeof(wchar_t));
    require(memory != nullptr, "drop handle memory");
    DROPFILES *files = GlobalLock(memory);
    require(files != nullptr, "drop handle lock");
    files->pFiles = sizeof *files;
    files->pt = point;
    files->fWide = TRUE;
    wchar_t *names = (wchar_t *)(files + 1);
    for (size_t at = 0; at < count; ++at)
    {
        size_t length = wcslen(paths[at]) + 1;
        memcpy(names, paths[at], length * sizeof(wchar_t));
        names += length;
    }
    GlobalUnlock(memory);
    return (HDROP)memory;
}
static size_t drop_body(struct folio_window *_Nonnull window, wchar_t *_Nonnull text)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    GETTEXTEX request = {
        .cb = drop_text_capacity * sizeof(wchar_t), .flags = GT_RAWTEXT, .codepage = 1200};
    LRESULT length = SendMessageW(body, EM_GETTEXTEX, (WPARAM)&request, (LPARAM)text);
    require(length >= 0 && (size_t)length < drop_text_capacity - 1, "drop body read");
    return (size_t)length;
}
/* The client point of the character at `position`, so the drop lands on a known place. */
static POINT drop_point(struct folio_window *_Nonnull window, LONG position)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    POINTL where = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&where, position);
    POINTL probe = {where.x + 1, where.y + 2};
    require(SendMessageW(body, EM_CHARFROMPOS, 0, (LPARAM)&probe) == position,
            "the drop point maps back to the character");
    return (POINT){probe.x, probe.y};
}
static void drop_on_pane(struct folio_window *_Nonnull window,
                         const wchar_t *_Nonnull const *_Nonnull paths, size_t count, LONG position)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HDROP drop = drop_handle(paths, count, drop_point(window, position));
    SendMessageW(body, WM_DROPFILES, (WPARAM)drop, 0); /* the pane frees the handle */
}
/* Runs the idle turns the way the message loop does, until the drop has finished. */
static size_t drop_turns(struct folio_window *_Nonnull window)
{
    size_t turns = 0;
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    for (; turns < 200 && !IsWindowEnabled(root); ++turns)
    {
        require(folio_window_idle_work_ready(window), "a running drop keeps the idle turn");
        folio_window_continue_idle_work(window);
        pump(window);
    }
    require(IsWindowEnabled(root), "the window takes input again after the drop");
    return turns;
}
static void expect_drop_notice(struct folio_window *_Nonnull window,
                               const struct folio_state *_Nonnull state,
                               struct ui_text_request request)
{
    char line[1024];
    wchar_t wide[1024];
    request.language = folio_state_language(state);
    require(ui_text_format(&request, line, sizeof line) == UI_TEXT_FORMAT_READY &&
                MultiByteToWideChar(CP_UTF8, 0, line, -1, wide, 1024) > 0,
            "expected drop line");
    expect_notice(window, wide);
}
static void expect_drop_text(struct folio_window *_Nonnull window, const wchar_t *_Nonnull before,
                             size_t position, const wchar_t *_Nonnull inserted)
{
    wchar_t expected[drop_text_capacity];
    wchar_t text[drop_text_capacity];
    size_t length = drop_body(window, text);
    require(wcslen(before) + wcslen(inserted) < drop_text_capacity, "expected text fits");
    memcpy(expected, before, position * sizeof(wchar_t));
    memcpy(expected + position, inserted, wcslen(inserted) * sizeof(wchar_t));
    memcpy(expected + position + wcslen(inserted), before + position,
           (wcslen(before) - position + 1) * sizeof(wchar_t));
    require(length == wcslen(expected) && wcscmp(text, expected) == 0,
            "the Markdown is written at the dropped place and nothing else changes");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE caret = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&caret);
    require(caret.cpMin == caret.cpMax && (size_t)caret.cpMin == position + wcslen(inserted),
            "the caret sits after what was written");
}
/* An empty list of paths, for asking the state whether a drop could start. */
static struct storage_path *_Nonnull const *_Nonnull nullptr_paths(void)
{
    static struct storage_path *_Nonnull const none[1];
    return none;
}
static void drop_refused_untitled(struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const one[] = {L"C:\\drop\\a.png"};
    require(folio_state_new_note(fixture->state, 0, FOLIO_UNTITLED_PROTECT) == FOLIO_STATE_READY,
            "an untitled draft");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "window on an untitled draft");
    pump(window);
    wchar_t before[drop_text_capacity];
    wchar_t after[drop_text_capacity];
    (void)drop_body(window, before);
    drop_on_pane(window, one, 1, 0);
    pump(window);
    (void)drop_body(window, after);
    require(image_drop_calls == 0 && wcscmp(before, after) == 0 &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_WINDOW)),
            "a drop onto an untitled draft copies nothing and writes nothing");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_UNTITLED});
    folio_window_destroy(window);
}
static void drop_written(struct folio_window *_Nonnull window,
                         struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const mixed[] = {L"C:\\drop\\a.png", L"C:\\drop\\notes.txt",
                                           L"C:\\drop\\b c.png"};
    static const wchar_t *const one[] = {L"D:\\pictures\\photo.png"};
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    (void)drop_body(window, before);
    drop_on_pane(window, mixed, 3, 5);
    require(!IsWindowEnabled(root) && image_drop_calls == 0,
            "the drop starts with input held and takes no file in the message itself");
    (void)drop_body(window, now);
    require(wcscmp(before, now) == 0, "nothing is written before the files are taken in");
    /* Keys typed while the files are taken in must not reach the body: they would move the text
     * under the dropped place. */
    PostMessageW(body, WM_CHAR, L'x', 0);
    PostMessageW(body, WM_KEYDOWN, VK_BACK, 0);
    PostMessageW(body, WM_CHAR, L'\b', 0);
    PostMessageW(body, WM_KEYDOWN, 'Z', 0);
    pump(window);
    (void)drop_body(window, now);
    require(wcscmp(before, now) == 0 && !IsWindowEnabled(root),
            "typing during the drop changes nothing");
    require(drop_turns(window) == 3 && image_drop_calls == 3, "one file per idle turn");
    expect_drop_text(window, before, 5, L"![a](images/a.png)\r![b c](<images/b c.png>)");
    require(SendMessageW(body, EM_GETMODIFY, 0, 0) != 0 && GetFocus() == body,
            "the note reads as changed and the body has the keyboard again");
    size_t written = drop_body(window, now);
    require(folio_state_unsaved(fixture->state, now, written) == FOLIO_NOTE_CHANGED,
            "the application sees the note as unsaved");
    expect_drop_notice(
        window, fixture->state,
        (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_SOME_NOT_IMAGE, .n = 2, .k = 1});
    require(folio_state_image_adoption_summary(fixture->state).dropped == 0 &&
                folio_state_begin_image_adoption(fixture->state, nullptr_paths(), 0) ==
                    IMAGE_ADOPTION_NOTHING,
            "the finished drop is closed: the next one is not refused as busy");
    history_undo(window);
    (void)drop_body(window, now);
    require(wcscmp(before, now) == 0, "one Undo takes the written Markdown out again");
    drop_on_pane(window, one, 1, 0);
    require(drop_turns(window) == 1 && image_drop_calls == 4, "a single file takes one turn");
    expect_drop_text(window, before, 0, L"![photo](images/photo.png)");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_TAKEN, .n = 1});
    history_undo(window);
}
/* The body is read again from the file while the files are taken in: the dropped place belonged to
 * the old text, so nothing is written, the copies stay, and the window takes input again. */
static void drop_displaced(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const three[] = {L"C:\\drop\\a.png", L"C:\\drop\\b.png",
                                           L"C:\\drop\\c.png"};
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    size_t calls = image_drop_calls;
    (void)drop_body(window, before);
    drop_on_pane(window, three, 3, 4);
    require(!IsWindowEnabled(root) && folio_window_idle_work_ready(window),
            "the drop to be displaced is running");
    folio_window_continue_idle_work(window);
    require(image_drop_calls == calls + 1 &&
                folio_state_reload_document(fixture->state) == FOLIO_STATE_READY,
            "one file is taken in, then the body is read again");
    require(drop_turns(window) == 1 && image_drop_calls == calls + 1,
            "the rest of the files are left alone");
    (void)drop_body(window, now);
    require(wcscmp(before, now) == 0, "nothing is written into a body that was read again");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_NOT_WRITTEN, .n = 1});
}
/* A dropped name that is no absolute path cannot be handed on: it counts as unreadable. */
static void drop_lost(struct folio_window *_Nonnull window, struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const pair[] = {L"relative.png", L"C:\\drop\\kept.png"};
    static const wchar_t *const lone[] = {L"relative.png"};
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    (void)drop_body(window, before);
    drop_on_pane(window, pair, 2, 1);
    require(drop_turns(window) == 1, "only the usable path is taken in");
    expect_drop_text(window, before, 1, L"![kept](images/kept.png)");
    expect_drop_notice(
        window, fixture->state,
        (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_SOME_UNREADABLE, .n = 1, .k = 1});
    history_undo(window);
    drop_on_pane(window, lone, 1, 1);
    pump(window);
    (void)drop_body(window, now);
    require(wcscmp(before, now) == 0 &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_WINDOW)),
            "a drop with no usable path starts nothing");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_NONE_UNREADABLE, .k = 1});
}
/* The edit control's own drop target reports files with EN_DROPFILES; the position comes with it.
 */
static void drop_notified(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const one[] = {L"C:\\drop\\from-target.png"};
    static const wchar_t *const none[] = {L"C:\\drop\\readme.md"};
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    (void)drop_body(window, before);
    HDROP drop = drop_handle(one, 1, (POINT){0, 0});
    ENDROPFILES notice = {
        .nmhdr = {.hwndFrom = body, .idFrom = (UINT_PTR)GetDlgCtrlID(body), .code = EN_DROPFILES},
        .hDrop = drop,
        .cp = 7,
        .fProtected = FALSE};
    require(SendMessageW(root, WM_NOTIFY, notice.nmhdr.idFrom, (LPARAM)&notice) == 0,
            "the edit control is told not to take the files itself");
    GlobalFree(drop); /* with the notification the handle stays the sender's */
    require(drop_turns(window) == 1, "the notified drop runs the same way");
    expect_drop_text(window, before, 7, L"![from-target](images/from-target.png)");
    history_undo(window);
    drop_on_pane(window, none, 1, 3);
    require(drop_turns(window) == 1, "a drop with nothing to take still finishes");
    (void)drop_body(window, now);
    require(wcscmp(before, now) == 0, "nothing is written when nothing was taken in");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_NONE_NOT_IMAGE, .k = 1});
}
/* #1195: a real drag from Explorer does not come as WM_DROPFILES in the editing view. The edit
 * control registers its own OLE drop target there and asks the pane's IRichEditOleCallback. The
 * tests hand that drop target, and a callback made the way the pane makes its own, a data object
 * (the shell's empty one, filled with one format). No pointer is moved and no file or clipboard is
 * touched; a drag by hand from Explorer is still checked by hide. */
static struct accessible_view text_menu_row(struct folio_window *_Nonnull window, size_t slot);
constexpr DWORD drop_effects = DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK;
static void drop_add(IDataObject *_Nonnull data, CLIPFORMAT format, HGLOBAL _Nonnull memory)
{
    FORMATETC shape = {
        .cfFormat = format, .dwAspect = DVASPECT_CONTENT, .lindex = -1, .tymed = TYMED_HGLOBAL};
    STGMEDIUM medium = {.tymed = TYMED_HGLOBAL, .hGlobal = memory};
    require(SUCCEEDED(data->lpVtbl->SetData(data, &shape, &medium, TRUE)),
            "the data object takes the format");
}
static IDataObject *_Nonnull drop_data(CLIPFORMAT format, HGLOBAL _Nonnull memory)
{
    IDataObject *data = nullptr;
    require(SUCCEEDED(SHCreateDataObject(nullptr, 0, nullptr, nullptr, &IID_IDataObject,
                                         (void **)&data)) &&
                data != nullptr,
            "an empty data object");
    drop_add(data, format, memory);
    return data;
}
static IDataObject *_Nonnull drop_files_data(const wchar_t *_Nonnull const *_Nonnull paths,
                                             size_t count)
{
    return drop_data(CF_HDROP, (HGLOBAL)drop_handle(paths, count, (POINT){0, 0}));
}
static IDataObject *_Nonnull drop_words_data(const wchar_t *_Nonnull words)
{
    size_t bytes = (wcslen(words) + 1) * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GHND, bytes);
    require(memory != nullptr, "dragged text memory");
    void *text = GlobalLock(memory);
    require(text != nullptr, "dragged text lock");
    memcpy(text, words, bytes);
    GlobalUnlock(memory);
    return drop_data(CF_UNICODETEXT, memory);
}
static LONG drop_objects(HWND _Nonnull body)
{
    IRichEditOle *ole = nullptr;
    require(SendMessageW(body, EM_GETOLEINTERFACE, 0, (LPARAM)&ole) != 0 && ole != nullptr,
            "the edit control's object list");
    LONG count = ole->lpVtbl->GetObjectCount(ole);
    ole->lpVtbl->Release(ole);
    return count;
}
/* Enter, move over and drop on the edit control's own drop target, at the character `position`.
 * Returns the pointer effect the target answered while the data was over it. */
static DWORD drop_gesture(struct folio_window *_Nonnull window, IDataObject *_Nonnull data,
                          LONG position)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    IDropTarget *target = (IDropTarget *)GetPropW(body, L"OleDropTargetInterface");
    require(target != nullptr, "the editing view registers the edit control's own drop target");
    POINT point = drop_point(window, position);
    ClientToScreen(body, &point);
    POINTL where = {point.x, point.y};
    DWORD entered = drop_effects;
    DWORD over = drop_effects;
    DWORD dropped = drop_effects;
    require(target->lpVtbl->DragEnter(target, data, MK_LBUTTON, where, &entered) == S_OK &&
                target->lpVtbl->DragOver(target, MK_LBUTTON, where, &over) == S_OK &&
                over == entered,
            "the data moves over the editing view");
    require(SUCCEEDED(target->lpVtbl->Drop(target, data, 0, where, &dropped)),
            "the data is dropped on the editing view");
    return entered;
}
static void drop_target_files(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const one[] = {L"C:\\drop\\ole.png"};
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    size_t calls = image_drop_calls;
    (void)drop_body(window, before);
    IDataObject *files = drop_files_data(one, 1);
    require(drop_gesture(window, files, 5) == DROPEFFECT_COPY,
            "files over the editing view show the copy pointer");
    (void)drop_body(window, now);
    require(!IsWindowEnabled(root) && image_drop_calls == calls && wcscmp(before, now) == 0,
            "an OLE drop of files starts the same intake and writes nothing itself");
    require(drop_turns(window) == 1 && image_drop_calls == calls + 1,
            "the OLE drop takes the file in on the idle turn");
    expect_drop_text(window, before, 5, L"![ole](images/ole.png)");
    require(drop_objects(body) == 0, "the dropped file is not embedded in the body");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_TAKEN, .n = 1});
    history_undo(window);
    files->lpVtbl->Release(files);
}
static void drop_target_words(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t before[drop_text_capacity];
    size_t calls = image_drop_calls;
    (void)drop_body(window, before);
    IDataObject *words = drop_words_data(L"DROPPED");
    require(drop_gesture(window, words, 3) == DROPEFFECT_MOVE,
            "text over the editing view keeps the edit control's own pointer");
    pump(window);
    expect_drop_text(window, before, 3, L"DROPPED");
    require(IsWindowEnabled(root) && image_drop_calls == calls && drop_objects(body) == 0,
            "dragged text is written as text and starts no intake");
    history_undo(window);
    words->lpVtbl->Release(words);
}
/* What the callback answers while files are over the body and when they are dropped. */
static void drop_callback_files(struct folio_window *_Nonnull window,
                                IRichEditOleCallback *_Nonnull callback)
{
    static const wchar_t *const one[] = {L"C:\\drop\\asked.png"};
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t before[drop_text_capacity];
    size_t calls = image_drop_calls;
    (void)drop_body(window, before);
    IDataObject *files = drop_files_data(one, 1);
    IDataObject *words = drop_words_data(L"LATE");
    CLIPFORMAT format = 0;
    SendMessageW(body, EM_SETSEL, 4, 4);
    require(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, FALSE,
                                              nullptr) == S_FALSE &&
                IsWindowEnabled(root) && !folio_window_idle_work_ready(window),
            "asking whether files may be dropped takes nothing in");
    DWORD effect = drop_effects;
    DWORD moving = DROPEFFECT_MOVE;
    require(callback->lpVtbl->GetDragDropEffect(callback, FALSE, MK_LBUTTON, &effect) == S_OK &&
                effect == DROPEFFECT_COPY &&
                callback->lpVtbl->GetDragDropEffect(callback, FALSE, MK_LBUTTON, &moving) == S_OK &&
                moving == DROPEFFECT_NONE,
            "files are copied, or refused when the source allows no copy");
    require(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, TRUE, nullptr) ==
                    S_FALSE &&
                !IsWindowEnabled(root) && image_drop_calls == calls,
            "dropped files go to the intake and the edit control is told they were handled");
    require(FAILED(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, FALSE,
                                                     nullptr)) &&
                FAILED(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, TRUE,
                                                         nullptr)),
            "nothing more is dropped while the files are taken in");
    require(drop_turns(window) == 1 && image_drop_calls == calls + 1,
            "the refused second drop adds no file");
    expect_drop_text(window, before, 4, L"![asked](images/asked.png)");
    history_undo(window);
    words->lpVtbl->Release(words);
    files->lpVtbl->Release(files);
}
/* Text is taken as text only, dropped or pasted; a paste starts no intake. */
static void drop_callback_words(struct folio_window *_Nonnull window,
                                IRichEditOleCallback *_Nonnull callback)
{
    static const wchar_t *const one[] = {L"C:\\drop\\pasted.png"};
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    size_t calls = image_drop_calls;
    IDataObject *files = drop_files_data(one, 1);
    IDataObject *words = drop_words_data(L"WORDS");
    CLIPFORMAT format = 0;
    DWORD effect = drop_effects;
    require(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, FALSE,
                                              nullptr) == S_OK &&
                format == CF_UNICODETEXT,
            "dragged text is accepted in the plain text format only");
    require(FAILED(callback->lpVtbl->GetDragDropEffect(callback, FALSE, MK_LBUTTON, &effect)) &&
                effect == drop_effects &&
                callback->lpVtbl->GetDragDropEffect(callback, TRUE, 0, &effect) == S_OK &&
                effect == drop_effects,
            "the pointer for text and for a drag out of the body is left to the edit control");
    format = CF_HDROP;
    require(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_PASTE, TRUE,
                                              nullptr) == S_OK &&
                format == CF_UNICODETEXT && IsWindowEnabled(root) && image_drop_calls == calls &&
                !folio_window_idle_work_ready(window),
            "a paste is not a drop: it starts no intake, and it is taken as plain text only");
    format = CF_HDROP;
    require(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_COPY, TRUE, nullptr) ==
                    S_OK &&
                format == CF_HDROP,
            "a question that is neither a drop nor a paste is left as it was asked");
    words->lpVtbl->Release(words);
    files->lpVtbl->Release(files);
}
/* No storage for embedded objects, no menu of its own, and the callback counts its holders. */
static void drop_callback_objects(IRichEditOleCallback *_Nonnull callback)
{
    CLSID kind = {0};
    IStorage *storage = (IStorage *)callback;
    HMENU menu = (HMENU)callback;
    IRichEditOleCallback *same = nullptr;
    IDropTarget *other = (IDropTarget *)callback;
    require(FAILED(callback->lpVtbl->GetNewStorage(callback, &storage)) && storage == nullptr,
            "the body gives an embedded object no storage");
    require(callback->lpVtbl->QueryInsertObject(callback, &kind, nullptr, 0) == S_OK,
            "the reading view's own pictures may still be inserted");
    require(FAILED(callback->lpVtbl->GetContextMenu(callback, SEL_TEXT, nullptr, nullptr, &menu)) &&
                menu == nullptr,
            "the callback offers no menu of its own");
    require(callback->lpVtbl->QueryInterface(callback, &IID_IUnknown, (void **)&same) == S_OK &&
                same == callback && callback->lpVtbl->Release(callback) == 1 &&
                callback->lpVtbl->QueryInterface(callback, &IID_IDropTarget, (void **)&other) ==
                    E_NOINTERFACE &&
                other == nullptr,
            "the callback answers for itself only");
    require(callback->lpVtbl->AddRef(callback) == 2 && callback->lpVtbl->Release(callback) == 1,
            "the callback counts its holders");
}
/* With a callback set, the edit control would answer the right button itself and make no
 * WM_CONTEXTMENU: the app's own menu must still open on a real right click. */
static int drop_right_click(struct folio_window *_Nonnull window, LONG position)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    POINT point = drop_point(window, position);
    SendMessageW(body, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(point.x, point.y));
    SendMessageW(body, WM_RBUTTONUP, 0, MAKELPARAM(point.x, point.y));
    pump(window);
    int rows = text_menu_row(window, 1).set_size;
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_KEYDOWN, VK_ESCAPE,
                 0);
    pump(window);
    return rows;
}
static void drop_keeps_menu(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE chosen = {0};
    SendMessageW(body, EM_SETSEL, 8, 8);
    require(drop_right_click(window, 2) == 5,
            "a right click still opens the editing view's own menu");
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&chosen);
    require(chosen.cpMin == 2 && chosen.cpMax == 2, "the right click moved the caret there first");
    SendMessageW(body, EM_SETSEL, 2, 9);
    require(drop_right_click(window, 5) == 5, "a right click inside the selection opens the menu");
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&chosen);
    require(chosen.cpMin == 2 && chosen.cpMax == 9, "and the selection stays as it was");
    SendMessageW(body, EM_SETSEL, 0, 0);
    action(window, ACCESSIBLE_VIEW);
    require(drop_right_click(window, 2) == 4,
            "a right click in the reading view opens the reading view's menu");
    action(window, ACCESSIBLE_EDIT);
}
/* #1195 review: text dragged with a rich format beside it lands as plain text only. */
static void drop_target_rich(struct folio_window *_Nonnull window)
{
    static const char rich[] = "{\\rtf1\\ansi\\b RICH}";
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    wchar_t before[drop_text_capacity];
    (void)drop_body(window, before);
    CHARFORMAT2W plain = character_format(body, 3);
    HGLOBAL memory = GlobalAlloc(GHND, sizeof rich);
    require(memory != nullptr, "dragged rich text memory");
    void *text = GlobalLock(memory);
    require(text != nullptr, "dragged rich text lock");
    memcpy(text, rich, sizeof rich);
    GlobalUnlock(memory);
    IDataObject *both = drop_words_data(L"PLAIN");
    drop_add(both, (CLIPFORMAT)RegisterClipboardFormatW(L"Rich Text Format"), memory);
    (void)drop_gesture(window, both, 3);
    pump(window);
    expect_drop_text(window, before, 3, L"PLAIN");
    CHARFORMAT2W landed = character_format(body, 4);
    require((landed.dwEffects & CFE_BOLD) == (plain.dwEffects & CFE_BOLD) &&
                landed.yHeight == plain.yHeight && landed.wWeight == plain.wWeight &&
                drop_objects(body) == 0,
            "text dragged with a rich format beside it lands as plain text");
    history_undo(window);
    both->lpVtbl->Release(both);
}
/* #1195 review: while a composition is open the edit control's own drop target takes no text,
 * the way a paste is refused. */
static void drop_target_refused(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    IDropTarget *target = (IDropTarget *)GetPropW(body, L"OleDropTargetInterface");
    require(target != nullptr, "the edit control's own drop target");
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    (void)drop_body(window, before);
    IDataObject *words = drop_words_data(L"NEVER");
    POINT point = drop_point(window, 3);
    ClientToScreen(body, &point);
    POINTL where = {point.x, point.y};
    DWORD entered = drop_effects;
    DWORD dropped = drop_effects;
    SendMessageW(body, WM_IME_STARTCOMPOSITION, 0, 0);
    HRESULT enter = target->lpVtbl->DragEnter(target, words, MK_LBUTTON, where, &entered);
    HRESULT drop = target->lpVtbl->Drop(target, words, 0, where, &dropped);
    SendMessageW(body, WM_IME_ENDCOMPOSITION, 0, 0);
    pump(window);
    (void)drop_body(window, now);
    printf("drop while composing: enter %08lx effect %lu, drop %08lx effect %lu\n",
           (unsigned long)enter, entered, (unsigned long)drop, dropped);
    fflush(stdout);
    require(entered == DROPEFFECT_NONE && wcscmp(before, now) == 0,
            "text dropped while a composition is open is refused and writes nothing");
    words->lpVtbl->Release(words);
}
/* #1195 review: dragged text is refused in every state a paste is refused in (the pane answers
 * both from one place), and files are refused while the pane itself takes no input. */
static void drop_callback_guards(struct folio_window *_Nonnull window,
                                 IRichEditOleCallback *_Nonnull callback)
{
    static const wchar_t *const one[] = {L"C:\\drop\\guarded.png"};
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    size_t calls = image_drop_calls;
    IDataObject *files = drop_files_data(one, 1);
    IDataObject *words = drop_words_data(L"GUARD");
    CLIPFORMAT format = 0;
    require(SendMessageW(body, EM_SETREADONLY, TRUE, 0) != 0 &&
                FAILED(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, TRUE,
                                                         nullptr)),
            "dragged text is refused while the body is read-only");
    require(SendMessageW(body, EM_SETREADONLY, FALSE, 0) != 0, "the body is writable again");
    SendMessageW(body, WM_IME_STARTCOMPOSITION, 0, 0);
    require(FAILED(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, FALSE,
                                                     nullptr)) &&
                FAILED(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, TRUE,
                                                         nullptr)),
            "dragged text is refused while a composition is open");
    require(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, TRUE, nullptr) ==
                    S_FALSE &&
                IsWindowEnabled(root) && !folio_window_idle_work_ready(window),
            "files dropped while a composition is open are handed on and the window refuses them");
    SendMessageW(body, WM_IME_ENDCOMPOSITION, 0, 0);
    EnableWindow(body, FALSE);
    require(FAILED(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, TRUE,
                                                     nullptr)) &&
                FAILED(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, TRUE,
                                                         nullptr)) &&
                IsWindowEnabled(root) && !folio_window_idle_work_ready(window),
            "nothing is dropped while the body itself takes no input");
    EnableWindow(body, TRUE);
    require(callback->lpVtbl->QueryAcceptData(callback, words, &format, RECO_DROP, FALSE,
                                              nullptr) == S_OK &&
                format == CF_UNICODETEXT && image_drop_calls == calls,
            "the guards leave the body open for dragged text again");
    words->lpVtbl->Release(words);
    files->lpVtbl->Release(files);
}
/* #1195 review: "files are over the body" is not left standing for a question that is asked
 * without a new drop being announced (a drag out of the body, a paste). */
static void drop_callback_stale(IRichEditOleCallback *_Nonnull callback)
{
    static const wchar_t *const one[] = {L"C:\\drop\\left.png"};
    IDataObject *files = drop_files_data(one, 1);
    CLIPFORMAT format = 0;
    DWORD effect = drop_effects;
    require(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, FALSE,
                                              nullptr) == S_FALSE &&
                callback->lpVtbl->GetDragDropEffect(callback, FALSE, MK_LBUTTON, &effect) == S_OK &&
                effect == DROPEFFECT_COPY,
            "files are over the body");
    effect = drop_effects;
    require(callback->lpVtbl->GetDragDropEffect(callback, TRUE, 0, &effect) == S_OK &&
                FAILED(callback->lpVtbl->GetDragDropEffect(callback, FALSE, MK_LBUTTON, &effect)) &&
                effect == drop_effects,
            "a drag out of the body forgets the files that were over it");
    require(callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_DROP, FALSE,
                                              nullptr) == S_FALSE &&
                callback->lpVtbl->QueryAcceptData(callback, files, &format, RECO_PASTE, FALSE,
                                                  nullptr) == S_OK &&
                FAILED(callback->lpVtbl->GetDragDropEffect(callback, FALSE, MK_LBUTTON, &effect)) &&
                effect == drop_effects,
            "a paste question forgets them too");
    files->lpVtbl->Release(files);
}
/* #1195 review: lists of dropped files as another program might hand them over. The data object
 * is the test's own (the shell's would not keep a damaged block as it is): it says it holds files
 * and hands its block over once. Only the two methods the callback calls are filled in; the edit
 * control never sees it. */
struct drop_fake
{
    IDataObject object;
    HGLOBAL _Nullable memory;
};
static HRESULT STDMETHODCALLTYPE drop_fake_query(IDataObject *_Nonnull self,
                                                 FORMATETC *_Nullable shape)
{
    (void)self;
    return shape != nullptr && shape->cfFormat == CF_HDROP ? S_OK : DV_E_FORMATETC;
}
static HRESULT STDMETHODCALLTYPE drop_fake_get(IDataObject *_Nonnull self,
                                               FORMATETC *_Nullable shape,
                                               STGMEDIUM *_Nullable medium)
{
    struct drop_fake *fake = (struct drop_fake *)self;
    (void)shape;
    if (fake->memory == nullptr || medium == nullptr)
        return E_FAIL;
    *medium = (STGMEDIUM){.tymed = TYMED_HGLOBAL, .hGlobal = fake->memory};
    fake->memory = nullptr; /* the receiver frees the block */
    return S_OK;
}
static IDataObjectVtbl drop_fake_table = {.GetData = drop_fake_get,
                                          .QueryGetData = drop_fake_query};
/* `start` is what the header claims as the start of the names (0: the block is shorter than a
 * header). `names` are `bytes` bytes put after the header as they are, with no closing zeros added
 * (nullptr: the data object fails to hand anything over). `written` is the Markdown expected in the
 * body, or nullptr when the list must be reported as unreadable. */
struct drop_damage
{
    DWORD start;
    BOOL wide;
    const char *_Nullable names;
    size_t bytes;
    const wchar_t *_Nullable written;
};
static HGLOBAL _Nullable drop_damaged(const struct drop_damage *_Nonnull damage)
{
    if (damage->names == nullptr)
        return nullptr;
    size_t size = damage->start == 0 ? 8 : sizeof(DROPFILES) + damage->bytes;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, size);
    require(memory != nullptr && GlobalSize(memory) == size, "a block of exactly the wanted size");
    if (damage->start == 0)
        return memory;
    DROPFILES *files = GlobalLock(memory);
    require(files != nullptr, "damaged list lock");
    if (files == nullptr) /* said again for the analyzer, which does not follow require here */
        return nullptr;
    files->pFiles = damage->start;
    files->fWide = damage->wide;
    memcpy(files + 1, damage->names, damage->bytes);
    GlobalUnlock(memory);
    return memory;
}
static void drop_damage_case(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture,
                             IRichEditOleCallback *_Nonnull callback,
                             const struct drop_damage *_Nonnull damage)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct drop_fake fake = {.object = {.lpVtbl = &drop_fake_table},
                             .memory = drop_damaged(damage)};
    wchar_t before[drop_text_capacity];
    wchar_t now[drop_text_capacity];
    size_t calls = image_drop_calls;
    CLIPFORMAT format = 0;
    (void)drop_body(window, before);
    SendMessageW(body, EM_SETSEL, 4, 4);
    require(callback->lpVtbl->QueryAcceptData(callback, &fake.object, &format, RECO_DROP, TRUE,
                                              nullptr) == S_FALSE &&
                fake.memory == nullptr,
            "the edit control is told the dropped list was handled");
    if (damage->written == nullptr)
    {
        (void)drop_body(window, now);
        require(IsWindowEnabled(root) && !folio_window_idle_work_ready(window) &&
                    image_drop_calls == calls && wcscmp(before, now) == 0,
                "a list that cannot be read starts no intake and writes nothing");
        expect_drop_notice(
            window, fixture->state,
            (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_NONE_UNREADABLE, .k = 1});
        return;
    }
    require(drop_turns(window) == 1 && image_drop_calls == calls + 1,
            "a list with no closing zeros is read as exactly its one name");
    expect_drop_text(window, before, 4, damage->written);
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_TAKEN, .n = 1});
    history_undo(window);
}
/* Readable and unreadable lists take turns, so each "could not be read" line is a new one. */
static void drop_callback_damaged(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture,
                                  IRichEditOleCallback *_Nonnull callback)
{
    constexpr DWORD head = sizeof(DROPFILES);
    static const wchar_t odd[] = L"C:\\drop\\odd.png";
    static const wchar_t even[] = L"C:\\drop\\even.png";
    static const char narrow[] = "C:\\drop\\ansi.png";
    static const char narrow_odd[] = "C:\\drop\\ansi1.png";
    const struct drop_damage cases[] = {
        /* an odd count of name bytes: the last name's final unit is cut in half */
        {head, TRUE, (const char *)odd, sizeof odd - 3, L"![odd](images/odd.png)"},
        {4, TRUE, (const char *)odd, sizeof odd, nullptr}, /* names inside the header */
        {head, TRUE, (const char *)even, sizeof even - 2, L"![even](images/even.png)"},
        {head + sizeof odd + 2, TRUE, (const char *)odd, sizeof odd, nullptr}, /* past the end */
        {head, FALSE, narrow, sizeof narrow - 1, L"![ansi](images/ansi.png)"},
        {0, TRUE, (const char *)odd, 0, nullptr}, /* shorter than a header */
        {head, FALSE, narrow_odd, sizeof narrow_odd - 1, L"![ansi1](images/ansi1.png)"},
        {head, TRUE, nullptr, 0, nullptr}, /* the data object hands nothing over */
    };
    for (size_t at = 0; at < sizeof cases / sizeof cases[0]; ++at)
    {
        printf("damaged list %zu\n", at);
        fflush(stdout);
        drop_damage_case(window, fixture, callback, &cases[at]);
    }
}
/* #1195 review: a refused in-place context leaves nothing behind in what the caller handed in. */
static void drop_callback_in_place(IRichEditOleCallback *_Nonnull callback)
{
    IOleInPlaceFrame *frame = (IOleInPlaceFrame *)callback;
    IOleInPlaceUIWindow *document = (IOleInPlaceUIWindow *)callback;
    OLEINPLACEFRAMEINFO information = {.cb = sizeof information,
                                       .fMDIApp = TRUE,
                                       .hwndFrame = (HWND)callback,
                                       .haccel = (HACCEL)callback,
                                       .cAccelEntries = 3};
    require(FAILED(callback->lpVtbl->GetInPlaceContext(callback, &frame, &document, &information)),
            "the body offers no in-place context");
    require(frame == nullptr && document == nullptr && !information.fMDIApp &&
                information.hwndFrame == nullptr && information.haccel == nullptr &&
                information.cAccelEntries == 0 && information.cb == sizeof information,
            "the refused in-place context is handed back empty");
}
static void drop_through_ole(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    drop_target_files(window, fixture);
    drop_target_words(window);
    drop_target_rich(window);
    drop_target_refused(window);
    IRichEditOleCallback *callback = nullptr;
    require(note_pane_ole_create(body, &callback) && callback != nullptr,
            "a callback made the way the pane makes its own");
    drop_callback_files(window, callback);
    drop_callback_words(window, callback);
    drop_callback_guards(window, callback);
    drop_callback_stale(callback);
    drop_callback_damaged(window, fixture, callback);
    drop_callback_in_place(callback);
    drop_callback_objects(callback);
    require(callback->lpVtbl->Release(callback) == 0, "the last holder frees the callback");
    drop_keeps_menu(window);
}
static void run_image_drop_case(struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const one[] = {L"C:\\drop\\a.png"};
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the note the files are dropped onto");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "real drop window");
    pump(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require((GetWindowLongPtrW(body, GWL_EXSTYLE) & WS_EX_ACCEPTFILES) != 0,
            "the body accepts dropped files in the reading view too");
    require(GetPropW(body, L"OleDropTargetInterface") == nullptr,
            "the reading view has no drop target of its own: files come as WM_DROPFILES");
    wchar_t before[drop_text_capacity];
    wchar_t after[drop_text_capacity];
    (void)drop_body(window, before);
    drop_on_pane(window, one, 1, 2);
    pump(window);
    (void)drop_body(window, after);
    require(image_drop_calls == 0 && wcscmp(before, after) == 0 &&
                IsWindowEnabled(folio_window_accessible_window(window, ACCESSIBLE_WINDOW)),
            "a drop onto the reading view copies nothing and writes nothing");
    expect_drop_notice(window, fixture->state,
                       (struct ui_text_request){.id = UI_TEXT_IMAGE_DROP_VIEWING});
    action(window, ACCESSIBLE_EDIT);
    drop_written(window, fixture);
    drop_notified(window, fixture);
    drop_through_ole(window, fixture);
    drop_lost(window, fixture);
    drop_displaced(window, fixture);
    folio_window_destroy(window);
    image_drop_calls = 0;
    drop_refused_untitled(fixture);
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

/* #1199: the heading row's tooltip is the UIA name of the count, and it ends with the number of
 * notes the state reports now (the sentence is rewritten when the number changes). */
static void index_count_tooltip(struct folio_window *_Nonnull window,
                                const struct folio_state *_Nonnull state)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    wchar_t text[1024] = {0};
    TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                      .hwnd = drawer,
                      .uId = ACCESSIBLE_INDEX_COUNT,
                      .lpszText = text};
    require(tooltip != nullptr, "the drawer has a native tooltip");
    SendMessageW(tooltip, TTM_GETTEXTW, 1024, (LPARAM)&tool);
    struct accessible_item item = {.kind = ACCESSIBLE_INDEX_COUNT};
    struct accessible_view view = {0};
    wchar_t tail[32];
    require(swprintf(tail, 32, L" %zu", folio_state_note_count(state)) > 0, "the count fits");
    size_t length = wcslen(text);
    size_t end = wcslen(tail);
    require(folio_window_accessible_read(window, &item, &view) == S_OK &&
                wcscmp(text, view.name) == 0 && length > end &&
                wcscmp(text + length - end, tail) == 0,
            "the heading tooltip and the UIA count name the current number of notes");
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
    index_count_tooltip(window, fixture->state);
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
    if (image_drop_case)
    {
        run_image_drop_case(fixture);
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

/* #1199: the favorites chip is a Toggle whose state is the application's scope, and the clear
 * row exists only while a condition is in effect. */
static void expect_favorite_scope_name(struct folio_window *_Nonnull window, bool favorites)
{
    struct accessible_item chip = {.kind = ACCESSIBLE_FILTER_FAVORITES};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &chip, &view) == S_OK &&
                wcscmp(view.name, L"お気に入り") == 0 && view.selected == favorites &&
                !view.offscreen && view.focusable,
            "the favorites chip exposes the scope even when no row matches");
    struct accessible_item clear = {.kind = ACCESSIBLE_FILTER_CLEAR};
    struct accessible_view clear_view = {0};
    HRESULT read = folio_window_accessible_read(window, &clear, &clear_view);
    bool shown = read == S_OK && !clear_view.offscreen && !IsRectEmpty(&clear_view.bounds);
    require(shown == favorites, "the clear row exists only while a condition is in effect");
    if (shown)
        require(wcscmp(clear_view.name, L"絞り込みを解除") == 0 &&
                    clear_view.bounds.top >= view.bounds.bottom,
                "the clear row is named and sits below the chips");
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
    /* #1199: the chips' lit font (bold 11). Each chip alone fits the 200 DIP drawer's row
     * (168) with its padding and star (30). */
    HFONT font = CreateFontW(-MulDiv(11, (int)dpi, 96), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    HGDIOBJ old = SelectObject(dc, font);
    const enum ui_text labels[] = {UI_TEXT_TITLE_FAVORITES, UI_TEXT_TAG_FILTER_EMPTY};
    for (size_t at = 0; at < sizeof labels / sizeof labels[0]; ++at)
    {
        char16_t text[64];
        size_t count = 0;
        require(utf16_text_fill(ui_text_line(labels[at], language), text, 64, &count) ==
                    UTF16_TEXT_FILL_READY,
                "scope label conversion");
        SIZE size;
        require(GetTextExtentPoint32W(dc, text, (int)count, &size) &&
                    size.cx + MulDiv(30, (int)dpi, 96) <= MulDiv(168, (int)dpi, 96),
                "each filter chip fits one row of the 200 DIP drawer");
        printf("scope-text language=%u dpi=%u label=%u width=%ld available=%d face=%ls\n",
               (unsigned)language, dpi, (unsigned)labels[at], size.cx, MulDiv(168, (int)dpi, 96),
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

/* #1199: the scope is set through the favorites chip (UIA Toggle = the command's one path). */
static void select_favorite_scope(struct folio_window *_Nonnull window, bool favorites)
{
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_FILTER_FAVORITES},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &request.item, &view) == S_OK,
            "the favorites chip is readable");
    if (view.selected != favorites)
        require(folio_window_accessible_action(window, &request) == S_OK,
                "UIA toggles the favorites chip");
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
    select_favorite_scope(window, true);
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
                focus.kind == ACCESSIBLE_FILTER_FAVORITES,
            "empty tree focus lands on the favorites chip");
    select_favorite_scope(window, false);
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
                focus.kind == ACCESSIBLE_FILTER_FAVORITES,
            "Tab enters the chip row after search");
    favorite_key(window, VK_RIGHT);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
                folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_TAG_FILTER,
            "arrow moves only focus within the chip row");
    favorite_key(window, VK_LEFT);
    favorite_key(window, VK_SPACE);
    expect_favorite_scope_name(window, true);
    require(folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_FILTER_FAVORITES,
            "Space toggles the chip and keeps focus on it");
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &tree, &view) == S_OK && view.help[0] != 0,
            "empty favorites have an accessible explanation");
    favorite_empty_pixels(window, "favorites-empty.bmp");
    favorite_key(window, VK_TAB);
    require(folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_FILTER_CLEAR,
            "Tab stops once on the chip row, then on the clear row");
    favorite_key(window, VK_TAB);
    require(folio_window_accessible_focus(window, ACCESSIBLE_WINDOW, &focus) != S_OK ||
                focus.kind != ACCESSIBLE_TREE,
            "Tab skips an empty tree");
    select_favorite_scope(window, false);
    favorite_empty_scope(window, fixture);
    require(IsWindow(root), "scope keeps the same window");
}

static void favorite_tooltips(struct folio_window *_Nonnull window)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    /* #1199: the heading row's count is a tooltip and a UIA text with the same sentence. */
    const enum accessible_kind kinds[] = {ACCESSIBLE_FILTER_FAVORITES, ACCESSIBLE_INDEX_COUNT};
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
    struct accessible_item counts = {.kind = ACCESSIBLE_INDEX_COUNT};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &counts, &view) == S_OK &&
                wcscmp(view.name, L"カテゴリ 2 ・ ノート 2") == 0 && !view.focusable,
            "the heading row names the numbers of categories and notes, and takes no focus");
    struct accessible_item plus = {.kind = ACCESSIBLE_NEW_CATEGORY};
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE};
    struct accessible_view plus_view = {0};
    struct accessible_view tree_view = {0};
    require(folio_window_accessible_read(window, &plus, &plus_view) == S_OK &&
                folio_window_accessible_read(window, &tree, &tree_view) == S_OK &&
                view.bounds.left > tree_view.bounds.left &&
                view.bounds.right < plus_view.bounds.left && view.bounds.top < view.bounds.bottom &&
                view.bounds.bottom <= tree_view.bounds.top,
            "the count's rectangle is the heading row, left of the new-category button");
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

/* The first category row, found the way a UIA client finds it: the first child of the tree. */
static struct accessible_item first_category_item(struct folio_window *_Nonnull window)
{
    struct accessible_item tree = {.kind = ACCESSIBLE_TREE}, row = {0};
    require(folio_window_accessible_navigate(window, &tree, ACCESSIBLE_NAVIGATION_FIRST, &row) ==
                    S_OK &&
                row.kind == ACCESSIBLE_CATEGORY && row.ref.category == 0,
            "the tree's first child is the first category row");
    return row;
}

/* How many pixels of a one-pixel-wide or one-pixel-high strip differ from the ground. */
static size_t line_ink(HDC dc, RECT strip, COLORREF ground)
{
    size_t count = 0;
    for (int y = strip.top; y < strip.bottom; ++y)
        for (int x = strip.left; x < strip.right; ++x)
            count += GetPixel(dc, x, y) != ground;
    return count;
}

/* #1199: what one part of a drawer row shows. count is the number of pixels that differ from the
 * drawer's ground; width and height are the numbers of columns and rows that hold such pixels. */
struct row_ink
{
    size_t count;
    uint64_t hash;
    int width;
    int height;
};

/* The part is given in DIPs from the left edge (or from the right edge when the numbers are
 * negative or zero). */
static struct row_ink row_part_ink(struct folio_window *_Nonnull window,
                                   const struct accessible_item *_Nonnull item, int left, int right)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT client;
    GetClientRect(drawer, &client);
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(drawer, dc, PW_CLIENTONLY), "render the real drawer for the fold mark");
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, item, &view) == S_OK && !view.offscreen,
            "measure a visible drawer row");
    MapWindowPoints(nullptr, drawer, (POINT *)&view.bounds, 2);
    int dpi = (int)GetDpiForWindow(drawer);
    RECT part = view.bounds;
    part.left = (left < 0 ? client.right : 0) + MulDiv(left, dpi, 96);
    part.right = (right <= 0 ? client.right : 0) + MulDiv(right, dpi, 96);
    /* The row's own last column: nothing is drawn there on a row that is not selected. */
    COLORREF ground = GetPixel(dc, client.right - 2, part.top + 1);
    struct row_ink ink = {.hash = favorite_pixel_hash(dc, part)};
    for (int x = part.left; x < part.right; ++x)
    {
        size_t column = line_ink(dc, (RECT){x, part.top, x + 1, part.bottom}, ground);
        ink.count += column;
        ink.width += column != 0;
    }
    for (int y = part.top; y < part.bottom; ++y)
        ink.height += line_ink(dc, (RECT){part.left, y, part.right, y + 1}, ground) != 0;
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
    return ink;
}

static void fold_first_category(struct folio_window *_Nonnull window, enum accessible_action action)
{
    struct accessible_request request = {.item = first_category_item(window), .action = action};
    require(folio_window_accessible_action(window, &request) == S_OK,
            "fold or open the first category through the public tree");
    pump(window);
}

/* #1199: the title of a note row starts on the same line as the category name (42 DIP), not two
 * pixels to its left. The second note is measured: the first one is selected and has a face. */
static void note_title_edge(struct folio_window *_Nonnull window)
{
    struct accessible_item category = first_category_item(window), note = {0};
    require(folio_window_accessible_navigate(window, &category, ACCESSIBLE_NAVIGATION_FIRST,
                                             &note) == S_OK &&
                folio_window_accessible_navigate(window, &note, ACCESSIBLE_NAVIGATION_NEXT,
                                                 &note) == S_OK &&
                note.kind == ACCESSIBLE_NOTE,
            "the second note row of the first category");
    require(row_part_ink(window, &note, 0, 41).count == 0,
            "nothing of a note row is drawn left of the category name's line");
    require(row_part_ink(window, &note, 41, 48).count > 0,
            "the note title starts at the category name's line");
}

/* #1199: the fold mark sits in the left margin of a category row (before the number), and the
 * right column of that row is empty. An open category shows a mark that is wider than tall, a
 * folded one a mark that is taller than wide. The cursor is put back on the note afterwards. */
static void fold_mark_pixels(struct folio_window *_Nonnull window)
{
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_DRAWER), WM_MOUSELEAVE, 0, 0);
    note_title_edge(window);
    struct accessible_item category = first_category_item(window);
    struct row_ink open = row_part_ink(window, &category, 0, 16);
    require(open.count > 0 && open.width > open.height,
            "an open category row draws a downward mark in the left margin");
    require(row_part_ink(window, &category, -40, 0).count == 0,
            "the right column of a category row holds no fold mark");
    fold_first_category(window, ACCESSIBLE_ACTION_COLLAPSE);
    category = first_category_item(window);
    struct row_ink folded = row_part_ink(window, &category, 0, 16);
    require(folded.count > 0 && folded.height > folded.width && folded.hash != open.hash,
            "a folded category row draws a rightward mark in the same place");
    require(row_part_ink(window, &category, -40, 0).count == 0,
            "the right column stays empty when the category is folded");
    fold_first_category(window, ACCESSIBLE_ACTION_EXPAND);
    category = first_category_item(window);
    struct row_ink again = row_part_ink(window, &category, 0, 16);
    require(again.count == open.count && again.hash == open.hash,
            "opening the category brings the first mark back");
    struct accessible_request focus = {.item = favorite_item(window),
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK,
            "put the cursor back on the selected note");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    printf("fold-mark open=%dx%d folded=%dx%d ink=%zu\n", open.width, open.height, folded.width,
           folded.height, open.count);
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

/* #1215: how far left of the count's right edge the leftmost drawn pixel is, looking no further
 * than `reach` pixels. The count is right-aligned 8 DIP before the "new category" button. */
static int header_count_reach(struct folio_window *_Nonnull window, int reach)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT client;
    GetClientRect(drawer, &client);
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(drawer, dc, PW_CLIENTONLY), "render the real drawer for the count");
    struct accessible_item button = {.kind = ACCESSIBLE_NEW_CATEGORY};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &button, &view) == S_OK,
            "measure the new category button");
    MapWindowPoints(nullptr, drawer, (POINT *)&view.bounds, 2);
    int right = view.bounds.left - MulDiv(8, (int)GetDpiForWindow(drawer), 96);
    COLORREF ground = GetPixel(dc, right + 2, view.bounds.top + 2);
    int found = 0;
    for (int back = 1; back <= reach; ++back)
        if (line_ink(dc,
                     (RECT){right - back, view.bounds.top, right - back + 1, view.bounds.bottom},
                     ground) != 0)
            found = back;
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
    return found;
}

/* One font of the heading row: the face, its size in DIP and its weight. */
struct header_font
{
    const wchar_t *_Nonnull face;
    int size;
    int weight;
};

static int header_text_width(HWND drawer, const wchar_t *_Nonnull text, struct header_font font)
{
    HDC dc = GetDC(drawer);
    HFONT handle =
        CreateFontW(-MulDiv(font.size, (int)GetDpiForWindow(drawer), 96), 0, 0, 0, font.weight,
                    FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, font.face);
    HGDIOBJ old = SelectObject(dc, handle);
    SIZE size = {0};
    require(GetTextExtentPoint32W(dc, text, (int)wcslen(text), &size) != 0, "measure header text");
    SelectObject(dc, old);
    DeleteObject(handle);
    ReleaseDC(drawer, dc);
    return size.cx;
}

/* What the heading row should show: the unit in the language's face, a 6 DIP gap, then the
 * numbers in the monospace face. title is the heading's own text, which limits the room. */
struct header_count
{
    enum folio_language language;
    const wchar_t *_Nonnull title;
    const wchar_t *_Nonnull unit;
    const wchar_t *_Nonnull number;
};

/* The room the product gives the count: from 8 DIP right of the heading (which itself ends at
 * most 80 DIP before the button) to 8 DIP left of the button. */
static int header_count_room(struct folio_window *_Nonnull window, struct header_count expected)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    int dpi = (int)GetDpiForWindow(drawer);
    wchar_t face[LF_FACESIZE];
    ui_face_for(expected.language, face);
    struct accessible_item button = {.kind = ACCESSIBLE_NEW_CATEGORY};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &button, &view) == S_OK, "find the button");
    MapWindowPoints(nullptr, drawer, (POINT *)&view.bounds, 2);
    int left = MulDiv(16, dpi, 96);
    int title = header_text_width(drawer, expected.title, (struct header_font){face, 12, FW_BOLD});
    int limit = view.bounds.left - MulDiv(80, dpi, 96) - left;
    return view.bounds.left - MulDiv(8, dpi, 96) -
           (left + (title < limit ? title : limit) + MulDiv(8, dpi, 96));
}

/* The row shows the numbers, and the unit exactly when unit + gap + numbers fit the room. The
 * leftmost drawn pixel sits one line width left of the right edge, and the 10 DIP beyond that
 * are empty (a longer line would leave ink there). Returns whether the unit was expected. */
static bool expect_header_count(struct folio_window *_Nonnull window, struct header_count expected,
                                const char *_Nonnull what)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    int dpi = (int)GetDpiForWindow(drawer);
    wchar_t face[LF_FACESIZE];
    ui_face_for(expected.language, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    int number = header_text_width(drawer, expected.number,
                                   (struct header_font){mono, mono_size, FW_NORMAL});
    int unit = header_text_width(drawer, expected.unit, (struct header_font){face, 11, FW_NORMAL});
    int full = unit + MulDiv(6, dpi, 96) + number;
    int room = header_count_room(window, expected);
    bool with_unit = full <= room;
    int width = with_unit ? full : number;
    int reach = header_count_reach(window, width + MulDiv(14, dpi, 96));
    printf("header-count %s: room=%d full=%d number=%d unit=%d drawn=%d\n", what, room, full,
           number, (int)with_unit, reach);
    require(reach >= width - MulDiv(4, dpi, 96) && reach <= width + MulDiv(4, dpi, 96), what);
    return with_unit;
}

/* #1222: the main window draws the names of its buttons in the language's bundled face, not in
 * the monospace face. The "new note" button is as wide as its name measured in that face (the
 * name is not ASCII, so no tracking is added), rounded the way every command part is. */
static void word_font_parts(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    UINT dpi = GetDpiForWindow(root);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    int name = header_text_width(root, L"新しいノート", (struct header_font){face, 11, FW_NORMAL});
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    int linked =
        header_text_width(root, L"新しいノート", (struct header_font){mono, mono_size, FW_NORMAL});
    struct accessible_item button = {.kind = ACCESSIBLE_NEW_NOTE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &button, &view) == S_OK && !view.offscreen,
            "the new note button is visible");
    int width = view.bounds.right - view.bounds.left;
    printf("word-font new-note width=%d word=%d mono=%d dpi=%u\n", width,
           ui_part_command_width(name, true, dpi), ui_part_command_width(linked, true, dpi), dpi);
    require(width == ui_part_command_width(name, true, dpi),
            "the new note button is sized for its name in the language's face");
}

static int breadcrumb_width(struct folio_window *_Nonnull window)
{
    struct accessible_item crumb = {.kind = ACCESSIBLE_BREADCRUMB};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &crumb, &view) == S_OK && !view.offscreen,
            "the breadcrumb is visible");
    return view.bounds.right - view.bounds.left;
}

/* #1222: the breadcrumb measures the note's name in the language's bundled face. The two notes
 * of the fixture are "one" and "two": the same number of letters, so in the monospace face (and
 * with the tracking, which counts letters) the breadcrumb would be equally wide for both. In a
 * proportional face it differs by exactly the difference of the two names. */
static void breadcrumb_word_font(struct native_fixture *_Nonnull fixture,
                                 struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    struct header_font words = {face, 11, FW_NORMAL};
    int expected = header_text_width(root, L"two", words) - header_text_width(root, L"one", words);
    int first = breadcrumb_width(window);
    require(folio_state_select_note(fixture->state, 0, 1, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the second note");
    pump(window);
    int second = breadcrumb_width(window);
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the first note again");
    pump(window);
    /* At a DPI where the two names happen to be equally wide in the language's face, this check
     * cannot tell the two faces apart (it still holds); the line below says which case ran. */
    printf("breadcrumb one=%d two=%d expected-difference=%d (%s)\n", first, second, expected,
           expected != 0 ? "tells the faces apart" : "cannot tell the faces apart at this DPI");
    require(second - first == expected,
            "the breadcrumb is sized for the note's name in the language's face");
}

/* #1222: the status row draws its words in the language's bundled face and the values put into
 * the sentence (here the two key names) in the monospace face. The keys' part is right-aligned;
 * its leftmost drawn pixel sits one line width left of its rightmost one, and the 10 DIP beyond
 * that are empty (the note's part ends at least 24 DIP before it). */
/* #1253: how wide the ink of a band of the main window is, from its first inked column to its
 * last, as the window draws it. The band's top-left pixel is taken as the ground. */
static int root_ink_width(struct folio_window *_Nonnull window, RECT band)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    RECT client;
    GetClientRect(root, &client);
    HDC source = GetDC(root), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(root, dc, PW_CLIENTONLY), "render the main window for an ink width");
    COLORREF ground = GetPixel(dc, band.left, band.top);
    int first = band.right, last = band.left;
    for (int x = band.left; x < band.right; ++x)
    {
        if (line_ink(dc, (RECT){x, band.top, x + 1, band.bottom}, ground) != 0)
        {
            first = x < first ? x : first;
            last = x;
        }
    }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(root, source);
    return last >= first ? last - first + 1 : 0;
}

/* #1253: a line of words drawn inside a band is as wide as the language's face makes it, not as
 * wide as the monospace face would. Where the two faces differ by too little to tell at this
 * DPI, the check only reports and answers false: the caller says whether that is acceptable. */
static bool expect_word_face(struct folio_window *_Nonnull window, RECT band,
                             const wchar_t *_Nonnull text, const char *_Nonnull what)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    struct header_font words = {face, 11, FW_NORMAL}, values = {mono, mono_size, FW_NORMAL};
    int expected = header_text_width(root, text, words);
    int monospace = header_text_width(root, text, values);
    int drawn = root_ink_width(window, band);
    printf("word-face %s drawn=%d expected=%d monospace=%d\n", what, drawn, expected, monospace);
    if (monospace - expected < 5 && expected - monospace < 5)
    {
        return false;
    }
    require(drawn >= expected - 3 && drawn <= expected + 3, what);
    return true;
}

/* #1253: the tag strip above the body says its words in the language's face. */
static void tag_strip_word_font(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct accessible_item strip = {.kind = ACCESSIBLE_TAG_EDIT};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &strip, &view) == S_OK && !view.offscreen &&
                view.name[0] != 0,
            "the tag strip is visible and named");
    MapWindowPoints(nullptr, root, (POINT *)&view.bounds, 2);
    int inset = MulDiv(4, (int)GetDpiForWindow(root), 96);
    RECT band = {view.bounds.left + inset, view.bounds.top + inset, view.bounds.right - inset,
                 view.bounds.bottom - inset};
    require(expect_word_face(window, band, (const wchar_t *)view.name,
                             "the tag strip's words are drawn in the language's face"),
            "the tag strip's text tells the two faces apart, so the check ran");
    /* The guide marks its key names for drawing. The same sentence, as the new-note button's
     * spoken help, carries no mark. */
    struct accessible_item button = {.kind = ACCESSIBLE_NEW_NOTE};
    require(folio_window_accessible_read(window, &button, &view) == S_OK &&
                wcsstr((const wchar_t *)view.help, L"Ctrl+N") != nullptr,
            "the new-note button's help names its key");
    for (size_t at = 0; view.help[at] != 0; ++at)
        require(view.help[at] != 1 && view.help[at] != 2,
                "the marks around a key name never reach the new-note button's help");
}

static void status_keys_fonts(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int dpi = (int)GetDpiForWindow(root);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    struct header_font words = {face, 11, FW_NORMAL}, values = {mono, mono_size, FW_NORMAL};
    /* Spaces are drawn in the monospace face too, so the gaps match the key names' advance. */
    int expected =
        header_text_width(root, L"F1 ", values) + header_text_width(root, L"ヘルプ", words) +
        header_text_width(root, L"  ", values) + header_text_width(root, L"·", words) +
        header_text_width(root, L"  Ctrl+P ", values) + header_text_width(root, L"コマンド", words);
    int all_mono = header_text_width(root, L"F1 ヘルプ  ·  Ctrl+P コマンド", values);
    struct accessible_item notice = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &notice, &view) == S_OK && !view.offscreen,
            "the status row is visible");
    MapWindowPoints(nullptr, root, (POINT *)&view.bounds, 2);
    RECT client;
    GetClientRect(root, &client);
    HDC source = GetDC(root), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(root, dc, PW_CLIENTONLY), "render the real status row");
    COLORREF ground = GetPixel(dc, 2, client.bottom - 12);
    int top = view.bounds.top + 2, bottom = view.bounds.bottom;
    int right = client.right;
    while (right > 0 && line_ink(dc, (RECT){right - 1, top, right, bottom}, ground) == 0)
        --right;
    int reach = 0;
    for (int back = 1; back <= expected + MulDiv(14, dpi, 96); ++back)
        if (line_ink(dc, (RECT){right - back, top, right - back + 1, bottom}, ground) != 0)
            reach = back;
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(root, source);
    printf("status-keys drawn=%d expected=%d all-mono=%d dpi=%d\n", reach, expected, all_mono, dpi);
    /* The check below allows three pixels: it must be able to tell the two faces apart. */
    require(all_mono - expected > 8 || expected - all_mono > 8,
            "words in the monospace face would be clearly narrower or wider at this DPI");
    for (size_t at = 0; view.name[at] != 0; ++at)
        require(view.name[at] != 1 && view.name[at] != 2,
                "the marks around the values never reach the status row's spoken name");
    /* Three pixels: the first and the last glyph's side bearings. Words in the monospace face
     * are four pixels wider at 120 DPI, values in the language's face eleven narrower. */
    require(reach >= expected - 3 && reach <= expected + 3,
            "the keys of the status row are drawn as words in the language's face and key names "
            "in the monospace face");
}

/* #1215: the count says what it counts ("ノート 2"), also while a filter is on ("ノート 0 / 2").
 * Where the unit does not fit, only the numbers are drawn. The narrow English cases follow the
 * same rule with the measured room, whichever way it falls at this DPI. */
static void header_count_units(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "count fixture");
    pump(window);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    /* #1217: the fixture registers the bundled fonts, so the monospace face is the bundled one. */
    wchar_t mono[LF_FACESIZE];
    require(ui_face_mono(mono, 11) == 10 && wcscmp(mono, L"IBM Plex Mono") == 0,
            "the bundled monospace face is registered and chosen, one size down");
    struct header_count line = {FOLIO_LANGUAGE_JA, L"カテゴリ", L"ノート", L"2"};
    require(expect_header_count(window, line, "the count carries its unit"),
            "the default drawer has room for the unit");
    word_font_parts(window);
    breadcrumb_word_font(fixture, window);
    status_keys_fonts(window);
    tag_strip_word_font(window);
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    pump(window);
    line.number = L"0 / 2";
    require(expect_header_count(window, line, "the filtered count carries its unit"),
            "the default drawer has room for the unit of a filtered count");
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    pump(window);
    folio_window_destroy(window);
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_READY,
            "English for the narrow count");
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "narrow count fixture");
    pump(window);
    struct accessible_request width = {.item = {.kind = ACCESSIBLE_SPLITTER},
                                       .action = ACCESSIBLE_ACTION_SET_RANGE,
                                       .range_value = 200};
    require(folio_window_accessible_action(window, &width) == S_OK, "minimum drawer width");
    pump(window);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    line = (struct header_count){FOLIO_LANGUAGE_EN, L"Categories", L"Notes", L"2"};
    (void)expect_header_count(window, line, "the narrow English count follows the room");
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    pump(window);
    line.number = L"0 / 2";
    (void)expect_header_count(window, line, "the narrow filtered English count follows the room");
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    pump(window);
    folio_window_destroy(window);
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "back to Japanese");
}

/* #1199: two tag conditions, nothing else. */
static void discovery_seed_tags(struct native_fixture *_Nonnull fixture)
{
    struct note_tag *red = nullptr, *blue = nullptr;
    struct note_tags *tags = nullptr;
    require(note_tag_create("red", 3, &red) == NOTE_TAG_READY &&
                note_tag_create("blue", 4, &blue) == NOTE_TAG_READY,
            "create two tag conditions");
    const struct note_tag *names[] = {red, blue};
    require(note_tags_create(names, 2, &tags) == NOTE_TAGS_READY &&
                folio_state_set_tag_filter(fixture->state, tags, TAG_MATCH_ANY) ==
                    FOLIO_STATE_READY,
            "the list owns two tag conditions");
    note_tags_destroy(tags);
    note_tag_destroy(red);
    note_tag_destroy(blue);
}

/* #1199: a part of the drawer's fixed band, in the drawer's client pixels. */
static RECT chip_bounds(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK && !view.offscreen &&
                !IsRectEmpty(&view.bounds),
            "a visible part of the filter band");
    MapWindowPoints(nullptr, folio_window_accessible_window(window, ACCESSIBLE_DRAWER),
                    (POINT *)&view.bounds, 2);
    return view.bounds;
}

/* A real pointer press and release in the middle of the part; the pointer then leaves. */
static void chip_click(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT bounds = chip_bounds(window, kind);
    LPARAM position =
        MAKELPARAM((bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    pump(window);
}

static COLORREF drawer_pixel(struct folio_window *_Nonnull window, int x, int y)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT client;
    GetClientRect(drawer, &client);
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(drawer, dc, PW_CLIENTONLY), "render the real drawer for the filter chips");
    COLORREF pixel = GetPixel(dc, x, y);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
    return pixel;
}

/* The chips with nothing lit: one row of the chip height, a 1px frame, no clear row. */
static void filter_chips_resting(struct folio_window *_Nonnull window, RECT star, RECT tags)
{
    int dpi = (int)GetDpiForWindow(folio_window_accessible_window(window, ACCESSIBLE_DRAWER));
    require(star.top == tags.top && star.right < tags.left &&
                star.bottom - star.top == MulDiv(21, dpi, 96) &&
                tags.bottom - tags.top == MulDiv(21, dpi, 96),
            "the favorites chip and the tag opener share one row of 21 DIP chips");
    require(drawer_pixel(window, star.left, star.top) !=
                drawer_pixel(window, star.left + 3, star.top + 3),
            "a resting chip has a frame around the drawer's ground");
    struct accessible_item clear = {.kind = ACCESSIBLE_FILTER_CLEAR};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &clear, &view) != S_OK || view.offscreen,
            "no clear row while nothing is filtered");
}

/* With nothing filtered there is no clear row to stop on: Tab goes from the chip row to the
 * new-category button. */
static void filter_tab_without_clear(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_FILTER));
    favorite_key(window, VK_TAB);
    struct accessible_item focus = {0};
    require(!folio_state_conditions_active(fixture->state) &&
                folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_FILTER_FAVORITES,
            "Tab from the search field stops on the chip row");
    favorite_key(window, VK_TAB);
    require(folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_NEW_CATEGORY,
            "with nothing filtered, the next Tab skips the absent clear row");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
}

/* Tab reaches the clear row; Space clears, and the focus does not stay on the vanished row. */
static void filter_clear_keyboard(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_FILTER));
    favorite_key(window, VK_TAB);
    favorite_key(window, VK_SPACE);
    favorite_key(window, VK_TAB);
    struct accessible_item focus = {0};
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_FAVORITES &&
                folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_FILTER_CLEAR,
            "Space lights the chip and Tab reaches the clear row");
    favorite_key(window, VK_SPACE);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
                folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
                focus.kind == ACCESSIBLE_FILTER_FAVORITES,
            "Space on the clear row clears and moves the focus to the favorites chip");
}

/* The clear row vanishes under the pointer and the "new category" button's hit area takes its
 * place. A second press at the same point (a double click) must not reach that button. */
static void filter_clear_twice(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    select_favorite_scope(window, true);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
    RECT clear = chip_bounds(window, ACCESSIBLE_FILTER_CLEAR);
    size_t categories = folio_state_category_count(fixture->state);
    LPARAM position = MAKELPARAM(clear.right - 3, clear.bottom - 3);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    for (int press = 0; press < 2; ++press)
    {
        SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
        SendMessageW(drawer, WM_LBUTTONUP, 0, position);
        pump(window);
    }
    RECT plus = chip_bounds(window, ACCESSIBLE_NEW_CATEGORY);
    POINT point = {clear.right - 3, clear.bottom - 3};
    require(PtInRect(&plus, point), "the pressed point is inside the new-category button now");
    require(
        folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
            folio_state_category_count(fixture->state) == categories &&
            !IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT)) &&
            GetCapture() != drawer,
        "a second press where the clear row was does not start a new category");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(clear.right - 4, clear.bottom - 3));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    pump(window);
}

/* The tooltip's rectangle of a part is the rectangle UIA reports for it. */
static void expect_tooltip_rect(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    wchar_t text[1024] = {0};
    TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                      .hwnd = drawer,
                      .uId = kind,
                      .lpszText = text};
    require(tooltip != nullptr && SendMessageW(tooltip, TTM_GETTOOLINFOW, 0, (LPARAM)&tool),
            "the filter part has a registered tooltip");
    RECT bounds = chip_bounds(window, kind);
    require(EqualRect(&tool.rect, &bounds), "the tooltip follows the part's rectangle");
}

/* #1247: one tag chip of the filter row, by its number, at the tag condition's revision. */
static struct accessible_item tag_chip(const struct native_fixture *_Nonnull fixture, size_t slot)
{
    return (struct accessible_item){.kind = ACCESSIBLE_FILTER_TAG,
                                    .slot = slot,
                                    .revision = folio_state_tag_filter_revision(fixture->state)};
}

/* The chip's rectangle in the drawer's client pixels and its name. */
static RECT tag_chip_view(struct folio_window *_Nonnull window,
                          const struct native_fixture *_Nonnull fixture, size_t slot,
                          struct accessible_view *_Nonnull view)
{
    struct accessible_item item = tag_chip(fixture, slot);
    *view = (struct accessible_view){0};
    require(folio_window_accessible_read(window, &item, view) == S_OK && !view->offscreen &&
                !IsRectEmpty(&view->bounds),
            "a placed tag chip answers");
    RECT bounds = view->bounds;
    MapWindowPoints(nullptr, folio_window_accessible_window(window, ACCESSIBLE_DRAWER),
                    (POINT *)&bounds, 2);
    return bounds;
}

static void filter_settle(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture)
{
    for (unsigned at = 0; at < 100 && folio_state_index_filter_pending(fixture->state); ++at)
        folio_window_continue_idle_work(window);
    InvalidateRect(folio_window_accessible_window(window, ACCESSIBLE_DRAWER), nullptr, FALSE);
    pump(window);
}

static size_t tag_condition_count(const struct native_fixture *_Nonnull fixture)
{
    const struct note_tags *tags = folio_state_tag_filter(fixture->state);
    return tags == nullptr ? 0 : note_tags_count(tags);
}

/* The tooltip of a tag chip: its own tool, the chip's rectangle, the sentence UIA names. */
static void expect_tag_tooltip(struct folio_window *_Nonnull window,
                               const struct native_fixture *_Nonnull fixture, size_t slot)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    wchar_t text[1024] = {0};
    TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                      .hwnd = drawer,
                      .uId = 0x200 + slot,
                      .lpszText = text};
    require(tooltip != nullptr && SendMessageW(tooltip, TTM_GETTOOLINFOW, 0, (LPARAM)&tool),
            "each tag chip has a registered tooltip");
    struct accessible_view view = {0};
    RECT bounds = tag_chip_view(window, fixture, slot, &view);
    require(EqualRect(&tool.rect, &bounds) && wcscmp(text, view.name) == 0,
            "the tag chip's tooltip follows its rectangle and says its name");
}

/* #1247: the row's children as UIA walks them: favorites, each tag chip, the opener. */
static void filter_tag_walk(struct folio_window *_Nonnull window,
                            const struct native_fixture *_Nonnull fixture)
{
    struct accessible_item at = {.kind = ACCESSIBLE_FILTER_FAVORITES};
    struct accessible_item next = {0};
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_NEXT, &next) ==
                    S_OK &&
                accessible_item_equal(next, tag_chip(fixture, 0)),
            "the favorites chip's next sibling is the first tag chip");
    at = next;
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_NEXT, &next) ==
                    S_OK &&
                accessible_item_equal(next, tag_chip(fixture, 1)),
            "a tag chip's next sibling is the next tag chip");
    struct accessible_item parent = {0};
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_PARENT, &parent) ==
                    S_OK &&
                parent.kind == ACCESSIBLE_FILTER_GROUP &&
                folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_FIRST,
                                                 &parent) == S_FALSE,
            "a tag chip is a child of the filter group and has no children");
    at = next;
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_NEXT, &next) ==
                    S_OK &&
                next.kind == ACCESSIBLE_TAG_FILTER,
            "the last tag chip's next sibling is the opener");
    at = next;
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_NEXT, &next) ==
                    S_FALSE &&
                folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_PREVIOUS,
                                                 &next) == S_OK &&
                accessible_item_equal(next, tag_chip(fixture, 1)),
            "the opener is last and steps back to the last tag chip");
    at = tag_chip(fixture, 0);
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_PREVIOUS, &next) ==
                    S_OK &&
                next.kind == ACCESSIBLE_FILTER_FAVORITES,
            "the first tag chip steps back to the favorites chip");
    at = (struct accessible_item){.kind = ACCESSIBLE_FILTER_GROUP};
    struct accessible_item edge = {0};
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_FIRST, &next) ==
                    S_OK &&
                next.kind == ACCESSIBLE_FILTER_FAVORITES &&
                folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_LAST, &edge) ==
                    S_OK &&
                edge.kind == ACCESSIBLE_TAG_FILTER,
            "the group's first child is the favorites chip and its last the opener");
    struct accessible_view view = {0};
    struct accessible_item stale = tag_chip(fixture, 0);
    stale.revision -= 1;
    struct accessible_item beyond = tag_chip(fixture, 2);
    struct accessible_item more = {.kind = ACCESSIBLE_FILTER_MORE};
    require(folio_window_accessible_read(window, &stale, &view) != S_OK &&
                folio_window_accessible_read(window, &beyond, &view) != S_OK &&
                (folio_window_accessible_read(window, &more, &view) != S_OK || view.offscreen),
            "a chip of an earlier condition, a number past the chips and an unused +N do not "
            "answer");
    struct accessible_request invoke = {.item = stale, .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &invoke) != S_OK &&
                tag_condition_count(fixture) == 2,
            "invoking a chip of an earlier condition removes nothing");
}

static bool filter_focus_is(struct folio_window *_Nonnull window, struct accessible_item expected)
{
    struct accessible_item focus = {0};
    return folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) == S_OK &&
           accessible_item_equal(focus, expected);
}

/* #1247: arrows walk the chips and wrap; Enter and Space remove the chip under the keyboard and
 * the focus stays on a chip, then on the favorites chip when no tag is left. */
static void filter_tag_keyboard(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_FILTER_FAVORITES},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK, "focus the favorites chip");
    pump(window);
    favorite_key(window, VK_RIGHT);
    require(filter_focus_is(window, tag_chip(fixture, 0)), "Right reaches the first tag chip");
    struct accessible_view view = {0};
    (void)tag_chip_view(window, fixture, 0, &view);
    require(view.focused && view.focusable && view.enabled,
            "the tag chip under the keyboard reports the focus");
    favorite_key(window, VK_RIGHT);
    require(filter_focus_is(window, tag_chip(fixture, 1)), "Right reaches the second tag chip");
    favorite_key(window, VK_RIGHT);
    require(filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_TAG_FILTER}),
            "Right reaches the opener");
    favorite_key(window, VK_RIGHT);
    require(filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_FAVORITES}),
            "Right wraps to the favorites chip");
    favorite_key(window, VK_LEFT);
    favorite_key(window, VK_LEFT);
    require(filter_focus_is(window, tag_chip(fixture, 1)), "Left wraps and walks back");
    favorite_key(window, VK_RETURN);
    filter_settle(window, fixture);
    struct accessible_view left = {0};
    (void)tag_chip_view(window, fixture, 0, &left);
    require(tag_condition_count(fixture) == 1 && wcsstr(left.name, L"red") != nullptr &&
                filter_focus_is(window, tag_chip(fixture, 0)),
            "Enter removes the tag under the keyboard and the focus moves to the chip that is "
            "left");
    favorite_key(window, VK_SPACE);
    filter_settle(window, fixture);
    require(
        tag_condition_count(fixture) == 0 && !folio_state_conditions_active(fixture->state) &&
            filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_FAVORITES}),
        "Space removes the last tag and the focus moves to the favorites chip");
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    struct accessible_request first = {.item = tag_chip(fixture, 0),
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &first) == S_OK &&
                filter_focus_is(window, tag_chip(fixture, 0)),
            "UIA focuses the first tag chip");
    favorite_key(window, VK_RETURN);
    filter_settle(window, fixture);
    (void)tag_chip_view(window, fixture, 0, &left);
    require(tag_condition_count(fixture) == 1 && wcsstr(left.name, L"blue") != nullptr &&
                left.focused && filter_focus_is(window, tag_chip(fixture, 0)),
            "Enter on the first chip removes it and the focus stays where the next chip came");
    favorite_key(window, VK_RETURN);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 0, "Enter removes the chip that came");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
}

/* The tooltip tool of a tag chip's number has no rectangle while no chip is placed there. */
static bool tag_tooltip_empty(struct folio_window *_Nonnull window, size_t slot)
{
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    wchar_t text[1024] = {0};
    TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                      .hwnd = folio_window_accessible_window(window, ACCESSIBLE_DRAWER),
                      .uId = 0x200 + slot,
                      .lpszText = text};
    return SendMessageW(tooltip, TTM_GETTOOLINFOW, 0, (LPARAM)&tool) && IsRectEmpty(&tool.rect);
}

/* #1247: the condition changes between the press and the release: the chip pressed is of the
 * earlier condition, so the release removes nothing. */
static void filter_tag_stale_press(struct folio_window *_Nonnull window,
                                   struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    struct accessible_view view = {0};
    RECT first = tag_chip_view(window, fixture, 0, &view);
    LPARAM position = MAKELPARAM(first.left + 4, (first.top + first.bottom) / 2);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    uint64_t pressed = folio_state_tag_filter_revision(fixture->state);
    discovery_seed_tags(fixture); /* the same two tags, a new revision */
    require(folio_state_tag_filter_revision(fixture->state) != pressed,
            "seeding the tags again is a new revision of the condition");
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 2 && GetCapture() != drawer,
            "a release after the condition changed removes nothing");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    filter_settle(window, fixture);
}

/* #1247: a press on the second chip removes the second tag, not the first. */
static void filter_tag_pointer_second(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    struct accessible_view view = {0};
    RECT second = tag_chip_view(window, fixture, 1, &view);
    require(wcsstr(view.name, L"blue") != nullptr, "the second chip is the second tag");
    LPARAM position =
        MAKELPARAM((second.left + second.right) / 2, (second.top + second.bottom) / 2);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    filter_settle(window, fixture);
    (void)tag_chip_view(window, fixture, 0, &view);
    require(tag_condition_count(fixture) == 1 && wcsstr(view.name, L"red") != nullptr,
            "a press on the second chip removes the second tag and leaves the first");
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 0, "the clear row removes the tag that is left");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    pump(window);
}

/* #1247: a press anywhere on a tag chip removes that tag. The next chip slides under the
 * pointer, so a second press at the same point (a double click) does not remove it too. */
static void filter_tag_pointer(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    struct accessible_view view = {0};
    RECT first = tag_chip_view(window, fixture, 0, &view);
    LPARAM position = MAKELPARAM(first.right - 6, (first.top + first.bottom) / 2);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    for (int press = 0; press < 2; ++press)
    {
        SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
        SendMessageW(drawer, WM_LBUTTONUP, 0, position);
        filter_settle(window, fixture);
    }
    (void)tag_chip_view(window, fixture, 0, &view);
    require(tag_condition_count(fixture) == 1 && wcsstr(view.name, L"blue") != nullptr &&
                GetCapture() != drawer,
            "a press on the chip's mark removes that tag; a second press at the same point "
            "removes nothing");
    require(tag_tooltip_empty(window, 1) && !tag_tooltip_empty(window, 0),
            "the removed chip's tooltip has no rectangle; the chip that is left keeps its own");
    RECT second = tag_chip_view(window, fixture, 0, &view);
    POINT same = {first.right - 6, (first.top + first.bottom) / 2};
    require(PtInRect(&second, same), "the chip that is left came under the pressed point");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(same.x - 1, same.y));
    SendMessageW(drawer, WM_MOUSEMOVE, 0, position);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, position);
    SendMessageW(drawer, WM_LBUTTONUP, 0, position);
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 0 && !folio_state_conditions_active(fixture->state) &&
                tag_tooltip_empty(window, 0),
            "after the pointer moved and came back, a press at the same point removes the last "
            "tag");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
}

/* #1247: twelve tag conditions, "label-00" to "label-11". */
constexpr size_t fold_total = 12;

static void filter_seed_fold(struct native_fixture *_Nonnull fixture)
{
    struct note_tag *made[fold_total] = {nullptr};
    const struct note_tag *names[fold_total] = {nullptr};
    for (size_t at = 0; at < fold_total; ++at)
    {
        char name[] = "label-00";
        name[6] = (char)('0' + at / 10);
        name[7] = (char)('0' + at % 10);
        require(note_tag_create(name, 8, &made[at]) == NOTE_TAG_READY, "create a folded tag");
        names[at] = made[at];
    }
    struct note_tags *tags = nullptr;
    require(note_tags_create(names, fold_total, &tags) == NOTE_TAGS_READY &&
                folio_state_set_tag_filter(fixture->state, tags, TAG_MATCH_ANY) ==
                    FOLIO_STATE_READY,
            "the list owns twelve tag conditions");
    note_tags_destroy(tags);
    for (size_t at = 0; at < fold_total; ++at)
        note_tag_destroy(made[at]);
}

/* How many tag chips answer, counting from the first. */
static size_t filter_tags_placed(struct folio_window *_Nonnull window,
                                 const struct native_fixture *_Nonnull fixture)
{
    size_t shown = 0;
    struct accessible_view view = {0};
    struct accessible_item item = tag_chip(fixture, shown);
    while (shown < fold_total && folio_window_accessible_read(window, &item, &view) == S_OK)
    {
        shown += 1;
        item = tag_chip(fixture, shown);
    }
    return shown;
}

/* #1247: a tag with a long name is cut to a width that still shares a row with the opener, so
 * two long tags keep to two rows and none is folded. */
static void filter_tag_long(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture)
{
    static const char first[] = "a-very-long-tag-name-that-cannot-fit-the-row-one";
    static const char second[] = "a-very-long-tag-name-that-cannot-fit-the-row-two";
    struct note_tag *made[2] = {nullptr, nullptr};
    require(note_tag_create(first, sizeof first - 1, &made[0]) == NOTE_TAG_READY &&
                note_tag_create(second, sizeof second - 1, &made[1]) == NOTE_TAG_READY,
            "create two long tags");
    const struct note_tag *names[] = {made[0], made[1]};
    struct note_tags *tags = nullptr;
    require(note_tags_create(names, 2, &tags) == NOTE_TAGS_READY &&
                folio_state_set_tag_filter(fixture->state, tags, TAG_MATCH_ANY) ==
                    FOLIO_STATE_READY,
            "the list owns two long tag conditions");
    note_tags_destroy(tags);
    note_tag_destroy(made[0]);
    note_tag_destroy(made[1]);
    filter_settle(window, fixture);
    int dpi = (int)GetDpiForWindow(folio_window_accessible_window(window, ACCESSIBLE_DRAWER));
    RECT group = chip_bounds(window, ACCESSIBLE_FILTER_GROUP);
    RECT opener = chip_bounds(window, ACCESSIBLE_TAG_FILTER);
    struct accessible_view one = {0}, two = {0}, more = {0};
    RECT left = tag_chip_view(window, fixture, 0, &one);
    RECT right = tag_chip_view(window, fixture, 1, &two);
    struct accessible_item folded = {.kind = ACCESSIBLE_FILTER_MORE};
    require(filter_tags_placed(window, fixture) == 2 &&
                (folio_window_accessible_read(window, &folded, &more) != S_OK || more.offscreen) &&
                group.bottom - group.top == 2 * MulDiv(21, dpi, 96) + MulDiv(5, dpi, 96) &&
                right.top == opener.top && right.right < opener.left &&
                opener.right <= group.right && left.right <= group.right,
            "two long tags are cut, not folded: the second shares its row with the opener");
    require(wcsstr(one.name, L"row-one") != nullptr && wcsstr(two.name, L"row-two") != nullptr,
            "a cut chip still names its whole tag");
    printf("filter-tag-long first=%ld second=%ld opener=%ld..%ld\n", left.right - left.left,
           right.right - right.left, opener.left, opener.right);
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 0, "the clear row removes the long tags");
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_DRAWER), WM_MOUSELEAVE, 0, 0);
    pump(window);
}

/* +N names how many tags it folds; its help and its tooltip list them. */
static void filter_fold_names(struct folio_window *_Nonnull window, size_t shown, RECT more)
{
    struct accessible_item folded = {.kind = ACCESSIBLE_FILTER_MORE};
    struct accessible_view view = {0};
    wchar_t number[8] = {0};
    require(swprintf(number, 8, L"%zu", fold_total - shown) > 0, "the folded count fits");
    require(folio_window_accessible_read(window, &folded, &view) == S_OK &&
                wcsstr(view.name, number) != nullptr && wcsstr(view.help, L"label-11") != nullptr &&
                wcsstr(view.help, L"label-00") == nullptr,
            "+N names how many tags it folds and lists the folded ones");
    HWND tooltip = status_tooltip(folio_window_accessible_window(window, ACCESSIBLE_WINDOW));
    wchar_t text[1024] = {0};
    TOOLINFOW tool = {.cbSize = offsetof(TOOLINFOW, lParam) + sizeof(LPARAM),
                      .hwnd = folio_window_accessible_window(window, ACCESSIBLE_DRAWER),
                      .uId = ACCESSIBLE_FILTER_MORE,
                      .lpszText = text};
    require(SendMessageW(tooltip, TTM_GETTOOLINFOW, 0, (LPARAM)&tool) &&
                EqualRect(&tool.rect, &more) && wcscmp(text, view.help) == 0,
            "the +N tooltip lists the folded tags over the chip");
}

/* +N opens the tag surface; Esc closes it; the clear row removes every tag. */
static void filter_fold_open(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    struct accessible_request invoke = {.item = {.kind = ACCESSIBLE_FILTER_MORE},
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &invoke) == S_OK, "invoke +N");
    pump(window);
    require(IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_SURFACE)),
            "+N opens the tag surface");
    favorite_key(window, VK_ESCAPE);
    require(!IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_SURFACE)),
            "Esc closes the tag surface");
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 0 && !folio_state_conditions_active(fixture->state),
            "the clear row removes the twelve tags");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
}

/* #1247: twelve tags do not fit two rows: the last ones fold into "+N", which names how many it
 * folds, lists them as its help and tooltip, and opens the tag surface. */
static void filter_tag_fold(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture)
{
    filter_seed_fold(fixture);
    filter_settle(window, fixture);
    int dpi = (int)GetDpiForWindow(folio_window_accessible_window(window, ACCESSIBLE_DRAWER));
    RECT group = chip_bounds(window, ACCESSIBLE_FILTER_GROUP);
    RECT more = chip_bounds(window, ACCESSIBLE_FILTER_MORE);
    RECT opener = chip_bounds(window, ACCESSIBLE_TAG_FILTER);
    size_t shown = filter_tags_placed(window, fixture);
    require(shown > 0 && shown < fold_total, "some of the twelve tags are placed, some folded");
    struct accessible_view view = {0};
    RECT last = tag_chip_view(window, fixture, shown - 1, &view);
    require(group.bottom - group.top == 2 * MulDiv(21, dpi, 96) + MulDiv(5, dpi, 96) &&
                more.top == opener.top && more.right < opener.left && opener.right <= group.right &&
                (last.top < more.top || last.right < more.left),
            "the chips keep to two rows: the placed tags, then +N, then the opener");
    filter_fold_names(window, shown, more);
    struct accessible_item at = tag_chip(fixture, shown - 1);
    struct accessible_item next = {0};
    require(folio_window_accessible_navigate(window, &at, ACCESSIBLE_NAVIGATION_NEXT, &next) ==
                    S_OK &&
                next.kind == ACCESSIBLE_FILTER_MORE,
            "the last placed tag chip's next sibling is +N");
    printf("filter-tag-fold shown=%zu hidden=%zu more=%ld..%ld opener=%ld..%ld\n", shown,
           fold_total - shown, more.left, more.right, opener.left, opener.right);
    filter_fold_open(window, fixture);
}

/* Two tag conditions put two lit tag chips between the favorites chip and the opener, which
 * stays unlit, and show the clear row. Lighting the favorites chip then keeps the band's height.
 * Its Japanese label is full-width, so the bold face does not widen the chip and the chips stay
 * (filter_chip_shift covers a label that widens). Clearing removes both conditions. */
static void filter_chip_tags(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT resting = chip_bounds(window, ACCESSIBLE_TAG_FILTER);
    COLORREF frame = drawer_pixel(window, resting.left, resting.top);
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    RECT lit = chip_bounds(window, ACCESSIBLE_TAG_FILTER);
    RECT star = chip_bounds(window, ACCESSIBLE_FILTER_FAVORITES);
    struct accessible_item opener = {.kind = ACCESSIBLE_TAG_FILTER};
    struct accessible_view view = {0};
    struct accessible_view red = {0}, blue = {0};
    RECT first = tag_chip_view(window, fixture, 0, &red);
    RECT second = tag_chip_view(window, fixture, 1, &blue);
    require(folio_window_accessible_read(window, &opener, &view) == S_OK &&
                wcsstr(view.name, L"2") != nullptr &&
                lit.right - lit.left == resting.right - resting.left &&
                drawer_pixel(window, lit.left, lit.top) == frame,
            "two tag conditions keep the opener's size and frame; its name counts them");
    require(wcsstr(red.name, L"red") != nullptr && wcsstr(blue.name, L"blue") != nullptr &&
                first.top == star.top && first.left > star.right &&
                first.bottom - first.top == star.bottom - star.top &&
                (second.top > first.top || second.left > first.right) &&
                (lit.top > second.top || lit.left > second.right) &&
                drawer_pixel(window, first.left, first.top) != frame,
            "each tag condition is a lit chip after the favorites chip, before the opener");
    expect_tag_tooltip(window, fixture, 0);
    expect_tag_tooltip(window, fixture, 1);
    filter_tag_walk(window, fixture);
    int tree = chip_bounds(window, ACCESSIBLE_TREE).top;
    expect_tooltip_rect(window, ACCESSIBLE_TAG_FILTER);
    chip_click(window, ACCESSIBLE_FILTER_FAVORITES);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
    require(chip_bounds(window, ACCESSIBLE_TREE).top == tree &&
                chip_bounds(window, ACCESSIBLE_TAG_FILTER).left == lit.left,
            "lighting favorites keeps the band's height and a full-width label keeps the opener");
    expect_tooltip_rect(window, ACCESSIBLE_TAG_FILTER);
    expect_tooltip_rect(window, ACCESSIBLE_FILTER_FAVORITES);
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    for (unsigned at = 0; at < 100 && folio_state_index_filter_pending(fixture->state); ++at)
        folio_window_continue_idle_work(window);
    pump(window);
    const struct note_tags *tags = folio_state_tag_filter(fixture->state);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
                (tags == nullptr || note_tags_count(tags) == 0) &&
                !folio_state_conditions_active(fixture->state),
            "one press on the clear row removes the favorites scope and the tags");
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    pump(window);
}

/* #1242: an English label is wider in the lit chip's bold face. With a tag condition already in
 * effect the clear row is there, so lighting the favorites chip moves the opener while the band
 * keeps its height: the tooltip rectangles must follow the chips, not the height. */
static void filter_chip_shift(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_EN) == FOLIO_STATE_READY,
            "English for the widening chip");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "widening chip fixture");
    pump(window);
    discovery_seed_tags(fixture);
    for (unsigned at = 0; at < 100 && folio_state_index_filter_pending(fixture->state); ++at)
        folio_window_continue_idle_work(window);
    InvalidateRect(folio_window_accessible_window(window, ACCESSIBLE_DRAWER), nullptr, FALSE);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
    RECT star = chip_bounds(window, ACCESSIBLE_FILTER_FAVORITES);
    struct accessible_view view = {0};
    RECT opener = tag_chip_view(window, fixture, 0, &view);
    int tree = chip_bounds(window, ACCESSIBLE_TREE).top;
    expect_tag_tooltip(window, fixture, 0);
    chip_click(window, ACCESSIBLE_FILTER_FAVORITES);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
    RECT lit = chip_bounds(window, ACCESSIBLE_FILTER_FAVORITES);
    RECT moved = tag_chip_view(window, fixture, 0, &view);
    require(lit.right - lit.left > star.right - star.left && moved.left > opener.left &&
                moved.top == opener.top && chip_bounds(window, ACCESSIBLE_TREE).top == tree,
            "a bold English label widens the lit chip and moves the tag chip on the same row");
    expect_tag_tooltip(window, fixture, 0);
    expect_tag_tooltip(window, fixture, 1);
    expect_tooltip_rect(window, ACCESSIBLE_TAG_FILTER);
    expect_tooltip_rect(window, ACCESSIBLE_FILTER_FAVORITES);
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    for (unsigned at = 0; at < 100 && folio_state_index_filter_pending(fixture->state); ++at)
        folio_window_continue_idle_work(window);
    pump(window);
    require(!folio_state_conditions_active(fixture->state), "the widening chip fixture is cleared");
    printf("filter-chip-shift star=%ld lit=%ld opener=%ld moved=%ld\n", star.right - star.left,
           lit.right - lit.left, opener.left, moved.left);
    folio_window_destroy(window);
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "back to Japanese after the widening chip");
}

/* #1249: the pair "all / any" of the tag condition, on the clear row while two or more tags
 * filter the list. */
static bool match_part_absent(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind};
    struct accessible_view view = {0};
    return folio_window_accessible_read(window, &item, &view) != S_OK || view.offscreen;
}

/* Every pixel of a part of the drawer is the drawer's ground: nothing is drawn there. */
static bool drawer_region_plain(struct folio_window *_Nonnull window, RECT region)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    RECT client;
    GetClientRect(drawer, &client);
    HDC source = GetDC(drawer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(drawer, dc, PW_CLIENTONLY), "render the drawer for a plain region");
    COLORREF ground = GetPixel(dc, region.left, region.top);
    bool plain = true;
    for (int y = region.top; y < region.bottom && plain; ++y)
        for (int x = region.left; x < region.right && plain; ++x)
            plain = GetPixel(dc, x, y) == ground;
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(drawer, source);
    return plain;
}

static bool match_selected(struct folio_window *_Nonnull window, enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind};
    struct accessible_view view = {0};
    return folio_window_accessible_read(window, &item, &view) == S_OK && view.selected;
}

/* Two tags (any): the pair sits left of the clear row's button, "any" is selected and carries
 * the line under it, UIA reports the same selection, the tooltips follow. */
static void match_pair_resting(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    RECT group = chip_bounds(window, ACCESSIBLE_FILTER_GROUP);
    RECT all = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ALL);
    RECT any = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ANY);
    RECT clear = chip_bounds(window, ACCESSIBLE_FILTER_CLEAR);
    RECT pair = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_GROUP);
    require(all.left == group.left && all.top >= group.bottom && any.left > all.right &&
                any.top == all.top && clear.top == all.top && clear.left > any.right &&
                pair.left == all.left && pair.right == any.right,
            "the pair sits at the left of the clear row, the clear button at its right");
    struct accessible_item item = {.kind = ACCESSIBLE_FILTER_MATCH_GROUP};
    struct accessible_item selected = {0};
    require(folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ANY &&
                match_selected(window, ACCESSIBLE_FILTER_MATCH_ANY) &&
                !match_selected(window, ACCESSIBLE_FILTER_MATCH_ALL) &&
                folio_window_accessible_selected(window, &item, &selected) == S_OK &&
                selected.kind == ACCESSIBLE_FILTER_MATCH_ANY,
            "the selected radio is the condition's mode");
    COLORREF ground = drawer_pixel(window, all.left + 1, all.bottom - 1);
    require(drawer_pixel(window, any.left + 1, any.bottom - 1) != ground &&
                drawer_pixel(window, any.left + 1, any.top) == ground,
            "the selected one carries a line under it and no fill");
    expect_tooltip_rect(window, ACCESSIBLE_FILTER_MATCH_ALL);
    expect_tooltip_rect(window, ACCESSIBLE_FILTER_MATCH_ANY);
}

/* A press selects; the pair does not move; UIA Select selects; selecting the selected one
 * leaves the condition's revision alone. */
static void match_pair_select(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    RECT all = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ALL);
    RECT any = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ANY);
    chip_click(window, ACCESSIBLE_FILTER_MATCH_ALL);
    filter_settle(window, fixture);
    RECT all_after = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ALL);
    RECT any_after = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ANY);
    require(folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ALL &&
                tag_condition_count(fixture) == 2 &&
                match_selected(window, ACCESSIBLE_FILTER_MATCH_ALL) &&
                EqualRect(&all, &all_after) && EqualRect(&any, &any_after),
            "a press selects the other mode, keeps the tags and does not move the pair");
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    LPARAM at = MAKELPARAM((all.left + all.right) / 2, (all.top + all.bottom) / 2);
    uint64_t before = folio_state_tag_filter_revision(fixture->state);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, at);
    SendMessageW(drawer, WM_LBUTTONDOWN, MK_LBUTTON, at);
    bool captured = GetCapture() == drawer;
    SendMessageW(drawer, WM_LBUTTONUP, 0, at);
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    filter_settle(window, fixture);
    require(!captured && folio_state_tag_filter_revision(fixture->state) == before &&
                folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ALL,
            "a press on the selected mode is not a press and changes nothing");
    struct accessible_request select = {.item = {.kind = ACCESSIBLE_FILTER_MATCH_ANY},
                                        .action = ACCESSIBLE_ACTION_SELECT};
    require(folio_window_accessible_action(window, &select) == S_OK, "UIA Select on any");
    filter_settle(window, fixture);
    uint64_t revision = folio_state_tag_filter_revision(fixture->state);
    require(folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ANY &&
                folio_window_accessible_action(window, &select) == S_OK &&
                folio_state_tag_filter_revision(fixture->state) == revision,
            "UIA Select selects; selecting the selected one changes nothing");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
}

/* Tab stops once on the pair, on the selected one; arrows move only the focus; Space selects;
 * the next Tab reaches the clear row. */
static void match_pair_keyboard(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_FILTER));
    favorite_key(window, VK_TAB);
    favorite_key(window, VK_TAB);
    require(filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_MATCH_ANY}),
            "Tab from the chip row enters the pair on the selected mode");
    RECT any = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ANY);
    int dpi = (int)GetDpiForWindow(folio_window_accessible_window(window, ACCESSIBLE_DRAWER));
    int ring = MulDiv(2, dpi, 96);
    /* Just inside the ring's left edge and above its bottom edge: no glyph is there. */
    COLORREF edge = drawer_pixel(window, any.left + ring, any.bottom - 1);
    COLORREF line = drawer_pixel(window, any.left + ring, any.bottom - ring - 1);
    favorite_key(window, VK_LEFT);
    RECT all = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ALL);
    require(drawer_pixel(window, all.left + ring, all.bottom - 1) == edge &&
                drawer_pixel(window, all.left + ring, all.bottom - ring - 1) != line &&
                drawer_pixel(window, any.left + ring, any.bottom - 1) == line,
            "under the focus ring the selected mode still carries its line, inside the ring");
    require(
        filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_MATCH_ALL}) &&
            folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ANY,
        "an arrow moves only the focus within the pair");
    favorite_key(window, VK_SPACE);
    filter_settle(window, fixture);
    require(
        folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ALL &&
            filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_MATCH_ALL}),
        "Space selects the focused mode and the focus stays");
    favorite_key(window, VK_TAB);
    require(filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_CLEAR}),
            "the next Tab reaches the clear row");
}

/* With one tag left the pair is gone, and a focus that was on it moves to the favorites chip. */
static void match_pair_vanishes(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    struct accessible_request focus = {.item = {.kind = ACCESSIBLE_FILTER_MATCH_ALL},
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK, "focus the pair");
    pump(window);
    struct accessible_request remove = {.item = tag_chip(fixture, 1),
                                        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &remove) == S_OK, "remove the second tag");
    filter_settle(window, fixture);
    require(
        tag_condition_count(fixture) == 1 &&
            match_part_absent(window, ACCESSIBLE_FILTER_MATCH_GROUP) &&
            match_part_absent(window, ACCESSIBLE_FILTER_MATCH_ALL) &&
            match_part_absent(window, ACCESSIBLE_FILTER_MATCH_ANY) &&
            !match_part_absent(window, ACCESSIBLE_FILTER_CLEAR) &&
            filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_FAVORITES}),
        "with one tag the pair is gone and its focus moves to the favorites chip");
    struct accessible_item pair = {.kind = ACCESSIBLE_FILTER_MATCH_GROUP};
    struct accessible_item chosen = {0};
    RECT clear = chip_bounds(window, ACCESSIBLE_FILTER_CLEAR);
    RECT left = chip_bounds(window, ACCESSIBLE_FILTER_GROUP);
    require(drawer_region_plain(window, (RECT){left.left, clear.top, clear.left, clear.bottom}) &&
                folio_window_accessible_selected(window, &pair, &chosen) != S_OK,
            "with one tag nothing is drawn left of the clear button and no mode is reported");
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_FILTER));
    favorite_key(window, VK_TAB);
    favorite_key(window, VK_TAB);
    require(filter_focus_is(window, (struct accessible_item){.kind = ACCESSIBLE_FILTER_CLEAR}),
            "with one tag, Tab goes from the chip row to the clear row");
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    filter_settle(window, fixture);
    require(tag_condition_count(fixture) == 0 &&
                folio_state_tag_match_mode(fixture->state) == TAG_MATCH_ALL,
            "the clear row removes the tag and the mode goes back to all");
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    SendMessageW(drawer, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
}

static void filter_match_pair(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture)
{
    require(match_part_absent(window, ACCESSIBLE_FILTER_MATCH_GROUP) &&
                match_part_absent(window, ACCESSIBLE_FILTER_MATCH_ALL),
            "no pair while no tag filters the list");
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    match_pair_resting(window, fixture);
    match_pair_select(window, fixture);
    match_pair_keyboard(window, fixture);
    match_pair_vanishes(window, fixture);
}

/* #1249: at the narrowest drawer, in each language, the pair stays inside the row; the clear
 * button is on the same row when it fits and on the next one when it does not. */
static void filter_match_narrow(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    for (size_t at = 0; at < sizeof languages / sizeof languages[0]; ++at)
    {
        require(folio_state_set_language(fixture->state, languages[at]) == FOLIO_STATE_READY,
                "language for the narrow pair");
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "narrow pair fixture");
        pump(window);
        struct accessible_request width = {.item = {.kind = ACCESSIBLE_SPLITTER},
                                           .action = ACCESSIBLE_ACTION_SET_RANGE,
                                           .range_value = 200};
        require(folio_window_accessible_action(window, &width) == S_OK, "minimum drawer width");
        discovery_seed_tags(fixture);
        filter_settle(window, fixture);
        RECT group = chip_bounds(window, ACCESSIBLE_FILTER_GROUP);
        RECT any = chip_bounds(window, ACCESSIBLE_FILTER_MATCH_ANY);
        RECT clear = chip_bounds(window, ACCESSIBLE_FILTER_CLEAR);
        int gap = MulDiv(
            8, (int)GetDpiForWindow(folio_window_accessible_window(window, ACCESSIBLE_DRAWER)), 96);
        bool beside = clear.top == any.top && clear.left >= any.right + gap;
        bool below = clear.top >= any.bottom;
        require(any.right <= group.right && clear.right <= group.right && (beside || below),
                "the pair and the clear button stay inside the narrow row without overlapping");
        printf("filter-match-narrow language=%u pair=%ld clear=%ld..%ld %s\n",
               (unsigned)languages[at], any.right - group.left, clear.left - group.left,
               clear.right - group.left, beside ? "beside" : "below");
        int tree = chip_bounds(window, ACCESSIBLE_TREE).top;
        require(tree >= clear.bottom, "the tree starts below the clear button, on either row");
        chip_click(window, ACCESSIBLE_FILTER_CLEAR);
        filter_settle(window, fixture);
        require(!folio_state_conditions_active(fixture->state),
                "a press on the clear button clears, on either row");
        folio_window_destroy(window);
    }
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "back to Japanese after the narrow pair");
}

/* #1199 / ADR0122: the two-way scope choice and the tag button are one row of chips. A lit chip
 * changes its frame, not its fill; the clear row appears below while a condition
 * is in effect and moves the tree down; pointer, keyboard and UIA end in the same state. */
static void filter_chip_row(struct folio_window *_Nonnull window,
                            struct native_fixture *_Nonnull fixture)
{
    HWND drawer = folio_window_accessible_window(window, ACCESSIBLE_DRAWER);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    SendMessageW(drawer, WM_MOUSELEAVE, 0, 0);
    pump(window);
    RECT star = chip_bounds(window, ACCESSIBLE_FILTER_FAVORITES);
    RECT tags = chip_bounds(window, ACCESSIBLE_TAG_FILTER);
    int tree = chip_bounds(window, ACCESSIBLE_TREE).top;
    filter_chips_resting(window, star, tags);
    COLORREF resting = drawer_pixel(window, star.left, star.top);
    COLORREF ground = drawer_pixel(window, star.left + 3, star.top + 3);
    chip_click(window, ACCESSIBLE_FILTER_FAVORITES);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
    RECT lit = chip_bounds(window, ACCESSIBLE_FILTER_FAVORITES);
    require(
        folio_state_note_scope(fixture->state) == NOTE_SCOPE_FAVORITES &&
            lit.right - lit.left >= star.right - star.left &&
            chip_bounds(window, ACCESSIBLE_TAG_FILTER).top == lit.top,
        "a press lights the chip: its bold label never narrows it and the opener stays on the row");
    require(drawer_pixel(window, lit.left, lit.top) != resting &&
                drawer_pixel(window, lit.left + 3, lit.top + 3) == ground,
            "a lit chip changes the colour of its frame and is not filled");
    RECT clear = chip_bounds(window, ACCESSIBLE_FILTER_CLEAR);
    int lowered = chip_bounds(window, ACCESSIBLE_TREE).top;
    require(clear.top >= lit.bottom && clear.bottom <= lowered && lowered > tree,
            "the clear row sits between the chips and the tree, which moves down");
    favorite_tooltips(window);
    chip_click(window, ACCESSIBLE_FILTER_CLEAR);
    struct accessible_item focus = {0};
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
                chip_bounds(window, ACCESSIBLE_TREE).top == tree &&
                (folio_window_accessible_focus(window, ACCESSIBLE_DRAWER, &focus) != S_OK ||
                 focus.kind != ACCESSIBLE_FILTER_CLEAR),
            "a press on the clear row clears, moves the tree back and leaves no focus behind");
    favorite_tooltips(window);
    select_favorite_scope(window, true);
    struct accessible_request request = {.item = {.kind = ACCESSIBLE_FILTER_CLEAR},
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK &&
                folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL,
            "UIA Invoke on the clear row clears");
    pump(window);
    filter_tab_without_clear(window, fixture);
    filter_clear_keyboard(window, fixture);
    filter_clear_twice(window, fixture);
    filter_chip_tags(window, fixture);
    discovery_seed_tags(fixture);
    filter_settle(window, fixture);
    filter_tag_keyboard(window, fixture);
    filter_tag_stale_press(window, fixture);
    filter_tag_pointer_second(window, fixture);
    filter_tag_pointer(window, fixture);
    filter_tag_fold(window, fixture);
    filter_tag_long(window, fixture);
    filter_match_pair(window, fixture);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    pump(window);
    printf("filter-chips star=%ld lit=%ld opener=%ld tree=%d lowered=%d\n", star.right - star.left,
           lit.right - lit.left, tags.right - tags.left, tree, lowered);
}

/* #1246: the search field's clear mark empties the term, so it is there only while the field
 * has text. The favorites scope alone used to draw it over the empty field's shortcut hint.
 * The field paints the mark straight to its window DC after WM_PAINT, so PrintWindow does not
 * see it: the pixels are read from the window. */
constexpr int filter_mark_limit = 64;

struct filter_mark
{
    COLORREF pixels[filter_mark_limit * filter_mark_limit];
    int size;
    int strokes; /* pixels that differ from the field's ground while a short term is shown */
};

static RECT filter_mark_rect(HWND input)
{
    RECT client;
    GetClientRect(input, &client);
    int size = MulDiv(20, (int)GetDpiForWindow(input), 96);
    size = size < client.bottom ? size : client.bottom;
    int middle = client.bottom / 2;
    return (RECT){client.right - size, middle - size / 2, client.right, middle - size / 2 + size};
}

/* The reference: with a one-letter term nothing but the mark is inside its box. */
static void filter_mark_learn(HWND input, struct filter_mark *_Nonnull mark)
{
    RECT box = filter_mark_rect(input);
    mark->size = box.right - box.left;
    require(mark->size > 0 && mark->size <= filter_mark_limit, "the clear mark's box fits");
    HDC dc = GetDC(input);
    COLORREF ground = GetPixel(dc, box.left, box.top);
    mark->strokes = 0;
    for (int y = 0; y < mark->size; ++y)
        for (int x = 0; x < mark->size; ++x)
        {
            COLORREF pixel = GetPixel(dc, box.left + x, box.top + y);
            mark->pixels[y * filter_mark_limit + x] = pixel;
            mark->strokes += pixel != ground && pixel != CLR_INVALID;
        }
    ReleaseDC(input, dc);
    for (int at = 0; at < mark->size * mark->size; ++at)
        if (mark->pixels[(at / mark->size) * filter_mark_limit + at % mark->size] == ground)
            mark->pixels[(at / mark->size) * filter_mark_limit + at % mark->size] = CLR_INVALID;
}

/* How many of the learned stroke pixels are on the window now. */
static int filter_mark_shown(HWND input, const struct filter_mark *_Nonnull mark)
{
    RECT box = filter_mark_rect(input);
    HDC dc = GetDC(input);
    /* A window whose pixels cannot be read would pass every "the mark is gone" check. */
    require(GetPixel(dc, box.left, box.top) != CLR_INVALID, "the field's pixels can be read");
    int shown = 0;
    for (int y = 0; y < mark->size; ++y)
        for (int x = 0; x < mark->size; ++x)
        {
            COLORREF stroke = mark->pixels[y * filter_mark_limit + x];
            shown += stroke != CLR_INVALID && GetPixel(dc, box.left + x, box.top + y) == stroke;
        }
    ReleaseDC(input, dc);
    return shown;
}

static void filter_mark_settle(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    pump(window);
    for (unsigned at = 0; at < 100 && folio_state_index_filter_pending(fixture->state); ++at)
        folio_window_continue_idle_work(window);
    pump(window);
    UpdateWindow(folio_window_accessible_window(window, ACCESSIBLE_FILTER));
}

/* Typed input: the EDIT repaints only its text for a keystroke, so the field itself has to
 * erase and repaint when the mark comes and goes. */
static void filter_mark_type(struct folio_window *_Nonnull window,
                             struct native_fixture *_Nonnull fixture, wchar_t unit)
{
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_FILTER), WM_CHAR, unit, 0);
    filter_mark_settle(window, fixture);
}

/* #1267: the EDIT repaints its own line for every keystroke, over the mark. While a term is in
 * the field the mark is there after each one: a second and a third character, and a deletion
 * that leaves characters. The field ends with the one character it started with. */
static void filter_mark_stays(struct folio_window *_Nonnull window,
                              struct native_fixture *_Nonnull fixture,
                              const struct filter_mark *_Nonnull mark)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    const wchar_t typed[] = {L'b', L'c', L'\b', L'\b'};
    for (size_t at = 0; at < sizeof typed / sizeof typed[0]; ++at)
    {
        filter_mark_type(window, fixture, typed[at]);
        int shown = filter_mark_shown(input, mark);
        printf("filter-mark-stays key=%zu length=%d shown=%d of %d\n", at,
               GetWindowTextLengthW(input), shown, mark->strokes);
        require(GetWindowTextLengthW(input) >= 1 && shown == mark->strokes,
                "the clear mark stays while the term changes and is not empty");
    }
    require(GetWindowTextLengthW(input) == 1, "the term is back to its first character");
}

static void filter_mark_needs_a_term(struct native_fixture *_Nonnull fixture)
{
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "filter mark fixture");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_FILTER);
    SetFocus(input);
    filter_mark_settle(window, fixture);
    static struct filter_mark mark;
    filter_mark_type(window, fixture, L'a');
    filter_mark_learn(input, &mark);
    require(GetWindowTextLengthW(input) == 1 && mark.strokes > 8,
            "a typed term shows the clear mark");
    filter_mark_stays(window, fixture, &mark);
    filter_mark_type(window, fixture, L'\b');
    int bare = filter_mark_shown(input, &mark);
    require(GetWindowTextLengthW(input) == 0 && 2 * bare < mark.strokes,
            "deleting the typed term erases the mark");
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    filter_mark_settle(window, fixture);
    int scoped = filter_mark_shown(input, &mark);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_FAVORITES &&
                folio_state_filtering(fixture->state) && 2 * scoped < mark.strokes,
            "the favorites scope alone leaves the empty search field without a clear mark");
    filter_mark_type(window, fixture, L'a');
    require(filter_mark_shown(input, &mark) == mark.strokes,
            "a term typed under the favorites scope shows the whole mark");
    filter_mark_type(window, fixture, L'\b');
    require(folio_state_filtering(fixture->state) &&
                2 * filter_mark_shown(input, &mark) < mark.strokes,
            "deleting the term erases the mark while the favorites scope stays");
    filter_mark_type(window, fixture, L'a');
    RECT box = filter_mark_rect(input);
    LPARAM at = MAKELPARAM((box.left + box.right) / 2, (box.top + box.bottom) / 2);
    SendMessageW(input, WM_LBUTTONDOWN, MK_LBUTTON, at);
    SendMessageW(input, WM_LBUTTONUP, 0, at);
    filter_mark_settle(window, fixture);
    require(GetWindowTextLengthW(input) == 0 &&
                folio_state_note_scope(fixture->state) == NOTE_SCOPE_FAVORITES &&
                2 * filter_mark_shown(input, &mark) < mark.strokes,
            "a press on the mark empties the term, keeps the scope and removes the mark");
    invoke_operation(window, FOLIO_COMMAND_TOGGLE_FAVORITES_ONLY);
    filter_mark_settle(window, fixture);
    require(folio_state_note_scope(fixture->state) == NOTE_SCOPE_ALL &&
                !folio_state_filtering(fixture->state),
            "back to every note, with no term left in the state");
    printf("filter-mark strokes=%d bare=%d scoped=%d\n", mark.strokes, bare, scoped);
    folio_window_destroy(window);
}

static void run_favorites_ui(struct native_fixture *_Nonnull fixture)
{
    prepare_favorites(fixture);
    header_count_units(fixture);
    filter_chip_shift(fixture);
    filter_match_narrow(fixture);
    filter_mark_needs_a_term(fixture);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "favorite interaction fixture");
    pump(window);
    fold_mark_pixels(window);
    favorite_tooltips(window);
    puts("favorites UI: filter chips");
    fflush(stdout);
    filter_chip_row(window, fixture);
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

/* The lines under a surface's rows are measured in the face that draws them: between the foot's
 * text and the buttons below it, nothing is drawn. A foot measured in a lower face would let its
 * last lines run into that gap. The rectangles are screen rectangles, as UIA reports them. */
static void surface_foot_fits(struct folio_window *_Nonnull window, RECT foot, RECT button)
{
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    MapWindowPoints(nullptr, layer, (POINT *)&foot, 2);
    MapWindowPoints(nullptr, layer, (POINT *)&button, 2);
    RECT client;
    GetClientRect(layer, &client);
    HDC source = GetDC(layer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(layer, dc, PW_CLIENTONLY), "render the surface's own window for its foot");
    COLORREF ground = GetPixel(dc, foot.right - 1, foot.bottom);
    size_t text = line_ink(dc, foot, ground);
    size_t gap = line_ink(dc, (RECT){foot.left, foot.bottom, foot.right, button.top}, ground);
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(layer, source);
    printf("surface-foot text=%zu gap-ink=%zu gap=%ld\n", text, gap, button.top - foot.bottom);
    require(button.top - foot.bottom >= 4 && text > 0,
            "the foot has text and a gap above the buttons, so the check can tell");
    require(gap == 0, "the foot's lines stay inside the height measured for them");
}

/* A row of a surface says its words in the language's face. The row is read from the surface's
 * own window; nothing but the row's text is drawn right of its mark, so the ink from the first
 * inked column to the last is the text. The text must hold a non-ASCII character (such a line
 * is drawn without tracking, as it is measured here) and the window must be in Japanese. */
static void surface_row_word_font(struct folio_window *_Nonnull window, struct accessible_view row,
                                  const wchar_t *_Nonnull text)
{
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int dpi = (int)GetDpiForWindow(root);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    struct header_font words = {face, 11, FW_NORMAL}, values = {mono, mono_size, FW_NORMAL};
    int expected = header_text_width(root, text, words);
    int monospace = header_text_width(root, text, values);
    MapWindowPoints(nullptr, layer, (POINT *)&row.bounds, 2);
    RECT client;
    GetClientRect(layer, &client);
    HDC source = GetDC(layer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(layer, dc, PW_CLIENTONLY), "render the surface's own window");
    int inset = MulDiv(6, dpi, 96);
    /* The mark's box ends 28 DIP in, where the text starts. */
    RECT band = {row.bounds.left + MulDiv(28, dpi, 96), row.bounds.top + inset,
                 row.bounds.right - inset, row.bounds.bottom - inset};
    COLORREF ground = GetPixel(dc, band.right - 1, band.top);
    int first = band.right, last = band.left;
    for (int x = band.left; x < band.right; ++x)
    {
        if (line_ink(dc, (RECT){x, band.top, x + 1, band.bottom}, ground) != 0)
        {
            first = x < first ? x : first;
            last = x;
        }
    }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(layer, source);
    int drawn = last >= first ? last - first + 1 : 0;
    printf("surface-row-face drawn=%d expected=%d monospace=%d\n", drawn, expected, monospace);
    /* Three DIP: the side bearings of the first and last glyph, and the dash's. The two faces
     * must differ by at least twice that for the check to tell them apart (measured: 96 against
     * 103 at 96 DPI, 125 against 140 at 120). */
    int slack = MulDiv(3, dpi, 96);
    require(monospace - expected >= 2 * slack || expected - monospace >= 2 * slack,
            "the row's text tells the two faces apart");
    require(drawn >= expected - slack && drawn <= expected + slack,
            "a surface row's words are drawn in the language's face");
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
    /* #1258: the surface's rows say their words in the language's face. */
    struct accessible_item first = tags_row(window, L"red");
    struct accessible_view row = {0};
    require(folio_window_accessible_read(window, &first, &row) == S_OK, "read the red tag's row");
    surface_row_word_font(window, row, (const wchar_t *)row.name);
    surface_foot_fits(
        window, tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0).bounds,
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_TAG_CLEAR).bounds);
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
    select_favorite_scope(window, true);
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
    struct note_store_request request = {"schedule", "one", expected, expected, false};
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
                wcsstr(notice.name, L"外部で変更されたため、保存していません。") != nullptr,
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
        L"a;\r\n\tb;\r\n" /* #1205: the row between the two code blocks is an empty line */
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

/* One print of the top-level window (it prints its children). Several areas can be counted in
 * the same frame while nothing repaints in between (#948). */
struct code_copy_frame
{
    HWND _Nullable root;
    RECT client;
    HDC _Nullable screen;
    HDC _Nullable memory;
    HBITMAP _Nullable bitmap;
    HGDIOBJ _Nullable old;
    const DWORD *_Nullable bits;
};

static struct code_copy_frame code_copy_frame_begin(HWND _Nonnull body)
{
    struct code_copy_frame frame = {.root = GetAncestor(body, GA_ROOT)};
    GetClientRect(frame.root, &frame.client);
    BITMAPINFO info = {.bmiHeader = {.biSize = sizeof(BITMAPINFOHEADER),
                                     .biWidth = frame.client.right,
                                     .biHeight = -frame.client.bottom,
                                     .biPlanes = 1,
                                     .biBitCount = 32,
                                     .biCompression = BI_RGB}};
    void *bits = nullptr;
    frame.screen = GetDC(frame.root);
    frame.memory = CreateCompatibleDC(frame.screen);
    frame.bitmap = CreateDIBSection(frame.screen, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    require(frame.memory != nullptr && frame.bitmap != nullptr && bits != nullptr,
            "body capture surface");
    frame.bits = bits;
    frame.old = SelectObject(frame.memory, frame.bitmap);
    require(PrintWindow(frame.root, frame.memory, PW_CLIENTONLY),
            "print the window with its decoration");
    return frame;
}

/* Pixels of one colour inside a client rectangle of the body; the area is moved into the
 * top-level window's client coordinates. */
static size_t code_copy_frame_count(const struct code_copy_frame *_Nonnull frame,
                                    HWND _Nonnull body, RECT area, struct rgb_color color)
{
    MapWindowPoints(body, frame->root, (POINT *)&area, 2);
    DWORD wanted = ((DWORD)color.red << 16) | ((DWORD)color.green << 8) | color.blue;
    return frame->bits == nullptr ? 0 : code_copy_count(frame->bits, frame->client, area, wanted);
}

static void code_copy_frame_end(struct code_copy_frame *_Nonnull frame)
{
    if (frame->root == nullptr)
        return;
    SelectObject(frame->memory, frame->old);
    DeleteObject(frame->bitmap);
    DeleteDC(frame->memory);
    ReleaseDC(frame->root, frame->screen);
    *frame = (struct code_copy_frame){0};
}

/* Pixels of one colour inside a client rectangle of the body, as the window prints itself. */
static size_t code_copy_pixels(HWND _Nonnull body, RECT area, struct rgb_color color)
{
    struct code_copy_frame frame = code_copy_frame_begin(body);
    size_t count = code_copy_frame_count(&frame, body, area, color);
    code_copy_frame_end(&frame);
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

/* #850: the bare URL gallery in light, dark and high contrast, for the record's screenshots.
 * Every link, including one that shows its own URL, takes the theme's link colour (ADR0100). */
static void bare_url_visual(struct native_fixture *_Nonnull fixture)
{
    const char *const looks[] = {"light", "dark", "contrast"};
    for (size_t at = 0; at < 3; ++at)
    {
        favorite_high_contrast = at == 2;
        require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                  : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "bare URL gallery palette");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "bare URL gallery window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 760, 720, SWP_NOZORDER | SWP_NOACTIVATE),
                "bare URL gallery size");
        pump(window);
        char path[64];
        require(snprintf(path, sizeof path, "bare-url-%s.bmp", looks[at]) > 0,
                "bare URL screenshot path");
        tags_capture(root, path);
        /* #869: the footnotes are at the end of the note. */
        SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY), WM_VSCROLL, SB_BOTTOM,
                     0);
        pump(window);
        require(snprintf(path, sizeof path, "bare-url-%s-end.bmp", looks[at]) > 0,
                "bare URL end screenshot path");
        tags_capture(root, path);
        folio_window_destroy(window);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
    favorite_high_contrast = false;
}

/* #891 / ADR0108: the main window opens at the recorded place when that can be used, at the
 * initial place otherwise, and records its place when it closes. */
static RECT window_place_open(struct native_fixture *_Nonnull fixture,
                              struct window_placement recorded, bool *_Nonnull maximized)
{
    require(storage_state_set_window_restore(fixture->location, true) == STORAGE_READY &&
                storage_state_record_window(fixture->location, &recorded) == STORAGE_READY,
            "a recorded place");
    if (!recorded.restore)
        require(storage_state_set_window_restore(fixture->location, false) == STORAGE_READY,
                "the choice is off");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "window for the recorded place");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    pump(window);
    RECT bounds;
    GetWindowRect(root, &bounds);
    *maximized = IsZoomed(root) != 0;
    folio_window_destroy(window);
    return bounds;
}

static void window_place_closes(struct native_fixture *_Nonnull fixture)
{
    require(storage_state_set_window_restore(fixture->location, true) == STORAGE_READY,
            "carry the place over");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "window to close");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(SetWindowPos(root, nullptr, 90, 70, 700, 480, SWP_NOZORDER | SWP_NOACTIVATE),
            "the place before closing");
    pump(window);
    SendMessageW(root, WM_CLOSE, 0, 0);
    pump(window);
    require(!IsWindow(root), "the window closed");
    struct window_placement recorded = storage_state_window(fixture->location);
    require(recorded.restore && recorded.placed && recorded.x == 90 && recorded.y == 70 &&
                recorded.width == 700 && recorded.height == 480 && !recorded.maximized,
            "closing records the place the window had");
    folio_window_destroy(window);
}

/* #904: the list-only layout narrows the window; closing in it records the width the window
 * returns to, so the next start can use the record. */
/* #936: a width that a round trip through 96-DPI units would change at this DPI (any width at 96
 * DPI, where nothing rounds). */
static int window_place_odd_width(UINT dpi)
{
    for (int width = 760; width < 800; ++width)
        if (MulDiv(MulDiv(width, 96, (int)dpi), (int)dpi, 96) != width)
            return width;
    return 760;
}

static int window_place_width(HWND root)
{
    RECT bounds;
    GetWindowRect(root, &bounds);
    return bounds.right - bounds.left;
}

static void window_place_closes_narrow(struct native_fixture *_Nonnull fixture)
{
    require(storage_state_set_window_restore(fixture->location, true) == STORAGE_READY,
            "carry the place over");
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "window to narrow and close");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int wide = window_place_odd_width(GetDpiForWindow(root));
    require(SetWindowPos(root, nullptr, 90, 70, wide, 480, SWP_NOZORDER | SWP_NOACTIVATE),
            "the wide place");
    pump(window);
    action(window, ACCESSIBLE_LAYOUT_DRAWER);
    require(window_place_width(root) < wide, "the list-only layout narrows the window");
    const struct layout_step both = {FOLIO_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH,
                                     FOLIO_COMMAND_LAYOUT_BOTH, ACCESSIBLE_LAYOUT_BOTH,
                                     ACCESSIBLE_LAYOUT_BOTH};
    (void)select_layout(window, &both);
    pump(window);
    int back = window_place_width(root);
    action(window, ACCESSIBLE_LAYOUT_DRAWER);
    SendMessageW(root, WM_CLOSE, 0, 0);
    pump(window);
    require(!IsWindow(root), "the narrow window closed");
    struct window_placement recorded = storage_state_window(fixture->location);
    printf("list-only width: dpi %u wide %d back %d recorded %d\n", GetDpiForSystem(), wide, back,
           recorded.width);
    fflush(stdout);
    require(back == wide, "returning from the list-only layout restores the exact width (#936)");
    require(recorded.placed && recorded.width == wide && recorded.height == 480,
            "closing in the list-only layout records the exact wide width");
    folio_window_destroy(window);
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
}

static void window_place(struct native_fixture *_Nonnull fixture)
{
    UINT dpi = GetDpiForSystem();
    bool maximized = false;
    struct window_placement recorded = {
        .restore = true, .placed = true, .x = 60, .y = 50, .width = 720, .height = 500, .dpi = dpi};
    RECT bounds = window_place_open(fixture, recorded, &maximized);
    require(bounds.left == 60 && bounds.top == 50 && bounds.right - bounds.left == 720 &&
                bounds.bottom - bounds.top == 500 && !maximized,
            "the window opens at the recorded place and size");
    recorded.maximized = true;
    (void)window_place_open(fixture, recorded, &maximized);
    require(maximized, "a maximized window opens maximized");
    recorded.maximized = false;
    recorded.x = 30000;
    bounds = window_place_open(fixture, recorded, &maximized);
    require(bounds.left != 30000 && bounds.right - bounds.left == MulDiv(960, (int)dpi, 96) &&
                bounds.bottom - bounds.top == MulDiv(640, (int)dpi, 96),
            "a place off every screen opens at the initial place and size");
    recorded.x = 60;
    recorded.y = -30;
    bounds = window_place_open(fixture, recorded, &maximized);
    require(bounds.top != -30 && bounds.right - bounds.left == MulDiv(960, (int)dpi, 96),
            "a header above the screen opens at the initial place");
    recorded.y = 50;
    recorded.restore = false;
    bounds = window_place_open(fixture, recorded, &maximized);
    require(bounds.right - bounds.left == MulDiv(960, (int)dpi, 96) &&
                !(bounds.left == 60 && bounds.top == 50),
            "with the choice off the recorded place is not used");
    window_place_closes(fixture);
    window_place_closes_narrow(fixture);
    require(storage_state_set_window_restore(fixture->location, false) == STORAGE_READY,
            "leave the fixture at the initial place");
}

/* #883: the body's context menu through the real window: its rows, which are enabled, the
 * edit a row runs, and the screenshots for the record. Nothing writes the clipboard. */
static struct accessible_view text_menu_row(struct folio_window *_Nonnull window, size_t slot)
{
    struct accessible_item item = {.kind = ACCESSIBLE_SURFACE_CHOICE,
                                   .slot = slot,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "context menu row");
    return view;
}

static void text_menu_open(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    SetFocus(body);
    SendMessageW(body, EM_SETSEL, 2, 9);
    SendMessageW(body, WM_CONTEXTMENU, (WPARAM)body, -1);
    pump(window);
}

static void text_menu_edit_rows(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    text_menu_open(window, body);
    struct accessible_view cut = text_menu_row(window, 1);
    struct accessible_view copy = text_menu_row(window, 2);
    struct accessible_view all = text_menu_row(window, 4);
    require(cut.set_size == 5 && cut.enabled && copy.enabled && all.enabled &&
                wcscmp((const wchar_t *)cut.name, L"切り取り") == 0 &&
                wcscmp((const wchar_t *)copy.accelerator, L"Ctrl+C") == 0 &&
                wcscmp((const wchar_t *)all.access_key, L"A") == 0,
            "editing offers Undo, Cut, Copy, Paste and Select all with their keys");
    require(!text_menu_row(window, 0).enabled, "nothing to undo in an unchanged note");
}

/* A row that starts with its name says the name in the language's face; what is drawn at the
 * row's right (a key, an alias) stays monospace. The row is read from the surface's own window:
 * the name's ink is as wide as the language's face makes it. The band ends just past the name
 * (it starts `start` DIP into the row), before what is drawn at the right. bounds is a screen
 * rectangle. The window must be in Japanese. */
static void surface_label_word_font(struct folio_window *_Nonnull window, RECT bounds,
                                    const wchar_t *_Nonnull label, int start)
{
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    struct header_font words = {face, 11, FW_NORMAL}, values = {mono, mono_size, FW_NORMAL};
    int expected = header_text_width(root, label, words);
    int monospace = header_text_width(root, label, values);
    MapWindowPoints(nullptr, layer, (POINT *)&bounds, 2);
    RECT client;
    GetClientRect(layer, &client);
    HDC source = GetDC(layer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(layer, dc, PW_CLIENTONLY), "render the surface's own window for a label");
    int inset = MulDiv(6, (int)GetDpiForWindow(root), 96);
    RECT band = {bounds.left + inset, bounds.top + inset,
                 bounds.left + MulDiv(start, (int)GetDpiForWindow(root), 96) + expected + 4,
                 bounds.bottom - inset};
    COLORREF ground = GetPixel(dc, band.left, band.top);
    int first = band.right, last = band.left;
    for (int x = band.left; x < band.right; ++x)
    {
        if (line_ink(dc, (RECT){x, band.top, x + 1, band.bottom}, ground) != 0)
        {
            first = x < first ? x : first;
            last = x;
        }
    }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(layer, source);
    int drawn = last >= first ? last - first + 1 : 0;
    printf("label-word-face drawn=%d expected=%d monospace=%d\n", drawn, expected, monospace);
    /* The band ends just past the expected width, so a wider drawing would be cut and could
     * pass: the check holds only while the monospace drawing is the narrower one. */
    require(expected - monospace >= 5,
            "the row's label is clearly narrower in the monospace face, so the check can tell");
    require(drawn >= expected - 3 && drawn <= expected + 3,
            "a row's name is drawn in the language's face");
}

/* A menu row's name is "name (key)"; it starts 16 DIP in. */
static void menu_row_word_font(struct folio_window *_Nonnull window, size_t slot)
{
    struct accessible_view row = text_menu_row(window, slot);
    wchar_t label[128];
    require(swprintf(label, 128, L"%ls (%ls)", (const wchar_t *)row.name,
                     (const wchar_t *)row.access_key) > 0,
            "the menu row's label fits");
    surface_label_word_font(window, row.bounds, label, 16);
}

static void palette_alias_monospace(struct folio_window *_Nonnull window, RECT row);

/* The palette's rows say the command's name in the language's face (the aliases at the right
 * stay monospace). The first row whose name is long enough to tell the faces apart is read; a
 * row's name starts 12 DIP in. */
static void palette_row_word_font(struct folio_window *_Nonnull window)
{
    struct accessible_request open = {.item = {.kind = ACCESSIBLE_HELP},
                                      .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &open) == S_OK, "open the palette for its rows");
    pump(window);
    struct accessible_item list = {.kind = ACCESSIBLE_SURFACE_LIST,
                                   .revision = folio_window_accessible_surface_revision(window)};
    struct accessible_item row;
    struct accessible_view view = {0};
    HRESULT found =
        folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &row);
    while (found == S_OK)
    {
        require(folio_window_accessible_read(window, &row, &view) == S_OK, "read a palette row");
        if (wcslen((const wchar_t *)view.name) >= 8 && !view.offscreen)
            break;
        found = folio_window_accessible_navigate(window, &row, ACCESSIBLE_NAVIGATION_NEXT, &row);
    }
    require(found == S_OK, "the palette shows a row with a long name");
    surface_label_word_font(window, view.bounds, (const wchar_t *)view.name, 12);
    found = folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &row);
    while (found == S_OK)
    {
        require(folio_window_accessible_read(window, &row, &view) == S_OK, "read a palette row");
        if (wcscmp((const wchar_t *)view.name, L"編集") == 0)
            break;
        found = folio_window_accessible_navigate(window, &row, ACCESSIBLE_NAVIGATION_NEXT, &row);
    }
    require(found == S_OK && !view.offscreen, "the palette shows the Edit row");
    palette_alias_monospace(window, view.bounds);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

/* The width of the ink in a band of the surface's own window: from the first inked column to
 * the last. band is a screen rectangle whose top left pixel is ground. */
static int surface_ink_span(struct folio_window *_Nonnull window, RECT band)
{
    HWND layer = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    MapWindowPoints(nullptr, layer, (POINT *)&band, 2);
    RECT client;
    GetClientRect(layer, &client);
    HDC source = GetDC(layer), dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(layer, dc, PW_CLIENTONLY), "render the surface's own window for a band");
    COLORREF ground = GetPixel(dc, band.left, band.top);
    int first = band.right, last = band.left;
    for (int x = band.left; x < band.right; ++x)
    {
        if (line_ink(dc, (RECT){x, band.top, x + 1, band.bottom}, ground) != 0)
        {
            first = x < first ? x : first;
            last = x;
        }
    }
    SelectObject(dc, old);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(layer, source);
    return last >= first ? last - first + 1 : 0;
}

/* The alias at a palette row's right stays monospace. The "Edit" row has one alias; it is drawn
 * in the 96 DIP at the row's right, with the layer's tracking (1 DIP a character). */
static void palette_alias_monospace(struct folio_window *_Nonnull window, RECT row)
{
    static const wchar_t alias[] = L"startinsert";
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int dpi = (int)GetDpiForWindow(root);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    struct header_font words = {face, 11, FW_NORMAL}, values = {mono, mono_size, FW_NORMAL};
    int tracking = ((int)wcslen(alias) - 1) * MulDiv(1, dpi, 96);
    int expected = header_text_width(root, alias, values) + tracking;
    int wordy = header_text_width(root, alias, words) + tracking;
    int inset = MulDiv(6, dpi, 96);
    int drawn = surface_ink_span(window, (RECT){row.right - MulDiv(96, dpi, 96), row.top + inset,
                                                row.right - inset, row.bottom - inset});
    printf("alias-mono-face drawn=%d expected=%d words=%d\n", drawn, expected, wordy);
    int slack = MulDiv(3, dpi, 96);
    require(expected - wordy >= 2 * slack || wordy - expected >= 2 * slack,
            "the alias tells the two faces apart");
    require(drawn >= expected - slack && drawn <= expected + slack,
            "a palette row's alias stays in the monospace face");
}

/* A history row starts with the version's number in the monospace face (with the spaces after
 * it), then says the note's first line in the language's face. The ink from the row's first
 * inked column to its last is as wide as that mix. An all-ASCII run is drawn with the layer's
 * tracking (1 DIP a character); the first line holds a non-ASCII character and is not. */
static void history_row_fonts(struct folio_window *_Nonnull window, struct accessible_item row)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int dpi = (int)GetDpiForWindow(root);
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &row, &view) == S_OK, "read a history row");
    const wchar_t *text = (const wchar_t *)view.name;
    wchar_t number[16] = {0};
    size_t split = wcsspn(text, L"0123456789");
    split += wcsspn(text + split, L" ");
    require(split > 0 && split < 16 && text[split] != L'\0',
            "a history row is a number and a line");
    wmemcpy(number, text, split);
    wchar_t face[LF_FACESIZE];
    ui_face_for(FOLIO_LANGUAGE_JA, face);
    wchar_t mono[LF_FACESIZE];
    int mono_size = ui_face_mono(mono, 11);
    struct header_font words = {face, 11, FW_NORMAL}, values = {mono, mono_size, FW_NORMAL};
    int expected = header_text_width(root, number, values) + (int)split * MulDiv(1, dpi, 96) +
                   header_text_width(root, text + split, words);
    int monospace = header_text_width(root, text, values);
    int inset = MulDiv(6, dpi, 96);
    int drawn =
        surface_ink_span(window, (RECT){view.bounds.left + inset, view.bounds.top + inset,
                                        view.bounds.right - inset, view.bounds.bottom - inset});
    printf("history-row-face drawn=%d expected=%d monospace=%d\n", drawn, expected, monospace);
    /* Four DIP: the side bearings of the first and last glyph (the last is an emoji or a
     * backtick from a linked face). */
    int slack = MulDiv(4, dpi, 96);
    require(monospace - expected >= 2 * slack || expected - monospace >= 2 * slack,
            "the row's text tells the two faces apart");
    require(drawn >= expected - slack && drawn <= expected + slack,
            "a history row draws its number monospace and its line in the language's face");
}

/* The reading view's menu: Copy, Copy as Markdown | Edit here | Select all (#1012). */
static void text_menu_view_rows(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    text_menu_open(window, body);
    struct accessible_view copy = text_menu_row(window, 0);
    struct accessible_view markdown = text_menu_row(window, 1);
    struct accessible_view here = text_menu_row(window, 2);
    require(copy.set_size == 4 && copy.enabled &&
                wcscmp((const wchar_t *)copy.name, L"コピー") == 0 &&
                wcscmp((const wchar_t *)markdown.name, L"Markdownとしてコピー") == 0 &&
                wcscmp((const wchar_t *)here.name, L"ここを編集") == 0 && markdown.enabled &&
                here.enabled,
            "viewing offers Copy, Copy as Markdown, Edit here and Select all (#1012)");
    menu_row_word_font(window, 1);
}

/* #1012 review: when the reading view no longer agrees with the source map (here one more
 * paragraph mark is put into it), both source items are greyed and nothing else is. */
static void text_menu_mismatch(struct folio_window *_Nonnull window)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_SETSEL, 0, 0);
    SETTEXTEX insert = {ST_SELECTION | ST_KEEPUNDO | ST_UNICODE, 1200};
    SendMessageW(body, EM_SETTEXTEX, (WPARAM)&insert, (LPARAM)L"\r");
    text_menu_open(window, body);
    struct accessible_view copy = text_menu_row(window, 0);
    struct accessible_view markdown = text_menu_row(window, 1);
    struct accessible_view here = text_menu_row(window, 2);
    struct accessible_view all = text_menu_row(window, 3);
    require(copy.enabled && all.enabled && !markdown.enabled && !here.enabled,
            "a view that disagrees with the source map greys both source items");
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_KEYDOWN, VK_ESCAPE,
                 0);
    pump(window);
    action(window, ACCESSIBLE_EDIT); /* render the note again */
    action(window, ACCESSIBLE_VIEW);
}

/* #1012: 「ここを編集」 opens the editing view with the caret on the line that holds the text
 * selected in the reading view. 「Markdownとしてコピー」 is only checked as offered: the tests do
 * not write the user's clipboard (ADR0106). */
static void text_menu_edit_here(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    text_menu_open(window, body);
    wchar_t chosen[64] = {0};
    SendMessageW(body, EM_GETSELTEXT, 0, (LPARAM)chosen);
    for (size_t at = 0; chosen[at] != L'\0'; ++at)
        if (chosen[at] == L'\r' || chosen[at] == L'\v')
            chosen[at] = L'\0'; /* the clicked paragraph's part of the selection */
    struct accessible_request request = {
        .item = {.kind = ACCESSIBLE_SURFACE_CHOICE,
                 .slot = 2,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "invoke Edit here");
    pump(window);
    body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    CHARRANGE caret = {0, 0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&caret);
    LRESULT line = SendMessageW(body, EM_EXLINEFROMCHAR, 0, caret.cpMin);
    wchar_t text[512] = {0};
    text[0] = (wchar_t)(sizeof text / sizeof text[0] - 1);
    LRESULT copied = SendMessageW(body, EM_GETLINE, (WPARAM)line, (LPARAM)text);
    text[copied >= 0 && copied < 511 ? copied : 0] = L'\0';
    printf("edit here: mode %d, focus %d, chosen %zu units, caret %ld..%ld, line %lld (%zu units), "
           "found %d\n",
           (int)folio_state_pane_mode(fixture->state), GetFocus() == body, wcslen(chosen),
           caret.cpMin, caret.cpMax, (long long)line, wcslen(text),
           wcsstr(text, chosen) != nullptr);
    fflush(stdout);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT && GetFocus() == body &&
                caret.cpMin == caret.cpMax && chosen[0] != L'\0' && wcsstr(text, chosen) != nullptr,
            "Edit here opens the editing view on the chosen text's line");
    action(window, ACCESSIBLE_VIEW);
}

/* #1052: the reading view's menu is as wide as its longest row, up to a ceiling (360 at 96
 * DPI) past which text would be cut. In every language its rows stay under that ceiling. */
static void text_menu_languages(struct native_fixture *_Nonnull fixture)
{
    /* Japanese is the language of text_menu_visual above. */
    const enum folio_language languages[] = {FOLIO_LANGUAGE_EN, FOLIO_LANGUAGE_ZH_HANS};
    const char *const names[] = {"en", "zh"};
    for (size_t at = 0; at < 2; ++at)
    {
        require(folio_state_set_language(fixture->state, languages[at]) == FOLIO_STATE_READY,
                "menu language");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "menu language window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 760, 520, SWP_NOZORDER | SWP_NOACTIVATE),
                "menu language window size");
        pump(window);
        text_menu_open(window, folio_window_accessible_window(window, ACCESSIBLE_BODY));
        struct accessible_view row = text_menu_row(window, 1);
        int width = row.bounds.right - row.bounds.left;
        int ceiling = MulDiv(360, (int)GetDpiForWindow(root), 96);
        printf("text menu %s: rows %d wide, ceiling %d\n", names[at], width, ceiling);
        fflush(stdout);
        require(row.set_size == 4 && width > 0 && width < ceiling - 4,
                "the reading view's menu is under its ceiling in this language");
        char path[64];
        require(snprintf(path, sizeof path, "text-menu-view-%s.bmp", names[at]) > 0, "path");
        tags_capture(root, path);
        SendMessageW(root, WM_KEYDOWN, VK_ESCAPE, 0);
        pump(window);
        folio_window_destroy(window);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "back to Japanese");
}

/* #1101: a key sent bare is matched against the modifiers this thread's key state holds
 * (`pressed_shortcut_modifiers` reads GetKeyState). A Ctrl, Shift or Alt that is down when the
 * case runs makes the menu key another shortcut, and the menu stays open. The case states the
 * modifiers it means, none, and says so when it found some held: that is the evidence the
 * next time the full run fails here. */
/* Leaves no Ctrl, Shift or Alt held in this thread's key state, keeps what was there in
 * `original` for the caller to restore, and says so when one was held (`where` names the case). */
static void release_modifiers(BYTE original[256], const char *_Nonnull where)
{
    static const int modifiers[] = {VK_SHIFT,    VK_CONTROL,  VK_MENU,  VK_LSHIFT, VK_RSHIFT,
                                    VK_LCONTROL, VK_RCONTROL, VK_LMENU, VK_RMENU};
    BYTE neutral[256];
    require(GetKeyboardState(original), "save the keyboard state");
    memcpy(neutral, original, sizeof neutral);
    bool held = false;
    for (size_t at = 0; at < sizeof modifiers / sizeof modifiers[0]; ++at)
    {
        held = held || (original[modifiers[at]] & 0x80) != 0;
        neutral[modifiers[at]] = 0;
    }
    if (held)
        printf("%s: a modifier was held (shift=%d ctrl=%d alt=%d)\n", where,
               original[VK_SHIFT] >> 7, original[VK_CONTROL] >> 7, original[VK_MENU] >> 7);
    fflush(stdout);
    require(SetKeyboardState(neutral), "no modifier is held for the plain key");
}

static void menu_key_closes(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    BYTE original[256];
    release_modifiers(original, "text menu, the menu key");
    /* The key goes to the open menu; a focus that is nowhere, or still in the body, would fail
     * below for another reason, and is told apart here. */
    HWND focus = GetFocus();
    require(focus != nullptr && focus != body,
            "the body's menu holds the focus before the menu key");
    PostMessageW(focus, WM_KEYDOWN, VK_APPS, 0);
    pump(window);
    bool closed = GetFocus() == body;
    require(SetKeyboardState(original), "restore the keyboard state");
    require(closed, "the menu key closes the body's menu");
}

static void text_menu_visual(struct native_fixture *_Nonnull fixture)
{
    const char *const looks[] = {"light", "dark"};
    for (size_t at = 0; at < 2; ++at)
    {
        require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                  : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "context menu palette");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "context menu window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 760, 520, SWP_NOZORDER | SWP_NOACTIVATE),
                "context menu window size");
        pump(window);
        HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
        text_menu_view_rows(window, body);
        char path[64];
        require(snprintf(path, sizeof path, "text-menu-view-%s.bmp", looks[at]) > 0, "path");
        tags_capture(root, path);
        struct accessible_request request = {
            .item = {.kind = ACCESSIBLE_SURFACE_CHOICE,
                     .slot = 3,
                     .revision = folio_window_accessible_surface_revision(window)},
            .action = ACCESSIBLE_ACTION_INVOKE};
        require(folio_window_accessible_action(window, &request) == S_OK, "invoke Select all");
        pump(window);
        CHARRANGE all = {0, 0};
        SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&all);
        require(all.cpMin == 0 && all.cpMax > 9 && GetFocus() == body,
                "Select all runs in the body and the focus returns to it");
        text_menu_edit_here(window, fixture);
        text_menu_mismatch(window);
        action(window, ACCESSIBLE_EDIT);
        body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
        text_menu_edit_rows(window, body);
        require(snprintf(path, sizeof path, "text-menu-edit-%s.bmp", looks[at]) > 0, "path");
        tags_capture(root, path);
        /* #908: with the body's menu open, the menu key closes it and the focus returns to the
         * body; it must not swap in the list's menu. */
        menu_key_closes(window, body);
        SendMessageW(root, WM_KEYDOWN, VK_ESCAPE, 0);
        pump(window);
        /* Leave the session viewing, so the next window starts from the same state. */
        action(window, ACCESSIBLE_VIEW);
        folio_window_destroy(window);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
}

/* #945: the native position of `text` at or after `from` in the viewed note. */
static LONG footnote_find(HWND _Nonnull body, const wchar_t *_Nonnull text, LONG from)
{
    FINDTEXTEXW find = {.chrg = {from, -1}, .lpstrText = text};
    LONG found = (LONG)SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find);
    require(found >= 0, "the fixture text is in the viewed note");
    return found;
}

/* Ctrl+Enter from wherever the caret is now; returns the caret the activation left. */
static LONG footnote_key(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    SetFocus(body);
    native_key_input(window, VK_RETURN, FCONTROL);
    CHARRANGE range = {0, 0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&range);
    require(range.cpMin == range.cpMax, "a jump leaves a caret, not a selection");
    return range.cpMin;
}

/* A real pointer click on the character at `position`; returns the caret it left. */
static LONG footnote_click(struct folio_window *_Nonnull window, HWND _Nonnull body, LONG position)
{
    POINTL from = {0}, to = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&from, position);
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&to, position + 1);
    LPARAM where = MAKELPARAM((from.x + to.x) / 2, from.y + 6);
    SendMessageW(body, WM_MOUSEMOVE, 0, where);
    SendMessageW(body, WM_LBUTTONDOWN, MK_LBUTTON, where);
    SendMessageW(body, WM_LBUTTONUP, 0, where);
    pump(window);
    CHARRANGE range = {0, 0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&range);
    return range.cpMin;
}

static bool footnote_in_view(HWND _Nonnull body, LONG position)
{
    POINTL point = {0};
    SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&point, position);
    RECT client = {0};
    GetClientRect(body, &client);
    return point.y >= 0 && point.y < client.bottom;
}

static bool footnote_text_at(HWND _Nonnull body, LONG position, const wchar_t *_Nonnull expected)
{
    wchar_t text[8] = L"";
    TEXTRANGEW range = {.chrg = {position, position + (LONG)wcslen(expected)}, .lpstrText = text};
    SendMessageW(body, EM_GETTEXTRANGE, 0, (LPARAM)&range);
    return wcscmp(text, expected) == 0;
}

/* #945: a footnote's raised number goes to its footnote and the footnote's number comes back to
 * the first reference, by Ctrl+Enter and by a click, each from where the last jump landed. The
 * links are internal, so the body's menu stays the plain viewing one. */
static void footnote_jump(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "footnote fixture language");
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "footnote window");
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int width = MulDiv(620, (int)GetDpiForWindow(root), 96);
    int height = MulDiv(380, (int)GetDpiForWindow(root), 96);
    require(SetWindowPos(root, nullptr, 20, 20, width, height, SWP_NOZORDER | SWP_NOACTIVATE),
            "a short footnote window");
    pump(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG sentence = footnote_find(body, L"脚注の例", 0);
    LONG first = footnote_find(body, L"と", sentence) - 1;
    LONG second = footnote_find(body, L"（#869）", sentence) - 1;
    LONG notes = footnote_find(body, L"一つ目の脚注", second);
    require(!footnote_in_view(body, notes), "the footnotes start below the short window");
    SendMessageW(body, EM_SETSEL, (WPARAM)first, first);
    LONG at = footnote_key(window, body);
    printf("footnote jump: ref %ld -> note %ld (text at %ld) in view %d\n", first, at, notes,
           footnote_in_view(body, at));
    fflush(stdout);
    require(footnote_text_at(body, at, L"1.") && at > second && at < notes &&
                footnote_in_view(body, at),
            "Ctrl+Enter on the first raised number goes to footnote 1 and scrolls it into view");
    LONG back = footnote_key(window, body);
    printf("footnote jump: back %ld in view %d\n", back, footnote_in_view(body, back));
    fflush(stdout);
    require(back == first && footnote_in_view(body, back),
            "Ctrl+Enter where the jump landed comes back to the first reference");
    at = footnote_click(window, body, second);
    printf("footnote click: ref %ld -> note %ld\n", second, at);
    fflush(stdout);
    require(footnote_text_at(body, at, L"2.") && at > notes,
            "a click on the second raised number goes to footnote 2, after footnote 1");
    back = footnote_click(window, body, at);
    require(back == second, "a click on footnote 2's number comes back to its reference");
    SendMessageW(body, EM_SETSEL, (WPARAM)first, first);
    SendMessageW(body, WM_CONTEXTMENU, (WPARAM)body, -1);
    pump(window);
    struct accessible_view copy = text_menu_row(window, 0);
    require(copy.set_size == 4 && wcscmp((const wchar_t *)copy.name, L"コピー") == 0,
            "a footnote number is not a web link: the menu offers no open or copy-link");
    SendMessageW(root, WM_KEYDOWN, VK_ESCAPE, 0);
    pump(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW && controlled.requests == 0,
            "moving between footnotes keeps the reading view and requests nothing");
    folio_window_destroy(window);
    folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
}

/* #855: the list's search field with its key at the right of the placeholder, in each language
 * and both themes, for the record's screenshots. */
static void search_key_visual(struct native_fixture *_Nonnull fixture)
{
    const enum folio_language languages[] = {FOLIO_LANGUAGE_JA, FOLIO_LANGUAGE_EN,
                                             FOLIO_LANGUAGE_ZH_HANS};
    const char *const names[] = {"ja", "en", "zh"};
    for (size_t at = 0; at < 6; ++at)
    {
        require(folio_state_set_language(fixture->state, languages[at % 3]) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at < 3 ? FOLIO_THEME_CHOICE_LIGHT
                                                                 : FOLIO_THEME_CHOICE_DARK) ==
                        FOLIO_STATE_READY,
                "search key palette and language");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "search key window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 980, 420, SWP_NOZORDER | SWP_NOACTIVATE),
                "search key window size");
        pump(window);
        char path[64];
        require(snprintf(path, sizeof path, "search-key-%s-%s.bmp", names[at % 3],
                         at < 3 ? "light" : "dark") > 0,
                "search key screenshot path");
        tags_capture(root, path);
        folio_window_destroy(window);
    }
}

/* What a failed icon check needs to be diagnosed from a remote log: geometry, DPI, zoom, the
 * place that was read and how many pixels of each role it holds. */
static void code_copy_scroll_report(HWND _Nonnull body, const struct rtf_palette *_Nonnull colors,
                                    const char *_Nonnull path, RECT place)
{
    RECT window;
    RECT client;
    GetWindowRect(GetAncestor(body, GA_ROOT), &window);
    GetClientRect(body, &client);
    int numerator = 0, denominator = 0;
    SendMessageW(body, EM_GETZOOM, (WPARAM)&numerator, (LPARAM)&denominator);
    fprintf(stderr,
            "scroll path %s: window %ldx%ld pane %ldx%ld dpi %u zoom %d/%d place %ld,%ld,%ld,%ld "
            "surface %zu comment %zu text %zu\n",
            path, window.right - window.left, window.bottom - window.top, client.right,
            client.bottom, GetDpiForWindow(body), numerator, denominator, place.left, place.top,
            place.right, place.bottom, code_copy_pixels(body, place, colors->code_background),
            code_copy_pixels(body, place, colors->code_comment),
            code_copy_pixels(body, place, colors->code_text));
}

/* #839 round 8: every header row that is fully inside the pane shows its icon after any scroll
 * path. The expected place comes from the row's own first and last character only: the square
 * at the row's right end, which the document leaves blank (non-breaking spaces), so anything
 * there that is not the code surface is the painted icon. The icon is antialiased, so its exact
 * colour is not counted: at 96 DPI the 14 px strokes hold only a few pixels of it (round 9). */
static unsigned code_copy_scroll_rows(struct folio_window *_Nonnull window,
                                      struct native_fixture *_Nonnull fixture,
                                      const char *_Nonnull path)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    const struct rtf_palette *colors = &folio_state_palette(fixture->state)->rtf;
    RECT client;
    GetClientRect(body, &client);
    int numerator = 0, denominator = 0;
    SendMessageW(body, EM_GETZOOM, (WPARAM)&numerator, (LPARAM)&denominator);
    int zoom = denominator != 0 ? MulDiv(100, numerator, denominator) : 100;
    LONG row = MulDiv(MulDiv(360, (int)GetDpiForWindow(body), 1440), zoom, 100);
    LONG icon = MulDiv(MulDiv(20, (int)GetDpiForWindow(body), 96), zoom, 100);
    unsigned visible = 0;
    /* #948: the window is printed once for this scroll position and every visible header row is
     * counted in that frame; nothing repaints between the rows. */
    struct code_copy_frame frame = {0};
    for (size_t block = 0; block < 5; ++block)
    {
        LONG word = code_copy_word(body, block);
        POINTL start = {0}, end = {0};
        SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&start, word - 12);
        SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&end, word + 7);
        if (start.y < 0 || start.y + row > client.bottom || end.y != start.y)
            continue;
        if (visible++ == 0)
            frame = code_copy_frame_begin(body);
        RECT place = {end.x - icon, start.y, end.x, start.y + row};
        size_t area = (size_t)(place.right - place.left) * (size_t)(place.bottom - place.top);
        size_t surface = code_copy_frame_count(&frame, body, place, colors->code_background);
        if (surface + 20 > area || surface < area / 2)
        {
            code_copy_scroll_report(body, colors, path, place);
            require(false, "a fully visible header row shows its copy icon");
        }
    }
    code_copy_frame_end(&frame);
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
    /* The hosted CI desktop is 96 DPI and a developer desktop often is not: the -96 selector makes
     * this thread's windows 96 DPI wherever it runs, so both geometries are checked locally. */
    if (code_copy_scroll_96)
        require(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE) != nullptr,
                "96 DPI windows for the scroll case");
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

/* Where the text is in the viewed note, or -1. */
static LONG definition_find(HWND _Nonnull body, const wchar_t *_Nonnull text)
{
    FINDTEXTEXW find = {.chrg = {0, -1}, .lpstrText = text};
    return (LONG)SendMessageW(body, EM_FINDTEXTEXW, FR_DOWN | FR_MATCHCASE, (LPARAM)&find);
}

/* What the reading view holds for the gallery, whatever the look. */
static void definition_checks(HWND _Nonnull body, const char *_Nonnull look)
{
    LONG term = definition_find(body, L"閲覧表示");
    LONG first = definition_find(body, L"Markdownを整えて");
    LONG second = definition_find(body, L"保存は自動。");
    require(term >= 0 && first > term && second > first,
            "the term, then its descriptions, in the order written");
    require(definition_find(body, L": Markdown") < 0 && definition_find(body, L": 保存") < 0 &&
                definition_find(body, L": 原文") < 0,
            "a description's colon is not shown");
    require(definition_find(body, L"続きの行は同じ説明。") > second,
            "a line under a description continues it");
    require(definition_find(body, L": 用語の無い行は段落のまま。") >= 0 &&
                definition_find(body, L": リストの中は従来どおり") >= 0,
            "a colon line without a term, or inside a list, stays ordinary text");
    int columns = 0;
    LONG row = first;
    while (row > term && note_pane_table_columns(body, row, &columns) != NOTE_PANE_TABLE_PRESENT)
        --row;
    printf("definition list (%s): term %ld, first description %ld in a row at %ld of %d "
           "column(s)\n",
           look, term, first, row, columns);
    fflush(stdout);
    require(row > term && columns == 1, "a description is a native row of one cell");
}

/* #971 / ADR0113: a term and its descriptions in the reading view, in each look. A description
 * is a one-cell native row the pane fits to its width; its colon is not shown. */
static void definition_list(struct native_fixture *_Nonnull fixture)
{
    const char *const looks[] = {"light", "dark", "contrast"};
    for (size_t at = 0; at < 3; ++at)
    {
        favorite_high_contrast = at == 2;
        require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                  : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "definition list palette");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "definition list window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        require(SetWindowPos(root, nullptr, 20, 20, 620, 900, SWP_NOZORDER | SWP_NOACTIVATE),
                "definition list size");
        pump(window);
        definition_checks(folio_window_accessible_window(window, ACCESSIBLE_BODY), looks[at]);
        char path[64];
        require(snprintf(path, sizeof path, "definition-%s.bmp", looks[at]) > 0,
                "definition list screenshot path");
        tags_capture(root, path);
        folio_window_destroy(window);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
    favorite_high_contrast = false;
}

/* #1071 / ADR0118: what the reading view has selected. */
static void expect_link_selection(HWND _Nonnull body, const wchar_t *_Nonnull holds,
                                  const wchar_t *_Nullable lacks, const char *_Nonnull message)
{
    wchar_t text[256] = L"";
    CHARRANGE range = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&range);
    require(range.cpMax - range.cpMin < 250, "the selection fits the test's buffer");
    SendMessageW(body, EM_GETSELTEXT, 0, (LPARAM)text);
    bool same =
        wcsstr(text, holds) != nullptr && (lacks == nullptr || wcsstr(text, lacks) == nullptr);
    if (!same)
        printf("open link: selection %ld..%ld (%zu units)\n", range.cpMin, range.cpMax,
               wcslen(text));
    fflush(stdout);
    require(same, message);
}

/* The selection is exactly the paragraph's own text: not its mark, not its neighbours. */
static void expect_link_exact(HWND _Nonnull body, const wchar_t *_Nonnull expected)
{
    wchar_t text[256] = L"";
    CHARRANGE range = {0};
    SendMessageW(body, EM_EXGETSEL, 0, (LPARAM)&range);
    require(range.cpMax - range.cpMin < 250, "the selection fits the test's buffer");
    SendMessageW(body, EM_GETSELTEXT, 0, (LPARAM)text);
    require(wcscmp(text, expected) == 0, "the selection is the paragraph's text and no more");
}

static void open_link_send(struct folio_window *_Nonnull window, const char *_Nonnull link)
{
    folio_window_open_link(window, link, strlen(link));
    pump(window);
}

static void open_link_places(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    open_link_send(window,
                   "nenefolio://open?category=schedule&note=one&lines=5&text=%E7%9B%AE%E5%8D%B0");
    expect_link_selection(body, L"二つ目の段落に目印の文がある。", L"最後",
                          "the paragraph near the given line that holds the text is selected");
    expect_link_exact(body, L"二つ目の段落に目印の文がある。");
    open_link_send(window, "nenefolio://open?category=SCHEDULE&note=One.md&lines=10&text=absent");
    expect_link_selection(body, L"最後の段落にも目印がある。", L"項目",
                          "without the text, the given line's paragraph is selected");
    expect_notice(window, L"指定の文は見つかりませんでした。指定の行を選んでいます。");
    open_link_send(window,
                   "nenefolio://open?category=schedule&note=one&lines=1&text=%E9%A0%85%E7%9B%AEB");
    expect_link_selection(body, L"項目B", L"項目A",
                          "a text found away from the given line is selected where it is");
    expect_notice(window, L"指定の行の近くには無く、ノート内の別の場所の文を選んでいます。");
    open_link_send(window, "nenefolio://open?category=schedule&note=one&lines=7-8");
    expect_link_selection(body, L"項目A", nullptr,
                          "a range of lines selects its paragraphs: the first");
    expect_link_selection(body, L"項目B", L"最後", "and the last, and no more");
    open_link_send(window, "nenefolio://open?category=schedule&note=one&lines=40&text=nowhere");
    expect_notice(window, L"指定の箇所は見つかりませんでした。ノートを開きました。");
}

static void open_link_refusals(struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    open_link_send(window, "nenefolio://open?category=schedule&note=one&lines=3");
    expect_link_selection(body, L"最初の段落。", L"二つ目", "a known place before the refusals");
    open_link_send(window, "nenefolio://open?category=nowhere&note=one&lines=5");
    expect_notice(window, L"リンクのカテゴリが見つかりません。何も開いていません。");
    open_link_send(window, "nenefolio://open?category=schedule&note=nothing&lines=5");
    expect_notice(window, L"リンクのノートが見つかりません。何も開いていません。");
    open_link_send(window, "nenefolio://open?category=schedule&note=..%2Fsecret");
    expect_notice(window, L"リンクの形が正しくないため、開けませんでした。");
    open_link_send(window, "nenefolio://open?category=nowhere&note=one");
    expect_notice(window, L"リンクのカテゴリが見つかりません。何も開いていません。");
    open_link_send(window, "nenefolio://delete?category=schedule&note=one");
    expect_notice(window, L"リンクの形が正しくないため、開けませんでした。");
    expect_link_selection(body, L"最初の段落。", L"二つ目",
                          "a refused link leaves the selection alone");
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "the settings surface opens");
    pump(window);
    open_link_send(window, "nenefolio://open?category=schedule&note=one&lines=10");
    expect_link_selection(body, L"最初の段落。", L"最後",
                          "a link does not open while a surface is open");
    expect_notice(window, L"ほかの操作の途中です。終えてから、もう一度リンクを開いてください。");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    pump(window);
}

/* A second start hands the link to the running window with WM_COPYDATA. */
static LRESULT open_link_hand(struct native_fixture *_Nonnull fixture,
                              struct folio_window *_Nonnull window, bool own,
                              const char *_Nonnull link)
{
    char data[1024];
    const struct storage_path *place = storage_state_directory(fixture->location);
    int written = snprintf(data, sizeof data, "%s%s", storage_path_text(place), own ? "" : "x");
    require(written > 0 && (size_t)written + strlen(link) + 1 < sizeof data, "hand-over fits");
    memcpy(data + written + 1, link, strlen(link));
    COPYDATASTRUCT copy = {.dwData = folio_open_link_data,
                           .cbData = (DWORD)((size_t)written + 1 + strlen(link)),
                           .lpData = data};
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    LRESULT taken = SendMessageW(root, WM_COPYDATA, 0, (LPARAM)&copy);
    pump(window);
    return taken;
}

static void open_link_handed(struct native_fixture *_Nonnull fixture,
                             struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    require(open_link_hand(fixture, window, false,
                           "nenefolio://open?category=schedule&note=one&lines=10") == FALSE,
            "a hand-over for another data folder is not taken");
    expect_link_selection(body, L"最初の段落。", L"最後", "and it changes nothing");
    require(open_link_hand(fixture, window, true,
                           "nenefolio://open?category=schedule&note=one&lines=10") == TRUE,
            "a hand-over for this window's data folder is taken");
    expect_link_selection(body, L"最後の段落にも目印がある。", L"項目", "and it opens the place");
    char unsplit[] = "no separator between the folder and the link";
    COPYDATASTRUCT whole = {
        .dwData = folio_open_link_data, .cbData = (DWORD)(sizeof unsplit - 1), .lpData = unsplit};
    require(SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_COPYDATA, 0,
                         (LPARAM)&whole) == FALSE,
            "a hand-over without the separator is not taken");
    COPYDATASTRUCT other = {.dwData = 1, .cbData = 4, .lpData = "abc"};
    require(SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_COPYDATA, 0,
                         (LPARAM)&other) == FALSE,
            "other WM_COPYDATA is not taken");
}

/* The entry's side: reading the argument, and handing it to the running window. */
static void expect_launch_link(const wchar_t *_Nonnull arguments, const char *_Nullable expected)
{
    struct utf8_text *link = folio_launch_link_read(arguments);
    bool same = expected == nullptr
                    ? link == nullptr
                    : link != nullptr && strcmp(utf8_text_bytes(link), expected) == 0;
    utf8_text_destroy(link);
    require(same, "the launch argument is read as a link, or not at all");
}

static void open_link_launch(struct native_fixture *_Nonnull fixture,
                             struct folio_window *_Nonnull window, HWND _Nonnull body)
{
    expect_launch_link(L"", nullptr);
    expect_launch_link(L"C:\\notes\\file.md", nullptr);
    expect_launch_link(L"--help", nullptr);
    expect_launch_link(L"nenefolio://open?category=a&note=b", "nenefolio://open?category=a&note=b");
    expect_launch_link(L"  \"NeneFolio://open?category=a&note=b\"  ",
                       "NeneFolio://open?category=a&note=b");
    expect_launch_link(L"nenefolio://open?category=仕事&note=b",
                       "nenefolio://open?category=仕事&note=b");
    static const char link[] = "nenefolio://open?category=schedule&note=one&lines=3";
    struct storage_path *elsewhere = nullptr;
    require(storage_path_create("Z:/another/data", 15, &elsewhere) == STORAGE_PATH_READY,
            "another data folder");
    require(!folio_launch_link_hand_over(elsewhere, link, sizeof link - 1),
            "no running window has that data folder: nothing takes the link");
    storage_path_destroy(elsewhere);
    expect_link_selection(body, L"最後の段落にも目印がある。", L"最初", "and nothing changed");
    require(folio_launch_link_hand_over(storage_state_directory(fixture->location), link,
                                        sizeof link - 1),
            "the running window with this data folder takes the link");
    pump(window);
    expect_link_selection(body, L"最初の段落。", L"二つ目", "and opens the place");
}

/* A link to another note while this one has unsaved edits: the edit is saved, as when a note
 * is clicked in the drawer, and the other note opens in the reading view. */
static void open_link_departs(struct native_fixture *_Nonnull fixture,
                              struct folio_window *_Nonnull window)
{
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    open_link_send(window, "nenefolio://open?category=schedule&note=two&text=Other");
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW,
            "the other note is open in the reading view");
    expect_link_selection(folio_window_accessible_window(window, ACCESSIBLE_BODY), L"Other",
                          nullptr, "and its place is selected");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *saved = nullptr;
    require(disk.read_note(disk.adapter, "schedule", "one", &saved) == PERSISTENCE_LOADED &&
                saved != nullptr && strstr(note_text_bytes(saved), "追記") != nullptr,
            "the edit that was open is saved before the link's note opens");
    note_text_destroy(saved);
}

/* The link's note is read again even when it is the open one: the file may have just been
 * written by whoever made the link. */
static void open_link_rereads(struct native_fixture *_Nonnull fixture,
                              struct folio_window *_Nonnull window)
{
    static const char changed[] = "> Other 別ノート 🐾\n\n外で足した段落。\n";
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *body = nullptr;
    require(note_text_create(changed, sizeof changed - 1, &body) == NOTE_TEXT_ACCEPTED &&
                fixture_store(disk, "schedule", "two", body) == NOTE_STORE_STORED,
            "the open note's file is written outside");
    note_text_destroy(body);
    open_link_send(window, "nenefolio://open?category=schedule&note=two&lines=3");
    expect_link_exact(folio_window_accessible_window(window, ACCESSIBLE_BODY), L"外で足した段落。");
}

static void open_link(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "open link language");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "open link window");
    pump(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    puts("open link: places");
    fflush(stdout);
    open_link_places(window, body);
    puts("open link: refusals");
    fflush(stdout);
    open_link_refusals(window, body);
    puts("open link: hand-over");
    fflush(stdout);
    open_link_handed(fixture, window, body);
    puts("open link: the launch argument");
    fflush(stdout);
    open_link_launch(fixture, window, body);
    puts("open link: from the editing view");
    fflush(stdout);
    action(window, ACCESSIBLE_EDIT);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT, "the note is being edited");
    open_link_send(window, "nenefolio://open?category=schedule&note=one&lines=5");
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW,
            "a link shows the note in the reading view");
    expect_link_selection(folio_window_accessible_window(window, ACCESSIBLE_BODY),
                          L"二つ目の段落に目印の文がある。", L"最後",
                          "and selects the place there");
    puts("open link: another note, with unsaved edits here");
    fflush(stdout);
    open_link_departs(fixture, window);
    puts("open link: the open note changed outside");
    fflush(stdout);
    open_link_rereads(fixture, window);
    folio_window_destroy(window);
}

/* #1070 / ADR0119 stage 3: what another program did to the data folder while the window was
 * away is read when the window comes back, once nothing holds the window. */
static void outside_write(struct native_fixture *_Nonnull fixture, const char *_Nonnull note,
                          const char *_Nonnull text)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *body = nullptr;
    require(note_text_create(text, strlen(text), &body) == NOTE_TEXT_ACCEPTED &&
                fixture_store(disk, "schedule", note, body) == NOTE_STORE_STORED,
            "a note's file is written outside");
    note_text_destroy(body);
}

/* Another program deletes the note this case makes. The fixture's folder outlives a run, so the
 * case also starts by doing this. */
static void outside_delete(const struct native_fixture *_Nonnull fixture,
                           const wchar_t *_Nonnull leaf)
{
    wchar_t folder[storage_native_capacity];
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), folder, &length) &&
                swprintf_s(path, storage_native_capacity, L"%ls\\schedule\\%ls", folder, leaf) > 0,
            "outside note path fits");
    (void)DeleteFileW(path);
}

static void outside_remove(const struct native_fixture *_Nonnull fixture)
{
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), path, &length) &&
                length + 32 <= storage_native_capacity,
            "outside note path fits");
    memcpy(path + length, L"\\schedule\\made-outside.md", 26 * sizeof(wchar_t));
    (void)DeleteFileW(path);
}

/* One turn of the message loop's idle work, as the loop gives it when the queue is empty. */
static void outside_idle(struct folio_window *_Nonnull window)
{
    pump(window);
    if (folio_window_idle_work_ready(window))
        folio_window_continue_idle_work(window);
    pump(window);
}

/* The user goes to another application and comes back. */
static void outside_return(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SendMessageW(root, WM_ACTIVATEAPP, FALSE, 0);
    SendMessageW(root, WM_ACTIVATEAPP, TRUE, 0);
    outside_idle(window);
}

static bool outside_shows(struct folio_window *_Nonnull window, const wchar_t *_Nonnull text)
{
    static wchar_t shown[16384];
    shown[0] = L'\0';
    GetWindowTextW(folio_window_accessible_window(window, ACCESSIBLE_BODY), shown,
                   (int)(sizeof shown / sizeof shown[0]));
    return wcsstr(shown, text) != nullptr;
}

/* Whether the status line says exactly this. */
static bool outside_told(struct folio_window *_Nonnull window, const wchar_t *_Nonnull line)
{
    struct accessible_item item = {.kind = ACCESSIBLE_NOTICE};
    struct accessible_view view;
    return folio_window_accessible_read(window, &item, &view) == S_OK &&
           wcscmp(view.name, line) == 0;
}

/* The reading view reads the changed file again and stays where it was being read. */
static void outside_reading(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window)
{
    static char text[8192];
    size_t used = (size_t)snprintf(text, sizeof text, "# 長いノート\n\n");
    for (int line = 1; line <= 150; ++line)
        used += (size_t)snprintf(text + used, sizeof text - used, "段落 %d\n\n", line);
    outside_return(window);
    require(outside_shows(window, L"最初の段落。") && !outside_shows(window, L"段落 150"),
            "nothing changed outside: the note is as it was");
    outside_write(fixture, "one", text);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    SendMessageW(root, WM_ACTIVATEAPP, TRUE, 0);
    outside_idle(window);
    require(!outside_shows(window, L"段落 150"),
            "an activation that follows no leaving, as at the start, reads nothing");
    SendMessageW(root, WM_ACTIVATEAPP, FALSE, 0);
    outside_idle(window);
    require(!outside_shows(window, L"段落 150"), "nothing is read while the window is away");
    SendMessageW(root, WM_ACTIVATEAPP, TRUE, 0);
    outside_idle(window);
    require(outside_shows(window, L"段落 150"), "the changed file is read on the way back");
    expect_notice(window, L"外部の変更を読み込みました。");
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    SendMessageW(body, EM_LINESCROLL, 0, 40);
    pump(window);
    LRESULT first = SendMessageW(body, EM_GETFIRSTVISIBLELINE, 0, 0);
    require(first > 0, "the long note is scrolled");
    (void)snprintf(text + used, sizeof text - used, "末尾に足した段落。\n");
    outside_write(fixture, "one", text);
    outside_return(window);
    require(outside_shows(window, L"末尾に足した段落。") &&
                SendMessageW(body, EM_GETFIRSTVISIBLELINE, 0, 0) == first,
            "the note is read again and stays where it was being read");
}

/* A note made outside is in the list on the way back; nothing is read while a surface is open,
 * and the reading is not dropped: it follows when the surface closes. */
static void outside_lists(struct native_fixture *_Nonnull fixture,
                          struct folio_window *_Nonnull window)
{
    outside_remove(fixture);
    outside_return(window);
    size_t notes = folio_state_note_count(fixture->state);
    outside_write(fixture, "made-outside", "外で作ったノート。\n");
    SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_WINDOW), WM_ACTIVATEAPP, FALSE,
                 0);
    outside_idle(window);
    require(folio_state_note_count(fixture->state) == notes,
            "the list is as it was while the window is away");
    outside_return(window);
    require(!folio_window_idle_work_ready(window) ||
                folio_state_note_count(fixture->state) == notes + 1,
            "the reading ran in that one turn");
    require(folio_state_note_count(fixture->state) == notes + 1,
            "a note made outside is in the list on the way back");
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "the settings surface opens");
    pump(window);
    outside_write(fixture, "one", "# 面のあいだ\n\n面が開いているあいだに外で書いた。\n");
    outside_return(window);
    require(!outside_shows(window, L"面が開いているあいだに外で書いた。"),
            "nothing is read again while a surface is open");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    outside_idle(window);
    require(outside_shows(window, L"面が開いているあいだに外で書いた。"),
            "and the reading follows when the surface closes");
    outside_remove(fixture);
    outside_return(window);
    require(folio_state_note_count(fixture->state) == notes,
            "a note deleted outside leaves the list");
}

/* Lets the window's timers fire until the gathered notices have settled (the timer that ends
 * the gathering has fired), then gives the loop's idle turn. False when it never settled. */
static bool outside_gathered(struct folio_window *_Nonnull window)
{
    MSG message;
    for (unsigned turn = 0; turn < 100 && folio_window_data_settling(window); ++turn)
    {
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 100, QS_TIMER);
        while (PeekMessageW(&message, nullptr, WM_TIMER, WM_TIMER, PM_REMOVE))
            DispatchMessageW(&message);
    }
    bool settled = !folio_window_data_settling(window);
    outside_idle(window);
    return settled;
}

/* #1070 / ADR0119 decision 6: a request file as another program writes it. */
static void outside_request(const struct native_fixture *_Nonnull fixture,
                            const wchar_t *_Nonnull leaf, const char *_Nonnull json)
{
    wchar_t root[storage_native_capacity];
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), root, &length) &&
                swprintf_s(path, storage_native_capacity, L"%ls\\.requests", root) > 0,
            "requests folder path fits");
    require(CreateDirectoryW(path, nullptr) != 0 || GetLastError() == ERROR_ALREADY_EXISTS,
            "the requests folder");
    require(swprintf_s(path, storage_native_capacity, L"%ls\\.requests\\%ls", root, leaf) > 0,
            "request path fits");
    HANDLE file =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "the request file is created");
    DWORD written = 0;
    bool stored = WriteFile(file, json, (DWORD)strlen(json), &written, nullptr) != 0;
    CloseHandle(file);
    require(stored && written == strlen(json), "the request file is written");
}

/* The fixture's folder outlives a run: request files an earlier, failed run left are removed. */
static void outside_requests_cleared(const struct native_fixture *_Nonnull fixture)
{
    static const wchar_t *const leaves[] = {L"open-1.json", L"open-2.json", L"open-3.json",
                                            L"open-4.json", L"open-5.json", L"open-6.json"};
    wchar_t root[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), root, &length),
            "requests folder path fits");
    for (size_t at = 0; at < sizeof leaves / sizeof leaves[0]; ++at)
    {
        wchar_t path[storage_native_capacity];
        if (swprintf_s(path, storage_native_capacity, L"%ls\\.requests\\%ls", root, leaves[at]) > 0)
            (void)DeleteFileW(path);
    }
}

static bool outside_request_waits(const struct native_fixture *_Nonnull fixture,
                                  const wchar_t *_Nonnull leaf)
{
    wchar_t root[storage_native_capacity];
    wchar_t path[storage_native_capacity];
    size_t length = 0;
    require(storage_native_path_fill(storage_state_directory(fixture->location), root, &length) &&
                swprintf_s(path, storage_native_capacity, L"%ls\\.requests\\%ls", root, leaf) > 0,
            "request path fits");
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

/* A request file opens its note and selects its place, like a link; one that is no request is
 * removed and opens nothing; while a surface is open the file waits. */
static void outside_requested(struct native_fixture *_Nonnull fixture,
                              struct folio_window *_Nonnull window)
{
    outside_request(fixture, L"open-1.json",
                    "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\","
                    "\"note\":\"two\",\"text\":\"Other\"}");
    folio_window_data_changed(window);
    require(outside_gathered(window) &&
                strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                !outside_request_waits(fixture, L"open-1.json"),
            "the request's note opens in the reading view and the file is removed");
    expect_link_selection(folio_window_accessible_window(window, ACCESSIBLE_BODY), L"Other",
                          nullptr, "and its place is selected");
    outside_request(fixture, L"open-2.json", "{\"version\":1,\"action\":\"erase\"}");
    folio_window_data_changed(window);
    require(outside_gathered(window) &&
                strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                !outside_request_waits(fixture, L"open-2.json"),
            "a file that is no request is removed and opens nothing");
    struct accessible_request settings = {.item = {.kind = ACCESSIBLE_SETTINGS},
                                          .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &settings) == S_OK,
            "the settings surface opens");
    pump(window);
    outside_request(fixture, L"open-3.json",
                    "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\","
                    "\"note\":\"one\"}");
    folio_window_data_changed(window);
    require(outside_gathered(window) &&
                strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                outside_request_waits(fixture, L"open-3.json"),
            "while a surface is open the request waits, file and all");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    outside_idle(window);
    outside_idle(window);
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                !outside_request_waits(fixture, L"open-3.json") &&
                !folio_window_idle_work_ready(window),
            "and is opened when the surface closes; then the window is at rest");
    outside_request(fixture, L"open-4.json", "{}");
    outside_request(fixture, L"open-5.json", "[]");
    outside_request(fixture, L"open-6.json", "not json");
    folio_window_data_changed(window);
    bool gathered = outside_gathered(window);
    for (unsigned turn = 0; turn < 8; ++turn)
        outside_idle(window);
    require(gathered && !outside_request_waits(fixture, L"open-4.json") &&
                !outside_request_waits(fixture, L"open-5.json") &&
                !outside_request_waits(fixture, L"open-6.json") &&
                !folio_window_idle_work_ready(window),
            "several files are taken one after another, a quiet turn each, until none is left");
}

/* #1070 / ADR0119 stage 4: the data folder changed while the window stayed in front. The
 * notices are gathered, and the reading follows once, when the window is quiet. */
static void outside_watched(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window)
{
    outside_write(fixture, "one", "# 見張り\n\n前面のまま外で書いた。\n");
    folio_window_data_changed(window);
    folio_window_data_changed(window);
    outside_idle(window);
    require(folio_window_data_settling(window) && !outside_shows(window, L"前面のまま外で書いた。"),
            "a notice alone reads nothing: the notices are gathered first");
    require(outside_gathered(window) && outside_shows(window, L"前面のまま外で書いた。"),
            "after the notices settle, the changed note is read without leaving the window");
    expect_notice(window, L"外部の変更を読み込みました。");
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    (void)native_key_input(window, 'S', FCONTROL);
    pump(window);
    folio_window_data_changed(window);
    require(outside_gathered(window) && folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"追記") &&
                !outside_told(window,
                              L"このノートは外部で変更されました。編集中の本文はそのままです。"),
            "the notice raised by the app's own save is not an outside change");
    action(window, ACCESSIBLE_VIEW);
}

/* Entering the edit reads a changed file first; an edit that is open is only told. */
static void outside_editing(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window)
{
    outside_write(fixture, "one", "# 編集の前\n\n編集に入る前に外で書いた。\n");
    action(window, ACCESSIBLE_EDIT);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"編集に入る前に外で書いた。"),
            "the edit starts from the file as it is now");
    expect_notice(window, L"外部の変更を読み込みました。");
    outside_write(fixture, "one", "# 編集なし\n\n何も打たずに閲覧へ戻った。\n");
    action(window, ACCESSIBLE_VIEW);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                outside_shows(window, L"何も打たずに閲覧へ戻った。"),
            "back in the reading view without edits, the changed file is read at once");
    expect_notice(window, L"外部の変更を読み込みました。");
    action(window, ACCESSIBLE_EDIT);
    outside_write(fixture, "one", "# リンク\n\n外で書いてからリンクで開く。\n\n次の段落。\n");
    open_link_send(window, "nenefolio://open?category=schedule&note=one&lines=3");
    expect_link_exact(folio_window_accessible_window(window, ACCESSIBLE_BODY),
                      L"外で書いてからリンクで開く。");
    outside_idle(window);
    expect_link_exact(folio_window_accessible_window(window, ACCESSIBLE_BODY),
                      L"外で書いてからリンクで開く。");
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    action(window, ACCESSIBLE_VIEW);
    outside_idle(window);
    require(outside_shows(window, L"追記") &&
                !outside_told(window, L"外部の変更を読み込みました。"),
            "the app's own save is not a change from outside");
    action(window, ACCESSIBLE_EDIT);
    append_switch_text(window);
    outside_write(fixture, "one", "# 編集中\n\n編集中に外で書いた。\n");
    outside_return(window);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"追記") && !outside_shows(window, L"編集中に外で書いた。") &&
                SendMessageW(body, EM_CANUNDO, 0, 0) != 0,
            "an open edit keeps its text and its Undo");
    expect_notice(window, L"このノートは外部で変更されました。編集中の本文はそのままです。");
}

/* ADR0119 decision 4 in the window: a save that an outside change stopped keeps the edit, the
 * selection and the window, and says what to do next. Comes in with an unsaved edit of "one"
 * whose file was written outside. */
static void outside_stopped(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window)
{
    static const wchar_t stopped[] =
        L"外部で変更されたため、保存していません。別名で保存するか、編集を破棄して読み直せます。";
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    (void)native_key_input(window, 'S', FCONTROL);
    pump(window);
    require(outside_told(window, stopped), "the save says why it stopped and what can be done");
    open_link_send(window, "nenefolio://open?category=schedule&note=two");
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                outside_told(window, stopped),
            "another note does not open over the edit that could not be saved");
    SendMessageW(root, WM_CLOSE, 0, 0);
    pump(window);
    require(IsWindow(root) && outside_told(window, stopped), "closing stops too; the window stays");
    require(SendMessageW(root, WM_QUERYENDSESSION, 0, (LPARAM)ENDSESSION_LOGOFF) == FALSE,
            "the end of the Windows session is refused, not lost");
    SendMessageW(root, WM_ENDSESSION, FALSE, (LPARAM)ENDSESSION_LOGOFF);
    pump(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"追記") && SendMessageW(body, EM_CANUNDO, 0, 0) != 0,
            "through all of it the edit keeps its text and its Undo");
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *file = nullptr;
    require(disk.read_note(disk.adapter, "schedule", "one", &file) == PERSISTENCE_LOADED &&
                file != nullptr && strstr(note_text_bytes(file), "編集中に外で書いた。") != nullptr,
            "and the file keeps the outside version");
    note_text_destroy(file);
}

/* The way out the line names: discarding reads the file, and the next save goes through. */
static void outside_discarded(struct native_fixture *_Nonnull fixture,
                              struct folio_window *_Nonnull window)
{
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"編集中に外で書いた。") && !outside_shows(window, L"追記"),
            "discarding shows the outside version, still in the editing view");
    expect_notice(window, L"外部の変更を読み込みました。");
    append_switch_text(window);
    (void)native_key_input(window, 'S', FCONTROL);
    pump(window);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *file = nullptr;
    require(disk.read_note(disk.adapter, "schedule", "one", &file) == PERSISTENCE_LOADED &&
                file != nullptr &&
                strstr(note_text_bytes(file), "編集中に外で書いた。") != nullptr &&
                strstr(note_text_bytes(file), "追記") != nullptr,
            "the next save, made from the outside version, is written");
    note_text_destroy(file);
    append_switch_text(window);
    outside_delete(fixture, L"one.md");
    invoke_operation(window, FOLIO_COMMAND_DISCARD_EDITS);
    pump(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"編集中に外で書いた。"),
            "with the file gone, discarding goes back to the text last saved");
    expect_notice(window, L"ファイルを読み直せなかったため、最後に読んだ本文に戻しました。");
}

/* #1152 / ADR0119: a request file arrives while a named note is edited with unsaved changes. The
 * window asks on a modal surface with three stacked choices. An observer on a thread timer finds
 * the surface, checks it, and answers it; the request files are the ones another program writes. */
constexpr size_t request_step_limit = 4;
struct request_step
{
    int identity; /* the button to answer with: IDOK, 3 (keep) or IDCANCEL */
    bool by_key;  /* give that button the keyboard and press Enter, instead of clicking it */
    bool pile;    /* before answering, two more requests arrive: an older and a newer one */
    /* The user was typing when the surface appeared: a letter, a space and Enter reach it at once.
     * They must not answer it. A second later Enter does (the real key, through the dialog). */
    bool typing;
    /* While the surface is open the main window shrinks to its smallest and grows back: the
     * explanations fold away and come back, and the choices stay inside the surface throughout. */
    bool resizing;
    /* The surface is saved under this name beside the test, for looking at. Null: no picture. */
    const char *_Nullable picture;
};
static struct
{
    HWND root;
    UINT_PTR timer;
    const struct native_fixture *_Nullable fixture;
    struct request_step steps[request_step_limit];
    size_t count;
    size_t answered;
    unsigned ticks;
    unsigned typed;     /* observer ticks since the keys of a typing step were posted; 0: not yet */
    bool explained;     /* the last surface showed the line under each choice */
    unsigned stage;     /* how far a resizing step has come */
    LONG opened_height; /* the surface's height when a resizing step first saw it */
    RECT root_before;   /* the main window before a resizing step made it small */
    LONG height;        /* the last surface's height */
    wchar_t message[512];
    wchar_t keep_hint[512];
} request_dialog;

static void request_answer(HWND dialog, struct request_step step)
{
    if (step.pile)
    {
        outside_request(request_dialog.fixture, L"open-older.json",
                        "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\","
                        "\"note\":\"one\"}");
        Sleep(30); /* the two files must differ in their time of writing */
        outside_request(request_dialog.fixture, L"open-newer.json",
                        "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\","
                        "\"note\":\"no such note\"}");
    }
    HWND button = GetDlgItem(dialog, step.identity);
    require(button != nullptr, "the surface has the button to answer with");
    if (!step.by_key)
    {
        SendMessageW(button, BM_CLICK, 0, 0);
        return;
    }
    if (step.identity == IDCANCEL)
    {
        SendMessageW(dialog, WM_COMMAND, IDCANCEL, 0); /* what Esc sends to a dialog */
        return;
    }
    SetFocus(button);
    SendMessageW(dialog, WM_COMMAND, MAKEWPARAM(IDOK, BN_CLICKED), 0); /* what Enter sends */
}

/* The surface as it is painted, for looking at (request-prompt.bmp beside the test). */
static void request_pixels(HWND dialog, const char *_Nonnull path)
{
    RECT client;
    GetClientRect(dialog, &client);
    HDC source = GetDC(dialog);
    HDC dc = CreateCompatibleDC(source);
    HBITMAP bitmap = CreateCompatibleBitmap(source, client.right, client.bottom);
    HGDIOBJ old = SelectObject(dc, bitmap);
    require(PrintWindow(dialog, dc, PW_CLIENTONLY), "render the request surface");
    SelectObject(dc, old);
    save_bitmap(dc, bitmap, client, path);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(dialog, source);
}

/* The keys a typing user would still be sending. True once a second has passed since. */
static bool request_typed(HWND dialog)
{
    if (request_dialog.typed == 0)
    {
        HWND focus = GetFocus();
        require(focus == GetDlgItem(dialog, IDOK), "the keys go to the surface's first button");
        PostMessageW(focus, WM_CHAR, L's', 0);
        PostMessageW(focus, WM_KEYDOWN, VK_SPACE, 0);
        PostMessageW(focus, WM_KEYUP, VK_SPACE, 0);
        PostMessageW(focus, WM_KEYDOWN, VK_RETURN, 0);
        PostMessageW(focus, WM_KEYDOWN, VK_ESCAPE, 0);
    }
    return ++request_dialog.typed > 22; /* 22 ticks of 50 ms: past the 700 ms the surface waits */
}

/* What every sighting of the surface checks and remembers: three stacked choices inside it, the
 * keyboard on saving, and the texts. */
static void request_looked(HWND dialog)
{
    HWND message = GetWindow(dialog, GW_CHILD);
    HWND keep = GetDlgItem(dialog, 3);
    require(message != nullptr && GetDlgItem(dialog, IDOK) != nullptr && keep != nullptr &&
                GetDlgItem(dialog, IDCANCEL) != nullptr && GetFocus() == GetDlgItem(dialog, IDOK),
            "three choices, with the keyboard on saving");
    GetWindowTextW(message, request_dialog.message, 512);
    GetWindowTextW(GetWindow(keep, GW_HWNDNEXT), request_dialog.keep_hint, 512);
    RECT save_bounds;
    RECT keep_bounds;
    GetWindowRect(GetDlgItem(dialog, IDOK), &save_bounds);
    GetWindowRect(keep, &keep_bounds);
    require(keep_bounds.top > save_bounds.bottom && keep_bounds.left == save_bounds.left,
            "the choices are stacked, one under the other");
    RECT surface;
    RECT decline_bounds;
    GetWindowRect(dialog, &surface);
    GetWindowRect(GetDlgItem(dialog, IDCANCEL), &decline_bounds);
    require(decline_bounds.bottom <= surface.bottom && save_bounds.top >= surface.top,
            "every choice is inside the surface");
    unsigned explanations = 0;
    static const int choices[] = {IDOK, 3, IDCANCEL};
    for (size_t at = 0; at < 3; ++at)
        explanations +=
            IsWindowVisible(GetWindow(GetDlgItem(dialog, choices[at]), GW_HWNDNEXT)) ? 1U : 0U;
    require(explanations == 0 || explanations == 3, "the explanations show or fold together");
    request_dialog.explained = explanations == 3;
    RECT root_bounds;
    GetWindowRect(GetAncestor(dialog, GA_ROOTOWNER), &root_bounds);
    require(surface.top >= root_bounds.top && surface.bottom <= root_bounds.bottom,
            "the surface is inside the main window");
    request_dialog.height = surface.bottom - surface.top;
}

/* One tick of a resizing step. True once the window is back at its size and the step may answer. */
static bool request_resized(HWND dialog)
{
    HWND root = GetAncestor(dialog, GA_ROOTOWNER);
    RECT *before = &request_dialog.root_before;
    switch (request_dialog.stage++)
    {
    case 0:
        require(request_dialog.explained, "the surface opens with its explanations");
        request_dialog.opened_height = request_dialog.height;
        GetWindowRect(root, before);
        SetWindowPos(root, nullptr, 0, 0, before->right - before->left, 1,
                     SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return false;
    case 1:
        require(!request_dialog.explained &&
                    request_dialog.height * 4 < request_dialog.opened_height * 3,
                "the open surface folds when the window shrinks");
        SetWindowPos(root, nullptr, 0, 0, before->right - before->left,
                     before->bottom - before->top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        return false;
    default:
        require(request_dialog.explained && request_dialog.height == request_dialog.opened_height,
                "the surface is as it was when the window grows back");
        return true;
    }
}

static BOOL CALLBACK request_found(HWND dialog, LPARAM parameter)
{
    wchar_t kind[32];
    if (GetAncestor(dialog, GA_ROOTOWNER) != (HWND)parameter || !IsWindowVisible(dialog) ||
        GetClassNameW(dialog, kind, 32) == 0 || wcscmp(kind, L"#32770") != 0)
        return TRUE;
    require(request_dialog.answered < request_dialog.count, "no surface beyond the expected ones");
    request_looked(dialog);
    struct request_step step = request_dialog.steps[request_dialog.answered];
    if (step.picture != nullptr && request_dialog.typed == 0)
        request_pixels(dialog, step.picture);
    if (step.typing && !request_typed(dialog))
        return FALSE; /* still inside the second in which keys are not taken */
    if (step.resizing && !request_resized(dialog))
        return FALSE; /* the window is at another size until the next tick */
    ++request_dialog.answered;
    if (step.typing)
        PostMessageW(GetFocus(), WM_KEYDOWN, VK_RETURN, 0);
    else
        request_answer(dialog, step);
    return FALSE;
}

static void CALLBACK request_observe(HWND window, UINT message, UINT_PTR timer, DWORD tick_millis)
{
    (void)window;
    (void)message;
    (void)tick_millis;
    if (request_dialog.timer == 0 || timer != request_dialog.timer)
        return;
    require(++request_dialog.ticks <= 200,
            "the request surface observer stays within its deadline");
    EnumThreadWindows(GetCurrentThreadId(), request_found, (LPARAM)request_dialog.root);
}

static void request_expect(struct folio_window *_Nonnull window,
                           const struct native_fixture *_Nonnull fixture,
                           const struct request_step *_Nonnull steps, size_t count)
{
    request_dialog.root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    request_dialog.fixture = fixture;
    request_dialog.count = count;
    request_dialog.answered = 0;
    request_dialog.ticks = 0;
    request_dialog.typed = 0;
    request_dialog.stage = 0;
    request_dialog.message[0] = L'\0';
    request_dialog.keep_hint[0] = L'\0';
    for (size_t at = 0; at < count; ++at)
        request_dialog.steps[at] = steps[at];
    request_dialog.timer = SetTimer(nullptr, 0, 50, request_observe);
    require(request_dialog.timer != 0, "arm the request surface observer");
}

static void request_settled(struct folio_window *_Nonnull window, size_t answered)
{
    for (unsigned turn = 0; turn < 8; ++turn)
        outside_idle(window);
    require(KillTimer(nullptr, request_dialog.timer), "stop the request surface observer");
    request_dialog.timer = 0;
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(request_dialog.answered == answered && IsWindowEnabled(root) &&
                !folio_window_idle_work_ready(window),
            "the expected surfaces were answered and the window is at rest");
}

/* Writes a request for a note of the test category and lets the window notice it. */
static void request_arrives(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window, const char *_Nonnull json)
{
    outside_request(fixture, L"open-1.json", json);
    folio_window_data_changed(window);
    require(outside_gathered(window), "the request is noticed");
}

static bool request_file_holds(struct native_fixture *_Nonnull fixture, const char *_Nonnull note,
                               const char *_Nonnull text)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *file = nullptr;
    bool held = disk.read_note(disk.adapter, "schedule", note, &file) == PERSISTENCE_LOADED &&
                file != nullptr && strstr(note_text_bytes(file), text) != nullptr;
    note_text_destroy(file);
    return held;
}

static void request_edit(struct folio_window *_Nonnull window,
                         const struct native_fixture *_Nonnull fixture)
{
    if (folio_state_pane_mode(fixture->state) != PANE_MODE_EDIT)
        action(window, ACCESSIBLE_EDIT);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT, "the note is being edited");
}

/* A note whose name is 150 letters, and the request to open it. The note is made for the one
 * step that needs a long message and is removed again; the request is only ever declined. */
#define REQUEST_LONG_NAME                                                                          \
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"                                           \
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"                                           \
    "WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW"
static const char request_long[] =
    "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\",\"note\":\"" REQUEST_LONG_NAME
    "\"}";
static const char request_one[] =
    "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\",\"note\":\"one\"}";
static const char request_two[] =
    "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\",\"note\":\"two\"}";

/* Saving and opening, and no question at all when nothing is unsaved. */
static void request_saved(struct native_fixture *_Nonnull fixture,
                          struct folio_window *_Nonnull window)
{
    request_edit(window, fixture);
    static const struct request_step none[1] = {{.identity = IDOK}};
    request_expect(window, fixture, none, 0);
    request_arrives(fixture, window, request_two);
    request_settled(window, 0);
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0,
            "an edited note without unsaved changes is left without a question");
    request_edit(window, fixture);
    append_switch_text(window);
    static const struct request_step save[] = {
        {.identity = IDOK, .typing = true, .picture = "request-prompt.bmp"}};
    request_expect(window, fixture, save, 1);
    request_arrives(fixture, window, request_one);
    request_settled(window, 1);
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                request_file_holds(fixture, "two", "追記") &&
                !outside_request_waits(fixture, L"open-1.json"),
            "saving and opening writes the edited note, then opens the requested one");
    require(request_dialog.typed > 22,
            "the keys typed as the surface appeared did not answer it; Enter did, a second later");
    require(wcsstr(request_dialog.message, L"one") != nullptr &&
                wcsstr(request_dialog.message, L"two") != nullptr &&
                wcsstr(request_dialog.keep_hint, L"1_two") != nullptr,
            "the surface names the requested note, the edited note and the name it would keep");
}

/* Keeping the edited body under a numbered name: the note's own file is not written. */
static void request_kept(struct native_fixture *_Nonnull fixture,
                         struct folio_window *_Nonnull window)
{
    request_edit(window, fixture);
    require(!request_file_holds(fixture, "one", "追記"), "the note has no addition yet");
    append_switch_text(window);
    size_t notes = folio_state_note_count(fixture->state);
    static const struct request_step keep[] = {{.identity = 3, .by_key = true}};
    request_expect(window, fixture, keep, 1);
    request_arrives(fixture, window, request_two);
    request_settled(window, 1);
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_note_count(fixture->state) == notes + 1 &&
                request_file_holds(fixture, "1_one", "追記") &&
                !request_file_holds(fixture, "one", "追記"),
            "the edited body is kept as 1_one, the file of one is untouched, and two is open");
    require(outside_told(window, L"編集中の本文を「1_one」として残しました。"),
            "the status line says under which name the body was kept");
}

/* Declining, and the requests that pile up while the surface is asking. */
static void request_declined(struct native_fixture *_Nonnull fixture,
                             struct folio_window *_Nonnull window)
{
    request_edit(window, fixture);
    append_switch_text(window);
    static const struct request_step decline[] = {{.identity = IDCANCEL, .by_key = true}};
    request_expect(window, fixture, decline, 1);
    request_arrives(fixture, window, request_one);
    request_settled(window, 1);
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                outside_shows(window, L"追記") &&
                !request_file_holds(fixture, "two", "追記 🙂 追記") &&
                !outside_request_waits(fixture, L"open-1.json"),
            "declining keeps the note, its unsaved text and the editing view; the request is gone");
    /* Still unsaved. A request arrives; while the surface asks, an older one (for the note one)
     * and a newer one (for a note that does not exist) arrive. The first is declined. Of the two
     * that piled up only the newer is asked about: its note is not found, so nothing is opened and
     * nothing is saved. Had the older one been handled too, the note one would be open now. */
    static const struct request_step piled[] = {{.identity = IDCANCEL, .pile = true},
                                                {.identity = IDOK}};
    request_expect(window, fixture, piled, 2);
    request_arrives(fixture, window, request_one);
    request_settled(window, 2);
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                !outside_request_waits(fixture, L"open-older.json") &&
                !outside_request_waits(fixture, L"open-newer.json") &&
                !request_file_holds(fixture, "two", "追記 🙂 追記") &&
                wcsstr(request_dialog.message, L"no such note") != nullptr,
            "of the requests that piled up, only the newest is handled; the older one is dropped");
    require(outside_told(window, L"リンクのノートが見つかりません。何も開いていません。"),
            "the newest request names a note that is not there, and the line says so");
    /* Keeping aside for a request whose note is not there: the body is kept, nothing is opened,
     * and the line still says that the note was not found (the kept name does not replace it). */
    static const struct request_step keep[] = {{.identity = 3}};
    request_expect(window, fixture, keep, 1);
    request_arrives(fixture, window,
                    "{\"version\":1,\"action\":\"open\",\"category\":\"schedule\","
                    "\"note\":\"no such note\"}");
    request_settled(window, 1);
    require(strcmp(folio_state_document_name(fixture->state), "1_two") == 0 &&
                request_file_holds(fixture, "1_two", "追記 🙂 追記") &&
                !request_file_holds(fixture, "two", "追記 🙂 追記") &&
                outside_told(window, L"リンクのノートが見つかりません。何も開いていません。"),
            "the body is kept as 1_two and stays open; the line says the note was not found");
}

/* The notes this case makes are taken out again, file and ledger row: the data folder is shared
 * with the other cases, which count its rows. */
static void request_forget(struct native_fixture *_Nonnull fixture, const wchar_t *_Nonnull leaf,
                           const char *_Nonnull note)
{
    outside_delete(fixture, leaf);
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_ledger *ledger = nullptr;
    if (disk.read_note_ledger(disk.adapter, "schedule", &ledger) != PERSISTENCE_LOADED ||
        ledger == nullptr)
        return;
    for (size_t index = 0; index < note_ledger_count(ledger); ++index)
    {
        if (strcmp(note_ledger_name(ledger, index), note) != 0)
            continue;
        struct note_ledger *without = nullptr;
        require(note_ledger_removed(ledger, index, &without) == NOTE_LEDGER_ACCEPTED &&
                    without != nullptr &&
                    disk.write_note_ledger(disk.adapter, "schedule", without) == PERSISTENCE_STORED,
                "the kept note's ledger row is removed");
        note_ledger_destroy(without);
        break;
    }
    note_ledger_destroy(ledger);
}

/* The surface where it does not fit: the smallest window, a very long name, and a window
 * that changes its size while the surface is open. `explained_height` is the height of the
 * surface with its explanations. */
static void request_cramped(struct native_fixture *_Nonnull fixture,
                            struct folio_window *_Nonnull window, LONG explained_height)
{
    /* The window at its smallest: the surface would not fit with the explanations, so they are
     * folded away and the three choices stay inside it. */
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    RECT before;
    GetWindowRect(root, &before);
    SetWindowPos(root, nullptr, 0, 0, before.right - before.left, 1,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(window);
    static const struct request_step smallest = {.identity = IDCANCEL,
                                                 .picture = "request-prompt-small.bmp"};
    request_expect(window, fixture, &smallest, 1);
    request_arrives(fixture, window, request_one);
    request_settled(window, 1);
    RECT smallest_root;
    GetWindowRect(root, &smallest_root);
    require(smallest_root.bottom - smallest_root.top < before.bottom - before.top,
            "the window did get smaller");
    require(!request_dialog.explained && request_dialog.height * 4 < explained_height * 3,
            "in the smallest window the explanations are folded, the surface is lower by their "
            "lines, and the choices all show");
    /* A name so long that the message wraps over many lines: folding is not enough, so the
     * message gives up its last lines and the choices still stay inside the surface. */
    static const struct request_step crowded = {.identity = IDCANCEL,
                                                .picture = "request-prompt-crowded.bmp"};
    outside_write(fixture, REQUEST_LONG_NAME, "長い名前のノート。\n");
    request_expect(window, fixture, &crowded, 1);
    request_arrives(fixture, window, request_long);
    request_settled(window, 1);
    request_forget(fixture, L"" REQUEST_LONG_NAME L".md", REQUEST_LONG_NAME);
    require(!request_dialog.explained && wcsstr(request_dialog.message, L"WWWWWWWW") != nullptr,
            "a long name folds the explanations and the choices stay inside the surface");
    SetWindowPos(root, nullptr, 0, 0, before.right - before.left, before.bottom - before.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    pump(window);
    /* The surface stays open while the window shrinks and grows back. */
    static const struct request_step resized = {.identity = IDCANCEL, .resizing = true};
    request_expect(window, fixture, &resized, 1);
    request_arrives(fixture, window, request_one);
    request_settled(window, 1);
    require(request_dialog.explained && request_dialog.stage == 3,
            "the surface went through shrinking and growing before it was answered");
}

/* The surface in the other languages and themes, declined each time: pictures to look at, and the
 * check that it can be built and answered in each. The note stays as it is. */
static void request_pictured(struct native_fixture *_Nonnull fixture,
                             struct folio_window *_Nonnull window)
{
    static const struct
    {
        enum folio_language language;
        enum folio_theme_choice theme;
        struct request_step step;
    } looks[] = {
        {FOLIO_LANGUAGE_JA,
         FOLIO_THEME_CHOICE_LIGHT,
         {.identity = IDCANCEL, .picture = "request-prompt-ja-light.bmp"}},
        {FOLIO_LANGUAGE_EN,
         FOLIO_THEME_CHOICE_LIGHT,
         {.identity = IDCANCEL, .picture = "request-prompt-en-light.bmp"}},
        {FOLIO_LANGUAGE_ZH_HANS,
         FOLIO_THEME_CHOICE_DARK,
         {.identity = IDCANCEL, .picture = "request-prompt-zh-dark.bmp"}},
    };
    request_edit(window, fixture);
    append_switch_text(window);
    for (size_t at = 0; at < sizeof looks / sizeof looks[0]; ++at)
    {
        require(folio_state_set_language(fixture->state, looks[at].language) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, looks[at].theme) == FOLIO_STATE_READY,
                "language and theme for the picture");
        pump(window);
        request_expect(window, fixture, &looks[at].step, 1);
        request_arrives(fixture, window, request_one);
        request_settled(window, 1);
        require(request_dialog.message[0] != L'\0' && request_dialog.keep_hint[0] != L'\0' &&
                    outside_shows(window, L"追記"),
                "the surface is built and declined in this language and theme");
    }
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "back to the language the other steps expect");
    require(request_dialog.explained, "with room to spare each choice has its line of explanation");
    request_cramped(fixture, window, request_dialog.height);
}

static void request_prompt(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "request prompt language");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "request prompt window");
    outside_requests_cleared(fixture);
    request_forget(fixture, L"1_one.md", "1_one"); /* what a run that stopped early left behind */
    request_forget(fixture, L"1_two.md", "1_two");
    folio_window_data_changed(window);
    require(outside_gathered(window), "the test category is read again");
    pump(window);
    open_link_send(window, "nenefolio://open?category=schedule&note=one");
    puts("request prompt: save and open");
    fflush(stdout);
    request_saved(fixture, window);
    puts("request prompt: keep under another name");
    fflush(stdout);
    request_kept(fixture, window);
    puts("request prompt: decline, and requests that pile up");
    fflush(stdout);
    request_declined(fixture, window);
    puts("request prompt: the other languages and themes");
    fflush(stdout);
    request_pictured(fixture, window);
    folio_window_destroy(window);
    request_forget(fixture, L"1_one.md", "1_one");
    request_forget(fixture, L"1_two.md", "1_two");
}

static void outside_refresh(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY,
            "outside refresh language");
    struct folio_window *window = nullptr;
    require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY, &window) ==
                FOLIO_WINDOW_CREATED,
            "outside refresh window");
    outside_requests_cleared(fixture);
    pump(window);
    open_link_send(window, "nenefolio://open?category=schedule&note=one");
    puts("outside refresh: the reading view");
    fflush(stdout);
    outside_reading(fixture, window);
    puts("outside refresh: the lists, and a surface in the way");
    fflush(stdout);
    outside_lists(fixture, window);
    puts("outside refresh: request files");
    fflush(stdout);
    outside_requested(fixture, window);
    puts("outside refresh: a change while the window stays in front");
    fflush(stdout);
    outside_watched(fixture, window);
    puts("outside refresh: editing");
    fflush(stdout);
    outside_editing(fixture, window);
    puts("outside refresh: a save stopped by the outside change");
    fflush(stdout);
    outside_stopped(fixture, window);
    puts("outside refresh: discard reads the file");
    fflush(stdout);
    outside_discarded(fixture, window);
    folio_window_destroy(window);
}

/* A part of the main window as UIA reads it. */
static struct accessible_view outline_read(struct folio_window *_Nonnull window,
                                           enum accessible_kind kind)
{
    struct accessible_item item = {.kind = kind};
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "outline part reads");
    return view;
}

static struct accessible_view outline_row(struct folio_window *_Nonnull window,
                                          struct accessible_item item)
{
    struct accessible_view view = {0};
    require(folio_window_accessible_read(window, &item, &view) == S_OK, "outline row reads");
    return view;
}

/* The column's row for a heading, as UIA reaches it from the list. */
static struct accessible_item outline_item_at(struct folio_window *_Nonnull window, size_t slot)
{
    struct accessible_item list = {.kind = ACCESSIBLE_OUTLINE};
    struct accessible_item item = {0};
    require(folio_window_accessible_navigate(window, &list, ACCESSIBLE_NAVIGATION_FIRST, &item) ==
                    S_OK &&
                item.kind == ACCESSIBLE_OUTLINE_ITEM && item.slot == 0,
            "the column's first child is the first heading");
    item.slot = slot;
    return item;
}

/* The heading UIA reports as being read, or SIZE_MAX before the first. */
static size_t outline_current(struct folio_window *_Nonnull window)
{
    struct accessible_item list = {.kind = ACCESSIBLE_OUTLINE};
    struct accessible_item selected = {.kind = ACCESSIBLE_WINDOW};
    HRESULT result = folio_window_accessible_selected(window, &list, &selected);
    require(result == S_OK || result == S_FALSE, "the column's selection reads");
    return result == S_OK ? selected.slot : SIZE_MAX;
}

static LRESULT outline_first_line(struct folio_window *_Nonnull window)
{
    return SendMessageW(folio_window_accessible_window(window, ACCESSIBLE_BODY),
                        EM_GETFIRSTVISIBLELINE, 0, 0);
}

/* Every row names its heading, with its level and place, inside the column; the list walks
 * from the first row to the last and no further. */
static void outline_rows(struct folio_window *_Nonnull window)
{
    struct accessible_view column = outline_read(window, ACCESSIBLE_OUTLINE);
    require(!column.offscreen && !IsRectEmpty(&column.bounds) &&
                wcscmp((const wchar_t *)column.name, L"見出し") == 0,
            "the column is on screen and named");
    /* #1253: the column's title is drawn in the language's face. Its band is the top 28 DIP. */
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int dpi = (int)GetDpiForWindow(root);
    RECT title = column.bounds;
    MapWindowPoints(nullptr, root, (POINT *)&title, 2);
    title.left += MulDiv(4, dpi, 96);
    title.right -= MulDiv(4, dpi, 96);
    title.bottom = title.top + MulDiv(22, dpi, 96);
    title.top += MulDiv(6, dpi, 96);
    bool told = expect_word_face(window, title, L"見出し",
                                 "the headings column's title is drawn in the language's face");
    /* Three glyphs differ by six pixels at 96 DPI and by four at 120: the 96 DPI run decides. */
    require(told || dpi != 96, "at 96 DPI the title tells the two faces apart, so the check ran");
    const wchar_t *const names[] = {L"設計メモ", L"背景", L"決めたこと", L"色", L"配置", L"余白"};
    const int levels[] = {1, 2, 2, 3, 3, 4};
    struct accessible_item item = outline_item_at(window, 0);
    for (size_t at = 0; at < 9; ++at)
    {
        struct accessible_view row = outline_row(window, item);
        require(
            row.position == (int)at + 1 && row.set_size == 9 && !row.offscreen &&
                row.bounds.left >= column.bounds.left && row.bounds.right <= column.bounds.right &&
                row.bounds.top >= column.bounds.top && row.bounds.bottom <= column.bounds.bottom,
            "each row is a place in the set inside the column");
        require(at >= 6 ||
                    (wcscmp((const wchar_t *)row.name, names[at]) == 0 && row.level == levels[at]),
                "each row names its heading and level");
        struct accessible_item next = {0};
        HRESULT step =
            folio_window_accessible_navigate(window, &item, ACCESSIBLE_NAVIGATION_NEXT, &next);
        require(at < 8 ? step == S_OK && next.slot == at + 1 : step == S_FALSE,
                "the rows follow one another and end at the last heading");
        item = next;
    }
    struct accessible_item last = outline_item_at(window, 8);
    struct accessible_item parent = {0};
    require(folio_window_accessible_navigate(window, &last, ACCESSIBLE_NAVIGATION_PARENT,
                                             &parent) == S_OK &&
                parent.kind == ACCESSIBLE_OUTLINE,
            "a row's parent is the column");
}

/* Pressing and releasing a row brings its heading to the top of the body; releasing off the row
 * does nothing. UIA's Invoke takes the same path. */
static void outline_jump(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    require(outline_current(window) == 0 && outline_first_line(window) == 0,
            "at the top the first heading is being read");
    struct accessible_view row = outline_row(window, outline_item_at(window, 3));
    POINT inside = {(row.bounds.left + row.bounds.right) / 2,
                    (row.bounds.top + row.bounds.bottom) / 2};
    require(ScreenToClient(root, &inside), "row point");
    POINT away = {inside.x, inside.y + (row.bounds.bottom - row.bounds.top) * 2};
    SendMessageW(root, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(inside.x, inside.y));
    SendMessageW(root, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(away.x, away.y));
    SendMessageW(root, WM_LBUTTONUP, 0, MAKELPARAM(away.x, away.y));
    pump(window);
    require(outline_current(window) == 0 && outline_first_line(window) == 0,
            "releasing off the row does not jump");
    SendMessageW(root, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(inside.x, inside.y));
    SendMessageW(root, WM_LBUTTONUP, 0, MAKELPARAM(inside.x, inside.y));
    pump(window);
    LRESULT clicked = outline_first_line(window);
    printf("outline: click row 3 -> first line %lld, current %zu\n", (long long)clicked,
           outline_current(window));
    fflush(stdout);
    require(clicked > 0 && outline_current(window) == 3, "clicking a row jumps to its heading");
    struct accessible_request request = {.item = outline_item_at(window, 1),
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "UIA invokes a row");
    pump(window);
    require(outline_current(window) == 1 && outline_first_line(window) < clicked,
            "UIA's Invoke jumps back up to its heading");
}

/* The item UIA reads as having the keyboard focus, or the window when none. */
static struct accessible_item outline_focused(struct folio_window *_Nonnull window)
{
    struct accessible_item focused = {.kind = ACCESSIBLE_WINDOW};
    if (folio_window_accessible_focus(window, ACCESSIBLE_WINDOW, &focused) != S_OK)
        focused = (struct accessible_item){.kind = ACCESSIBLE_WINDOW};
    return focused;
}

static bool outline_focused_row(struct folio_window *_Nonnull window, size_t slot)
{
    struct accessible_item focused = outline_focused(window);
    return focused.kind == ACCESSIBLE_OUTLINE_ITEM && focused.slot == slot;
}

/* Tab from the pane's tools reaches the column on the heading being read. The arrows, Home and
 * End move the focused row; Enter jumps to it; Shift+Tab leaves; Tab back starts again at the
 * heading being read. */
static void outline_keyboard(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct accessible_request menu = {.item = {.kind = ACCESSIBLE_MENU},
                                      .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &menu) == S_OK,
            "the pane's menu takes the focus");
    pump(window);
    for (int step = 0; step < 4 && outline_focused(window).kind != ACCESSIBLE_OUTLINE_ITEM; ++step)
        favorite_key(window, VK_TAB);
    size_t reading = outline_current(window);
    printf("outline: Tab reached kind %d slot %zu, reading %zu\n",
           (int)outline_focused(window).kind, outline_focused(window).slot, reading);
    fflush(stdout);
    require(reading != SIZE_MAX && outline_focused_row(window, reading),
            "Tab reaches the column on the heading being read");
    favorite_key(window, VK_DOWN);
    require(outline_focused_row(window, reading + 1), "Down moves to the next row");
    favorite_key(window, VK_END);
    require(outline_focused_row(window, 8), "End moves to the last row");
    favorite_key(window, VK_HOME);
    require(outline_focused_row(window, 0) && outline_current(window) == reading,
            "Home moves to the first row without jumping");
    for (int step = 0; step < 3; ++step)
        favorite_key(window, VK_DOWN);
    LRESULT before = outline_first_line(window);
    favorite_key(window, VK_RETURN);
    require(outline_current(window) == 3 && outline_first_line(window) != before &&
                outline_focused_row(window, 3),
            "Enter jumps to the focused row's heading and keeps the focus there");
    struct accessible_view focused_row = outline_row(window, outline_item_at(window, 3));
    require(focused_row.focused && focused_row.focusable &&
                !outline_row(window, outline_item_at(window, 2)).focused,
            "UIA reads the cursor row, and only it, as focused");
    favorite_key(window, VK_UP);
    native_key_input_to(window, root, VK_TAB, FSHIFT);
    struct accessible_item left = outline_focused(window);
    require(left.kind != ACCESSIBLE_OUTLINE_ITEM && left.kind != ACCESSIBLE_OUTLINE,
            "Shift+Tab leaves the column");
    favorite_key(window, VK_TAB);
    require(outline_focused_row(window, 3), "Tab back starts at the heading being read");
}

static void outline_send_wheel(struct folio_window *_Nonnull window, POINT screen, int delta)
{
    MSG message = {.hwnd = folio_window_accessible_window(window, ACCESSIBLE_BODY),
                   .message = WM_MOUSEWHEEL,
                   .wParam = MAKEWPARAM(0, (WORD)(short)delta),
                   .lParam = MAKELPARAM(screen.x, screen.y)};
    require(folio_window_translate(window, &message),
            "the wheel over the column is taken while the body has the focus");
    pump(window);
}

/* In a short window the column holds fewer rows than headings. The wheel over it scrolls the
 * column and leaves the body; the other way brings the first row back. */
static void outline_wheel(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    UINT dpi = GetDpiForWindow(root);
    require(SetWindowPos(root, nullptr, 20, 20, MulDiv(900, (int)dpi, 96),
                         MulDiv(300, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
            "short outline window");
    pump(window);
    SetFocus(folio_window_accessible_window(window, ACCESSIBLE_BODY));
    struct accessible_view column = outline_read(window, ACCESSIBLE_OUTLINE);
    require(!column.offscreen, "the short window keeps the column");
    require(outline_row(window, outline_item_at(window, 8)).offscreen,
            "the short column cannot show every row");
    POINT over = {(column.bounds.left + column.bounds.right) / 2,
                  (column.bounds.top + column.bounds.bottom) / 2};
    LRESULT line = outline_first_line(window);
    outline_send_wheel(window, over, -WHEEL_DELTA * 3);
    require(outline_row(window, outline_item_at(window, 0)).offscreen &&
                !outline_row(window, outline_item_at(window, 8)).offscreen &&
                outline_first_line(window) == line,
            "the wheel scrolls the column to its end and not the body");
    outline_send_wheel(window, over, WHEEL_DELTA * 3);
    require(!outline_row(window, outline_item_at(window, 0)).offscreen &&
                outline_first_line(window) == line,
            "the wheel the other way brings the first row back");
}

/* #1026: new colours pour the same note again; the column the wheel moved stays where it was,
 * the body stays at its line, and a row read before the pour no longer answers. Runs in the
 * short window outline_wheel leaves. */
static void outline_recolor(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct accessible_view column = outline_read(window, ACCESSIBLE_OUTLINE);
    POINT over = {(column.bounds.left + column.bounds.right) / 2,
                  (column.bounds.top + column.bounds.bottom) / 2};
    outline_send_wheel(window, over, -WHEEL_DELTA * 3);
    struct accessible_item stale = outline_item_at(window, 8);
    LRESULT line = outline_first_line(window);
    require(outline_row(window, outline_item_at(window, 0)).offscreen &&
                !outline_row(window, stale).offscreen,
            "the column is at its end before the colours change");
    favorite_high_contrast = !favorite_high_contrast;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    pump(window);
    struct accessible_view view = {0};
    bool renewed = folio_window_accessible_read(window, &stale, &view) != S_OK;
    printf("outline: recolour, rows %s, first row %s, last row %s\n", renewed ? "new" : "same",
           outline_row(window, outline_item_at(window, 0)).offscreen ? "hidden" : "shown",
           outline_row(window, outline_item_at(window, 8)).offscreen ? "hidden" : "shown");
    fflush(stdout);
    require(renewed, "the colours poured the note again: an earlier row does not answer");
    require(outline_row(window, outline_item_at(window, 0)).offscreen &&
                !outline_row(window, outline_item_at(window, 8)).offscreen &&
                outline_first_line(window) == line,
            "the column and the body stay where they were through new colours");
    favorite_high_contrast = !favorite_high_contrast;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    pump(window);
    outline_send_wheel(window, over, WHEEL_DELTA * 3);
    require(!outline_row(window, outline_item_at(window, 0)).offscreen,
            "the wheel brings the first row back");
}

static void outline_resize(struct folio_window *_Nonnull window, int height)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    UINT dpi = GetDpiForWindow(root);
    require(SetWindowPos(root, nullptr, 20, 20, MulDiv(900, (int)dpi, 96),
                         MulDiv(height, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
            "outline window height");
    pump(window);
}

/* #1056 (review of #1040): scrolling the body up a line at a time from the chosen last
 * heading, that heading is never the one being read once its line has left the view. */
static void outline_chosen_leaves(struct folio_window *_Nonnull window, size_t chosen)
{
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LONG mark = footnote_find(body, L"付録", 0);
    RECT client = {0};
    require(GetClientRect(body, &client), "body client");
    size_t held = 0;
    size_t gone = 0;
    for (int step = 0; step < 120 && outline_first_line(window) > 0; ++step)
    {
        SendMessageW(body, EM_LINESCROLL, 0, -1);
        pump(window);
        POINTL place = {0};
        SendMessageW(body, EM_POSFROMCHAR, (WPARAM)&place, mark);
        bool visible = place.y >= 0 && place.y < client.bottom;
        bool marked = outline_current(window) == chosen;
        held += visible && marked ? 1 : 0;
        gone += visible ? 0 : 1;
        require(visible || !marked, "a heading that left the view is not the one being read");
    }
    printf("outline: chosen heading stayed marked for %zu lines in view, then %zu lines out\n",
           held, gone);
    fflush(stdout);
    require(held > 0 && gone > 0,
            "the chosen heading stayed marked while in view, and the walk took it out of the view");
}

/* #1056 (review of #1026): with a chosen heading and the column wheeled away from it, new
 * colours leave the column where the wheel put it and the chosen heading still being read.
 * Makes the window short for it, and leaves it tall with the body at the top. */
static void outline_chosen_recolor(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    outline_resize(window, 300);
    size_t count = (size_t)outline_row(window, outline_item_at(window, 0)).set_size;
    struct accessible_request request = {.item = outline_item_at(window, count - 1),
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "the last row");
    pump(window);
    struct accessible_view column = outline_read(window, ACCESSIBLE_OUTLINE);
    POINT over = {(column.bounds.left + column.bounds.right) / 2,
                  (column.bounds.top + column.bounds.bottom) / 2};
    outline_send_wheel(window, over, WHEEL_DELTA * 3);
    require(outline_current(window) == count - 1 &&
                !outline_row(window, outline_item_at(window, 0)).offscreen &&
                outline_row(window, outline_item_at(window, count - 1)).offscreen,
            "the column is wheeled away from the chosen heading");
    favorite_high_contrast = !favorite_high_contrast;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    pump(window);
    printf("outline: chosen and recoloured, current %zu of %zu, first row %s\n",
           outline_current(window), count,
           outline_row(window, outline_item_at(window, 0)).offscreen ? "hidden" : "shown");
    fflush(stdout);
    require(outline_current(window) == count - 1 &&
                !outline_row(window, outline_item_at(window, 0)).offscreen &&
                outline_row(window, outline_item_at(window, count - 1)).offscreen,
            "new colours keep the chosen heading and do not pull the wheeled column back");
    favorite_high_contrast = !favorite_high_contrast;
    SendMessageW(root, WM_SETTINGCHANGE, 0, 0);
    pump(window);
    request.item = outline_item_at(window, 0);
    require(folio_window_accessible_action(window, &request) == S_OK, "back to the first heading");
    pump(window);
    outline_resize(window, 620);
}

/* #1018: reading a late heading and then making the window shorter keeps that heading's row in
 * the column; making it tall again leaves no empty rows below the last heading. */
static void outline_shorten(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    UINT dpi = GetDpiForWindow(root);
    size_t count = (size_t)outline_row(window, outline_item_at(window, 0)).set_size;
    struct accessible_request request = {.item = outline_item_at(window, count - 1),
                                         .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &request) == S_OK, "UIA invokes the last row");
    pump(window);
    /* #1040: the last heading cannot rise to the top of the body, yet it is the one being read
     * until the body moves; then the body's place decides again. */
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    LRESULT low = outline_first_line(window);
    require(outline_current(window) == count - 1 &&
                outline_row(window, outline_item_at(window, count - 1)).selected,
            "the row that was invoked is the heading being read, though it is not at the top");
    outline_chosen_leaves(window, count - 1);
    SendMessageW(body, EM_LINESCROLL, 0, -100000);
    pump(window);
    printf("outline: last row chosen at line %lld, after scrolling up line %lld current %zu\n",
           (long long)low, (long long)outline_first_line(window), outline_current(window));
    fflush(stdout);
    require(low > 0 && outline_first_line(window) == 0 && outline_current(window) == 0,
            "scrolling the body gives the mark back to the body's place");
    require(folio_window_accessible_action(window, &request) == S_OK, "the last row again");
    pump(window);
    size_t current = outline_current(window);
    require(SetWindowPos(root, nullptr, 20, 20, MulDiv(900, (int)dpi, 96),
                         MulDiv(260, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
            "short outline window");
    pump(window);
    struct accessible_view row = outline_row(window, outline_item_at(window, current));
    struct accessible_view first = outline_row(window, outline_item_at(window, 0));
    printf("outline: short window, current %zu of %zu, its row %s, first row %s\n", current, count,
           row.offscreen ? "hidden" : "shown", first.offscreen ? "hidden" : "shown");
    fflush(stdout);
    require(current != SIZE_MAX && current >= 4 && !row.offscreen && first.offscreen,
            "a shorter window keeps the heading being read in the column");
    require(SetWindowPos(root, nullptr, 20, 20, MulDiv(900, (int)dpi, 96),
                         MulDiv(620, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
            "tall outline window again");
    pump(window);
    require(!outline_row(window, outline_item_at(window, count - 1)).offscreen &&
                !outline_row(window, outline_item_at(window, 0)).offscreen,
            "a taller window shows every heading again, with no empty rows below the last");
    request.item = outline_item_at(window, 0);
    require(folio_window_accessible_action(window, &request) == S_OK, "back to the first heading");
    pump(window);
}

/* A window too narrow for the body beside the column hides the column and gives the body its
 * width; the column stays open and comes back when the window widens. */
static void outline_narrow(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    UINT dpi = GetDpiForWindow(root);
    struct accessible_item stale = outline_item_at(window, 0);
    struct accessible_request focus = {.item = outline_item_at(window, 2),
                                       .action = ACCESSIBLE_ACTION_FOCUS};
    require(folio_window_accessible_action(window, &focus) == S_OK &&
                outline_focused_row(window, 2),
            "UIA focuses a row of the column");
    require(SetWindowPos(root, nullptr, 20, 20, MulDiv(560, (int)dpi, 96),
                         MulDiv(620, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
            "narrow outline window");
    pump(window);
    struct accessible_view column = outline_read(window, ACCESSIBLE_OUTLINE);
    struct accessible_view map = outline_read(window, ACCESSIBLE_MINIMAP);
    printf("outline: narrow window, column %s, minimap %s\n", column.offscreen ? "hidden" : "shown",
           map.offscreen ? "hidden" : "shown");
    fflush(stdout);
    struct accessible_view view = {0};
    require(
        column.offscreen && IsRectEmpty(&column.bounds) && folio_state_outline(fixture->state) &&
            folio_window_accessible_read(window, &stale, &view) == accessibility_node_unavailable,
        "a narrow window hides the open column and its rows");
    require(!map.offscreen, "the narrow window still has the minimap");
    struct accessible_item moved = outline_focused(window);
    require(moved.kind != ACCESSIBLE_OUTLINE_ITEM && moved.kind != ACCESSIBLE_OUTLINE,
            "the keyboard leaves a column that is no longer shown");
    require(SetWindowPos(root, nullptr, 20, 20, MulDiv(900, (int)dpi, 96),
                         MulDiv(620, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
            "wide outline window");
    pump(window);
    require(!outline_read(window, ACCESSIBLE_OUTLINE).offscreen &&
                folio_window_accessible_read(window, &stale, &view) == S_OK,
            "the column and the same rows come back in a wide window");
    native_key_input(window, 'E', FCONTROL);
    pump(window);
    native_key_input(window, 'E', FCONTROL);
    pump(window);
    require(folio_window_accessible_read(window, &stale, &view) == accessibility_node_unavailable,
            "a row of an earlier load does not answer");
}

/* The body control's rectangle in the main window's client coordinates. */
static RECT outline_body_rect(struct folio_window *_Nonnull window)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    HWND body = folio_window_accessible_window(window, ACCESSIBLE_BODY);
    RECT bounds = {0};
    GetWindowRect(body, &bounds);
    MapWindowPoints(nullptr, root, (POINT *)&bounds, 2);
    return bounds;
}

/* Opening the column takes its width and gap from the body's right; closing gives it back. */
static void outline_toggle(struct folio_window *_Nonnull window,
                           struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int carve = MulDiv(184 + 12, (int)GetDpiForWindow(root), 96);
    RECT closed = outline_body_rect(window);
    struct accessible_view map_closed = outline_read(window, ACCESSIBLE_MINIMAP);
    require(!folio_state_outline(fixture->state), "the column starts closed");
    native_key_input(window, 'O', FCONTROL | FSHIFT);
    pump(window);
    RECT open = outline_body_rect(window);
    struct accessible_view map_open = outline_read(window, ACCESSIBLE_MINIMAP);
    printf("outline: minimap %s, left %ld -> %ld\n", map_closed.offscreen ? "hidden" : "shown",
           map_closed.bounds.left, map_open.bounds.left);
    require(EqualRect(&map_closed.bounds, &map_open.bounds) &&
                map_closed.offscreen == map_open.offscreen,
            "opening the column leaves the minimap where it was");
    printf("outline: body right %ld -> %ld (carve %d)\n", closed.right, open.right, carve);
    fflush(stdout);
    require(folio_state_outline(fixture->state) && open.left == closed.left &&
                open.right == closed.right - carve,
            "Ctrl+Shift+O opens the column beside the body");
    native_key_input(window, 'E', FCONTROL);
    pump(window);
    RECT editing = outline_body_rect(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                editing.right == closed.right && folio_state_outline(fixture->state),
            "the editing view has the body's full width; the column stays open for later");
    native_key_input(window, 'E', FCONTROL);
    pump(window);
    RECT viewing = outline_body_rect(window);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW && viewing.right == open.right,
            "back in the reading view the column is there again");
    native_key_input(window, 'O', FCONTROL | FSHIFT);
    pump(window);
    require(!folio_state_outline(fixture->state) &&
                outline_body_rect(window).right == closed.right &&
                outline_read(window, ACCESSIBLE_OUTLINE).offscreen,
            "Ctrl+Shift+O again closes the column and gives the body its width back");
    native_key_input(window, 'O', FCONTROL | FSHIFT);
    pump(window);
    require(folio_state_outline(fixture->state) && outline_body_rect(window).right == open.right,
            "and opens it again");
}

/* #975 / ADR0114: the headings column in the main window, in each look. */
static void outline_column(struct native_fixture *_Nonnull fixture)
{
    const char *const looks[] = {"light", "dark", "contrast"};
    if (outline_96)
        require(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE) != nullptr,
                "96 DPI windows for the outline case");
    for (size_t at = 0; at < 3; ++at)
    {
        favorite_high_contrast = at == 2;
        require(folio_state_set_language(fixture->state, FOLIO_LANGUAGE_JA) == FOLIO_STATE_READY &&
                    folio_state_refresh_theme(fixture->state) == FOLIO_STATE_READY &&
                    folio_state_set_theme(fixture->state, at == 1 ? FOLIO_THEME_CHOICE_DARK
                                                                  : FOLIO_THEME_CHOICE_LIGHT) ==
                        FOLIO_STATE_READY,
                "outline palette");
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_NOTE);
        folio_state_set_outline(fixture->state, false);
        struct folio_window *window = nullptr;
        require(folio_window_create(fixture->state, fixture->location, FONT_BUNDLE_READY,
                                    &window) == FOLIO_WINDOW_CREATED,
                "outline window");
        HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
        UINT dpi = GetDpiForWindow(root);
        require(SetWindowPos(root, nullptr, 20, 20, MulDiv(900, (int)dpi, 96),
                             MulDiv(620, (int)dpi, 96), SWP_NOZORDER | SWP_NOACTIVATE),
                "outline size");
        pump(window);
        outline_toggle(window, fixture);
        outline_rows(window);
        outline_jump(window);
        outline_keyboard(window);
        char path[64];
        require(snprintf(path, sizeof path, "outline-%s.bmp", looks[at]) > 0,
                "outline screenshot path");
        tags_capture(root, path);
        outline_wheel(window);
        outline_recolor(window);
        outline_shorten(window);
        outline_chosen_recolor(window);
        outline_narrow(window, fixture);
        folio_window_destroy(window);
        folio_state_set_outline(fixture->state, false);
        folio_state_set_layout_mode(fixture->state, FOLIO_LAYOUT_BOTH);
    }
    favorite_high_contrast = false;
}

static void text_menu_cases(struct native_fixture *_Nonnull fixture)
{
    text_menu_visual(fixture);
    text_menu_languages(fixture);
}

static bool window_or_footnote_case(void)
{
    return window_place_case || footnote_jump_case || definition_list_case || outline_case ||
           open_link_case;
}

static void run_window_or_footnote(struct native_fixture *_Nonnull fixture)
{
    if (window_place_case)
        window_place(fixture);
    else if (definition_list_case)
        definition_list(fixture);
    else if (outline_case)
        outline_column(fixture);
    else if (request_prompt_case)
        request_prompt(fixture);
    else if (outside_refresh_case)
        outside_refresh(fixture);
    else if (open_link_case)
        open_link(fixture);
    else
        footnote_jump(fixture);
}

static void run_code_copy_case(struct native_fixture *_Nonnull fixture)
{
    require(folio_state_select_note(fixture->state, 0, 0, FOLIO_UNTITLED_PROTECT) ==
                FOLIO_STATE_READY,
            "select the dedicated code note");
    if (text_menu_case)
    {
        text_menu_cases(fixture);
        return;
    }
    if (window_or_footnote_case())
    {
        run_window_or_footnote(fixture);
        return;
    }
    if (search_key_visual_case)
    {
        search_key_visual(fixture);
        return;
    }
    if (bare_url_visual_case)
    {
        bare_url_visual(fixture);
        return;
    }
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

/* #855: Ctrl+O opens the whole-library search from the body, the same command as the menu's. */
static void discovery_shortcut(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND surface = folio_window_accessible_window(window, ACCESSIBLE_SURFACE);
    bool before = IsWindowVisible(surface);
    require(native_key_input(window, 'O', FCONTROL), "Ctrl+O is a window accelerator");
    pump(window);
    discovery_settle(window, fixture->state);
    printf("discovery shortcut: visible before %d after %d shown %zu\n", before,
           IsWindowVisible(surface), folio_state_discovery_summary(fixture->state).shown);
    require(!before && IsWindowVisible(surface) &&
                folio_state_discovery_summary(fixture->state).shown == 3,
            "Ctrl+O opens the whole-library search");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    require(!IsWindowVisible(surface), "the search closes as before");
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
                wcscmp(open.name, L"一致行を開く Enter") == 0 &&
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

/* #933: from the query, Down selects without leaving the input. */
static void body_discovery_typing_rows(struct folio_window *_Nonnull window,
                                       struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"needle");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    size_t before = folio_state_discovery_summary(fixture->state).selected;
    SendMessageW(input, WM_KEYDOWN, VK_DOWN, 0);
    pump(window);
    discovery_settle(window, fixture->state);
    size_t after = folio_state_discovery_summary(fixture->state).selected;
    printf("typing rows: selected %zu -> %zu focus input %d\n", before, after, GetFocus() == input);
    fflush(stdout);
    require(after == before + 1 && GetFocus() == input,
            "Down in the query selects the next candidate and keeps typing focus");
    SendMessageW(input, EM_SETSEL, 6, 6);
    SendMessageW(input, WM_CHAR, L'x', 0);
    pump(window);
    discovery_settle(window, fixture->state);
    wchar_t query[32];
    require(GetWindowTextW(input, query, 32) > 0 && wcscmp(query, L"needlex") == 0,
            "typing continues in the query after Down");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

static size_t body_discovery_first_line(HWND preview)
{
    return (size_t)SendMessageW(preview, EM_GETFIRSTVISIBLELINE, 0, 0);
}

static LONG body_discovery_width(HWND window)
{
    RECT bounds = {0};
    GetWindowRect(window, &bounds);
    return bounds.right - bounds.left;
}

/* #933: a width change re-wraps the preview, so the match is revealed again. */
static void body_discovery_resize_reveal(struct folio_window *_Nonnull window, HWND input,
                                         HWND preview)
{
    SendMessageW(input, WM_KEYDOWN, VK_PRIOR, 0);
    SendMessageW(input, WM_KEYDOWN, VK_PRIOR, 0);
    pump(window);
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    LONG before = body_discovery_width(preview);
    size_t hidden = body_discovery_first_line(preview);
    /* The surface is min(960 logical, client - 32): 900 logical stays wide and changes the
     * preview width from the 1050-pixel fixture at 96 DPI (narrower) and 120 DPI (wider). */
    int width = MulDiv(900, (int)GetDpiForWindow(root), 96);
    require(SetWindowPos(root, nullptr, 20, 20, width, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "resize the window while the match is scrolled away");
    pump(window);
    printf("typing preview: window %d dpi %u, preview width %ld -> %ld first line %zu -> %zu\n",
           width, GetDpiForWindow(root), before, body_discovery_width(preview), hidden,
           body_discovery_first_line(preview));
    fflush(stdout);
    require(body_discovery_width(preview) != before, "the resized window resizes the preview");
    body_discovery_preview_visible(preview);
    require(SetWindowPos(root, nullptr, 20, 20, 1050, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "restore the wide body fixture");
    pump(window);
}

/* #933: PageUp/PageDown in the query page the wrapped source preview, not the candidates. */
static void body_discovery_typing_preview(struct folio_window *_Nonnull window,
                                          struct native_fixture *_Nonnull fixture)
{
    char source[8192] = "";
    for (int line = 1; line <= 60; ++line)
    {
        char text[32];
        (void)snprintf(text, sizeof text, "line %d\n", line);
        (void)strcat_s(source, sizeof source, text);
    }
    for (int word = 0; word < 120; ++word)
        (void)strcat_s(source, sizeof source, "wrapword ");
    (void)strcat_s(source, sizeof source, "farneedle tail\nlast\n");
    body_discovery_write(fixture, L"\\target\\three.md", source);
    body_discovery_open(window, fixture, L"farneedle");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    LONG_PTR style = GetWindowLongPtrW(preview, GWL_STYLE);
    LRESULT visual = SendMessageW(preview, EM_GETLINECOUNT, 0, 0);
    wchar_t shown[4096];
    int length = GetWindowTextW(preview, shown, 4096);
    LRESULT logical = 1;
    for (int at = 0; at < length; ++at)
        logical += shown[at] == L'\n';
    printf("typing preview: shown %zu hscroll %d autohscroll %d lines visual %lld logical %lld\n",
           folio_state_discovery_summary(fixture->state).shown, (style & WS_HSCROLL) != 0,
           (style & ES_AUTOHSCROLL) != 0, (long long)visual, (long long)logical);
    fflush(stdout);
    require(folio_state_discovery_summary(fixture->state).shown == 1 &&
                (style & (WS_HSCROLL | ES_AUTOHSCROLL)) == 0 && visual > logical + 2,
            "the source preview wraps long lines instead of scrolling sideways");
    body_discovery_preview_visible(preview);
    SetFocus(input);
    size_t selected = folio_state_discovery_summary(fixture->state).selected;
    size_t revealed = body_discovery_first_line(preview);
    SendMessageW(input, WM_KEYDOWN, VK_PRIOR, 0);
    pump(window);
    size_t up = body_discovery_first_line(preview);
    SendMessageW(input, WM_KEYDOWN, VK_NEXT, 0);
    pump(window);
    size_t down = body_discovery_first_line(preview);
    printf("typing preview: first line revealed %zu up %zu down %zu\n", revealed, up, down);
    fflush(stdout);
    require(revealed > 0 && up < revealed && down > up && GetFocus() == input &&
                folio_state_discovery_summary(fixture->state).selected == selected,
            "PageUp/PageDown in the query scroll the preview and keep the candidate and focus");
    body_discovery_resize_reveal(window, input, preview);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    body_discovery_write(fixture, L"\\target\\three.md",
                         "---\nitem:\n  child: value\n---\nneedle unknown");
}

/* #940: Ctrl held while Enter goes to the query, as the real key press delivers it. */
static void discovery_ctrl_enter(struct folio_window *_Nonnull window, HWND input)
{
    BYTE original[256], keyboard[256] = {0};
    require(GetKeyboardState(original), "save fixture keyboard state");
    keyboard[VK_CONTROL] = 0x80;
    require(SetKeyboardState(keyboard), "hold Ctrl");
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    SendMessageW(input, WM_CHAR, L'\n', 0);
    require(SetKeyboardState(original), "release Ctrl");
    pump(window);
}

static bool discovery_mode_is(struct native_fixture *_Nonnull fixture, enum discovery_mode mode,
                              HWND input)
{
    return folio_state_discovery_summary(fixture->state).mode == mode && GetFocus() == input;
}

/* #940: in an empty query Left/Right choose names/body; with text they move the caret and
 * Alt+Left / Alt+Right choose. The focus stays in the query. */
static void discovery_mode_arrows(struct folio_window *_Nonnull window,
                                  struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    SendMessageW(input, WM_KEYDOWN, VK_RIGHT, 0);
    require(discovery_mode_is(fixture, DISCOVERY_MODE_BODY, input),
            "Right in an empty query chooses body and keeps typing focus");
    SendMessageW(input, WM_KEYDOWN, VK_LEFT, 0);
    require(discovery_mode_is(fixture, DISCOVERY_MODE_NAMES, input),
            "Left in an empty query chooses names");
    discovery_query(window, fixture->state, L"one");
    SendMessageW(input, EM_SETSEL, 0, 0);
    SendMessageW(input, WM_KEYDOWN, VK_RIGHT, 0);
    DWORD caret = 0;
    SendMessageW(input, EM_GETSEL, (WPARAM)&caret, 0);
    printf("mode arrows: caret after Right in text %lu\n", caret);
    fflush(stdout);
    require(discovery_mode_is(fixture, DISCOVERY_MODE_NAMES, input) && caret == 1,
            "Right in a query with text moves the caret, not the mode");
    SendMessageW(input, WM_SYSKEYDOWN, VK_RIGHT, 0);
    discovery_settle(window, fixture->state);
    wchar_t query[16];
    require(discovery_mode_is(fixture, DISCOVERY_MODE_BODY, input) &&
                GetWindowTextW(input, query, 16) == 3 && wcscmp(query, L"one") == 0,
            "Alt+Right chooses body and keeps the query and the focus");
    SendMessageW(input, WM_SYSKEYDOWN, VK_LEFT, 0);
    discovery_settle(window, fixture->state);
    require(discovery_mode_is(fixture, DISCOVERY_MODE_NAMES, input),
            "Alt+Left chooses names again");
    SendMessageW(input, WM_IME_STARTCOMPOSITION, 0, 0);
    SendMessageW(input, WM_SYSKEYDOWN, VK_RIGHT, 0);
    require(folio_state_discovery_summary(fixture->state).mode == DISCOVERY_MODE_NAMES,
            "composition keeps Alt+Right away from the mode");
    SendMessageW(input, WM_IME_ENDCOMPOSITION, 0, 0);
    discovery_settle(window, fixture->state);
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
}

/* #940: Ctrl+Enter (and the editing button) open a named candidate in the editing view. */
static void discovery_edit_open(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    action(window, ACCESSIBLE_VIEW);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_query(window, fixture->state, L"two");
    struct accessible_view edit =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    struct accessible_view open =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    printf("edit open: buttons %ls | %ls\n", edit.name, open.name);
    fflush(stdout);
    require(edit.enabled && wcscmp(edit.name, L"編集で開く Ctrl+Enter") == 0 &&
                wcscmp(open.name, L"開く Enter") == 0 && edit.bounds.right <= open.bounds.left,
            "both open actions show their keys side by side");
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    discovery_ctrl_enter(window, input);
    wchar_t typed[16];
    require(strcmp(folio_state_document_name(fixture->state), "two") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED &&
                GetWindowTextW(input, typed, 16) >= 0 && wcschr(typed, L'\n') == nullptr,
            "Ctrl+Enter opens the named candidate in the editing view and types nothing");
    action(window, ACCESSIBLE_VIEW);
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    discovery_query(window, fixture->state, L"one");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0 &&
                folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "the editing button opens the first fixture note in the editing view");
}

/* #940: Enter on a body match opens the reading view and selects the first match shown there;
 * the term becomes the in-note search term. Ctrl+Enter keeps the editing path. */
static void body_discovery_view_open(struct folio_window *_Nonnull window,
                                     struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"needle");
    discovery_choose(window, 0);
    discovery_settle(window, fixture->state);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    SetFocus(input);
    /* The product reads Ctrl for this key: a Ctrl left in the thread's key state makes the plain
     * Enter the editing path (seen in parallel full runs, 2026-11-21; the same kind as #1101). */
    BYTE original[256];
    release_modifiers(original, "body discovery, the plain Enter");
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    pump(window);
    require(SetKeyboardState(original), "restore the keyboard state");
    struct native_body_snapshot shown = {0};
    capture_native_body(window, &shown);
    const wchar_t *first = wcsstr(shown.text, L"needle");
    printf("view open: mode view %d selection %ld..%ld first %td term %s\n",
           folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW, shown.selection.cpMin,
           shown.selection.cpMax, first == nullptr ? -1 : first - shown.text,
           folio_state_search_term(fixture->state));
    fflush(stdout);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_VIEW &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_CLOSED,
            "Enter on a body match opens the reading view");
    require(first != nullptr && shown.selection.cpMin == first - shown.text &&
                shown.selection.cpMax == shown.selection.cpMin + 6 &&
                strcmp(folio_state_search_term(fixture->state), "needle") == 0,
            "the first match in the displayed text is selected and becomes the search term");
    body_discovery_open(window, fixture, L"needle");
    discovery_choose(window, 0);
    discovery_settle(window, fixture->state);
    SetFocus(input);
    discovery_ctrl_enter(window, input);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "Ctrl+Enter on a body match opens the editing view");
    body_discovery_expected_selection(window, L"needle", 0);
}

/* #940 (review): in the narrow layout the source replaces the results. Alt+Left from the source
 * starts a mode without a source, so the focus returns to the query instead of staying on the
 * hidden source; the three bottom actions never overlap. */
static void body_discovery_narrow_switch(struct folio_window *_Nonnull window,
                                         struct native_fixture *_Nonnull fixture)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    int width = MulDiv(560, (int)GetDpiForWindow(root), 96);
    require(SetWindowPos(root, nullptr, 20, 20, width, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "narrow finder window");
    pump(window);
    body_discovery_open(window, fixture, L"needle");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE);
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    HWND preview = folio_window_accessible_window(window, ACCESSIBLE_DISCOVERY_PREVIEW);
    require(IsWindowVisible(preview), "the narrow layout shows the source on request");
    struct accessible_view toggle =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_TOGGLE);
    struct accessible_view edit =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    struct accessible_view open =
        tags_view(window, ACCESSIBLE_SURFACE_BUTTON, ACCESSIBLE_SURFACE_DISCOVERY_OPEN);
    printf("narrow actions: toggle %ld..%ld edit %ld..%ld open %ld..%ld\n", toggle.bounds.left,
           toggle.bounds.right, edit.bounds.left, edit.bounds.right, open.bounds.left,
           open.bounds.right);
    fflush(stdout);
    require(toggle.bounds.right <= edit.bounds.left && edit.bounds.right <= open.bounds.left,
            "the three bottom actions do not overlap at the minimum width");
    SetFocus(preview);
    SendMessageW(preview, WM_SYSKEYDOWN, VK_LEFT, 0);
    pump(window);
    discovery_settle(window, fixture->state);
    printf("narrow switch: mode names %d preview visible %d focus input %d\n",
           folio_state_discovery_summary(fixture->state).mode == DISCOVERY_MODE_NAMES,
           IsWindowVisible(preview), GetFocus() == input);
    fflush(stdout);
    require(folio_state_discovery_summary(fixture->state).mode == DISCOVERY_MODE_NAMES &&
                GetFocus() == input,
            "Alt+Left from the source switches the mode and returns the focus to the query");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    require(SetWindowPos(root, nullptr, 20, 20, 1050, 720, SWP_NOZORDER | SWP_NOACTIVATE),
            "restore the wide body fixture");
    pump(window);
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
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    require(folio_state_pane_mode(fixture->state) == PANE_MODE_EDIT,
            "same VIEW target enters the canonical editing path");
    body_discovery_expected_selection(window, L"needle first", 0);
}

static void body_discovery_changed_source(struct folio_window *_Nonnull window,
                                          struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"Other needle");
    body_discovery_write(fixture, L"\\schedule\\two.md", "changed prefix\nOther needle\nnew end");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    body_discovery_expected_selection(window, L"Other needle", 0);
    body_discovery_return_one(window, fixture);
}

static void body_discovery_read_failure(struct folio_window *_Nonnull window,
                                        struct native_fixture *_Nonnull fixture)
{
    body_discovery_open(window, fixture, L"Other needle");
    HANDLE locked = management_lock(fixture, L"\\schedule\\two.md", 0);
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "post-search read failure keeps the original document and finder");
    require(CloseHandle(locked), "release only the isolated target read lock");
    wchar_t path[storage_native_capacity];
    native_file_path(fixture, L"\\schedule\\two.md", path);
    require(DeleteFileW(path), "remove only the private target to test disappearance");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    require(folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY &&
                strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "known uneditable snapshot is rejected before departure and remains searchable");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    body_discovery_write(fixture, L"\\schedule\\two.md", body_discovery_other);
    body_discovery_open(window, fixture, L"Other needle");
    body_discovery_write(fixture, L"\\schedule\\two.md", "Other needle\a");
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
    require(name_dialog.timer == 0 && name_dialog.closed == 1 &&
                folio_state_document_kind(fixture->state) == FOLIO_DOCUMENT_UNTITLED &&
                folio_state_discovery_summary(fixture->state).phase == DISCOVERY_READY,
            "cancelled draft departure preserves the original draft and body finder");
    start_name_dialog(window, (struct name_dialog_request){.title = L"無題のノート",
                                                           .name = L"body-discovery-saved",
                                                           .discard = true,
                                                           .action = NAME_DIALOG_SAVE});
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
        require(strcat_s(source, sizeof source,
                         line == 33 ? "長い行は折り返して表示し、行番号は論理行の1行目にだけ付ける "
                                      "wrapped source line keeps one number per logical line😀\r\n"
                                    : "前の行😀\r\n") == 0,
                "bounded visual context with one wrapped line (#933)");
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
    /* #933: CI is 96 DPI and a developer desktop often is not; -96 checks that geometry too. */
    if (body_discovery_96)
        require(SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_UNAWARE) != nullptr,
                "96 DPI windows for the body-discovery case");
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
        body_discovery_typing_rows(window, fixture);
        body_discovery_typing_preview(window, fixture);
        body_discovery_view_open(window, fixture);
        body_discovery_narrow_switch(window, fixture);
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

/* #941: the note row's menu entry opens the destination picker for that row's note. */
static void move_category_open(struct folio_window *_Nonnull window, struct note_ref target)
{
    HWND root = folio_window_accessible_window(window, ACCESSIBLE_WINDOW);
    struct menu_request request = {.context = FOLIO_MENU_NOTE, .target = target, .keyboard = true};
    SendMessageW(root, folio_message_open_menu, 0, (LPARAM)&request);
    pump(window);
    size_t row = SIZE_MAX;
    for (size_t at = 0; at < folio_menu_action_count(FOLIO_MENU_NOTE); ++at)
        if (folio_menu_action_command(folio_menu_action_at(FOLIO_MENU_NOTE, at)) ==
            FOLIO_COMMAND_MOVE_NOTE)
            row = at;
    require(row != SIZE_MAX, "the note menu lists the move");
    struct accessible_view entry = tags_view(window, ACCESSIBLE_SURFACE_CHOICE, row);
    require(entry.enabled && wcsstr(entry.name, L"カテゴリを移動") != nullptr,
            "the menu row names the move");
    struct accessible_request invoke = {
        .item = {.kind = ACCESSIBLE_SURFACE_CHOICE,
                 .slot = row,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_INVOKE};
    require(folio_window_accessible_action(window, &invoke) == S_OK, "invoke the move");
    pump(window);
    require(IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_SURFACE)),
            "the destination picker opens");
}

static bool move_category_stored(struct native_fixture *_Nonnull fixture,
                                 const char *_Nonnull category, const char *_Nonnull name)
{
    struct persistence_port disk = persistence_adapter_port(fixture->files);
    struct note_text *stored = nullptr;
    bool found = disk.read_note(disk.adapter, category, name, &stored) == PERSISTENCE_LOADED;
    note_text_destroy(stored);
    return found;
}

/* #941: schedule/two goes to target by Enter on the first choosable row, and back by typing a
 * filter; the note's own category cannot be chosen; Esc moves nothing; the open note stays. */
static bool move_category_closed(struct folio_window *_Nonnull window)
{
    pump(window);
    return !IsWindowVisible(folio_window_accessible_window(window, ACCESSIBLE_SURFACE));
}

static HRESULT move_category_invoke(struct folio_window *_Nonnull window, size_t slot)
{
    struct accessible_request row = {
        .item = {.kind = ACCESSIBLE_SURFACE_CHOICE,
                 .slot = slot,
                 .revision = folio_window_accessible_surface_revision(window)},
        .action = ACCESSIBLE_ACTION_INVOKE};
    return folio_window_accessible_action(window, &row);
}

/* What the picker shows and refuses before anything moves. */
static void move_category_picker(struct folio_window *_Nonnull window,
                                 struct native_fixture *_Nonnull fixture)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    move_category_open(window, (struct note_ref){0, 1});
    struct accessible_view own = tags_view(window, ACCESSIBLE_SURFACE_CHOICE, 0);
    struct accessible_view other = tags_view(window, ACCESSIBLE_SURFACE_CHOICE, 1);
    struct accessible_view status = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    require(wcscmp(own.name, L"schedule（いまのカテゴリ）") == 0 &&
                wcscmp(other.name, L"target") == 0 && GetFocus() == input,
            "the picker lists every category, marks the note's own, and takes typing");
    require(wcscmp(status.name, L"移すノート: schedule / two") == 0,
            "the picker's status names the note, which is not opened");
    if (move_category_visual)
        tags_capture(folio_window_accessible_window(window, ACCESSIBLE_WINDOW),
                     "move-category.bmp");
    require(move_category_invoke(window, 0) == accessibility_node_disabled &&
                move_category_stored(fixture, "schedule", "two"),
            "the note's own category is not an action");
    SendMessageW(input, WM_KEYDOWN, VK_ESCAPE, 0);
    require(move_category_closed(window) && move_category_stored(fixture, "schedule", "two"),
            "Esc closes the picker and moves nothing");
}

/* By the keyboard: Enter on the first choosable row, then back through a typed filter. */
static void move_category_keys(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    HWND input = folio_window_accessible_window(window, ACCESSIBLE_COMMAND_INPUT);
    move_category_open(window, (struct note_ref){0, 1});
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    require(move_category_closed(window) && move_category_stored(fixture, "target", "two") &&
                !move_category_stored(fixture, "schedule", "two") &&
                strcmp(folio_state_note_name(fixture->state, (struct note_ref){1, 1}), "two") == 0,
            "Enter moves the note to the end of the first other category");
    require(strcmp(folio_state_document_name(fixture->state), "one") == 0,
            "the open note stays open");
    move_category_open(window, (struct note_ref){1, 1});
    require(SetWindowTextW(input, L"TARG"), "filter to the note's own category");
    pump(window);
    struct accessible_view own = tags_view(window, ACCESSIBLE_SURFACE_CHOICE, 0);
    require(wcscmp(own.name, L"target（いまのカテゴリ）") == 0, "the filter ignores ASCII case");
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    require(!move_category_closed(window) && move_category_stored(fixture, "target", "two"),
            "the note's own category cannot be chosen");
    require(SetWindowTextW(input, L"sched"), "filter to the other category");
    pump(window);
    SendMessageW(input, WM_KEYDOWN, VK_RETURN, 0);
    require(move_category_closed(window) && move_category_stored(fixture, "schedule", "two") &&
                !move_category_stored(fixture, "target", "two") &&
                strcmp(folio_state_note_name(fixture->state, (struct note_ref){0, 1}), "two") == 0,
            "typing a filter and Enter moves the note back to where the fixture had it");
}

/* The same round trip by invoking the rows, as a pointer or assistive technology does. */
static void move_category_rows(struct folio_window *_Nonnull window,
                               struct native_fixture *_Nonnull fixture)
{
    move_category_open(window, (struct note_ref){0, 1});
    require(move_category_invoke(window, 1) == S_OK && move_category_closed(window) &&
                move_category_stored(fixture, "target", "two"),
            "invoking a row moves the note");
    move_category_open(window, (struct note_ref){1, 1});
    require(move_category_invoke(window, 0) == S_OK && move_category_closed(window) &&
                move_category_stored(fixture, "schedule", "two") &&
                !move_category_stored(fixture, "target", "two"),
            "and back");
}

static void move_category(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture)
{
    /* The everyday state the drag refuses: favorites first, with a star in the source category.
     * The fixture folder is shared, so the state is set here and put back at the end. */
    enum note_order order = folio_state_note_order(fixture->state);
    bool starred = folio_state_note_favorite(fixture->state, (struct note_ref){0, 0});
    if (!starred)
        require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, 0}) ==
                    FOLIO_STATE_READY,
                "a star beside the note to move");
    require(folio_state_set_note_order(fixture->state, NOTE_ORDER_FAVORITES_FIRST) ==
                FOLIO_STATE_READY,
            "favorites first");
    move_category_picker(window, fixture);
    move_category_keys(window, fixture);
    move_category_rows(window, fixture);
    require(folio_state_set_note_order(fixture->state, order) == FOLIO_STATE_READY,
            "the shared fixture gets its order back");
    if (!starred)
        require(folio_state_toggle_favorite(fixture->state, (struct note_ref){0, 0}) ==
                    FOLIO_STATE_READY,
                "and its stars");
}

static void discovery_cut_status(struct folio_window *_Nonnull window, const wchar_t *_Nonnull part,
                                 bool present, const char *_Nonnull label)
{
    struct accessible_view status = tags_view(window, ACCESSIBLE_SURFACE_STATUS, 0);
    printf("cut status: %ls\n", status.name);
    fflush(stdout);
    require((wcsstr(status.name, part) != nullptr) == present, label);
}

/* #928: when the list is cut at 200, the count line says which part is shown: the best-ranked
 * ones for a name query, the first ones without a query and for body matches. */
static void discovery_cut(struct folio_window *_Nonnull window,
                          struct native_fixture *_Nonnull fixture)
{
    invoke_operation(window, FOLIO_COMMAND_DISCOVER_NOTES);
    pump(window);
    discovery_settle(window, fixture->state);
    struct discovery_summary all = folio_state_discovery_summary(fixture->state);
    require(all.total == 213 && all.shown == 200, "213 notes, 200 shown");
    discovery_cut_status(window, L"先頭の200件", true, "without a query the first ones are shown");
    discovery_cut_status(window, L"完全→前方", false, "no ranking is named without a query");
    discovery_query(window, fixture->state, L"cut");
    require(folio_state_discovery_summary(fixture->state).total == 210, "210 names match");
    discovery_cut_status(window, L"上位200件を表示（完全→前方→部分→あいまいの順）", true,
                         "a cut name query names the ranking");
    discovery_query(window, fixture->state, L"cut-209");
    require(folio_state_discovery_summary(fixture->state).total == 1, "one name matches");
    discovery_cut_status(window, L"上位", false, "a complete list says nothing about a cut");
    body_discovery_mode(window, 1);
    discovery_query(window, fixture->state, L"cutbody");
    require(folio_state_discovery_summary(fixture->state).total == 210, "210 sources match");
    discovery_cut_status(window, L"先頭の200件", true, "cut body matches are the first ones");
    surface_button(window, ACCESSIBLE_SURFACE_CLOSE);
    remove_cut_category(fixture->location);
}

static void run_discovery_extra(struct folio_window *_Nonnull window,
                                struct native_fixture *_Nonnull fixture)
{
    if (discovery_cut_case)
        discovery_cut(window, fixture);
    else
        move_category(window, fixture);
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
    if (discovery_cut_case || move_category_case)
    {
        run_discovery_extra(window, fixture);
        return;
    }
    discovery_shortcut(window, fixture);
    discovery_mode_arrows(window, fixture);
    discovery_edit_open(window, fixture);
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
    motion_tick(window, 8);
    tags_capture(layer, "motion-open-middle.bmp");
    motion_tick(window, 8);
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
    motion_tick(window, 6);
    tags_capture(layer, "motion-selection-middle.bmp");
    motion_tick(window, 6);
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
    motion_tick(window, 16);
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
    motion_tick(window, 16);
    discovery_choose(window, 1);
    motion_tick(window, 2);
    discovery_choose(window, 2);
    discovery_choose(window, 0);
    discovery_settle(window, fixture->state);
    uint64_t revision = folio_window_accessible_surface_revision(window);
    motion_tick(window, 12);
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
    /* #940: the editing open is the one that pours the note again. */
    surface_button(window, ACCESSIBLE_SURFACE_DISCOVERY_EDIT);
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
    motion_tick(window, 6);
    discovery_choose(window, 1);
    discovery_settle(window, fixture->state);
    motion_tick(window, 4);
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
    body_discovery_96 = strcmp(selector, "--body-discovery-96") == 0;
    body_discovery_case = strcmp(selector, "--body-discovery") == 0 || body_discovery_visual_case ||
                          body_discovery_copy_case || body_discovery_measure_case ||
                          body_discovery_96;
    discovery_visual_case = strcmp(selector, "--discovery-visual") == 0;
    discovery_copy_case = strcmp(selector, "--discovery-copy") == 0;
    discovery_measure_case = strcmp(selector, "--discovery-measure") == 0;
    discovery_cut_case = strcmp(selector, "--discovery-cut") == 0;
    move_category_visual = strcmp(selector, "--move-category-visual") == 0;
    move_category_case = strcmp(selector, "--move-category") == 0 || move_category_visual;
    discovery_case = strcmp(selector, "--discovery") == 0 || discovery_visual_case ||
                     discovery_copy_case || discovery_measure_case || discovery_cut_case ||
                     move_category_case;
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
    image_drop_case = strcmp(selector, "--image-drop") == 0;
    search_key_visual_case = strcmp(selector, "--search-key-visual") == 0;
    text_menu_case = strcmp(selector, "--text-menu") == 0;
    window_place_case = strcmp(selector, "--window-place") == 0;
    footnote_jump_case = strcmp(selector, "--footnote-jump") == 0;
    definition_list_case = strcmp(selector, "--definition-list") == 0;
    outside_refresh_case = strcmp(selector, "--outside-refresh") == 0;
    request_prompt_case = strcmp(selector, "--request-prompt") == 0;
    open_link_case =
        strcmp(selector, "--open-link") == 0 || outside_refresh_case || request_prompt_case;
    outline_96 = strcmp(selector, "--outline-96") == 0;
    outline_case = strcmp(selector, "--outline") == 0 || outline_96;
    bare_url_visual_case = strcmp(selector, "--bare-url-visual") == 0 || search_key_visual_case ||
                           text_menu_case || window_place_case || footnote_jump_case ||
                           definition_list_case || outline_case || open_link_case;
    code_copy_visual_case = strcmp(selector, "--code-copy-visual") == 0 || bare_url_visual_case;
    code_copy_keys_case_selected = strcmp(selector, "--code-copy-keys") == 0;
    code_copy_scroll_96 = strcmp(selector, "--code-copy-scroll-96") == 0;
    code_copy_scroll_case = strcmp(selector, "--code-copy-scroll") == 0 || code_copy_scroll_96;
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
